#include "controls.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <pwd.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace {
const auto pkexec = u"/usr/bin/pkexec"_s;
const auto properties = u"org.freedesktop.DBus.Properties"_s;
const auto powerService = u"org.freedesktop.UPower.PowerProfiles"_s;
const auto powerPath = u"/org/freedesktop/UPower/PowerProfiles"_s;
const auto kwin = u"org.kde.KWin"_s;

QString helper(const QString &name)
{
    return QString::fromUtf8(LPT_HELPERS) + u"/"_s + name;
}

QString resultError(QProcess &process, int code, const QString &fallback)
{
    if (code == 126) {
        return QObject::tr("Administrator approval was canceled.");
    }
    if (code == 127) {
        return QObject::tr("System access could not be authorized. Use an active local KDE session.");
    }
    const auto message = QString::fromUtf8(process.readAllStandardError()).trimmed();
    return message.isEmpty() ? fallback : message.left(500);
}
} // namespace

SystemAccess::SystemAccess(QObject *parent) : QObject(parent)
{
    connect(&process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (code != 0 || status != QProcess::NormalExit || !authorized()) {
            errorMessage = resultError(process, code, tr("Could not set up system access."));
        }
        emit changed();
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            errorMessage = tr("Could not start administrator authorization.");
            emit changed();
        }
    });
}

bool SystemAccess::installed()
{
    return QFileInfo(pkexec).isExecutable() && QFileInfo(helper(u"access"_s)).isExecutable()
        && QFileInfo(QString::fromUtf8(LPT_POLICY)).isFile();
}

bool SystemAccess::authorized()
{
    const uid_t uid = getuid();
    const auto *account = getpwuid(uid);
    QFile grant(QString::fromUtf8(LPT_GRANTS) + u"/"_s + QString::number(uid));
    return account && QFileInfo(grant).ownerId() == 0 && grant.open(QIODevice::ReadOnly)
        && grant.read(512).trimmed() == QByteArray(account->pw_name);
}

bool SystemAccess::busy() const { return process.state() != QProcess::NotRunning; }

void SystemAccess::authorize()
{
    if (!busy() && !authorized() && installed()) {
        errorMessage.clear();
        process.start(pkexec, {helper(u"access"_s), u"authorize"_s});
        emit changed();
    }
}

PowerProfiles::PowerProfiles(QObject *parent)
    : QObject(parent), service(powerService, QDBusConnection::systemBus(),
                               QDBusServiceWatcher::WatchForOwnerChange)
{
    QDBusConnection::systemBus().connect(powerService, powerPath, properties, u"PropertiesChanged"_s,
        this, SLOT(profilesChanged(QString,QVariantMap,QStringList)));
    connect(&service, &QDBusServiceWatcher::serviceOwnerChanged, this, &PowerProfiles::refresh);
    connect(&process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        finished(status == QProcess::NormalExit ? code : -1);
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            errorMessage = tr("Install the Linux Power Toys RPM to use Power.");
            complete(false);
        }
    });
}

QStringList PowerProfiles::modes()
{
    return {u"power-saver"_s, u"cool"_s, u"balanced"_s, u"performance"_s, u"gaming"_s};
}

QString PowerProfiles::label(const QString &profile)
{
    if (profile == u"power-saver"_s) return tr("Power Saver");
    if (profile == u"cool"_s) return tr("Cool");
    if (profile == u"balanced"_s) return tr("Balanced");
    if (profile == u"performance"_s) return tr("Performance");
    if (profile == u"gaming"_s) return tr("Gaming");
    return tr("Unavailable");
}

QStringList PowerProfiles::available() const
{
    QStringList result;
    for (const auto &entry : status.value(u"available"_s).toArray()) {
        result.append(entry.toString());
    }
    return result;
}

QString PowerProfiles::current() const { return status.value(u"mode"_s).toString(); }
QString PowerProfiles::systemProfile() const { return status.value(u"profile"_s).toString(); }
bool PowerProfiles::managed() const { return status.value(u"managed"_s).toBool(); }
bool PowerProfiles::busy() const { return !operation.isEmpty(); }
bool PowerProfiles::applying() const { return busy() && operation != u"status"_s; }

