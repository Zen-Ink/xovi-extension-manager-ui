import QtQuick
import QtQuick.Layouts
import ark.controls as ArkControls
import xofm.libs.homescreen as Homescreen
import org.xovi.Manager 1.0

Item {
    id: root
    property string location: "sidebar"
    property int slots: 2
    property var popupOverlay: null
    property var anchorTarget: root
    property string currentView: "myfiles"
    property bool showLabels: location !== "bottom" && width >= 320
    property var excludedPages: []
    property var entries: []
    property int pageIndex: 0
    readonly property int count: entries.length
    readonly property int perPage: location !== "bottom" ? 1 : Math.max(1, slots)
    readonly property int pageCount: Math.max(1, Math.ceil(count / perPage))
    readonly property var visibleEntries: entries.slice(pageIndex * perPage, (pageIndex + 1) * perPage)
    readonly property var displayedEntries: pageCount > 1 ? visibleEntries.concat([{nextPage: true, title: qsTr("More pinned entries")}]) : visibleEntries
    signal activated()
    implicitWidth: location === "bottom" ? Math.max(0, displayedEntries.length * 104 - 24) : 80
    implicitHeight: count === 0 ? 0 : grid.implicitHeight
    onPageCountChanged: pageIndex = Math.min(pageIndex, pageCount - 1)
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
        if (entry.nextPage) { pageIndex = (pageIndex + 1) % pageCount; return }
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
