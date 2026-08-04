import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

// Reusable waveform player for both generated audio and imported/recorded media.
Rectangle {
    id: root

    Layout.fillWidth: true
    Layout.preferredHeight: active ? 214 : 0
    visible: active
    opacity: active ? 1 : 0
    radius: Theme.radiusMedium
    color: Qt.rgba(1, 1, 1, 0.035)
    border.color: isPlaying ? Qt.rgba(outputAccent.r, outputAccent.g, outputAccent.b, 0.34) : Qt.rgba(1, 1, 1, 0.09)
    border.width: 1
    clip: true

    property bool previewReady: false
    property var samples: []
    property string title: qsTr("Audio Preview")
    property string durationText: "--"
    property int sampleRate: 0
    property string statusText: ""
    property bool isPlaying: false
    property bool isPaused: false
    property int playbackPositionMs: 0
    property int playbackDurationMs: 0
    property int audioDurationMs: 0
    property var family: null
    property color accent: family ? family.accent : Theme.accent
    property bool processing: false
    property int processingProgress: 0
    property bool progressEstimated: true
    property string processingLabel: qsTr("Processing audio")
    property bool showSaveAction: false
    property string saveActionText: qsTr("Save")
    property bool showReplaceAction: false
    property string replaceActionText: qsTr("Replace")
    readonly property bool active: previewReady || processing
    readonly property real normalizedProgress: Math.max(0, Math.min(100, processingProgress)) / 100.0
    readonly property real playbackProgress: playbackDurationMs > 0
                                          ? Math.max(0, Math.min(1, playbackPositionMs / playbackDurationMs)) : 0
    readonly property int seekDurationMs: playbackDurationMs > 0 ? playbackDurationMs : audioDurationMs
    readonly property color outputAccent: accent

    signal playClicked()
    signal pauseClicked()
    signal resumeClicked()
    signal stopClicked()
    signal seekRequested(int positionMs)
    signal saveClicked()
    signal replaceClicked()

    function formatTime(milliseconds) {
        var seconds = Math.max(0, Math.floor(milliseconds / 1000))
        var minutes = Math.floor(seconds / 60)
        return minutes + ":" + (seconds % 60 < 10 ? "0" : "") + (seconds % 60)
    }

    Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutQuad } }
    Behavior on border.color { ColorAnimation { duration: 140 } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            spacing: Theme.paddingSmall

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: Theme.radiusSmall
                color: Qt.rgba(root.outputAccent.r, root.outputAccent.g, root.outputAccent.b, 0.16)
                LineIcon { anchors.centerIn: parent; name: "volume"; color: root.outputAccent; width: 15; height: 15 }
            }

            Text {
                Layout.fillWidth: true
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
                elide: Text.ElideRight
            }

            PrimaryButton {
                text: root.replaceActionText
                iconName: "folder"
                quiet: true
                textColor: Theme.textPrimary
                visible: root.showReplaceAction
                enabled: !root.processing
                onClicked: root.replaceClicked()
            }

            Rectangle {
                implicitWidth: outputMetaRow.implicitWidth + Theme.paddingSmall * 1.5
                implicitHeight: 24
                radius: Theme.radiusSmall
                color: Qt.rgba(0, 0, 0, 0.14)
                border.color: Qt.rgba(1, 1, 1, 0.07)
                border.width: 1
                RowLayout {
                    id: outputMetaRow
                    anchors.centerIn: parent
                    spacing: 5
                    Text { text: root.durationText; color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true }
                    Text { text: root.sampleRate > 0 ? root.sampleRate + " Hz" : ""; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; visible: text.length > 0 }
                }
            }
        }

        WaveformView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 72
            visible: !root.processing
            samples: root.samples
            framed: true
            placeholderText: ""
            showPlaceholder: false
            barWidth: 3
            barGap: 2
            waveColor: root.outputAccent
            playedWaveColor: Theme.accentLight
            playbackProgress: root.playbackProgress
            showPlaybackProgress: root.isPlaying || root.isPaused
            seekEnabled: !root.processing && root.samples.length > 0 && root.seekDurationMs > 0
            onSeekRequested: function(progress) { root.seekRequested(Math.round(progress * root.seekDurationMs)) }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 72
            visible: root.processing
            radius: Theme.radiusSmall
            color: Qt.rgba(0, 0, 0, 0.10)
            border.color: Qt.rgba(1, 1, 1, 0.06)
            border.width: 1
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    BusyIndicator { Layout.preferredWidth: 20; Layout.preferredHeight: 20; running: root.processing; visible: root.processing; palette.dark: root.outputAccent }
                    Text { Layout.fillWidth: true; text: root.progressEstimated ? qsTr("%1 (estimated)").arg(root.processingLabel) : root.processingLabel; color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true; elide: Text.ElideRight }
                    Text { text: root.processingProgress + "%"; color: root.outputAccent; font.pixelSize: Theme.fontSmall; font.bold: true }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 6; radius: 3; color: Theme.surface
                    Rectangle { height: parent.height; radius: parent.radius; color: root.outputAccent; width: parent.width * root.normalizedProgress; Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } } }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            spacing: Theme.paddingSmall
            Text {
                Layout.fillWidth: true
                text: root.isPaused ? qsTr("Paused at %1").arg(root.formatTime(root.playbackPositionMs))
                                    : root.isPlaying ? qsTr("Playing %1 / %2").arg(root.formatTime(root.playbackPositionMs)).arg(root.durationText)
                                                     : root.statusText
                color: root.isPaused ? Theme.warning : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
            PrimaryButton { text: root.isPaused ? qsTr("Resume") : qsTr("Play"); iconName: "play"; buttonColor: root.outputAccent; textColor: "#ffffff"; implicitWidth: 88; implicitHeight: 34; enabled: !root.processing && root.samples.length > 0 && (!root.isPlaying || root.isPaused); onClicked: root.isPaused ? root.resumeClicked() : root.playClicked() }
            PrimaryButton { text: qsTr("Pause"); iconName: "pause"; quiet: true; textColor: Theme.textPrimary; borderColor: Qt.rgba(1, 1, 1, 0.10); implicitWidth: 90; implicitHeight: 34; enabled: !root.processing && root.isPlaying && !root.isPaused; onClicked: root.pauseClicked() }
            PrimaryButton { text: qsTr("Stop"); iconName: "stop"; quiet: true; textColor: Theme.textPrimary; borderColor: Qt.rgba(1, 1, 1, 0.10); implicitWidth: 84; implicitHeight: 34; enabled: !root.processing && (root.isPlaying || root.isPaused); onClicked: root.stopClicked() }
            PrimaryButton { text: root.saveActionText; iconName: "save"; quiet: true; textColor: Theme.textPrimary; borderColor: Qt.rgba(1, 1, 1, 0.10); implicitWidth: 84; implicitHeight: 34; visible: root.showSaveAction; enabled: !root.processing && root.samples.length > 0; onClicked: root.saveClicked() }
        }
    }
}
