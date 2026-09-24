import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import ark.controls as ArkControls
import xofm.libs.homescreen as Homescreen
import org.xovi.Manager 1.0

Item {
    id: root
    property string location: "sidebar"
    // -1 means derive capacity from the native layout, not an arbitrary page size.
    property int slots: -1
    property var layoutHost: parent ? parent.parent : null
    property real availableExtent: location === "bottom" ? -1 : layoutHost ? layoutHost.height : -1
    readonly property real entryExtent: location === "bottom" ? 80 : metrics.item ? metrics.item.implicitHeight : 0
    readonly property int capacity: slots >= 0 ? slots : measuredCapacity()
    function measuredCapacity() {
        if (!layoutHost || !isFinite(availableExtent) || availableExtent < 0 || entryExtent <= 0) return count
        var horizontal = location === "bottom"
        var spacing = horizontal ? layoutHost.spacing || 0 : layoutHost.rowSpacing || layoutHost.spacing || 0
        var used = 0
        var children = layoutHost.children
        for (var i = 0; i < children.length; ++i) {
            var child = children[i]
            if (child === root.parent || !child.visible) continue
            var preferred = horizontal ? child.Layout.preferredWidth : child.Layout.preferredHeight
            var implicit = horizontal ? child.implicitWidth : child.implicitHeight
            var minimum = horizontal ? child.Layout.minimumWidth : child.Layout.minimumHeight
            var extent = Math.max(minimum || 0, preferred >= 0 ? preferred : implicit || 0)
            if (extent <= 0) continue // Repeater and flexible spacers consume no fixed extent.
            used += extent + spacing + (horizontal ? child.Layout.leftMargin + child.Layout.rightMargin : child.Layout.topMargin + child.Layout.bottomMargin)
        }
        return Math.max(0, Math.floor((availableExtent - used + spacing) / (entryExtent + spacing)))
    }
    property var popupOverlay: null
    property var anchorTarget: root
    property string currentView: "myfiles"
    property bool showLabels: location !== "bottom" && width >= 320
    property var excludedPages: []
    property var entries: []
    property int pageIndex: 0
    readonly property int count: entries.length
    readonly property bool overflow: count > capacity
    readonly property int perPage: Math.max(1, capacity - (overflow ? 1 : 0))
    readonly property int pageCount: Math.max(1, Math.ceil(count / perPage))
    readonly property var visibleEntries: entries.slice(pageIndex * perPage, (pageIndex + 1) * perPage)
    readonly property var displayedEntries: capacity <= 0 ? [] : !overflow ? entries : capacity === 1
        ? [{nextPage: true, title: qsTr("More pinned entries")}]
        : visibleEntries.concat([{nextPage: true, title: qsTr("More pinned entries")}])
    signal activated()
    implicitWidth: location === "bottom" ? Math.max(0, displayedEntries.length * 104 - 24) : 80
    implicitHeight: count === 0 ? 0 : grid.implicitHeight
    onPageCountChanged: pageIndex = Math.max(0, Math.min(pageIndex, pageCount - 1))
    ManagerBridge { id: bridge }
    function refresh() {
        var result = bridge.request("launcherList", {})
        var next = (result.entries || []).filter(function(e) { return e.available && e[root.location] && root.excludedPages.indexOf(e.id + "/" + e.pageId) < 0 })
        if (JSON.stringify(entries) !== JSON.stringify(next)) entries = next
    }
    function iconFor(entry) {
        if (entry.nextPage) return "qrc:/ark/icons/chevron_right"
        return entry.iconSource || "qrc:/ark/icons/puzzle"
    }
    function activate(entry) {
        if (entry.nextPage) {
            if (capacity === 1) {
                ManagerNavigation.openSettings("xovi-extension-manager", "inventory", root.location)
                activated()
            } else pageIndex = (pageIndex + 1) % pageCount
            return
        }
        ManagerNavigation.openSettings(entry.id, entry.pageId, root.location)
        activated()
    }
    Component {
        id: sidebarEntry
        ArkControls.SidebarItem {
            objectName: "nativeSidebarPin"
            property var entry: parent ? parent.entry : ({})
            text: root.showLabels ? (bridge.uiLanguage, bridge.entryTitle(entry)) : ""
            iconSource: root.iconFor(entry)
            onTriggered: root.activate(entry)
        }
    }
    Component {
        id: quickEntry
        ArkControls.StateSwitch {
            objectName: "nativeQuickPin"
            property var entry: parent ? parent.entry : ({})
            iconSource: root.iconFor(entry)
            onClicked: root.activate(entry)
        }
    }
    Component {
        id: bottomEntry
        Homescreen.Action {
            objectName: "nativeBottomPin"
            property var entry: parent ? parent.entry : ({})
            popupOverlay: root.popupOverlay
            anchorTarget: root.anchorTarget
            currentView: root.currentView
            supportedViews: [root.currentView]
            iconSource: root.iconFor(entry)
            onActionClicked: root.activate(entry)
        }
    }
    Loader {
        id: metrics
        visible: false
        property var entry: ({})
        width: root.width
        onLoaded: item.objectName = "pinMetrics"
        sourceComponent: root.location === "quick" ? quickEntry : root.location === "bottom" ? bottomEntry : sidebarEntry
    }
    GridLayout {
        id: grid
        anchors.fill: parent
        columns: root.location === "bottom" ? Math.max(1, root.displayedEntries.length) : 1
        rowSpacing: 0; columnSpacing: 24
        Repeater {
            model: root.displayedEntries
            delegate: Loader {
                required property var modelData
                readonly property var entry: modelData
                Layout.fillWidth: root.location !== "bottom"
                Layout.preferredWidth: root.location === "bottom" ? 80 : -1
                Layout.preferredHeight: root.location === "bottom" ? 80 : (item ? item.implicitHeight : 0)
                sourceComponent: root.location === "quick" ? quickEntry : root.location !== "bottom" ? sidebarEntry : bottomEntry
            }
        }
    }
    Connections { target: ManagerNavigation; function onLaunchersChanged() { root.refresh() } }
    Timer { interval: 2000; repeat: true; running: root.visible; onTriggered: root.refresh() }
    onVisibleChanged: if (visible) refresh()
    Component.onCompleted: refresh()
}
