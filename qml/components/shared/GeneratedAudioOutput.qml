import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Rectangle {
    id: root

    Layout.fillWidth: true
    Layout.preferredHeight: active ? 214 : 0
    visible: active
    opacity: active ? 1 : 0
    radius: Theme.radiusMedium
    color: Qt.rgba(1, 1, 1, 0.035)
    border.color: isPlaying ? Qt.rgba(0.49, 0.30, 1.0, 0.34) : Qt.rgba(1, 1, 1, 0.09)
    border.width: 1
    clip: true

    property bool outputReady: false
    property var samples: []
    property string durationText: "--"
    property int sampleRate: 0
    property string sampleCountText: ""
    property bool isPlaying: false
    property bool isPaused: false
    property int playbackPositionMs: 0
    property int playbackDurationMs: 0
    property int audioDurationMs: 0
    property var family: null
    property bool processing: false
    property int generationProgress: 0
    property bool progressEstimated: true
    property string progressLabel: qsTr("Generating audio")
    readonly property bool active: outputReady || processing
    readonly property real normalizedProgress: Math.max(0, Math.min(100, generationProgress)) / 100.0
    readonly property real playbackProgress: playbackDurationMs > 0
                                          ? Math.max(0, Math.min(1, playbackPositionMs / playbackDurationMs)) : 0
    readonly property int seekDurationMs: playbackDurationMs > 0 ? playbackDurationMs : audioDurationMs
    readonly property color outputAccent: family ? family.accent : Theme.accent

    signal playClicked()
    signal pauseClicked()
    signal resumeClicked()
    signal stopClicked()
    signal seekRequested(int positionMs)
    signal saveClicked()

    function formatTime(milliseconds) {
        var seconds = Math.max(0, Math.floor(milliseconds / 1000))
        var minutes = Math.floor(seconds / 60)
        return minutes + ":" + (seconds % 60 < 10 ? "0" : "") + (seconds % 60)
    }

    Behavior on opacity {
        NumberAnimation { duration: 160; easing.type: Easing.OutQuad }
    }

    Behavior on border.color {
        ColorAnimation { duration: 140 }
    }

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

                LineIcon {
                    anchors.centerIn: parent
                    name: "volume"
                    color: root.outputAccent
                    width: 15
                    height: 15
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Generated Audio")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
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

                    Text {
                        text: root.durationText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }

                    Text {
                        text: root.sampleRate > 0 ? root.sampleRate + " Hz" : "-- Hz"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }
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
            showPlaybackProgress: root.isPlaying
            seekEnabled: !root.processing && root.samples.length > 0 && root.seekDurationMs > 0
            onSeekRequested: function(progress) {
                root.seekRequested(Math.round(progress * root.seekDurationMs))
            }
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

                    BusyIndicator {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        running: root.processing
                        visible: root.processing
                        palette.dark: root.outputAccent
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.progressEstimated ? qsTr("%1 (estimated)").arg(root.progressLabel) : root.progressLabel
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.generationProgress + "%"
                        color: root.outputAccent
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Theme.surface

                    Rectangle {
                        height: parent.height
                        radius: parent.radius
                        color: root.outputAccent
                        width: parent.width * root.normalizedProgress
                        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }
                    }
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
                                                     : root.sampleCountText
                color: root.isPaused ? Theme.warning : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }

            PrimaryButton {
                text: root.isPaused ? qsTr("Resume") : qsTr("Play")
                iconName: "play"
                buttonColor: root.outputAccent
                textColor: "#ffffff"
                implicitWidth: 88
                implicitHeight: 34
                enabled: !root.processing && root.samples.length > 0 && (!root.isPlaying || root.isPaused)
                onClicked: root.isPaused ? root.resumeClicked() : root.playClicked()
            }

            PrimaryButton {
                text: qsTr("Pause")
                iconName: "pause"
                quiet: true
                textColor: Theme.textPrimary
                borderColor: Qt.rgba(1, 1, 1, 0.10)
                implicitWidth: 90
                implicitHeight: 34
                enabled: !root.processing && root.isPlaying && !root.isPaused
                onClicked: root.pauseClicked()
            }

            PrimaryButton {
                text: qsTr("Stop")
                iconName: "stop"
                quiet: true
                textColor: Theme.textPrimary
                borderColor: Qt.rgba(1, 1, 1, 0.10)
                implicitWidth: 84
                implicitHeight: 34
                enabled: !root.processing && root.isPlaying
                onClicked: root.stopClicked()
            }

            PrimaryButton {
                text: qsTr("Save")
                iconName: "save"
                quiet: true
                textColor: Theme.textPrimary
                borderColor: Qt.rgba(1, 1, 1, 0.10)
                implicitWidth: 84
                implicitHeight: 34
                enabled: !root.processing && root.samples.length > 0
                onClicked: root.saveClicked()
            }
        }
    }
}
