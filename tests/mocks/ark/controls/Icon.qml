import QtQuick
Item {
    property string source: ""
    property int size: 48
    property color color: "black"
    implicitWidth: size
    implicitHeight: size
    Image { anchors.fill: parent; source: parent.source.toString().indexOf("qrc:/ark/") === 0 ? "" : parent.source; fillMode: Image.PreserveAspectFit }
}
