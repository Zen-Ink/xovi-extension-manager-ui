pragma Singleton
import QtQuick
import org.xovi.Manager 1.0
QtObject {
    id: root
    property var entries: []
    property int unread: 0
    property var nativeQueue: null
    property double lastToast: 0
    property string richToastKey: ""
    property double richToastSequence: -1
    readonly property var richToast: entries.filter(function(entry) {
        return root.key(entry) === root.richToastKey && entry.sequence === root.richToastSequence
    })[0] || ({})
    property int revision: -1
    property var seen: ({})
    property string error: ""
    property bool subscribed: false
    property string actionError: ""
    property var bridge: ManagerBridge {}
    property var pendingToasts: []
    property var changes: Connections { target: root.bridge; function onNotificationsChanged() { root.refresh() } }
    Component.onCompleted: {
        subscribed = bridge.subscribeNotifications()
        refresh()
    }
    property var toastTimer: Timer { interval: 5000; onTriggered: root.hideToast() }
    function key(entry) { return entry.ownerId + "/" + entry.notificationId }
    function hideToast() {
        richToastKey = ""; richToastSequence = -1; toastTimer.stop()
        Qt.callLater(showNextToast)
    }
    function showNextToast() {
        if (richToastKey) return
        var queue = pendingToasts.slice()
        while (queue.length) {
            var candidate = queue.shift()
            var entry = entries.find(function(e) { return key(e) === candidate && !e.read })
            if (!entry) continue
            pendingToasts = queue
            richToastKey = candidate; richToastSequence = entry.sequence
            lastToast = Date.now(); toastTimer.restart()
            return
        }
        pendingToasts = []
    }
    function refresh() {
        var result = bridge.request("notificationsList", revision >= 0 ? {sinceRevision: revision} : {})
        if (!result.ok) { error = result.error || ""; return }
        error = subscribed ? "" : "notification-subscription-unavailable"
        if (result.revision === revision) return
        revision = result.revision
        entries = result.entries || []
        unread = result.unread || 0
        var nextSeen = ({}), fresh = []
        entries.forEach(function(entry) {
            var stamp = entry.toastRevision === undefined ? entry.sequence + ":" + entry.count : entry.toastRevision
            if (!entry.read && seen[key(entry)] !== stamp) fresh.push(entry)
            nextSeen[key(entry)] = stamp
        })
        seen = nextSeen
        var queue = pendingToasts.slice()
        // Store order is newest first; banners are FIFO. Updates to the same
        // notification replace its queued view instead of creating duplicates.
        fresh.reverse().forEach(function(entry) {
            var id = key(entry)
            if (id === richToastKey) {
                richToastSequence = entry.sequence
                lastToast = Date.now(); toastTimer.restart()
            } else if (queue.indexOf(id) < 0) queue.push(id)
        })
        pendingToasts = queue.slice(-100)
        if (richToastKey && !richToast.ownerId) hideToast()
        else showNextToast()
    }

    function invoke(entry, actionId) {
        var result = bridge.request("notificationsActionInvoke", {ownerId:entry.ownerId, notificationId:entry.notificationId, sequence:entry.sequence, actionId:actionId})
        refresh()
        actionError = result.ok ? "" : result.error || qsTr("Action unavailable")
        return result
    }
    function read(entry) { bridge.request("notificationsRead", {ownerId:entry.ownerId, notificationId:entry.notificationId}); refresh() }
    function dismiss(entry) { bridge.request("notificationsDismiss", {ownerId:entry.ownerId, notificationId:entry.notificationId}); refresh() }
    function clear() {
        var owners = ({})
        entries.forEach(function(entry) { owners[entry.ownerId] = true })
        Object.keys(owners).forEach(function(owner) { bridge.request("notificationsClear", {ownerId:owner}) })
        refresh()
    }
}
