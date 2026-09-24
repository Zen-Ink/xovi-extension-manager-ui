#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd "$(dirname "$0")/.." && pwd)
workspace_dir=$(cd "$project_dir/.." && pwd)
export XEM_UI_PROJECT_DIR="$project_dir" XEM_UI_WORKSPACE_DIR="$workspace_dir"

python3 - <<'PY'
import os
import re
import pathlib
import shutil
import subprocess
import tempfile

project = pathlib.Path(os.environ["XEM_UI_PROJECT_DIR"])
workspace = pathlib.Path(os.environ["XEM_UI_WORKSPACE_DIR"])
qmldiff = pathlib.Path(os.environ.get("XEM_QMLDIFF", workspace / "advanced_settings" / "qmldiff"))
qmlformat = shutil.which(os.environ.get("XEM_QMLFORMAT", "/usr/lib/qt6/bin/qmlformat"))
if not qmlformat:
    raise SystemExit("qmlformat is required")

cases = (("3.27.1.0", "manager_3_27_x.qmd", "settings.showLabels"),
         ("3.28.0.164", "manager_3_28_x.qmd", "root.showLabels"))
affected = ("qml/device/view/settings/Settings.qml",
            "qml/device/view/main/MainView.qml",
            "qml/device/view/navigator/Sidebar.qml",
            "qt/qml/xofm/libs/homescreen/qml/CreateMenu.qml",
            "qt/qml/xofm/modules/settings/qml/QuickSettingsWindow.qml",
            "qt/qml/xofm/modules/settings/qml/quicksettings/ToggleColumn.qml",
            "qt/qml/xofm/modules/settings/qml/quicksettings/ToggleGrid.qml",
            "qt/qml/xofm/modules/settings/qml/quicksettings/AirplaneToggle.qml",
            "qt/qml/xofm/modules/screenshare/qml/ScreenShareToggle.qml",
            "qt/qml/xofm/modules/orientationsensor/qml/LockOrientationToggle.qml")

for version, qmd_name, label_guard in cases:
    source = workspace / "xochitl_rcc" / ("rcc_" + version)
    with tempfile.TemporaryDirectory(prefix="xem-firmware-qmd-") as tmp:
        output = pathlib.Path(tmp) / "applied"
        subprocess.run([str(qmldiff), "apply-diffs", "--version", version,
                        str(source), str(output), str(project / "qmd" / qmd_name)], check=True)
        for relative in affected:
            generated = output / relative
            assert generated.is_file(), f"{version}: missing patched {relative}"
            subprocess.run([qmlformat, str(generated)], check=True, stdout=subprocess.DEVNULL)
        settings = (output / affected[0]).read_text()
        assert 'ManagerNavigation.registerSettingsHost(xoviManagerLoader)' in settings
        assert 'xoviManagerLoader.openEntry(owner, page, source)' in settings
        assert 'xoviManagerLoader.openPage(page)' in settings
        main = (output / affected[1]).read_text()
        assert main.count('if (ManagerNavigation.settingsHostVisible()) return') == 2, 'MainView must not hide settings PIN pages behind Settings'
        assert "ArkControls.SidebarItem" in settings, f"{version}: sidebar item missing"
        assert label_guard in settings, f"{version}: label visibility guard missing"
        marker = settings.index('objectName: "xoviExtensionsEntry"')
        start = settings.rfind("ArkControls.SidebarItem", 0, marker)
        end = settings.index("}", marker)
        sidebar_item = settings[start:end]
        assert label_guard in sidebar_item, f"{version}: injected entry ignores showLabels"
        assert "contentItem:" not in sidebar_item, f"{version}: SidebarItem contentItem overridden"
        assert "background:" not in sidebar_item, f"{version}: SidebarItem background overridden"
        launchers = (project / "resources/qml/LauncherEntries.qml").read_text()
        assert "ArkControls.SidebarItem {" in launchers and "Homescreen.Action {" in launchers
        assert "EButton {" not in launchers, "pins must use the native navigation components"
        for icon in re.findall(r'qrc:/ark/icons/([a-z_]+)', launchers + sidebar_item):
            assert (source / "ark/icons" / icon).is_file(), f"{version}: missing native icon {icon}"
        assert '"popupOverlay": Qt.binding' in (output / affected[3]).read_text()
        sidebar = (output / affected[2]).read_text()
        for entry in ("my-files", "filters", "favorites", "tags", "integrations", "trash", "help", "settings"):
            assert 'nativeEntryEnabled("' + entry + '", "sidebar")' in sidebar
        bottom = (output / affected[3]).read_text()
        padding = 'sidePadding' if version.startswith('3.27.') else 'horizontalPadding'
        assert f'root.{padding} * 2' in bottom, f'{version}: wrong native bottom padding property'
        assert re.search(r'property\s+int\s+' + padding + r'\s*:', bottom), f'{version}: referenced padding is not declared'

        assert 'nativeEntryEnabled(item.objectName, "bottom")' in bottom
        assert 'supportedViews.includes(root.view)' in bottom
        quick = (output / affected[4]).read_text()
        toggles = (output / affected[5]).read_text()
        assert 'objectName: "xoviQuickNotifications"' in toggles and 'NotificationDrawer.qml' in quick
        assert 'anchors.right: xoviQuickSettingsPanel.left' in quick
        assert 'parent.width < 1200' not in quick
        assert 'ManagerNavigation.toggleQuickNotifications()' in toggles
        assert 'anchors.left: parent.left' in quick and 'anchors.bottom: parent.bottom' in quick
        main = (output / affected[1]).read_text()
        # Manager creation must not capture a closing notification's JS context.
        assert 'xoviPinnedManager.setSource(' not in main
        assert 'ManagerNavigation.registerLauncher(xoviPinnedManager)' in main
        print("PASS", version)

