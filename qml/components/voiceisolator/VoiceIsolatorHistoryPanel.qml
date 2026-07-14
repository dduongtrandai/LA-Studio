import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

ColumnLayout {
    id: root
    property var isolator: null
    signal closeRequested()
    spacing: Theme.paddingMedium

    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 36
        spacing: Theme.paddingSmall

        Button {
            id: closeButton
            implicitWidth: 30
            implicitHeight: 30
            flat: true
            contentItem: LineIcon { anchors.centerIn: parent; name: "chevron-left"; color: closeButton.hovered ? Theme.accent : Theme.textSecondary; width: 16; height: 16 }
            background: Rectangle { radius: 7; color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.025); border.color: Qt.rgba(1, 1, 1, 0.08) }
            onClicked: root.closeRequested()
        }
        LineIcon { name: "history"; color: Theme.accent; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
        Text { text: qsTr("Isolation History"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; Layout.fillWidth: true }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - Theme.paddingLarge * 2
            visible: !root.isolator || root.isolator.recentResults.length === 0
            spacing: Theme.paddingMedium
            LineIcon { name: "history"; color: Theme.textSecondary; opacity: 0.5; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 32; Layout.preferredHeight: 32 }
            Text { Layout.fillWidth: true; text: qsTr("No results yet"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; horizontalAlignment: Text.AlignHCenter }
            Text { Layout.fillWidth: true; text: qsTr("Separated vocals and background stems will appear here."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        }

        ListView {
            anchors.fill: parent
            model: root.isolator ? root.isolator.recentResults : []
            spacing: Theme.paddingSmall
            clip: true
            delegate: Rectangle {
                id: resultCard
                width: ListView.view.width
                height: 88
                radius: Theme.radiusSmall
                color: resultMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(1, 1, 1, 0.015)
                border.color: resultMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30) : Qt.rgba(1, 1, 1, 0.06)
                MouseArea { id: resultMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.isolator.openRecent(modelData.vocalsPath, modelData.backgroundPath) }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingMedium
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        LineIcon { name: "voice-isolator"; color: Theme.accent; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                        Text { text: qsTr("Two-stem result"); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true; Layout.fillWidth: true }
                    }
                    Text { Layout.fillWidth: true; text: modelData.sourceHash || ""; color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideMiddle }
                    Text { Layout.fillWidth: true; text: qsTr("Vocals + Background"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                }
            }
        }
    }
}