bool PowerProfiles::needsAccess(const QString &profile) const
{
    return managed() || profile == u"cool"_s || profile == u"gaming"_s;
}

QString PowerProfiles::details() const
{
    if (systemProfile().isEmpty()) {
        return busy() ? tr("Reading system power profiles…") : tr("No power profiles are available.");
    }
    const auto turbo = status.value(u"turbo"_s).toString();
    const auto thermal = status.value(u"thermal"_s).toString();
    QStringList lines{tr("System profile  %1").arg(label(systemProfile()))};
    if (!turbo.isEmpty()) lines.append(tr("Turbo Boost  %1").arg(turbo == u"0"_s ? tr("On") : tr("Off")));
    if (!thermal.isEmpty()) lines.append(tr("Dell thermal mode  %1").arg(thermal));
    return lines.join(u'\n');
}

void PowerProfiles::profilesChanged(const QString &interface, const QVariantMap &changes,
                                    const QStringList &invalidated)
{
    if (interface != powerService) {
        return;
    }
    for (const auto &key : {u"ActiveProfile"_s, u"Profiles"_s}) {
        if (changes.contains(key) || invalidated.contains(key)) {
            refresh();
            return;
        }
    }
}

void PowerProfiles::refresh()
{
    if (busy()) {
        pendingRefresh = true;
    } else {
        run();
    }
}

void PowerProfiles::select(const QString &profile)
{
    if (busy() || !available().contains(profile) || (profile == current() && !managed())) {
        return;
    }
    if (needsAccess(profile) && (!SystemAccess::authorized() || !SystemAccess::installed())) {
        errorMessage = tr("Set up system access to change Dell hardware settings.");
        emit changed();
        return;
    }
    errorMessage.clear();
    run(profile);
}

void PowerProfiles::run(const QString &profile)
{
    operation = profile.isEmpty() ? u"status"_s : profile;
    QStringList args = profile.isEmpty() ? QStringList{u"status"_s}
                                        : QStringList{u"apply"_s, profile};
    const auto executable = helper(u"power"_s);
    if (!profile.isEmpty() && needsAccess(profile)) {
        args.prepend(executable);
        process.start(pkexec, args);
    } else {
        process.start(executable, args);
    }
    emit changed();
}

void PowerProfiles::finished(int code)
{
    const auto result = QJsonDocument::fromJson(process.readAllStandardOutput());
    const bool success = code == 0 && result.isObject();
    if (success) {
        status = result.object();
    } else if (operation != u"status"_s || errorMessage.isEmpty()) {
        errorMessage = resultError(process, code, tr("Could not read or change the system power profile."));
    }
    complete(success);
}

void PowerProfiles::complete(bool success)
{
    const bool applying = operation != u"status"_s;
    operation.clear();
    emit changed();
    if (applying) emit applied(success);
    if (pendingRefresh || (applying && !success)) {
        pendingRefresh = false;
        QTimer::singleShot(0, this, [this] { if (!busy()) refresh(); });
    }
}

TouchpadEdges::TouchpadEdges(QObject *parent)
    : QObject(parent), compositor(kwin, QDBusConnection::sessionBus(),
                                  QDBusServiceWatcher::WatchForUnregistration)
{
    process.setReadChannel(QProcess::StandardOutput);
    retry.setSingleShot(true);
    retry.setInterval(100);
    connect(&retry, &QTimer::timeout, this, &TouchpadEdges::confirmDevice);
    connect(&compositor, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        errorMessage = tr("KWin stopped. Enable Touchpad Edges again to reconnect.");
        stop();
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, &TouchpadEdges::messages);
    connect(&process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        finished(status == QProcess::NormalExit ? code : -1);
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finished(-1);
    });
}

