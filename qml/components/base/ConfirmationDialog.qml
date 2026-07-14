import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Dialog {
    id: root

    property string titleText: qsTr("Confirm Action")
    property string messageText: qsTr("Are you sure you want to proceed?")
    property string confirmText: qsTr("Confirm")
    property string cancelText: qsTr("Cancel")
    property bool isDestructive: false

    signal confirmed()
    signal cancelled()

    width: Math.min(420, parent ? parent.width - 32 : 420)
    height: contentLayout.implicitHeight + Theme.paddingLarge * 2
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    padding: Theme.paddingLarge
    title: ""

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Theme.surfaceAlt
        border.width: 1
        
        // Add a soft drop shadow effect using border gradient / opacity
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.4)
            border.width: 1
            radius: Theme.radiusMedium + 1
            z: -1
        }
    }

    contentItem: ColumnLayout {
        id: contentLayout
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            LineIcon {
                name: root.isDestructive ? "trash" : "settings"
                color: root.isDestructive ? Theme.danger : Theme.accentLight
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                strokeWidth: 2.0
            }

            Text {
                Layout.fillWidth: true
                text: root.titleText
                color: root.isDestructive ? Theme.danger : Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.messageText
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            lineHeight: 1.15
        }

        Item {
            Layout.preferredHeight: Theme.paddingSmall
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight
            spacing: Theme.paddingSmall

            PrimaryButton {
                text: root.cancelText
                quiet: true
                implicitWidth: 90
                implicitHeight: 32
                onClicked: {
                    root.cancelled()
                    root.reject()
                }
            }

            PrimaryButton {
                text: root.confirmText
                implicitWidth: 100
                implicitHeight: 32
                buttonColor: root.isDestructive ? Theme.danger : Theme.accent
                onClicked: {
                    root.confirmed()
                    root.accept()
                }
            }
        }
    }
}
