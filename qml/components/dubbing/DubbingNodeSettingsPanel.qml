import QtQuick
import QtQuick.Layouts
import "../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property string nodeId
    required property var node
    required property string nodeTitle
    required property bool canRun
    required property bool canRerun
    required property bool runReady
    required property string nextNodeId
    required property bool nextReady

    signal configureRequested()
    signal runRequested()
    signal nextRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: 52
    radius: Theme.radiusSmall
    color: Theme.surfaceAlt
    border.color: Qt.rgba(1, 1, 1, 0.08)

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingSmall
        spacing: Theme.paddingSmall
        LineIcon { name: "settings"; color: Theme.accentLight; width: 16; height: 16 }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text { text: root.node && root.node.configurable ? qsTr("%1 settings").arg(root.nodeTitle) : qsTr("%1 actions").arg(root.nodeTitle); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true }
            Text { Layout.fillWidth: true; text: root.node && root.node.providerName ? root.node.providerName : qsTr("Use the workflow default model"); color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
        }
        PrimaryButton { text: qsTr("Configure"); iconName: "settings"; quiet: true; visible: root.node && root.node.configurable === true; enabled: !root.dubbing.processing; onClicked: root.configureRequested() }
        PrimaryButton { visible: root.canRun; text: qsTr("Run"); iconName: "play"; enabled: !root.dubbing.processing && root.runReady; Layout.preferredWidth: 104; onClicked: root.runRequested() }
        PrimaryButton { visible: root.canRerun; text: qsTr("Run Again"); iconName: "refresh"; quiet: true; enabled: !root.dubbing.processing && root.runReady; Layout.preferredWidth: 104; onClicked: root.runRequested() }
        PrimaryButton { visible: root.nextNodeId !== "" && root.nextReady; text: qsTr("Next"); iconName: "chevron-right"; enabled: !root.dubbing.processing; onClicked: root.nextRequested() }
    }
}
