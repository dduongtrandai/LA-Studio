import QtQuick
import QtQuick.Layouts
import LAStudio

ColumnLayout {
    property string title: ""
    property string detail: ""
    spacing: 2

    Text {
        Layout.fillWidth: true
        text: parent.title
        color: Theme.textPrimary
        font.pixelSize: Theme.fontLarge
        font.bold: true
    }
    Text {
        Layout.fillWidth: true
        text: parent.detail
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.Wrap
    }
}
