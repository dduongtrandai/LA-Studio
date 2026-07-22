import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property int selectedSegment: -1
    readonly property bool isVideoSource: root.dubbing.sourceMediaPath.length > 0 && /\.(mp4|mkv|mov|webm|avi)$/i.test(root.dubbing.sourceMediaPath)
    readonly property bool showingDubbedMedia: root.dubbing.exportPath.length > 0

    signal browseRequested()
    signal segmentSelected(int index)

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 300
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

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

    function pause() { mediaPlayer.pause() }
    function seekToSegment(index) {
        if (mediaPlayer.seekable && index >= 0 && index < root.dubbing.segments.length)
            mediaPlayer.position = root.dubbing.segments[index].startMs
    }

    MediaPlayer {
        id: mediaPlayer
        source: root.dubbing.playbackMediaUrl
        audioOutput: AudioOutput {}
        videoOutput: videoOutput
    }

    Connections {
        target: mediaPlayer
        function onSubtitleTracksChanged() {
            if (mediaPlayer.subtitleTracks.length > 0)
                mediaPlayer.activeSubtitleTrack = 0
        }
        function onPositionChanged() {
            if (mediaPlayer.playbackState !== MediaPlayer.PlayingState) return
            for (var i = 0; i < root.dubbing.segments.length; ++i) {
                var segment = root.dubbing.segments[i]
                if (mediaPlayer.position >= segment.startMs && mediaPlayer.position <= segment.endMs) {
                    if (root.selectedSegment !== i) {
                        root.selectedSegment = i
                        root.segmentSelected(i)
                    }
                    break
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall
        RowLayout {
            Layout.fillWidth: true
            Text { text: root.showingDubbedMedia ? qsTr("DUBBED PREVIEW") : qsTr("SOURCE MEDIA"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1; Layout.fillWidth: true }
            Text { text: root.showingDubbedMedia ? qsTr("Voice + background + subtitles") : (root.dubbing.sourceMediaPath.length > 0 ? qsTr("Loaded") : qsTr("No media")); color: root.dubbing.sourceMediaPath.length > 0 ? Theme.success : Theme.textSecondary; font.pixelSize: Theme.fontSmall }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusSmall
            color: Qt.rgba(0, 0, 0, 0.30)
            border.color: Qt.rgba(1, 1, 1, 0.06)
            border.width: 1
            clip: true

            Column {
                anchors.centerIn: parent
                width: parent.width - Theme.paddingXL * 2
                spacing: Theme.paddingSmall
                visible: root.dubbing.sourceMediaPath.length === 0
                LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "folder"; color: Theme.accentLight; width: 38; height: 38 }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Add source media"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("WAV, MP3, MP4 or MKV"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            }
            VideoOutput { id: videoOutput; anchors.fill: parent; visible: root.isVideoSource; fillMode: VideoOutput.PreserveAspectFit }
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0.06, 0.06, 0.09, 0.95)
                visible: root.dubbing.sourceMediaPath.length > 0 && !root.isVideoSource
                Column {
                    anchors.centerIn: parent
                    width: parent.width - Theme.paddingXL * 2
                    spacing: Theme.paddingSmall
                    LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "volume"; color: Theme.accentLight; width: 42; height: 42 }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.dubbing.sourceMediaPath.split(/[\\/]/).pop(); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; elide: Text.ElideMiddle; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Audio track playing"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                }
            }
            MouseArea { anchors.fill: parent; enabled: root.dubbing.sourceMediaPath.length === 0; cursorShape: Qt.PointingHandCursor; onClicked: root.browseRequested() }

            MouseArea {
                id: hoverArea
                anchors.fill: parent
                visible: root.dubbing.sourceMediaPath.length > 0
                hoverEnabled: true
                preventStealing: true
                onPositionChanged: controlsTimer.restart()
            }
            Timer { id: controlsTimer; interval: 2500; running: mediaPlayer.playbackState === MediaPlayer.PlayingState }
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 44
                visible: opacity > 0
                opacity: (hoverArea.containsMouse || seekArea.pressed || !controlsTimer.running || mediaPlayer.playbackState !== MediaPlayer.PlayingState) ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250 } }
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: Qt.rgba(0.06, 0.06, 0.09, 0.92) }
                }

                Item {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.topMargin: -8
                    height: 16
                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        color: Qt.rgba(255, 255, 255, 0.2)
                        Rectangle {
                            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                            color: Theme.accentLight
                            width: mediaPlayer.duration > 0 ? mediaPlayer.position / mediaPlayer.duration * parent.width : 0
                        }
                    }
                    MouseArea {
                        id: seekArea
                        anchors.fill: parent
                        property bool wasPlaying: false
                        function updatePosition(x) { if (mediaPlayer.duration > 0) mediaPlayer.position = Math.max(0, Math.min(1, x / width)) * mediaPlayer.duration }
                        onPressed: { wasPlaying = mediaPlayer.playbackState === MediaPlayer.PlayingState; if (wasPlaying) mediaPlayer.pause(); updatePosition(mouseX) }
                        onPositionChanged: if (pressed) updatePosition(mouseX)
                        onReleased: if (wasPlaying) mediaPlayer.play()
                    }
                }
                RowLayout {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    anchors.leftMargin: Theme.paddingMedium; anchors.rightMargin: Theme.paddingMedium
                    height: 36
                    spacing: Theme.paddingMedium
                    Button {
                        implicitWidth: 28; implicitHeight: 28
                        flat: true
                        contentItem: LineIcon { anchors.centerIn: parent; name: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "pause" : "play"; color: Theme.textPrimary; width: 14; height: 14 }
                        onClicked: mediaPlayer.playbackState === MediaPlayer.PlayingState ? mediaPlayer.pause() : mediaPlayer.play()
                    }
                    Button {
                        implicitWidth: 28; implicitHeight: 28
                        flat: true
                        contentItem: LineIcon { anchors.centerIn: parent; name: "volume"; color: mediaPlayer.audioOutput.muted ? Theme.textSecondary : Theme.textPrimary; width: 14; height: 14 }
                        onClicked: mediaPlayer.audioOutput.muted = !mediaPlayer.audioOutput.muted
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: "%1 / %2".arg(root.formatTime(mediaPlayer.position)).arg(root.formatTime(mediaPlayer.duration)); color: Theme.textSecondary; font.pixelSize: 11; font.family: "Monospace" }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            FieldProxy { Layout.fillWidth: true; text: root.dubbing.sourceMediaPath; placeholderText: qsTr("Media file path") }
            PrimaryButton { text: qsTr("Browse"); iconName: "folder"; quiet: true; enabled: !root.dubbing.processing; onClicked: root.browseRequested() }
        }
    }

    component FieldProxy: TextField {
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        selectByMouse: true
        readOnly: true
        leftPadding: Theme.paddingMedium
        rightPadding: Theme.paddingMedium
        background: Rectangle { radius: Theme.radiusSmall; color: Qt.rgba(1, 1, 1, 0.035); border.color: parent.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.09); border.width: parent.activeFocus ? 2 : 1 }
    }
}
