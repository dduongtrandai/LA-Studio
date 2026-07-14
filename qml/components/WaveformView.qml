import QtQuick
import LAStudio

Rectangle {
    id: root

    property var samples: []
    property color waveColor: Theme.accent
    property color secondaryWaveColor: Theme.accentLight
    property color playedWaveColor: Theme.accentLight
    property string placeholderText: qsTr("No audio data")
    property var buckets: []
    property bool framed: false
    property bool showPlaceholder: true
    property real verticalScale: 0.88
    property int barWidth: 3
    property int barGap: 2
    property real playbackProgress: 0.0
    property bool showPlaybackProgress: false
    property bool seekEnabled: false
    property real hoverProgress: -1.0
    property real pendingSeekProgress: -1.0

    signal seekRequested(real progress)

    function requestSeek(x, immediate) {
        if (!seekEnabled || width <= 0)
            return
        const progress = Math.max(0, Math.min(1, x / width))
        if (immediate) {
            seekThrottle.stop()
            pendingSeekProgress = -1.0
            seekRequested(progress)
            return
        }

        pendingSeekProgress = progress
        if (!seekThrottle.running)
            seekThrottle.start()
    }

    Timer {
        id: seekThrottle
        interval: 45
        repeat: false
        onTriggered: {
            if (root.pendingSeekProgress >= 0) {
                root.seekRequested(root.pendingSeekProgress)
                root.pendingSeekProgress = -1.0
            }
        }
    }

    function rebuildBuckets() {
        if (!root.samples || root.samples.length === 0 || width <= 0) {
            buckets = []
            return
        }

        const step = barWidth + barGap
        const bucketCount = Math.max(1, Math.floor(width / step))
        const values = new Array(bucketCount)
        for (let b = 0; b < bucketCount; b++) {
            values[b] = { peak: 0, rms: 0, count: 0 }
        }

        let maxAbs = 0.001
        for (let i = 0; i < root.samples.length; i++) {
            const absVal = Math.abs(root.samples[i])
            if (absVal > maxAbs)
                maxAbs = absVal
        }

        for (let i = 0; i < root.samples.length; i++) {
            const bucket = Math.min(bucketCount - 1, Math.floor(i * bucketCount / root.samples.length))
            const normVal = Math.abs(root.samples[i]) / maxAbs
            values[bucket].peak = Math.max(values[bucket].peak, normVal)
            values[bucket].rms += normVal * normVal
            values[bucket].count += 1
        }

        for (let x = 0; x < bucketCount; x++) {
            const count = Math.max(1, values[x].count)
            const rms = Math.sqrt(values[x].rms / count)
            values[x] = {
                peak: Math.max(values[x].peak, 0.025),
                rms: Math.max(rms, 0.018)
            }
        }

        buckets = values
    }

    implicitHeight: 120
    radius: Theme.radiusSmall
    color: root.framed ? Qt.rgba(1, 1, 1, 0.025) : "transparent"
    border.color: root.framed ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
    border.width: root.framed ? 1 : 0
    clip: true

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.margins: root.framed ? Theme.paddingSmall : 0

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            if (!root.samples || root.samples.length === 0 || root.buckets.length === 0) {
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.15)
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(8, height / 2)
                ctx.lineTo(width - 8, height / 2)
                ctx.stroke()
                return
            }

            const midY = height / 2
            const maxBarHeight = height * root.verticalScale
            const step = root.barWidth + root.barGap
            const barRadiusWidth = Math.max(1, root.barWidth)
            const progress = Math.max(0, Math.min(1, root.playbackProgress))
            const progressX = width * progress

            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.06)
            ctx.lineWidth = 1
            for (let y = 0.25; y <= 0.75; y += 0.25) {
                const gridY = height * y
                ctx.beginPath()
                ctx.moveTo(0, gridY)
                ctx.lineTo(width, gridY)
                ctx.stroke()
            }

            ctx.lineCap = "round"
            for (let i = 0; i < root.buckets.length; i++) {
                const peak = root.buckets[i].peak
                const rms = root.buckets[i].rms
                const blend = Math.min(1, rms * 0.72 + peak * 0.28)
                const half = Math.max(2, (blend * maxBarHeight) / 2)
                const xPos = i * step + root.barWidth / 2
                const isPlayed = root.showPlaybackProgress && xPos <= progressX

                ctx.lineWidth = barRadiusWidth + 3
                ctx.strokeStyle = Qt.rgba(isPlayed ? root.playedWaveColor.r : root.waveColor.r,
                                          isPlayed ? root.playedWaveColor.g : root.waveColor.g,
                                          isPlayed ? root.playedWaveColor.b : root.waveColor.b, 0.14)
                ctx.beginPath()
                ctx.moveTo(xPos, midY - half)
                ctx.lineTo(xPos, midY + half)
                ctx.stroke()

                ctx.lineWidth = barRadiusWidth
                ctx.strokeStyle = isPlayed ? root.playedWaveColor : root.waveColor
                ctx.beginPath()
                ctx.moveTo(xPos, midY - half)
                ctx.lineTo(xPos, midY + half)
                ctx.stroke()
            }

            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.12)
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(8, midY)
            ctx.lineTo(width - 8, midY)
            ctx.stroke()

            if (root.showPlaybackProgress && progress > 0 && progress < 1) {
                ctx.strokeStyle = root.playedWaveColor
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(progressX, 2)
                ctx.lineTo(progressX, height - 2)
                ctx.stroke()
            }

            if (root.seekEnabled && root.hoverProgress >= 0 && root.hoverProgress <= 1) {
                const hoverX = width * root.hoverProgress
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.34)
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(hoverX, 2)
                ctx.lineTo(hoverX, height - 2)
                ctx.stroke()
            }
        }
    }

    onSamplesChanged: {
        rebuildBuckets()
        canvas.requestPaint()
    }
    onWidthChanged: {
        rebuildBuckets()
        canvas.requestPaint()
    }
    onHeightChanged: canvas.requestPaint()
    onPlaybackProgressChanged: canvas.requestPaint()
    onShowPlaybackProgressChanged: canvas.requestPaint()
    onHoverProgressChanged: canvas.requestPaint()

    MouseArea {
        anchors.fill: parent
        enabled: root.seekEnabled && root.samples && root.samples.length > 0
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true

        onPressed: function(mouse) {
            root.requestSeek(mouse.x, true)
        }
        onPositionChanged: function(mouse) {
            root.hoverProgress = Math.max(0, Math.min(1, mouse.x / width))
            if (pressed)
                root.requestSeek(mouse.x, false)
        }
        onReleased: function(mouse) { root.requestSeek(mouse.x, true) }
        onExited: root.hoverProgress = -1.0
    }

    Text {
        anchors.centerIn: parent
        text: root.placeholderText
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        visible: root.showPlaceholder && (!root.samples || root.samples.length === 0)
    }
}
