import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import LAStudio
import "../base"
import ".."

// Shared media input surface for Studio workflows.
// AudioInputSourcePicker remains the compatibility implementation for audio-only callers.
ColumnLayout {
    id: root

    property alias activeTab: picker.activeTab
    property alias mediaLabel: picker.audioLabel
    property alias mediaHint: picker.audioHint
    property alias fileDialogTitle: picker.fileDialogTitle
    property alias fileNameFilters: picker.fileNameFilters
    property alias showMicrophone: picker.showMicrophone
    property alias showSystemSource: picker.showSystemSource
    property alias busy: picker.busy
    property alias recording: picker.recording
    property alias saving: picker.saving
    property alias recordingLevel: picker.recordingLevel
    property string selectedPath: ""
    property string previewError: ""
    readonly property var waveformSamples: (AppController.preview.wavSamplesSourcePath === root.selectedPath) ? AppController.preview.wavSamples : []
    readonly property bool waveformLoading: AppController.preview.wavSamplesLoading
                                           && (AppController.preview.wavSamplesSourcePath === root.selectedPath
                                               || root.waveformSamples.length === 0)
    readonly property bool isPlaying: previewPlayer.playbackState === MediaPlayer.PlayingState
    readonly property bool isPaused: previewPlayer.playbackState === MediaPlayer.PausedState
    readonly property real playbackProgress: previewPlayer.duration > 0 ? previewPlayer.position / previewPlayer.duration : 0

    function localMediaUrl(path) {
        if (!path) return ""
        // Encode local paths before handing them to MediaPlayer. In particular,
        // '#' must not be interpreted as a URL fragment (common in filenames).
        var normalized = path.replace(/\\/g, "/")
        var encoded = encodeURI(normalized).replace(/#/g, "%23")
        return Qt.platform.os === "windows"
                ? "file:///" + encoded
                : "file://" + encoded
    }

    signal mediaSelected(string path)
    signal startRecordingRequested(bool systemAudio)
    signal stopRecordingRequested()

    AudioInputSourcePicker {
        id: picker
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.selectedPath.length === 0

        onAudioSelected: function(path) {
            root.selectedPath = path
            root.previewError = ""
            previewPlayer.stop()
            AppController.preview.requestWavSamples(path)
            root.mediaSelected(path)
        }
        onStartRecordingRequested: function(systemAudio) { root.startRecordingRequested(systemAudio) }
        onStopRecordingRequested: root.stopRecordingRequested()
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.selectedPath.length > 0
        radius: Theme.radiusSmall
        color: Qt.rgba(1, 1, 1, 0.025)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                LineIcon {
                    name: "file"
                    color: Theme.accent
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }

                Text {
                    Layout.fillWidth: true
                    text: root.selectedPath.split(/[\\/]/).pop()
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    elide: Text.ElideMiddle
                }

                PrimaryButton {
                    text: qsTr("Replace")
                    iconName: "folder"
                    quiet: true
                    textColor: Theme.textPrimary
                    enabled: !root.busy
                    onClicked: {
                        root.selectedPath = ""
                        root.previewError = ""
                        previewPlayer.stop()
                        AppController.preview.requestWavSamples("")
                    }
                }
            }

            WaveformView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 72
                samples: root.waveformSamples
                framed: true
                placeholderText: root.waveformLoading ? qsTr("Loading waveform...") : qsTr("Waveform preview unavailable")
                showPlaceholder: true
                playbackProgress: root.playbackProgress
                showPlaybackProgress: root.isPlaying
                seekEnabled: root.waveformSamples.length > 0 && previewPlayer.duration > 0
                onSeekRequested: function(progress) {
                    if (root.isPlaying)
                        previewPlayer.position = Math.round(progress * previewPlayer.duration)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                Text {
                    Layout.fillWidth: true
                    text: root.waveformLoading ? qsTr("Reading audio...")
                                                 : root.waveformSamples.length > 0 ? qsTr("Audio ready")
                                                                                   : root.previewError.length > 0 ? root.previewError : qsTr("Preview unavailable for this media")
                    color: root.previewError.length > 0 ? Theme.danger : root.waveformSamples.length > 0 ? Theme.success : Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }

                PrimaryButton {
                    text: root.isPaused ? qsTr("Resume") : qsTr("Play")
                    iconName: root.isPaused ? "play" : "play"
                    enabled: root.selectedPath.length > 0 && (!root.isPlaying || root.isPaused)
                    onClicked: {
                        previewPlayer.play()
                    }
                }

                PrimaryButton {
                    text: qsTr("Pause")
                    iconName: "pause"
                    quiet: true
                    enabled: root.isPlaying && !root.isPaused
                    onClicked: previewPlayer.pause()
                }

                PrimaryButton {
                    text: qsTr("Stop")
                    iconName: "stop"
                    quiet: true
                    enabled: root.isPlaying
                    onClicked: previewPlayer.stop()
                }
            }
        }
    }

    MediaPlayer {
        id: previewPlayer
        source: root.localMediaUrl(root.selectedPath)
        audioOutput: AudioOutput {}
        onErrorOccurred: function(error, errorString) {
            root.previewError = errorString || qsTr("Could not play this media")
        }
    }
}
