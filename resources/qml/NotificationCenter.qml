import QtQuick
import QtQuick.Layouts
import org.xovi.Manager.Controls 1.0
Rectangle {
    id: root
    color: "white"
    property bool navigationEnabled: true
    property bool showHeader: true
    property int pageIndex: 0
    property int messagePage: 0
    property bool details: false
    readonly property var entries: NotificationStore.entries
    readonly property var current: entries[Math.min(pageIndex, Math.max(0, entries.length - 1))] || ({})
    readonly property string currentKey: (current.ownerId || "") + "/" + (current.notificationId || "")
    readonly property int textPageSize: Math.max(1, Math.floor(messageArea.width / 36)) * Math.max(1, Math.floor(messageArea.height / 48))
    signal openPage(string owner, string pageId)
    onCurrentKeyChanged: { messagePage = 0; details = false }
    onEntriesChanged: if (navigationEnabled) pageIndex = Math.min(pageIndex, Math.max(0, entries.length - 1))
    onTextPageSizeChanged: messagePage = 0
    ColumnLayout {
        anchors.fill: parent; spacing: 8
        RowLayout {
            visible: root.showHeader
            Layout.fillWidth: true
            ELabel { text: qsTr("Notifications") + " · " + NotificationStore.unread; Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
            EButton { iconName: root.details ? "back" : "close"; enabled: root.entries.length > 0; description: root.details ? qsTr("Back") : qsTr("Clear notifications"); onClicked: { if (root.details) root.details = false; else NotificationStore.clear() } }
        }
        ELabel { text: root.current.title || qsTr("No notifications"); Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
        ELabel { visible: !!root.current.ownerId && !root.details; text: (root.current.ownerId || "") + " · " + (root.current.state || root.current.level || ""); Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
        NotificationProgress { visible: !root.details && !!root.current.progress; progress: root.current.progress || null; Layout.fillWidth: true }
        RowLayout {
            visible: !root.details && (root.current.actions || []).length > 0
            Layout.fillWidth: true
            Repeater {
                model: root.current.actions || []
                delegate: EButton {
                    required property var modelData
                    objectName: "notificationAction_" + modelData.id
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    text: modelData.label
                    enabled: (root.current.pendingActionIds || []).indexOf(modelData.id) < 0
                    onClicked: NotificationStore.invoke(root.current, modelData.id)
                }
            }
        }
        ELabel { visible: NotificationStore.actionError.length > 0; Layout.fillWidth: true; text: NotificationStore.actionError; maximumLineCount: 1; elide: Text.ElideRight }
        Item {
            id: messageArea
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 48
            ELabel {
                anchors.fill: parent; verticalAlignment: Text.AlignTop
                wrapMode: Text.WrapAnywhere; elide: Text.ElideRight
                text: (root.current.message || "").slice(root.details ? root.messagePage * root.textPageSize : 0, root.details ? (root.messagePage + 1) * root.textPageSize : root.textPageSize)
            }
        }
        Pager { visible: root.details; Layout.fillWidth: true; pageCount: Math.max(1, Math.ceil((root.current.message || "").length / root.textPageSize)); pageIndex: root.messagePage; onPageRequested: function(index) { root.messagePage = index } }
        RowLayout {
            visible: !!root.current.ownerId && !root.details; Layout.fillWidth: true
            EButton { iconName: "check"; enabled: !root.current.read; description: qsTr("Mark read"); onClicked: NotificationStore.read(root.current) }
            EButton { iconName: "settings"; visible: !!root.current.pageId; description: qsTr("Open plugin settings"); onClicked: { NotificationStore.read(root.current); root.openPage(root.current.ownerId, root.current.pageId) } }
            EButton { iconName: "info"; description: qsTr("Details"); onClicked: root.details = true }
            Item { Layout.fillWidth: true }
            EButton { iconName: "close"; description: qsTr("Dismiss notification"); onClicked: NotificationStore.dismiss(root.current) }
        }
        Pager { visible: !root.details && root.navigationEnabled; Layout.fillWidth: true; pageIndex: root.pageIndex; pageCount: Math.max(1, root.entries.length); onPageRequested: function(index) { root.pageIndex = index } }
    }
    Component.onCompleted: NotificationStore.refresh()
}
