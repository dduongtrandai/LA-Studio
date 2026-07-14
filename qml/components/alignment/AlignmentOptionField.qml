import QtQuick
import QtQuick.Layouts
import LAStudio

ColumnLayout {
    property string label: ""
    default property alias content: contentLayout.data
    spacing: Theme.paddingSmall

    Text {
        text: parent.label
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        font.bold: true
    }
    ColumnLayout {
        id: contentLayout
        Layout.fillWidth: true
    }
}
