#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
"$(pkg-config --variable=libexecdir Qt6Core)/rcc" -name xovi_controls "$root/xovi-extension-manager-ui/sdk/xovi_controls.qrc" -o "$build/controls.cpp"
c++ -std=c++17 -fPIC $(pkg-config --cflags Qt6Quick Qt6Qml) "$root/xovi-extension-manager-ui/tests/typography_tests.cpp" "$build/controls.cpp" $(pkg-config --libs Qt6Quick Qt6Qml) -o "$build/test"
for firmware in 3.27.1.0 3.28.0.164; do
    mkdir -p "$build/$firmware"
    cp -r "$root/xochitl_rcc/rcc_$firmware/ark-imports/ark" "$build/$firmware/ark"
    # C++ settings, text sanitation and icon rendering are substituted. All
    # text, panel, button, sidebar QML and typography tokens are real firmware.
    cat > "$build/$firmware/ark/tokens/Settings.qml" <<'QML'
pragma Singleton
import QtQuick
QtObject {
    property bool accessibility: false
    property bool usingDebugColors: false
    property int textRulesPolicy: 0
    function convertColor(color) { return color }
}
QML
    echo 'singleton Settings 1.0 Settings.qml' >> "$build/$firmware/ark/tokens/qmldir"
    cat > "$build/$firmware/ark/controls/Icon.qml" <<'QML'
import QtQuick
Item {
    property string source
    property int size: 48
    property color color
    property bool multicolor: false
    property var style
    implicitWidth: size
    implicitHeight: size
}
QML
    cat > "$build/$firmware/ark/controls/TextRules.qml" <<'QML'
pragma Singleton
import QtQuick
QtObject { function apply(text, format, policy) { return text } }
QML
    echo 'singleton TextRules 1.0 TextRules.qml' >> "$build/$firmware/ark/controls/qmldir"
    QT_QPA_PLATFORM=offscreen "$build/test" "$build/$firmware" "$root"
done