TouchpadEdges::~TouchpadEdges()
{
    process.closeWriteChannel();
    if (running()) process.waitForFinished(700);
}

QString TouchpadEdges::unavailableReason()
{
    if (!QGuiApplication::platformName().startsWith(u"wayland"_s)) {
        return tr("Touchpad Edges requires a local KDE Plasma Wayland session.");
    }
    if (!QFileInfo(helper(u"touchpad"_s)).isExecutable() || !SystemAccess::installed()) {
        return tr("Install the Linux Power Toys RPM to use Touchpad Edges.");
    }
    return {};
}

bool TouchpadEdges::running() const { return process.state() != QProcess::NotRunning; }

void TouchpadEdges::start()
{
    if (running()) return;
    device.clear();
    errorMessage = unavailableReason();
    const auto bus = QDBusConnection::sessionBus().interface();
    if (errorMessage.isEmpty() && (!bus || !bus->isServiceRegistered(kwin).value())) {
        errorMessage = tr("KWin is unavailable. Enable Touchpad Edges again after KDE starts.");
    }
    if (errorMessage.isEmpty() && !SystemAccess::authorized()) {
        errorMessage = tr("Set up system access to enable Touchpad Edges.");
    }
    if (errorMessage.isEmpty()) {
        stopping = false;
        process.start(pkexec, {helper(u"touchpad"_s), u"start"_s});
    }
    emit changed();
}

void TouchpadEdges::stop()
{
    stopping = true;
    ready = false;
    retry.stop();
    process.closeWriteChannel();
    emit changed();
}

void TouchpadEdges::messages()
{
    static const QRegularExpression eventName(u"^DEVICE (event[0-9]+)$"_s);
    while (process.canReadLine()) {
        const auto line = QString::fromUtf8(process.readLine()).trimmed();
        if (stopping) continue;
        const auto match = eventName.match(line);
        if (match.hasMatch() && device.isEmpty()) {
            device = match.captured(1);
            attempts = 40;
            confirmDevice();
        } else if (line == u"ON"_s) {
            ready = true;
            emit changed();
        } else if (ready) {
            shortcut(line);
        }
    }
}

void TouchpadEdges::confirmDevice()
{
    auto call = QDBusMessage::createMethodCall(kwin, u"/org/kde/KWin/InputDevice/"_s + device,
                                               properties, u"GetAll"_s);
    call.setAutoStartService(false);
    call << u"org.kde.KWin.InputDevice"_s;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 150), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<QVariantMap> reply(*watcher);
        watcher->deleteLater();
        if (stopping || !running()) return;
        const auto props = reply.isValid() ? reply.value() : QVariantMap{};
        if (props.value(u"touchpad"_s).toBool() && props.value(u"enabled"_s).toBool()) {
            process.write("ready\n");
        } else if (--attempts > 0) {
            retry.start();
        }
    });
}

void TouchpadEdges::shortcut(const QString &action)
{
    QString component, name;
    if (action == u"volume+"_s || action == u"volume-"_s) {
        component = u"kmix"_s;
        name = action.endsWith(u'+') ? u"increase_volume_small"_s : u"decrease_volume_small"_s;
    } else if (action == u"brightness+"_s || action == u"brightness-"_s) {
        component = u"org_kde_powerdevil"_s;
        name = action.endsWith(u'+') ? u"Increase Screen Brightness Small"_s : u"Decrease Screen Brightness Small"_s;
    } else {
        return;
    }
    auto call = QDBusMessage::createMethodCall(u"org.kde.kglobalaccel"_s, u"/component/"_s + component,
                                              u"org.kde.kglobalaccel.Component"_s, u"invokeShortcut"_s);
    call.setAutoStartService(false);
    call << name;
    QDBusConnection::sessionBus().asyncCall(call, 800);
}

void TouchpadEdges::finished(int code)
{
    retry.stop();
    ready = false;
    if (!stopping) {
        errorMessage = resultError(process, code, tr("The touchpad relay stopped. Enable it again to reconnect."));
    }
    emit changed();
}
