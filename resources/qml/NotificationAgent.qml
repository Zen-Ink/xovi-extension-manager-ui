import QtQuick
import QtQuick.Layouts
import com.remarkable
import org.xovi.Manager 1.0
import org.xovi.Manager.Controls 1.0

Item {
    id: root
    property var nativeQueue: null // Retained for existing firmware adapters.
    readonly property var store: NotificationStore
    // Native OCR is not an SDK operation. Observe the two real RecognitionJobs;
    // never replace their native status/error handling or initiate conversion.
    property var pageJob: Recognizer.recognitionJob
    property var selectionJob: Recognizer.selectionToolJob
    property int pageStatus: pageJob ? pageJob.status : -1
    property int selectionStatus: selectionJob ? selectionJob.status : -1
    property var reportedPageJob: null
    property var reportedSelectionJob: null
    property var ocrBridge: ManagerBridge {}
    onPageStatusChanged: ocrFailureTimer.restart()
    onSelectionStatusChanged: ocrFailureTimer.restart()
    onPageJobChanged: ocrFailureTimer.restart()
    onSelectionJobChanged: ocrFailureTimer.restart()
    function localText(zh, en) { return Qt.locale().name.indexOf("zh") === 0 ? zh : en }
    function reportOcr(job, page) {
        if (!job) return
        if (job.status === RecognitionJob.Status.Processing) {
            if (page) reportedPageJob = null
            else reportedSelectionJob = null
        }
        if (job.status !== RecognitionJob.Status.Error) return
        if (job === reportedPageJob || job === reportedSelectionJob) return
        var code = "NATIVE_OCR_UNKNOWN"
        var message = localText("OCR 转换未完成。请检查网络、云账号及订阅；当前原生错误不提供具体原因，请勿据此认定额度不足。", "OCR did not finish. Check connectivity, cloud account and subscription. The native error does not identify the cause or establish quota exhaustion.")
        if (job.error === RecognitionJob.ConvertError.NetworkError) {
            code = "NATIVE_OCR_NETWORK"
            message = localText("OCR 网络连接失败。请检查 Wi-Fi，确认原任务结果后手动重试。", "OCR connection failed. Check Wi-Fi and the original task result before retrying manually.")
        } else if (job.error === RecognitionJob.ConvertError.AuthenticationError) {
            code = "NATIVE_OCR_AUTHENTICATION"
            message = localText("OCR 身份验证失败。请在 NFJ 云设置检查登录和配对；这不代表一定没有订阅。", "OCR authentication failed. Check sign-in and pairing in NFJ Cloud settings; this does not prove a missing subscription.")
        } else if (job.error === RecognitionJob.ConvertError.ServerError) {
            code = "NATIVE_OCR_SERVER"
            message = localText("OCR 服务无法处理本次转换。请检查选区大小或稍后手动重试；原生错误未提供 HTTP 原因。", "OCR could not process this conversion. Check selection size or retry manually later. Native error contains no HTTP cause.")
        } else if (job.error === RecognitionJob.ConvertError.NoTasks || job.error === RecognitionJob.ConvertError.NoText) {
            code = "NATIVE_OCR_NO_TEXT"
            message = localText("未识别到可转换文字。请检查手写选区；不会自动重试。", "No convertible text recognized. Check the handwriting selection. No automatic retry.")
        }
        var result = ocrBridge.request("notificationsPost", {
            ownerId: "xovi-nfj-cloud", notificationId: "native-ocr-failure", pageId: "main",
            title: localText("OCR 转换未完成", "OCR did not finish"), level: "warning",
            message: message + "\n" + code + " · native=" + Number(job.error)
        })
        if (result.ok) {
            if (page) reportedPageJob = job
            else reportedSelectionJob = job
        }
    }
    Timer {
        id: ocrFailureTimer
        interval: 150
        onTriggered: { root.reportOcr(root.pageJob, true); root.reportOcr(root.selectionJob, false) }
    }
    Component.onCompleted: NotificationStore.refresh()
    Connections {
        target: ManagerNavigation
        function onNotificationsRequested() { drawer.active = true; NotificationStore.hideToast() }
    }
    Loader {
        id: drawer
        objectName: "notificationDrawerLoader"
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: root.width < 1200 ? root.width : Math.min(900, root.width * 0.55)
        active: false
        source: "qrc:/xovi/manager/NotificationDrawer.qml"
        onLoaded: {
            item.closeRequested.connect(function() { drawer.active = false })
            item.openPage.connect(function(owner, pageId) {
                ManagerNavigation.openSettings(owner, pageId, "notification")
                drawer.active = false
            })
        }
    }
    Rectangle {
        id: banner
        objectName: "shortNotificationBanner"
        readonly property var entry: NotificationStore.richToast
        visible: !!entry.ownerId && !drawer.active
        anchors.top: parent.top; anchors.right: parent.right
        anchors.margins: 24
        width: Math.max(0, Math.min(620, root.width - 48))
        height: body.implicitHeight + 32
        color: "white"; border.color: "black"; border.width: 3
        MouseArea {
            anchors.fill: parent
            onClicked: { NotificationStore.hideToast(); drawer.active = true }
        }
        ColumnLayout {
            id: body
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 16; spacing: 8
            RowLayout {
                Layout.fillWidth: true
                ELabel { Layout.fillWidth: true; text: banner.entry.title || ""; maximumLineCount: 1; elide: Text.ElideRight; emphasized: true }
                EButton { iconName: "close"; description: qsTr("Hide notification"); onClicked: NotificationStore.hideToast() }
            }
            ELabel { Layout.fillWidth: true; text: banner.entry.message || ""; maximumLineCount: 2; elide: Text.ElideRight }
        }
    }
}
