import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"
import ".."

Rectangle {
    id: root
    Layout.fillWidth: true
    Layout.preferredHeight: 58
    color: Theme.background

    property var family: null
    property string statusText: "Ready"
    property color statusColor: Theme.success
    property var currentRuntimeItem: null
    property bool modelLoaded: false
    property bool processing: false
    property bool lifecycleBusy: false
    property bool modelConfigured: false
    property real cpuUsage: 0
    property string estimatedRamUsage: ""
    property string estimatedVramUsage: ""
    property string loadedModelName: ""
    property var loadedModelFiles: []
    property var loadedModels: []
    property string activeModelId: ""
    property string inferenceElapsedText: ""
    property bool studioReady: false
    property string studioTitle: ""
    property string studioIconName: ""
    property string backToolTip: qsTr("Back")
    property string runtimeDisplayText: ""
    property bool runtimeClickable: false
    property bool modelPickerOpen: false
    property bool workflowMode: false
    property bool workflowReady: false
    property bool workflowBusy: false
    property real workflowProgress: 0
    property string workflowTitle: qsTr("Workflow")
    property string workflowStatusText: ""
    property string workflowActionText: qsTr("Set up workflow")
    readonly property bool showLoadedModelCard: root.modelLoaded && root.width >= 1180
    readonly property bool showRuntimeCard: root.modelConfigured && root.width >= 760
    readonly property bool showStatusDetails: root.width >= 760

    signal backClicked()
    signal runtimeClicked()
    signal reloadRequested()
    signal ejectRequested()
    signal loadedModelActivated(string modelId)
    signal loadedModelUnloadRequested(string modelId)
    signal loadAnotherModelRequested()
    signal configureCurrentModelRequested()
    signal loadedModelPickerRequested()
    signal workflowClicked()

    function loadedFilesSummary() {
        var count = root.loadedModelFiles ? root.loadedModelFiles.length : 0
        if (count <= 0) return qsTr("No files reported")
        if (count === 1) return qsTr("1 file loaded")
        return qsTr("%1 files loaded").arg(count)
    }

    function friendlyModelName() {
        if (root.loadedModelName !== "") return root.loadedModelName
        if (root.family && root.family.title) return root.family.title
        return ""
    }

    function runtimeSummary() {
        if (root.currentRuntimeItem) {
            return root.currentRuntimeItem.name
                    + (root.currentRuntimeItem.version ? "  ·  " + root.currentRuntimeItem.version : "")
        }
        return root.runtimeDisplayText
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.paddingLarge
        anchors.rightMargin: Theme.paddingLarge
        spacing: Theme.paddingMedium
        clip: true

        ToolIconButton {
            iconName: "chevron-left"
            toolTip: root.backToolTip
            enabled: !root.processing
            onClicked: root.backClicked()
        }

        Item {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            visible: root.width > 560

            Image {
                anchors.centerIn: parent
                source: "qrc:/LAStudio/icons/app_icon_32.png"
                width: 36
                height: 36
                fillMode: Image.PreserveAspectFit
                visible: root.studioIconName === ""
            }

            Rectangle {
                anchors.fill: parent
                radius: 9
                color: Qt.rgba(0.49, 0.30, 1.0, 0.13)
                border.color: Qt.rgba(0.49, 0.30, 1.0, 0.30)
                border.width: 1
                visible: root.studioIconName !== ""

                LineIcon {
                    anchors.centerIn: parent
                    name: root.studioIconName
                    color: Theme.accentLight
                    width: Theme.iconSize
                    height: Theme.iconSize
                    strokeWidth: 1.8
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 150
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: root.studioTitle !== "" ? root.studioTitle : (root.family ? qsTr("%1 Studio").arg(root.family.title) : qsTr("Studio"))
                color: Theme.textPrimary
                font.pixelSize: Theme.fontLarge
                font.bold: true
                elide: Text.ElideRight
            }

            RowLayout {
                spacing: Theme.paddingSmall

                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 4
                    color: root.statusColor
                }

                Text {
                    text: root.statusText
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }

                Text {
                    text: root.friendlyModelName() !== "" ? " ·  " + root.friendlyModelName() : ""
                    color: Theme.textSecondary
                    opacity: 0.8
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.maximumWidth: 220
                    visible: root.modelConfigured && root.showStatusDetails
                }

                Rectangle {
                    radius: 5
                    color: Qt.rgba(0.49, 0.30, 1.0, 0.16)
                    border.color: Qt.rgba(0.49, 0.30, 1.0, 0.40)
                    border.width: 1
                    visible: root.inferenceElapsedText !== "" && root.width >= 680
                    implicitHeight: 20
                    implicitWidth: inferenceTimerText.implicitWidth + 10

                    Text {
                        id: inferenceTimerText
                        anchors.centerIn: parent
                        text: root.processing ? qsTr("Inference %1").arg(root.inferenceElapsedText) : qsTr("Last %1").arg(root.inferenceElapsedText)
                        color: Theme.textPrimary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    radius: 5
                    color: Qt.rgba(0.22, 0.62, 0.39, 0.18)
                    border.color: Qt.rgba(0.22, 0.62, 0.39, 0.45)
                    border.width: 1
                    visible: root.processing && root.width >= 720
                    implicitHeight: 20
                    implicitWidth: cpuText.implicitWidth + 10

                    Text {
                        id: cpuText
                        anchors.centerIn: parent
                        text: "CPU " + root.cpuUsage.toFixed(1) + "%"
                        color: Theme.textPrimary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    radius: 5
                    color: Qt.rgba(0.22, 0.62, 0.39, 0.18)
                    border.color: Qt.rgba(0.22, 0.62, 0.39, 0.45)
                    border.width: 1
                    visible: root.modelLoaded && root.width >= 780
                    implicitHeight: 20
                    implicitWidth: ramText.implicitWidth + 10

                    Text {
                        id: ramText
                        anchors.centerIn: parent
                        text: "RAM " + root.estimatedRamUsage
                        color: Theme.textPrimary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    radius: 5
                    color: Qt.rgba(0.31, 0.48, 0.89, 0.16)
                    border.color: Qt.rgba(0.31, 0.48, 0.89, 0.40)
                    border.width: 1
                    visible: root.modelLoaded && root.width >= 900
                    implicitHeight: 20
                    implicitWidth: vramText.implicitWidth + 10

                    Text {
                        id: vramText
                        anchors.centerIn: parent
                        text: "VRAM " + root.estimatedVramUsage
                        color: Theme.textPrimary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {
            visible: root.workflowMode
            Layout.preferredWidth: Math.min(220, Math.max(150, workflowButtonText.implicitWidth + 54))
            Layout.preferredHeight: 38
            radius: Theme.radiusSmall
            color: workflowMouse.containsMouse
                   ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                   : Qt.rgba(1, 1, 1, 0.035)
            border.color: root.workflowReady ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.42)
                                               : Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.48)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingSmall

                LineIcon {
                    name: root.workflowBusy ? "activity" : (root.workflowReady ? "check" : "settings")
                    color: root.workflowReady ? Theme.success : (root.workflowBusy ? Theme.accentLight : Theme.warning)
                    width: 16
                    height: 16
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        id: workflowButtonText
                        Layout.fillWidth: true
                        text: root.workflowActionText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.workflowBusy
                              ? qsTr("Preparing %1%").arg(Math.round(root.workflowProgress * 100))
                              : root.workflowStatusText
                        color: root.workflowReady ? Theme.success : Theme.textSecondary
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }

            MouseArea {
                id: workflowMouse
                anchors.fill: parent
                enabled: !root.processing
                hoverEnabled: true
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.workflowClicked()
            }
        }

        Rectangle {
            id: loadedModelInfo
            visible: root.showLoadedModelCard
            Layout.preferredWidth: 230
            Layout.minimumWidth: 170
            Layout.maximumWidth: 260
            Layout.preferredHeight: 38
            radius: 8
            color: modelInfoMouse.containsMouse || root.modelPickerOpen ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(1, 1, 1, 0.03)
            border.color: root.modelPickerOpen ? Qt.rgba(0.49, 0.30, 1.0, 0.45) : Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                LineIcon {
                    name: "file"
                    color: Theme.accentLight
                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        text: root.loadedModelName !== "" ? root.loadedModelName : (root.family ? root.family.title : qsTr("Loaded model"))
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.loadedFilesSummary()
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }

                LineIcon {
                    name: root.modelPickerOpen ? "chevron-up" : "chevron-down"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 13
                    Layout.preferredHeight: 13
                }
            }

            MouseArea {
                id: modelInfoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.loadedModelPickerRequested()
            }
        }

        Rectangle {
            visible: root.showRuntimeCard
            Layout.preferredWidth: 280
            Layout.minimumWidth: 120
            Layout.maximumWidth: 320
            Layout.preferredHeight: 38
            radius: 8
            color: Qt.rgba(1, 1, 1, 0.03)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                LineIcon {
                    name: "cpu"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }

                Text {
                    Layout.fillWidth: true
                    text: root.runtimeSummary()
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: root.runtimeClickable
                hoverEnabled: root.runtimeClickable
                cursorShape: root.runtimeClickable ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.runtimeClicked()
            }
        }

        PrimaryButton {
            visible: !root.modelLoaded
            Layout.preferredWidth: 132
            Layout.minimumWidth: 118
            Layout.preferredHeight: 38
            text: qsTr("Load Model")
            iconName: "gallery"
            loading: root.lifecycleBusy
            enabled: !root.lifecycleBusy && !root.processing
            onClicked: root.loadAnotherModelRequested()
        }

        Rectangle {
            visible: root.modelLoaded
            Layout.preferredWidth: 114
            Layout.minimumWidth: 114
            Layout.maximumWidth: 114
            Layout.preferredHeight: 38
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            radius: 10
            color: Qt.rgba(1, 1, 1, 0.032)
            border.color: Qt.rgba(1, 1, 1, 0.085)
            border.width: 1

            RowLayout {
                id: modelActions
                anchors.fill: parent
                anchors.margins: 3
                spacing: 0

                HeaderActionButton {
                    iconName: "gallery"
                    toolTip: qsTr("Loaded model details")
                    enabled: root.modelLoaded && !root.processing && !root.lifecycleBusy
                    onClicked: root.loadedModelPickerRequested()
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignVCenter
                    color: Qt.rgba(1, 1, 1, 0.08)
                }

                HeaderActionButton {
                    iconName: "refresh"
                    toolTip: qsTr("Reload current model")
                    enabled: root.modelLoaded && !root.processing && !root.lifecycleBusy
                    onClicked: root.reloadRequested()
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignVCenter
                    color: Qt.rgba(1, 1, 1, 0.08)
                }

                HeaderActionButton {
                    iconName: "power"
                    toolTip: qsTr("Unload model from memory")
                    enabled: root.modelLoaded && !root.processing && !root.lifecycleBusy
                    onClicked: root.ejectRequested()
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.surfaceAlt
    }

    component ToolIconButton: Button {
        id: iconButton
        property string iconName: ""
        property string toolTip: ""

        implicitWidth: 32
        implicitHeight: 32
        flat: true

        AppToolTip {
            text: iconButton.toolTip
            visible: iconButton.hovered && iconButton.toolTip !== ""
        }

        contentItem: LineIcon {
            name: iconButton.iconName
            color: iconButton.hovered ? Theme.accent : Theme.textSecondary
            anchors.centerIn: parent
            width: 18
            height: 18
        }

        background: Rectangle {
            radius: 7
            color: iconButton.checked ? Qt.rgba(0.49, 0.30, 1.0, 0.14) : (iconButton.hovered ? Qt.rgba(1, 1, 1, 0.045) : "transparent")
            border.color: iconButton.checked ? Qt.rgba(0.49, 0.30, 1.0, 0.70) : Qt.rgba(1, 1, 1, 0.08)
            border.width: iconButton.checked || iconButton.hovered ? 1 : 0
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }

    component HeaderActionButton: Button {
        id: actionButton
        property string iconName: "refresh"
        property string toolTip: ""

        Layout.preferredWidth: 35
        Layout.preferredHeight: 32
        padding: 0
        flat: true

        AppToolTip {
            text: toolTip
            visible: parent.hovered && toolTip !== ""
        }

        contentItem: LineIcon {
            name: actionButton.iconName
            color: !actionButton.enabled ? Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.42)
                  : actionButton.hovered ? Theme.accent : Theme.textSecondary
            strokeWidth: actionButton.hovered ? 2.0 : 1.75
            anchors.centerIn: parent
            width: 17
            height: 17
        }

        background: Rectangle {
            radius: 8
            color: actionButton.enabled && actionButton.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.14) : "transparent"
            border.color: actionButton.enabled && actionButton.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.45) : "transparent"
            border.width: actionButton.enabled && actionButton.hovered ? 1 : 0
        }

        HoverHandler { cursorShape: actionButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }
}
