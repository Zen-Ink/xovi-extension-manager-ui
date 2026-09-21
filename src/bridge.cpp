#include "bridge.h"
#include "../sdk/xovi-i18n.h"
#include "../xovi.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQmlContext>
#include <QTimer>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <QJsonArray>
#include <QFile>
#include <QUuid>
#include <QRegularExpression>
#include "../sdk/xovi-navigation.h"

static QString signalRequest(const QString &signal,const QString &message) {
    auto name=signal.toUtf8(), data=message.toUtf8();
    int hits=0;
    char *response=reinterpret_cast<char *(*)(const char *,const char *,int *)>(xovi_message_broker$broadcastToNative)(name.constData(),data.constData(),&hits);
    QString result=hits==1 && response ? QString::fromUtf8(response) : QString();
    std::free(response);
    return result;
}
namespace {
std::atomic<bool> navigationReady{false};
QVariantMap requestSettings(const QString &ownerId,const QString &pageId) {
    static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$"));
    if (!idPattern.match(ownerId).hasMatch() || !idPattern.match(pageId).hasMatch())
        return {{"ok",false},{"accepted",false},{"error","invalid-id"}};
    auto *app=QCoreApplication::instance();
    if (!app || !navigationReady.load()) return {{"ok",false},{"accepted",false},{"error","ui-not-ready"}};
    QString resolvedOwner=ownerId;
    const bool builtIn=ownerId=="xovi-extension-manager" && (pageId=="inventory" || pageId=="notifications");
    if (!builtIn) {
        // Pure native broker call: do not create a QObject on the caller thread.
        const auto reply=QJsonDocument::fromJson(signalRequest("xovi-extension-manager$settingsList","{}").toUtf8()).object();
        if (!reply.value("ok").toBool()) return {{"ok",false},{"accepted",false},{"error","manager-unavailable"}};
        bool found=false;
        for (const auto &entry:reply.value("pages").toArray()) {
            const auto page=entry.toObject();
            if ((page.value("id").toString()==ownerId || page.value("packageId").toString()==ownerId) && page.value("pageId").toString()==pageId) {
                if (!page.value("available").toBool()) return {{"ok",false},{"accepted",false},{"error","page-unavailable"}};
                resolvedOwner=page.value("id").toString();found=true;break;
            }
        }
        if (!found) return {{"ok",false},{"accepted",false},{"error","page-not-found"}};
    }
    QMetaObject::invokeMethod(app,[resolvedOwner,pageId] {
        if (navigationReady.load()) ManagerNavigation::shared()->openSettings(resolvedOwner,pageId,"plugin");
    },Qt::QueuedConnection);
    return {{"ok",true},{"accepted",true},{"state","queued"},{"ownerId",resolvedOwner},{"pageId",pageId}};
}
char *navigationReply(const QVariantMap &value) {
    return strdup(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact).constData());
}
void freeNavigationString(char *value) { std::free(value); }
char *openSettingsNative(const char *ownerId,const char *pageId) {
    if (!ownerId || strnlen(ownerId,129)>128 || (pageId && strnlen(pageId,129)>128))
        return navigationReply({{"ok",false},{"accepted",false},{"error","invalid-id"}});
    return navigationReply(requestSettings(QString::fromUtf8(ownerId),pageId ? QString::fromUtf8(pageId) : QStringLiteral("main")));
}
}
extern "C" const XemNavigationApiV1 *xem_get_navigation_api_v1() {
    static const XemNavigationApiV1 api{XEM_NAVIGATION_ABI,sizeof(XemNavigationApiV1),openSettingsNative,freeNavigationString};
    return &api;
}
extern "C" char *xem_open_settings(const char *request) {
    if (!request || strnlen(request,4097)>4096) return navigationReply({{"ok",false},{"accepted",false},{"error","invalid-request"}});
    const auto doc=QJsonDocument::fromJson(QByteArray(request));
    const auto args=doc.object();
    if (!doc.isObject() || !args.value("ownerId").isString() || (args.contains("pageId") && !args.value("pageId").isString()))
        return navigationReply({{"ok",false},{"accepted",false},{"error","invalid-request"}});
    return navigationReply(requestSettings(args.value("ownerId").toString(),args.value("pageId").toString("main")));
}
ManagerNavigation *ManagerNavigation::shared() {
    static auto *navigation=new ManagerNavigation(QCoreApplication::instance());
    return navigation;
}
void ManagerNavigation::registerLauncher(QObject *owner) {
    if (!owner || launchers_.contains(owner)) return;
    launchers_.insert(owner);navigationReady.store(true);
    connect(owner,&QObject::destroyed,this,[this,owner] { launchers_.remove(owner);navigationReady.store(!launchers_.isEmpty()); });
}
QVariantMap ManagerNavigation::requestOpenSettings(const QString &ownerId,const QString &pageId) { return requestSettings(ownerId,pageId); }
QVariantMap SettingsContext::openSettings(const QString &ownerId,const QString &pageId) { return requestSettings(ownerId,pageId); }
void ManagerNavigation::registerSettingsHost(QQuickItem *host) {
    if(!host || settingsHosts_.contains(host)) return;
    settingsHosts_.insert(host);
    connect(host,&QObject::destroyed,this,[this,host] { settingsHosts_.remove(host); });
}
bool ManagerNavigation::settingsHostVisible() const {
    for(auto *host:settingsHosts_) if(host->isVisible()) return true;
    return false;
}
QQmlComponent *ManagerNavigation::pageComponent(const QString &token) const {
    return components_.value(token).data();
}
QVariantMap ManagerNavigation::normalizePage(const QVariantMap &input, bool registered) {
    auto fail=[](const QString &error) { return QVariantMap{{"ok",false},{"error",error}}; };
    auto page=input;
    const auto id=page.value("id",registered ? QString() : QStringLiteral("direct-page")).toString();
    const auto pageId=page.value("pageId",QStringLiteral("main")).toString();
    static const QRegularExpression pattern("^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$");
    if(!pattern.match(id).hasMatch() || !pattern.match(pageId).hasMatch()) return fail("invalid-id");
    if(registered && page.value("title").toString().trimmed().isEmpty()) return fail("missing-title");
    const auto chrome=page.value("chrome",QStringLiteral("host")).toString();
    if(chrome!="host" && chrome!="page") return fail("invalid-chrome");
    auto *component=qobject_cast<QQmlComponent *>(page.value("component").value<QObject *>());
    if(!component && page.value("component").canConvert<QJSValue>())
        component=qobject_cast<QQmlComponent *>(page.value("component").value<QJSValue>().toQObject());
    page.remove("component");
    if(component) {
        if(component->thread()!=thread() || (component->creationContext() && !component->creationContext()->isValid())) return fail("invalid-component-context");
        if(component->isError()) return fail("invalid-component");
        QString token;
        for(auto it=components_.cbegin();it!=components_.cend();++it) if(it.value()==component) {token=it.key();break;}
        if(token.isEmpty()) {
            if(components_.size()>=256) return fail("component-limit");
            token=QUuid::createUuid().toString(QUuid::WithoutBraces);
            components_.insert(token,component);
            connect(component,&QObject::destroyed,this,[this,token] { components_.remove(token); });
        }
        page.insert("kind","component");page.insert("source",token);
    } else {
        if(input.contains("component")) return fail("invalid-component");
        const auto kind=page.value("kind",QStringLiteral("url")).toString();
        if(kind!="url" && kind!="inline") return fail("invalid-page-kind");
        const auto source=page.value("url",page.value("source")).toString();
        if(source.isEmpty() || source.toUtf8().size()>1024*1024) return fail("invalid-page");
        const QUrl url(kind=="inline" ? page.value("baseUrl").toString() : source);
        if(!url.isValid() || (url.scheme()!="file" && url.scheme()!="qrc")) return fail("invalid-page-url");
        page.insert("kind",kind);page.insert("source",source);
    }
    page.remove("url");
    if(!page.contains("title")) page.insert("title",QStringLiteral("Settings"));
    page.insert("id",id);page.insert("pageId",pageId);page.insert("chrome",chrome);
    page.insert("usesSettingsContext",input.value("usesSettingsContext",false));
    page.insert("available",true);page.insert("transient",!registered);page.insert("ok",true);
    return page;
}
QVariantMap ManagerNavigation::openPage(const QVariantMap &input) {
    if(!navigationReady.load()) return {{"ok",false},{"accepted",false},{"error","ui-not-ready"}};
    auto page=normalizePage(input,false);
    if(!page.value("ok").toBool()) {page.insert("accepted",false);return page;}
    QTimer::singleShot(0,this,[this,page] { if(navigationReady.load()) emit pageOpenRequested(page); });
    return {{"ok",true},{"accepted",true},{"state","queued"}};
}
QVariantMap ManagerNavigation::registerPage(QObject *owner,const QVariantMap &input) {
    if(!owner || owner->thread()!=thread()) return {{"ok",false},{"error","invalid-owner"}};
    const auto key=input.value("id").toString()+"/"+input.value("pageId",QStringLiteral("main")).toString();
    if(registrations_.contains(key) && registrations_[key].owner!=owner)
        return {{"ok",false},{"error","page-already-registered"}};
    auto page=normalizePage(input,true);
    if(!page.value("ok").toBool()) return page;
    const bool existing=registrations_.contains(key);
    const auto token=existing ? registrations_[key].token : QUuid::createUuid().toString(QUuid::WithoutBraces);
    page.insert("registrationToken",token);
    ManagerBridge bridge;
    auto result=bridge.request("settingsRegister",page);
    if(!result.value("ok").toBool()) return result;
    registrations_.insert(key,{owner,token});
    if(!existing) connect(owner,&QObject::destroyed,this,[this,key,token] {
        if(!registrations_.contains(key) || registrations_[key].token!=token) return;
        ManagerBridge bridge;
        bridge.request("settingsUnregister",{{"id",key.section('/',0,0)},{"pageId",key.section('/',1)},{"registrationToken",token}});
        registrations_.remove(key);notifyLaunchersChanged();
    });
    notifyLaunchersChanged();
    return result;
}
QVariantMap ManagerNavigation::unregisterPage(QObject *owner,const QString &id,const QString &pageId) {
    const auto key=id+"/"+pageId;
    if(!registrations_.contains(key) || registrations_[key].owner!=owner)
        return {{"ok",false},{"error","registration-not-owned"}};
    ManagerBridge bridge;
    auto result=bridge.request("settingsUnregister",{{"id",id},{"pageId",pageId},{"registrationToken",registrations_[key].token}});
    if(result.value("ok").toBool()) {registrations_.remove(key);notifyLaunchersChanged();}
    return result;
}
ManagerBridge::ManagerBridge(QObject *parent):QObject(parent) {
    XoviI18n::prepareCatalog("xovi-extension-manager-ui");
    auto *observer=new XoviI18n::LanguageObserver(this,[this]() { emit languageChanged(); });
    QCoreApplication::instance()->installEventFilter(observer);
    QTimer::singleShot(0,this,[this]() { XoviI18n::attach(qmlEngine(this), "xovi-extension-manager-ui"); });
}
QString ManagerBridge::uiLanguage() const { return XoviI18n::currentLanguage(); }
QString ManagerBridge::translate(const QString &context,const QString &source) {
    return XoviI18n::service()->translate("xovi-extension-manager-ui",context,source);
}
QVariantMap ManagerBridge::request(const QString &command,const QVariantMap &args) {
    // This is the native in-process broker path; it does not use the pipe limit
    // or the broker's blocking QML broadcast.
    // Lifecycle endpoints predate the JSON settings protocol and take a plain id.
    const bool plainId = command=="get" || command=="enable" || command=="disable" || command=="repair";
    const QString payload = plainId ? args.value("id").toString()
        : QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(args)).toJson(QJsonDocument::Compact));
    if (plainId && payload.isEmpty()) return {{"ok",false},{"error","missing-id"}};
    auto text=signalRequest("xovi-extension-manager$"+command, payload);
    auto doc=QJsonDocument::fromJson(text.toUtf8());
    if(!doc.isObject()) return {{"ok",false},{"error","manager-unavailable"}};
    return doc.object().toVariantMap();
}
SettingsContext::SettingsContext(QString id,QObject *parent):QObject(parent),id_(std::move(id)) {
    uiLanguage_=XoviI18n::currentLanguage();
    auto *languageObserver=new XoviI18n::LanguageObserver(this,[this]() {
        const auto language=XoviI18n::currentLanguage();
        if (language!=uiLanguage_) { uiLanguage_=language; emit languageChanged(); }
    });
    QCoreApplication::instance()->installEventFilter(languageObserver);
    reload();
    actionTimer_.setInterval(1000);
    connect(&actionTimer_, &QTimer::timeout, this, [this] {
        const auto result = takeNotificationActions();
        for (const auto &value : result.value("actions").toList()) {
            // A provider may close its page while handling an action.
            QPointer<SettingsContext> alive(this);
            emit notificationAction(value.toMap());
            if (!alive) return;
        }
    });
}
void SettingsContext::setNotificationActionsEnabled(bool enabled) {
    if (enabled == actionTimer_.isActive()) return;
    if (enabled) actionTimer_.start(); else actionTimer_.stop();
    emit notificationActionsEnabledChanged();
}
QVariantMap SettingsContext::takeNotificationActions() {
    ManagerBridge bridge;
    return bridge.request("notificationsPollActions", {{"ownerId", id_}});
}
QVariantMap SettingsContext::openSystemSettings(const QString &target) {
    if(target!="wifi" && target!="language") return {{"ok",false},{"error","unsupported-target"}};
    if(!systemNavigationAvailable()) return {{"ok",false},{"error","navigation-unavailable"}};
    auto result=navigationHandler_.call({QJSValue(target)});
    if(result.isError()) return {{"ok",false},{"error","navigation-failed"},{"message",result.toString()}};
    if(!result.isBool() || !result.toBool()) return {{"ok",false},{"error","navigation-failed"}};
    return {{"ok",true},{"state","dispatched"}};
}
void SettingsHost::setNavigationHandler(const QJSValue &handler) {
    navigationHandler_=handler;
    if(context_) context_->setNavigationHandler(handler);
    emit navigationHandlerChanged();
}
bool SettingsContext::reload() {
    ManagerBridge b;
    auto result=b.request("settingsGet",{{"id",id_}});
    error_=result.value("error").toString();
    if(result.value("ok").toBool()) {values_=result.value("values").toMap();revision_=result.value("revision").toInt();}
    emit changed(); return result.value("ok").toBool();
}
bool SettingsContext::save(const QVariantMap &changes) {
    ManagerBridge b;
    auto result=b.request("settingsUpdate",{{"id",id_},{"expectedRevision",revision_},{"values",changes}});
    error_=result.value("error").toString();
    if(result.value("ok").toBool()) {values_=result.value("values").toMap();revision_=result.value("revision").toInt();}
    emit changed(); return result.value("ok").toBool();
}
QVariantMap SettingsContext::notify(const QVariantMap &notification) {
    auto request=notification;
    request.insert("ownerId",id_);
    ManagerBridge bridge;
    auto result = bridge.request("notificationsPost",request);
    if (result.value("ok").toBool() && !notification.value("actions").toList().isEmpty())
        setNotificationActionsEnabled(true);
    return result;
}
QVariantMap SettingsContext::dismissNotification(const QString &id) {
    ManagerBridge bridge;
    return bridge.request("notificationsDismiss",{{"ownerId",id_},{"notificationId",id}});
}
QVariantMap SettingsContext::injectionStates() {ManagerBridge b;return b.request("injectionsGet");}
QVariantMap SettingsContext::setInjectionEnabled(const QString &id,bool enabled) {
    ManagerBridge b;return b.request("injectionsSet",{{"id",id_},{"injectionId",id},{"enabled",enabled}});
}
QString SettingsContext::sendPluginSignal(const QString &signal,const QString &message) {
    return signalRequest(id_+"$"+signal,message);
}
QString SettingsHost::errorSummary() const {
    if (error_.contains("Non-existent attached object") || error_.contains("Cannot override FINAL property"))
        return tr("This plugin's settings page is not compatible with this device.");
    if (error_.contains("is not installed") || error_.contains("not found", Qt::CaseInsensitive) || error_.contains("No such file"))
        return tr("A component required by this page is missing.");
    if (error_.contains("unavailable", Qt::CaseInsensitive))
        return tr("This settings page is currently unavailable.");
    return tr("This plugin's settings page could not be opened.");
}
QString SettingsHost::recoveryHint() const {
    return tr("Update the plugin, then restart xochitl.");
}
void SettingsHost::retry() {
    if (page_.isEmpty()) return;
    clear();
    load();
}
void SettingsHost::report(const QString &state,const QString &message) {
    state_=state;error_=message;
    if(!page_.isEmpty() && !page_.value("transient").toBool()) {
        ManagerBridge b;b.request("uiReport",{{"id",page_.value("id")},{"pageId",page_.value("pageId")},{"state",state},{"message",message}});
    }
    if(state=="failed" && !page_.isEmpty() && !page_.value("transient").toBool()) {
        ManagerBridge bridge;
        bridge.request("notificationsPost",{{"ownerId",page_.value("id")},
            {"notificationId",QString("settings-")+page_.value("pageId").toString()},
            {"title",page_.value("title", QString("Settings unavailable"))},{"message",errorSummary()+"\n"+recoveryHint()},
            {"level",QString("error")},{"pageId",page_.value("pageId")}});
    }
    emit stateChanged();
}
void SettingsHost::clear() {
    ++generation_;
    disconnect(warningsConnection_);
    disconnect(languageConnection_);
    if(component_) disconnect(component_,nullptr,this,nullptr);
    delete item_;item_=nullptr;
    if(ownsComponent_) delete component_;
    component_=nullptr;
    ownsComponent_=true;
    delete context_;context_=nullptr;
}
SettingsHost::~SettingsHost() {clear();}
void SettingsHost::setPage(const QVariantMap &page) {
    if(page_==page) return;
    if(!page_.isEmpty()) report("unloaded");
    clear();page_=page;emit pageChanged();
    if(isComponentComplete()) load();
}
void SettingsHost::componentComplete() {
    QQuickItem::componentComplete();
    auto *observer=new XoviI18n::LanguageObserver(this,[this]() { emit stateChanged(); });
    QCoreApplication::instance()->installEventFilter(observer);
    load();
}
void SettingsHost::load() {
    if(page_.isEmpty()) {report("unloaded");return;}
    if(!page_.value("available").toBool()) {report("failed","Settings page unavailable");return;}
    auto *hostContext=qmlContext(this);
    // Navigation can destroy the Loader's creation context before queued work
    // runs. This is cancellation, not a broken plugin settings page.
    if (!hostContext || !hostContext->isValid()) { report("unloaded"); return; }
    auto *engine=hostContext->engine();
    XoviI18n::attach(engine, "xovi-extension-manager-ui");
    if(!engine) {report("failed","QML engine unavailable");return;}
    report("loading");
    const QUrl pageUrl(page_.value("kind")=="inline" ? page_.value("baseUrl").toString() : page_.value("source").toString());
    languageConnection_=connect(engine,&QQmlEngine::uiLanguageChanged,this,[this]() { emit stateChanged(); });
    warningsConnection_=connect(engine,&QQmlEngine::warnings,this,[this,pageUrl](const QList<QQmlError> &errors) {
        QStringList messages;
        for(const auto &e:errors) if(e.url()==pageUrl) messages.append(e.toString());
        if(messages.isEmpty()) return;
        // Warnings do not prove creation failed; keep the loading/ready state.
        ManagerBridge b;b.request("uiReport",{{"id",page_.value("id")},{"pageId",page_.value("pageId")},{"state","warning"},{"message",messages.join("\n")}});
    });
    context_=new SettingsContext(page_.value("id").toString(),this);
    context_->setNavigationHandler(navigationHandler_);
    context_->setPresentation(launchSource_,page_.value("chrome")!="page");
    connect(context_,&SettingsContext::pinRequested,this,&SettingsHost::pinRequested);
    const auto contextGeneration=generation_;
    connect(context_,&SettingsContext::closeRequested,this,[this,contextGeneration] {
        if(contextGeneration==generation_) emit closeRequested();
    },Qt::QueuedConnection);
    ownsComponent_=page_.value("kind")!="component";
    component_=ownsComponent_ ? new QQmlComponent(engine,this)
        : ManagerNavigation::shared()->pageComponent(page_.value("source").toString());
    if(!component_ || (!ownsComponent_ && component_->engine()!=engine)) {report("failed","Page component is no longer available in this QML engine");return;}
    if(!ownsComponent_) {
        const auto currentGeneration=generation_;
        auto cancel=[this,currentGeneration] {
            QTimer::singleShot(0,this,[this,currentGeneration] {
                if(generation_==currentGeneration) {clear();report("failed","Page component owner was destroyed");}
            });
        };
        connect(component_,&QObject::destroyed,this,cancel);
        if(component_->creationContext()) connect(component_->creationContext(),&QObject::destroyed,this,cancel);
    }
    const auto generation=generation_;
    connect(component_,&QQmlComponent::statusChanged,this,[this,generation] {
        // Finish outside the component signal stack; stale completions are ignored.
        QTimer::singleShot(0,this,[this,generation]{if(generation==generation_) finish();});
    });
    if(ownsComponent_ && page_.value("kind")=="inline")
        component_->setData(page_.value("source").toString().toUtf8(),QUrl(page_.value("baseUrl").toString()));
    else if(ownsComponent_) component_->loadUrl(QUrl(page_.value("source").toString()),QQmlComponent::Asynchronous);
    QTimer::singleShot(0,this,[this,generation]{if(generation==generation_) finish();});
}
void SettingsHost::finish() {
    if(!component_ || item_ || component_->isLoading() || state_=="failed") return;
    if(component_->isError()) {report("failed",component_->errorString());return;}
    if(!component_->isReady()) return;
    auto *hostContext=qmlContext(this);
    if (!hostContext || !hostContext->isValid()) { report("unloaded"); return; }
    auto *engine=hostContext->engine();
    if (!engine || !engine->rootContext()->isValid()) { report("unloaded"); return; }
    // The declarative manager Loader owns this context, rather than the
    // notification callback. Preserve native ambient services (ghostBuster,
    // etc.) while giving each provider a separately owned child context.
    auto *creationContext=!ownsComponent_ ? component_->creationContext() : hostContext;
    if(creationContext && !creationContext->isValid()) {report("failed","Page component context is no longer valid");return;}
    auto *pageContext=new QQmlContext(creationContext ? creationContext : hostContext,context_);
    if (!pageContext->isValid()) { delete pageContext; report("unloaded"); return; }
    const bool usesContext=page_.value("usesSettingsContext",true).toBool();
    const QVariantMap initial=usesContext ? QVariantMap{{"settingsContext",QVariant::fromValue(context_)}} : QVariantMap{};
    auto *object=component_->createWithInitialProperties(initial,pageContext);
    item_=qobject_cast<QQuickItem *>(object);
    if(!item_ || (usesContext && object->metaObject()->indexOfProperty("settingsContext")<0)) {item_=nullptr;delete object;report("failed",component_->errorString().isEmpty() ? "Settings root must be an Item with a settingsContext property" : component_->errorString());return;}
    item_->setParent(this);item_->setParentItem(this);
    item_->setSize(size());
    report("ready");
}
void SettingsHost::geometryChange(const QRectF &a,const QRectF &b) {
    QQuickItem::geometryChange(a,b);if(item_) item_->setSize(a.size());
}

