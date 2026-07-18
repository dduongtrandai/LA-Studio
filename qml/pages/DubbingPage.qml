import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia
import "../components/base"
import "../components/alignment"
import "../components/dubbing"
import "../components/shared"
import LAStudio

Item {
    id: root
    anchors.fill: parent

    property var dubbing: AppController.dubbing
    property int selectedSegment: -1
    property bool isVideoSource: dubbing.sourceMediaPath.length > 0 && /\.(mp4|mkv|mov|webm|avi)$/i.test(dubbing.sourceMediaPath)
    property string reviewStepId: "import"
    property string observedCompletedStep: ""
    property string playingSeparationStem: ""
    property string playingVoiceClipPath: ""
    property bool isHistoryOpen: true
    property string pendingHistoryDeleteId: ""
    readonly property var languageCatalog: AppController.catalog.languageSet("default")

    StudioPageController {
        id: translationRecommendationController
        capabilityId: "translation"
        autoLoadOnSync: false
    }

    Connections {
        target: dubbing
        function onWorkflowChanged() {
            if (dubbing.processing) {
                root.reviewStepId = dubbing.currentStepId
            } else if (dubbing.lastCompletedStepId !== "" && dubbing.lastCompletedStepId !== root.observedCompletedStep) {
                root.observedCompletedStep = dubbing.lastCompletedStepId
                root.reviewStepId = dubbing.lastCompletedStepId
            }
        }
    }

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (!AppController.player.playing) {
                root.playingSeparationStem = ""
                root.playingVoiceClipPath = ""
            }
        }
    }

    function defaultExportPath() {
        var isVideo = /\.(mp4|mkv|mov|webm|avi)$/i.test(dubbing.sourceMediaPath)
        return dubbing.projectPath.replace(/\.json$/i, isVideo ? "-dubbed.mp4" : "-dubbed.wav")
    }

    function openWorkflowCanvas() {
        dubbing.prepareWorkflow()
        workflowDialog.open()
    }

    function stepTitle(stepId) {
        if (stepId === "import") return qsTr("Import")
        if (stepId === "ingest") return qsTr("Normalize")
        if (stepId === "source-separate") return qsTr("Separate speech")
        if (stepId === "transcribe") return qsTr("Transcribe")
        if (stepId === "translate") return qsTr("Translate")
        if (stepId === "synthesize") return qsTr("Generate voice")
        if (stepId === "mix") return qsTr("Mix audio")
        if (stepId === "export") return qsTr("Export")
        return qsTr("Completed")
    }

    function workflowNode(nodeId) {
        var nodes = dubbing.workflowNodes || []
        for (var i = 0; i < nodes.length; ++i)
            if (nodes[i].id === nodeId) return nodes[i]
        return null
    }

    function nextNodeId(nodeId) {
        var next = {"import": "ingest", "ingest": "source-separate", "source-separate": "transcribe", "transcribe": "translate", "translate": "synthesize", "synthesize": "mix", "mix": "export"}
        return next[nodeId] || ""
    }

    function nextNodeReady(nodeId) {
        return root.nextNodeId(nodeId) !== "" && root.stepComplete(nodeId)
    }

    function runNextNode(nodeId) {
        var next = nextNodeId(nodeId)
        if (next === "") return
        root.reviewStepId = next
        dubbing.startStepByStep()
    }

    function stepComplete(stepId) {
        if (stepId === "import") return dubbing.sourceMediaPath.length > 0
        if (stepId === "ingest") return dubbing.normalizedAudioPath.length > 0
        if (stepId === "source-separate") return dubbing.vocalsPath.length > 0 && dubbing.backgroundPath.length > 0
        if (stepId === "transcribe") return dubbing.segments.length > 0
        if (stepId === "translate") {
            if (dubbing.segments.length === 0) return false
            for (var i = 0; i < dubbing.segments.length; ++i)
                if (!(dubbing.segments[i].targetText || "").trim()) return false
            return true
        }
        if (stepId === "synthesize") {
            if (dubbing.segments.length === 0) return false
            for (var j = 0; j < dubbing.segments.length; ++j)
                if (!(dubbing.segments[j].clipPath || "")) return false
            return true
        }
        if (stepId === "mix") return dubbing.previewPath.length > 0
        if (stepId === "export") return dubbing.exportPath.length > 0
        return false
    }

    function canRerunStep(stepId) {
        return stepId !== "import" && stepId !== "completed" && root.stepComplete(stepId)
    }

    function canRunStep(stepId) {
        return ["ingest", "source-separate", "transcribe", "translate",
                "synthesize", "mix", "export"].indexOf(stepId) >= 0
            && !root.stepComplete(stepId)
    }

    function stepRunReady(stepId) {
        var node = root.workflowNode(stepId)
        if (!node || node.state === "missing" || node.state === "blocked") return false
        if (node.configurable === true && node.selectedFamilyId
                && node.providerState !== "ready") return false
        return true
    }

    function runStep(stepId) {
        dubbing.rerunStep(stepId, root.defaultExportPath())
    }

    function generatedClipCount() {
        var count = 0
        for (var i = 0; i < dubbing.segments.length; ++i)
            if ((dubbing.segments[i].clipPath || "") !== "") ++count
        return count
    }

    function languageDisplayName(code) {
        for (var i = 0; i < root.languageCatalog.length; ++i) {
            if (root.languageCatalog[i].value === code)
                return root.languageCatalog[i].text || code
        }
        return code
    }

    component Field: TextField {
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        selectByMouse: true
        leftPadding: Theme.paddingMedium
        rightPadding: Theme.paddingMedium
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.035)
            border.color: parent.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.09)
            border.width: parent.activeFocus ? 2 : 1
        }
    }

    component Panel: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    // Workflow node settings are rendered by DubbingNodeSettingsPanel.

    component TranslationSettingsPanel: Rectangle {
        id: translationPanel
        readonly property var node: root.workflowNode("translate")
        readonly property int recommendationRevision: translationRecommendationController.familiesModel.revision
        readonly property var recommendation: recommendationRevision >= 0
            ? translationRecommendationController.familiesModel.recommendedConfiguration() : ({})
        readonly property bool configured: node && node.selectedFamilyId
        readonly property string modelName: configured
            ? (node.providerName || node.selectedFamilyId)
            : (recommendation.modelName || qsTr("No compatible model"))
        readonly property string runtimeName: configured
            ? (node.selectedRuntimeId || qsTr("Runtime not selected"))
            : (recommendation.runtimeName || recommendation.runtimeId || qsTr("Runtime unavailable"))
        readonly property bool ready: configured
            ? node.providerState === "ready"
            : recommendation.ready === true
        Layout.fillWidth: true
        Layout.preferredHeight: 72
        radius: Theme.radiusSmall
        color: Theme.surfaceAlt
        border.color: Qt.rgba(1, 1, 1, 0.08)
        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingMedium
            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                LineIcon { anchors.centerIn: parent; name: "translate"; color: Theme.accentLight; width: 16; height: 16 }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    Text { text: qsTr("Translation model"); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true }
                    Rectangle {
                        implicitWidth: recommendationLabel.implicitWidth + Theme.paddingSmall * 2
                        implicitHeight: 20
                        radius: Theme.radiusSmall
                        color: Qt.rgba(translationPanel.ready ? Theme.success.r : Theme.warning.r,
                                       translationPanel.ready ? Theme.success.g : Theme.warning.g,
                                       translationPanel.ready ? Theme.success.b : Theme.warning.b, 0.12)
                        Text {
                            id: recommendationLabel
                            anchors.centerIn: parent
                            text: translationPanel.configured
                                ? (translationPanel.ready ? qsTr("Ready") : qsTr("Setup required"))
                                : qsTr("Recommended")
                            color: translationPanel.ready ? Theme.success : Theme.warning
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1  ·  %2").arg(translationPanel.modelName).arg(translationPanel.runtimeName)
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    visible: !translationPanel.configured && (translationPanel.recommendation.reason || "") !== ""
                    text: translationPanel.recommendation.reason || ""
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
            PrimaryButton {
                text: qsTr("Choose model")
                iconName: "settings"
                quiet: true
                enabled: !dubbing.processing
                onClicked: nodeModelDialog.openFor("translate")
            }
            PrimaryButton {
                visible: root.canRunStep("translate")
                text: qsTr("Run")
                iconName: "play"
                enabled: !dubbing.processing && translationPanel.ready
                    && root.stepRunReady("translate")
                Layout.preferredWidth: 104
                onClicked: root.runStep("translate")
            }
            PrimaryButton {
                visible: root.canRerunStep("translate")
                text: qsTr("Run Again")
                iconName: "refresh"
                quiet: true
                enabled: !dubbing.processing && translationPanel.ready
                    && root.stepRunReady("translate")
                Layout.preferredWidth: 104
                onClicked: root.runStep("translate")
            }
            PrimaryButton {
                visible: root.nextNodeReady("translate")
                text: qsTr("Next")
                iconName: "chevron-right"
                enabled: !dubbing.processing
                onClicked: root.runNextNode("translate")
            }
        }
    }

    Rectangle { anchors.fill: parent; color: Theme.background }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DubbingWorkflowHeader {
            dubbing: root.dubbing
            steps: [
                { stepId: "import", title: qsTr("Import"), iconName: "folder", complete: root.stepComplete("import"), active: root.dubbing.currentStepId === "import" },
                { stepId: "ingest", title: qsTr("Normalize"), iconName: "activity", complete: root.stepComplete("ingest"), active: root.dubbing.currentStepId === "ingest" },
                { stepId: "source-separate", title: qsTr("Separate"), iconName: "waves", complete: root.stepComplete("source-separate"), active: root.dubbing.currentStepId === "source-separate" },
                { stepId: "transcribe", title: qsTr("Transcribe"), iconName: "mic", complete: root.stepComplete("transcribe"), active: root.dubbing.currentStepId === "transcribe" },
                { stepId: "translate", title: qsTr("Translate"), iconName: "translate", complete: root.stepComplete("translate"), active: root.dubbing.currentStepId === "translate" },
                { stepId: "synthesize", title: qsTr("Voice"), iconName: "volume", complete: root.stepComplete("synthesize"), active: root.dubbing.currentStepId === "synthesize" },
                { stepId: "export", title: qsTr("Output"), iconName: "download", complete: root.stepComplete("export"), active: ["mix", "export", "completed"].indexOf(root.dubbing.currentStepId) >= 0 }
            ]
            statusText: root.dubbing.processing ? qsTr("%1 · %2%").arg(root.stepTitle(root.dubbing.currentStepId)).arg(root.dubbing.progress) : (root.dubbing.workflowMode === "step" ? qsTr("Ready for node run") : qsTr("Ready"))
            defaultExportPath: root.defaultExportPath()
            historyOpen: root.isHistoryOpen
            onStepSelected: root.reviewStepId = stepId
            onHistoryToggled: root.isHistoryOpen = !root.isHistoryOpen
            onGenerateRequested: root.dubbing.startAutomaticWorkflow(root.defaultExportPath())
            onWorkflowRequested: root.openWorkflowCanvas()
            onSaveRequested: root.dubbing.saveProject()
            onExportRequested: exportOptionsDialog.open()
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.margins: Theme.paddingMedium; spacing: Theme.paddingMedium

            DubbingHistoryPanel {
                id: historyPanel
                dubbing: root.dubbing
                expanded: root.isHistoryOpen
                onClearRequested: clearHistoryDialog.open()
                onDeleteRequested: function(historyId) {
                    root.pendingHistoryDeleteId = historyId
                    deleteHistoryDialog.open()
                }
                onProjectOpened: root.isHistoryOpen = false
                onExpandedChanged: root.isHistoryOpen = expanded
            }

            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 500; spacing: Theme.paddingMedium

                DubbingSourceMediaPanel {
                    id: sourceMediaPanel
                    dubbing: root.dubbing
                    selectedSegment: root.selectedSegment
                    onBrowseRequested: mediaFileDialog.open()
                    onSegmentSelected: root.selectedSegment = index
                    onSelectedSegmentChanged: root.selectedSegment = selectedSegment
                }
                Panel {
                    Layout.fillWidth: true; Layout.preferredHeight: 146
                    ColumnLayout { anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingSmall
                        RowLayout { Layout.fillWidth: true
                            Text { text: qsTr("TIMELINE"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1; Layout.fillWidth: true }
                            Text { text: qsTr("%1 segments").arg(dubbing.segments.length); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                        }
                        WaveformView { Layout.fillWidth: true; Layout.fillHeight: true; framed: true; showPlaceholder: true; placeholderText: dubbing.sourceMediaPath.length > 0 ? qsTr("Waveform preview becomes available after audio analysis") : qsTr("Import media to begin") }
                        RowLayout { Layout.fillWidth: true
                            Text { text: qsTr("00:00"); color: Theme.textSecondary; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                            Text { text: dubbing.processing ? qsTr("Processing %1%").arg(dubbing.progress) : qsTr("Edit transcript on the right"); color: Theme.textSecondary; font.pixelSize: 10 }
                        }
                    }
                }
            }

            Panel {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 670
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingSmall
                    visible: root.reviewStepId === "transcribe" || root.reviewStepId === "translate"
                    DubbingNodeSettingsPanel {
                        dubbing: root.dubbing
                        nodeId: root.reviewStepId
                        node: root.workflowNode(nodeId)
                        nodeTitle: root.stepTitle(nodeId)
                        canRun: root.canRunStep(nodeId)
                        canRerun: root.canRerunStep(nodeId)
                        runReady: root.stepRunReady(nodeId)
                        nextNodeId: root.nextNodeId(nodeId)
                        nextReady: root.nextNodeReady(nodeId)
                        visible: root.reviewStepId === "transcribe"
                        onConfigureRequested: nodeModelDialog.openFor(nodeId)
                        onRunRequested: root.runStep(nodeId)
                        onNextRequested: root.runNextNode(nodeId)
                    }
                    TranslationSettingsPanel { visible: root.reviewStepId === "translate" }
                    RowLayout { Layout.fillWidth: true
                        ColumnLayout { Layout.fillWidth: true; spacing: 1
                            Text { text: root.stepTitle(root.reviewStepId).toUpperCase(); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                            Text { text: qsTr("Review and edit every segment before continuing."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                        }
                        PrimaryButton { text: qsTr("Add segment"); iconName: "more-horizontal"; quiet: true; enabled: dubbing.hasProject && !dubbing.processing; onClicked: dubbing.addSegment(0, 2000, "") }
                    }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.paddingSmall
                        Field { Layout.fillWidth: true; placeholderText: qsTr("Search segments...") }
                        Text { text: qsTr("%1 / %1").arg(dubbing.segments.length); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    }
                    Rectangle { Layout.fillWidth: true; height: 30; color: Qt.rgba(1, 1, 1, 0.035); radius: Theme.radiusSmall
                        RowLayout { anchors.fill: parent; anchors.leftMargin: Theme.paddingSmall; anchors.rightMargin: Theme.paddingSmall; spacing: Theme.paddingSmall
                            Text { text: qsTr("TIME"); Layout.preferredWidth: 88; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true }
                            Text { text: qsTr("SOURCE / TARGET TEXT"); Layout.fillWidth: true; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true }
                            Text { text: qsTr("STATE"); Layout.preferredWidth: 64; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true }
                            Item { Layout.preferredWidth: 64 }
                        }
                    }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5; model: dubbing.segments
                        delegate: Rectangle {
                            width: ListView.view.width; height: 82; radius: Theme.radiusSmall
                            color: root.selectedSegment === index ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12) : Qt.rgba(1, 1, 1, 0.025)
                            border.color: root.selectedSegment === index ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55) : Qt.rgba(1, 1, 1, 0.06); border.width: 1
                            MouseArea {
                                anchors.fill: parent; z: -1
                                onClicked: {
                                    root.selectedSegment = index
                                    sourceMediaPanel.seekToSegment(index)
                                }
                            }
                            RowLayout { anchors.fill: parent; anchors.margins: Theme.paddingSmall; spacing: Theme.paddingSmall
                                Text { text: "%1–%2".arg(modelData.startMs).arg(modelData.endMs); color: Theme.textSecondary; font.pixelSize: 10; Layout.preferredWidth: 88; elide: Text.ElideRight }
                                ColumnLayout { Layout.fillWidth: true; spacing: 3
                                    Field { text: modelData.sourceText || ""; placeholderText: qsTr("Source transcript"); implicitHeight: 30; Layout.fillWidth: true; onEditingFinished: dubbing.updateSegment(index, { sourceText: text }) }
                                    Field { text: modelData.targetText || ""; placeholderText: qsTr("Target translation"); implicitHeight: 30; Layout.fillWidth: true; onEditingFinished: dubbing.updateSegment(index, { targetText: text }) }
                                }
                                Text { text: modelData.state || qsTr("Ready"); color: modelData.state === "stale" ? Theme.warning : Theme.textSecondary; font.pixelSize: 10; Layout.preferredWidth: 64; horizontalAlignment: Text.AlignRight }
                                PrimaryButton { text: qsTr("Remove"); quiet: true; Layout.preferredWidth: 64; onClicked: dubbing.removeSegment(index) }
                            }
                        }
                        Column { anchors.centerIn: parent; visible: dubbing.segments.length === 0; spacing: Theme.paddingSmall
                            LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "mic"; color: Theme.accentLight; width: 32; height: 32 }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Your transcript will appear here"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Import media, then run transcription."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                        }
                    }
                }
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: Theme.paddingLarge; spacing: Theme.paddingMedium
                    visible: root.reviewStepId !== "transcribe" && root.reviewStepId !== "translate"
                    DubbingNodeSettingsPanel {
                        dubbing: root.dubbing
                        nodeId: root.reviewStepId
                        node: root.workflowNode(nodeId)
                        nodeTitle: root.stepTitle(nodeId)
                        canRun: root.canRunStep(nodeId)
                        canRerun: root.canRerunStep(nodeId)
                        runReady: root.stepRunReady(nodeId)
                        nextNodeId: root.nextNodeId(nodeId)
                        nextReady: root.nextNodeReady(nodeId)
                        visible: ["import", "ingest", "source-separate", "synthesize", "mix", "export"].indexOf(root.reviewStepId) >= 0
                        onConfigureRequested: nodeModelDialog.openFor(nodeId)
                        onRunRequested: root.runStep(nodeId)
                        onNextRequested: root.runNextNode(nodeId)
                    }
                    RowLayout { Layout.fillWidth: true
                        ColumnLayout { Layout.fillWidth: true; spacing: 1
                            Text { text: root.stepTitle(root.reviewStepId).toUpperCase(); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                            Text { text: root.reviewStepId === "import" ? qsTr("Import only selects the source; no processing starts automatically.") : qsTr("Review this step output before continuing."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                        }
                    }
                    Item { Layout.fillHeight: true; visible: root.reviewStepId !== "synthesize" }
                    VoiceSeparationOutput {
                        visible: root.reviewStepId === "source-separate"
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? implicitHeight : 0
                        compact: true
                        showActions: true
                        showPlaybackControls: true
                        showExportButton: false
                        showWaveforms: false
                        vocalsPath: dubbing.vocalsPath
                        backgroundPath: dubbing.backgroundPath
                        playingStem: root.playingSeparationStem
                        onPlayRequested: function(kind, path) {
                            if (root.playingSeparationStem === kind && AppController.player.playing) {
                                AppController.player.stop()
                            } else {
                                root.playingVoiceClipPath = ""
                                AppController.player.playFile(path)
                                root.playingSeparationStem = kind
                            }
                        }
                    }
                    DubbingVoiceClipReview {
                        visible: root.reviewStepId === "synthesize"
                        dubbing: root.dubbing
                        sourceMediaPanel: sourceMediaPanel
                        playingVoiceClipPath: root.playingVoiceClipPath
                        generatedClipCount: root.generatedClipCount()
                        synthesisComplete: root.stepComplete("synthesize")
                        onVoiceClipPlaybackRequested: root.playingVoiceClipPath = path
                        onSeparationPlaybackStopped: root.playingSeparationStem = ""
                    }
                    ColumnLayout {
                        visible: root.reviewStepId !== "source-separate"
                                 && root.reviewStepId !== "synthesize"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Theme.paddingMedium
                        LineIcon { Layout.alignment: Qt.AlignHCenter; name: root.reviewStepId === "synthesize" ? "volume" : "folder"; color: Theme.accentLight; Layout.preferredWidth: 40; Layout.preferredHeight: 40 }
                        Text { Layout.alignment: Qt.AlignHCenter; text: root.stepComplete(root.reviewStepId) ? qsTr("Step output is ready") : qsTr("No output for this step yet"); color: root.stepComplete(root.reviewStepId) ? Theme.success : Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true }
                        Text {
                            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSmall
                            text: root.reviewStepId === "import" ? dubbing.sourceMediaPath
                                : root.reviewStepId === "ingest" ? (dubbing.normalizedAudioPath || qsTr("Run Normalize to create the working audio."))
                                : root.reviewStepId === "synthesize" ? qsTr("%1 segment clips available").arg(dubbing.segments.length)
                                : root.reviewStepId === "export" ? (dubbing.exportPath || dubbing.previewPath || qsTr("Run Mix and Export to create final media."))
                                : qsTr("Select a step in the topbar to inspect its output.")
                        }
                    }
                    Item {
                        Layout.fillHeight: true
                        visible: root.reviewStepId !== "synthesize"
                    }
                }
            }
        }

        DubbingProjectStatusPanel {
            dubbing: root.dubbing
            languageCatalog: root.languageCatalog
            currentStepTitle: root.stepTitle(root.dubbing.currentStepId)
        }
    }

    FileDialog {
        id: mediaFileDialog
        title: qsTr("Select media file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Media files (*.wav *.mp3 *.mp4 *.mkv *.mov *.webm)"), qsTr("All files (*)")]
        onAccepted: {
            var path = AppController.files.urlToLocalPath(selectedFile.toString())
            dubbing.importMedia(path)
        }
    }

    DubbingExportDialog {
        id: exportOptionsDialog
        parent: Overlay.overlay
        projectName: dubbing.projectPath
        videoSource: root.isVideoSource
        busy: dubbing.processing
        segmentCount: dubbing.segments.length
        generatedClipCount: root.generatedClipCount()
        sourceLanguageCode: dubbing.sourceLanguage
        sourceLanguageName: root.languageDisplayName(dubbing.sourceLanguage)
        targetLanguageCode: dubbing.targetLanguage
        targetLanguageName: root.languageDisplayName(dubbing.targetLanguage)
        onVideoExportRequested: videoExportFileDialog.open()
        onAudioExportRequested: audioExportFileDialog.open()
        onSubtitleExportRequested: function(format, useTargetText, languageCode) {
            root.pendingSubtitleFormat = format
            root.pendingSubtitleUsesTarget = useTargetText
            root.pendingSubtitleLanguageCode = languageCode
            subtitleExportFileDialog.open()
        }
        onPackageExportRequested: packageExportFolderDialog.open()
    }

    FileDialog {
        id: videoExportFileDialog
        title: qsTr("Export dubbed video")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "mp4"
        nameFilters: [qsTr("MP4 video (*.mp4)")]
        onAccepted: dubbing.exportMedia(AppController.files.urlToLocalPath(selectedFile.toString()))
    }

    FileDialog {
        id: audioExportFileDialog
        title: qsTr("Export dubbing mix")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: [qsTr("WAV audio (*.wav)")]
        onAccepted: dubbing.renderPreview(AppController.files.urlToLocalPath(selectedFile.toString()))
    }

    property string pendingSubtitleFormat: "srt"
    property bool pendingSubtitleUsesTarget: true
    property string pendingSubtitleLanguageCode: ""

    FileDialog {
        id: subtitleExportFileDialog
        title: root.pendingSubtitleFormat === "vtt" ? qsTr("Export WebVTT subtitles")
                                                    : qsTr("Export SubRip subtitles")
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.pendingSubtitleFormat
        currentFile: "subtitles-" + (root.pendingSubtitleLanguageCode || "und")
                     + "." + root.pendingSubtitleFormat
        nameFilters: root.pendingSubtitleFormat === "vtt"
                     ? [qsTr("WebVTT subtitles (*.vtt)")]
                     : [qsTr("SubRip subtitles (*.srt)")]
        onAccepted: dubbing.exportSubtitles(
                        AppController.files.urlToLocalPath(selectedFile.toString()),
                        root.pendingSubtitleUsesTarget)
    }

    FolderDialog {
        id: packageExportFolderDialog
        title: qsTr("Choose review package folder")
        onAccepted: dubbing.exportPackage(AppController.files.urlToLocalPath(selectedFolder.toString()))
    }

    ConfirmationDialog {
        id: deleteHistoryDialog
        parent: Overlay.overlay
        titleText: qsTr("Delete project from history")
        messageText: qsTr("The project file will not be deleted; only its history entry will be removed.")
        confirmText: qsTr("Delete")
        isDestructive: true
        onConfirmed: { dubbing.deleteHistoryItem(root.pendingHistoryDeleteId); root.pendingHistoryDeleteId = "" }
        onRejected: root.pendingHistoryDeleteId = ""
    }

    ConfirmationDialog {
        id: clearHistoryDialog
        parent: Overlay.overlay
        titleText: qsTr("Clear dubbing history")
        messageText: qsTr("All saved dubbing project entries will be removed from history.")
        confirmText: qsTr("Clear all")
        isDestructive: true
        onConfirmed: dubbing.clearHistory()
    }

    WorkflowPipelineDialog {
        id: workflowDialog
        nodes: dubbing.workflowNodes; workflowReady: dubbing.workflowReady; statusText: dubbing.workflowStatusText
        busy: dubbing.processing; progress: dubbing.progress / 100.0; dialogTitle: qsTr("Dubbing workflow")
        reviewWaiting: dubbing.workflowWaitingForInput
        description: qsTr("Review the media, transcription, translation, voice, timing, and output stages.")
        onPrepareRequested: dubbing.prepareWorkflow()
        onRunRequested: dubbing.startAutomaticWorkflow(defaultExportPath())
        onApproveRequested: dubbing.approveWorkflowReview()
        onRejectRequested: dubbing.rejectWorkflowReview(qsTr("Rejected from workflow review"))
        nodeConfigurations: dubbing.workflowNodeConfigurations
        onNodeConfigurationChanged: dubbing.setWorkflowNodeModel(nodeId, familyId, runtimeId, runtimeVersion, selectedFiles)
    }

    WorkflowNodeModelDialog {
        id: nodeModelDialog
        nodes: dubbing.workflowNodes
        nodeConfigurations: dubbing.workflowNodeConfigurations
        onConfigurationAccepted: function(nodeId, familyId, runtimeId, runtimeVersion, selectedFiles) {
            dubbing.setWorkflowNodeModel(nodeId, familyId, runtimeId, runtimeVersion, selectedFiles)
        }
    }
}
