#include "controls.h"

#include <KStatusNotifierItem>
#include <KWindowSystem>

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace {
const auto service = u"org.longweekendlabs.LinuxPowerToys"_s;
const auto objectPath = u"/org/longweekendlabs/LinuxPowerToys"_s;
const auto drawerService = u"org.kde.plasmashell"_s;
const auto drawerPath = u"/LinuxPowerToysDrawer"_s;
const auto drawerInterface = u"org.longweekendlabs.LinuxPowerToys.Drawer"_s;
const auto policy = u"org.kde.Solid.PowerManagement.PolicyAgent"_s;
const auto policyPath = u"/org/kde/Solid/PowerManagement/PolicyAgent"_s;
constexpr uint suspendPolicy = 1;
constexpr uint displayPolicy = 4;
constexpr uint activeInhibition = 1;
constexpr std::array minutes{15, 30, 60, 120};
enum class Mode { Passive, Timed, Until, Indefinite };
enum Page { Home, AwakePage, PowerPage, EdgesPage, CleanerPage, DrawerPage, PageCount };

const QIcon &powerIcon(const QString &profile = {})
{
    static const auto icons = [] {
        const std::array colors{QColor(u"#a4b8cc"_s), QColor(u"#b8ed82"_s), QColor(u"#88d5ff"_s),
                                QColor(u"#7aede0"_s), QColor(u"#ffcb6e"_s), QColor(u"#e4b1ff"_s)};
        std::array<QIcon, colors.size()> result;
        for (size_t i = 0; i < colors.size(); ++i) {
            for (const int size : {16, 22, 32, 48, 64}) {
                QPixmap pixmap(size, size);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.scale(size / 24.0, size / 24.0);
                const auto &color = colors[i];
                const QPolygonF badge{{12, 1.4}, {21.2, 6.1}, {21.2, 17.9}, {12, 22.6}, {2.8, 17.9}, {2.8, 6.1}};
                painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.setBrush(QColor(u"#172132"_s));
                painter.drawPolygon(badge);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(color, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                switch (i) {
                case 1: { // Power Saver
                    QPainterPath leaf;
                    leaf.moveTo(8, 16);
                    leaf.cubicTo(4.5, 10.2, 11.7, 7, 17.2, 6.8);
                    leaf.cubicTo(17.5, 13.7, 14.2, 18.2, 8, 16);
                    painter.setPen(Qt::NoPen);
                    painter.fillPath(leaf, color);
                    painter.setPen(QPen(QColor(u"#172132"_s), 1.25, Qt::SolidLine, Qt::RoundCap));
                    painter.drawLine(QPointF(7.3, 16.9), QPointF(14.3, 10.1));
                    break;
                }
                case 2: // Cool
                    painter.drawLine(QPointF(12, 6.5), QPointF(12, 17.5));
                    painter.drawLine(QPointF(7.2, 9.2), QPointF(16.8, 14.8));
                    painter.drawLine(QPointF(7.2, 14.8), QPointF(16.8, 9.2));
                    break;
                case 3: // Balanced
                    painter.drawLine(QPointF(7.4, 9.6), QPointF(16.6, 9.6));
                    painter.drawLine(QPointF(7.4, 14.4), QPointF(16.6, 14.4));
                    break;
                case 4: // Performance
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(color);
                    painter.drawPolygon(QPolygonF{{13.8, 4.8}, {7, 13.5}, {11.1, 13.5},
                                                  {10.2, 19.2}, {17, 10.1}, {12.8, 10.1}});
                    break;
                case 5: { // Gaming
                    painter.setPen(QPen(color, 1.55, Qt::SolidLine, Qt::RoundCap));
                    painter.drawRoundedRect(QRectF(6.2, 8.5, 11.6, 7), 3, 3);
                    painter.drawLine(QPointF(8.5, 12), QPointF(11.5, 12));
                    painter.drawLine(QPointF(10, 10.5), QPointF(10, 13.5));
                    painter.drawPoint(QPointF(14.6, 11.3));
                    painter.drawPoint(QPointF(16.0, 12.8));
                    break;
                }
                default:
                    painter.drawArc(QRectF(7, 8, 10, 10), 0, 180 * 16);
                    painter.drawLine(QPointF(12, 13), QPointF(14, 10.6));
                    break;
                }
                painter.end();
                result[i].addPixmap(pixmap);
            }
        }
        return result;
    }();
    return icons[PowerProfiles::modes().indexOf(profile) + 1];
}

QIcon toolboxIcon()
{
    QIcon result;
    for (const int size : {16, 22, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 32.0, size / 32.0);
        const QColor ink(u"#79e2ec"_s);
        painter.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawRoundedRect(QRectF(11, 5, 10, 9), 2, 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        painter.drawRoundedRect(QRectF(4, 11, 24, 15), 3, 3);
        painter.setPen(QPen(QColor(u"#23434c"_s), 1.4));
        painter.drawLine(QPointF(4, 17), QPointF(28, 17));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(u"#23434c"_s));
        painter.drawRoundedRect(QRectF(14, 15, 4, 5), 1, 1);
        painter.end();
        result.addPixmap(pixmap);
    }
    return result;
}

const QIcon &awakeTrayIcon(Mode mode)
{
    static const auto icons = [] {
        const std::array names{u"disabled"_s, u"timed"_s, u"expirable"_s, u"indefinite"_s};
        std::array<QIcon, names.size()> result;
        for (size_t i = 0; i < names.size(); ++i) {
            for (const int size : {16, 24, 32, 48}) {
                result[i].addFile(u":/awake/%1-%2.png"_s.arg(names[i]).arg(size), QSize(size, size));
            }
        }
        return result;
    }();
    return icons[static_cast<size_t>(mode)];
}

QString durationLabel(int value)
{
    if (value < 60) {
        return QObject::tr("%1 minutes").arg(value);
    }
    if (value == 60) {
        return QObject::tr("1 hour");
    }
    return QObject::tr("%1 hours").arg(value / 60);
}

QDBusMessage policyCall(const QString &member)
{
    return QDBusMessage::createMethodCall(policy, policyPath, policy, member);
}

class Awake final : public QObject {
    Q_OBJECT

public:
    explicit Awake(bool screen, QObject *parent = nullptr)
        : QObject(parent), keepScreen(screen)
    {
        expiry.setSingleShot(true);
        connect(&expiry, &QTimer::timeout, this, &Awake::armTimer);
    }

    ~Awake() override { release(); }

    bool setSession(Mode requested, const QDateTime &end, int minutes)
    {
        if (requested == Mode::Passive) {
            stop();
            return true;
        }
        if ((requested == Mode::Timed || requested == Mode::Until)
            && (!end.isValid() || end <= QDateTime::currentDateTime())) {
            return false;
        }
        if (!suspendCookie) {
            if (!startInhibitors()) {
                release();
                return false;
            }
        }
        mode = requested;
        deadline = end;
        sessionMinutes = minutes;
        armTimer();
        emit changed();
        return true;
    }

    bool setScreen(bool enabled)
    {
        if (suspendCookie && enabled != keepScreen) {
            if (enabled) {
                displayCookie = requestInhibition(displayPolicy);
                if (!displayCookie) {
                    return false;
                }
            } else {
                releaseCookie(displayCookie);
                displayActive = false;
            }
        }
        keepScreen = enabled;
        emit changed();
        return true;
    }

    Mode currentMode() const { return mode; }
    QDateTime endTime() const { return deadline; }
    int durationMinutes() const { return sessionMinutes; }
    bool screenOn() const { return keepScreen; }
    bool isEffective() const { return suspendActive && (!keepScreen || displayActive); }
    const QIcon &trayIcon() const { return awakeTrayIcon(isEffective() ? mode : Mode::Passive); }

signals:
    void changed();

private:
    std::optional<uint> requestInhibition(uint flags)
    {
        auto request = policyCall(u"AddInhibition"_s);
        request << flags << u"Linux Power Toys"_s << u"Always Awake"_s;
        const QDBusReply<uint> reply = bus.call(request, QDBus::Block, 1200);
        return reply.isValid() && reply.value() ? std::optional<uint>(reply.value()) : std::nullopt;
    }

    bool startInhibitors()
    {
        // PowerDevil also releases these requests when their connection closes.
        bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, busName);
        suspendCookie = requestInhibition(suspendPolicy);
        if (!suspendCookie) {
            return false;
        }
        if (keepScreen) {
            displayCookie = requestInhibition(displayPolicy);
            if (!displayCookie) {
                return false;
            }
        }
        bus.connect(policy, policyPath, u"org.freedesktop.DBus.Properties"_s,
                    u"PropertiesChanged"_s, this,
                    SLOT(policyPropertiesChanged(QString,QVariantMap,QStringList)));
        watcher = std::make_unique<QDBusServiceWatcher>(
            policy, bus, QDBusServiceWatcher::WatchForOwnerChange);
        connect(watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged,
                this, [this](const QString &, const QString &previous, const QString &) {
            if (!previous.isEmpty()) {
                displayCookie.reset();
                suspendCookie.reset();
                stop();
            }
        }, Qt::QueuedConnection);
        return true;
    }

private slots:
    void policyPropertiesChanged(const QString &interface, const QVariantMap &properties, const QStringList &)
    {
        if (!suspendCookie || interface != policy || !properties.contains(u"ActiveInhibitions"_s)) {
            return;
        }
        const auto argument = properties.value(u"ActiveInhibitions"_s).value<QDBusArgument>();
        const bool wasEffective = isEffective();
        suspendActive = false;
        displayActive = false;
        argument.beginArray();
        while (!argument.atEnd()) {
            QString what, who, why, type;
            uint flags = 0;
            argument.beginStructure();
            argument >> what >> who >> why >> type >> flags;
            argument.endStructure();
            if (who == u"Linux Power Toys" && why == u"Always Awake" && (flags & activeInhibition)) {
                const auto actions = QStringView(what).split(u':');
                suspendActive |= actions.contains(u"sleep");
                displayActive |= displayCookie && actions.contains(u"idle");
            }
        }
        argument.endArray();
        if (wasEffective != isEffective()) {
            emit changed();
        }
    }

private:
    void releaseCookie(std::optional<uint> &cookie)
    {
        if (!cookie) {
            return;
        }
        auto request = policyCall(u"ReleaseInhibition"_s);
        request << *cookie;
        bus.send(request);
        cookie.reset();
    }

    void release()
    {
        expiry.stop();
        suspendActive = false;
        displayActive = false;
        watcher.reset();
        releaseCookie(displayCookie);
        releaseCookie(suspendCookie);
        bus = QDBusConnection(QString{});
        QDBusConnection::disconnectFromBus(busName);
    }

    void stop()
    {
        release();
        mode = Mode::Passive;
        deadline = {};
        sessionMinutes = 0;
        emit changed();
    }

    void armTimer()
    {
        expiry.stop();
        if (!deadline.isValid()) {
            return;
        }
        const auto remaining = QDateTime::currentDateTime().msecsTo(deadline);
        if (remaining <= 0) {
            stop();
            return;
        }
        expiry.start(static_cast<int>(qMin<qint64>(remaining, std::numeric_limits<int>::max())));
    }

    const QString busName = u"linux-power-toys-awake"_s;
    QDBusConnection bus{QString{}};
    std::unique_ptr<QDBusServiceWatcher> watcher;
    std::optional<uint> suspendCookie;
    std::optional<uint> displayCookie;
    QTimer expiry;
    QDateTime deadline;
    int sessionMinutes = 0;
    Mode mode = Mode::Passive;
    bool keepScreen;
    bool suspendActive = false;
    bool displayActive = false;
};

class Toggle final : public QCheckBox {
public:
    Toggle() { setFixedSize(39, 24); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setOpacity(isEnabled() ? 1.0 : 0.45);
        const QColor accent(u"#79e2ec"_s);
        painter.setPen(isChecked() ? accent : QColor(u"#8292a4"_s));
        painter.setBrush(isChecked() ? accent : QColor(u"#161d26"_s));
        painter.drawRoundedRect(QRectF(1, 2, 37, 20), 10, 10);
        painter.setPen(Qt::NoPen);
        painter.setBrush(isChecked() ? QColor(u"#16323b"_s) : QColor(u"#a6b4c4"_s));
        painter.drawEllipse(QRectF(isChecked() ? 22 : 5, 6, 12, 12));
        if (hasFocus()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(accent, 1, Qt::DotLine));
            painter.drawRoundedRect(QRectF(0.5, 0.5, 38, 23), 11, 11);
        }
    }
};

