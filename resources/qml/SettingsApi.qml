import QtQml
import org.xovi.Manager 1.0

// Optional entry point: Qt.createComponent() this URL, check Ready, then createObject().
// Keeping this import out of a firmware QML file makes an absent manager harmless.
QtObject {
    function openPage(page) { return ManagerNavigation.openPage(page) }
    function registerPage(owner, page) { return ManagerNavigation.registerPage(owner, page) }
    function unregisterPage(owner, id, pageId) { return ManagerNavigation.unregisterPage(owner, id, pageId || "main") }
    function openSettings(id, pageId) { return ManagerNavigation.requestOpenSettings(id, pageId || "main") }
}
