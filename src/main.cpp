#include "bridge.h"
#include "../sdk/xovi-i18n.h"
#include "../xovi.h"
#include "../sdk/qrr-api.h"
#include <QtQml>
#include <QFile>
extern "C" void _xovi_construct() {
    qAddPreRoutine(+[]() { XoviI18n::prepareCatalog("xovi-extension-manager-ui"); });
    qmlRegisterSingletonType<ManagerNavigation>("org.xovi.Manager",1,0,"ManagerNavigation",[](QQmlEngine *engine,QJSEngine *) -> QObject * {
        XoviI18n::attach(engine, "xovi-extension-manager-ui");
        auto *navigation=ManagerNavigation::shared();
        QQmlEngine::setObjectOwnership(navigation,QQmlEngine::CppOwnership);
        return navigation;
    });
    qmlRegisterType<ManagerBridge>("org.xovi.Manager",1,0,"ManagerBridge");
    qmlRegisterType<SettingsHost>("org.xovi.Manager",1,0,"SettingsHost");
    const QrrApiV1 *api=reinterpret_cast<const QrrApiV1 *(*)()>(qt_resource_rebuilder$qrr_get_api_v1)();
    if(api && api->abiVersion==1 && api->structSize>=sizeof(*api)) {
        QFile release("/etc/os-release");
        if(!release.open(QIODevice::ReadOnly)) return;
        QString version;
        for(const auto &line:release.readAll().split('\n'))
            if(line.startsWith("IMG_VERSION=")) version=QString::fromUtf8(line.mid(12)).remove('"').trimmed();
        const char *patch=nullptr;size_t length=0;
        if(version.startsWith("3.27.")) { patch=r$settingsEntry327;length=sizeof(r$settingsEntry327)-1; }
        else if(version.startsWith("3.28.")) { patch=r$settingsEntry328;length=sizeof(r$settingsEntry328)-1; }
        if(!patch) { qWarning("[extension-manager-ui] unsupported IMG_VERSION");return; }
        char *result=api->registerPatch("xovi-extension-manager-ui","settings-entry",patch,length);
        if(result) {qInfo("[extension-manager-ui] %s",result);api->freeString(result);}
    }
}

extern "C" bool override$_ZN16QCoreApplication17installTranslatorEP11QTranslator(QTranslator *translator) {
    auto original=reinterpret_cast<bool (*)(QTranslator *)>($_ZN16QCoreApplication17installTranslatorEP11QTranslator);
    const bool installed=original(translator);
    if (installed) ManagerNavigation::observeNativeTranslator(translator);
    return installed;
}
