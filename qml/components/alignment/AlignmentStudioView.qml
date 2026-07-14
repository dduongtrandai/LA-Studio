import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LAStudio
import "../shared"
import "../base"

WorkflowStudioShell {
    id: root

    readonly property string defaultAlignmentFamilyId: "mms-forced-aligner-onnx"
    readonly property string defaultSttFamilyId: "nemotron-3.5-asr-streaming-0.6b"

    family: null
    families: []
    capability: "forced-alignment"
    studioReady: studioController ? studioController.studioReady : root.configurationReady
    isSettingsOpen: true
    showLeftPanel: false
    modalSelectionMode: true
    showSwitcher: false
    studioTitle: studioController ? studioController.studioHeaderTitle : qsTr("Alignment Studio")
    studioIconName: "alignment"
    modalSelectionTitle: qsTr("Model + Runtime")
    modalSelectionValue: root.selectedModelName
    modalSelectionDetail: root.selectedModelDetail
    backToolTip: qsTr("Change model and runtime")

    property string audioPath: ""
    property string modelId: ""
    property string runtimeId: ""
    property string runtimeVersion: ""
    property var selectedFiles: ({})
    property bool configurationReady: false
    property bool executionBackendReady: false
    property string selectedSttFamilyId: defaultSttFamilyId
    property bool defaultSetupActive: false
    property bool alignmentDefaultCommitted: false
    property bool sttDefaultCommitted: false
    property var pendingAlignmentRequest: null
    readonly property bool processing: AppController.alignment.processing
    property string selectedModelName: qsTr("No alignment model selected")
    property string selectedModelDetail: qsTr("Choose a forced-alignment model and compatible runtime.")
    property string configurationStatus: qsTr("Setup required")
    property int resultViewMode: 0 // 0: karaoke, 1: table
    property bool ownsReviewPlayback: false
    readonly property bool canonicalMode: modeInput.currentIndex === 0
    readonly property bool inputReady: audioPath !== "" && (!canonicalMode || transcriptInput.text.trim().length > 0)
                                       && settingsPanel.anchorModelAvailable
    readonly property bool sessionReady: studioController ? studioController.canProcess : configurationReady
    readonly property bool canAlign: executionBackendReady && sessionReady && inputReady
                                     && !processing && pendingAlignmentRequest === null
    workflowTitle: qsTr("Alignment workflow")
    workflowReady: AppController.alignment.workflowReady && executionBackendReady && sessionReady
    workflowBusy: defaultSetupActive || (studioController ? studioController.statusText === "Loading" : false)
    workflowProgress: defaultSetupProgress()
    workflowStatusText: workflowReady ? qsTr("Workflow ready") : qsTr("Setup required")
    workflowActionText: workflowReady ? qsTr("View workflow") : qsTr("Set up workflow")
    readonly property string inputStatusText: audioPath === ""
                                              ? qsTr("Audio required")
                                              : (canonicalMode && transcriptInput.text.trim().length === 0
                                                 ? qsTr("Transcript required")
                                                 : (!settingsPanel.anchorModelAvailable
                                                    ? qsTr("STT anchor model required")
                                                    : qsTr("Inputs ready")))
    readonly property color inputStatusColor: inputReady ? Theme.success : Theme.warning
    readonly property int reviewPositionMs: ownsReviewPlayback ? AppController.player.playbackPositionMs : 0
    readonly property int reviewDurationMs: AppController.alignment.duration > 0
                                            ? Math.round(AppController.alignment.duration * 1000)
                                            : (ownsReviewPlayback ? AppController.player.playbackDurationMs : 0)
    readonly property int activeSegmentIndex: AppController.alignment.segmentIndexAt(reviewPositionMs / 1000.0)
    readonly property int activeKaraokeLineIndex: AppController.alignment.karaokeLineIndexAt(reviewPositionMs / 1000.0)

    onActiveSegmentIndexChanged: {
        if (activeSegmentIndex < 0) return
        if (resultViewMode === 0 && karaokeList && activeKaraokeLineIndex >= 0) {
            karaokeList.positionViewAtIndex(activeKaraokeLineIndex, ListView.Center)
        } else if (resultViewMode === 1 && segmentTable) {
            segmentTable.positionViewAtIndex(activeSegmentIndex, ListView.Contain)
        }
    }

    signal configureRequested()

    StudioPageController {
        id: sttWorkflowController
        capabilityId: "stt"
    }

    function familyItem(controller, familyId) {
        return controller && controller.familiesModel && controller.familiesModel.itemForFamily
                ? controller.familiesModel.itemForFamily(familyId) : null
    }

    function defaultSetupProgress() {
        if (workflowReady) return 1
        var downloads = AppController.downloads.activeDownloads
        if (downloads && downloads.length > 0) {
            var received = 0
            var total = 0
            for (var i = 0; i < downloads.length; ++i) {
                received += downloads[i].bytesReceived || 0
                total += downloads[i].bytesTotal || 0
            }
            return total > 0 ? received / total : 0.08
        }
        if (studioController && studioController.statusText === "Loading") return 0.85
        return defaultSetupActive ? 0.15 : 0
    }

    function commitDefaultModelsIfReady() {
        if (!defaultSetupActive) return
        var alignmentFamilyId = studioController.selectedFamilyId !== "" ? studioController.selectedFamilyId : defaultAlignmentFamilyId
        var sttFamilyId = selectedSttFamilyId !== "" ? selectedSttFamilyId : defaultSttFamilyId
        var alignmentItem = familyItem(studioController, alignmentFamilyId)
        var sttItem = familyItem(sttWorkflowController, sttFamilyId)

        if (alignmentItem && alignmentItem.ready && !alignmentDefaultCommitted) {
            alignmentDefaultCommitted = true
            if (studioController.selectionCommitted && studioController.selectedFamilyId === alignmentItem.familyId)
                studioController.loadSelectedConfiguration()
            else
                studioController.commitConfigurationSelection(
                            alignmentItem.familyId,
                            alignmentItem.preferredRuntimeId || "",
                            alignmentItem.preferredRuntimeVersion || "",
                            alignmentItem.selectedFiles || ({}))
        }
        if (sttItem && sttItem.ready && !sttDefaultCommitted) {
            sttDefaultCommitted = true
            selectedSttFamilyId = sttFamilyId
            sttWorkflowController.saveConfigurationSelection(
                        sttItem.familyId,
                        sttItem.preferredRuntimeId || "",
                        sttItem.preferredRuntimeVersion || "",
                        sttItem.selectedFiles || ({}))
        }
        if (alignmentDefaultCommitted && sttDefaultCommitted) {
            defaultSetupActive = false
            AppController.alignment.prepareWorkflow(workflowRequest())
        }
    }

    function loadDefaultWorkflow() {
        defaultSetupActive = true
        alignmentDefaultCommitted = false
        sttDefaultCommitted = false

        var alignmentFamilyId = studioController.selectedFamilyId !== "" ? studioController.selectedFamilyId : defaultAlignmentFamilyId
        var sttFamilyId = selectedSttFamilyId !== "" ? selectedSttFamilyId : defaultSttFamilyId
        if (studioController.selectedFamilyId === "")
            studioController.selectFamily(alignmentFamilyId)
        if (sttWorkflowController.selectedFamilyId === "")
            sttWorkflowController.selectFamily(sttFamilyId)
        var alignmentItem = familyItem(studioController, alignmentFamilyId)
        var sttItem = familyItem(sttWorkflowController, sttFamilyId)
        if (alignmentItem && !alignmentItem.ready)
            AppController.downloadInstall.enqueueRecommendedSetup(alignmentItem)
        if (sttItem && !sttItem.ready)
            AppController.downloadInstall.enqueueRecommendedSetup(sttItem)
        commitDefaultModelsIfReady()
    }

    function openNodeModel(nodeId) {
        if (nodeId === "stt") {
            sttWorkflowController.openConfiguration(selectedSttFamilyId)
            sttConfigurationDialog.open()
        } else if (nodeId === "aligner") {
            workflowDialog.close()
            root.configureRequested()
        }
    }

    onRequestBack: root.configureRequested()
    onRequestConfigurationPicker: root.configureRequested()
    onRequestWorkflow: root.showWorkflow()
    onRequestReload: {
        if (studioController) studioController.reload()
    }
    onRequestEject: {
        if (studioController) studioController.unload()
    }

    function fileName(path) {
        if (!path) return ""
        var normalized = path.replace(/\\/g, "/")
        return normalized.substring(normalized.lastIndexOf("/") + 1)
    }

    function selectAudio(path) {
        if (!path) return
        stopReviewPlayback()
        var nextPath = AppController.files.urlToLocalPath(path)
        if (audioPath !== nextPath)
            AppController.alignment.clearResult()
        audioPath = nextPath
    }

    function runAlignment() {
        var request = workflowRequest()
        if (!AppController.alignment.prepareWorkflow(request)) {
            workflowDialog.open()
            return
        }
        // The Alignment pipeline owns its temporary STT session. A model left
        // resident by STT Studio would create a second Nemotron GPU session and
        // can make CrispASR fail to open it. Release the shared STT session first.
        if (!canonicalMode && sttWorkflowController.modelActive) {
            pendingAlignmentRequest = request
            sttWorkflowController.unload()
            return
        }
        AppController.alignment.runStudioAlignment(request)
    }

    function workflowRequest() {
        var stt = settingsPanel.selectedAnchorModel()
        return {
            mode: canonicalMode ? "canonical" : "automatic",
            runtimeId: runtimeId,
            runtimeVersion: runtimeVersion,
            sttRuntimeId: sttWorkflowController.runtimeId,
            sttRuntimeVersion: sttWorkflowController.runtimeVersion,
            modelId: modelId,
            audioPath: audioPath,
            transcript: transcriptInput.text,
            language: settingsPanel.languageCode,
            sttModel: stt,
            selectedFiles: selectedFiles,
            vadOptions: settingsPanel.vadOptions(),
            timestampUnit: settingsPanel.timestampUnit,
            outputFormat: settingsPanel.outputFormat
        }
    }

    function showWorkflow() {
        AppController.alignment.prepareWorkflow(workflowRequest())
        workflowDialog.open()
    }

    function confidenceColor(confidence) {
        if (confidence >= 0.75) return Theme.success
        if (confidence >= 0.45) return Theme.warning
        return Theme.danger
    }

    function formatReviewTime(milliseconds) {
        var total = Math.max(0, milliseconds) / 1000.0
        var minutes = Math.floor(total / 60)
        var seconds = total - minutes * 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds.toFixed(1)
    }

    function playReviewAt(milliseconds) {
        if (audioPath === "") return
        ownsReviewPlayback = true
        AppController.player.playFile(audioPath)
        if (AppController.player.playing && milliseconds > 0)
            AppController.player.seek(milliseconds)
    }

    function seekReview(milliseconds) {
        if (ownsReviewPlayback && AppController.player.playing)
            AppController.player.seek(milliseconds)
        else
            playReviewAt(milliseconds)
    }

    function stopReviewPlayback() {
        if (ownsReviewPlayback && AppController.player.playing)
            AppController.player.stop()
        ownsReviewPlayback = false
    }

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (!AppController.player.playing)
                root.ownsReviewPlayback = false
        }
    }

    Connections {
        target: AppController.alignment
        function onResultChanged() {
            root.stopReviewPlayback()
            if (AppController.alignment.segments.length > 0)
                root.resultViewMode = 0
        }
    }

    Connections {
        target: AppController.downloadInstall
        function onInstallStatesChanged() {
            if (!root.defaultSetupActive) return
            root.studioController.familiesModel.refresh()
            sttWorkflowController.familiesModel.refresh()
            root.commitDefaultModelsIfReady()
        }
    }

    Connections {
        target: sttWorkflowController
        function onStateChanged() {
            if (root.pendingAlignmentRequest === null) return
            if (sttWorkflowController.statusText === "Error") {
                root.pendingAlignmentRequest = null
                return
            }
            if (sttWorkflowController.statusText !== "Unloaded") return
            var request = root.pendingAlignmentRequest
            root.pendingAlignmentRequest = null
            AppController.alignment.runStudioAlignment(request)
        }
    }

    Connections {
        target: studioController ? studioController.familiesModel : null
        function onRevisionChanged() { root.commitDefaultModelsIfReady() }
    }

    Connections {
        target: sttWorkflowController.familiesModel
        function onRevisionChanged() { root.commitDefaultModelsIfReady() }
    }

    Component.onCompleted: {
        if (studioController && studioController.selectedFamilyId === "")
            studioController.selectFamily(defaultAlignmentFamilyId)
        if (sttWorkflowController.selectedFamilyId === "")
            sttWorkflowController.selectFamily(defaultSttFamilyId)
    }

    Component.onDestruction: stopReviewPlayback()

    mainContent: [
        ScrollView {
            id: contentScroll
            anchors.fill: parent
            clip: true
            contentWidth: availableWidth

            RowLayout {
                width: contentScroll.availableWidth
                height: Math.max(contentScroll.availableHeight, implicitHeight)
                spacing: Theme.paddingLarge

                Item { Layout.preferredWidth: Theme.paddingXL - Theme.paddingLarge }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 320
                    Layout.maximumWidth: 620
                    Layout.topMargin: Theme.paddingLarge
                    Layout.bottomMargin: Theme.paddingXL
                    spacing: Theme.paddingMedium

                    AlignmentSectionHeader {
                        title: qsTr("Source")
                        detail: qsTr("Add source material, then review processing options.")
                    }

                    AlignmentStatusStrip {
                        Layout.fillWidth: true
                        label: root.inputStatusText
                        detail: root.audioPath === ""
                                ? qsTr("Choose an audio file to begin")
                                : (root.canonicalMode && transcriptInput.text.trim().length === 0
                                   ? qsTr("Paste or import the spoken transcript")
                                   : qsTr("Ready for local processing"))
                        accent: root.inputStatusColor
                        iconName: root.inputReady ? "check" : "activity"
                    }

                    AlignmentOptionField {
                        label: qsTr("Workflow")
                        Layout.fillWidth: true
                        AppComboBox {
                            id: modeInput
                            Layout.fillWidth: true
                            model: [qsTr("Use existing transcript"), qsTr("Generate from audio")]
                            onCurrentIndexChanged: AppController.alignment.clearResult()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 92
                        radius: Theme.radiusSmall
                        color: audioDrop.containsDrag ? Qt.rgba(0.49, 0.30, 1.0, 0.10) : Theme.surface
                        border.color: audioDrop.containsDrag ? Theme.accent : Theme.surfaceAlt
                        border.width: 1

                        DropArea {
                            id: audioDrop
                            anchors.fill: parent
                            keys: ["text/uri-list"]
                            onDropped: function(drop) {
                                if (drop.hasUrls && drop.urls.length > 0)
                                    root.selectAudio(drop.urls[0].toString())
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingLarge
                            spacing: Theme.paddingMedium

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: 21
                                color: Qt.rgba(0.49, 0.30, 1.0, 0.12)
                                border.color: Qt.rgba(0.49, 0.30, 1.0, 0.28)
                                border.width: 1

                                LineIcon {
                                    anchors.centerIn: parent
                                    name: root.audioPath === "" ? "file" : "waves"
                                    color: Theme.accentLight
                                    width: Theme.iconSize
                                    height: Theme.iconSize
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: root.audioPath === "" ? qsTr("Drop an audio file or browse") : root.fileName(root.audioPath)
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMedium
                                    font.bold: true
                                    elide: Text.ElideMiddle
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.audioPath === "" ? qsTr("WAV, MP3, FLAC") : root.audioPath
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideMiddle
                                }
                            }

                            PrimaryButton {
                                text: qsTr("Browse")
                                iconName: "folder"
                                quiet: true
                                implicitWidth: 104
                                implicitHeight: 34
                                onClicked: audioFileDialog.open()
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 150
                        visible: root.canonicalMode
                        spacing: Theme.paddingSmall

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: qsTr("Transcript")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                flat: true
                                implicitHeight: 28
                                text: qsTr("Import text")
                                onClicked: transcriptFileDialog.open()
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        ScrollView {
                            id: transcriptScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 112
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            AppTextArea {
                                id: transcriptInput
                                width: transcriptScroll.availableWidth
                                height: Math.max(transcriptScroll.availableHeight, implicitHeight)
                                placeholderText: qsTr("Paste the transcript spoken in the audio...")
                                selectByMouse: true
                                onTextChanged: {
                                    if (!root.processing && AppController.alignment.segments.length > 0)
                                        AppController.alignment.clearResult()
                                }
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("%1 characters").arg(transcriptInput.text.length)
                            color: Theme.textSecondary
                            font.pixelSize: 10
                        }
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        text: root.processing ? qsTr("Cancel")
                                              : (root.canonicalMode ? qsTr("Align transcript")
                                                                    : qsTr("Generate timestamps"))
                        iconName: root.processing ? "stop" : "alignment"
                        enabled: root.processing || root.canAlign
                        onClicked: {
                            if (root.processing) {
                                AppController.alignment.cancel()
                                return
                            }
                            root.runAlignment()
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    Layout.topMargin: Theme.paddingLarge
                    Layout.bottomMargin: Theme.paddingXL
                    color: Theme.surfaceAlt
                    opacity: 0.6
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 300
                    Layout.topMargin: Theme.paddingLarge
                    Layout.bottomMargin: Theme.paddingXL
                    spacing: Theme.paddingMedium

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingMedium

                        AlignmentSectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Aligned output")
                            detail: AppController.alignment.segments.length > 0
                                    ? qsTr("%1 segments aligned in %2").arg(AppController.alignment.segments.length)
                                                                    .arg(root.formatReviewTime(root.reviewDurationMs))
                                    : qsTr("Word and character timestamps appear here.")
                        }

                        Rectangle {
                            Layout.preferredWidth: 218
                            Layout.preferredHeight: 34
                            radius: Theme.radiusSmall
                            color: Qt.rgba(0, 0, 0, 0.16)
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 2
                                spacing: 2

                                AlignmentViewModeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Karaoke")
                                    iconName: "waves"
                                    checked: root.resultViewMode === 0
                                    onClicked: root.resultViewMode = 0
                                }

                                AlignmentViewModeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Table")
                                    iconName: "sliders"
                                    checked: root.resultViewMode === 1
                                    onClicked: root.resultViewMode = 1
                                }
                            }
                        }

                        PrimaryButton {
                            text: qsTr("Export")
                            iconName: "save"
                            quiet: true
                            enabled: AppController.alignment.output !== ""
                            implicitWidth: 96
                            implicitHeight: 34
                            onClicked: outputFileDialog.open()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        spacing: Theme.paddingXL
                        visible: AppController.alignment.segments.length > 0

                        AlignmentResultMetric {
                            label: qsTr("SEGMENTS")
                            value: String(AppController.alignment.segments.length)
                        }

                        AlignmentResultMetric {
                            label: qsTr("DURATION")
                            value: root.formatReviewTime(root.reviewDurationMs)
                        }

                        AlignmentResultMetric {
                            label: qsTr("AVG. CONFIDENCE")
                            value: Math.round(AppController.alignment.averageConfidence * 100) + "%"
                            valueColor: root.confidenceColor(AppController.alignment.averageConfidence)
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 420
                        radius: Theme.radiusSmall
                        color: Theme.surface
                        border.color: Theme.surfaceAlt
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 70
                                color: Theme.surfaceAlt
                                visible: AppController.alignment.segments.length > 0

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.paddingMedium
                                    anchors.rightMargin: Theme.paddingMedium
                                    spacing: Theme.paddingMedium

                                    AlignmentPlayerButton {
                                        iconName: root.ownsReviewPlayback && AppController.player.playing && !AppController.player.paused
                                                  ? "pause" : "play"
                                        toolTip: root.ownsReviewPlayback && AppController.player.paused
                                                 ? qsTr("Resume review")
                                                 : (root.ownsReviewPlayback && AppController.player.playing
                                                    ? qsTr("Pause review") : qsTr("Play alignment review"))
                                        highlighted: true
                                        onClicked: {
                                            if (root.ownsReviewPlayback && AppController.player.playing) {
                                                if (AppController.player.paused)
                                                    AppController.player.resume()
                                                else
                                                    AppController.player.pause()
                                            } else {
                                                root.playReviewAt(0)
                                            }
                                        }
                                    }

                                    AlignmentPlayerButton {
                                        iconName: "stop"
                                        toolTip: qsTr("Stop review")
                                        enabled: root.ownsReviewPlayback && AppController.player.playing
                                        onClicked: root.stopReviewPlayback()
                                    }

                                    Text {
                                        Layout.preferredWidth: 84
                                        text: root.formatReviewTime(root.reviewPositionMs) + " / "
                                              + root.formatReviewTime(root.reviewDurationMs)
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    Slider {
                                        id: reviewSlider
                                        Layout.fillWidth: true
                                        from: 0
                                        to: Math.max(1, root.reviewDurationMs)
                                        value: root.reviewPositionMs
                                        enabled: root.audioPath !== "" && AppController.alignment.segments.length > 0
                                        onMoved: root.seekReview(Math.round(value))

                                        background: Rectangle {
                                            x: reviewSlider.leftPadding
                                            y: reviewSlider.topPadding + reviewSlider.availableHeight / 2 - height / 2
                                            width: reviewSlider.availableWidth
                                            height: 4
                                            radius: 2
                                            color: Qt.rgba(1, 1, 1, 0.12)

                                            Rectangle {
                                                width: parent.width * reviewSlider.visualPosition
                                                height: parent.height
                                                radius: parent.radius
                                                color: Theme.accentLight
                                            }
                                        }

                                        handle: Rectangle {
                                            x: reviewSlider.leftPadding + reviewSlider.visualPosition
                                               * (reviewSlider.availableWidth - width)
                                            y: reviewSlider.topPadding + reviewSlider.availableHeight / 2 - height / 2
                                            implicitWidth: 14
                                            implicitHeight: 14
                                            radius: 7
                                            color: reviewSlider.pressed ? Theme.accentLight : Theme.textPrimary
                                            border.color: Theme.accent
                                            border.width: 2
                                        }
                                    }
                                }
                            }

                            StackLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                currentIndex: root.resultViewMode
                                visible: AppController.alignment.segments.length > 0

                                Rectangle {
                                    color: Theme.surface

                                    ListView {
                                        id: karaokeList
                                        anchors.fill: parent
                                        anchors.topMargin: Theme.paddingMedium
                                        anchors.bottomMargin: Theme.paddingMedium
                                        clip: true
                                        spacing: Theme.paddingSmall
                                        model: AppController.alignment.karaokeLines
                                        ScrollBar.vertical: ScrollBar { }

                                        delegate: Rectangle {
                                            id: karaokeLine
                                            required property var modelData
                                            required property int index
                                            readonly property bool active: index === root.activeKaraokeLineIndex
                                            readonly property bool elapsed: root.reviewPositionMs > Number(modelData.end || 0) * 1000
                                            width: ListView.view.width
                                            height: Math.max(66, lineFlow.childrenRect.height + Theme.paddingLarge * 2)
                                            color: active ? Qt.rgba(0.49, 0.30, 1.0, 0.10)
                                                          : (lineMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.035) : "transparent")

                                            Rectangle {
                                                width: 3
                                                height: parent.height - Theme.paddingMedium
                                                anchors.left: parent.left
                                                anchors.verticalCenter: parent.verticalCenter
                                                color: Theme.accentLight
                                                visible: karaokeLine.active
                                            }

                                            Text {
                                                anchors.left: parent.left
                                                anchors.leftMargin: Theme.paddingLarge
                                                anchors.top: parent.top
                                                anchors.topMargin: Theme.paddingSmall
                                                text: root.formatReviewTime(Number(karaokeLine.modelData.start || 0) * 1000)
                                                color: karaokeLine.active ? Theme.accentLight : Theme.textSecondary
                                                font.pixelSize: 10
                                                font.bold: karaokeLine.active
                                            }

                                            Flow {
                                                id: lineFlow
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.leftMargin: 72
                                                anchors.rightMargin: Theme.paddingLarge
                                                anchors.topMargin: Theme.paddingSmall
                                                spacing: 4

                                                Repeater {
                                                    model: karaokeLine.modelData.words || []
                                                    delegate: Rectangle {
                                                        id: karaokeWord
                                                        required property var modelData
                                                        readonly property bool active: Number(modelData.segmentIndex) === root.activeSegmentIndex
                                                        readonly property bool elapsed: root.reviewPositionMs > Number(modelData.end || 0) * 1000
                                                        width: wordText.implicitWidth + 12
                                                        height: 30
                                                        radius: 5
                                                        color: active ? Theme.accent : "transparent"
                                                        border.color: active ? Theme.accentLight : "transparent"
                                                        border.width: 1

                                                        Text {
                                                            id: wordText
                                                            anchors.centerIn: parent
                                                            text: karaokeWord.modelData.text || ""
                                                            color: karaokeWord.active ? Theme.textPrimary
                                                                                       : (karaokeWord.elapsed ? Theme.textSecondary : Theme.textPrimary)
                                                            opacity: karaokeWord.elapsed && !karaokeWord.active ? 0.58 : 1
                                                            font.pixelSize: Theme.fontMedium
                                                            font.bold: karaokeWord.active
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: root.playReviewAt(Math.round(Number(karaokeWord.modelData.start || 0) * 1000))
                                                        }
                                                    }
                                                }
                                            }

                                            MouseArea {
                                                id: lineMouse
                                                anchors.fill: parent
                                                acceptedButtons: Qt.NoButton
                                                hoverEnabled: true
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    color: Theme.surface

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 38
                                            color: Qt.rgba(1, 1, 1, 0.035)

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: Theme.paddingMedium
                                                anchors.rightMargin: Theme.paddingMedium
                                                spacing: Theme.paddingMedium

                                                TableHeader { text: qsTr("START"); Layout.preferredWidth: 62 }
                                                TableHeader { text: qsTr("END"); Layout.preferredWidth: 62 }
                                                TableHeader { text: qsTr("LENGTH"); Layout.preferredWidth: 62 }
                                                TableHeader { text: qsTr("TEXT"); Layout.fillWidth: true }
                                                TableHeader { text: qsTr("CONFIDENCE"); Layout.preferredWidth: 88 }
                                            }
                                        }

                                        ListView {
                                            id: segmentTable
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true
                                            model: AppController.alignment.segments
                                            ScrollBar.vertical: ScrollBar { }

                                            delegate: Rectangle {
                                                id: segmentRow
                                                required property var modelData
                                                required property int index
                                                readonly property bool active: index === root.activeSegmentIndex
                                                readonly property real confidence: Number(modelData.confidence || 0)
                                                width: ListView.view.width
                                                height: 42
                                                color: active ? Qt.rgba(0.49, 0.30, 1.0, 0.16)
                                                              : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.055)
                                                                                       : (index % 2 === 0 ? Theme.surface
                                                                                                         : Qt.rgba(1, 1, 1, 0.025)))

                                                Rectangle {
                                                    width: 3
                                                    height: parent.height
                                                    color: Theme.accentLight
                                                    visible: parent.active
                                                }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: Theme.paddingMedium
                                                    anchors.rightMargin: Theme.paddingMedium
                                                    spacing: Theme.paddingMedium

                                                    TableCell { Layout.preferredWidth: 62; text: Number(modelData.start || 0).toFixed(3) }
                                                    TableCell { Layout.preferredWidth: 62; text: Number(modelData.end || 0).toFixed(3) }
                                                    TableCell {
                                                        Layout.preferredWidth: 62
                                                        text: (Number(modelData.end || 0) - Number(modelData.start || 0)).toFixed(3)
                                                    }
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.text || ""
                                                        color: Theme.textPrimary
                                                        font.pixelSize: Theme.fontSmall
                                                        font.bold: segmentRow.active
                                                        elide: Text.ElideRight
                                                    }
                                                    RowLayout {
                                                        Layout.preferredWidth: 88
                                                        spacing: 6

                                                        Rectangle {
                                                            Layout.preferredWidth: 42
                                                            Layout.preferredHeight: 4
                                                            radius: 2
                                                            color: Qt.rgba(1, 1, 1, 0.10)

                                                            Rectangle {
                                                                width: parent.width * Math.max(0, Math.min(1, segmentRow.confidence))
                                                                height: parent.height
                                                                radius: parent.radius
                                                                color: root.confidenceColor(segmentRow.confidence)
                                                            }
                                                        }
                                                        Text {
                                                            text: Math.round(segmentRow.confidence * 100) + "%"
                                                            color: root.confidenceColor(segmentRow.confidence)
                                                            font.pixelSize: 10
                                                            font.bold: true
                                                        }
                                                    }
                                                }

                                                MouseArea {
                                                    id: rowMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.playReviewAt(Math.round(Number(parent.modelData.start || 0) * 1000))
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                visible: AppController.alignment.segments.length === 0

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    width: Math.min(parent.width - Theme.paddingXL * 2, 280)
                                    spacing: Theme.paddingMedium

                                    LineIcon {
                                        Layout.alignment: Qt.AlignHCenter
                                        name: "waves"
                                        color: Theme.textSecondary
                                        opacity: 0.65
                                        Layout.preferredWidth: 34
                                        Layout.preferredHeight: 34
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: AppController.alignment.errorMessage !== ""
                                              && root.inputReady ? qsTr("Alignment failed")
                                              : (root.processing ? AppController.alignment.statusText
                                                                 : (root.executionBackendReady && !root.sessionReady
                                                                    ? qsTr("Alignment model unloaded")
                                                                    : (root.executionBackendReady ? qsTr("Ready for alignment") : qsTr("Alignment backend unavailable"))))
                                        color: AppController.alignment.errorMessage !== "" && root.inputReady ? Theme.danger : Theme.textPrimary
                                        font.pixelSize: Theme.fontMedium
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: AppController.alignment.errorMessage !== "" && root.inputReady
                                              ? AppController.alignment.errorMessage
                                                  : (root.executionBackendReady && !root.sessionReady
                                                  ? qsTr("Load the alignment model before running alignment.")
                                                  : (root.executionBackendReady
                                                  ? (root.audioPath === ""
                                                     ? qsTr("Choose an audio file to begin.")
                                                     : (root.canonicalMode && transcriptInput.text.trim().length === 0
                                                        ? qsTr("Paste or import the transcript spoken in the audio.")
                                                        : qsTr("Complete the required inputs to run alignment.")))
                                                  : qsTr("Configure and install an alignment model and process runtime.")))
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        wrapMode: Text.Wrap
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredWidth: Theme.paddingXL - Theme.paddingLarge }
            }
        }
    ]

    settingsContent: [
        AlignmentSetupPanel {
            id: settingsPanel
            anchors.fill: parent
            preferredAnchorModelId: root.selectedSttFamilyId
            onCloseRequested: root.isSettingsOpen = false
        }
    ]

    WorkflowPipelineDialog {
        id: workflowDialog
        nodes: AppController.alignment.workflowNodes
        workflowReady: AppController.alignment.workflowReady
        statusText: AppController.alignment.workflowStatusText
        busy: root.workflowBusy
        progress: root.workflowProgress
        dialogTitle: qsTr("Alignment workflow")
        description: qsTr("Execution stages, data flow, and the runtime provider used by each model.")
        modelName: root.selectedModelName
        runtimeName: root.runtimeId !== "" ? root.runtimeId + (root.runtimeVersion !== "" ? "  " + root.runtimeVersion : "") : qsTr("Not selected")
        configurableNodeIds: ["stt", "aligner"]
        actionText: root.workflowReady ? qsTr("Reload workflow") : qsTr("Load workflow")
        actionIconName: "alignment"
        onPrepareRequested: root.loadDefaultWorkflow()
        onNodeConfigureRequested: function(nodeId) { root.openNodeModel(nodeId) }
    }


    Dialog {
        id: sttConfigurationDialog
        parent: Overlay.overlay
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(1260, Math.max(980, parent.width - 48))
        height: Math.min(780, Math.max(560, parent.height - 48))
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        background: Rectangle {
            color: Qt.rgba(0.06, 0.06, 0.09, 0.96)
            radius: Theme.radiusMedium
            border.color: Qt.rgba(1, 1, 1, 0.10)
        }
        contentItem: CapabilityGallery {
            capability: "stt"
            modalMode: true
            familiesModel: sttWorkflowController.familiesModel
            selectedFamilyId: root.selectedSttFamilyId
            onFamilySelected: function(familyId) {
                root.selectedSttFamilyId = familyId
                sttWorkflowController.selectFamily(familyId)
            }
            onConfigurationAccepted: function(familyId, runtimeId, runtimeVersion, selectedFiles) {
                root.selectedSttFamilyId = familyId
                sttWorkflowController.saveConfigurationSelection(familyId, runtimeId, runtimeVersion, selectedFiles)
                sttConfigurationDialog.close()
                AppController.alignment.prepareWorkflow(root.workflowRequest())
                workflowDialog.open()
            }
        }
    }

    FileDialog {
        id: audioFileDialog
        title: qsTr("Select audio file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Audio files (*.wav *.mp3 *.flac)"), qsTr("All files (*)")]
        onAccepted: root.selectAudio(selectedFile.toString())
    }

    FileDialog {
        id: transcriptFileDialog
        title: qsTr("Import transcript")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]
        onAccepted: transcriptInput.text = AppController.files.readTextFile(selectedFile.toString())
    }

    FileDialog {
        id: outputFileDialog
        title: qsTr("Export alignment")
        fileMode: FileDialog.SaveFile
        defaultSuffix: settingsPanel.outputFormat === "srt" ? "srt" : (settingsPanel.outputFormat === "webvtt" ? "vtt" : "json")
        nameFilters: settingsPanel.outputFormat === "srt"
                     ? [qsTr("SubRip subtitles (*.srt)")]
                     : (settingsPanel.outputFormat === "webvtt"
                        ? [qsTr("WebVTT subtitles (*.vtt)")]
                        : [qsTr("JSON files (*.json)")])
        onAccepted: AppController.files.writeTextFile(selectedFile.toString(), AppController.alignment.output)
    }

    component TableHeader: Text {
        color: Theme.textSecondary
        font.pixelSize: 10
        font.bold: true
        verticalAlignment: Text.AlignVCenter
    }

    component TableCell: Text {
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        verticalAlignment: Text.AlignVCenter
    }

}
