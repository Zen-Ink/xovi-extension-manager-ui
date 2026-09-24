"""Exercise the real notification singleton with a signal-driven backend stub.
No C++ build; native subscription and Socket tests have separate host runners.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='notification-events-') as directory:
    root = Path(directory)
    manager = root / 'imports/org/xovi/Manager'
    manager.mkdir(parents=True)
    (manager / 'qmldir').write_text('module org.xovi.Manager\nManagerBridge 1.0 ManagerBridge.qml\n')
    (manager / 'ManagerBridge.qml').write_text('''import QtQml
QtObject {
    property bool subscribed: false
    property int requests: 0
    property int revision: 0
    property var entries: []
    signal notificationsChanged()
    function subscribeNotifications() { subscribed = true; return true }
    function request(command, args) {
        requests++
        return {ok:true, revision:revision, entries:entries,
                unread:entries.filter(function(e){return !e.read}).length}
    }
    function publish(list) { entries=list; revision++; notificationsChanged() }
}''')
    controls = root / 'imports/TestStore'
    controls.mkdir()
    shutil.copy(project/'resources/controls/NotificationStore.qml', controls)
    (controls / 'qmldir').write_text('module TestStore\nsingleton NotificationStore 1.0 NotificationStore.qml\n')
    (root / 'tst_events.qml').write_text('''import QtQuick
import QtTest
import TestStore
TestCase {
    name: "NotificationEvents"
    when: windowShown
    property var store: NotificationStore
    function entry(id, stamp, fraction) {
        return {ownerId:"test",notificationId:id,sequence:stamp,toastRevision:stamp,
                count:1,read:false,title:id,message:"message",progress:{value:fraction || 0}}
    }
    function init() {
        failOnWarning(/.*/)
        store.bridge.publish([])
        store.hideToast()
        wait(1)
    }
    function test_subscribe_and_no_polling() {
        verify(store.bridge.subscribed)
        var requests=store.bridge.requests
        wait(2100)
        compare(store.bridge.requests,requests)
        store.bridge.publish([entry("instant",1)])
        compare(store.richToastKey,"test/instant")
        compare(store.unread,1)
    }
    function test_burst_preserves_fifo() {
        store.bridge.publish([entry("first",2)])
        store.bridge.publish([entry("third",4),entry("second",3),entry("first",2)])
        compare(store.richToastKey,"test/first")
        compare(store.pendingToasts.length,2)
        store.hideToast()
        tryCompare(store,"richToastKey","test/second")
        store.hideToast()
        tryCompare(store,"richToastKey","test/third")
        store.hideToast()
        tryCompare(store,"richToastKey","")
    }
    function test_progress_and_same_key_coalesce() {
        store.bridge.publish([entry("job",5,0.1)])
        var started=store.lastToast
        wait(5)
        store.bridge.publish([entry("job",5,0.8)])
        compare(store.richToast.progress.value,0.8)
        compare(store.lastToast,started)
        compare(store.pendingToasts.length,0)
        store.bridge.publish([entry("job",6,1)])
        compare(store.richToastSequence,6)
        compare(store.pendingToasts.length,0)
        verify(store.lastToast>started)
    }
    function test_removed_or_read_queued_entries_are_skipped() {
        store.bridge.publish([entry("front",7)])
        store.bridge.publish([entry("removed",9),entry("read",8),entry("front",7)])
        var read=entry("read",8);read.read=true
        store.bridge.publish([read,entry("front",7)])
        store.hideToast()
        wait(1)
        compare(store.richToastKey,"")
        compare(store.pendingToasts.length,0)
    }
}''')
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_FORCE_STDERR_LOGGING='1')
    runner = shutil.which('qmltestrunner6') or '/usr/lib/qt6/bin/qmltestrunner'
    subprocess.run([runner, '-input', str(root), '-import', str(root/'imports')], env=env, check=True)
