import QtQuick
import QtQuick.Layouts
import LAStudio

// Display box for audio output with waveform, play, and save buttons
ColumnLayout {
    id: root

    property var samples: []
    property bool isPlaying: false
    property bool isProcessing: false

    signal playClicked()
    signal stopClicked()
    signal saveClicked()

    spacing: Theme.paddingMedium

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 100
        color: Theme.surface
        radius: Theme.radiusMedium
        clip: true

        WaveformView {
            id: outputWaveform
            anchors.fill: parent
            samples: root.samples
            placeholderText: "Generated Audio Waveform"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.paddingMedium

        PrimaryButton {
            text: root.isPlaying ? "Stop" : "Play Generated"
            buttonColor: root.isPlaying ? Theme.danger : Theme.success
            enabled: !root.isProcessing && root.samples.length > 0
            onClicked: {
                if (root.isPlaying) {
                    root.stopClicked()
                } else {
                    root.playClicked()
                }
            }
        }

        PrimaryButton {
            text: "Save WAV"
            buttonColor: Theme.surfaceAlt
            enabled: !root.isProcessing && root.samples.length > 0
            onClicked: root.saveClicked()
        }

        Item { Layout.fillWidth: true }
    }
}
