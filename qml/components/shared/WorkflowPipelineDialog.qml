import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Dialog {
    id: root

    property var nodes: []
    property bool workflowReady: false
    property bool allowIncompleteRun: false
    property string statusText: qsTr("Not prepared")
    property bool busy: false
    property real progress: 0
    property string dialogTitle: qsTr("Workflow")
    property string description: qsTr("Execution stages and data flow")
    property string modelName: ""
    property string runtimeName: ""
    property var configurableNodeIds: []
    property var nodeConfigurations: ({})
    property string actionText: qsTr("Refresh")
    property string actionIconName: "refresh"
    property bool reviewWaiting: false
    property real canvasScale: 1.0
    signal prepareRequested()
    signal runRequested()
    signal approveRequested()
    signal rejectRequested()
    signal nodeConfigureRequested(string nodeId)
    signal nodeConfigurationChanged(string nodeId, string familyId, string runtimeId, string runtimeVersion, var selectedFiles)

    function configureNode(nodeId) { Qt.callLater(function() { modelDialog.openFor(nodeId) }) }

    readonly property int nodeWidth: 188
    readonly property int nodeHeight: 208
    readonly property int nodeGap: 76
    readonly property int canvasPadding: 64

    function nodeConfigurable(nodeId) {
        var item = nodes ? nodes.find(function(entry) { return entry.id === nodeId }) : null
        return (item && item.configurable === true) || (configurableNodeIds && configurableNodeIds.indexOf(nodeId) >= 0)
    }

    function fitCanvas() {
        var count = nodes ? nodes.length : 0
        if (count === 0 || canvasViewport.width <= 0)
            return
        var naturalWidth = canvasPadding * 2 + count * nodeWidth + Math.max(0, count - 1) * nodeGap
        canvasScale = Math.max(0.58, Math.min(1.0, (canvasViewport.width - 32) / naturalWidth))
        canvasFlick.contentX = Math.max(0, (canvasFlick.contentWidth - canvasFlick.width) / 2)
        canvasFlick.contentY = Math.max(0, (canvasFlick.contentHeight - canvasFlick.height) / 2)
    }

    onOpened: Qt.callLater(fitCanvas)

    parent: Overlay.overlay
    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    width: Math.min(1240, Math.max(760, parent ? parent.width - Theme.paddingXL * 2 : 1100))
    height: Math.min(760, Math.max(540, parent ? parent.height - Theme.paddingXL * 2 : 680))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                LineIcon { anchors.centerIn: parent; name: "alignment"; color: Theme.accentLight; width: 20; height: 20 }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: root.dialogTitle; color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                Text { Layout.fillWidth: true; text: root.description; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight }
            }

            Rectangle {
                visible: root.modelName !== "" || root.runtimeName !== ""
                Layout.preferredWidth: summaryText.implicitWidth + 20
                Layout.preferredHeight: 30
                radius: Theme.radiusSmall
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: Qt.rgba(1, 1, 1, 0.08)
                Text {
                    id: summaryText
                    anchors.centerIn: parent
                    text: root.modelName !== "" ? root.modelName : root.runtimeName
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Button {
                id: closeButton
                implicitWidth: 32; implicitHeight: 32
                onClicked: root.close()
                contentItem: LineIcon { name: "close"; color: closeButton.hovered ? Theme.textPrimary : Theme.textSecondary; width: 16; height: 16 }
                background: Rectangle { radius: Theme.radiusSmall; color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent" }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        Item {
            id: canvasViewport
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 390
            clip: true

            Rectangle {
                anchors.fill: parent
                color: Qt.darker(Theme.background, 1.06)
            }

            Canvas {
                anchors.fill: parent
                opacity: 0.55
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.28)
                    var step = 20
                    for (var px = 10; px < width; px += step) {
                        for (var py = 10; py < height; py += step) {
                            ctx.beginPath()
                            ctx.arc(px, py, 0.8, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }
                }
            }

            Flickable {
                id: canvasFlick
                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, graphSurface.width * root.canvasScale)
                contentHeight: Math.max(height, graphSurface.height * root.canvasScale)

                Item {
                    id: graphSurface
                    width: root.canvasPadding * 2 + (root.nodes ? root.nodes.length : 0) * root.nodeWidth
                           + Math.max(0, (root.nodes ? root.nodes.length : 0) - 1) * root.nodeGap
                    height: Math.max(canvasFlick.height / root.canvasScale, root.nodeHeight + root.canvasPadding * 2)
                    scale: root.canvasScale
                    transformOrigin: Item.TopLeft

                    Row {
                        anchors.centerIn: parent
                        spacing: root.nodeGap

                        Repeater {
                            model: root.nodes || []
                            delegate: Item {
                                id: stageDelegate
                                required property var modelData
                                required property int index
                                readonly property bool nodeReady: modelData.state === "ready" || modelData.state === "completed"
                                readonly property bool nodeBlocked: modelData.state === "blocked"
                                readonly property bool nodeWaiting: modelData.state === "waiting_for_input"
                                readonly property bool nodeMissing: modelData.state === "missing"
                                readonly property bool nodeFailed: modelData.state === "failed"
                                readonly property color stateColor: nodeReady ? Theme.success : (nodeWaiting || nodeBlocked ? Theme.warning : (nodeMissing || nodeFailed ? Theme.danger : Theme.textSecondary))
                                width: root.nodeWidth
                                height: root.nodeHeight

                                Canvas {
                                    visible: stageDelegate.index > 0
                                    width: root.nodeGap + 12
                                    height: 80
                                    x: -root.nodeGap - 6
                                    y: Math.round((parent.height - height) / 2)
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, width, height)
                                        ctx.strokeStyle = Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.68)
                                        ctx.lineWidth = 2
                                        var y = height / 2
                                        ctx.beginPath()
                                        ctx.moveTo(0, y)
                                        ctx.bezierCurveTo(width * 0.35, y, width * 0.62, y, width, y)
                                        ctx.stroke()
                                    }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: Theme.radiusMedium
                                    color: cardHover.hovered ? Qt.lighter(Theme.surface, 1.08) : Theme.surface
                                    border.color: cardHover.hovered ? Qt.rgba(stageDelegate.stateColor.r, stageDelegate.stateColor.g, stageDelegate.stateColor.b, 0.78)
                                                                         : Qt.rgba(stageDelegate.stateColor.r, stageDelegate.stateColor.g, stageDelegate.stateColor.b, 0.42)
                                    border.width: 1

                                    HoverHandler { id: cardHover; cursorShape: root.nodeConfigurable(stageDelegate.modelData.id || "") ? Qt.PointingHandCursor : Qt.ArrowCursor }
                                    TapHandler {
                                        enabled: root.nodeConfigurable(stageDelegate.modelData.id || "")
                                        onTapped: root.nodeConfigureRequested(stageDelegate.modelData.id || "")
                                    }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: Theme.paddingMedium
                                        spacing: Theme.paddingSmall

                                        RowLayout {
                                            Layout.fillWidth: true
                                            Rectangle {
                                                Layout.preferredWidth: 34; Layout.preferredHeight: 34
                                                radius: Theme.radiusSmall
                                                color: Qt.rgba(stageDelegate.stateColor.r, stageDelegate.stateColor.g, stageDelegate.stateColor.b, 0.14)
                                                LineIcon { anchors.centerIn: parent; name: stageDelegate.nodeReady ? "check" : ((stageDelegate.nodeWaiting || stageDelegate.nodeBlocked) ? "activity" : "close"); color: stageDelegate.stateColor; width: 16; height: 16 }
                                            }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr("NODE %1").arg(stageDelegate.index + 1); color: Theme.textSecondary; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.8 }
                                        }

                                        Text { Layout.fillWidth: true; text: stageDelegate.modelData.title || ""; color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; wrapMode: Text.WordWrap; maximumLineCount: 2 }
                                        Text { Layout.fillWidth: true; text: stageDelegate.modelData.detail || qsTr("Built-in workflow stage"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight; wrapMode: Text.WordWrap; maximumLineCount: 2 }
                                        Item { Layout.fillHeight: true }

                                        Rectangle {
                                            visible: (stageDelegate.modelData.providerName || "") !== ""
                                            Layout.fillWidth: true; Layout.preferredHeight: 32
                                            radius: Theme.radiusSmall
                                            color: Qt.rgba(1, 1, 1, 0.04)
                                            border.color: Qt.rgba(1, 1, 1, 0.08)
                                            RowLayout {
                                                anchors.fill: parent; anchors.leftMargin: Theme.paddingSmall; anchors.rightMargin: Theme.paddingSmall; spacing: 6
                                                LineIcon { name: "cpu"; color: Theme.accentLight; width: 14; height: 14 }
                                                Text { Layout.fillWidth: true; text: stageDelegate.modelData.providerName || ""; color: Theme.textPrimary; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight }
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 6
                                            Rectangle { Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 4; color: stageDelegate.stateColor }
                                            Text { text: stageDelegate.nodeReady ? qsTr("Ready") : (stageDelegate.nodeWaiting ? qsTr("Review") : (stageDelegate.nodeBlocked ? qsTr("Blocked") : (stageDelegate.nodeMissing ? qsTr("Missing") : qsTr("Failed")))); color: stageDelegate.stateColor; font.pixelSize: 10; font.bold: true }
                                            Item { Layout.fillWidth: true }
                                            Button {
                                                visible: root.nodeConfigurable(stageDelegate.modelData.id || "")
                                                text: qsTr("Configure")
                                                implicitWidth: 76
                                                implicitHeight: 26
                                                padding: 0
                                                onClicked: root.nodeConfigureRequested(stageDelegate.modelData.id || "")
                                                contentItem: Text { anchors.fill: parent; text: parent.text + "  ›"; color: parent.hovered ? Theme.textPrimary : Theme.accentLight; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                                background: Rectangle { radius: Theme.radiusSmall; color: parent.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : "transparent" }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    width: 12; height: 12; radius: 6
                                    anchors.left: parent.left; anchors.leftMargin: -6; anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.background; border.color: Theme.accentLight; border.width: 2
                                }
                                Rectangle {
                                    width: 12; height: 12; radius: 6
                                    anchors.right: parent.right; anchors.rightMargin: -6; anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.accent; border.color: Theme.accentLight; border.width: 2
                                }
                            }
                        }
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.paddingMedium
                spacing: 4

                component CanvasButton: Button {
                    implicitWidth: 32; implicitHeight: 30
                    background: Rectangle { color: parent.hovered ? Theme.surfaceAlt : Theme.surface; radius: Theme.radiusSmall; border.color: Qt.rgba(1, 1, 1, 0.10) }
                    contentItem: Text { anchors.fill: parent; text: parent.text; color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
                CanvasButton { text: "−"; enabled: root.canvasScale > 0.58; onClicked: root.canvasScale = Math.max(0.58, root.canvasScale - 0.1) }
                Rectangle { width: 48; height: 30; radius: Theme.radiusSmall; color: Theme.surface; border.color: Qt.rgba(1, 1, 1, 0.10); Text { anchors.centerIn: parent; text: Math.round(root.canvasScale * 100) + "%"; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true } }
                CanvasButton { text: "+"; enabled: root.canvasScale < 1.35; onClicked: root.canvasScale = Math.min(1.35, root.canvasScale + 0.1) }
                CanvasButton { text: qsTr("Fit"); implicitWidth: 44; onClicked: root.fitCanvas() }
            }

            Text {
                visible: !root.nodes || root.nodes.length === 0
                anchors.centerIn: parent
                text: qsTr("Prepare the workflow to display its nodes")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontMedium
            }
        }

        ProgressBar { Layout.fillWidth: true; visible: root.busy; from: 0; to: 1; value: root.progress }
        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        RowLayout {
            Layout.fillWidth: true; Layout.margins: Theme.paddingLarge; spacing: Theme.paddingSmall
            Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: root.workflowReady ? Theme.success : Theme.warning }
            Text { Layout.fillWidth: true; text: root.statusText; color: root.workflowReady ? Theme.success : Theme.warning; font.pixelSize: Theme.fontSmall; font.bold: true; elide: Text.ElideRight }
            Text { text: qsTr("Topology locked"); color: Theme.textSecondary; font.pixelSize: 10 }
            PrimaryButton { visible: root.reviewWaiting; text: qsTr("Approve review"); iconName: "check"; enabled: !root.busy; onClicked: root.approveRequested() }
            PrimaryButton { visible: root.reviewWaiting; text: qsTr("Reject"); iconName: "close"; quiet: true; enabled: !root.busy; onClicked: root.rejectRequested() }
            PrimaryButton { visible: !root.reviewWaiting; text: root.busy ? qsTr("Running…") : qsTr("Run workflow"); iconName: root.busy ? "activity" : "play"; enabled: !root.busy && (root.workflowReady || root.allowIncompleteRun); onClicked: root.runRequested() }
            PrimaryButton { visible: !root.busy && !root.reviewWaiting; text: root.actionText; iconName: root.actionIconName; quiet: true; onClicked: root.prepareRequested() }
        }
    }

    WorkflowNodeModelDialog {
        id: modelDialog
        nodes: root.nodes
        nodeConfigurations: root.nodeConfigurations
        onConfigurationAccepted: function(nodeId, familyId, runtimeId, runtimeVersion, selectedFiles) {
            root.nodeConfigurationChanged(nodeId, familyId, runtimeId, runtimeVersion, selectedFiles)
        }
    }

    onNodeConfigureRequested: function(nodeId) { root.configureNode(nodeId) }
}
