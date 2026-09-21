import QtQuick
import QtQuick.Layouts
import ark.controls as ArkControls
import org.xovi.Manager 1.0
import org.xovi.Manager.Controls 1.0

Rectangle {
    id: root
    color: "white"
    property var systemNavigator: null
    property string initialOwner: ""
    property string entryPoint: initialOwner ? "shortcut" : "manager"
    readonly property bool pageOwnsChrome: !!selectedPage.id && selectedPage.chrome === "page" && mode !== "pins" && host.state === "ready"
    readonly property bool showNavigation: !pageOwnsChrome
    readonly property bool showNavigationLabels: width > 1200
    readonly property int navigationWidth: showNavigationLabels ? navigation.implicitWidth : navigationBack.implicitHeight
    property string initialPageId: ""
    property var packages: []
    property var pages: []
    property var selectedPage: ({})
    property var selectedPackage: ({})
    property var uiStates: ({})
    property string message: ""
    property string messageDetails: ""
    property var injections: []
    property var injectionPolicy: ({})
    property var launchers: []
    property string mode: "list"
    property string filter: "all"
    property int pageIndex: 0
    property var pinTarget: ({})
    property string pinPreviousMode: "list"
    property bool errorDetails: false
    property string pinFilter: "sidebar"
    property int pinPage: 0
    readonly property int pinPageSize: Math.max(1, Math.floor(shortcutArea.height / 112))
    readonly property var pinEntries: launchers.filter(function(entry) {
        if (pinTarget.id && (entry.id !== pinTarget.id || entry.pageId !== pinTarget.pageId)) return false
        return !entry.native || entry.location === pinFilter
    })
    readonly property int pinPageCount: Math.max(1, Math.ceil(pinEntries.length / pinPageSize))
    onPinFilterChanged: pinPage = 0
    onPinPageCountChanged: pinPage = Math.min(pinPage, pinPageCount - 1)
    property int detailPage: 0
    property int injectionPage: 0
    property int logPage: 0
    readonly property var ownedInjections: injections.filter(function(e) { return e.ownerId === selectedPackage.id && e.ownerId !== "legacy" && !e.resource })
    property int errorPage: 0
    property string logText: ""
    readonly property var filteredPackages: packages.filter(function(p) { return filter === "all" || p.type === filter })
    readonly property int pageSize: Math.max(1, Math.floor(listArea.height / 202))
    readonly property int pageCount: Math.max(1, Math.ceil(filteredPackages.length / pageSize))
    onFilterChanged: pageIndex = 0
    onPageCountChanged: pageIndex = Math.min(pageIndex, pageCount - 1)
    ManagerBridge { id: bridge }
    function translate(text) { bridge.uiLanguage; return bridge.translate("Manager", text) }
    function openSystemSettings(target) {
        var routes = {wifi: "wifi/window/network-select", language: "settings/window/language"}
        if (!routes[target] || !systemNavigator || typeof systemNavigator.open !== "function") return false
        try {
            if (systemNavigator.open(routes[target]) === false) throw new Error(root.translate(QT_TR_NOOP("Navigation rejected")))
            return true
        } catch (error) { message = String(error); return false }
    }
    function refresh() {
        var inventory = bridge.request("list", {})
        var nextPackages = inventory.packages || inventory.extensions || []
        if (selectedPackage.id) selectedPackage = nextPackages.filter(function(p) { return p.id === selectedPackage.id })[0] || selectedPackage
        var settings = bridge.request("settingsList", {})
        pages = settings.pages || []
        pages.forEach(function(page) {
            var owner = page.packageId || page.id
            if (page.provider === "runtime" && !nextPackages.some(function(pkg) { return pkg.id === owner }))
                nextPackages.push({id:owner, name:page.title, type:"qml", enabled:true, effectiveEnabled:true, availableActions:[]})
        })
        if (JSON.stringify(packages) !== JSON.stringify(nextPackages)) packages = nextPackages
        uiStates = settings.uiStates || ({})
        var status = bridge.request("injectionsGet", {})
        injections = status.results || []
        injectionPolicy = status.policy ? (status.policy.entries || {}) : ({})
        var pins = bridge.request("launcherList", {})
        launchers = (pins.entries || []).concat(pins.nativeEntries || [])
        if (!inventory.ok) message = root.operationError(inventory)
        else if (!pins.ok) message = root.operationError(pins)
    }
    function pageTitle(page) { bridge.uiLanguage; return bridge.entryTitle(page) }
    function packageTitle(pkg) {
        var owned = pagesFor(pkg.id)
        return owned.length ? pageTitle(owned[0]) : pageTitle({id:pkg.id, title:pkg.name || pkg.id})
    }
    function pagesFor(id) { return pages.filter(function(page) { return (page.packageId || page.id) === id }) }
    function diagnosticText(diagnostic) {
        if (!diagnostic) return ""
        bridge.uiLanguage
        return [diagnostic.summary ? bridge.translate("Diagnostics", diagnostic.summary) : "",
                diagnostic.recovery ? bridge.translate("Diagnostics", diagnostic.recovery) : ""].filter(function(s) { return s.length }).join("\n")
    }
    function operationError(result) {
        return diagnosticText(result.diagnostic) || result.message || result.error || root.translate(QT_TR_NOOP("Unable to save"))
    }
    function primaryDiagnostic(pkg) {
        var items = pkg.diagnostics || []
        return items.find(function(d) { return d.severity === "error" || d.severity === "critical" }) || null
    }
    function statusFor(pkg) {
        var failure = primaryDiagnostic(pkg)
        // Failure takes precedence over an unrelated pending change.
        if (failure) return bridge.translate("Diagnostics", failure.summary)
        var runtime = pkg.runtime || {}
        var state = runtime.loadStateName || "not-scanned"
        var failed = ["dlopen-failed", "link-failed", "dependency-failed"].indexOf(state) >= 0
        if (failed) return root.translate(QT_TR_NOOP("Load failed")) // Older backend compatibility.
        if (state === "shouldload-failed" || state === "condition-failed") return root.translate(QT_TR_NOOP("Load skipped"))
        if (pkg.activationNeedsRepair && pkg.activationIssue !== "absolute-link") return root.translate(QT_TR_NOOP("Activation needs repair"))
        if (pkg.restartToApply || pkg.requiresRestart) return root.translate(QT_TR_NOOP("Restart xochitl to apply"))
        if (pkg.type !== "extension") return pkg.effectiveEnabled ? root.translate(QT_TR_NOOP("Active")) : root.translate(QT_TR_NOOP("Inactive"))
        return state === "initialized" ? root.translate(QT_TR_NOOP("Initialized")) : root.translate(QT_TR_NOOP("Not loaded"))
    }
    function diagnosticsFor(pkg) {
        var lines = []
        if ((pkg.diagnostics || []).length) {
            pkg.diagnostics.forEach(function(d) {
                lines.push(d.severity + " · " + d.code + (d.causeCode ? " / " + d.causeCode : "") + "\n" + diagnosticText(d) + (d.detail ? "\n" + d.detail : ""))
            })
        } else {
            if (pkg.runtime && pkg.runtime.loadError) lines.push(pkg.runtime.loadError)
            lines = lines.concat(pkg.errors || [], pkg.issues || [])
            if ((pkg.warnings || []).length) lines.push(root.translate(QT_TR_NOOP("Warnings")) + ":\n" + pkg.warnings.join("\n"))
        }
        if (pkg.pendingChange) lines.push(root.translate(QT_TR_NOOP("A saved change is pending. Restart applies the change; it does not fix the reported error.")))
        pagesFor(pkg.id).forEach(function(page) {
            var state = uiStates[page.id + "/" + page.pageId]
            if (state && (state.state === "failed" || state.warning)) lines.push(page.title + ": " + (state.message || state.warning))
        })
        injections.forEach(function(entry) {
            if (!pkg.id || entry.ownerId === pkg.id) lines.push(entry.ownerId + "/" + entry.injectionId + "\n" + entry.state + " · " + entry.code + "\n" + diagnosticText(entry.diagnostic) + "\n" + (entry.message || ""))
        })
        return lines.filter(function(line, index) { return lines.indexOf(line) === index }).join("\n")
    }
    function toggleAction(pkg) {
        var actions = pkg.availableActions || []
        if (actions.indexOf("disable") >= 0) return "disable"
        if (actions.indexOf("enable") >= 0) return "enable"
        if (actions.indexOf("disableLegacy") >= 0) return "disableLegacy"
        return ""
    }
    function showPage(page) { mode = "list"; message = ""; messageDetails = ""; errorPage = 0; errorDetails = false; selectedPage = page }
    function openEntry(id, pageId) {
        if (id === "xovi-extension-manager" && pageId === "notifications") { selectedPage = ({}); mode = "notifications"; return }
        if (id === "xovi-extension-manager" && pageId === "inventory") { selectedPage = ({}); mode = "list"; return }
        var page = pages.filter(function(p) { return p.id === id && p.pageId === pageId && p.available })[0]
        if (page) showPage(page)
        else message = root.translate(QT_TR_NOOP("Page unavailable"))
    }
    function openPins(page) {
        if (page && page.transient) return
        message = ""; messageDetails = ""
        refresh()
        pinTarget = page || ({})
        var target = launchers.find(function(e) { return e.id === pinTarget.id && e.pageId === pinTarget.pageId })
        if (target && target.native) pinFilter = target.location
        pinPage = 0
        pinPreviousMode = mode === "pins" ? "list" : mode
        mode = "pins"
    }
    function repairActivation(pkg) {
        var result = bridge.request("repair", {id: pkg.id})
        message = result.ok ? (result.requiresRestart ? root.translate(QT_TR_NOOP("Activation repaired. Restart xochitl to apply.")) : root.translate(QT_TR_NOOP("Activation repaired"))) : root.operationError(result)
        messageDetails = result.ok ? "" : JSON.stringify(result, null, 2)
        refresh()
    }
    function changeEnabled(pkg) {
        var action = toggleAction(pkg)
        var result = bridge.request(action, {id:pkg.id})
        var text = result.ok ? (action === "enable" ? root.translate(QT_TR_NOOP("Enabled")) : root.translate(QT_TR_NOOP("Disabled"))) : root.translate(QT_TR_NOOP("Unable to save"))
        if (result.ok && result.requiresRestart) text += " · " + root.translate(QT_TR_NOOP("Restart xochitl to apply"))
        if (!result.ok) text = root.operationError(result)
        var posted = bridge.request("notificationsPost", {ownerId:"xovi-extension-manager", notificationId:"lifecycle-" + pkg.id,
            title:root.packageTitle(pkg), message:text, level:result.ok ? "info" : ((result.diagnostic || {}).severity === "warning" ? "warning" : "error"), pageId:"inventory"})
        if (!posted.ok) message = text
        else { message = ""; NotificationStore.refresh() }
        refresh()
        ManagerNavigation.notifyLaunchersChanged()
    }
    function setPin(entry, location) {
        var result = bridge.request("launcherSet", {id: entry.id, pageId: entry.pageId, location: location, enabled: !entry[location]})
        if (!result.ok) message = result.error === "last-manager-entry" ? root.translate(QT_TR_NOOP("Keep at least one entry to Extensions.")) : root.operationError(result)
        else { refresh(); ManagerNavigation.notifyLaunchersChanged() }
    }
    function closePage() {
        if (initialOwner) {
            if (parent && parent.active !== undefined) parent.active = false
            else root.visible = false
        } else selectedPage = ({})
    }
    Keys.onEscapePressed: root.back()
    function back() {
        message = ""
        if (mode === "pins") { mode = pinPreviousMode; pinTarget = ({}) }
        else if (selectedPage.id) closePage()
        else if (mode !== "list") mode = "list"
        else if (parent && parent.active !== undefined) parent.active = false
    }
    Connections {
        target: ManagerNavigation
        function onLaunchersChanged() {
            root.refresh()
            if (root.selectedPage.provider === "runtime" && !root.pages.some(function(page) {
                return page.id === root.selectedPage.id && page.pageId === root.selectedPage.pageId
            })) { root.selectedPage = ({}); root.closePage() }
        }
    }
    MouseArea { anchors.fill: parent }
    ColumnLayout {
        id: navigation
        visible: root.showNavigation
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: root.navigationWidth
        spacing: 0
        NavigationItem {
            id: navigationBack
            objectName: "navigationBack"
            title: root.translate(QT_TR_NOOP("Back")); showText: root.showNavigationLabels
            iconSource: "qrc:/ark/icons/chevron_left"
            bottomDivider: true
            Layout.fillWidth: true
            onTriggered: root.back()
        }
        Repeater {
            model: root.mode === "pins" ? [] : [
                {id:"list",title:root.translate(QT_TR_NOOP("Extensions")),icon:"plugins"},
                {id:"pins",title:root.translate(QT_TR_NOOP("Manage shortcuts")),icon:"pin"},
                {id:"notifications",title:root.translate(QT_TR_NOOP("Notifications")),icon:"bell"}]
            delegate: NavigationItem {
                required property var modelData
                Layout.fillWidth: true
                title: modelData.title; showText: root.showNavigationLabels
                iconSource: "qrc:/xovi/manager/icons/" + modelData.icon + ".svg"
                highlighted: !root.selectedPage.id && root.mode === modelData.id
                onTriggered: {
                    root.selectedPage = ({})
                    if (modelData.id === "pins") root.openPins({})
                    else { root.mode = modelData.id; root.refresh(); NotificationStore.refresh() }
                }
            }
        }
        Repeater {
            model: root.mode === "pins" ? [
                {id:"sidebar",title:root.translate(QT_TR_NOOP("Sidebar")),icon:"sidebar"},
                {id:"bottom",title:root.translate(QT_TR_NOOP("Bottom bar")),icon:"bottom-bar"},
                {id:"settings",title:root.translate(QT_TR_NOOP("Settings sidebar")),icon:"settings"},
                {id:"quick",title:root.translate(QT_TR_NOOP("Quick settings")),icon:"settings"}] : []
            delegate: NavigationItem {
                required property var modelData
                Layout.fillWidth: true
                title: modelData.title; showText: root.showNavigationLabels
                iconSource: "qrc:/xovi/manager/icons/" + modelData.icon + ".svg"
                highlighted: root.pinFilter === modelData.id
                onTriggered: root.pinFilter = modelData.id
            }
        }
        Item { Layout.fillHeight: true }
    }
    Rectangle { visible: root.showNavigation; x: root.navigationWidth; width: 1; height: parent.height; color: "black" }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: root.pageOwnsChrome ? 0 : 24
        anchors.leftMargin: root.pageOwnsChrome ? 0 : root.showNavigation ? root.navigationWidth + 24 : 24
        spacing: root.pageOwnsChrome ? 0 : 16
        RowLayout {
            objectName: "managerPageHeader"
            visible: !root.pageOwnsChrome
            Layout.fillWidth: true
            ELabel { text: (root.selectedPage.id ? root.pageTitle(root.selectedPage) : root.translate(QT_TR_NOOP("Extensions"))); Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
            EButton { objectName: "pinCurrentPage"; iconName: "pin"; text: root.translate(QT_TR_NOOP("Pin")); visible: !!root.selectedPage.id && !root.selectedPage.transient && root.mode !== "pins"; onClicked: root.openPins(root.selectedPage) }
        }
        RowLayout {
            visible: root.message.length > 0 && !root.pageOwnsChrome
            Layout.fillWidth: true
            ELabel { text: root.message; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
            EButton { iconName: "logs"; onClicked: { root.logText = root.messageDetails || root.message; root.logPage = 0; root.mode = "logs"; root.selectedPage = ({}) } }
            EButton { iconName: "close"; onClicked: root.message = "" }
        }
        RowLayout {
            visible: !root.selectedPage.id && root.mode === "list"
            Layout.fillWidth: true
            Repeater {
                model: [{value:"all", title:root.translate(QT_TR_NOOP("All"))}, {value:"extension", title:root.translate(QT_TR_NOOP("Native"))}, {value:"qmd", title:"QMD"}, {value:"qml", title:"QML"}]
                delegate: EButton {
                    required property var modelData
                    text: modelData.title; checked: root.filter === modelData.value
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    onClicked: root.filter = modelData.value
                }
            }
        }
        Item {
            id: listArea
            visible: !root.selectedPage.id && root.mode === "list"
            Layout.fillWidth: true; Layout.fillHeight: true
            Column {
                width: parent.width; spacing: 12
                Repeater {
                    model: root.filteredPackages.slice(root.pageIndex * root.pageSize, (root.pageIndex + 1) * root.pageSize)
                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width; height: 190; color: "white"; border.color: "black"; border.width: 2
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 8
                            ELabel { text: root.packageTitle(modelData); Layout.fillWidth: true; elide: Text.ElideRight; maximumLineCount: 1 }
                            RowLayout {
                                Layout.fillWidth: true
                                ELabel { text: root.statusFor(modelData); Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
                                EButton {
                                    iconName: modelData.activationNeedsRepair ? "refresh" : "power"
                                    text: modelData.activationNeedsRepair ? root.translate(QT_TR_NOOP("Repair")) : ""
                                    checked: !modelData.activationNeedsRepair && !!modelData.enabled
                                    enabled: modelData.activationNeedsRepair ? (modelData.availableActions || []).indexOf("repair") >= 0 : root.toggleAction(modelData).length > 0
                                    onClicked: {
                                        if (modelData.activationNeedsRepair) { root.repairActivation(modelData); return }
                                        root.changeEnabled(modelData)
                                    }
                                }
                                EButton {
                                    iconName: "settings"; enabled: root.pagesFor(modelData.id).some(function(p) { return p.available })
                                    onClicked: { var pages = root.pagesFor(modelData.id).filter(function(p) { return p.available }); if (pages.length === 1) root.showPage(pages[0]); else { root.selectedPackage = modelData; root.detailPage = 0; root.mode = "detail" } }
                                }
                                EButton { iconName: "info"; onClicked: { root.selectedPackage = modelData; root.detailPage = 0; root.mode = "detail" } }
                            }
                        }
                    }
                }
                ELabel { visible: root.filteredPackages.length === 0; text: root.translate(QT_TR_NOOP("No extensions")) }
            }
        }
        RowLayout {
            visible: !root.selectedPage.id && root.mode === "list"
            Layout.fillWidth: true
            EButton { iconName: "refresh"; description: root.translate(QT_TR_NOOP("Refresh")); onClicked: root.refresh() }
        }
        Pager { visible: !root.selectedPage.id && root.mode === "list"; Layout.fillWidth: true; pageCount: root.pageCount; pageIndex: root.pageIndex; onPageRequested: function(index) { root.pageIndex = index } }
        ColumnLayout {
            visible: !root.selectedPage.id && root.mode === "detail"
            Layout.fillWidth: true; Layout.fillHeight: true
            ELabel { text: (root.selectedPackage.name || root.selectedPackage.id || "") + " · " + (root.selectedPackage.version || ""); Layout.fillWidth: true }
            ELabel { text: root.statusFor(root.selectedPackage); Layout.fillWidth: true }
            Repeater {
                model: root.pagesFor(root.selectedPackage.id).slice(root.detailPage, root.detailPage + 1)
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    EButton { text: root.pageTitle(modelData); iconName: "settings"; enabled: modelData.available; Layout.fillWidth: true; Layout.minimumWidth: 0; onClicked: root.showPage(modelData) }
                    EButton { objectName: "pinProviderPage"; text: root.translate(QT_TR_NOOP("Pin")); iconName: "pin"; enabled: modelData.available; onClicked: root.openPins(modelData) }
                }
            }
            Pager { visible: root.pagesFor(root.selectedPackage.id).length > 1; Layout.fillWidth: true; pageCount: root.pagesFor(root.selectedPackage.id).length; pageIndex: root.detailPage; onPageRequested: function(index) { root.detailPage = index } }
            EButton { objectName: "repairActivation"; text: root.translate(QT_TR_NOOP("Repair activation")); iconName: "refresh"; visible: (root.selectedPackage.availableActions || []).indexOf("repair") >= 0; onClicked: root.repairActivation(root.selectedPackage) }
            EButton { text: root.translate(QT_TR_NOOP("Injections")); iconName: "plugins"; visible: root.ownedInjections.length > 0; onClicked: { root.injectionPage = 0; root.mode = "injections" } }
            EButton { iconName: "refresh"; text: root.translate(QT_TR_NOOP("Refresh")); onClicked: root.refresh() }
            EButton { text: root.translate(QT_TR_NOOP("Diagnostics")); iconName: "logs"; onClicked: { root.logText = root.diagnosticsFor(root.selectedPackage) || root.translate(QT_TR_NOOP("No diagnostics")); root.logPage = 0; root.mode = "logs" } }
            Item { Layout.fillHeight: true }
        }
        Item {
            id: pinPanel
            visible: root.mode === "pins"
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.minimumWidth: 0; Layout.minimumHeight: 0
            ColumnLayout {
            anchors.fill: parent
            spacing: 16
            ELabel { text: root.pinTarget.id ? root.translate(QT_TR_NOOP("Pin this page")) : root.translate(QT_TR_NOOP("Manage shortcuts")); Layout.fillWidth: true }
            Item {
                id: shortcutArea
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumHeight: 0; Layout.minimumWidth: 0
                clip: true
                Column {
                    id: shortcutList
                    objectName: "shortcutList"
                    readonly property int count: root.pinEntries.length
                    width: parent.width
                    spacing: 12
                    Repeater {
                        model: root.pinEntries.slice(root.pinPage * root.pinPageSize, (root.pinPage + 1) * root.pinPageSize)
                        delegate: Item {
                            required property var modelData
                            width: shortcutList.width; height: 100
                            RowLayout {
                                anchors.fill: parent; anchors.bottomMargin: 8
                                spacing: 12
                                ArkControls.Icon { source: modelData.iconSource || "qrc:/ark/icons/puzzle"; color: "black"; size: 48; Layout.preferredWidth: 48; Layout.preferredHeight: 48 }
                                ELabel { text: root.pageTitle(modelData); Layout.fillWidth: true; Layout.minimumWidth: 0; maximumLineCount: 1; elide: Text.ElideRight }
                                EButton {
                                    objectName: "toggleLocationPin"
                                    description: root.translate(root.pinFilter === "sidebar" ? QT_TR_NOOP("Sidebar") : root.pinFilter === "bottom" ? QT_TR_NOOP("Bottom bar") : root.pinFilter === "quick" ? QT_TR_NOOP("Quick settings") : QT_TR_NOOP("Settings sidebar"))
                                    text: root.translate(modelData[root.pinFilter] ? QT_TR_NOOP("Shown") : QT_TR_NOOP("Hidden"))
                                    iconName: root.pinFilter === "bottom" ? "bottom-bar" : root.pinFilter === "sidebar" ? "sidebar" : "settings"
                                    checked: !!modelData[root.pinFilter]
                                    Layout.preferredWidth: 220
                                    onClicked: root.setPin(modelData, root.pinFilter)
                                }
                            }
                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "black" }
                        }
                    }
                }
            }
            Pager { objectName: "shortcutPager"; Layout.fillWidth: true; pageIndex: root.pinPage; pageCount: root.pinPageCount; onPageRequested: function(index) { root.pinPage = index } }
            ELabel { visible: root.pinEntries.length === 0; text: root.translate(QT_TR_NOOP("No available page to pin")); Layout.fillWidth: true }
            }
        }
        ColumnLayout {
            visible: !root.selectedPage.id && root.mode === "logs"
            Layout.fillWidth: true; Layout.fillHeight: true
            readonly property int characters: Math.max(80, Math.floor(width / 24) * Math.max(2, Math.floor((height - 100) / 48)))
            ELabel { text: root.logText.slice(root.logPage * parent.characters, (root.logPage + 1) * parent.characters); Layout.fillWidth: true; Layout.fillHeight: true; verticalAlignment: Text.AlignTop; wrapMode: Text.WrapAnywhere }
            Pager { Layout.fillWidth: true; pageCount: Math.max(1, Math.ceil(root.logText.length / parent.characters)); pageIndex: root.logPage; onPageRequested: function(index) { root.logPage = index } }
        }
        ColumnLayout {
            visible: !root.selectedPage.id && root.mode === "injections"
            Layout.fillWidth: true; Layout.fillHeight: true
            Repeater {
                model: root.ownedInjections.slice(root.injectionPage, root.injectionPage + 1)
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    ELabel { text: modelData.injectionId; Layout.fillWidth: true }
                    ELabel { text: root.diagnosticText(modelData.diagnostic) || modelData.state; Layout.fillWidth: true }
                    EButton {
                        property bool desired: root.injectionPolicy[modelData.ownerId + "/" + modelData.injectionId] !== false
                        iconName: "power"; checked: desired; text: desired ? root.translate(QT_TR_NOOP("Disable")) : root.translate(QT_TR_NOOP("Enable"))
                        onClicked: {
                            var result = bridge.request("injectionsSet", {id:modelData.ownerId, injectionId:modelData.injectionId, enabled:!desired})
                            root.message = result.ok ? root.translate(QT_TR_NOOP("Restart xochitl to apply")) : root.operationError(result)
                            root.refresh()
                        }
                    }
                }
            }
            Item { Layout.fillHeight: true }
            Pager { Layout.fillWidth: true; pageIndex: root.injectionPage; pageCount: Math.max(1, root.ownedInjections.length); onPageRequested: function(index) { root.injectionPage = index } }
        }
        NotificationDrawer {
            onCloseRequested: root.mode = "list"
            visible: !root.selectedPage.id && root.mode === "notifications"
            Layout.fillWidth: true; Layout.fillHeight: true
            onOpenPage: function(owner, pageId) { root.refresh(); root.openEntry(owner, pageId) }
        }
        SettingsHost {
            id: host
            visible: !!root.selectedPage.id && root.mode !== "pins"
            Layout.fillWidth: true; Layout.fillHeight: true
            navigationHandler: root.systemNavigator && typeof root.systemNavigator.open === "function" ? root.openSystemSettings : undefined
            page: root.selectedPage
            launchSource: root.entryPoint
            onPinRequested: root.openPins(root.selectedPage)
            onCloseRequested: root.closePage()
            Rectangle {
                anchors.fill: parent; color: "white"
                visible: host.state === "failed" || host.state === "loading"
                ColumnLayout {
                    anchors.fill: parent; spacing: 16
                    ELabel { text: host.state === "failed" ? root.translate(QT_TR_NOOP("Unable to open page")) : root.translate(QT_TR_NOOP("Loading…")); Layout.fillWidth: true }
                    ELabel { objectName: "settingsFailureSummary"; visible: host.state === "failed" && !root.errorDetails; text: host.errorSummary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    ELabel { visible: host.state === "failed" && !root.errorDetails; text: host.recoveryHint; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    ELabel { visible: root.errorDetails; text: host.error.slice(root.errorPage * 180, (root.errorPage + 1) * 180); Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.WrapAnywhere }
                    Item { visible: !root.errorDetails; Layout.fillHeight: true }
                    Pager { visible: host.state === "failed" && root.errorDetails; Layout.fillWidth: true; pageCount: Math.max(1, Math.ceil(host.error.length / 180)); pageIndex: root.errorPage; onPageRequested: function(index) { root.errorPage = index } }
                    RowLayout {
                        visible: host.state === "failed"; Layout.fillWidth: true
                        EButton { text: root.translate(QT_TR_NOOP("Retry")); onClicked: { root.errorDetails = false; host.retry() } }
                        EButton { objectName: "settingsFailureDetails"; text: root.errorDetails ? root.translate(QT_TR_NOOP("Hide details")) : root.translate(QT_TR_NOOP("Details")); Layout.fillWidth: true; onClicked: { root.errorPage = 0; root.errorDetails = !root.errorDetails } }
                    }
                }
            }
        }
    }
    Component.onCompleted: { refresh(); if (initialOwner) openEntry(initialOwner, initialPageId) }
}
