import QtQuick
import QtQuick.Layouts
import LAStudio

ColumnLayout {
    id: root
    property string label: ""
    property string value: ""
    property color valueColor: Theme.textPrimary
    spacing: 1
    Text { text: root.label; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true }
    Text { text: root.value; color: root.valueColor; font.pixelSize: Theme.fontSmall; font.bold: true }
}
