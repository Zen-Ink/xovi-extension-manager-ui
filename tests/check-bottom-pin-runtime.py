"""Run the patched firmware CreateMenu and actual LauncherEntries without a build."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

project = Path(__file__).resolve().parents[1]
workspace = project.parent
version = sys.argv[1] if len(sys.argv) > 1 else '3.27.1.0'
qmd = 'manager_3_27_x.qmd' if version.startswith('3.27.') else 'manager_3_28_x.qmd'
with tempfile.TemporaryDirectory(prefix='bottom-pin-') as tmp:
    fixture = Path(tmp)
    applied = fixture / 'applied'
    subprocess.run([str(workspace/'advanced_settings/qmldiff'), 'apply-diffs', '--version', version,
                    str(workspace/f'xochitl_rcc/rcc_{version}'), str(applied), str(project/'qmd'/qmd)], check=True, stdout=subprocess.DEVNULL)
    shutil.copytree(project/'tests/mocks', fixture/'imports')
    def module(name, files):
        folder=fixture/'imports'/name.replace('.','/')
        folder.mkdir(parents=True,exist_ok=True)
        lines=['module '+name]
        for name_,text in files.items():
            singleton=text.startswith('pragma Singleton')
            (folder/(name_+'.qml')).write_text(text)
            lines.append(('singleton ' if singleton else '')+name_+' 1.0 '+name_+'.qml')
        (folder/'qmldir').write_text('\n'.join(lines)+'\n')
    for name in ['common','com.remarkable','device.global']:
        module(name,{'Stub':'import QtQml\nQtObject {}'})
    module('device.ui.controls',{'PopupOverlay':'import QtQuick\nItem {}'})
    module('xofm.libs.controls', {'PopupOverlay':'import QtQuick\nItem {}'})
    module('xofm.bindings.qt', {'Stub':'import QtQml\nQtObject {}'})
    module('xofm.libs.qtgui',{'WidgetLoader':'''import QtQuick
Item {
 property var injections
 property QtObject item: QtObject {
  objectName: ["search-action","new-document-action","calendar-actions"][index]
  property var supportedViews: ["myfiles"]
  property bool popupActive: false
 }
}'''})
    shutil.copy(fixture/'imports/xofm/libs/qtgui/WidgetLoader.qml', fixture/'imports/xofm/bindings/qt/WidgetLoader.qml')
    with (fixture/'imports/xofm/bindings/qt/qmldir').open('a') as f:
        f.write('WidgetLoader 1.0 WidgetLoader.qml\n')
    module('org.xovi.Manager',{
        'ManagerNavigation':'''pragma Singleton
import QtQml
QtObject {
 property int launcherRevision: 0
 property var entries: []
 property bool searchEnabled: true
 signal launchersChanged()
 function notifyLaunchersChanged() { launcherRevision++; launchersChanged() }
 function nativeEntryEnabled(name,location) { return name !== "search-action" || searchEnabled }
 function openSettings(owner,page,location) {}
}''',
        'ManagerBridge':'''import QtQml
QtObject {
 property string uiLanguage: "en"
 function request(command,args) { return {ok:true, entries:ManagerNavigation.entries} }
 function entryTitle(entry) { return entry.title || "" }
}'''})
    menu=(applied/'qt/qml/xofm/libs/homescreen/qml/CreateMenu.qml').read_text()
    menu=menu.replace('id: xoviBottomEntries','id: xoviBottomEntries\nobjectName: "bottomPinLoader"')
    menu=menu.replace('qrc:/xovi/manager/LauncherEntries.qml',(fixture/'LauncherEntries.qml').as_uri())
    (fixture/'CreateMenu.qml').write_text(menu)
    shutil.copy(project/'resources/qml/LauncherEntries.qml',fixture)
    (fixture/'tst_bottom.qml').write_text('''import QtQuick
import QtTest
import device.ui.controls
import org.xovi.Manager
Item {
 id: viewport; width: 1000; height: 600
 PopupOverlay { id: overlay }
 CreateMenu { id: menu; popupOverlay: overlay; createWidgets: [0,1,2]; view: "myfiles" }
 TestCase {
  name: "BottomPin"; when: windowShown
  function init() { failOnWarning(/.*/) }
  function entries(n) {
   var result=[];for(var i=0;i<n;i++)result.push({id:"plugin"+i,pageId:"main",title:"Plugin",available:true,bottom:true})
   return result
  }
  function pins() { return findChild(menu,"bottomPinLoader").item }
  function test_live_changes() {
   tryVerify(function(){return !!pins()})
   verify(isFinite(pins().availableExtent));verify(pins().availableExtent>0)
   ManagerNavigation.entries=entries(1);ManagerNavigation.notifyLaunchersChanged()
   tryCompare(pins(),"count",1);tryCompare(pins(),"displayedEntries",ManagerNavigation.entries)
   var originalWidth=menu.width
   viewport.visible=false
   ManagerNavigation.entries=entries(2);ManagerNavigation.notifyLaunchersChanged()
   viewport.visible=true
   tryCompare(pins(),"count",2)
   tryVerify(function(){return pins().displayedEntries.length===2 && menu.width>originalWidth})
   ManagerNavigation.entries=entries(0);ManagerNavigation.notifyLaunchersChanged()
   tryCompare(pins(),"count",0)
   tryCompare(findChild(menu,"bottomPinLoader"),"visible",false)
   ManagerNavigation.entries=entries(1);ManagerNavigation.notifyLaunchersChanged()
   tryCompare(findChild(menu,"bottomPinLoader"),"visible",true)
   tryVerify(function(){return pins().displayedEntries.length===1})
   var withSearch=menu.width
   ManagerNavigation.searchEnabled=false;ManagerNavigation.notifyLaunchersChanged()
   tryVerify(function(){return menu.width<withSearch})
  }
 }
}''')
    if version.startswith('3.28.'):
        test=fixture/'tst_bottom.qml'
        test.write_text(test.read_text().replace('import device.ui.controls','import xofm.libs.controls'))
    result=subprocess.run(['/usr/lib/qt6/bin/qmltestrunner','-import',str(fixture/'imports'),'-input',str(fixture),'-o','-,txt'],env={**os.environ,'QT_QPA_PLATFORM':'offscreen','QT_FORCE_STDERR_LOGGING':'1'})
    sys.exit(result.returncode)
