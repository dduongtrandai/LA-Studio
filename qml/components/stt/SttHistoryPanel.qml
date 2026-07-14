import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

ColumnLayout {
    id: root

    spacing: Theme.paddingMedium
    Layout.fillWidth: true
    Layout.fillHeight: true

    property var sttSession: null

    signal closeRequested()
    signal loadHistoryRequested(string text, string filePath)

    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 36
        spacing: Theme.paddingSmall

        Button {
            id: closeBtn
            implicitWidth: 30
            implicitHeight: 30
            flat: true

            AppToolTip {
                text: qsTr("Hide history")
                visible: parent.hovered
            }

            contentItem: LineIcon {
                name: "chevron-left"
                color: closeBtn.hovered ? Theme.accent : Theme.textSecondary
                anchors.centerIn: parent
                width: 16
                height: 16
            }
            background: Rectangle {
                color: closeBtn.hovered ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.025)
                border.color: closeBtn.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.55) : Qt.rgba(1, 1, 1, 0.08)
                border.width: 1
                radius: 7
            }
            onClicked: root.closeRequested()
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }

        LineIcon {
            name: "history"
            color: Theme.accent
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }

        Text {
            text: qsTr("Transcription History")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.bold: true
            Layout.fillWidth: true
        }

        Button {
            id: clearAllBtn
            visible: sttSession ? (sttSession.history.length > 0) : false
            implicitWidth: 64
            implicitHeight: 28
            flat: true

            contentItem: Text {
                text: qsTr("Clear All")
                color: clearAllBtn.hovered ? Theme.danger : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: clearAllBtn.hovered ? Qt.rgba(0.937, 0.325, 0.314, 0.1) : "transparent"
                border.color: clearAllBtn.hovered ? Qt.rgba(0.937, 0.325, 0.314, 0.3) : "transparent"
                border.width: 1
                radius: 5
            }
            onClicked: {
                if (sttSession) {
                    sttSession.clearHistory()
                }
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - Theme.paddingLarge * 2
            visible: sttSession ? (sttSession.history.length === 0) : true
            spacing: Theme.paddingMedium

            LineIcon {
                name: "history"
                color: Theme.textSecondary
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                opacity: 0.5
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No history yet")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Your transcribed audio clips will appear here.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
        }

        ListView {
            id: historyList
            anchors.fill: parent
            visible: sttSession ? (sttSession.history.length > 0) : false
            model: sttSession ? sttSession.history : []
            spacing: Theme.paddingSmall
            clip: true

            delegate: Rectangle {
                id: itemRect
                width: historyList.width
                height: itemCol.implicitHeight + Theme.paddingMedium * 2
                radius: Theme.radiusSmall
                color: mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(1, 1, 1, 0.015)
                border.color: mouseArea.containsMouse ? Qt.rgba(0.49, 0.30, 1.0, 0.25) : Qt.rgba(1, 1, 1, 0.06)
                border.width: 1

                property bool isCurrentlyPlaying: sttSession ? (sttSession.playbackPath === modelData.filePath) : false

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.loadHistoryRequested(modelData.text, modelData.filePath)
                    }
                }

                ColumnLayout {
                    id: itemCol
                    anchors.fill: parent
                    anchors.margins: Theme.paddingMedium
                    spacing: Theme.paddingSmall

                    Text {
                        Layout.fillWidth: true
                        text: modelData.text
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: modelData.modelName
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall - 1
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: modelData.timestamp
                                color: Qt.rgba(1, 1, 1, 0.35)
                                font.pixelSize: Theme.fontSmall - 2
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Rectangle {
                            implicitWidth: durationText.implicitWidth + 8
                            implicitHeight: 18
                            radius: 4
                            color: Qt.rgba(0.49, 0.30, 1.0, 0.1)
                            border.color: Qt.rgba(0.49, 0.30, 1.0, 0.2)
                            border.width: 1

                            Text {
                                id: durationText
                                anchors.centerIn: parent
                                text: modelData.durationText
                                color: Theme.accentLight
                                font.pixelSize: Theme.fontSmall - 1
                                font.bold: true
                            }
                        }

                        Button {
                            id: playBtn
                            implicitWidth: 28
                            implicitHeight: 28
                            flat: true

                            contentItem: LineIcon {
                                name: itemRect.isCurrentlyPlaying ? "stop" : "play"
                                color: itemRect.isCurrentlyPlaying ? Theme.danger : (playBtn.hovered ? Theme.success : Theme.textPrimary)
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                            }
                            background: Rectangle {
                                color: playBtn.hovered ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.03)
                                border.color: playBtn.hovered ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                                border.width: 1
                                radius: 6
                            }
                            onClicked: {
                                if (sttSession) {
                                    if (itemRect.isCurrentlyPlaying) {
                                        sttSession.stopPlayback()
                                    } else {
                                        sttSession.playHistoryFile(modelData.filePath)
                                    }
                                }
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }

                        Button {
                            id: deleteBtn
                            implicitWidth: 28
                            implicitHeight: 28
                            flat: true

                            contentItem: LineIcon {
                                name: "trash"
                                color: deleteBtn.hovered ? Theme.danger : Theme.textSecondary
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                            }
                            background: Rectangle {
                                color: deleteBtn.hovered ? Qt.rgba(0.937, 0.325, 0.314, 0.1) : Qt.rgba(1, 1, 1, 0.03)
                                border.color: deleteBtn.hovered ? Qt.rgba(0.937, 0.325, 0.314, 0.2) : Qt.rgba(1, 1, 1, 0.05)
                                border.width: 1
                                radius: 6
                            }
                            onClicked: {
                                if (sttSession) {
                                    sttSession.deleteHistoryItem(modelData.id)
                                }
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }
                    }
                }
            }
        }
    }
}
