import QtQuick
import QtQuick.Layouts
import ark.controls as ArkControls

ColumnLayout {
    id: root
    property var progress: null
    readonly property bool indeterminate: !!progress && !!progress.indeterminate
    readonly property real fraction: progress && typeof progress.value === "number" ? Math.max(0, Math.min(1, progress.value)) : 0
    spacing: 8
    ELabel {
        Layout.fillWidth: true
        text: root.indeterminate ? qsTr("Working…") : Math.round(root.fraction * 100) + "%"
    }
    ArkControls.ProgressBar {
        objectName: "notificationProgressBar"
        Layout.fillWidth: true
        implicitHeight: 28
        value: root.fraction
        // An unknown total uses a static pattern; no continuous e-paper animation.
        background: Rectangle { color: "white"; border.color: "black"; border.width: 2 }
        contentItem: Item {
            Rectangle {
                x: 4; y: 4; height: Math.max(0, parent.height - 8)
                width: Math.max(0, parent.width - 8) * root.fraction
                color: "black"; visible: !root.indeterminate
            }
            Row {
                anchors.fill: parent; anchors.margins: 4
                visible: root.indeterminate
                spacing: 8
                Repeater {
                    model: Math.max(1, Math.floor(parent.width / 24))
                    Rectangle { width: 16; height: parent.height; color: "black" }
                }
            }
        }
    }
}
