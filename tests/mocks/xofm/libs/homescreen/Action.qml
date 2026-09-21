import QtQuick
Item {
    required property var popupOverlay
    required property var anchorTarget
    required property list<string> supportedViews
    required property string currentView
    property string iconSource
    signal actionClicked()
}
