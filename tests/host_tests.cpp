#include "../src/bridge.h"
#include "../../xovi-extension-manager/src/notifications.h"
#include <QGuiApplication>
#include "../sdk/xovi-i18n.h"
#include <QTemporaryDir>
#include <QFile>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QQmlContext>
#include <QQuickWindow>
#include <QJSValue>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include "../sdk/xovi-navigation.h"
extern "C" const XemNavigationApiV1 *xem_get_navigation_api_v1();
extern "C" char *xem_open_settings(const char *);
static bool sidebarPin=true, bottomPin=true, nativeSidebar=true, settingsPin=true;
static QString lastLifecycleCommand, lastLifecycleId;
static QJsonObject mockRuntimePages;
static char *mockBroker(const char *signal,const char *request,int *hits) {
    const QMap<QString,QString> commands{{"notificationsPost","post"},{"notificationsList","list"},{"notificationsRead","markRead"},{"notificationsDismiss","dismiss"},{"notificationsClear","clear"},{"notificationsActionInvoke","actionInvoke"},{"notificationsPollActions","pollActions"}};
    const auto name=QString::fromUtf8(signal).section('$',1);
    if(commands.contains(name)) { *hits=1;return strdup(notificationCommand(commands[name].toStdString(),request).c_str()); }
    *hits=1;
    if (name=="get" || name=="enable" || name=="disable" || name=="repair") {
        lastLifecycleCommand=name; lastLifecycleId=QString::fromUtf8(request);
        return strdup(lastLifecycleId=="example" ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"invalid-id\"}");
    }
    if(name=="settingsRegister" || name=="settingsUnregister") {
        auto page=QJsonDocument::fromJson(request).object();
        auto key=page.value("id").toString()+"/"+page.value("pageId").toString();
        if(name=="settingsRegister") {page.insert("provider","runtime");page.insert("available",true);mockRuntimePages.insert(key,page);}
        else mockRuntimePages.remove(key);
        return strdup("{\"ok\":true}");
    }
    if (name=="launcherSet") {
        const auto arg=QJsonDocument::fromJson(request).object();
        if(arg.value("id")!="example" || arg.value("pageId")!="main") return strdup("{\"ok\":false}");
        if(arg.value("location")=="sidebar") sidebarPin=arg.value("enabled").toBool();
        if(arg.value("location")=="bottom") bottomPin=arg.value("enabled").toBool();
        if(arg.value("location")=="settings") settingsPin=arg.value("enabled").toBool();
        return strdup("{\"ok\":true}");
    }
    if (std::strcmp(signal,"xovi-extension-manager$list")==0)
        return strdup(R"({"ok":true,"packages":[{"id":"example","name":"Example","type":"extension","version":"1.0","enabled":true,"requiresRestart":true,"runtime":{"loadStateName":"link-failed","loadError":"missing symbol"},"issues":["runtime-link-failed"],"availableActions":["disable"]},{"id":"legacy","name":"Legacy","type":"extension","runtime":{"loadStateName":"initialized"},"availableActions":["disableLegacy"]}]})");
    if (std::strcmp(signal,"xovi-extension-manager$launcherList")==0) {
        QJsonObject result{{"ok",true},{"entries",QJsonArray{QJsonObject{{"id","example"},{"pageId","main"},{"title","Preferences"},{"sidebar",sidebarPin},{"bottom",bottomPin},{"settings",settingsPin},{"available",true}}}},
            {"nativeEntries",QJsonArray{QJsonObject{{"id","xochitl"},{"pageId","my-files"},{"title","My files"},{"titleContext","NativeLaunchers"},{"native",true},{"location","sidebar"},{"sidebar",nativeSidebar},{"bottom",false},{"available",true}}}}};
        return strdup(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    }
    if (std::strcmp(signal,"xovi-extension-manager$settingsList")==0) {
        auto reply=QJsonDocument::fromJson(R"({"ok":true,"pages":[{"id":"example","pageId":"main","title":"Preferences","available":true},{"id":"example","pageId":"status","title":"Status","available":true}],"uiStates":{"example/main":{"state":"failed","message":"bad page"}}})").object();
        auto pages=reply.value("pages").toArray();
        for(auto page:mockRuntimePages) pages.append(page);
        reply.insert("pages",pages);
        return strdup(QJsonDocument(reply).toJson(QJsonDocument::Compact).constData());
    }
    *hits=1;return strdup("{\"ok\":true,\"revision\":0,\"values\":{},\"packages\":[],\"pages\":[],\"results\":[]}");
}
extern "C" {const void *LINKTABLEVALUES[]={nullptr,nullptr,nullptr,reinterpret_cast<const void *>(mockBroker),nullptr};}
static void check(bool b,const char *message){if(!b){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}std::printf("PASS %s\n",message);}
static QQuickItem *visualChild(QQuickItem *parent, const QString &name) {
    if(parent->objectName()==name) return parent;
    for(auto *child:parent->childItems()) if(auto *found=visualChild(child,name)) return found;
    return nullptr;
}
static void settle(SettingsHost *h){QElapsedTimer t;t.start();while(h->state()=="loading" && t.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(1);}}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    QTemporaryDir languageDir;
    const auto languagePath=languageDir.path()+"/xochitl.conf";
    qputenv("XOVI_LANGUAGE_SETTINGS",languagePath.toUtf8());
    { QFile config(languagePath);check(config.open(QIODevice::WriteOnly),"open language fixture");config.write("[General]\nLanguage=en\n"); }
    QQmlEngine engine;
    engine.addImportPath(QString::fromLocal8Bit(argv[1])+"/xovi-extension-manager-ui/tests/mocks");
    qmlRegisterSingletonType<ManagerNavigation>("org.xovi.Manager",1,0,"ManagerNavigation",[](QQmlEngine *,QJSEngine *) -> QObject * { auto *navigation=ManagerNavigation::shared(); QQmlEngine::setObjectOwnership(navigation,QQmlEngine::CppOwnership); return navigation; });
    qmlRegisterSingletonType("org.xovi.EpubPreloader",1,0,"Preloader",[](QQmlEngine *engine,QJSEngine *) {
        auto result=engine->newObject();
        result.setProperty("queuedCount",0);result.setProperty("idleEnabled",false);
        result.setProperty("summaryText","");result.setProperty("busy",false);result.setProperty("statusText","");
        return result;
    });
    qmlRegisterType<ManagerBridge>("org.xovi.Manager",1,0,"ManagerBridge");
    qmlRegisterType<SettingsHost>("org.xovi.Manager",1,0,"SettingsHost");
    ManagerNavigation nativeNavigation;
    engine.rootContext()->setContextProperty("nativeNavigation", &nativeNavigation);
    const auto *navigationApi=xem_get_navigation_api_v1();
    auto nativeOpen=[&](const char *id,const char *pageId) {
        char *raw=navigationApi->openSettings(id,pageId);
        const auto result=QJsonDocument::fromJson(raw).object();navigationApi->freeString(raw);return result;
    };
    check(navigationApi->abiVersion==1 && navigationApi->structSize>=sizeof(XemNavigationApiV1),"navigation C ABI table is discoverable");
    check(nativeOpen("example","main").value("error")=="ui-not-ready","native navigation reports unavailable UI without claiming success");
    QQuickWindow settingsNativeWindow;
    settingsNativeWindow.show();
    QQuickItem settingsWindow(settingsNativeWindow.contentItem());
    settingsWindow.setVisible(false);
    auto *windowHost=new QQuickItem(&settingsWindow);
    ManagerNavigation::shared()->registerSettingsHost(windowHost);
    check(!ManagerNavigation::shared()->settingsHostVisible(),"hidden settings window does not capture shortcut navigation");
    settingsWindow.setVisible(true);
    check(ManagerNavigation::shared()->settingsHostVisible(),"visible settings window is preferred over background MainView");
    delete windowHost;
    settingsNativeWindow.hide();
    check(!ManagerNavigation::shared()->settingsHostVisible(),"destroyed settings window releases navigation host");
    QObject launcherOwner;
    auto *sharedNavigation=ManagerNavigation::shared();
    sharedNavigation->registerLauncher(&launcherOwner);
    QString apiOwner,apiPage,apiSource;
    const auto apiConnection=QObject::connect(sharedNavigation,&ManagerNavigation::openRequested,[&](const QString &owner,const QString &page,const QString &source) { apiOwner=owner;apiPage=page;apiSource=source; });
    QJsonObject workerReply;
    std::thread worker([&] { workerReply=nativeOpen("example",nullptr); });worker.join();
    check(workerReply.value("accepted").toBool() && workerReply.value("state")=="queued" && apiOwner.isEmpty(),"worker navigation queues without executing QML or claiming page ready");
    for(int i=0;i<4;++i) QCoreApplication::processEvents();
    check(apiOwner=="example" && apiPage=="main" && apiSource=="plugin","native navigation reaches GUI with copied IDs");
    check(nativeOpen("missing","main").value("error")=="page-not-found","unknown provider rejected before navigation");
    check(nativeOpen("../bad","main").value("error")=="invalid-id","invalid navigation ID rejected");
    char *invalidRequest=xem_open_settings("{\"ownerId\":12}");
    check(QJsonDocument::fromJson(invalidRequest).object().value("error")=="invalid-request","broker navigation validates JSON field types");navigationApi->freeString(invalidRequest);
    check(sharedNavigation->requestOpenSettings("example","status").value("accepted").toBool(),"QML navigation uses the same validated API");
    SettingsContext externalContext("caller");
    check(externalContext.openSettings("example","main").value("accepted").toBool(),"hosted plugin context can open another plugin's settings");
    QQmlComponent nativeItem(&engine);
    nativeItem.setData("import QtQuick\nItem { visible: { nativeNavigation.launcherRevision; return nativeNavigation.nativeEntryEnabled(\"my-files\", \"sidebar\") } }", QUrl("qrc:/test/NativeItem.qml"));
    QScopedPointer<QObject> nativeObject(nativeItem.create());
    check(nativeObject && nativeObject->property("visible").toBool(), "native item starts visible");
    nativeSidebar=false; nativeNavigation.notifyLaunchersChanged();
    check(!nativeObject->property("visible").toBool(), "native visibility binding reacts to saved preference revision");
    check(nativeNavigation.nativeEntryEnabled("unknown-action", "bottom"), "unknown native actions remain visible");
    nativeSidebar=true, settingsPin=true; nativeNavigation.notifyLaunchersChanged();
    QQmlComponent host(&engine);host.setData("import QtQuick\nimport org.xovi.Manager 1.0\nSettingsHost { width: 800; height: 600 }",QUrl("qrc:/test/Host.qml"));
    QScopedPointer<QObject> object(host.create());
    if(!object) qWarning()<<host.errors();
    auto *h=qobject_cast<SettingsHost *>(object.data());check(h,"host instantiated");
    // Direct pages have no provider registration or settingsContext requirement.
    QVariantMap deliveredPage;
    auto directConnection=QObject::connect(sharedNavigation,&ManagerNavigation::pageOpenRequested,[&](const QVariantMap &p) { deliveredPage=p; });
    const auto registrationsBefore=mockRuntimePages.size();
    check(sharedNavigation->openPage({{"kind","inline"},{"baseUrl","qrc:/test/Direct.qml"},
        {"source","import QtQuick; Item { objectName: 'directRoot' }"}}).value("accepted").toBool(),"unregistered inline page accepted");
    check(deliveredPage.isEmpty(),"direct opening leaves caller signal stack first");
    QCoreApplication::processEvents();
    check(deliveredPage.value("transient").toBool() && mockRuntimePages.size()==registrationsBefore,"direct page creates no persistent registration");
    h->setPage(deliveredPage);settle(h);
    check(h->state()=="ready" && visualChild(h,"directRoot"),"ordinary Item opens without SDK properties");
    check(!sharedNavigation->openPage({{"url","https://example.com/Page.qml"}}).value("ok").toBool(),"direct remote URL rejected");
    QTemporaryDir directDir;
    QFile directFile(directDir.path()+"/Page.qml");check(directFile.open(QIODevice::WriteOnly),"direct URL fixture");
    directFile.write("import QtQuick; Item { objectName: 'urlRoot' }");directFile.close();
    check(sharedNavigation->openPage({{"url",QUrl::fromLocalFile(directFile.fileName())}}).value("accepted").toBool(),"direct file URL accepted without an owner ID");
    QCoreApplication::processEvents();h->setPage(deliveredPage);settle(h);
    check(h->state()=="ready" && visualChild(h,"urlRoot"),"direct URL Item rendered");
    auto *borrowed=new QQmlComponent(&engine);
    borrowed->setData("import QtQuick; Item { objectName: 'componentRoot' }",QUrl("qrc:/test/Borrowed.qml"));
    check(sharedNavigation->openPage({{"component",QVariant::fromValue(borrowed)},{"chrome","page"}}).value("accepted").toBool(),"direct Component accepted");
    QCoreApplication::processEvents();h->setPage(deliveredPage);settle(h);
    check(h->state()=="ready" && visualChild(h,"componentRoot"),"borrowed Component instantiated");
    h->setPage({});
    check(borrowed->isReady(),"host does not delete caller-owned Component");
    h->setPage(deliveredPage);settle(h);delete borrowed;
    for(int i=0;i<4;++i) QCoreApplication::processEvents();
    check(h->state()=="failed" && !visualChild(h,"componentRoot"),"destroyed Component safely unloads page with a useful error");
    h->setPage({});
    auto *registrationOwner=new QObject;
    QVariantMap runtimeDescriptor{{"id","qmd-example"},{"title","QMD example"},{"kind","inline"},
        {"baseUrl","qrc:/test/Runtime.qml"},{"source","import QtQuick; Item {}"}};
    check(sharedNavigation->registerPage(registrationOwner,runtimeDescriptor).value("ok").toBool(),"QML registration API accepts manifest-free page");
    check(sharedNavigation->requestOpenSettings("qmd-example").value("accepted").toBool(),"registered page opens through existing owner/page navigation");
    QObject unrelatedOwner;
    check(sharedNavigation->registerPage(&unrelatedOwner,runtimeDescriptor).value("error")=="page-already-registered","different QML owner cannot replace registration");
    check(sharedNavigation->unregisterPage(&unrelatedOwner,"qmd-example").value("error")=="registration-not-owned","different QML owner cannot unregister page");
    check(sharedNavigation->registerPage(registrationOwner,runtimeDescriptor).value("ok").toBool(),"same owner can update registration");
    delete registrationOwner;
    check(!mockRuntimePages.contains("qmd-example/main"),"owner destruction unregisters runtime page");
    check(sharedNavigation->requestOpenSettings("qmd-example").value("error")=="page-not-found","dead runtime registration cannot be reopened");
    QQmlComponent qmdCaller(&engine);
    qmdCaller.setData(R"(
        import QtQuick
        Item {
            id: caller
            property string marker: "original-qml-scope"
            property var api
            property var result
            property Component page: Item { objectName: "qmlBorrowed"; property string value: caller.marker }
            Component.onCompleted: {
                var factory = Qt.createComponent("qrc:/xovi/manager/SettingsApi.qml")
                if (factory.status === Component.Ready) {
                    api = factory.createObject(caller)
                    result = api.registerPage(caller, {id:"qml-caller", title:"QML caller", component:page})
                    if (result.ok) result = api.openPage({component:page})
                }
                factory.destroy()
            }
        }
    )",QUrl("qrc:/test/OptionalCaller.qml"));
    auto *caller=qmdCaller.create();
    if(!caller) qWarning()<<qmdCaller.errors();
    check(caller && caller->property("result").toMap().value("accepted").toBool(),"optional QRC helper registers and opens a Component from QML without a hard Manager import");
    QCoreApplication::processEvents();h->setPage(deliveredPage);settle(h);
    auto *borrowedRoot=visualChild(h,"qmlBorrowed");
    check(h->state()=="ready" && borrowedRoot && borrowedRoot->property("value")=="original-qml-scope","Component preserves caller lexical context");
    delete caller;
    for(int i=0;i<4;++i) QCoreApplication::processEvents();
    check(!mockRuntimePages.contains("qml-caller/main") && h->state()=="failed","caller destruction unregisters entry and invalidates borrowed page safely");
    h->setPage({});
    auto underscorePage=runtimeDescriptor;underscorePage["id"]="_qml-provider";
    check(sharedNavigation->registerPage(&unrelatedOwner,underscorePage).value("ok").toBool()
        && sharedNavigation->requestOpenSettings("_qml-provider").value("accepted").toBool(),"registration and navigation accept the same stable ID syntax");
    sharedNavigation->unregisterPage(&unrelatedOwner,"_qml-provider");
    QObject::disconnect(directConnection);
    auto *transientContext=new QQmlContext(engine.rootContext());
    QScopedPointer<QObject> abandoned(host.create(transientContext));
    auto *abandonedHost=qobject_cast<SettingsHost *>(abandoned.data());
    check(abandonedHost,"temporary host created");
    delete transientContext;
    abandonedHost->setPage({{"id","example"},{"pageId","main"},{"available",true},{"kind","inline"},
        {"baseUrl","qrc:/test/Cancelled.qml"},{"source","import QtQuick; Item { required property var settingsContext }"}});
    settle(abandonedHost);
        check(abandonedHost->state()!="failed","closing caller does not invalidate the isolated provider context");
    class ExplicitContextHost : public SettingsHost { public: void complete() { componentComplete(); } };
    ExplicitContextHost invalidHost;
    auto *expiredContext=new QQmlContext(engine.rootContext());
    QQmlEngine::setContextForObject(&invalidHost,expiredContext);
    invalidHost.complete();
    delete expiredContext;
    invalidHost.setPage({{"id","example"},{"pageId","main"},{"available",true},{"kind","inline"},
        {"baseUrl","qrc:/test/Cancelled.qml"},{"source","import QtQuick; Item { required property var settingsContext }"}});
    check(invalidHost.state()=="unloaded","invalid host context is cancellation, not plugin failure");
    bool navigationDelivered=false;
    QObject::connect(&nativeNavigation,&ManagerNavigation::openRequested,[&](const QString &,const QString &,const QString &) { navigationDelivered=true; });
    nativeNavigation.openSettings("example","main","notification");
    check(!navigationDelivered,"notification navigation leaves caller destruction stack first");
    QCoreApplication::processEvents();
    check(navigationDelivered,"deferred notification navigation is delivered");
    QVariantMap page{{"id","test"},{"pageId","main"},{"available",true},{"kind","inline"},{"baseUrl","qrc:/xovi/test/Main.qml"},
        {"source","import QtQuick\nItem { required property var settingsContext; property string owner: settingsContext.pluginId }"}};
    h->setPage(page);settle(h);check(h->state()=="ready","inline native page created with context");
    page["source"]="import QtQuick\nItem { invalid syntax !!! }";
    h->setPage(page);settle(h);check(h->state()=="failed" && !h->error().isEmpty(),"invalid QML contained by host");
    page["source"]="import QtQuick\nQtObject { required property var settingsContext }";
    h->setPage(page);settle(h);check(h->state()=="failed","nonvisual root rejected");
    page["kind"]="url";page["source"]=QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])+"/examples/settings-qml/Settings.qml").toString();
    h->setPage(page);settle(h);check(h->state()=="ready","pure QML example loads after failure");
    page["source"]=QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])+"/examples/settings-native/Settings.qml").toString();
    h->setPage(page);settle(h);check(h->state()=="ready","native example layout loads");
    h->setPage({});check(h->state()=="unloaded","page unloads");
    page["kind"]="inline";page["source"]="import QtQuick\nItem { required property var settingsContext; required property int missing }";
    h->setPage(page);settle(h);check(h->state()=="failed","missing required property is a creation failure");
    page["source"]="import QtQuick\nItem { required property var settingsContext }";
    h->setPage(page);h->setPage({});QCoreApplication::processEvents();check(h->state()=="unloaded","stale completion cannot recreate a closed page");
    page["source"]="import QtQuick\nItem { required property var settingsContext; Component.onCompleted: settingsContext.close() }";
    int closeCount=0;
    QObject::connect(h,&SettingsHost::closeRequested,[&]{++closeCount;});
    h->setPage(page);settle(h);
    page["source"]="import QtQuick\nItem { required property var settingsContext }";
    h->setPage(page);settle(h);QCoreApplication::processEvents();
    check(closeCount==0,"queued close from old provider cannot close a replacement page");
    QQmlComponent ui(&engine,QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])+"/xovi-extension-manager-ui/resources/qml/Manager.qml"));
    QScopedPointer<QObject> manager(ui.create());if(!manager) std::fprintf(stderr,"%s\n",qPrintable(ui.errorString()));check(!manager.isNull(),"manager UI loads");
    QObject listedOwner;
    check(sharedNavigation->registerPage(&listedOwner,runtimeDescriptor).value("ok").toBool(),"runtime registration added while manager is open");
    const auto packageArray=manager->property("packages").toList();
    bool foundRuntime=false;
    for(const auto &package:packageArray) if(package.toMap().value("id")=="qmd-example") foundRuntime=true;
    check(foundRuntime,"manifest-free page appears in open manager inventory immediately");
    sharedNavigation->unregisterPage(&listedOwner,"qmd-example");
    QQuickWindow window; window.resize(1200,1600);
    auto *managerItem=qobject_cast<QQuickItem *>(manager.data());
    managerItem->setParentItem(window.contentItem()); managerItem->setSize(QSizeF(1200,1600));
    QStringList warnings;
    auto connection=QObject::connect(&engine,&QQmlEngine::warnings,[&](const QList<QQmlError> &errors){for(const auto &error:errors) warnings.append(error.toString());});
    window.show();
    for(int i=0;i<30;++i){QCoreApplication::processEvents();QThread::msleep(2);}
    for(const auto &size:{QSize(600,900),QSize(1200,1600),QSize(1600,900)}) {
        window.resize(size);managerItem->setSize(size);
        for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(2);}
        check(manager->property("pageSize").toInt()>0,"pagination adapts to viewport");
    }
    window.resize(600,900);managerItem->setSize(QSizeF(600,900));
    for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(2);}
    window.grabWindow().save("/tmp/manager-ui-portrait.png");
    QVariant result;
    check(QMetaObject::invokeMethod(manager.data(),"pagesFor",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QString("example"))) && result.value<QJSValue>().toVariant().toList().size()==2,"all provider pages are available in inventory");
    const auto originalPages=manager->property("pages");
    manager->setProperty("pages",QVariantList{QVariantMap{{"id","example"},{"packageId","manifest-id"},{"pageId","main"}}});
    check(QMetaObject::invokeMethod(manager.data(),"pagesFor",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QString("manifest-id"))) && result.value<QJSValue>().toVariant().toList().size()==1,"settings card resolves provider through canonical manifest id");
    QMetaObject::invokeMethod(manager.data(),"diagnosticsFor",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QVariant(QVariantMap{{"id","manifest-id"}})));
    check(result.toString().contains("bad page"),"aliased package retains runtime provider diagnostics");
    manager->setProperty("pages",originalPages);
    QVariant pkg=QVariantMap{{"id","example"},{"enabled",true},{"type","extension"},{"requiresRestart",true},{"runtime",QVariantMap{{"loadStateName","link-failed"},{"loadError","missing symbol"}}}};
    QMetaObject::invokeMethod(manager.data(),"statusFor",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,pkg));
    check(result.toString().contains("Load failed") && result.toString().contains("Restart required"),"inventory distinguishes runtime failure and pending restart");
    QMetaObject::invokeMethod(manager.data(),"diagnosticsFor",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,pkg));
    check(result.toString().contains("missing symbol") && result.toString().contains("bad page"),"native and settings page errors are visible");
    QMetaObject::invokeMethod(manager.data(),"toggleAction",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QVariant(QVariantMap{{"availableActions",QStringList{"disableLegacy"}}})));
    check(result.toString()=="disableLegacy","legacy plugins use supported lifecycle action");
    for(const auto &warning:warnings) std::fprintf(stderr,"%s\n",qPrintable(warning));
    check(warnings.isEmpty(),"populated manager delegates render without QML warnings");
    auto navigator=engine.evaluate("({lastRoute: '', open: function(route) { this.lastRoute=route; }})");
    manager->setProperty("systemNavigator",QVariant::fromValue(navigator));
    page["source"]="import QtQuick\nItem { required property var settingsContext; property bool navigationReady: settingsContext.systemNavigationAvailable }";
    manager->setProperty("selectedPage",page);
    auto *navigationHost=manager->findChild<SettingsHost *>();
    settle(navigationHost);
    auto *context=navigationHost->findChild<SettingsContext *>();
    check(context && context->systemNavigationAvailable(),"host supplies navigation before page creation");
    check(context->openSystemSettings("wifi").value("ok").toBool() && navigator.property("lastRoute").toString()=="wifi/window/network-select","wifi target mapped by manager adapter");
    check(context->openSystemSettings("language").value("state")=="dispatched" && navigator.property("lastRoute").toString()=="settings/window/language","language target dispatched without claiming window creation");
    check(context->openSystemSettings("arbitrary/path").value("error")=="unsupported-target","unknown navigation target rejected");
    manager->setProperty("systemNavigator",QVariant());
    check(!context->systemNavigationAvailable() && context->openSystemSettings("wifi").value("error")=="navigation-unavailable","navigation availability updates on adapter removal");
    manager->setProperty("systemNavigator",QVariant::fromValue(engine.evaluate("({open: function(route) { throw new Error('test rejection'); }})")));
    check(context->openSystemSettings("wifi").value("error")=="navigation-failed" && manager->property("message").toString().contains("test rejection"),"navigation exceptions surface without breaking settings page");
    check(navigationHost->state()=="ready","navigation failure leaves settings page usable");
    auto posted=context->notify({{"ownerId","spoofed"},{"notificationId","test-complete"},{"title","Completed"},{"message","Ready"},{"pageId","main"}});
    check(posted.value("ok").toBool() && posted.value("notification").toMap().value("ownerId")=="test","QML notification owner bound by settings context");
    QQmlComponent queueComponent(&engine);
    queueComponent.setData("import QtQml\nQtObject { property int calls: 0; property string lastTitle: ''; function enqueue(notification) { ++calls; lastTitle=notification.text } }",QUrl("qrc:/test/Queue.qml"));
    QScopedPointer<QObject> queue(queueComponent.create());check(!!queue,"native notification queue adapter fixture");
    QQmlComponent notificationAgent(&engine,QUrl("qrc:/xovi/manager/NotificationAgent.qml"));
    QScopedPointer<QObject> notifier(notificationAgent.createWithInitialProperties({{"nativeQueue",QVariant::fromValue(queue.data())}}));
    if (!notifier) std::fprintf(stderr, "%s\n", qPrintable(notificationAgent.errorString()));
    check(notifier && queue->property("calls").toInt()==0,"plugin notifications use their own upper-right short prompt");
    auto *store=notifier->property("store").value<QObject *>();
    check(store && store->property("unread").toInt()>0,"notification center has unread session history");
    QMetaObject::invokeMethod(store,"refresh");
    check(queue->property("calls").toInt()==0,"notification polling does not enqueue native duplicate messages");
    check(context->dismissNotification("test-complete").value("ok").toBool(),"plugin can dismiss its own notification");
    // Exercise the real store, rich banner, native broker and owner-bound delivery.
    store->setProperty("lastToast", 0);
    auto task = context->notify({{"notificationId", "import-task"}, {"title", "Importing"},
        {"message", QString(2000, 'x')}, {"state", "running"},
        {"progress", QVariantMap{{"value", 0.2}}},
        {"actions", QVariantList{QVariantMap{{"id", "cancel"}, {"label", "Cancel"}}, QVariantMap{{"id", "details"}, {"label", "Details"}}}}});
    check(task.value("ok").toBool(), "QML task notification accepted");
    QMetaObject::invokeMethod(store, "refresh");
    auto *notifierItem = qobject_cast<QQuickItem *>(notifier.data());
    notifierItem->setParentItem(window.contentItem()); notifierItem->setSize(QSizeF(600,900));
    for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(2);}
    auto *richBanner = notifier->findChild<QQuickItem *>("shortNotificationBanner");
    check(richBanner && richBanner->isVisible()
          && richBanner->x()+richBanner->width()==notifierItem->width()-24,
          "simple notification is anchored at the upper right");
    check(!notifier->findChild<QQuickItem *>("notificationPrimaryAction"), "short prompt has no inline task actions");
    check(queue->property("calls").toInt()==0, "short notification does not duplicate native queue toast");
    QQmlComponent drawerComponent(&engine,QUrl("qrc:/xovi/manager/NotificationDrawer.qml"));
    QScopedPointer<QObject> drawer(drawerComponent.create());
    if (!drawer) std::fprintf(stderr,"%s\n",qPrintable(drawerComponent.errorString()));
    check(bool(drawer), "notification list drawer loads");
    auto *drawerItem=qobject_cast<QQuickItem *>(drawer.data());
    drawerItem->setParentItem(window.contentItem()); drawerItem->setSize(QSizeF(600,900));
    for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(2);}
    check(visualChild(drawerItem,"notificationListCard") && drawer->property("pageSize").toInt()>1,
          "drawer presents multiple notification cards with pagination");
    drawer->setProperty("selectedKey","test/import-task");
    auto *primaryAction=visualChild(drawerItem,"notificationAction_cancel");
    check(primaryAction && primaryAction->isVisible(), "task actions are available in notification details");
    auto toastTime = store->property("lastToast");
    context->notify({{"notificationId", "import-task"}, {"progress", QVariantMap{{"value", 0.6}}}});
    QMetaObject::invokeMethod(store, "refresh");
    check(store->property("lastToast") == toastTime, "progress update does not restart banner or emit another toast");
    auto *progressBar = drawer->findChild<QQuickItem *>("notificationProgressBar");
    check(progressBar && progressBar->property("value").toDouble()==0.6, "displayed progress follows the same notification");
    int delivered = 0;
    QObject::connect(context, &SettingsContext::notificationAction, [&](const QVariantMap &action) {
        if(action.value("notificationId")=="import-task" && action.value("actionId")=="cancel" && action.value("ownerId")=="test") ++delivered;
    });
    check(QMetaObject::invokeMethod(primaryAction, "clicked"), "notification action click handled");
    check(!primaryAction->isEnabled(), "pending action button disables until plugin takes the event");
    QElapsedTimer actionWait; actionWait.start();
    while(delivered==0 && actionWait.elapsed()<1600){QCoreApplication::processEvents();QThread::msleep(5);}
    check(delivered==1, "queued action delivered asynchronously to owning QML context");
    context->setNotificationActionsEnabled(false);
    check(context->takeNotificationActions().value("actions").toList().isEmpty(), "action is delivered once");
    QMetaObject::invokeMethod(store, "hideToast");
    drawerItem->setParentItem(nullptr);
    notifierItem->setParentItem(nullptr);
    manager->setProperty("selectedPage",QVariantMap{});
    manager->setProperty("mode","pins");
    QCoreApplication::processEvents();
    check(manager->property("launchers").toList().size()==2,"pin choices discovered without plugin declarations");
    QQmlComponent launcher(&engine,QUrl("qrc:/xovi/manager/LauncherEntries.qml"));
    QScopedPointer<QObject> shortcuts(launcher.createWithInitialProperties({{"location","bottom"},{"slots",1}}));
    if(!shortcuts) std::fprintf(stderr,"%s\n",qPrintable(launcher.errorString()));
    check(shortcuts && shortcuts->property("count").toInt()==1,"bottom bar loads user-selected entries");
    QVariant iconResult;
    QMetaObject::invokeMethod(shortcuts.data(),"iconFor",Q_RETURN_ARG(QVariant,iconResult),Q_ARG(QVariant,QVariant(QVariantMap{{"id","advanced_settings"},{"iconSource","qrc:/advanced_settings/icons/advanced_settings.svg"}})));
    check(iconResult=="qrc:/advanced_settings/icons/advanced_settings.svg","pin uses the provider's original icon without an id-based substitute");
    auto *shortcutItem=qobject_cast<QQuickItem *>(shortcuts.data());
    check(visualChild(shortcutItem,"nativeBottomPin"), "bottom pin uses homescreen Action component");
    QString pinnedOwner,pinnedPage,pinnedSource;
    const auto pinConnection=QObject::connect(sharedNavigation,&ManagerNavigation::openRequested,[&](const QString &owner,const QString &page,const QString &source) {
        pinnedOwner=owner;pinnedPage=page;pinnedSource=source;
    });
    QMetaObject::invokeMethod(visualChild(shortcutItem,"nativeBottomPin"),"actionClicked");
    QCoreApplication::processEvents();
    check(pinnedOwner=="example" && pinnedPage=="main" && pinnedSource=="bottom","native bottom PIN dispatches its provider and page");
    shortcuts->setProperty("entries",QVariantList{QVariantMap{{"id","one"},{"pageId","main"},{"title","One"}},QVariantMap{{"id","two"},{"pageId","main"},{"title","Two"}}});
    QCoreApplication::processEvents();
    check(shortcuts->property("pageCount").toInt()==2 && shortcutItem->implicitWidth()==184, "bottom paging occupies a second native action slot including spacing");
    shortcuts->setProperty("location", "sidebar");
    QCoreApplication::processEvents();
    check(visualChild(shortcutItem,"nativeSidebarPin"), "sidebar pin uses native SidebarItem");
    shortcutItem->setWidth(80);
    check(!shortcuts->property("showLabels").toBool(), "narrow sidebar pins stay icon-only");
    shortcutItem->setWidth(360);
    check(shortcuts->property("showLabels").toBool(), "wide sidebar pins may show labels");
    shortcuts->setProperty("location", "settings");
    QMetaObject::invokeMethod(shortcuts.data(),"refresh");
    check(shortcuts->property("count").toInt()==1 && visualChild(shortcutItem,"nativeSidebarPin"), "settings-sidebar pins use native SidebarItem and their own location");
    QMetaObject::invokeMethod(visualChild(shortcutItem,"nativeSidebarPin"),"triggered");
    QCoreApplication::processEvents();
    check(pinnedOwner=="example" && pinnedPage=="main" && pinnedSource=="settings","native settings PIN dispatches to the settings window route");
    QObject::disconnect(pinConnection);
    shortcuts->setProperty("excludedPages",QStringList{"example/main"});
    QMetaObject::invokeMethod(shortcuts.data(),"refresh");
    check(shortcuts->property("count").toInt()==0, "settings launcher excludes separately rendered Extensions entry");
    manager->setProperty("mode", "notifications");
    auto *managerDrawer=visualChild(managerItem,"notificationDrawer");
    check(managerDrawer!=nullptr,"manager uses the same notification list drawer");
    managerDrawer->setProperty("selectedKey","test/import-task");
    QMetaObject::invokeMethod(store, "refresh");
    for(const auto &size:{QSize(600,900),QSize(1600,900)}) {
        window.resize(size); managerItem->setSize(size);
        for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(2);}
        auto *cancel = visualChild(managerItem, "notificationAction_cancel");
        check(cancel && cancel->isVisible(), "notification center exposes declared actions");
        const auto corner=cancel->mapToScene(QPointF(cancel->width(), cancel->height()));
        check(corner.x()<=size.width() && corner.y()<=size.height(), "notification buttons fit portrait and landscape");
    }
    window.resize(600,900); managerItem->setSize(QSizeF(600,900));
    for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(2);}
    window.grabWindow().save("/tmp/manager-notifications-portrait.png");
    for(const auto &warning:warnings) std::fprintf(stderr,"%s\n",qPrintable(warning));
    check(warnings.isEmpty(), "notifications and shortcuts render without QML warnings");
    ManagerBridge lifecycle;
    for(const auto &command:{"enable", "disable", "repair", "get"}) {
        check(lifecycle.request(command, {{"id","example"}}).value("ok").toBool() && lastLifecycleCommand==command && lastLifecycleId=="example", "lifecycle broker receives plain package id, not JSON");
    }
    page["id"]="example";
    page["source"]="import QtQuick\nItem { required property var settingsContext }";
    manager->setProperty("mode", "detail");
    manager->setProperty("selectedPage", page); settle(navigationHost);
    auto *pinContext=navigationHost->findChild<SettingsContext *>();
    auto *pinButton=visualChild(managerItem,"pinCurrentPage");
    check(pinContext && pinButton && pinButton->isVisible(), "opened settings page has a labelled Pin action");
    QMetaObject::invokeMethod(pinButton,"clicked"); QCoreApplication::processEvents();
    check(manager->property("mode")=="pins", "Pin opens options for the current page");
    auto *sidebarButton=visualChild(managerItem,"toggleLocationPin");
    check(sidebarButton && sidebarButton->isVisible(), "sidebar pin control is actionable inside settings");
    QMetaObject::invokeMethod(sidebarButton,"clicked"); QCoreApplication::processEvents();
    check(!sidebarPin && bottomPin, "unpin sidebar preserves independent bottom-bar preference");
    sidebarButton=visualChild(managerItem,"toggleLocationPin");
    check(!sidebarButton->property("checked").toBool(), "pin selection reflects persisted preference");
    QMetaObject::invokeMethod(sidebarButton,"clicked"); QCoreApplication::processEvents();
    check(sidebarPin, "Pin to sidebar persists the chosen page");
    manager->setProperty("pinFilter","settings");
    auto *settingsPinButton=visualChild(managerItem,"toggleLocationPin");
    check(settingsPinButton && settingsPinButton->isVisible(), "settings-sidebar pin is independently actionable");
    QMetaObject::invokeMethod(settingsPinButton,"clicked");QCoreApplication::processEvents();
    check(!settingsPin && sidebarPin && bottomPin,"settings-sidebar change preserves both home pin choices");
    settingsPinButton=visualChild(managerItem,"toggleLocationPin");
    QMetaObject::invokeMethod(settingsPinButton,"clicked");QCoreApplication::processEvents();
    window.grabWindow().save("/tmp/manager-pin-options.png");
    QMetaObject::invokeMethod(manager.data(),"back");
    check(manager->property("mode")=="detail" && navigationHost->findChild<SettingsContext *>()==pinContext, "pin options return to same live settings context");
    page["source"]="import QtQuick\nItem { required property var settingsContext; MissingAttached.name: \"test\" }";
    manager->setProperty("selectedPage",page); settle(navigationHost);
    check(navigationHost->state()=="failed" && navigationHost->errorSummary().contains("not compatible with this device"), "unsupported attached object gets an actionable compatibility explanation");
    check(!manager->property("errorDetails").toBool(), "technical settings error is collapsed by default");
    auto *details=visualChild(managerItem,"settingsFailureDetails");
    check(details && details->isVisible(), "raw diagnostics remain available on request");
    window.grabWindow().save("/tmp/manager-settings-error.png");
    QVariant nativePage = QVariantMap{{"id","xochitl"},{"pageId","my-files"}};
    QMetaObject::invokeMethod(manager.data(), "openPins", Q_ARG(QVariant,nativePage));
    QCoreApplication::processEvents();
    auto *nativeSidebarButton=visualChild(managerItem,"toggleLocationPin");
    auto *nativeBottomButton=visualChild(managerItem,"toggleBottomPin");
    check(nativeSidebarButton && nativeSidebarButton->isVisible()
          && nativeSidebarButton->property("description")=="Sidebar",
          "native item has a visible sidebar control");
    check(!nativeBottomButton,
          "native sidebar item does not offer an unsupported bottom-bar move");
    manager->setProperty("mode","detail");
    page["chrome"]="page";
    page["source"]="import QtQuick\nItem { required property var settingsContext; objectName: \"selfContainedPage\"; property bool hostHeader: settingsContext.hostHeaderVisible; property string entry: settingsContext.launchSource; property string language: settingsContext.uiLanguage }";
    manager->setProperty("entryPoint","sidebar");
    manager->setProperty("selectedPage",page);settle(navigationHost);
    for(int i=0;i<4;++i) QCoreApplication::processEvents();
    auto *selfContained=visualChild(managerItem,"selfContainedPage");
    auto *hostHeader=visualChild(managerItem,"managerPageHeader");
    check(selfContained && !selfContained->property("hostHeader").toBool()
          && selfContained->property("entry")=="sidebar","page receives chrome ownership and launch source");
    check(hostHeader && !hostHeader->isVisible(),"self-contained page does not get a second manager header");
    window.grabWindow();
    check(navigationHost->width()==managerItem->width() && navigationHost->height()==managerItem->height(),"self-contained page receives the whole viewport without host footer or margins");
    check(!selfContained->property("language").toString().isEmpty(),"external page receives session language without reading a config file");
    { QFile config(languagePath);check(config.open(QIODevice::WriteOnly|QIODevice::Truncate),"update language fixture");config.write("[General]\nLanguage=zh_CN\n"); }
    XoviI18n::attach(&engine,"epub-preloader");
    page["baseUrl"]="qrc:/xovi/epub-preloader/Settings.qml";
    QFile epubPage(QString::fromLocal8Bit(argv[1])+"/epub-preloader/Settings.qml");
    check(epubPage.open(QIODevice::ReadOnly),"read actual EPUB settings page");
    page["source"]=QString::fromUtf8(epubPage.readAll());
    manager->setProperty("selectedPage",page);settle(navigationHost);
    check(navigationHost->state()=="ready","actual EPUB settings page loads before close");
    // A later global translator can shadow generic Manager context keys.
    class ForeignTranslator : public QTranslator {
        QString translate(const char *context,const char *source,const char *,int) const override {
            return QByteArray(context)=="Manager" ? QString::fromUtf8(source) : QString();
        }
        bool isEmpty() const override { return false; }
    } foreignTranslator;
    QCoreApplication::installTranslator(&foreignTranslator);
    check(QCoreApplication::translate("Manager","Extensions")=="Extensions","fixture reproduces global catalog shadowing");
    manager->setProperty("selectedPage",QVariantMap{});
    manager->setProperty("mode","list");
    engine.retranslate();
    window.grabWindow();
    auto *translatedHeader=visualChild(managerItem,"managerPageHeader");
    bool chineseHeader=false;
    for (auto *label:translatedHeader->findChildren<QObject *>())
        if(label->property("text").toString()==QString::fromUtf8("扩展")) chineseHeader=true;
    check(chineseHeader,"manager remains Chinese after inline EPUB settings is destroyed and global catalog changes");
    QCoreApplication::removeTranslator(&foreignTranslator);
    QMetaObject::invokeMethod(manager.data(),"openPins",Q_ARG(QVariant,QVariant(QVariantMap{})));
    manager->setProperty("pinFilter","sidebar");
    window.grabWindow();
    auto *shortcutList=visualChild(managerItem,"shortcutList");
    check(shortcutList && shortcutList->property("count").toInt()==2,"shortcut list contains plugin and native entries together");
    manager->setProperty("pinFilter","bottom");
    check(manager->property("pinEntries").value<QJSValue>().toVariant().toList().size()==1,"bottom category only contains bottom pins");
    manager->setProperty("pinFilter","settings");
    check(manager->property("pinEntries").value<QJSValue>().toVariant().toList().size()==1,"settings category contains only plugin settings-sidebar pins");
    manager->setProperty("pinFilter","quick");
    check(manager->property("pinEntries").value<QJSValue>().toVariant().toList().size()==1,"quick settings category includes unpinned plugin pages");
    check(visualChild(managerItem,"navigationBack"),"Back is located in native sidebar");
    check(lifecycle.entryTitle({{"id","keyboardcjk"},{"title","Keyboard CJK"},{"titleContext","XoviPluginNames"}})==QString::fromUtf8("中日韩输入法"),"plugin title resolves from its own catalog before opening its page");
    check(lifecycle.entryTitle({{"id","bluetooth-settings"},{"title","Bluetooth"},{"titleTranslations",QVariantMap{{"zh_CN",QString::fromUtf8("蓝牙")}}}})==QString::fromUtf8("蓝牙"),"pure QMD package localizes title without native translator or manager catalog entry");
    check(lifecycle.entryTitle({{"id","xochitl"},{"native",true},{"title","My files"},{"titleContext","NativeLaunchers"}})!=QStringLiteral("My files"),"native shortcut title resolves from manager catalog");
    QVariantList manyShortcuts;
    for (int i=0;i<20;++i) manyShortcuts.append(QVariantMap{{"id",QString("entry-%1").arg(i)},{"pageId","main"},{"title",QString("Entry %1").arg(i)},{"sidebar",true},{"bottom",false},{"available",true}});
    manager->setProperty("launchers",manyShortcuts);
    manager->setProperty("pinFilter","sidebar");
    window.grabWindow();
    check(manager->property("pinPageSize").toInt()>=2 && manager->property("pinPageCount").toInt()>1,"shortcuts paginate multiple rows rather than one entry per page");
    for(const auto &size:{QSize(600,900),QSize(1600,900),QSize(1200,1600),QSize(600,900)}) {
        window.resize(size);managerItem->setSize(size);window.grabWindow();
        for(const auto &warning:warnings) check(!warning.contains("recursive rearrange"),"shortcut resize has no recursive layout warning");
    }
    auto *shortcutPager=visualChild(managerItem,"shortcutPager");
    check(shortcutPager && shortcutPager->isVisible(),"shortcut pagination stays in its own footer");
    manager->setProperty("pinPage",1);
    manager->setProperty("pinFilter","quick");
    check(manager->property("pinPage").toInt()==0,"changing shortcut category resets page");
    QVariant lifecyclePackage=QVariantMap{{"id","example"},{"name","Example"},{"availableActions",QStringList{"disable"}}};
    QMetaObject::invokeMethod(manager.data(),"changeEnabled",Q_ARG(QVariant,lifecyclePackage));
    auto notices=lifecycle.request("notificationsList").value("entries").toList();
    bool successNotice=false;
    for(const auto &v:notices) { const auto n=v.toMap(); if(n.value("notificationId")=="lifecycle-example") successNotice=n.value("level")=="info" && n.value("pageId")=="inventory"; }
    check(successNotice,"disable result is posted to notification center");
    lifecyclePackage=QVariantMap{{"id","missing"},{"name","Missing"},{"availableActions",QStringList{"enable"}}};
    QMetaObject::invokeMethod(manager.data(),"changeEnabled",Q_ARG(QVariant,lifecyclePackage));
    notices=lifecycle.request("notificationsList").value("entries").toList();
    bool failureNotice=false;
    for(const auto &v:notices) { const auto n=v.toMap(); if(n.value("notificationId")=="lifecycle-missing") failureNotice=n.value("level")=="error"; }
    check(failureNotice,"failed enable is posted as error notification");
    window.grabWindow().save("/tmp/shortcuts-list.png");
    manager->setProperty("selectedPage",page);settle(navigationHost);
    manager->setProperty("initialOwner","example");
    QMetaObject::invokeMethod(navigationHost->findChild<SettingsContext *>(),"close");
    for(int i=0;i<4;++i) QCoreApplication::processEvents();
    check(!managerItem->isVisible(),"closing a shortcut-launched page returns to its caller without showing manager inventory");
    QObject::disconnect(connection);
    QObject::disconnect(apiConnection);
    managerItem->setParentItem(nullptr);
}
