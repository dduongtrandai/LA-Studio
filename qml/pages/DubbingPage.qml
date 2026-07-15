import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia
import "../components/base"
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
    readonly property var languageCatalog: AppController.catalog.languageSet("default")

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
        function onPlayingChanged() { if (!AppController.player.playing) root.playingSeparationStem = "" }
    }

    MediaPlayer {
        id: videoPlayer
        source: dubbing.sourceMediaUrl
        audioOutput: AudioOutput {}
        videoOutput: videoOutput
    }

    Connections {
        target: videoPlayer
        function onPositionChanged() {
            if (videoPlayer.playbackState !== MediaPlayer.PlayingState) return
            var pos = videoPlayer.position
            for (var i = 0; i < dubbing.segments.length; i++) {
                var seg = dubbing.segments[i]
                if (pos >= seg.startMs && pos <= seg.endMs) {
                    if (root.selectedSegment !== i) {
                        root.selectedSegment = i
                    }
                    break
                }
            }
        }
    }

    function formatTime(ms) {
        if (isNaN(ms) || ms < 0) return "00:00";
        var totalSec = Math.floor(ms / 1000);
        var hr = Math.floor(totalSec / 3600);
        var min = Math.floor((totalSec - (hr * 3600)) / 60);
        var sec = totalSec - (hr * 3600) - (min * 60);

        var minStr = min < 10 ? "0" + min : min.toString();
        var secStr = sec < 10 ? "0" + sec : sec.toString();

        if (hr > 0) {
            var hrStr = hr < 10 ? "0" + hr : hr.toString();
            return hrStr + ":" + minStr + ":" + secStr;
        }
        return minStr + ":" + secStr;
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

    component Step: Item {
        property string stepId: ""
        property string title: ""
        property string iconName: "check"
        property bool complete: false
        property bool active: false
        implicitWidth: stepRow.implicitWidth
        implicitHeight: 32
        signal selected(string stepId)

        RowLayout {
            id: stepRow
            anchors.fill: parent
            spacing: 6
            Rectangle {
                Layout.preferredWidth: 25; Layout.preferredHeight: 25; radius: 13
                color: active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20) : "transparent"
                border.color: complete || active ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.16)
                border.width: 1
                LineIcon { anchors.centerIn: parent; width: 13; height: 13; name: complete ? "check" : iconName; color: complete || active ? Theme.accentLight : Theme.textSecondary }
            }
            Text { text: title; color: active ? Theme.textPrimary : Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: active || complete }
        }
        TapHandler { onTapped: parent.selected(parent.stepId) }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }

    Rectangle { anchors.fill: parent; color: Theme.background }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // The workflow header makes the current stage visible without taking focus from the edit surface.
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 58
            color: Qt.rgba(0, 0, 0, 0.10)
            border.color: Qt.rgba(1, 1, 1, 0.06); border.width: 1
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: Theme.paddingLarge; anchors.rightMargin: Theme.paddingLarge
                spacing: Theme.paddingMedium
                RowLayout {
                    Layout.preferredWidth: 205; spacing: Theme.paddingSmall
                    LineIcon { name: "waves"; color: Theme.accentLight; Layout.preferredWidth: 21; Layout.preferredHeight: 21 }
                    ColumnLayout { spacing: 0
                        Text { text: qsTr("Dubbing Studio"); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                        Text { text: dubbing.hasProject ? qsTr("Project workspace") : qsTr("New project"); color: Theme.textSecondary; font.pixelSize: 10 }
                    }
                }
                Step { stepId: "import"; title: qsTr("Import"); iconName: "folder"; complete: root.stepComplete(stepId); active: dubbing.currentStepId === stepId; onSelected: function(id) { root.reviewStepId = id } }
                Rectangle { Layout.preferredWidth: 16; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.14) }
                Step { stepId: "ingest"; title: qsTr("Normalize"); iconName: "activity"; complete: root.stepComplete(stepId); active: dubbing.currentStepId === stepId; onSelected: function(id) { root.reviewStepId = id } }
                Rectangle { Layout.preferredWidth: 16; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.14) }
                Step { stepId: "source-separate"; title: qsTr("Separate"); iconName: "waves"; complete: root.stepComplete(stepId); active: dubbing.currentStepId === stepId; onSelected: function(id) { root.reviewStepId = id } }
                Rectangle { Layout.preferredWidth: 16; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.14) }
                Step { stepId: "transcribe"; title: qsTr("Transcribe"); iconName: "mic"; complete: root.stepComplete(stepId); active: dubbing.currentStepId === stepId; onSelected: function(id) { root.reviewStepId = id } }
                Rectangle { Layout.preferredWidth: 16; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.14) }
                Step { stepId: "translate"; title: qsTr("Translate"); iconName: "alignment"; complete: root.stepComplete(stepId); active: dubbing.currentStepId === stepId; onSelected: function(id) { root.reviewStepId = id } }
                Rectangle { Layout.preferredWidth: 16; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.14) }
                Step { stepId: "synthesize"; title: qsTr("Voice"); iconName: "volume"; complete: root.stepComplete(stepId); active: dubbing.currentStepId === stepId; onSelected: function(id) { root.reviewStepId = id } }
                Rectangle { Layout.preferredWidth: 16; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.14) }
                Step { stepId: "export"; title: qsTr("Output"); iconName: "download"; complete: root.stepComplete(stepId); active: dubbing.currentStepId === "mix" || dubbing.currentStepId === stepId || dubbing.currentStepId === "completed"; onSelected: function(id) { root.reviewStepId = id } }
                Item { Layout.fillWidth: true }
                PrimaryButton {
                    text: qsTr("Workflow")
                    iconName: "alignment"
                    quiet: true
                    onClicked: root.openWorkflowCanvas()
                    AppToolTip {
                        text: qsTr("View and configure workflow")
                        visible: parent.hovered
                    }
                }
                Rectangle { implicitWidth: statusRow.implicitWidth + 16; implicitHeight: 28; radius: 14; color: Qt.rgba(dubbing.processing ? Theme.warning.r : Theme.success.r, dubbing.processing ? Theme.warning.g : Theme.success.g, dubbing.processing ? Theme.warning.b : Theme.success.b, 0.12)
                    RowLayout { id: statusRow; anchors.centerIn: parent; spacing: 5
                        Rectangle { width: 6; height: 6; radius: 3; color: dubbing.processing ? Theme.warning : Theme.success }
                        Text { text: dubbing.processing ? qsTr("%1 · %2%").arg(root.stepTitle(dubbing.currentStepId)).arg(dubbing.progress) : (dubbing.workflowMode === "step" ? qsTr("Waiting for next step") : qsTr("Ready")); color: dubbing.processing ? Theme.warning : Theme.success; font.pixelSize: Theme.fontSmall; font.bold: true }
                    }
                }
                PrimaryButton { text: qsTr("Save"); iconName: "save"; quiet: true; enabled: dubbing.hasProject; onClicked: dubbing.saveProject() }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.margins: Theme.paddingMedium; spacing: Theme.paddingMedium

            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 500; spacing: Theme.paddingMedium

                Panel {
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 300
                    ColumnLayout { anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingSmall
                        RowLayout { Layout.fillWidth: true
                            Text { text: qsTr("SOURCE MEDIA"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1; Layout.fillWidth: true }
                            Text { text: dubbing.sourceMediaPath.length > 0 ? qsTr("Loaded") : qsTr("No media"); color: dubbing.sourceMediaPath.length > 0 ? Theme.success : Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true; radius: Theme.radiusSmall; color: Qt.rgba(0, 0, 0, 0.30)
                            border.color: Qt.rgba(1, 1, 1, 0.06); border.width: 1
                            clip: true

                            // Empty placeholder state
                            Column {
                                anchors.centerIn: parent; width: parent.width - Theme.paddingXL * 2; spacing: Theme.paddingSmall
                                visible: dubbing.sourceMediaPath.length === 0
                                LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "folder"; color: Theme.accentLight; width: 38; height: 38 }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Add source media"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; elide: Text.ElideMiddle; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("WAV, MP3, MP4 or MKV"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                            }

                            // Video Output Renderer
                            VideoOutput {
                                id: videoOutput
                                anchors.fill: parent
                                visible: isVideoSource
                                fillMode: VideoOutput.PreserveAspectFit
                            }

                            // Audio track placeholder
                            Rectangle {
                                anchors.fill: parent
                                color: Qt.rgba(0.06, 0.06, 0.09, 0.95)
                                visible: dubbing.sourceMediaPath.length > 0 && !isVideoSource

                                Column {
                                    anchors.centerIn: parent; width: parent.width - Theme.paddingXL * 2; spacing: Theme.paddingSmall
                                    LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "volume"; color: Theme.accentLight; width: 42; height: 42 }
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: dubbing.sourceMediaPath.split(/[\\/]/).pop(); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; elide: Text.ElideMiddle; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Audio track playing"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                                }
                            }

                            // Mouse Area to load file on empty state click
                            MouseArea {
                                anchors.fill: parent
                                enabled: dubbing.sourceMediaPath.length === 0
                                cursorShape: Qt.PointingHandCursor
                                onClicked: mediaFileDialog.open()
                            }

                            // Controls and Hover Area when media is loaded
                            MouseArea {
                                id: videoHoverArea
                                anchors.fill: parent
                                hoverEnabled: true
                                visible: dubbing.sourceMediaPath.length > 0
                                preventStealing: true
                                onPositionChanged: controlsTimer.restart()
                            }

                            Timer {
                                id: controlsTimer
                                interval: 2500
                                repeat: false
                                running: videoPlayer.playbackState === MediaPlayer.PlayingState
                            }

                            Rectangle {
                                id: controlsOverlay
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 44
                                visible: opacity > 0.0
                                opacity: (videoHoverArea.containsMouse || seekBarMouseArea.isDraggingSeekBar || !controlsTimer.running || videoPlayer.playbackState !== MediaPlayer.PlayingState) ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 250 } }

                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "transparent" }
                                    GradientStop { position: 1.0; color: Qt.rgba(0.06, 0.06, 0.09, 0.92) }
                                }

                                // Full-width seek timeline
                                Item {
                                    id: seekBar
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.topMargin: -8
                                    height: 16
                                    z: 10

                                    Rectangle {
                                        id: seekBarTrack
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: seekBarMouseArea.containsMouse || seekBarMouseArea.pressed ? 5 : 3
                                        color: Qt.rgba(255, 255, 255, 0.2)
                                        Behavior on height { NumberAnimation { duration: 100 } }

                                        Rectangle {
                                            id: seekBarProgress
                                            anchors.left: parent.left
                                            height: parent.height
                                            color: Theme.accentLight
                                            width: {
                                                if (videoPlayer.duration <= 0) return 0;
                                                var pos = seekBarMouseArea.isDraggingSeekBar ? seekBarMouseArea.dragPositionMs : videoPlayer.position;
                                                return (pos / videoPlayer.duration) * parent.width;
                                            }
                                        }

                                        Rectangle {
                                            id: seekBarHandle
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: {
                                                if (videoPlayer.duration <= 0) return -width/2;
                                                var pos = seekBarMouseArea.isDraggingSeekBar ? seekBarMouseArea.dragPositionMs : videoPlayer.position;
                                                return (pos / videoPlayer.duration) * seekBarTrack.width - width/2;
                                            }
                                            width: seekBarMouseArea.containsMouse || seekBarMouseArea.pressed ? 12 : 0
                                            height: width
                                            radius: width / 2
                                            color: Theme.textPrimary
                                            Behavior on width { NumberAnimation { duration: 100 } }
                                        }
                                    }

                                    MouseArea {
                                        id: seekBarMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true

                                        property bool isDraggingSeekBar: false
                                        property real dragPositionMs: 0.0
                                        property bool wasPlayingBeforeDrag: false

                                        function updatePosition(mouseX) {
                                            if (videoPlayer.duration > 0) {
                                                var ratio = Math.max(0.0, Math.min(1.0, mouseX / width));
                                                dragPositionMs = ratio * videoPlayer.duration;
                                                videoPlayer.position = dragPositionMs;
                                            }
                                        }

                                        onPressed: {
                                            isDraggingSeekBar = true;
                                            wasPlayingBeforeDrag = (videoPlayer.playbackState === MediaPlayer.PlayingState);
                                            if (wasPlayingBeforeDrag) {
                                                videoPlayer.pause();
                                            }
                                            controlsTimer.restart();
                                            updatePosition(mouseX);
                                        }
                                        onPositionChanged: {
                                            if (pressed) {
                                                controlsTimer.restart();
                                                updatePosition(mouseX);
                                            }
                                        }
                                        onReleased: {
                                            isDraggingSeekBar = false;
                                            if (wasPlayingBeforeDrag) {
                                                videoPlayer.play();
                                            }
                                        }
                                    }
                                }

                                RowLayout {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 36
                                    anchors.leftMargin: Theme.paddingMedium
                                    anchors.rightMargin: Theme.paddingMedium
                                    spacing: Theme.paddingMedium

                                    // Play/Pause borderless circular button
                                    Rectangle {
                                        id: playPauseBtn
                                        width: 28; height: 28; radius: 14
                                        color: playPauseMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                                        LineIcon {
                                            anchors.centerIn: parent
                                            name: videoPlayer.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
                                            color: Theme.textPrimary
                                            width: 14; height: 14
                                        }
                                        MouseArea {
                                            id: playPauseMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (videoPlayer.playbackState === MediaPlayer.PlayingState) {
                                                    videoPlayer.pause()
                                                } else {
                                                    videoPlayer.play()
                                                }
                                            }
                                        }
                                    }

                                    // Mute / Volume borderless button with cross overlay
                                    Rectangle {
                                        id: muteBtn
                                        width: 28; height: 28; radius: 14
                                        color: muteMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                                        LineIcon {
                                            anchors.centerIn: parent
                                            name: "volume"
                                            color: videoPlayer.audioOutput.muted ? Theme.textSecondary : Theme.textPrimary
                                            width: 14; height: 14
                                        }
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 16
                                            height: 1.5
                                            color: Theme.danger
                                            rotation: -45
                                            visible: videoPlayer.audioOutput.muted
                                        }
                                        MouseArea {
                                            id: muteMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                videoPlayer.audioOutput.muted = !videoPlayer.audioOutput.muted
                                            }
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: {
                                            var pos = seekBarMouseArea.isDraggingSeekBar ? seekBarMouseArea.dragPositionMs : videoPlayer.position;
                                            return "%1 / %2".arg(formatTime(pos)).arg(formatTime(videoPlayer.duration));
                                        }
                                        color: Theme.textSecondary
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                    }
                                }
                            }
                        }
                        RowLayout { Layout.fillWidth: true; spacing: Theme.paddingSmall
                            Field { id: mediaPath; Layout.fillWidth: true; text: dubbing.sourceMediaPath; placeholderText: qsTr("Media file path") }
                            PrimaryButton { text: qsTr("Browse"); iconName: "folder"; quiet: true; onClicked: mediaFileDialog.open() }
                            PrimaryButton { text: qsTr("Import"); enabled: mediaPath.text.trim().length > 0 && !dubbing.processing; onClicked: dubbing.importMedia(mediaPath.text) }
                        }
                    }
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
                    RowLayout { Layout.fillWidth: true
                        ColumnLayout { Layout.fillWidth: true; spacing: 1
                            Text { text: root.stepTitle(root.reviewStepId).toUpperCase(); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                            Text { text: qsTr("Review and edit every segment before continuing."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                        }
                        PrimaryButton { text: qsTr("Add segment"); iconName: "more-horizontal"; quiet: true; enabled: dubbing.hasProject && !dubbing.processing; onClicked: dubbing.addSegment(0, 2000, "") }
                        PrimaryButton { visible: root.canRerunStep(root.reviewStepId); text: qsTr("Run again"); iconName: "refresh"; quiet: true; enabled: !dubbing.processing; Layout.preferredWidth: 104; onClicked: dubbing.rerunStep(root.reviewStepId, root.defaultExportPath()) }
                        PrimaryButton { text: qsTr("Automatic A–Z"); iconName: "activity"; enabled: !dubbing.processing && dubbing.workflowReady; onClicked: dubbing.startAutomaticWorkflow(root.defaultExportPath()) }
                        PrimaryButton { text: qsTr("Step-by-step"); iconName: "chevron-right"; quiet: true; enabled: !dubbing.processing && dubbing.sourceMediaPath.length > 0; onClicked: { dubbing.startStepByStep(); root.reviewStepId = dubbing.currentStepId } }
                        PrimaryButton { visible: dubbing.workflowMode === "step"; text: qsTr("Run: %1").arg(root.stepTitle(dubbing.currentStepId)); iconName: "play"; enabled: !dubbing.processing && dubbing.currentStepId !== "completed"; onClicked: dubbing.runCurrentStep(root.defaultExportPath()) }
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
                                    if (videoPlayer.seekable && modelData.startMs !== undefined) {
                                        videoPlayer.position = modelData.startMs
                                    }
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
                    RowLayout { Layout.fillWidth: true
                        ColumnLayout { Layout.fillWidth: true; spacing: 1
                            Text { text: root.stepTitle(root.reviewStepId).toUpperCase(); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                            Text { text: root.reviewStepId === "import" ? qsTr("Import only selects the source; no processing starts automatically.") : qsTr("Review this step output before continuing."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                        }
                        PrimaryButton { visible: root.canRerunStep(root.reviewStepId); text: qsTr("Run again"); iconName: "refresh"; quiet: true; enabled: !dubbing.processing; Layout.preferredWidth: 104; onClicked: dubbing.rerunStep(root.reviewStepId, root.defaultExportPath()) }
                        PrimaryButton { text: qsTr("Automatic A-Z"); iconName: "activity"; enabled: !dubbing.processing && dubbing.workflowReady; onClicked: dubbing.startAutomaticWorkflow(root.defaultExportPath()) }
                        PrimaryButton { text: qsTr("Step-by-step"); iconName: "chevron-right"; quiet: true; enabled: !dubbing.processing && dubbing.sourceMediaPath.length > 0; onClicked: { dubbing.startStepByStep(); root.reviewStepId = dubbing.currentStepId } }
                        PrimaryButton { visible: dubbing.workflowMode === "step"; text: qsTr("Run: %1").arg(root.stepTitle(dubbing.currentStepId)); iconName: "play"; enabled: !dubbing.processing && dubbing.currentStepId !== "completed"; onClicked: dubbing.runCurrentStep(root.defaultExportPath()) }
                    }
                    Item { Layout.fillHeight: true }
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
                                root.playingSeparationStem = kind
                                AppController.player.playFile(path)
                            }
                        }
                    }
                    ColumnLayout {
                        visible: root.reviewStepId !== "source-separate"
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
                    Item { Layout.fillHeight: true }
                }
            }
        }

        Panel {
            Layout.fillWidth: true; Layout.preferredHeight: 136; Layout.leftMargin: Theme.paddingMedium; Layout.rightMargin: Theme.paddingMedium; Layout.bottomMargin: Theme.paddingMedium
            RowLayout { anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingMedium
                ColumnLayout { Layout.preferredWidth: 270; Layout.fillHeight: true; spacing: 4
                    Text { text: qsTr("LANGUAGE & VOICE"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1 }
                    RowLayout { Layout.fillWidth: true
                        AppComboBox {
                            id: sourceLanguageCombo
                            Layout.fillWidth: true
                            model: root.languageCatalog
                            textRole: "text"
                            secondaryTextRole: "detail"
                            searchable: model.length > 6
                            currentIndex: {
                                for (var i = 0; i < model.length; ++i)
                                    if (model[i].value === dubbing.sourceLanguage) return i
                                return 0
                            }
                            onActivated: function(index) {
                                if (index >= 0 && index < model.length) dubbing.sourceLanguage = model[index].value
                            }
                        }
                        LineIcon { name: "chevron-right"; color: Theme.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                        AppComboBox {
                            id: targetLanguageCombo
                            Layout.fillWidth: true
                            model: root.languageCatalog
                            textRole: "text"
                            secondaryTextRole: "detail"
                            searchable: model.length > 6
                            currentIndex: {
                                for (var i = 0; i < model.length; ++i)
                                    if (model[i].value === dubbing.targetLanguage) return i
                                return 0
                            }
                            onActivated: function(index) {
                                if (index >= 0 && index < model.length) dubbing.targetLanguage = model[index].value
                            }
                        }
                    }
                    PrimaryButton { text: qsTr("Add speaker"); iconName: "users"; quiet: true; onClicked: dubbing.addSpeaker() }
                }
                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Qt.rgba(1, 1, 1, 0.08) }
                ColumnLayout { Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4
                    RowLayout { Layout.fillWidth: true
                        Text { text: qsTr("SPEAKERS"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1; Layout.fillWidth: true }
                        Text { text: dubbing.speakers.length; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    }
                    Flow { Layout.fillWidth: true; spacing: Theme.paddingSmall
                        Repeater { model: dubbing.speakers; delegate: Rectangle { width: speakerLabel.implicitWidth + 22; height: 29; radius: 14; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12); border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28); border.width: 1
                            Text { id: speakerLabel; anchors.centerIn: parent; text: modelData.name || qsTr("Speaker %1").arg(index + 1); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall }
                        } }
                        Text { visible: dubbing.speakers.length === 0; text: qsTr("No speakers assigned"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    }
                    Text { visible: dubbing.lastError.length > 0; text: dubbing.lastError; color: Theme.danger; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight; Layout.fillWidth: true }
                }
                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Qt.rgba(1, 1, 1, 0.08) }
                ColumnLayout { Layout.preferredWidth: 340; Layout.fillHeight: true; spacing: Theme.paddingSmall
                    Text { text: qsTr("OUTPUT"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1 }
                    Text { text: dubbing.workflowMode === "automatic" ? qsTr("Automatic A-Z") : (dubbing.workflowMode === "step" ? qsTr("Step-by-step") : qsTr("Choose a processing mode")); color: dubbing.workflowMode === "idle" ? Theme.textSecondary : Theme.accentLight; font.pixelSize: Theme.fontSmall; font.bold: true }
                    Text { Layout.fillWidth: true; text: qsTr("Current: %1").arg(root.stepTitle(dubbing.currentStepId)); color: dubbing.processing ? Theme.warning : Theme.textSecondary; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: dubbing.exportPath.length > 0 ? dubbing.exportPath : (dubbing.previewPath.length > 0 ? dubbing.previewPath : qsTr("Final output has not been created.")); color: dubbing.exportPath.length > 0 ? Theme.success : Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideMiddle }
                    PrimaryButton { text: qsTr("Cancel processing"); visible: dubbing.processing; buttonColor: Theme.danger; onClicked: dubbing.cancelProcessing() }
                }
            }
        }
    }

    FileDialog {
        id: mediaFileDialog
        title: qsTr("Select media file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Media files (*.wav *.mp3 *.mp4 *.mkv *.mov *.webm)"), qsTr("All files (*)")]
        onAccepted: {
            var path = AppController.files.urlToLocalPath(selectedFile.toString())
            mediaPath.text = path
            dubbing.importMedia(path)
        }
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
}