class SettingsLink final : public QPushButton {
public:
    QSize sizeHint() const override { return layout() ? layout()->sizeHint() : QPushButton::sizeHint(); }
    QSize minimumSizeHint() const override { return layout() ? layout()->minimumSize() : QPushButton::minimumSizeHint(); }
};

class SettingsWindow final : public QWidget {
    Q_OBJECT

public:
    SettingsWindow()
    {
        setWindowTitle(tr("Linux Power Toys"));
        setWindowIcon(toolboxIcon());
        resize(976, 645);
        setMinimumSize(822, 580);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        auto *sidebar = new QWidget;
        sidebar->setObjectName(u"sidebar"_s);
        sidebar->setFixedWidth(218);
        auto *nav = new QVBoxLayout(sidebar);
        nav->setContentsMargins(10, 12, 10, 12);
        nav->setSpacing(4);
        const std::array names{tr("Home"), tr("Always Awake"), tr("Power"), tr("Touchpad Edges"),
                               tr("Cleaner"), tr("Tray Icon Drawer")};
        icons = {QIcon::fromTheme(u"go-home"_s), QIcon(u":/awake/awake-32.png"_s), powerIcon(),
                 QIcon::fromTheme(u"input-touchpad"_s), QIcon::fromTheme(u"edit-clear"_s),
                 QIcon::fromTheme(u"view-grid"_s)};
        for (int index = 0; index < int(names.size()); ++index) {
            auto *button = new QPushButton(icons[index], names[index]);
            button->setCheckable(true);
            button->setObjectName(u"nav"_s);
            navButtons[index] = button;
            nav->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, index] { openPage(index); });
        }
        nav->addStretch();
        layout->addWidget(sidebar);
        layout->addWidget(pages, 1);

        auto *home = page(names[0], {}, tr("Utilities"));
        alwaysHome = new Toggle;
        alwaysHome->setAccessibleName(tr("Enable Always Awake"));
        row(home, names[1], tr("Keep your computer awake from the system tray."), alwaysHome, AwakePage);
        powerHome->setAccessibleName(tr("Enable Power"));
        row(home, names[PowerPage], tr("Switch power profiles from the tray."), powerHome, PowerPage);
        edgesHome->setAccessibleName(tr("Enable Touchpad Edges"));
        row(home, names[EdgesPage], tr("Slide along the touchpad edges for brightness and volume."), edgesHome, EdgesPage);
        row(home, names[CleanerPage], tr("Preview and reclaim disk space."), new QLabel(tr("In development")), CleanerPage);
        drawerHome = new Toggle;
        drawerHome->setEnabled(false);
        drawerHome->setAccessibleName(tr("Enable Tray Icon Drawer"));
        row(home, names[DrawerPage], tr("Drag icons between the panel and the drawer."), drawerHome, DrawerPage);
        home->addSpacing(8);
        launchAtLogin->setAccessibleName(tr("Start at login"));
        row(home, tr("Start at login"), tr("Launch silently when you sign in to KDE."), launchAtLogin);
        home->addStretch();

        auto *awake = page(names[1], tr("Keep your computer awake from the system tray."), tr("Settings"));
        alwaysPage = new Toggle;
        alwaysPage->setAccessibleName(tr("Enable Always Awake"));
        row(awake, tr("Enable Always Awake"), tr("Show the coffee cup in the system tray."), alwaysPage);
        mode->addItems({tr("Use power settings"), tr("For a duration"), tr("Until a time"), tr("Indefinitely")});
        row(awake, tr("Mode"), tr("Choose when to resume your normal power settings."), mode);
        for (const int value : minutes) {
            duration->addItem(durationLabel(value), value);
        }
        duration->setCurrentIndex(1);
        durationRow = row(awake, tr("Duration"), {}, duration);
        until->setDisplayFormat(u"yyyy-MM-dd HH:mm"_s);
        until->setCalendarPopup(true);
        until->setDateTime(QDateTime::currentDateTime().addSecs(3600));
        untilRow = row(awake, tr("End time"), {}, until);
        screen->setAccessibleName(tr("Keep screen on"));
        row(awake, tr("Keep screen on"), tr("Keep the display lit during the awake session."), screen);
        message->setWordWrap(true);
        awake->addWidget(message);
        awake->addStretch();

        auto *power = page(names[PowerPage], tr("Switch power profiles from Settings or the system tray."), tr("Settings"));
        powerPage->setAccessibleName(tr("Enable Power"));
        row(power, tr("Enable Power"), tr("Show power profiles in the system tray."), powerPage);
        auto *profileGroup = new QActionGroup(this);
        profileGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);
        auto *profileButtons = new QHBoxLayout;
        for (const auto &id : PowerProfiles::modes()) {
            auto *action = profileGroup->addAction(PowerProfiles::label(id));
            action->setCheckable(true);
            auto *button = new QToolButton;
            button->setDefaultAction(action);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            profileButtons->addWidget(button);
            profileActions.insert(id, action);
            connect(action, &QAction::triggered, this, [this, id] { emit profileSelected(id); });
        }
        power->addLayout(profileButtons);
        powerDetails->setWordWrap(true);
        power->addWidget(powerDetails);
        powerMessage->setWordWrap(true);
        power->addWidget(powerMessage);
        row(power, tr("System access"), tr("One-time approval for Dell presets and Touchpad Edges."), powerAccess);
        power->addStretch();

        auto *edges = page(names[EdgesPage], tr("Adjust brightness and volume from the built-in touchpad."), tr("Settings"));
        edgesPage->setAccessibleName(tr("Enable Touchpad Edges"));
        row(edges, tr("Enable Touchpad Edges"), tr("Use the physical edges of the touchpad."), edgesPage);
        row(edges, tr("Left edge"), tr("Slide up or down to change screen brightness."), new QLabel);
        row(edges, tr("Right edge"), tr("Slide up or down to change audio volume."), new QLabel);
        edgesMessage->setWordWrap(true);
        edges->addWidget(edgesMessage);
        row(edges, tr("System access"), tr("One-time approval for Touchpad Edges and Dell presets."), edgesAccess);
        edges->addStretch();

        auto *cleaner = page(names[CleanerPage], tr("Preview files before reclaiming disk space."), tr("In development"));
        cleaner->addStretch();

        auto *drawer = page(names[DrawerPage], tr("Organize icons directly in the Plasma system tray."), tr("Settings"));
        drawerPage = new Toggle;
        drawerPage->setEnabled(false);
        drawerPage->setAccessibleName(tr("Enable Tray Icon Drawer"));
        row(drawer, tr("Enable Tray Icon Drawer"),
            tr("Drag icons into the drawer, onto the panel, or into a new position."), drawerPage);
        drawerMessage->setWordWrap(true);
        drawer->addWidget(drawerMessage);
        drawer->addStretch();

        for (auto *toggle : {alwaysHome, alwaysPage}) {
            connect(toggle, &QCheckBox::toggled, this, &SettingsWindow::enableAwake);
        }
        for (auto *toggle : {drawerHome, drawerPage}) {
            connect(toggle, &QCheckBox::toggled, this, &SettingsWindow::enableDrawer);
        }
        for (auto *toggle : {powerHome, powerPage}) {
            connect(toggle, &QCheckBox::toggled, this, &SettingsWindow::enablePower);
        }
        for (auto *toggle : {edgesHome, edgesPage}) {
            connect(toggle, &QCheckBox::toggled, this, &SettingsWindow::enableEdges);
        }
        connect(launchAtLogin, &QCheckBox::toggled, this, &SettingsWindow::setStartup);
        for (auto *button : {powerAccess, edgesAccess}) {
            connect(button, &QPushButton::clicked, this, &SettingsWindow::authorize);
        }
        connect(mode, &QComboBox::activated, this, [this] { emitMode(); });
        connect(duration, &QComboBox::activated, this, [this] { emitMode(); });
        connect(until, &QDateTimeEdit::editingFinished, this, [this] {
            if (mode->currentIndex() == int(Mode::Until)) {
                emitMode();
            }
        });
        connect(screen, &QCheckBox::toggled, this, &SettingsWindow::screenChanged);
        setStyleSheet(uR"(
            QWidget { background: #171c23; color: #f0f3f7; font-size: 14px; }
            QWidget#sidebar { background: #1a2028; border-right: 1px solid #28323d; }
            QPushButton#nav { text-align: left; padding: 10px; border: 0; border-radius: 5px; }
            QPushButton#nav:hover { background: #222c37; }
            QPushButton#nav:checked { background: #29333e; border-left: 3px solid #79e2ec; }
            QLabel#heading { font-size: 30px; font-weight: 650; }
            QLabel#subheading { font-size: 16px; font-weight: 600; }
            QLabel#muted { color: #b3bfce; font-size: 12px; }
            QFrame#row { background: #202731; border: 1px solid #323d49; border-radius: 7px; }
            QFrame#row QLabel, QFrame#row QCheckBox { background: transparent; }
            QPushButton#detail { background: transparent; border: 0; border-radius: 4px; }
            QPushButton#detail:hover { background: #29333e; }
            QLabel#chevron { font-size: 19px; }
            QCheckBox::indicator { width: 36px; height: 20px; }
            QComboBox, QDateTimeEdit { min-height: 28px; padding-left: 7px; background: #25303c;
                                       border: 1px solid #536476; border-radius: 4px; }
            QToolButton { padding: 7px 10px; border: 1px solid #536476; border-radius: 5px; }
            QToolButton:checked { border-color: #79e2ec; color: #79e2ec; background: #293a45; }
            QToolButton:disabled { color: #8996a3; border-color: #313d48; }
            QPushButton:focus, QCheckBox:focus { outline: 2px solid #79e2ec; }
        )"_s);
        openPage(0);
    }

    void openPage(int index)
    {
        pages->setCurrentIndex(index);
        for (int i = 0; i < int(navButtons.size()); ++i) {
            navButtons[i]->setChecked(i == index);
        }
    }

    void sync(const Awake *awake, bool keepScreen)
    {
        const QSignalBlocker a(alwaysHome), b(alwaysPage), c(screen);
        alwaysHome->setChecked(awake != nullptr);
        alwaysPage->setChecked(awake != nullptr);
        const auto current = awake ? awake->currentMode() : Mode::Passive;
        mode->setCurrentIndex(int(current));
        mode->setEnabled(awake != nullptr);
        screen->setEnabled(awake != nullptr);
        screen->setChecked(keepScreen);
        durationRow->setVisible(current == Mode::Timed);
        untilRow->setVisible(current == Mode::Until);
        if (current == Mode::Timed) {
            duration->setCurrentIndex(duration->findData(awake->durationMinutes()));
        } else if (current == Mode::Until) {
            until->setDateTime(awake->endTime());
        }
    }

    void syncDrawer(const std::optional<bool> &enabled)
    {
        const QSignalBlocker a(drawerHome), b(drawerPage);
        for (auto *toggle : {drawerHome, drawerPage}) {
            toggle->setEnabled(enabled.has_value());
            toggle->setChecked(enabled.value_or(false));
        }
        if (enabled.has_value()) {
            drawerMessage->clear();
        } else if (QFileInfo::exists(QString::fromUtf8(LPT_DRAWER_PLUGIN))) {
            drawerMessage->setText(tr("Sign out and back in to load the installed Tray Icon Drawer. "
                                      "The drawer requires a version of the RPM that matches Plasma."));
        } else {
            drawerMessage->setText(tr("Install the Tray Icon Drawer RPM for your Plasma version, "
                                      "then sign out and back in."));
        }
    }

    void syncControls(const PowerProfiles *power, const TouchpadEdges *edges, const SystemAccess &access, bool startup)
    {
        const QSignalBlocker a(powerHome), b(powerPage), c(edgesHome), d(edgesPage), e(launchAtLogin);
        launchAtLogin->setChecked(startup);
        for (auto *toggle : {powerHome, powerPage}) {
            toggle->setChecked(power != nullptr);
            toggle->setEnabled(!power || !power->applying());
        }
        const auto unavailable = TouchpadEdges::unavailableReason();
        for (auto *toggle : {edgesHome, edgesPage}) {
            toggle->setChecked(edges != nullptr);
            toggle->setEnabled(edges || unavailable.isEmpty());
        }
        const auto available = power ? power->available() : QStringList{};
        const bool allowed = SystemAccess::authorized();
        for (auto it = profileActions.cbegin(); it != profileActions.cend(); ++it) {
            it.value()->setChecked(power && power->current() == it.key());
            it.value()->setEnabled(power && !power->busy() && available.contains(it.key())
                                   && (!power->needsAccess(it.key()) || allowed));
        }
        powerDetails->setText(power ? power->details() : tr("Power is off."));
        powerMessage->setText(power && !power->error().isEmpty() ? power->error() : access.error());
        const auto edgeState = edges ? (edges->active() ? tr("Touchpad Edges is active.")
                                     : edges->running() ? tr("Starting Touchpad Edges…") : edges->error())
                                     : unavailable;
        edgesMessage->setText(!edgeState.isEmpty() ? edgeState : access.error());
        for (auto *button : {powerAccess, edgesAccess}) {
            button->setEnabled(SystemAccess::installed() && !allowed && !access.busy());
            button->setText(allowed ? tr("Approved") : access.busy() ? tr("Waiting…") : tr("Set up"));
        }
    }

    void setMessage(const QString &text) { message->setText(text); }
    QDateTime selectedEnd() const { return until->dateTime(); }

signals:
    void enableAwake(bool enabled);
    void enableDrawer(bool enabled);
    void enablePower(bool enabled);
    void enableEdges(bool enabled);
    void setStartup(bool enabled);
    void authorize();
    void profileSelected(const QString &profile);
    void modeChanged(Mode mode, QDateTime end, int minutes);
    void screenChanged(bool enabled);

private:
    void emitMode()
    {
        const auto requested = Mode(mode->currentIndex());
        const int minutes = requested == Mode::Timed ? duration->currentData().toInt() : 0;
        QDateTime end;
        if (requested == Mode::Timed) {
            end = QDateTime::currentDateTime().addSecs(minutes * 60);
        } else if (requested == Mode::Until) {
            const auto now = QDateTime::currentDateTime();
            end = qMax(now.addSecs(60), until->dateTime());
        }
        emit modeChanged(requested, end, minutes);
    }

    QVBoxLayout *page(const QString &title, const QString &description, const QString &section)
    {
        auto *widget = new QWidget;
        auto *layout = new QVBoxLayout(widget);
        layout->setContentsMargins(32, 29, 32, 32);
        layout->setSpacing(8);
        auto *heading = new QLabel(title);
        heading->setObjectName(u"heading"_s);
        layout->addWidget(heading);
        if (!description.isEmpty()) {
            auto *label = new QLabel(description);
            label->setObjectName(u"muted"_s);
            layout->addWidget(label);
        }
        layout->addSpacing(10);
        auto *label = new QLabel(section);
        label->setObjectName(u"subheading"_s);
        layout->addWidget(label);
        pages->addWidget(widget);
        return layout;
    }

    QWidget *row(QVBoxLayout *list, const QString &title, const QString &description,
                 QWidget *control, int detail = -1)
    {
        auto *frame = new QFrame;
        frame->setObjectName(u"row"_s);
        frame->setMinimumHeight(65);
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(14, 8, 14, 8);
        auto *contents = new QHBoxLayout;
        contents->setContentsMargins(0, 0, 0, 0);
        if (detail > 0) {
            const QIcon &glyph = icons[detail];
            auto *label = new QLabel;
            label->setPixmap(glyph.pixmap(26, 26));
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            contents->addWidget(label);
        }
        auto *copy = new QVBoxLayout;
        copy->setSpacing(3);
        auto *name = new QLabel(title);
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto font = name->font();
        font.setWeight(QFont::DemiBold);
        name->setFont(font);
        copy->addWidget(name);
        if (!description.isEmpty()) {
            auto *note = new QLabel(description);
            note->setObjectName(u"muted"_s);
            note->setAttribute(Qt::WA_TransparentForMouseEvents);
            note->setWordWrap(true);
            copy->addWidget(note);
        }
        contents->addLayout(copy, 1);
        if (detail > 0) {
            auto *button = new SettingsLink;
            button->setObjectName(u"detail"_s);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            button->setAccessibleName(tr("Open %1 settings").arg(title));
            button->setCursor(Qt::PointingHandCursor);
            auto *chevron = new QLabel(u"›"_s);
            chevron->setObjectName(u"chevron"_s);
            chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
            contents->addWidget(chevron);
            button->setLayout(contents);
            connect(button, &QPushButton::clicked, this, [this, detail] { openPage(detail); });
            layout->addWidget(button, 1);
        } else {
            layout->addLayout(contents, 1);
        }
        layout->addWidget(control);
        list->addWidget(frame);
        return frame;
    }

    QStackedWidget *pages = new QStackedWidget;
    std::array<QPushButton *, PageCount> navButtons{};
    std::array<QIcon, PageCount> icons{};
    QCheckBox *powerHome = new Toggle;
    QCheckBox *powerPage = new Toggle;
    QCheckBox *edgesHome = new Toggle;
    QCheckBox *edgesPage = new Toggle;
    QCheckBox *launchAtLogin = new Toggle;
    QLabel *powerDetails = new QLabel;
    QLabel *powerMessage = new QLabel;
    QLabel *edgesMessage = new QLabel;
    QPushButton *powerAccess = new QPushButton;
    QPushButton *edgesAccess = new QPushButton;
    QHash<QString, QAction *> profileActions;
    QCheckBox *alwaysHome{};
    QCheckBox *alwaysPage{};
    QCheckBox *drawerHome{};
    QCheckBox *drawerPage{};
    QLabel *drawerMessage = new QLabel;
    QComboBox *mode = new QComboBox;
    QComboBox *duration = new QComboBox;
    QDateTimeEdit *until = new QDateTimeEdit;
    QCheckBox *screen = new Toggle;
    QLabel *message = new QLabel;
    QWidget *durationRow{};
    QWidget *untilRow{};
};

class Toolbox final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.longweekendlabs.LinuxPowerToys")

public:
    explicit Toolbox(bool background)
    {
        if (background) {
            refreshDrawer();
        }
        if (settings.value(u"awake/enabled"_s, false).toBool()) {
            enableAwake(true);
        }
        connect(&access, &SystemAccess::changed, this, &Toolbox::sync);
        if (settings.value(u"power/enabled"_s, true).toBool()) {
            setPower(true);
        }
        if (settings.value(u"edges/enabled"_s, false).toBool()) {
            setEdges(true);
        }
        updateToolboxTray();
    }

    bool needsTray() const { return awake || power || edges || drawerEnabled.value_or(false); }

    void finishBackgroundStartup()
    {
        if (!drawerEnabled.has_value() && settings.value(u"drawer/enabled"_s, false).toBool()) {
            waitForDrawer(8);
        } else if (!needsTray()) {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
    }

public slots:
    void ShowSettings() { showSettings({}); }

private:
    void waitForDrawer(int attempts)
    {
        // The tray applet can load after desktop autostart registers this process.
        QTimer::singleShot(750, this, [this, attempts] {
            refreshDrawer();
            sync();
            if (!drawerEnabled.has_value() && attempts > 1) {
                waitForDrawer(attempts - 1);
            } else if (!window && !needsTray()) {
                QCoreApplication::quit();
            }
        });
    }

    void showSettings(const QString &activationToken, int page = -1)
    {
        if (!window) {
            window = new SettingsWindow;
            window->setAttribute(Qt::WA_DeleteOnClose);
            connect(window, &QObject::destroyed, this, [this] {
                window = nullptr;
                if (!needsTray()) {
                    QCoreApplication::quit();
                }
            });
            connect(window, &SettingsWindow::enableAwake, this, &Toolbox::enableAwake);
            connect(window, &SettingsWindow::enableDrawer, this, &Toolbox::setDrawer);
            connect(window, &SettingsWindow::enablePower, this, &Toolbox::setPower);
            connect(window, &SettingsWindow::enableEdges, this, &Toolbox::setEdges);
            connect(window, &SettingsWindow::profileSelected, this, &Toolbox::selectProfile);
            connect(window, &SettingsWindow::authorize, &access, &SystemAccess::authorize);
            connect(window, &SettingsWindow::setStartup, this, [this](bool enabled) {
                settings.setValue(u"startup/enabled"_s, enabled);
                settings.sync();
            });
            connect(window, &SettingsWindow::modeChanged, this, &Toolbox::setMode);
            connect(window, &SettingsWindow::screenChanged, this, &Toolbox::setScreen);
            sync();
        }
        refreshDrawer();
        sync();
        if (power && !power->busy()) power->refresh();
        if (page >= 0) {
            window->openPage(page);
        }
        window->showNormal();
        window->raise();
        if (activationToken.isEmpty()) {
            window->activateWindow();
        } else {
            KWindowSystem::setCurrentXdgActivationToken(activationToken);
            KWindowSystem::activateWindow(window->windowHandle());
        }
    }

    void enableAwake(bool enabled)
    {
        settings.setValue(u"awake/enabled"_s, enabled);
        settings.sync();
        if (enabled && !awake) {
            awake = std::make_unique<Awake>(keepScreen());
            connect(awake.get(), &Awake::changed, this, &Toolbox::sync);
            awakeTray = makeTray(u"linux-power-toys-awake"_s, tr("Always Awake"), awake->trayIcon());
            buildAwakeMenu();
        } else if (!enabled && awake) {
            awakeTray.reset();
            awake.reset();
            if (!window && !needsTray()) {
                QCoreApplication::quit();
            }
        }
        if (window) {
            window->setMessage({});
        }
        updateToolboxTray();
        sync();
    }

    void setPower(bool enabled)
    {
        if (enabled == bool(power)) return;
        if (!enabled && power->applying()) {
            sync();
            return;
        }
        if (!enabled && power->managed()) {
            if (power->busy() || !SystemAccess::authorized() || power->systemProfile().isEmpty()) {
                showSettings({}, PowerPage);
                sync();
                return;
            }
            disablePowerAfterApply = true;
            power->select(power->systemProfile());
            return;
        }
        settings.setValue(u"power/enabled"_s, enabled);
        settings.sync();
        if (enabled) {
            power = std::make_unique<PowerProfiles>();
            powerTray = makeTray(u"linux-power-toys-power"_s, tr("Power Profiles"), powerIcon());
            auto *menu = new QMenu;
            auto *group = new QActionGroup(menu);
            group->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);
            for (const auto &id : PowerProfiles::modes()) {
                auto *action = new QAction(powerIcon(id), PowerProfiles::label(id), group);
                action->setCheckable(true);
                menu->addAction(action);
                powerTrayActions.insert(id, action);
                connect(action, &QAction::triggered, this, [this, id] { selectProfile(id); });
            }
            menu->addSeparator();
            connect(menu->addAction(tr("Open settings")), &QAction::triggered, this, [this] {
                showSettings(powerTray->providedToken(), PowerPage);
            });
            powerTray->setContextMenu(menu);
            powerTray->setIsMenu(true);
            connect(power.get(), &PowerProfiles::changed, this, &Toolbox::sync);
            connect(power.get(), &PowerProfiles::applied, this, [this](bool success) {
                if (!std::exchange(disablePowerAfterApply, false)) return;
                if (success) setPower(false);
            }, Qt::QueuedConnection);
            power->refresh();
        } else {
            powerTrayActions.clear();
            powerTray.reset();
            power.reset();
        }
        updateToolboxTray();
        sync();
        quitIfIdle();
    }

    void selectProfile(const QString &id)
    {
        if (!power) return;
        if (power->needsAccess(id) && !SystemAccess::authorized()) {
            showSettings({}, PowerPage);
            return;
        }
        power->select(id);
    }

    void setEdges(bool enabled)
    {
        if (enabled == bool(edges)) return;
        if (enabled && (!SystemAccess::authorized() || !TouchpadEdges::unavailableReason().isEmpty())) {
            if (window) window->openPage(EdgesPage);
            sync();
            return;
        }
        settings.setValue(u"edges/enabled"_s, enabled);
        settings.sync();
        if (enabled) {
            edges = std::make_unique<TouchpadEdges>();
            connect(edges.get(), &TouchpadEdges::changed, this, &Toolbox::sync);
            edges->start();
        } else {
            edges.reset();
        }
        updateToolboxTray();
        sync();
        quitIfIdle();
    }

    void quitIfIdle()
    {
        if (!window && !needsTray()) QCoreApplication::quit();
    }

    void refreshDrawer()
    {
        auto call = QDBusMessage::createMethodCall(drawerService, drawerPath, drawerInterface, u"Enabled"_s);
        const QDBusReply<bool> reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 750);
        drawerEnabled = reply.isValid() ? std::optional<bool>(reply.value()) : std::nullopt;
        updateToolboxTray();
    }

    void setDrawer(bool enabled)
    {
        auto call = QDBusMessage::createMethodCall(drawerService, drawerPath, drawerInterface, u"SetEnabled"_s);
        call << enabled;
        const QDBusReply<bool> reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 750);
        if (reply.isValid() && reply.value()) {
            drawerEnabled = enabled;
        } else {
            refreshDrawer();
        }
        updateToolboxTray();
        sync();
        if (!window && !needsTray()) {
            QCoreApplication::quit();
        }
    }

    void updateToolboxTray()
    {
        if (!needsTray()) {
            toolboxTray.reset();
            toolboxAwake = nullptr;
            toolboxDrawer = nullptr;
            toolboxPower = nullptr;
            toolboxEdges = nullptr;
            return;
        }
        if (!toolboxTray) {
            toolboxTray = makeTray(u"linux-power-toys"_s, tr("Linux Power Toys"), toolboxIcon());
            auto *menu = new QMenu;
            toolboxAwake = menu->addAction(tr("Always Awake"));
            toolboxAwake->setCheckable(true);
            toolboxPower = menu->addAction(tr("Power"));
            toolboxPower->setCheckable(true);
            toolboxEdges = menu->addAction(tr("Touchpad Edges"));
            toolboxEdges->setCheckable(true);
            toolboxDrawer = menu->addAction(tr("Tray Icon Drawer"));
            toolboxDrawer->setCheckable(true);
            // Disabling the last toy destroys the menu that owns the action.
            connect(toolboxAwake, &QAction::triggered, this, &Toolbox::enableAwake, Qt::QueuedConnection);
            connect(toolboxDrawer, &QAction::triggered, this, &Toolbox::setDrawer, Qt::QueuedConnection);
            connect(toolboxPower, &QAction::triggered, this, &Toolbox::setPower, Qt::QueuedConnection);
            connect(toolboxEdges, &QAction::triggered, this, [this](bool enabled) {
                if (enabled && !SystemAccess::authorized()) showSettings({}, EdgesPage);
                setEdges(enabled);
            }, Qt::QueuedConnection);
            menu->addSeparator();
            connect(menu->addAction(tr("Open settings")), &QAction::triggered, this, &Toolbox::ShowSettings);
            toolboxTray->setContextMenu(menu);
            connect(toolboxTray.get(), &KStatusNotifierItem::activateRequested, this, [this] {
                showSettings(toolboxTray->providedToken());
            });
        }
        toolboxAwake->setChecked(bool(awake));
        toolboxPower->setChecked(bool(power));
        toolboxEdges->setChecked(bool(edges));
        toolboxDrawer->setEnabled(drawerEnabled.has_value());
        toolboxDrawer->setChecked(drawerEnabled.value_or(false));
    }

    std::unique_ptr<KStatusNotifierItem> makeTray(const QString &id, const QString &title, const QIcon &glyph)
    {
        auto item = std::make_unique<KStatusNotifierItem>(id);
        item->setTitle(title);
        item->setCategory(KStatusNotifierItem::ApplicationStatus);
        item->setStatus(KStatusNotifierItem::Active);
        item->setStandardActionsEnabled(false);
        item->setIconByPixmap(glyph);
        item->setToolTip(glyph, title, {});
        return item;
    }

    void buildAwakeMenu()
    {
        auto *menu = new QMenu;
        passive = menu->addAction(tr("Use power settings"));
        connect(passive, &QAction::triggered, this, [this] { setMode(Mode::Passive, {}); });
        auto *timed = menu->addMenu(tr("Keep awake for"));
        timedAction = timed->menuAction();
        for (const int value : minutes) {
            connect(timed->addAction(durationLabel(value)), &QAction::triggered, this, [this, value] {
                setMode(Mode::Timed, QDateTime::currentDateTime().addSecs(value * 60), value);
            });
        }
        untilAction = menu->addAction(tr("Keep awake until…"));
        connect(untilAction, &QAction::triggered, this, [this] {
            showSettings(awakeTray->providedToken(), AwakePage);
            bool accepted = false;
            const auto end = chooseEndTime(&accepted);
            if (accepted) {
                setMode(Mode::Until, end);
            }
        });
        indefinite = menu->addAction(tr("Keep awake indefinitely"));
        connect(indefinite, &QAction::triggered, this, [this] { setMode(Mode::Indefinite, {}); });
        menu->addSeparator();
        screenAction = menu->addAction(tr("Keep screen on"));
        screenAction->setCheckable(true);
        connect(screenAction, &QAction::triggered, this, &Toolbox::setScreen);
        menu->addSeparator();
        const auto openAwakeSettings = [this] { showSettings(awakeTray->providedToken(), AwakePage); };
        connect(menu->addAction(tr("Open settings")), &QAction::triggered, this, openAwakeSettings);
        connect(menu->addAction(tr("Exit")), &QAction::triggered, this, [this] {
            enableAwake(false);
        }, Qt::QueuedConnection);
        for (auto *action : {passive, timedAction, untilAction, indefinite}) {
            action->setCheckable(true);
        }
        awakeTray->setContextMenu(menu);
        connect(awakeTray.get(), &KStatusNotifierItem::activateRequested, this, openAwakeSettings);
    }

    QDateTime chooseEndTime(bool *accepted)
    {
        QDialog dialog(window);
        dialog.setWindowTitle(tr("Keep awake until"));
        auto *layout = new QVBoxLayout(&dialog);
        const auto now = QDateTime::currentDateTime();
        const auto selected = window->selectedEnd();
        auto *edit = new QDateTimeEdit(selected > now ? selected : now.addSecs(3600), &dialog);
        edit->setMinimumDateTime(now.addSecs(60));
        edit->setDisplayFormat(u"yyyy-MM-dd HH:mm"_s);
        edit->setCalendarPopup(true);
        layout->addWidget(edit);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        *accepted = dialog.exec() == QDialog::Accepted;
        return edit->dateTime();
    }

    bool keepScreen() const { return settings.value(u"awake/screen"_s, false).toBool(); }

    void setMode(Mode mode, const QDateTime &end, int minutes = 0)
    {
        if (!awake) {
            return;
        }
        if (!awake->setSession(mode, end, minutes)) {
            showError(tr("Could not start Always Awake. Check the end time and Plasma power management."));
        } else if (window) {
            window->setMessage({});
        }
        sync();
    }

    void setScreen(bool enabled)
    {
        if (awake && awake->setScreen(enabled)) {
            settings.setValue(u"awake/screen"_s, enabled);
            settings.sync();
        } else {
            showError(tr("Could not change the display setting."));
        }
        sync();
    }

    void showError(const QString &message)
    {
        if (window) {
            window->setMessage(message);
        } else if (awakeTray) {
            awakeTray->showMessage(tr("Always Awake"), message, u"dialog-warning"_s);
        }
    }

    void sync()
    {
        if (window) {
            window->sync(awake.get(), keepScreen());
            window->syncDrawer(drawerEnabled);
        }
        if (window) window->syncControls(power.get(), edges.get(), access, settings.value(u"startup/enabled"_s, true).toBool());
        if (toolboxPower) {
            toolboxPower->setEnabled(!power || !power->applying());
            toolboxEdges->setEnabled(edges || TouchpadEdges::unavailableReason().isEmpty());
        }
        if (powerTray) {
            const auto available = power->available();
            const bool allowed = SystemAccess::authorized();
            for (auto it = powerTrayActions.cbegin(); it != powerTrayActions.cend(); ++it) {
                it.value()->setEnabled(!power->busy() && (!power->managed() || allowed) && available.contains(it.key()));
                it.value()->setChecked(power->current() == it.key());
            }
            const auto &icon = powerIcon(power->current());
            if (powerTray->iconPixmap().cacheKey() != icon.cacheKey()) {
                powerTray->setIconByPixmap(icon);
            }
            powerTray->setToolTip(icon, tr("Power Profiles"), PowerProfiles::label(power->current()));
        }
        if (!awake) {
            return;
        }
        const auto mode = awake->currentMode();
        const auto end = awake->endTime();
        passive->setChecked(mode == Mode::Passive);
        timedAction->setChecked(mode == Mode::Timed);
        untilAction->setChecked(mode == Mode::Until);
        indefinite->setChecked(mode == Mode::Indefinite);
        screenAction->setChecked(awake->screenOn());
        const auto &glyph = awake->trayIcon();
        awakeTray->setIconByPixmap(glyph);
        QString state = tr("Using normal power settings");
        if (mode != Mode::Passive && !awake->isEffective()) {
            state = tr("Waiting for Plasma power management");
        } else if (mode == Mode::Indefinite) {
            state = tr("Awake indefinitely");
        } else if (end.isValid()) {
            state = tr("Awake until %1").arg(end.toString(u"ddd HH:mm"_s));
        }
        awakeTray->setToolTip(glyph, tr("Always Awake"), state);
    }

    QSettings settings;
    SystemAccess access;
    SettingsWindow *window{};
    std::unique_ptr<PowerProfiles> power;
    std::unique_ptr<TouchpadEdges> edges;
    std::unique_ptr<KStatusNotifierItem> powerTray;
    QHash<QString, QAction *> powerTrayActions;
    bool disablePowerAfterApply = false;
    QAction *toolboxPower{};
    QAction *toolboxEdges{};
    std::unique_ptr<Awake> awake;
    std::unique_ptr<KStatusNotifierItem> toolboxTray;
    std::unique_ptr<KStatusNotifierItem> awakeTray;
    std::optional<bool> drawerEnabled;
    QAction *toolboxAwake{};
    QAction *toolboxDrawer{};
    QAction *passive{};
    QAction *timedAction{};
    QAction *untilAction{};
    QAction *indefinite{};
    QAction *screenAction{};
};
} // namespace

