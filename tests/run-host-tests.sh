#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$(mktemp -d "${TMPDIR:-/tmp}/xem-host-tests.XXXXXX")
trap 'rm -rf "$build"' EXIT
(cd "$root/xovi-extension-manager-ui" && python3 "$root/xovi-extension-manager-ui/xovi/util/xovigen.py" -o xovi.cpp -H xovi.h xovi-extension-manager-ui.xovi)
XOVI_WORKSPACE="$root" bash "$root/xovi-extension-manager-ui/sdk/tests/i18n_resources.sh" "$build"
qt_libexec=$(pkg-config --variable=libexecdir Qt6Core)
"$qt_libexec/moc" "$root/xovi-extension-manager-ui/src/bridge.h" -o "$build/moc_bridge.cpp"
"$qt_libexec/rcc" "$root/xovi-extension-manager-ui/manager_resources.qrc" -o "$build/resources.cpp"
"$qt_libexec/rcc" -name xovi_controls "$root/xovi-extension-manager-ui/sdk/xovi_controls.qrc" -o "$build/controls.cpp"
broker_slot=$(sed -n 's/^#define xovi_message_broker\$broadcastToNative .*LINKTABLEVALUES\[\([0-9]*\)\].*/\1/p' "$root/xovi-extension-manager-ui/xovi.h")
notifications_slot=$(sed -n 's/^#define xovi_extension_manager\$xem_notifications_get_api .*LINKTABLEVALUES\[\([0-9]*\)\].*/\1/p' "$root/xovi-extension-manager-ui/xovi.h")
[ -n "$broker_slot" ] && [ -n "$notifications_slot" ]
${CXX:-c++} -std=c++17 -fPIC -pthread -DTEST_BROKER_SLOT="$broker_slot" -DTEST_NOTIFICATIONS_SLOT="$notifications_slot" $(pkg-config --cflags Qt6Quick Qt6Qml) \
    "$root/xovi-extension-manager/src/notifications.cpp" \
    "$root/xovi-extension-manager-ui/tests/host_tests.cpp" "$root/xovi-extension-manager-ui/src/bridge.cpp" \
    "$build/translations.cpp" "$build/resources.cpp" "$build/controls.cpp" "$build/moc_bridge.cpp" $(pkg-config --libs Qt6Quick Qt6Qml) -o "$build/host-tests"
QT_QPA_PLATFORM=offscreen "$build/host-tests" "$root"
