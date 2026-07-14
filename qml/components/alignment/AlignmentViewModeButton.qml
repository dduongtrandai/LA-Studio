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
    contentItem: RowLayout {
        spacing: 6
        LineIcon { Layout.preferredWidth: 14; Layout.preferredHeight: 14; name: root.iconName; color: root.checked ? Theme.textPrimary : Theme.textSecondary }
        Text { Layout.fillWidth: true; text: root.text; color: root.checked ? Theme.textPrimary : Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: root.checked; horizontalAlignment: Text.AlignHCenter }
    }
    background: Rectangle {
        radius: 5
        color: root.checked ? Theme.surfaceAlt : (root.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
        border.color: root.checked ? Qt.rgba(1, 1, 1, 0.10) : "transparent"
        border.width: 1
    }
}
