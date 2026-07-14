import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Slider {
    id: slider

    implicitHeight: 30

    background: Rectangle {
        x: slider.leftPadding
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.availableWidth
        height: 4
        radius: 2
        color: Qt.rgba(1, 1, 1, 0.10)

        Rectangle {
            width: slider.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Qt.rgba(0.49, 0.30, 1.0, 0.78)
        }
    }

    handle: Rectangle {
        x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.pressed ? 20 : 18
        height: width
        radius: width / 2
        color: "#f4f2ff"
        border.color: slider.hovered || slider.pressed ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.16)
        border.width: 1

        Behavior on width { NumberAnimation { duration: 100 } }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
