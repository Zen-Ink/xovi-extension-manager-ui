# Integrate an existing plugin

## Choose an integration path

**The SDK is optional.** A native plugin can use the QML/QMD path too; being a
native plugin does not require using the native settings API. Neither path below
requires a manifest or a JSON form schema. Keep your existing settings page,
backend, storage and translation catalogs.

| | QML API, optionally injected by QMD | Native provider API |
|---|---|---|
| Best fit | Reuse an existing QML page or QMD | Discover settings independently of a QML object's lifetime |
| SDK dependency | None | Use the small C ABI header; shared controls/i18n helpers are optional |
| Page and PIN support | `registerPage()`; `openPage()` alone does not create a PIN entry | Export a settings provider; optional presentation/default PIN metadata |
| Discovery | After registration code runs; its owner must stay alive | From an initialized native plugin's XOVI exports |
| Main tradeoff | Any added firmware QMD injection point needs version maintenance | Requires native compilation and compliance with the ABI/lifetime rules |
| Background work | Needs an available QML bridge to call UI services | Optional notification/navigation APIs can be called from native code without creating a settings page |

Native registration avoids adding a per-plugin firmware injection just for the
entry. Manager UI itself still needs firmware compatibility. Both paths load the
page on demand: registration/discovery does **not** prove the QML can render.
The host reports page creation errors, but cannot prevent arbitrary synchronous
plugin code from blocking the GUI thread.

## 1. QML/QMD: no SDK

Use the following code in your existing QML, or insert it through your existing
QMD into a long-lived object such as MainView. There is no universal firmware QMD
patch: reuse the injection point your plugin already supports.

Load the optional helper from an existing action handler. Do not add a mandatory
`import org.xovi.Manager` to firmware QML if manager support is optional.

```qml
// Members of a long-lived Item { id: root; ... }
property var managerApi: null

function ensureManagerApi() {
    if (managerApi) return true
    var factory = Qt.createComponent("qrc:/xovi/manager/SettingsApi.qml")
    if (factory.status === Component.Ready)
        managerApi = factory.createObject(root)
    factory.destroy()
    return managerApi !== null
}

// Call from the plugin's existing settings action.
function openMySettings() {
    if (!ensureManagerApi()) {
        // Keep the plugin's existing settings-opening behavior here.
        return
    }
    var result = managerApi.openPage({
        url: "qrc:/my-plugin/Settings.qml",
        title: qsTr("My plugin"), chrome: "host"
    })
    if (!result.accepted) console.warn(result.error)
}
```

This simply displays the existing page: no registration, SDK, manifest, special
root properties or settings schema. It does not add the page to the plugin list
or Shortcuts. Supply a URL your plugin already registers; an existing QML
`Component` can instead be passed as `component: preferences`.

### Add a list/PIN entry

Call this once from your long-lived registration object's initialization **after
the manager helper is available**, rather than from the settings page itself:

```qml
function registerMySettings() {
    if (!ensureManagerApi()) return false
    var result = managerApi.registerPage(root, {
        id: "my-plugin", pageId: "main", title: "My plugin",
        titleTranslations: {
            en: "My plugin", zh_CN: "我的插件", zh_TW: "我的外掛"
        },
        iconSource: "qrc:/my-plugin/icon.svg",
        url: "qrc:/my-plugin/Settings.qml",
        chrome: "host",
        launcherDefaults: { settings: true }
    })
    if (!result.ok) console.warn(result.error)
    return result.ok
}

// Later, from an action handler:
// managerApi.openSettings("my-plugin", "main")
```

Use the native plugin runtime ID, or the QMD filename without `.qmd` and its
optional `NNN-` ordering prefix. This lets manager associate the page with the
plugin's enable/disable state. An unrelated ID cannot establish that association.

Keep `root` alive: its destruction unregisters the page. Register again each
session; user PIN choices persist and override defaults. If startup ordering
makes the helper unavailable, retry from an appropriate later lifecycle event.
Do not continuously poll or block startup waiting for manager.

### Keep your own page layout

`chrome: "host"` supplies the manager header/navigation. For a page that already
has its own header and back button, use `chrome: "page"` and
`usesSettingsContext: true` in either descriptor. In that page:

```qml
import QtQuick
Item {
    required property var settingsContext
    // Wire your existing back button to this function.
    function goBack() { settingsContext.close() }
}
```

The page owns its controls, translations and settings backend. Shared SDK
controls and translation helpers are optional. Keep a borrowed Component's owner
alive while displayed, or pass a registered URL instead.

## 2. Native provider: optional SDK headers

Add the SDK as a `sdk` submodule, add `sdk` to the compiler include path, and
export a static descriptor:

```cpp
#include "xovi-settings.h"
static const char url[] = "qrc:/my-plugin/Settings.qml";
extern "C" const XemSettingsProviderV1 *my_settings_provider_v1()
{
    static const XemSettingsProviderV1 page = {
        XEM_SETTINGS_ABI, sizeof(XemSettingsProviderV1),
        "main", "My plugin", 0, url, sizeof(url) - 1, nullptr
    };
    return &page;
}
```

In `.xovi`:

```text
export my_settings_provider_v1
with
    xovi-extension-manager$settingsProvider = 1
end
```

Use the plugin's normal build script to regenerate bindings and compile. This
exports metadata without importing manager; the native plugin can still load
without manager installed. The URL must already be registered by the plugin.
No per-plugin firmware injection is needed just to expose this entry.

Follow the [complete short native guide](https://github.com/Zen-Ink/xovi-extension-manager-sdk/blob/master/docs/native-integration.md)
for the submodule/build commands, optional icon/default PIN exports and ABI rules.
Do not register the same page through both paths unless deliberately handling
precedence; native provider metadata takes priority for matching identities.

## Check the adaptation

- Without manager-ui: the plugin's existing behavior still works.
- With manager-ui: open, close and reopen the page; check the plugin's language.
- For registered pages: PIN, restart, then disable the owning plugin and check its entry.
- Test an invalid page URL: the host should show a loading error. An accepted open request only means it was queued.

## Notification actions and external clients

Pages can call `settingsContext.notify()` without the SDK. Set
`notificationActionsEnabled` to subscribe, handle `notificationAction(action)`,
and call `completeNotificationAction(action.actionSequence, success, result)`
only after the asynchronous operation finishes. Use `notificationState()` after
`notificationStateChanged()` or when reopening to reconcile in-flight work.
There is no destructive action polling. See the
[notification examples](https://github.com/Zen-Ink/xovi-extension-manager-sdk/blob/master/docs/notification-events.md).

For services independent of page lifetime, native controllers can register
[managed Unix Sockets](https://github.com/Zen-Ink/xovi-extension-manager-sdk/blob/master/docs/managed-sockets.md)
through manager. External programs need only the service's JSON-lines protocol;
they do not link the SDK or require manager-ui.
