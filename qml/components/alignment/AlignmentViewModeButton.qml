import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Button {
    id: root
    property string iconName: "waves"
    checkable: true
    implicitHeight: 28
    padding: 0
    contentItem: Item {
        LineIcon {
            id: modeIcon
            width: 14
            height: 14
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: modeLabel.left
            anchors.rightMargin: 6
            name: root.iconName
            color: root.checked ? Theme.textPrimary : Theme.textSecondary
        }
        Text {
            id: modeLabel
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - modeIcon.width - 6)
            text: root.text
            color: root.checked ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            font.bold: root.checked
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        radius: 5
        color: root.checked ? Theme.surfaceAlt : (root.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
        border.color: root.checked ? Qt.rgba(1, 1, 1, 0.10) : "transparent"
        border.width: 1
    }
}
