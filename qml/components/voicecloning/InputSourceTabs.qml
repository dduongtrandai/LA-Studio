import QtQuick
import LAStudio
import "../shared"

AudioInputSourcePicker {
    id: root

    property string audioPath: ""
    property int recordingSampleRate: 24000
    property string recommendedAudioHint: "24kHz mono WAV recommended"

    signal audioLoaded(string path)

    audioLabel: "Reference audio"
    audioHint: root.recommendedAudioHint
    fileDialogTitle: "Select Reference Audio"
    fileNameFilters: root.recordingSampleRate === 48000 ? ["WAV files (*.wav)"] : ["Audio files (*.wav *.mp3 *.m4a *.flac)"]
    showSystemSource: true
    recording: AppController.recorder.recording
    saving: AppController.recorder.saving
    recordingLevel: AppController.recorder.level

    onAudioSelected: function(path) {
        root.audioPath = path
        root.audioLoaded(path)
    }

    onStartRecordingRequested: function(systemAudio) {
        AppController.recorder.recordSystemAudio = systemAudio
        AppController.recorder.start()
    }

    onStopRecordingRequested: {
        AppController.recorder.stop()
        AppController.recorder.saveLastRecordingToCacheAsync(root.recordingSampleRate)
    }

    Connections {
        target: AppController.recorder

        function onRecordingSaved(path) {
            if (path !== "") {
                root.audioPath = path
                root.audioLoaded(path)
            }
        }
    }
}
