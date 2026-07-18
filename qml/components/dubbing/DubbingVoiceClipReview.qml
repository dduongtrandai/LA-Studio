pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../alignment"
import "../base"
import LAStudio

ColumnLayout {
    id: root

    Layout.fillWidth: true
    Layout.fillHeight: true

    required property var dubbing
    required property var sourceMediaPanel
    required property string playingVoiceClipPath
    required property int generatedClipCount
    required property bool synthesisComplete

    signal voiceClipPlaybackRequested(string path)
    signal separationPlaybackStopped()

    function formatTime(ms) {
        if (isNaN(ms) || ms < 0) return "00:00"
        var totalSec = Math.floor(ms / 1000)
        var hr = Math.floor(totalSec / 3600)
        var min = Math.floor((totalSec - hr * 3600) / 60)
        var sec = totalSec - hr * 3600 - min * 60
        var minStr = min < 10 ? "0" + min : min.toString()
        var secStr = sec < 10 ? "0" + sec : sec.toString()
        return hr > 0 ? (hr < 10 ? "0" + hr : hr.toString()) + ":" + minStr + ":" + secStr : minStr + ":" + secStr
    }

    function segmentStateColor(segment) {
        if (!segment || !(segment.clipPath || "")) return Theme.textSecondary
        if (segment.state === "failed" || segment.state === "error") return Theme.danger
        if (segment.timingConflict || segment.state === "conflict") return Theme.warning
        return Theme.success
    }

ColumnLayout {
    Layout.fillWidth: true
    Layout.fillHeight: true
    spacing: Theme.paddingSmall

    RowLayout {
        Layout.fillWidth: true
        Text {
            Layout.fillWidth: true
            text: qsTr("%1 of %2 segment clips generated")
                  .arg(root.generatedClipCount).arg(root.dubbing.segments.length)
            color: root.synthesisComplete ? Theme.success : Theme.warning
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }
        Text {
            text: qsTr("Click a waveform or Play to review")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
        }
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Qt.rgba(1, 1, 1, 0.07)
    }

    ListView {
        id: generatedVoiceList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: Theme.paddingSmall
        model: root.dubbing.segments
        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            id: voiceClipCard
            required property int index
            required property var modelData
            readonly property string clipPath: modelData.clipPath || ""
            readonly property bool hasAudio: clipPath !== ""
            readonly property bool ownsPlayback: hasAudio
                                                    && root.playingVoiceClipPath === clipPath
                                                    && AppController.player.playing
            readonly property real playbackProgress: ownsPlayback
                                                      && AppController.player.playbackDurationMs > 0
                                                    ? AppController.player.playbackPositionMs
                                                      / AppController.player.playbackDurationMs
                                                    : 0

            width: ListView.view.width
            height: 116
            radius: Theme.radiusSmall
            color: ownsPlayback
                   ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                   : Qt.rgba(1, 1, 1, 0.025)
            border.color: ownsPlayback
                          ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                          : Qt.rgba(1, 1, 1, 0.07)
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    Text {
                        text: qsTr("Segment %1").arg(voiceClipCard.index + 1)
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                    Text {
                        text: "%1 – %2".arg(root.formatTime(modelData.startMs || 0))
                                       .arg(root.formatTime(modelData.endMs || 0))
                        color: Theme.textSecondary
                        font.pixelSize: 10
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.targetText || qsTr("No translated text")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        implicitWidth: voiceStateText.implicitWidth + 14
                        implicitHeight: 22
                        radius: 11
                        color: Qt.rgba(root.segmentStateColor(modelData).r,
                                       root.segmentStateColor(modelData).g,
                                       root.segmentStateColor(modelData).b, 0.12)
                        Text {
                            id: voiceStateText
                            anchors.centerIn: parent
                            text: !voiceClipCard.hasAudio ? qsTr("Missing")
                                  : modelData.timingConflict ? qsTr("Timing conflict")
                                  : qsTr("Ready")
                            color: root.segmentStateColor(modelData)
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.paddingSmall

                    WaveformView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 52
                        samples: modelData.waveformSamples || []
                        framed: true
                        placeholderText: voiceClipCard.hasAudio
                                         ? qsTr("Audio clip ready")
                                         : qsTr("Voice has not been generated")
                        showPlaceholder: !samples || samples.length === 0
                        barWidth: 2
                        barGap: 2
                        verticalScale: 0.76
                        waveColor: voiceClipCard.ownsPlayback ? Theme.accentLight : Theme.accent
                        playedWaveColor: Theme.accentLight
                        playbackProgress: voiceClipCard.playbackProgress
                        showPlaybackProgress: voiceClipCard.ownsPlayback
                        seekEnabled: voiceClipCard.hasAudio
                        onSeekRequested: function(progress) {
                            if (!voiceClipCard.ownsPlayback) {
                                root.sourceMediaPanel.pause()
                                root.separationPlaybackStopped()
                                AppController.player.playFile(voiceClipCard.clipPath)
                                root.voiceClipPlaybackRequested(voiceClipCard.clipPath)
                            }
                            AppController.player.seek(
                                Math.round(progress * AppController.player.playbackDurationMs))
                        }
                    }

                    AlignmentPlayerButton {
                        iconName: voiceClipCard.ownsPlayback && !AppController.player.paused
                                  ? "pause" : "play"
                        toolTip: voiceClipCard.ownsPlayback && !AppController.player.paused
                                 ? qsTr("Pause segment") : qsTr("Play segment")
                        highlighted: voiceClipCard.ownsPlayback
                        enabled: voiceClipCard.hasAudio
                        onClicked: {
                            if (voiceClipCard.ownsPlayback) {
                                if (AppController.player.paused)
                                    AppController.player.resume()
                                else
                                    AppController.player.pause()
                            } else {
                                root.sourceMediaPanel.pause()
                                root.separationPlaybackStopped()
                                AppController.player.playFile(voiceClipCard.clipPath)
                                root.voiceClipPlaybackRequested(voiceClipCard.clipPath)
                            }
                        }
                    }

                    AlignmentPlayerButton {
                        iconName: "stop"
                        toolTip: qsTr("Stop segment")
                        enabled: voiceClipCard.ownsPlayback
                        onClicked: AppController.player.stop()
                    }

                    Text {
                        Layout.preferredWidth: 52
                        text: voiceClipCard.ownsPlayback
                              ? root.formatTime(AppController.player.playbackPositionMs)
                              : root.formatTime(modelData.durationMs
                                                || modelData.sourceDurationMs || 0)
                        color: voiceClipCard.ownsPlayback
                               ? Theme.accentLight : Theme.textSecondary
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }
    }
}
}
