import QtQuick
import QtQuick.Controls
import LAStudio

ToolTip {
    id: control

    delay: 350
    
    // Modern compact padding
    topPadding: 5
    bottomPadding: 6
    leftPadding: 9
    rightPadding: 9

    contentItem: Text {
        text: control.text
        font.pixelSize: Theme.fontSmall
        font.family: "Segoe UI"
        color: Theme.textPrimary
    }

    background: Rectangle {
        color: Theme.surface
        border.color: Theme.surfaceAlt
        border.width: 1
        radius: 6
    }
}
