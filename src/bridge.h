#pragma once
#include <QObject>
#include <QQuickItem>
#include <QQmlComponent>
#include <QPointer>
#include <QJSValue>
#include <QVariantMap>
#include <QTimer>
#include <QSet>

class ManagerBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString uiLanguage READ uiLanguage NOTIFY languageChanged)
public:
    explicit ManagerBridge(QObject *parent=nullptr);
    QString uiLanguage() const;
    Q_INVOKABLE QString entryTitle(const QVariantMap &entry);
    Q_INVOKABLE QString translate(const QString &context,const QString &source);
    Q_INVOKABLE QVariantMap request(const QString &command, const QVariantMap &args = {});
signals:
    void languageChanged();
};
class SettingsContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool systemNavigationAvailable READ systemNavigationAvailable NOTIFY navigationChanged)
    Q_PROPERTY(QString pluginId READ pluginId CONSTANT)
    Q_PROPERTY(QString uiLanguage READ uiLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString launchSource READ launchSource NOTIFY presentationChanged)
    Q_PROPERTY(bool hostHeaderVisible READ hostHeaderVisible NOTIFY presentationChanged)
    Q_PROPERTY(bool hostFooterVisible READ hostFooterVisible CONSTANT)
    Q_PROPERTY(bool notificationActionsEnabled READ notificationActionsEnabled WRITE setNotificationActionsEnabled NOTIFY notificationActionsEnabledChanged)
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit SettingsContext(QString id, QObject *parent=nullptr);
    bool systemNavigationAvailable() const { return navigationHandler_.isCallable(); }
    void setNavigationHandler(const QJSValue &handler) { navigationHandler_=handler; emit navigationChanged(); }
    Q_INVOKABLE QVariantMap openSystemSettings(const QString &target);
    Q_INVOKABLE QVariantMap openSettings(const QString &ownerId, const QString &pageId="main");
    QString pluginId() const { return id_; }
    QString uiLanguage() const { return uiLanguage_; }
    QString launchSource() const { return launchSource_; }
    bool hostHeaderVisible() const { return hostHeader_; }
    bool hostFooterVisible() const { return false; }
    void setPresentation(const QString &source,bool header) { launchSource_=source;hostHeader_=header;emit presentationChanged(); }
    Q_INVOKABLE void requestPin() { emit pinRequested(); }
    QVariantMap values() const { return values_; }
    int revision() const { return revision_; }
    QString error() const { return error_; }
    bool notificationActionsEnabled() const { return actionTimer_.isActive(); }
    void setNotificationActionsEnabled(bool enabled);
    Q_INVOKABLE QVariantMap takeNotificationActions();
    Q_INVOKABLE QVariantMap notify(const QVariantMap &notification);
    Q_INVOKABLE QVariantMap dismissNotification(const QString &notificationId);
    Q_INVOKABLE bool save(const QVariantMap &changes);
    Q_INVOKABLE bool reload();
    Q_INVOKABLE QVariantMap injectionStates();
    Q_INVOKABLE QVariantMap setInjectionEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void close() { emit closeRequested(); }
    Q_INVOKABLE QString sendPluginSignal(const QString &signal, const QString &message);
signals:
    void languageChanged();
    void presentationChanged();
    void pinRequested();
    void notificationActionsEnabledChanged();
    void notificationAction(const QVariantMap &action);
    void navigationChanged();
    void changed();
    void closeRequested();
private:
    QTimer actionTimer_;
    QJSValue navigationHandler_;
    QString id_, error_, uiLanguage_, launchSource_="manager";
    bool hostHeader_=true;
    QVariantMap values_;
    int revision_=0;
};
class SettingsHost : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QJSValue navigationHandler READ navigationHandler WRITE setNavigationHandler NOTIFY navigationHandlerChanged)
    Q_PROPERTY(QVariantMap page READ page WRITE setPage NOTIFY pageChanged)
    Q_PROPERTY(QString launchSource READ launchSource WRITE setLaunchSource NOTIFY launchSourceChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString errorSummary READ errorSummary NOTIFY stateChanged)
    Q_PROPERTY(QString recoveryHint READ recoveryHint NOTIFY stateChanged)
