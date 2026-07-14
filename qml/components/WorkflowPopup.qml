import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Popup {
    id: root

    width: 420
    height: 430
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    readonly property var sessions: AppController.workflows.activeSessions
    readonly property var workflows: AppController.workflows.activeWorkflows
    readonly property int totalItems: sessions.length + workflows.length

    function elapsedText(seconds) {
        if (seconds < 60) return qsTr("%1s").arg(seconds)
        var minutes = Math.floor(seconds / 60)
        var remaining = seconds % 60
        if (minutes < 60) return qsTr("%1m %2s").arg(minutes).arg(remaining)
        var hours = Math.floor(minutes / 60)
        return qsTr("%1h %2m").arg(hours).arg(minutes % 60)
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Theme.surfaceAlt
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            color: "#00000025"
            radius: Theme.radiusMedium + 8
            z: -1
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: qsTr("Activity")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontLarge
                        font.bold: true
                    }

                    Text {
                        text: {
                            if (sessions.length > 0 && workflows.length > 0) {
                                return qsTr("%1 model loaded, %2 job running").arg(sessions.length).arg(workflows.length)
                            }
                            if (sessions.length > 0) {
                                return qsTr("%1 model loaded").arg(sessions.length)
                            }
                            if (workflows.length > 0) {
                                return qsTr("%1 job running").arg(workflows.length)
                            }
                            return qsTr("No loaded models")
                        }
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }
                }

                Button {
                    id: closeBtn
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: root.close()
                    contentItem: LineIcon {
                        name: "close"
                        color: closeBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                    }
                    background: Rectangle {
                        radius: 6
                        color: closeBtn.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.surfaceAlt
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: totalItems > 0
            clip: true
            leftPadding: Theme.paddingLarge
            rightPadding: Theme.paddingLarge
            topPadding: Theme.paddingLarge
            bottomPadding: Theme.paddingLarge

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.paddingMedium

                Text {
                    Layout.fillWidth: true
                    visible: sessions.length > 0
                    text: qsTr("Loaded models")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }

                Repeater {
                    model: sessions

                    delegate: Rectangle {
                        id: sessionRow
                        required property var modelData

                        Layout.fillWidth: true
                        height: 118
                        radius: Theme.radiusSmall
                        color: sessionHover.hovered ? Qt.rgba(1, 1, 1, 0.025) : Qt.rgba(1, 1, 1, 0.012)
                        border.color: Qt.rgba(1, 1, 1, 0.07)
                        border.width: 1

                        HoverHandler { id: sessionHover }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingMedium
                            spacing: Theme.paddingMedium

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: 7
                                color: Theme.surfaceAlt
                                border.color: Qt.rgba(1, 1, 1, 0.05)

                                LineIcon {
                                    anchors.centerIn: parent
                                    name: sessionRow.modelData.iconName || "activity"
                                    color: Theme.success
                                    width: 19
                                    height: 19
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingSmall

                                    Text {
                                        Layout.fillWidth: true
                                        text: sessionRow.modelData.title
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMedium
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: sessionStatusText.implicitWidth + 14
                                        Layout.preferredHeight: 20
                                        radius: 5
                                        color: Qt.rgba(0.22, 0.78, 0.40, 0.12)
                                        border.color: Qt.rgba(0.22, 0.78, 0.40, 0.32)
                                        border.width: 1

                                        Text {
                                            id: sessionStatusText
                                            anchors.centerIn: parent
                                            text: sessionRow.modelData.statusLabel
                                            color: Theme.success
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: sessionRow.modelData.runtime || qsTr("Runtime loaded")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    MetricPill {
                                        visible: sessionRow.modelData.ramUsage !== ""
                                        label: qsTr("RAM")
                                        value: sessionRow.modelData.ramUsage
                                    }

                                    MetricPill {
                                        visible: sessionRow.modelData.vramUsage !== ""
                                        label: qsTr("VRAM")
                                        value: sessionRow.modelData.vramUsage
                                    }

                                    MetricPill {
                                        label: qsTr("CPU")
                                        value: qsTr("%1%").arg(Math.round(sessionRow.modelData.cpuUsage || 0))
                                    }

                                    MetricPill {
                                        label: qsTr("Files")
                                        value: sessionRow.modelData.modelFileCount || 0
                                    }

                                    Item { Layout.fillWidth: true }
                                }
                            }

                            Button {
                                id: openSessionBtn
                                implicitWidth: 34
                                implicitHeight: 30
                                onClicked: {
                                    AppController.workflows.openWorkflow(sessionRow.modelData.id)
                                    root.close()
                                }
                                contentItem: LineIcon {
                                    name: "external-link"
                                    color: openSessionBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                                    anchors.centerIn: parent
                                    width: 15
                                    height: 15
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: openSessionBtn.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                                }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                AppToolTip {
                                    text: qsTr("Open studio")
                                    visible: openSessionBtn.hovered
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: workflows.length > 0
                    text: qsTr("Running jobs")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    Layout.topMargin: sessions.length > 0 ? Theme.paddingSmall : 0
                }

                Repeater {
                    model: workflows

                    delegate: Rectangle {
                        id: workflowRow
                        required property var modelData

                        Layout.fillWidth: true
                        height: 104
                        radius: Theme.radiusSmall
                        color: rowHover.hovered ? Qt.rgba(1, 1, 1, 0.025) : Qt.rgba(1, 1, 1, 0.012)
                        border.color: Qt.rgba(1, 1, 1, 0.07)
                        border.width: 1

                        HoverHandler { id: rowHover }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingMedium
                            spacing: Theme.paddingMedium

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: 7
                                color: Theme.surfaceAlt
                                border.color: Qt.rgba(1, 1, 1, 0.05)

                                LineIcon {
                                    anchors.centerIn: parent
                                    name: workflowRow.modelData.iconName || "activity"
                                    color: Theme.accentLight
                                    width: 19
                                    height: 19
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingSmall

                                    Text {
                                        Layout.fillWidth: true
                                        text: workflowRow.modelData.title
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMedium
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: elapsedText(workflowRow.modelData.elapsedSeconds || 0)
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: workflowRow.modelData.status === "cancelling"
                                          ? workflowRow.modelData.statusLabel
                                          : (workflowRow.modelData.stageLabel || workflowRow.modelData.statusLabel)
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingSmall

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 5
                                        radius: 3
                                        color: Theme.background

                                        Rectangle {
                                            width: parent.width * Math.max(0, Math.min(100, workflowRow.modelData.progress || 0)) / 100
                                            height: parent.height
                                            radius: 3
                                            color: workflowRow.modelData.progressEstimated ? Theme.warning : Theme.accent
                                        }
                                    }

                                    Text {
                                        text: qsTr("%1%").arg(Math.max(0, Math.min(100, workflowRow.modelData.progress || 0)))
                                        color: workflowRow.modelData.progressEstimated ? Theme.warning : Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        Layout.preferredWidth: 38
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }

                            ColumnLayout {
                                spacing: Theme.paddingSmall

                                Button {
                                    id: openBtn
                                    implicitWidth: 34
                                    implicitHeight: 30
                                    onClicked: {
                                        AppController.workflows.openWorkflow(workflowRow.modelData.id)
                                        root.close()
                                    }
                                    contentItem: LineIcon {
                                        name: "external-link"
                                        color: openBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 15
                                        height: 15
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: openBtn.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                                    }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    AppToolTip {
                                        text: qsTr("Open workflow")
                                        visible: openBtn.hovered
                                    }
                                }

                                Button {
                                    id: stopBtn
                                    visible: workflowRow.modelData.cancellable
                                    enabled: workflowRow.modelData.status !== "cancelling"
                                    implicitWidth: 34
                                    implicitHeight: 30
                                    onClicked: AppController.workflows.stopWorkflow(workflowRow.modelData.id)
                                    contentItem: LineIcon {
                                        name: "stop"
                                        color: stopBtn.hovered ? Theme.danger : Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 15
                                        height: 15
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: stopBtn.hovered ? Qt.rgba(1.0, 0.22, 0.22, 0.10) : "transparent"
                                    }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    AppToolTip {
                                        text: qsTr("Stop workflow")
                                        visible: stopBtn.hovered
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: totalItems === 0

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.paddingSmall

                LineIcon {
                    name: "activity"
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                    width: 30
                    height: 30
                    opacity: 0.55
                }

                Text {
                    text: qsTr("No loaded models")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }

    component MetricPill: Rectangle {
        id: pill
        property string label: ""
        property string value: ""

        Layout.preferredWidth: metricRow.implicitWidth + 14
        Layout.preferredHeight: 22
        radius: 5
        color: Qt.rgba(1, 1, 1, 0.035)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        RowLayout {
            id: metricRow
            anchors.centerIn: parent
            spacing: 4

            Text {
                text: pill.label
                color: Theme.textSecondary
                font.pixelSize: 10
                font.bold: true
            }

            Text {
                text: pill.value
                color: Theme.textPrimary
                font.pixelSize: 10
                font.bold: true
            }
        }
    }
}