int main(int argc, char **argv)
{
    const bool background = argc == 2 && QByteArrayView(argv[1]) == "--background";
    if (background) {
        const QSettings stored(u"Long Weekend Labs"_s, u"Linux Power Toys"_s);
        if (!stored.value(u"startup/enabled"_s, true).toBool()
            || (!stored.value(u"awake/enabled"_s, false).toBool()
                && !stored.value(u"power/enabled"_s, true).toBool()
                && !stored.value(u"edges/enabled"_s, false).toBool()
                && !stored.value(u"drawer/enabled"_s, false).toBool())) {
            return 0;
        }
    }
    QApplication app(argc, argv);
    app.setApplicationName(u"Linux Power Toys"_s);
    app.setOrganizationName(u"Long Weekend Labs"_s);
    app.setDesktopFileName(service);
    app.setQuitOnLastWindowClosed(false);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical("Linux Power Toys requires a session D-Bus connection.");
        return 1;
    }
    if (!bus.registerService(service)) {
        if (app.arguments().contains(u"--background"_s)) {
            return 0;
        }
        if (!bus.interface()->isServiceRegistered(service).value()) {
            return 1;
        }
        auto call = QDBusMessage::createMethodCall(service, objectPath, service, u"ShowSettings"_s);
        return bus.call(call, QDBus::Block, 2000).type() == QDBusMessage::ReplyMessage ? 0 : 1;
    }
    Toolbox toolbox(background);
    if (!bus.registerObject(objectPath, &toolbox, QDBusConnection::ExportAllSlots)) {
        return 1;
    }
    if (background) {
        toolbox.finishBackgroundStartup();
    } else {
        toolbox.ShowSettings();
    }
    return app.exec();
}

#include "main.moc"
