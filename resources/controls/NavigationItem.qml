import QtQuick

SettingsSidebarItem {
    property bool showText: true
    property string title: ""
    text: showText ? title : ""
}
