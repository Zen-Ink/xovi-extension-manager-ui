import QtQuick
import QtQuick.Layouts
import org.xovi.Manager 1.0
import org.xovi.Manager.Controls 1.0

Item {
    id: root
    property var nativeQueue: null // Retained for existing firmware adapters.
    readonly property var store: NotificationStore
    Component.onCompleted: NotificationStore.refresh()
    Connections {
        target: ManagerNavigation
        function onNotificationsRequested() { drawer.active = true; NotificationStore.hideToast() }
    }
    Loader {
        id: drawer
        objectName: "notificationDrawerLoader"
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: root.width < 1200 ? root.width : Math.min(900, root.width * 0.55)
        active: false
        source: "qrc:/xovi/manager/NotificationDrawer.qml"
        onLoaded: {
            item.closeRequested.connect(function() { drawer.active = false })
            item.openPage.connect(function(owner, pageId) {
                ManagerNavigation.openSettings(owner, pageId, "notification")
                drawer.active = false
            })
        }
    }
    Rectangle {
        id: banner
        objectName: "shortNotificationBanner"
        readonly property var entry: NotificationStore.richToast
        visible: !!entry.ownerId && !drawer.active
        anchors.top: parent.top; anchors.right: parent.right
        anchors.margins: 24
        width: Math.max(0, Math.min(620, root.width - 48))
        height: body.implicitHeight + 32
        color: "white"; border.color: "black"; border.width: 3
        MouseArea {
            anchors.fill: parent
            onClicked: { NotificationStore.hideToast(); drawer.active = true }
        }
        ColumnLayout {
            id: body
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 16; spacing: 8
            RowLayout {
                Layout.fillWidth: true
                ELabel { Layout.fillWidth: true; text: banner.entry.title || ""; maximumLineCount: 1; elide: Text.ElideRight; emphasized: true }
                EButton { iconName: "close"; description: qsTr("Hide notification"); onClicked: NotificationStore.hideToast() }
            }
            ELabel { Layout.fillWidth: true; text: banner.entry.message || ""; maximumLineCount: 2; elide: Text.ElideRight }
        }
    }
}
