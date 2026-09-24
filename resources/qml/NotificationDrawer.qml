import QtQuick
import QtQuick.Layouts
import org.xovi.Manager.Controls 1.0

Rectangle {
    id: root
    objectName: "notificationDrawer"
    color: "white"
    border.color: "black"
    border.width: 3
    property int pageIndex: 0
    property string selectedKey: ""
    readonly property int pageSize: Math.max(1, Math.floor((height - 340) / 260))
    readonly property var entries: NotificationStore.entries
    readonly property int pageCount: Math.max(1, Math.ceil(entries.length / pageSize))
    readonly property int selectedIndex: entries.findIndex(function(entry) { return NotificationStore.key(entry) === root.selectedKey })
    signal closeRequested()
    signal openPage(string owner, string pageId)
    onPageCountChanged: pageIndex = Math.min(pageIndex, pageCount - 1)
    onSelectedIndexChanged: if (selectedIndex < 0) selectedKey = ""
    MouseArea { anchors.fill: parent }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            EButton { visible: root.selectedIndex >= 0; iconName: "back"; description: qsTr("Back"); onClicked: { if (notificationDetails.details) notificationDetails.details = false; else root.selectedKey = "" } }
            ELabel { Layout.minimumWidth: 0; text: qsTr("Notifications") + " · " + NotificationStore.unread; Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
            EButton { objectName: "closeNotificationDrawer"; iconName: "close"; description: qsTr("Close"); onClicked: root.closeRequested() }
        }
        ColumnLayout {
            visible: root.selectedIndex < 0
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: 12
            ELabel { visible: root.entries.length === 0; text: qsTr("No notifications"); Layout.fillWidth: true }
            Repeater {
                model: root.entries.slice(root.pageIndex * root.pageSize, (root.pageIndex + 1) * root.pageSize)
                delegate: Rectangle {
                    required property var modelData
                    objectName: "notificationListCard"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 248
                    color: "white"; border.color: "black"; border.width: modelData.read ? 1 : 3
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 16; spacing: 8
                        ELabel { text: modelData.title; emphasized: !modelData.read; Layout.fillWidth: true; maximumLineCount: 1; elide: Text.ElideRight }
                        ELabel { text: modelData.message || ""; Layout.fillWidth: true; Layout.fillHeight: true; maximumLineCount: 2; elide: Text.ElideRight }
                        NotificationProgress { visible: !!modelData.progress; progress: modelData.progress || null; Layout.fillWidth: true }
                    }
                    MouseArea {
                        objectName: "openNotificationDetails"
                        anchors.fill: parent
                        onClicked: { root.selectedKey = NotificationStore.key(modelData); NotificationStore.read(modelData) }
                    }
                }
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                EButton { iconName: "check"; description: qsTr("Clear notifications"); enabled: root.entries.length > 0; onClicked: NotificationStore.clear() }
                Item { Layout.fillWidth: true }
            }
            Pager { Layout.fillWidth: true; pageIndex: root.pageIndex; pageCount: root.pageCount; onPageRequested: function(index) { root.pageIndex = index } }
        }
        NotificationCenter {
            id: notificationDetails
            onVisibleChanged: if (visible) details = false
            navigationEnabled: false
            showHeader: false
            visible: root.selectedIndex >= 0
            Layout.fillWidth: true; Layout.fillHeight: true
            pageIndex: Math.max(0, root.selectedIndex)
            onOpenPage: function(owner, pageId) { root.openPage(owner, pageId) }
        }
    }
    Component.onCompleted: NotificationStore.refresh()
}
