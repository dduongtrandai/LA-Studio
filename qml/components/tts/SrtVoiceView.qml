import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../base"

ColumnLayout {
    id: root

    property var voiceController: AppController.subtitleVoice
    property var settingsPanel: null
    property var family: null
    property bool locked: voiceController ? voiceController.processing : false
    readonly property int activePlaybackIndex: voiceController ? voiceController.activePlaybackIndex : -1

    function formatTime(milliseconds) {
        const totalSeconds = Math.max(0, Math.floor(milliseconds / 1000))
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function fitColor(cue) {
        if (cue.state === "failed_silence" || cue.state === "dropped_overlap")
            return Theme.warning
        if (cue.state === "ready" || cue.state === "generated")
            return Theme.success
        return Theme.textSecondary
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 112
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingLarge
            spacing: Theme.paddingSmall

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: qsTr("SRT to Voice")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
                Text {
                    text: voiceController && voiceController.ttsReady
                          ? qsTr("TTS model ready") : qsTr("Load a TTS model in TTS Studio")
                    color: voiceController && voiceController.ttsReady ? Theme.success : Theme.warning
                    font.pixelSize: Theme.fontSmall
                }
            }

            Text {
                Layout.fillWidth: true
                text: voiceController && voiceController.sourcePath !== ""
                      ? qsTr("Loaded: %1 (%2 cues)")
                            .arg(voiceController.sourcePath.split(/[/\\\\]/).pop())
                            .arg(voiceController.cues.length)
                      : qsTr("Import an SRT file to generate a non-overlapping voice track.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideMiddle
            }

            RowLayout {
                Layout.fillWidth: true

                PrimaryButton {
                    text: qsTr("Import SRT")
                    iconName: "folder"
                    enabled: !root.locked
                    onClicked: importDialog.open()
                }
                PrimaryButton {
                    text: qsTr("Clear")
                    quiet: true
                    enabled: !root.locked && voiceController && voiceController.cues.length > 0
                    onClicked: voiceController.clear()
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: voiceController
                          ? qsTr("%1% · %2").arg(voiceController.progress).arg(voiceController.phase) : ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Timeline review")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: voiceController && voiceController.summary.totalCues !== undefined
                          ? qsTr("Failed %1 · Trimmed %2")
                                .arg(voiceController.summary.failedCues || 0)
                                .arg(voiceController.summary.trimmedCues || 0) : ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }

            ListView {
                id: cueList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: voiceController ? voiceController.cues : []

                delegate: Rectangle {
                    id: cueCard

                    width: cueList.width
                    height: hasAudio ? 126 : 62
                    readonly property bool hasAudio: modelData.audioPath !== undefined
                                                     && modelData.audioPath !== ""
                    readonly property bool ownsPlayback: root.activePlaybackIndex === index
                                                          && AppController.player.playing
                    readonly property real playbackProgress: ownsPlayback
                                                               && AppController.player.playbackDurationMs > 0
                                                             ? AppController.player.playbackPositionMs
                                                               / AppController.player.playbackDurationMs
                                                             : 0

                    color: ownsPlayback
                           ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.09)
                           : (index % 2 === 0 ? Qt.rgba(1, 1, 1, 0.028)
                                              : Qt.rgba(1, 1, 1, 0.018))
                    radius: Theme.radiusSmall
                    border.color: ownsPlayback
                                  ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                                  : Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.paddingSmall
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.paddingSmall

                            Rectangle {
                                Layout.preferredWidth: 30
                                Layout.minimumWidth: 30
                                Layout.maximumWidth: 30
                                Layout.preferredHeight: 30
                                radius: Theme.radiusSmall
                                color: cueCard.ownsPlayback
                                       ? Theme.accent : Qt.rgba(1, 1, 1, 0.055)

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.cueNumber
                                    color: cueCard.ownsPlayback ? "#ffffff" : Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.text
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: qsTr("%1 → %2  ·  voice %3  ·  slot %4")
                                          .arg(root.formatTime(modelData.startMs))
                                          .arg(root.formatTime(modelData.endMs))
                                          .arg(root.formatTime(modelData.outputDurationMs
                                                               || modelData.naturalDurationMs || 0))
                                          .arg(root.formatTime(modelData.slotDurationMs
                                                               || (modelData.endMs - modelData.startMs)))
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: statusText.implicitWidth + Theme.paddingMedium
                                Layout.minimumWidth: 76
                                Layout.maximumWidth: 132
                                Layout.preferredHeight: 24
                                radius: 12
                                color: Qt.rgba(root.fitColor(modelData).r,
                                               root.fitColor(modelData).g,
                                               root.fitColor(modelData).b, 0.12)
                                border.color: Qt.rgba(root.fitColor(modelData).r,
                                                     root.fitColor(modelData).g,
                                                     root.fitColor(modelData).b, 0.24)
                                border.width: 1

                                Text {
                                    id: statusText
                                    anchors.centerIn: parent
                                    width: Math.min(implicitWidth, parent.width - Theme.paddingSmall)
                                    text: modelData.fitStatus || modelData.state || qsTr("pending")
                                    color: root.fitColor(modelData)
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: cueCard.hasAudio
                            spacing: Theme.paddingSmall

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumWidth: 240
                                Layout.minimumHeight: 58
                                radius: Theme.radiusSmall
                                color: Qt.rgba(0, 0, 0, 0.13)
                                border.color: Qt.rgba(1, 1, 1, 0.065)
                                border.width: 1
                                clip: true

                                WaveformView {
                                    anchors.fill: parent
                                    anchors.margins: Theme.paddingSmall
                                    samples: modelData.waveformSamples || []
                                    framed: false
                                    placeholderText: qsTr("Preparing waveform…")
                                    showPlaceholder: !samples || samples.length === 0
                                    barWidth: 2
                                    barGap: 2
                                    verticalScale: 0.78
                                    waveColor: cueCard.ownsPlayback ? Theme.accentLight : Theme.accent
                                    playedWaveColor: Theme.accentLight
                                    playbackProgress: cueCard.playbackProgress
                                    showPlaybackProgress: cueCard.ownsPlayback
                                    seekEnabled: cueCard.hasAudio
                                    onSeekRequested: function(progress) {
                                        const durationMs = cueCard.ownsPlayback
                                                           && AppController.player.playbackDurationMs > 0
                                                         ? AppController.player.playbackDurationMs
                                                         : (modelData.outputDurationMs
                                                            || modelData.naturalDurationMs || 0)
                                        if (!cueCard.ownsPlayback)
                                            voiceController.playCue(index)
                                        voiceController.seekPlayback(
                                            Math.round(progress * durationMs))
                                    }
                                }
                            }

                            RowLayout {
                                Layout.preferredWidth: 112
                                Layout.minimumWidth: 112
                                Layout.maximumWidth: 112
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 4

                                AlignmentPlayerButton {
                                    iconName: cueCard.ownsPlayback && !AppController.player.paused
                                              ? "pause" : "play"
                                    toolTip: cueCard.ownsPlayback && !AppController.player.paused
                                             ? qsTr("Pause segment")
                                             : cueCard.ownsPlayback && AppController.player.paused
                                               ? qsTr("Resume segment") : qsTr("Play segment")
                                    highlighted: cueCard.ownsPlayback
                                    enabled: cueCard.hasAudio
                                    onClicked: {
                                        if (cueCard.ownsPlayback) {
                                            if (AppController.player.paused)
                                                voiceController.resumePlayback()
                                            else
                                                voiceController.pausePlayback()
                                        } else {
                                            voiceController.playCue(index)
                                        }
                                    }
                                }

                                AlignmentPlayerButton {
                                    iconName: "stop"
                                    toolTip: qsTr("Stop segment")
                                    enabled: cueCard.ownsPlayback
                                    onClicked: voiceController.stopPlayback()
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: cueCard.ownsPlayback
                                          ? root.formatTime(AppController.player.playbackPositionMs)
                                          : root.formatTime(modelData.outputDurationMs
                                                            || modelData.naturalDurationMs || 0)
                                    color: cueCard.ownsPlayback ? Theme.accentLight : Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: cueCard.ownsPlayback
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: cueList.count === 0
                    text: qsTr("Import an SRT file to preview its timeline.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }

            AudioPreviewPlayer {
                Layout.fillWidth: true
                previewReady: root.voiceController && root.voiceController.outputPath !== ""
                samples: root.voiceController
                         ? (root.voiceController.summary.waveformSamples || []) : []
                durationText: root.voiceController
                              ? root.formatTime(root.voiceController.summary.durationMs || 0) : "--"
                sampleRate: root.voiceController
                            ? (root.voiceController.summary.sampleRate || 0) : 0
                statusText: root.voiceController
                                 ? qsTr("%1 samples · complete subtitle track")
                                       .arg(root.voiceController.summary.sampleCount || 0) : ""
                audioDurationMs: root.voiceController
                                 ? (root.voiceController.summary.durationMs || 0) : 0
                family: root.family
                showSaveAction: true
                isPlaying: root.activePlaybackIndex === -2 && AppController.player.playing
                isPaused: root.activePlaybackIndex === -2 && AppController.player.paused
                playbackPositionMs: root.activePlaybackIndex === -2
                                    ? AppController.player.playbackPositionMs : 0
                playbackDurationMs: root.activePlaybackIndex === -2
                                    ? AppController.player.playbackDurationMs : 0
                onPlayClicked: root.voiceController.playOutput()
                onPauseClicked: root.voiceController.pausePlayback()
                onResumeClicked: root.voiceController.resumePlayback()
                onStopClicked: root.voiceController.stopPlayback()
                onSeekRequested: function(positionMs) {
                    if (root.activePlaybackIndex !== -2)
                        root.voiceController.playOutput()
                    root.voiceController.seekPlayback(positionMs)
                }
                onSaveClicked: saveDialog.open()
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: voiceController && voiceController.error !== "" ? voiceController.error : ""
                    color: Theme.danger
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
                PrimaryButton {
                    text: voiceController && voiceController.processing
                          ? qsTr("Cancel") : qsTr("Generate Voice")
                    iconName: voiceController && voiceController.processing ? "close" : "spark"
                    enabled: voiceController
                             && (voiceController.processing
                                 || (voiceController.cues.length > 0 && voiceController.ttsReady))
                    onClicked: voiceController && voiceController.processing
                               ? voiceController.cancel()
                               : voiceController.generate(root.settingsPanel
                                                          ? root.settingsPanel.getSynthesisSettings()
                                                          : ({}))
                }
            }
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Select SRT File")
        nameFilters: [qsTr("SubRip subtitles (*.srt)"), qsTr("All files (*)")]
        onAccepted: voiceController.importSrt(selectedFile.toString())
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save Voice WAV")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("WAV files (*.wav)")]
        onAccepted: voiceController.saveOutput(selectedFile.toString())
    }
}