public:
    using QQuickItem::QQuickItem;
    ~SettingsHost() override;
    QJSValue navigationHandler() const { return navigationHandler_; }
    void setNavigationHandler(const QJSValue &handler);
    QVariantMap page() const { return page_; }
    QString launchSource() const { return launchSource_; }
    void setLaunchSource(const QString &source);
    QString state() const { return state_; }
    QString error() const { return error_; }
    QString errorSummary() const;
    QString recoveryHint() const;
    Q_INVOKABLE void retry();
    void setPage(const QVariantMap &page);
signals:
    void launchSourceChanged();
    void pinRequested();
    void navigationHandlerChanged();
    void pageChanged();
    void stateChanged();
    void closeRequested();
protected:
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
private:
    void load();
    void finish();
    void clear();
    void report(const QString &state, const QString &message={});
    QJSValue navigationHandler_;
    QVariantMap page_;
    QString state_="unloaded", error_, launchSource_="manager";
    QPointer<QQmlComponent> component_;
    bool ownsComponent_=true;
    QQuickItem *item_=nullptr;
    SettingsContext *context_=nullptr;
    QMetaObject::Connection warningsConnection_;
    QMetaObject::Connection languageConnection_;
    quint64 generation_=0;
};

// Process-wide launch signal: xochitl adapters own presentation and navigation.
class ManagerNavigation : public QObject {
    Q_OBJECT
    Q_PROPERTY(int launcherRevision READ launcherRevision NOTIFY launchersChanged)
public:
    using QObject::QObject;
    static ManagerNavigation *shared();
    Q_INVOKABLE void registerLauncher(QObject *owner);
    Q_INVOKABLE void setNativeLanguage(const QString &language);
    Q_INVOKABLE void registerSettingsHost(QQuickItem *host);
    Q_INVOKABLE bool settingsHostVisible() const;
    // Registration is session-scoped; owner destruction removes its entries.
    Q_INVOKABLE QVariantMap registerPage(QObject *owner, const QVariantMap &page);
    Q_INVOKABLE QVariantMap unregisterPage(QObject *owner, const QString &id, const QString &pageId="main");
    Q_INVOKABLE QVariantMap openPage(const QVariantMap &page);
    QQmlComponent *pageComponent(const QString &token) const;

    Q_INVOKABLE QVariantMap requestOpenSettings(const QString &ownerId,const QString &pageId="main");
    Q_INVOKABLE void openSettings(const QString &id,const QString &pageId,const QString &source="shortcut") {
        // Leave the caller's signal/Loader destruction stack before opening.
        QTimer::singleShot(0,this,[this,id,pageId,source] { emit openRequested(id,pageId,source); });
    }
    Q_INVOKABLE void toggleQuickNotifications() { emit quickNotificationsRequested(); }
    Q_INVOKABLE void openNotifications() { emit notificationsRequested(); }
    int launcherRevision() const { return launcherRevision_; }
    Q_INVOKABLE bool launcherEntryEnabled(const QString &owner, const QString &page, const QString &location, bool fallback = false);
    Q_INVOKABLE bool nativeEntryEnabled(const QString &page, const QString &location);
    Q_INVOKABLE void notifyLaunchersChanged() { ++launcherRevision_; emit launchersChanged(); }
signals:
    void openRequested(const QString &id,const QString &pageId,const QString &source);
    void pageOpenRequested(const QVariantMap &page);
    void notificationsRequested();
    void quickNotificationsRequested();
    void launchersChanged();
private:
    QVariantMap normalizePage(const QVariantMap &page, bool registered);
    struct Registration { QPointer<QObject> owner; QString token; };
    QHash<QString,Registration> registrations_;
    QHash<QString,QPointer<QQmlComponent>> components_;
    QSet<QObject *> launchers_;
    QSet<QQuickItem *> settingsHosts_;
    int launcherRevision_ = 0;
    int nativeCacheRevision_ = -1;
    QVariantList nativeEntries_;
    QVariantList launcherEntries_;
};
