import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

ColumnLayout {
    id: root

    property string vocalsPath: ""
    property string backgroundPath: ""
    property var vocalsSamples: []
    property var backgroundSamples: []
    property string playingStem: ""
    property bool showActions: true
    property bool showPlaybackControls: true
    property bool showExportButton: true
    property bool showWaveforms: true
    property bool compact: false

    signal playRequested(string kind, string path)
    signal exportRequested(string kind, string path)
    signal seekRequested(string kind, real progress)

    spacing: Theme.paddingSmall

    function stemTitle(kind) {
        return kind === "vocals" ? qsTr("Vocals") : qsTr("Background")
    }

    function stemSubtitle(kind) {
        return kind === "vocals"
                ? qsTr("Use for STT, diarization and voice reference")
                : qsTr("Use for dubbing mix and export")
    }

    function stemPath(kind) {
        return kind === "vocals" ? root.vocalsPath : root.backgroundPath
    }

    function stemSamples(kind) {
        return kind === "vocals" ? root.vocalsSamples : root.backgroundSamples
    }

    Repeater {
        model: ["vocals", "background"]

        delegate: Rectangle {
            required property string modelData
            Layout.fillWidth: true
            Layout.fillHeight: !root.compact
            Layout.minimumHeight: root.compact ? 92 : 180
            implicitHeight: root.compact ? 92 : 180
            radius: Theme.radiusMedium
            color: Theme.surface
            border.color: root.playingStem === modelData
                          ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45)
                          : Qt.rgba(1, 1, 1, 0.08)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    LineIcon {
                        name: modelData === "vocals" ? "mic" : "waves"
                        color: Theme.accent
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text { text: root.stemTitle(modelData); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true }
                        Text { text: root.stemSubtitle(modelData); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Text {
                        text: root.stemPath(modelData).length > 0 ? qsTr("Ready") : qsTr("Waiting")
                        color: root.stemPath(modelData).length > 0 ? Theme.success : Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }

                WaveformView {
                    Layout.fillWidth: true
                    Layout.fillHeight: !root.compact
                    Layout.minimumHeight: root.compact ? 0 : 72
                    visible: root.showWaveforms && !root.compact
                    framed: true
                    samples: root.stemSamples(modelData)
                    placeholderText: root.stemPath(modelData).length > 0 ? qsTr("Loading waveform...") : qsTr("Stem waveform will appear here")
                    showPlaceholder: root.stemSamples(modelData).length === 0
                    playbackProgress: AppController.player.playbackDurationMs > 0
                                      ? AppController.player.playbackPositionMs / AppController.player.playbackDurationMs : 0
                    showPlaybackProgress: root.playingStem === modelData && AppController.player.playing
                    seekEnabled: root.playingStem === modelData && AppController.player.playbackDurationMs > 0
                    onSeekRequested: function(progress) { root.seekRequested(modelData, progress) }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: root.stemPath(modelData); color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideMiddle; visible: root.showActions || root.compact }
                    PrimaryButton {
                        visible: root.showActions && root.showPlaybackControls
                        text: root.playingStem === modelData && AppController.player.playing ? qsTr("Stop") : qsTr("Play")
                        iconName: root.playingStem === modelData && AppController.player.playing ? "stop" : "play"
                        quiet: true
                        textColor: Theme.textPrimary
                        enabled: root.stemPath(modelData).length > 0
                        onClicked: root.playRequested(modelData, root.stemPath(modelData))
                    }
                    PrimaryButton {
                        visible: root.showActions && root.showExportButton
                        text: qsTr("Export WAV")
                        iconName: "save"
                        quiet: true
                        textColor: Theme.textPrimary
                        enabled: root.stemPath(modelData).length > 0
                        onClicked: root.exportRequested(modelData, root.stemPath(modelData))
                    }
                }
            }
        }
    }
}
