import QtQuick
import QtQuick.Layouts
import QtMultimedia
import LAStudio
import "../base"
import "../shared"

Rectangle {
    id: root

    Layout.fillWidth: true
    Layout.preferredHeight: root.sttSession && root.sttSession.inputPath !== "" ? 350 : 285
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    property var sttSession: null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingMedium

        Text {
            text: qsTr("Audio Input")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.sttSession && root.sttSession.inputPath !== "" ? 230 : 160
            color: Qt.rgba(1, 1, 1, 0.02)
            radius: Theme.radiusSmall
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            AudioInputSourcePicker {
                id: sourcePicker
                anchors.fill: parent
                anchors.margins: Theme.paddingLarge
                visible: !root.sttSession || root.sttSession.inputPath === ""
                audioLabel: qsTr("Audio file input")
                audioHint: qsTr("WAV, MP3, FLAC supported")
                fileDialogTitle: qsTr("Select Audio File")
                showSystemSource: true
                busy: root.sttSession ? root.sttSession.processing : false
                recording: root.sttSession ? root.sttSession.recording : false
                recordingLevel: root.sttSession ? root.sttSession.recordingLevel : 0.0

                onAudioSelected: function(path) {
                    audioPlayer.stop()
                    if (root.sttSession) root.sttSession.selectFileInput(path)
                }

                onStartRecordingRequested: function(systemAudio) {
                    audioPlayer.stop()
                    if (root.sttSession) root.sttSession.startRecording(systemAudio)
                }

                onStopRecordingRequested: {
                    if (root.sttSession) root.sttSession.stopRecording()
                }
            }

            AudioPreviewPlayer {
                id: inputPreview
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                visible: root.sttSession ? root.sttSession.inputPath !== "" : false
                previewReady: root.sttSession ? root.sttSession.inputPath !== "" : false
                title: qsTr("Loaded: %1").arg(root.sttSession ? root.sttSession.inputPath.split(/[/\\]/).pop() : "")
                samples: root.sttSession ? root.sttSession.waveformSamples : []
                durationText: formatTime(audioPlayer.duration)
                statusText: root.sttSession && root.sttSession.inputLoading ? qsTr("Decoding file...") : qsTr("Audio ready")
                isPlaying: audioPlayer.playbackState === MediaPlayer.PlayingState
                isPaused: audioPlayer.playbackState === MediaPlayer.PausedState
                playbackPositionMs: audioPlayer.position
                playbackDurationMs: audioPlayer.duration
                audioDurationMs: audioPlayer.duration
                processing: root.sttSession ? root.sttSession.processing : false
                processingProgress: root.sttSession ? root.sttSession.progress : 0
                processingLabel: qsTr("Transcribing audio")
                showReplaceAction: true
                replaceActionText: qsTr("Change / Record")
                onReplaceClicked: {
                    audioPlayer.stop()
                    sourcePicker.activeTab = "file"
                    if (root.sttSession) root.sttSession.clearInput()
                }
                onPlayClicked: audioPlayer.play()
                onResumeClicked: audioPlayer.play()
                onPauseClicked: audioPlayer.pause()
                onStopClicked: audioPlayer.stop()
                onSeekRequested: function(positionMs) {
                    audioPlayer.position = positionMs
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingLarge

            Text {
                text: {
                    if (!root.sttSession) return qsTr("No file selected")
                    if (root.sttSession.recording) return qsTr("Recording audio...")
                    if (root.sttSession.processing) return qsTr("Processing... %1%").arg(root.sttSession.progress)
                    if (root.sttSession.inputLoading) return qsTr("Decoding file...")
                    if (root.sttSession.inputError !== "") return root.sttSession.inputError
                    return root.sttSession.inputPath !== "" ? qsTr("Ready to transcribe") : qsTr("Choose a file or capture audio")
                }
                color: (root.sttSession && root.sttSession.inputError !== "") ? Theme.danger : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.fillWidth: true
            }

            PrimaryButton {
                text: (root.sttSession && root.sttSession.processing) ? qsTr("Processing...") : qsTr("Transcribe File")
                enabled: root.sttSession ? (root.sttSession.inputPath !== "" && !root.sttSession.processing && !root.sttSession.inputLoading && root.sttSession.inputError === "") : false
                buttonColor: Theme.accent
                onClicked: if (root.sttSession) root.sttSession.transcribeInput()
            }

            PrimaryButton {
                text: qsTr("Stop")
                iconName: "x"
                visible: root.sttSession ? root.sttSession.processing : false
                onClicked: if (root.sttSession) root.sttSession.cancelProcessing()
            }
        }
    }

    MediaPlayer {
        id: audioPlayer
        source: root.sttSession ? root.sttSession.inputUrl : ""
        audioOutput: AudioOutput {}
    }
}
