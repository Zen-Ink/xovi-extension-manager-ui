#include "bridge.h"
#include "ManagerI18n.h"
#include "../xovi.h"
#include "../sdk/qrr-api.h"
#include <QtQml>
#include <QFile>
extern "C" void _xovi_construct() {
    initializeOfficialUpdateCapture();
    qAddPreRoutine(+[]() { ManagerI18n::service(); });
    qmlRegisterSingletonType<ManagerNavigation>("org.xovi.Manager",1,0,"ManagerNavigation",[](QQmlEngine *engine,QJSEngine *) -> QObject * {
        ManagerI18n::attach(engine);
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
        else if(version.startsWith("3.29.")) { patch=r$settingsEntry329;length=sizeof(r$settingsEntry329)-1; }
        if(!patch) { qWarning("[extension-manager-ui] unsupported IMG_VERSION");return; }
        char *result=api->registerPatch("xovi-extension-manager-ui","settings-entry",patch,length);
        if(result) {qInfo("[extension-manager-ui] %s",result);api->freeString(result);}
#if defined(__aarch64__)
        // The orientation-sensor module is absent on RM2 ARM firmware.
        // Keep its shortcut control in the sensor-equipped architecture's patch.
        if(version.startsWith("3.29.")) {
            result=api->registerPatch("xovi-extension-manager-ui","orientation-sensor",r$orientationSensor329,sizeof(r$orientationSensor329)-1);
            if(result) {qInfo("[extension-manager-ui] %s",result);api->freeString(result);}
        }
#endif
    }
}