bool ManagerNavigation::nativeEntryEnabled(const QString &page, const QString &location) {
    if (nativeCacheRevision_ != launcherRevision_) {
        ManagerBridge bridge;
        const auto reply = bridge.request("launcherList", {});
        if (!reply.value("ok").toBool()) return true;
        nativeEntries_ = reply.value("nativeEntries").toList();
        launcherEntries_ = reply.value("entries").toList();
        nativeCacheRevision_ = launcherRevision_;
    }
    // A missing manager or unknown native item must not remove navigation.
    for (const auto &value : nativeEntries_) {
        const auto entry = value.toMap();
        if (entry.value("pageId").toString()==page && entry.value("location").toString()==location)
            return entry.value(location, true).toBool();
    }
    return true;
}

void SettingsHost::setLaunchSource(const QString &source) {
    if (launchSource_==source) return;
    launchSource_=source;
    if (context_) context_->setPresentation(source,page_.value("chrome")!="page");
    emit launchSourceChanged();
}

bool ManagerNavigation::launcherEntryEnabled(const QString &owner, const QString &page, const QString &location, bool fallback) {
    nativeEntryEnabled(QString(), QString()); // refresh the shared revision cache
    for (const auto &value:launcherEntries_) {
        const auto entry=value.toMap();
        if(entry.value("id").toString()==owner && entry.value("pageId").toString()==page)
            return entry.value("available").toBool() && entry.value(location,fallback).toBool();
    }
    return fallback;
}

QString ManagerBridge::entryTitle(const QVariantMap &entry) {
    const auto localized=entry.value("titleTranslations").toMap().value(uiLanguage()).toString();
    if(!localized.isEmpty()) return localized;
    const auto source=entry.value("title",entry.value("name",entry.value("id"))).toString();
    const auto context=entry.value("titleContext",QStringLiteral("XoviPluginNames")).toString();
    auto catalog=entry.value("translationCatalog",entry.value("id")).toString();
    if(entry.value("native").toBool() || catalog=="xovi-extension-manager") catalog="xovi-extension-manager-ui";
    // Catalogs remain plugin-owned. Resolve explicitly instead of depending on
    // global translator install order or whether a page has been opened yet.
    if(QFile::exists(":/xovi/i18n/"+catalog+"/"+catalog+"_en.qm")) {
        const auto translated=XoviI18n::service()->translate(catalog,context,source);
        if(translated!=source) return translated;
    }
    const auto translated=translate(context,source);
    return translated==source ? QCoreApplication::translate(context.toUtf8().constData(),source.toUtf8().constData()) : translated;
}
