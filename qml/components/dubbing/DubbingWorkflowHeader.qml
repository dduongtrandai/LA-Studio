pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import "../shared"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property var steps
    required property string statusText
    required property string defaultExportPath
    property bool historyOpen: false

    signal stepSelected(string stepId)
    signal historyToggled()
    signal generateRequested()
    signal workflowRequested()
    signal saveRequested()
    signal exportRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: 58
    color: Qt.rgba(0, 0, 0, 0.10)
    border.color: Qt.rgba(1, 1, 1, 0.06)
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.paddingLarge
        anchors.rightMargin: Theme.paddingLarge
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.preferredWidth: 205
            spacing: Theme.paddingSmall
            LineIcon { name: "dubbing"; color: Theme.accentLight; Layout.preferredWidth: 21; Layout.preferredHeight: 21 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Text { text: qsTr("Dubbing Studio"); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                Text { text: root.dubbing.hasProject ? qsTr("Project workspace") : qsTr("New project"); color: Theme.textSecondary; font.pixelSize: 10 }
            }
            Button {
                id: historyButton

                implicitWidth: 32
                implicitHeight: 32
                flat: true
                checkable: true
                checked: root.historyOpen

                AppToolTip {
                    text: root.historyOpen ? qsTr("Hide dubbing history") : qsTr("Show dubbing history")
                    visible: historyButton.hovered
                }

                contentItem: LineIcon {
                    anchors.centerIn: parent
                    name: "history"
                    color: root.historyOpen ? Theme.accentLight
                                            : (historyButton.hovered ? Theme.textPrimary : Theme.textSecondary)
                    width: 18
                    height: 18
                }

                background: Rectangle {
                    radius: 7
                    color: root.historyOpen
                           ? Qt.rgba(0.49, 0.30, 1.0, 0.16)
                           : (historyButton.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent")
                    border.color: root.historyOpen
                                  ? Qt.rgba(0.49, 0.30, 1.0, 0.42)
                                  : (historyButton.hovered ? Qt.rgba(1, 1, 1, 0.10) : "transparent")
                    border.width: 1
                }

                onClicked: root.historyToggled()
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }

        Repeater {
            model: root.steps
            delegate: Item {
                required property var modelData
                implicitWidth: step.implicitWidth
                implicitHeight: step.implicitHeight
                DubbingWorkflowStep {
                    id: step
                    anchors.fill: parent
                    stepId: modelData.stepId
                    title: modelData.title
                    iconName: modelData.iconName
                    complete: modelData.complete
                    active: modelData.active
                    onSelected: root.stepSelected(stepId)
                }
            }
        }

        Item { Layout.fillWidth: true }

        PrimaryButton {
            text: root.dubbing.processing ? qsTr("Running…") : qsTr("Generate Final Dub")
            iconName: root.dubbing.processing ? "activity" : "play"
            enabled: !root.dubbing.processing && root.dubbing.workflowReady
            onClicked: root.generateRequested()
            AppToolTip { text: qsTr("Run every stage automatically and create the final dubbed output"); visible: parent.hovered }
        }
        PrimaryButton {
            text: qsTr("Workflow")
            iconName: "workflow"
            quiet: true
            onClicked: root.workflowRequested()
            AppToolTip { text: qsTr("View and configure workflow"); visible: parent.hovered }
        }
        Rectangle {
            implicitWidth: statusRow.implicitWidth + 16
            implicitHeight: 28
            radius: 14
            color: Qt.rgba(root.dubbing.processing ? Theme.warning.r : Theme.success.r,
                           root.dubbing.processing ? Theme.warning.g : Theme.success.g,
                           root.dubbing.processing ? Theme.warning.b : Theme.success.b, 0.12)
            RowLayout {
                id: statusRow
                anchors.centerIn: parent
                spacing: 5
                Rectangle { width: 6; height: 6; radius: 3; color: root.dubbing.processing ? Theme.warning : Theme.success }
                Text { text: root.statusText; color: root.dubbing.processing ? Theme.warning : Theme.success; font.pixelSize: Theme.fontSmall; font.bold: true }
            }
        }
        PrimaryButton { text: qsTr("Save"); iconName: "save"; quiet: true; enabled: root.dubbing.hasProject; onClicked: root.saveRequested() }
        PrimaryButton { text: qsTr("Export"); iconName: "download"; enabled: root.dubbing.hasProject && !root.dubbing.processing; onClicked: root.exportRequested() }
    }
}
