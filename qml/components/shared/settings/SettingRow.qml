import QtQuick
import QtQuick.Layouts
import LAStudio

RowLayout {
    id: root
    property string label: ""
    property string description: ""
    property real labelWidth: 280
    
    default property alias control: controlContainer.data

    Layout.fillWidth: true
    spacing: Theme.paddingLarge

    ColumnLayout {
        Layout.preferredWidth: root.labelWidth
        Layout.maximumWidth: root.labelWidth
        Layout.fillWidth: false
        Layout.alignment: Qt.AlignTop
        spacing: 4

        Text {
            text: root.label
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.bold: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: root.description
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            visible: root.description !== ""
            opacity: 0.85
        }
    }

    ColumnLayout {
        id: controlContainer
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        spacing: 0
    }
}
