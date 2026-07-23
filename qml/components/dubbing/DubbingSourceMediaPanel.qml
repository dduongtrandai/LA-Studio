import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "../base"
import "../shared/settings"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property int selectedSegment: -1
    readonly property bool isVideoSource: root.dubbing.sourceMediaPath.length > 0 && /\.(mp4|mkv|mov|webm|avi)$/i.test(root.dubbing.sourceMediaPath)
    readonly property bool hasDubbedPreview: root.dubbing.dubbedVocalPath.length > 0
    property string previewMode: "source"
    readonly property bool showingDubbedMedia: root.previewMode === "dubbed" && root.hasDubbedPreview
    property bool previewMuted: false
    property real vocalLevel: 1.0
    property real backgroundLevel: 1.0
    property real pendingPosition: -1
    property bool pendingPlayback: false
    property int sourceSwitchAttempts: 0

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

    function localMediaUrl(path) {
        if (!path) return ""
        var normalized = path.replace(/\\/g, "/")
        var encoded = encodeURI(normalized).replace(/#/g, "%23")
        return Qt.platform.os === "windows" ? "file:///" + encoded : "file://" + encoded
    }

    function pause() { pauseAll() }
    function pauseAll() {
        mediaPlayer.pause()
        vocalPlayer.pause()
        backgroundPlayer.pause()
    }
    function seekAll(position) {
        mediaPlayer.position = position
        if (root.showingDubbedMedia) {
            if (vocalPlayer.seekable) vocalPlayer.position = position
            if (backgroundPlayer.seekable) backgroundPlayer.position = position
        }
    }
    function playAll() {
        if (root.showingDubbedMedia) {
            if (root.dubbing.dubbedVocalPath.length > 0) {
                vocalPlayer.position = mediaPlayer.position
                vocalPlayer.play()
            }
            if (root.dubbing.backgroundPath.length > 0) {
                backgroundPlayer.position = mediaPlayer.position
                backgroundPlayer.play()
            }
        }
        mediaPlayer.play()
    }
    function switchPreviewMode(mode) {
        if (mode === root.previewMode || (mode === "dubbed" && !root.hasDubbedPreview)) return
        root.pendingPosition = mediaPlayer.position
        root.pendingPlayback = mediaPlayer.playbackState === MediaPlayer.PlayingState
        pauseAll()
        root.previewMode = mode
        root.sourceSwitchAttempts = 0
        sourceSwitchFallback.restart()
    }
    function restoreAfterSourceSwitch() {
        if (root.pendingPosition < 0) return
        var restorePosition = root.pendingPosition
        var restorePlayback = root.pendingPlayback
        root.pendingPosition = -1
        root.pendingPlayback = false
        seekAll(restorePosition)
        if (restorePlayback) playAll()
    }
    function seekToSegment(index) {
        if (mediaPlayer.seekable && index >= 0 && index < root.dubbing.segments.length)
            seekAll(root.dubbing.segments[index].startMs)
    }

    MediaPlayer {
        id: mediaPlayer
        source: root.showingDubbedMedia ? root.dubbing.playbackMediaUrl : root.dubbing.sourceMediaUrl
        audioOutput: AudioOutput {
            id: sourceAudioOutput
            volume: root.showingDubbedMedia || root.previewMuted ? 0 : 1
        }
        videoOutput: videoOutput
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia)
                root.restoreAfterSourceSwitch()
        }
    }
    MediaPlayer {
        id: vocalPlayer
        source: root.localMediaUrl(root.dubbing.dubbedVocalPath)
        audioOutput: AudioOutput {
            volume: root.showingDubbedMedia && !root.previewMuted ? root.vocalLevel : 0
        }
    }
    MediaPlayer {
        id: backgroundPlayer
        source: root.localMediaUrl(root.dubbing.backgroundPath)
        audioOutput: AudioOutput {
            // The rendered mix uses 35% background gain. A 100% slider value
            // therefore reproduces the rendered/exported balance.
            volume: root.showingDubbedMedia && !root.previewMuted
                    ? root.backgroundLevel * 0.35 : 0
        }
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
    onHasDubbedPreviewChanged: {
        if (root.hasDubbedPreview)
            root.switchPreviewMode("dubbed")
        else
            root.switchPreviewMode("source")
    }
    Component.onCompleted: root.previewMode = root.hasDubbedPreview ? "dubbed" : "source"
    Timer {
        id: sourceSwitchFallback
        interval: 120
        onTriggered: {
            if (mediaPlayer.duration > 0
                || mediaPlayer.mediaStatus === MediaPlayer.LoadedMedia
                || mediaPlayer.mediaStatus === MediaPlayer.BufferedMedia
                || root.sourceSwitchAttempts >= 20) {
                root.restoreAfterSourceSwitch()
                return
            }
            root.sourceSwitchAttempts += 1
            restart()
        }
    }
    Timer {
        interval: 500
        repeat: true
        running: root.showingDubbedMedia
                 && mediaPlayer.playbackState === MediaPlayer.PlayingState
        onTriggered: {
            if (vocalPlayer.seekable && Math.abs(vocalPlayer.position - mediaPlayer.position) > 180)
                vocalPlayer.position = mediaPlayer.position
            if (backgroundPlayer.seekable && Math.abs(backgroundPlayer.position - mediaPlayer.position) > 180)
                backgroundPlayer.position = mediaPlayer.position
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("VIDEO PREVIEW")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                font.letterSpacing: 1.1
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                Layout.preferredWidth: 230
                Layout.preferredHeight: 32
                radius: Theme.radiusSmall
                color: Qt.rgba(0, 0, 0, 0.18)
                border.color: Qt.rgba(1, 1, 1, 0.08)
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3
                    PreviewModeButton {
                        Layout.fillWidth: true
                        text: qsTr("Original")
                        iconName: "file"
                        selected: !root.showingDubbedMedia
                        enabled: root.dubbing.sourceMediaPath.length > 0
                        onClicked: root.switchPreviewMode("source")
                    }
                    PreviewModeButton {
                        Layout.fillWidth: true
                        text: qsTr("Dubbed")
                        iconName: "mic"
                        selected: root.showingDubbedMedia
                        enabled: root.hasDubbedPreview
                        onClicked: root.switchPreviewMode("dubbed")
                    }
                }
            }
            Text {
                text: root.showingDubbedMedia
                      ? qsTr("Dubbed mix")
                      : (root.dubbing.sourceMediaPath.length > 0 ? qsTr("Original audio") : qsTr("No media"))
                color: root.dubbing.sourceMediaPath.length > 0 ? Theme.success : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }
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
                        function updatePosition(x) { if (mediaPlayer.duration > 0) root.seekAll(Math.max(0, Math.min(1, x / width)) * mediaPlayer.duration) }
                        onPressed: { wasPlaying = mediaPlayer.playbackState === MediaPlayer.PlayingState; if (wasPlaying) mediaPlayer.pause(); updatePosition(mouseX) }
                        onPositionChanged: if (pressed) updatePosition(mouseX)
                        onReleased: if (wasPlaying) root.playAll()
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
                        onClicked: mediaPlayer.playbackState === MediaPlayer.PlayingState ? root.pauseAll() : root.playAll()
                    }
                    Button {
                        implicitWidth: 28; implicitHeight: 28
                        flat: true
                        contentItem: LineIcon { anchors.centerIn: parent; name: "volume"; color: root.previewMuted ? Theme.textSecondary : Theme.textPrimary; width: 14; height: 14 }
                        onClicked: root.previewMuted = !root.previewMuted
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: "%1 / %2".arg(root.formatTime(mediaPlayer.position)).arg(root.formatTime(mediaPlayer.duration)); color: Theme.textSecondary; font.pixelSize: 11; font.family: "Monospace" }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            visible: root.showingDubbedMedia
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.025)
            border.color: Qt.rgba(1, 1, 1, 0.07)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingMedium
                LineIcon {
                    name: "volume"
                    color: Theme.accentLight
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                }
                Text {
                    text: qsTr("MIX")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1
                }
                MixerControl {
                    Layout.fillWidth: true
                    label: qsTr("Vocal")
                    value: root.vocalLevel
                    onMoved: root.vocalLevel = value
                }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 30; color: Qt.rgba(1, 1, 1, 0.08) }
                MixerControl {
                    Layout.fillWidth: true
                    label: qsTr("Background")
                    value: root.backgroundLevel
                    enabled: root.dubbing.backgroundPath.length > 0
                    onMoved: root.backgroundLevel = value
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

    component PreviewModeButton: Button {
        id: modeButton
        property bool selected: false
        property string iconName: ""
        implicitHeight: 26
        padding: 0
        contentItem: Row {
            anchors.centerIn: parent
            spacing: 5
            LineIcon {
                name: modeButton.iconName
                color: modeButton.enabled
                       ? (modeButton.selected ? Theme.textPrimary : Theme.textSecondary)
                       : Qt.rgba(0.56, 0.56, 0.69, 0.42)
                width: 13
                height: 13
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: modeButton.text
                color: modeButton.enabled
                       ? (modeButton.selected ? Theme.textPrimary : Theme.textSecondary)
                       : Qt.rgba(0.56, 0.56, 0.69, 0.42)
                font.pixelSize: 11
                font.bold: modeButton.selected
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        background: Rectangle {
            radius: 6
            color: modeButton.selected ? Theme.surfaceAlt
                                       : (modeButton.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
            border.color: modeButton.selected ? Qt.rgba(0.64, 0.49, 1, 0.5) : "transparent"
        }
        HoverHandler { cursorShape: modeButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }

    component MixerControl: RowLayout {
        id: mixerControl
        property string label: ""
        property alias value: levelSlider.value
        signal moved()
        spacing: Theme.paddingSmall
        Text {
            text: mixerControl.label
            color: mixerControl.enabled ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: 11
            Layout.preferredWidth: 66
        }
        ParameterSlider {
            id: levelSlider
            Layout.fillWidth: true
            Layout.minimumWidth: 72
            from: 0
            to: 1
            value: 1
            stepSize: 0.01
            enabled: mixerControl.enabled
            onMoved: mixerControl.moved()
        }
        Text {
            text: Math.round(levelSlider.value * 100) + "%"
            color: mixerControl.enabled ? Theme.textSecondary : Qt.rgba(0.56, 0.56, 0.69, 0.5)
            font.pixelSize: 10
            font.family: "Monospace"
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 34
        }
    }
}
