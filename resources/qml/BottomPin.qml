import QtQuick
import xofm.libs.homescreen as Homescreen

Homescreen.Action {
    objectName: "nativeBottomPin"
    required property var launchers
    required property var entry
    iconSource: launchers.iconFor(entry)
    onActionClicked: launchers.activate(entry)
}
