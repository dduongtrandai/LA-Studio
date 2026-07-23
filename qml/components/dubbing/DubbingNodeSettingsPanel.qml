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
    signal loadRequested()
    signal unloadRequested()
    signal reloadRequested()
    signal runRequested()
    signal nextRequested()
    signal fixRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: 66
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
        Text { visible: root.node && root.node.configurable === true; text: root.modelStateLabel(); color: root.modelStateColor(); font.pixelSize: 10; font.bold: true; Layout.preferredWidth: 62; horizontalAlignment: Text.AlignRight }
        PrimaryButton {
            text: root.modelActionText()
            iconName: root.modelActionIcon()
            iconOnly: root.modelState() === 3
            toolTip: root.modelState() === 3 ? root.modelActionText() : ""
            visible: root.node && root.node.configurable === true
            Layout.preferredWidth: root.modelState() === 3 ? 38 : 132
            Layout.minimumWidth: root.modelState() === 3 ? 38 : 132
            enabled: !root.lifecycleBusy()
            quiet: root.modelState() === 3
            onClicked: root.runModelAction()
        }
        PrimaryButton { iconName: "reload"; iconOnly: true; toolTip: qsTr("Reload model"); quiet: true; visible: root.canReload(); enabled: !root.lifecycleBusy(); onClicked: root.reloadRequested() }
        PrimaryButton { iconName: "power"; iconOnly: true; toolTip: qsTr("Unload model"); quiet: true; visible: root.canUnload(); enabled: !root.lifecycleBusy(); onClicked: root.unloadRequested() }
        PrimaryButton { visible: root.canRun; iconName: "play"; iconOnly: true; toolTip: qsTr("Run"); enabled: !root.dubbing.processing && root.runReady; onClicked: root.runRequested() }
        PrimaryButton { visible: root.canRerun; iconName: "run-again"; iconOnly: true; toolTip: qsTr("Run again"); quiet: true; enabled: !root.dubbing.processing && root.runReady; onClicked: root.runRequested() }
        PrimaryButton {
            visible: root.nodeId === "translate"
                     && root.dubbing.dubbingQuality !== "fast"
                     && root.dubbing.adaptiveProvider !== "local"
                     && root.dubbing.translationFixCandidateCount > 0
            text: qsTr("Fix %1").arg(root.dubbing.translationFixCandidateCount)
            iconName: "spark"
            quiet: true
            loading: root.dubbing.translationFixing
            enabled: !root.dubbing.processing
            onClicked: root.fixRequested()
        }
        PrimaryButton { visible: root.nextNodeId !== "" && root.nextReady; text: qsTr("Next"); iconName: "chevron-right"; enabled: !root.dubbing.processing; onClicked: root.nextRequested() }
    }

    function modelState() { return root.node && root.node.modelState !== undefined ? root.node.modelState : 0 }
    function lifecycleBusy() { return [2, 4, 5].indexOf(root.modelState()) >= 0 || root.dubbing.processing }
    function canLoad() { return root.node && root.node.configurable === true && [1, 6].indexOf(root.modelState()) >= 0 }
    function canReload() { return root.node && root.node.configurable === true && root.modelState() === 3 }
    function canUnload() { return root.node && root.node.configurable === true && [3, 6].indexOf(root.modelState()) >= 0 }
    function modelActionText() {
        var state = root.modelState()
        if (state === 0) return qsTr("Open model")
        if ([1, 6].indexOf(state) >= 0) return qsTr("Load model")
        return qsTr("Change model")
    }
    function modelActionIcon() {
        var state = root.modelState()
        if (state === 0) return "gallery"
        if ([1, 6].indexOf(state) >= 0) return "download"
        return "settings"
    }
    function runModelAction() {
        if ([1, 6].indexOf(root.modelState()) >= 0) root.loadRequested()
        else root.configureRequested()
    }
    function modelStateLabel() {
        var labels = [qsTr("Unconfigured"), qsTr("Unloaded"), qsTr("Loading"), qsTr("Ready"), qsTr("Running"), qsTr("Unloading"), qsTr("Error")]
        return labels[root.modelState()] || qsTr("Unknown")
    }
    function modelStateColor() {
        var state = root.modelState()
        return state === 3 ? Theme.success : (state === 6 ? Theme.danger : Theme.warning)
    }
}
