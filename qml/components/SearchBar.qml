import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root

    property alias text: input.text
    property alias placeholderText: input.placeholderText
    signal accepted()

    implicitHeight: 44
    radius: Theme.radiusMedium
    color: Theme.surfaceAlt

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.paddingMedium
        anchors.rightMargin: Theme.paddingSmall
        spacing: Theme.paddingSmall

        Text {
            text: "\u{1F50D}"
            font.pixelSize: Theme.iconSize
            verticalAlignment: Text.AlignVCenter
        }

        TextField {
            id: input
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.textPrimary
            placeholderTextColor: Theme.textSecondary
            font.pixelSize: Theme.fontMedium
            background: Item {}
            verticalAlignment: Text.AlignVCenter
            onAccepted: root.accepted()
        }

        PrimaryButton {
            text: qsTr("Search")
            implicitWidth: 80
            implicitHeight: 34
            onClicked: root.accepted()
        }
    }
}

