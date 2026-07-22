import QtQuick
import LAStudio

Canvas {
    id: root

    property string name: "settings"
    property color color: Theme.textPrimary
    property real strokeWidth: 1.7

    implicitWidth: Theme.iconSize
    implicitHeight: Theme.iconSize

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function line(ctx, x1, y1, x2, y2) {
        ctx.beginPath()
        ctx.moveTo(x1, y1)
        ctx.lineTo(x2, y2)
        ctx.stroke()
    }

    function circle(ctx, x, y, r) {
        ctx.beginPath()
        ctx.arc(x, y, r, 0, Math.PI * 2, false)
        ctx.stroke()
    }

    onPaint: {
        var ctx = getContext("2d")
        var w = width
        var h = height
        var s = Math.min(w, h)
        var cx = w / 2
        var cy = h / 2
        var u = s / 24

        ctx.clearRect(0, 0, w, h)
        ctx.strokeStyle = root.color
        ctx.fillStyle = root.color
        ctx.lineWidth = root.strokeWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        if (name === "home") {
            ctx.beginPath()
            ctx.moveTo(cx, cy - 8 * u)
            ctx.lineTo(cx - 8 * u, cy)
            ctx.lineTo(cx - 5 * u, cy)
            ctx.lineTo(cx - 5 * u, cy + 8 * u)
            ctx.lineTo(cx + 5 * u, cy + 8 * u)
            ctx.lineTo(cx + 5 * u, cy)
            ctx.lineTo(cx + 8 * u, cy)
            ctx.closePath()
            ctx.stroke()
            // Door
            ctx.beginPath()
            ctx.moveTo(cx - 2 * u, cy + 8 * u)
            ctx.lineTo(cx - 2 * u, cy + 3 * u)
            ctx.lineTo(cx + 2 * u, cy + 3 * u)
            ctx.lineTo(cx + 2 * u, cy + 8 * u)
            ctx.stroke()
        } else if (name === "chevron-left") {
            ctx.beginPath()
            ctx.moveTo(cx + 4 * u, cy - 7 * u)
            ctx.lineTo(cx - 4 * u, cy)
            ctx.lineTo(cx + 4 * u, cy + 7 * u)
            ctx.stroke()
        } else if (name === "chevron-right") {
            ctx.beginPath()
            ctx.moveTo(cx - 4 * u, cy - 7 * u)
            ctx.lineTo(cx + 4 * u, cy)
            ctx.lineTo(cx - 4 * u, cy + 7 * u)
            ctx.stroke()
        } else if (name === "chevron-down") {
            ctx.beginPath()
            ctx.moveTo(cx - 7 * u, cy - 4 * u)
            ctx.lineTo(cx, cy + 4 * u)
            ctx.lineTo(cx + 7 * u, cy - 4 * u)
            ctx.stroke()
        } else if (name === "chevron-up") {
            ctx.beginPath()
            ctx.moveTo(cx - 7 * u, cy + 4 * u)
            ctx.lineTo(cx, cy - 4 * u)
            ctx.lineTo(cx + 7 * u, cy + 4 * u)
            ctx.stroke()
        } else if (name === "arrow-down") {
            ctx.beginPath()
            ctx.moveTo(cx - 5 * u, cy - 2 * u)
            ctx.lineTo(cx, cy + 4 * u)
            ctx.lineTo(cx + 5 * u, cy - 2 * u)
            ctx.stroke()
        } else if (name === "arrow-up") {
            ctx.beginPath()
            ctx.moveTo(cx - 5 * u, cy + 2 * u)
            ctx.lineTo(cx, cy - 4 * u)
            ctx.lineTo(cx + 5 * u, cy + 2 * u)
            ctx.stroke()
        } else if (name === "close") {
            line(ctx, cx - 6 * u, cy - 6 * u, cx + 6 * u, cy + 6 * u)
            line(ctx, cx + 6 * u, cy - 6 * u, cx - 6 * u, cy + 6 * u)
        } else if (name === "plus") {
            line(ctx, cx - 7 * u, cy, cx + 7 * u, cy)
            line(ctx, cx, cy - 7 * u, cx, cy + 7 * u)
        } else if (name === "send") {
            ctx.beginPath()
            ctx.moveTo(cx - 9 * u, cy - 7 * u)
            ctx.lineTo(cx + 9 * u, cy)
            ctx.lineTo(cx - 9 * u, cy + 7 * u)
            ctx.lineTo(cx - 5 * u, cy)
            ctx.closePath()
            ctx.stroke()
            line(ctx, cx - 5 * u, cy, cx + 4 * u, cy)
        } else if (name === "minus") {
            line(ctx, cx - 7 * u, cy, cx + 7 * u, cy)
        } else if (name === "maximize") {
            ctx.strokeRect(cx - 7 * u, cy - 7 * u, 14 * u, 14 * u)
        } else if (name === "restore") {
            ctx.strokeRect(cx - 4 * u, cy - 4 * u, 11 * u, 11 * u)
            ctx.beginPath()
            ctx.moveTo(cx - 1 * u, cy - 4 * u)
            ctx.lineTo(cx - 1 * u, cy - 7 * u)
            ctx.lineTo(cx + 7 * u, cy - 7 * u)
            ctx.lineTo(cx + 7 * u, cy + 1 * u)
            ctx.lineTo(cx + 4 * u, cy + 1 * u)
            ctx.stroke()
        } else if (name === "settings") {
            circle(ctx, cx, cy, 2.5 * u)
            ctx.beginPath()
            var teethCount = 8
            for (var i = 0; i < teethCount; i++) {
                var baseAngle = i * Math.PI / 4
                var angle_a = baseAngle - 0.18
                var angle_b = baseAngle - 0.12
                var angle_c = baseAngle + 0.12
                var angle_d = baseAngle + 0.18

                var r_in = 5.5 * u
                var r_out = 8.0 * u

                var x1 = cx + r_in * Math.cos(angle_a)
                var y1 = cy + r_in * Math.sin(angle_a)
                var x2 = cx + r_out * Math.cos(angle_b)
                var y2 = cy + r_out * Math.sin(angle_b)
                var x3 = cx + r_out * Math.cos(angle_c)
                var y3 = cy + r_out * Math.sin(angle_c)
                var x4 = cx + r_in * Math.cos(angle_d)
                var y4 = cy + r_in * Math.sin(angle_d)

                if (i === 0) {
                    ctx.moveTo(x1, y1)
                } else {
                    ctx.lineTo(x1, y1)
                }
                ctx.lineTo(x2, y2)
                ctx.lineTo(x3, y3)
                ctx.lineTo(x4, y4)
            }
            ctx.closePath()
            ctx.stroke()
        } else if (name === "sliders") {
            line(ctx, cx - 8 * u, cy - 6 * u, cx + 8 * u, cy - 6 * u)
            line(ctx, cx - 8 * u, cy, cx + 8 * u, cy)
            line(ctx, cx - 8 * u, cy + 6 * u, cx + 8 * u, cy + 6 * u)
            circle(ctx, cx - 2 * u, cy - 6 * u, 2 * u)
            circle(ctx, cx + 4 * u, cy, 2 * u)
            circle(ctx, cx - 5 * u, cy + 6 * u, 2 * u)
        } else if (name === "alignment") {
            line(ctx, cx - 8 * u, cy - 2 * u, cx - 8 * u, cy + 2 * u)
            line(ctx, cx - 4 * u, cy - 6 * u, cx - 4 * u, cy + 2 * u)
            line(ctx, cx, cy - 9 * u, cx, cy + 2 * u)
            line(ctx, cx + 4 * u, cy - 6 * u, cx + 4 * u, cy + 2 * u)
            line(ctx, cx + 8 * u, cy - 2 * u, cx + 8 * u, cy + 2 * u)
            line(ctx, cx - 9 * u, cy + 8 * u, cx + 9 * u, cy + 8 * u)
            line(ctx, cx, cy + 4 * u, cx, cy + 10 * u)
        } else if (name === "workflow") {
            // Three connected stages: a compact pipeline/workflow glyph.
            line(ctx, cx - 6 * u, cy, cx - 1 * u, cy)
            line(ctx, cx + 1 * u, cy, cx + 6 * u, cy)
            circle(ctx, cx - 8 * u, cy, 2 * u)
            circle(ctx, cx, cy, 2 * u)
            circle(ctx, cx + 8 * u, cy, 2 * u)
            line(ctx, cx, cy - 2 * u, cx, cy - 6 * u)
            circle(ctx, cx, cy - 8 * u, 2 * u)
        } else if (name === "play") {
            ctx.beginPath()
            ctx.moveTo(cx - 5 * u, cy - 8 * u)
            ctx.lineTo(cx + 7 * u, cy)
            ctx.lineTo(cx - 5 * u, cy + 8 * u)
            ctx.closePath()
            ctx.fill()
        } else if (name === "pause") {
            ctx.fillRect(cx - 6 * u, cy - 7 * u, 4 * u, 14 * u)
            ctx.fillRect(cx + 2 * u, cy - 7 * u, 4 * u, 14 * u)
        } else if (name === "stop") {
            ctx.fillRect(cx - 6 * u, cy - 6 * u, 12 * u, 12 * u)
        } else if (name === "save") {
            ctx.strokeRect(cx - 8 * u, cy - 8 * u, 16 * u, 16 * u)
            line(ctx, cx - 4 * u, cy - 8 * u, cx - 4 * u, cy - 2 * u)
            line(ctx, cx + 4 * u, cy - 8 * u, cx + 4 * u, cy - 2 * u)
            line(ctx, cx - 4 * u, cy + 4 * u, cx + 4 * u, cy + 4 * u)
        } else if (name === "chat") {
            // Compact conversation bubble used by the local LLM studio.
            // The three dots communicate an active dialogue without implying
            // a cloud provider or external brand.
            ctx.beginPath()
            ctx.moveTo(cx - 8 * u, cy - 7 * u)
            ctx.lineTo(cx + 7 * u, cy - 7 * u)
            ctx.quadraticCurveTo(cx + 9 * u, cy - 7 * u, cx + 9 * u, cy - 5 * u)
            ctx.lineTo(cx + 9 * u, cy + 4 * u)
            ctx.quadraticCurveTo(cx + 9 * u, cy + 6 * u, cx + 7 * u, cy + 6 * u)
            ctx.lineTo(cx - 1 * u, cy + 6 * u)
            ctx.lineTo(cx - 6 * u, cy + 10 * u)
            ctx.lineTo(cx - 5 * u, cy + 6 * u)
            ctx.lineTo(cx - 8 * u, cy + 6 * u)
            ctx.quadraticCurveTo(cx - 10 * u, cy + 6 * u, cx - 10 * u, cy + 4 * u)
            ctx.lineTo(cx - 10 * u, cy - 5 * u)
            ctx.quadraticCurveTo(cx - 10 * u, cy - 7 * u, cx - 8 * u, cy - 7 * u)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(cx - 5 * u, cy, 1.2 * u, 0, Math.PI * 2, false)
            ctx.arc(cx, cy, 1.2 * u, 0, Math.PI * 2, false)
            ctx.arc(cx + 5 * u, cy, 1.2 * u, 0, Math.PI * 2, false)
            ctx.fill()
        } else if (name === "spark") {
            line(ctx, cx, cy - 9 * u, cx, cy + 9 * u)
            line(ctx, cx - 9 * u, cy, cx + 9 * u, cy)
            line(ctx, cx - 5 * u, cy - 5 * u, cx + 5 * u, cy + 5 * u)
            line(ctx, cx + 5 * u, cy - 5 * u, cx - 5 * u, cy + 5 * u)
        } else if (name === "search") {
            circle(ctx, cx - 2 * u, cy - 2 * u, 6 * u)
            line(ctx, cx + 3 * u, cy + 3 * u, cx + 9 * u, cy + 9 * u)
        } else if (name === "globe") {
            circle(ctx, cx, cy, 8 * u)
            ctx.beginPath()
            ctx.moveTo(cx, cy - 8 * u)
            ctx.bezierCurveTo(cx - 4 * u, cy - 5 * u, cx - 4 * u, cy + 5 * u, cx, cy + 8 * u)
            ctx.bezierCurveTo(cx + 4 * u, cy + 5 * u, cx + 4 * u, cy - 5 * u, cx, cy - 8 * u)
            ctx.stroke()
            line(ctx, cx - 8 * u, cy, cx + 8 * u, cy)
            ctx.beginPath()
            ctx.moveTo(cx - 6 * u, cy - 5 * u)
            ctx.quadraticCurveTo(cx, cy - 3 * u, cx + 6 * u, cy - 5 * u)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx - 6 * u, cy + 5 * u)
            ctx.quadraticCurveTo(cx, cy + 3 * u, cx + 6 * u, cy + 5 * u)
            ctx.stroke()
        } else if (name === "translate") {
            // Paired writing systems make translation distinct from audio
            // alignment while remaining legible at compact sidebar sizes.
            ctx.beginPath()
            ctx.moveTo(cx - 10 * u, cy + 7 * u)
            ctx.lineTo(cx - 6 * u, cy - 7 * u)
            ctx.lineTo(cx - 2 * u, cy + 7 * u)
            ctx.stroke()
            line(ctx, cx - 8.5 * u, cy + 2 * u, cx - 3.5 * u, cy + 2 * u)

            line(ctx, cx + 1 * u, cy - 5 * u, cx + 10 * u, cy - 5 * u)
            line(ctx, cx + 5.5 * u, cy - 8 * u, cx + 5.5 * u, cy - 5 * u)
            line(ctx, cx + 2 * u, cy - 1 * u, cx + 9 * u, cy - 1 * u)
            ctx.beginPath()
            ctx.moveTo(cx + 3 * u, cy - 1 * u)
            ctx.quadraticCurveTo(cx + 5.5 * u, cy + 4 * u, cx + 10 * u, cy + 7 * u)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx + 8.5 * u, cy - 1 * u)
            ctx.quadraticCurveTo(cx + 6.5 * u, cy + 4 * u, cx + 1 * u, cy + 7 * u)
            ctx.stroke()
        } else if (name === "download") {
            line(ctx, cx, cy - 8 * u, cx, cy + 4 * u)
            ctx.beginPath()
            ctx.moveTo(cx - 5 * u, cy)
            ctx.lineTo(cx, cy + 5 * u)
            ctx.lineTo(cx + 5 * u, cy)
            ctx.stroke()
            line(ctx, cx - 8 * u, cy + 9 * u, cx + 8 * u, cy + 9 * u)
        } else if (name === "activity") {
            ctx.beginPath()
            ctx.moveTo(cx - 10 * u, cy)
            ctx.lineTo(cx - 6 * u, cy)
            ctx.lineTo(cx - 4 * u, cy - 6 * u)
            ctx.lineTo(cx, cy + 7 * u)
            ctx.lineTo(cx + 3 * u, cy - 3 * u)
            ctx.lineTo(cx + 5 * u, cy)
            ctx.lineTo(cx + 10 * u, cy)
            ctx.stroke()
        } else if (name === "star") {
            ctx.beginPath()
            ctx.moveTo(cx, cy - 9 * u)
            ctx.lineTo(cx + 3 * u, cy - 3 * u)
            ctx.lineTo(cx + 9 * u, cy - 2 * u)
            ctx.lineTo(cx + 5 * u, cy + 3 * u)
            ctx.lineTo(cx + 6 * u, cy + 9 * u)
            ctx.lineTo(cx, cy + 6 * u)
            ctx.lineTo(cx - 6 * u, cy + 9 * u)
            ctx.lineTo(cx - 5 * u, cy + 3 * u)
            ctx.lineTo(cx - 9 * u, cy - 2 * u)
            ctx.lineTo(cx - 3 * u, cy - 3 * u)
            ctx.closePath()
            ctx.stroke()
        } else if (name === "trash") {
            line(ctx, cx - 7 * u, cy - 6 * u, cx + 7 * u, cy - 6 * u)
            line(ctx, cx - 3 * u, cy - 9 * u, cx + 3 * u, cy - 9 * u)
            line(ctx, cx - 2 * u, cy - 9 * u, cx + 2 * u, cy - 9 * u)
            ctx.strokeRect(cx - 6 * u, cy - 4 * u, 12 * u, 13 * u)
            line(ctx, cx - 2 * u, cy - 1 * u, cx - 2 * u, cy + 6 * u)
            line(ctx, cx + 2 * u, cy - 1 * u, cx + 2 * u, cy + 6 * u)
        } else if (name === "more-horizontal") {
            ctx.beginPath()
            ctx.arc(cx - 6 * u, cy, 1.5 * u, 0, Math.PI * 2, false)
            ctx.arc(cx, cy, 1.5 * u, 0, Math.PI * 2, false)
            ctx.arc(cx + 6 * u, cy, 1.5 * u, 0, Math.PI * 2, false)
            ctx.fill()
        } else if (name === "more-vertical") {
            ctx.beginPath()
            ctx.arc(cx, cy - 6 * u, 1.5 * u, 0, Math.PI * 2, false)
            ctx.arc(cx, cy, 1.5 * u, 0, Math.PI * 2, false)
            ctx.arc(cx, cy + 6 * u, 1.5 * u, 0, Math.PI * 2, false)
            ctx.fill()
        } else if (name === "check") {
            ctx.beginPath()
            ctx.moveTo(cx - 7 * u, cy)
            ctx.lineTo(cx - 2 * u, cy + 5 * u)
            ctx.lineTo(cx + 8 * u, cy - 6 * u)
            ctx.stroke()
        } else if (name === "volume") {
            ctx.beginPath()
            ctx.moveTo(cx - 9 * u, cy - 4 * u)
            ctx.lineTo(cx - 4 * u, cy - 4 * u)
            ctx.lineTo(cx + 2 * u, cy - 9 * u)
            ctx.lineTo(cx + 2 * u, cy + 9 * u)
            ctx.lineTo(cx - 4 * u, cy + 4 * u)
            ctx.lineTo(cx - 9 * u, cy + 4 * u)
            ctx.closePath()
            ctx.stroke()
            line(ctx, cx + 6 * u, cy - 5 * u, cx + 8 * u, cy + 5 * u)
        } else if (name === "mic") {
            ctx.strokeRect(cx - 4 * u, cy - 9 * u, 8 * u, 12 * u)
            line(ctx, cx - 8 * u, cy - 1 * u, cx - 8 * u, cy)
            line(ctx, cx + 8 * u, cy - 1 * u, cx + 8 * u, cy)
            ctx.beginPath()
            ctx.arc(cx, cy, 8 * u, 0, Math.PI, false)
            ctx.stroke()
            line(ctx, cx, cy + 8 * u, cx, cy + 11 * u)
            line(ctx, cx - 5 * u, cy + 11 * u, cx + 5 * u, cy + 11 * u)
        } else if (name === "file") {
            ctx.beginPath()
            ctx.moveTo(cx - 7 * u, cy - 9 * u)
            ctx.lineTo(cx + 2 * u, cy - 9 * u)
            ctx.lineTo(cx + 7 * u, cy - 4 * u)
            ctx.lineTo(cx + 7 * u, cy + 9 * u)
            ctx.lineTo(cx - 7 * u, cy + 9 * u)
            ctx.closePath()
            ctx.stroke()
            line(ctx, cx + 2 * u, cy - 9 * u, cx + 2 * u, cy - 4 * u)
            line(ctx, cx + 2 * u, cy - 4 * u, cx + 7 * u, cy - 4 * u)
        } else if (name === "code") {
            ctx.beginPath()
            ctx.moveTo(cx - 4 * u, cy - 7 * u)
            ctx.lineTo(cx - 10 * u, cy)
            ctx.lineTo(cx - 4 * u, cy + 7 * u)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(cx + 4 * u, cy - 7 * u)
            ctx.lineTo(cx + 10 * u, cy)
            ctx.lineTo(cx + 4 * u, cy + 7 * u)
            ctx.stroke()

            line(ctx, cx + 1 * u, cy - 9 * u, cx - 1 * u, cy + 9 * u)
        } else if (name === "gallery") {
            ctx.strokeRect(cx - 8 * u, cy - 7 * u, 6 * u, 6 * u)
            ctx.strokeRect(cx + 2 * u, cy - 7 * u, 6 * u, 6 * u)
            ctx.strokeRect(cx - 8 * u, cy + 3 * u, 6 * u, 6 * u)
            ctx.strokeRect(cx + 2 * u, cy + 3 * u, 6 * u, 6 * u)
        } else if (name === "cpu") {
            ctx.strokeRect(cx - 6 * u, cy - 6 * u, 12 * u, 12 * u)
            for (var i = -6; i <= 6; i += 4) {
                line(ctx, cx + i * u, cy - 10 * u, cx + i * u, cy - 6 * u)
                line(ctx, cx + i * u, cy + 6 * u, cx + i * u, cy + 10 * u)
                line(ctx, cx - 10 * u, cy + i * u, cx - 6 * u, cy + i * u)
                line(ctx, cx + 6 * u, cy + i * u, cx + 10 * u, cy + i * u)
            }
        } else if (name === "folder") {
            ctx.beginPath()
            ctx.moveTo(cx - 8 * u, cy - 6 * u)
            ctx.lineTo(cx - 3 * u, cy - 6 * u)
            ctx.lineTo(cx - 1 * u, cy - 8 * u)
            ctx.lineTo(cx + 8 * u, cy - 8 * u)
            ctx.lineTo(cx + 8 * u, cy + 6 * u)
            ctx.lineTo(cx - 8 * u, cy + 6 * u)
            ctx.closePath()
            ctx.stroke()
        } else if (name === "users") {
            // Left user
            circle(ctx, cx - 4 * u, cy - 3 * u, 3 * u)
            ctx.beginPath()
            ctx.arc(cx - 4 * u, cy + 7 * u, 6 * u, Math.PI, Math.PI * 2, false)
            ctx.stroke()
            // Right user
            circle(ctx, cx + 4 * u, cy - 3 * u, 3 * u)
            ctx.beginPath()
            ctx.arc(cx + 4 * u, cy + 7 * u, 6 * u, Math.PI, Math.PI * 2, false)
            ctx.stroke()
        } else if (name === "reload") {
            // VS Code Codicon "refresh" geometry, used for model reload actions.
            // Keep this separate from our two-arrow "refresh" glyph, which
            // communicates list/data synchronization.
            var v = s / 16
            var ox = cx - 8 * v
            var oy = cy - 8 * v
            function px(x) { return ox + x * v }
            function py(y) { return oy + y * v }

            ctx.beginPath()
            ctx.moveTo(px(3), py(8))
            ctx.bezierCurveTo(px(3), py(5.23858), px(5.23858), py(3), px(8), py(3))
            ctx.bezierCurveTo(px(9.63527), py(3), px(11.0878), py(3.78495), px(12.0005), py(5))
            ctx.lineTo(px(10), py(5))
            ctx.bezierCurveTo(px(9.72386), py(5), px(9.5), py(5.22386), px(9.5), py(5.5))
            ctx.bezierCurveTo(px(9.5), py(5.77614), px(9.72386), py(6), px(10), py(6))
            ctx.lineTo(px(12.8904), py(6))
            ctx.bezierCurveTo(px(12.8973), py(6.00014), px(12.9041), py(6.00014), px(12.911), py(6))
            ctx.lineTo(px(13), py(6))
            ctx.bezierCurveTo(px(13.2761), py(6), px(13.5), py(5.77614), px(13.5), py(5.5))
            ctx.lineTo(px(13.5), py(2.5))
            ctx.bezierCurveTo(px(13.5), py(2.22386), px(13.2761), py(2), px(13), py(2))
            ctx.bezierCurveTo(px(12.7239), py(2), px(12.5), py(2.22386), px(12.5), py(2.5))
            ctx.lineTo(px(12.5), py(4.03138))
            ctx.bezierCurveTo(px(11.4009), py(2.78613), px(9.79253), py(2), px(8), py(2))
            ctx.bezierCurveTo(px(4.68629), py(2), px(2), py(4.68629), px(2), py(8))
            ctx.bezierCurveTo(px(2), py(11.3137), px(4.68629), py(14), px(8), py(14))
            ctx.bezierCurveTo(px(11.1301), py(14), px(13.6999), py(11.6035), px(13.9756), py(8.54488))
            ctx.bezierCurveTo(px(14.0003), py(8.26985), px(13.7975), py(8.0268), px(13.5225), py(8.00202))
            ctx.bezierCurveTo(px(13.2474), py(7.97723), px(13.0044), py(8.1801), px(12.9796), py(8.45512))
            ctx.bezierCurveTo(px(12.75), py(11.003), px(10.6079), py(13), px(8), py(13))
            ctx.bezierCurveTo(px(5.23858), py(13), px(3), py(10.7614), px(3), py(8))
            ctx.closePath()
            ctx.fill()
        } else if (name === "refresh") {
            ctx.beginPath()
            ctx.arc(cx, cy, 7 * u, -Math.PI * 0.2, Math.PI * 1.05)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx - 5 * u, cy + 7 * u)
            ctx.lineTo(cx - 1 * u, cy + 6 * u)
            ctx.lineTo(cx - 3 * u, cy + 10 * u)
            ctx.closePath()
            ctx.fill()

            ctx.beginPath()
            ctx.arc(cx, cy, 7 * u, Math.PI * 0.8, Math.PI * 2.0)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx + 5 * u, cy - 7 * u)
            ctx.lineTo(cx + 1 * u, cy - 6 * u)
            ctx.lineTo(cx + 3 * u, cy - 10 * u)
            ctx.closePath()
            ctx.fill()
        } else if (name === "run-again") {
            // A rerun combines the repeat arrow with a play marker so it is
            // visually distinct from reloading a model.
            ctx.beginPath()
            ctx.arc(cx, cy, 7 * u, Math.PI * 0.15, Math.PI * 1.72)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx - 8 * u, cy - 1 * u)
            ctx.lineTo(cx - 4 * u, cy - 2 * u)
            ctx.lineTo(cx - 6 * u, cy - 6 * u)
            ctx.closePath()
            ctx.fill()
            ctx.beginPath()
            ctx.moveTo(cx + 1 * u, cy - 4 * u)
            ctx.lineTo(cx + 7 * u, cy)
            ctx.lineTo(cx + 1 * u, cy + 4 * u)
            ctx.closePath()
            ctx.fill()
        } else if (name === "eject") {
            // Arrow pointing up
            ctx.beginPath()
            ctx.moveTo(cx - 5 * u, cy - 2 * u)
            ctx.lineTo(cx, cy - 8 * u)
            ctx.lineTo(cx + 5 * u, cy - 2 * u)
            ctx.stroke()
            // Bottom line
            line(ctx, cx - 8 * u, cy + 8 * u, cx + 8 * u, cy + 8 * u)
        } else if (name === "power") {
            ctx.beginPath()
            ctx.arc(cx, cy + 1 * u, 7 * u, -Math.PI * 0.18, Math.PI * 1.18, false)
            ctx.stroke()
            line(ctx, cx, cy - 9 * u, cx, cy - 1 * u)
        } else if (name === "history") {
            ctx.beginPath()
            ctx.arc(cx, cy, 7 * u, -Math.PI * 0.45, Math.PI * 1.35, false)
            ctx.stroke()
            var angle = -Math.PI * 0.45
            var ax = cx + 7 * u * Math.cos(angle)
            var ay = cy + 7 * u * Math.sin(angle)
            ctx.beginPath()
            ctx.moveTo(ax - 2 * u, ay - 3 * u)
            ctx.lineTo(ax, ay)
            ctx.lineTo(ax + 3 * u, ay - 1 * u)
            ctx.stroke()
            line(ctx, cx, cy, cx, cy - 4 * u)
            line(ctx, cx, cy, cx + 3 * u, cy)
        } else if (name === "copy") {
            ctx.beginPath()
            ctx.moveTo(cx - 2 * u, cy - 6 * u)
            ctx.lineTo(cx - 6 * u, cy - 6 * u)
            ctx.lineTo(cx - 6 * u, cy + 2 * u)
            ctx.stroke()

            ctx.beginPath()
            ctx.rect(cx - 2 * u, cy - 2 * u, 8 * u, 8 * u)
            ctx.stroke()
        } else if (name === "waves") {
            line(ctx, cx - 8 * u, cy - 1 * u, cx - 8 * u, cy + 1 * u)
            line(ctx, cx - 4 * u, cy - 5 * u, cx - 4 * u, cy + 5 * u)
            line(ctx, cx, cy - 8 * u, cx, cy + 8 * u)
            line(ctx, cx + 4 * u, cy - 5 * u, cx + 4 * u, cy + 5 * u)
            line(ctx, cx + 8 * u, cy - 1 * u, cx + 8 * u, cy + 1 * u)
        } else if (name === "dubbing") {
            // Dialogue bubble + compact waveform: localized speech synced to media.
            ctx.beginPath()
            ctx.moveTo(cx - 9 * u, cy - 8 * u)
            ctx.lineTo(cx + 9 * u, cy - 8 * u)
            ctx.lineTo(cx + 9 * u, cy + 5 * u)
            ctx.lineTo(cx + 2 * u, cy + 5 * u)
            ctx.lineTo(cx - 3 * u, cy + 9 * u)
            ctx.lineTo(cx - 3 * u, cy + 5 * u)
            ctx.lineTo(cx - 9 * u, cy + 5 * u)
            ctx.closePath()
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(cx - 6 * u, cy - 1 * u)
            ctx.lineTo(cx - 4 * u, cy - 1 * u)
            ctx.lineTo(cx - 2 * u, cy - 5 * u)
            ctx.lineTo(cx + 1 * u, cy + 2 * u)
            ctx.lineTo(cx + 3 * u, cy - 4 * u)
            ctx.lineTo(cx + 5 * u, cy - 1 * u)
            ctx.lineTo(cx + 6 * u, cy - 1 * u)
            ctx.stroke()
        } else if (name === "voice-isolator") {
            // Original LA Studio wave-split mark: a compact waveform enters
            // a center gate and exits as two open, separated channels.
            line(ctx, cx - 9 * u, cy - 2 * u, cx - 6 * u, cy - 2 * u)
            line(ctx, cx - 6 * u, cy - 2 * u, cx - 4 * u, cy - 7 * u)
            line(ctx, cx - 4 * u, cy - 7 * u, cx - 2 * u, cy + 7 * u)
            line(ctx, cx - 2 * u, cy + 7 * u, cx, cy)
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.quadraticCurveTo(cx + 4 * u, cy - 6 * u, cx + 9 * u, cy - 6 * u)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.quadraticCurveTo(cx + 4 * u, cy + 6 * u, cx + 9 * u, cy + 6 * u)
            ctx.stroke()
        } else if (name === "external-link") {
            ctx.beginPath()
            ctx.moveTo(cx - 2 * u, cy - 8 * u)
            ctx.lineTo(cx - 8 * u, cy - 8 * u)
            ctx.lineTo(cx - 8 * u, cy + 8 * u)
            ctx.lineTo(cx + 8 * u, cy + 8 * u)
            ctx.lineTo(cx + 8 * u, cy + 2 * u)
            ctx.stroke()
            line(ctx, cx - 2 * u, cy + 2 * u, cx + 8 * u, cy - 8 * u)
            ctx.beginPath()
            ctx.moveTo(cx + 3 * u, cy - 8 * u)
            ctx.lineTo(cx + 8 * u, cy - 8 * u)
            ctx.lineTo(cx + 8 * u, cy - 3 * u)
            ctx.stroke()
        } else {
            circle(ctx, cx, cy, 7 * u)
        }
    }
}
