import QtQuick
import LAStudio

// Compatibility facade. New code should use AudioPreviewPlayer directly.
AudioPreviewPlayer {
    id: root

    property bool outputReady: false
    property string sampleCountText: ""
    property int generationProgress: 0
    property string progressLabel: qsTr("Generating audio")

    previewReady: outputReady
    title: qsTr("Generated Audio")
    statusText: sampleCountText
    processingProgress: generationProgress
    processingLabel: progressLabel
    showSaveAction: true
}