qml_roots = [project / "resources", workspace / "sdk/qml", workspace / "epub-preloader/Settings.qml",
             workspace / "advanced_settings/resources/qml", workspace / "keyboardcjk/resources/qml",
             workspace / "rm-librarian/Settings.qml", workspace / "xovi-rmfakecloud-plugin/plugin/qml"]
for location in qml_roots:
    files = [location] if location.is_file() else location.rglob("*.qml")
    for qml in files:
        assert "Accessible." not in qml.read_text(), f"unsupported accessibility attached property: {qml}"
for location in [workspace / "advanced_settings/resources/qml", workspace / "keyboardcjk/resources/qml"]:
    for qml in location.rglob("*.qml"):
        if "sleepscreen" in qml.parts:
            continue  # Reading content retains its user-selected font.
        source = qml.read_text()
        assert not re.search(r"font\.(pixelSize|pointSize)\s*:\s*[0-9]", source), f"local font size: {qml}"
        assert not re.search(r"\bArkControls\.(Body|Label|Title|Panel|Button)\s*\{", source), f"unshared settings typography: {qml}"
import xml.etree.ElementTree as ET
qrc = workspace / "advanced_settings/resources/resources.qrc"
resources = {"qrc:" + section.attrib["prefix"] + "/" + node.attrib.get("alias", node.text): qrc.parent / node.text
             for section in ET.parse(qrc).findall("qresource") for node in section.findall("file")}
provider = (workspace / "advanced_settings/main.cpp").read_text()
icon = re.search(r'"(qrc:[^"]+advanced_settings.svg)"', provider).group(1)
assert icon in resources and resources[icon].is_file(), "Advanced Settings pin icon is not registered"
print("PASS Advanced Settings presentation icon resolves to registered resource")
print("PASS native pin components, icon resources and shared UI typography")
print("PASS shared controls and provider accessibility scan")
PY

python3 - "$workspace_dir" <<'PYICONS'
from pathlib import Path
import sys
root=Path(sys.argv[1])
for folder in ('sdk/qml/org/xovi/Controls/icons','xovi-extension-manager-ui/resources/icons'):
    for icon in (root/folder).glob('*.svg'):
        assert '<rect width="48" height="48" fill="white"' not in icon.read_text(), icon
print('PASS native-tinted symbols have no opaque square canvas')
PYICONS
