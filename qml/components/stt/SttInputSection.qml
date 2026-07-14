import QtQuick
import QtQuick.Layouts
import QtMultimedia
import LAStudio
import "../base"
import "../shared"

Rectangle {
    id: root

    Layout.fillWidth: true
    Layout.preferredHeight: 285
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
            Layout.preferredHeight: 160
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

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                visible: root.sttSession ? root.sttSession.inputPath !== "" : false
                spacing: Theme.paddingMedium

                Text {
                    text: qsTr("Loaded: %1").arg(root.sttSession ? root.sttSession.inputPath.split(/[/\\]/).pop() : "")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                WaveformView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    samples: root.sttSession ? root.sttSession.waveformSamples : []
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Theme.paddingMedium

                    PrimaryButton {
                        text: qsTr("Change / Record")
                        quiet: true
                        implicitHeight: 32
                        enabled: root.sttSession ? !root.sttSession.processing : false
                        onClicked: {
                            audioPlayer.stop()
                            sourcePicker.activeTab = "file"
                            if (root.sttSession) root.sttSession.clearInput()
                        }
                    }

                    PrimaryButton {
                        text: audioPlayer.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
                        buttonColor: audioPlayer.playbackState === MediaPlayer.PlayingState ? Theme.danger : Theme.success
                        implicitHeight: 32
                        onClicked: {
                            if (audioPlayer.playbackState === MediaPlayer.PlayingState) audioPlayer.pause()
                            else audioPlayer.play()
                        }
                    }
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
