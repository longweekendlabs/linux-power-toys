#pragma once

#include <QDBusServiceWatcher>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

class SystemAccess final : public QObject {
    Q_OBJECT

public:
    explicit SystemAccess(QObject *parent = nullptr);
    static bool installed();
    static bool authorized();
    bool busy() const;
    QString error() const { return errorMessage; }
    void authorize();

signals:
    void changed();

private:
    QProcess process;
    QString errorMessage;
};

class PowerProfiles final : public QObject {
    Q_OBJECT

public:
    explicit PowerProfiles(QObject *parent = nullptr);
    static QString label(const QString &profile);
    static QStringList modes();
    QStringList available() const;
    QString current() const;
    QString systemProfile() const;
    QString details() const;
    QString error() const { return errorMessage; }
    bool managed() const;
    bool applying() const;
    bool busy() const;
    bool needsAccess(const QString &profile) const;
    void refresh();
    void select(const QString &profile);

signals:
    void changed();
    void applied(bool success);

private slots:
    void profilesChanged(const QString &interface, const QVariantMap &changes, const QStringList &invalidated);

private:
    void run(const QString &profile = {});
    void finished(int code);
    void complete(bool success);

    QProcess process;
    QDBusServiceWatcher service;
    QJsonObject status;
    QString operation;
    QString errorMessage;
    bool pendingRefresh = false;
};

class TouchpadEdges final : public QObject {
    Q_OBJECT

public:
    explicit TouchpadEdges(QObject *parent = nullptr);
    ~TouchpadEdges() override;
    static QString unavailableReason();
    bool active() const { return ready; }
    bool running() const;
    QString error() const { return errorMessage; }
    void start();
    void stop();

signals:
    void changed();

private:
    void messages();
    void confirmDevice();
    void finished(int code);
    void shortcut(const QString &action);

    QProcess process;
    QDBusServiceWatcher compositor;
    QTimer retry;
    QString device;
    QString errorMessage;
    bool ready = false;
    bool stopping = false;
    int attempts = 0;
};
