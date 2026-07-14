import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Item {
    id: root

    property string activePlayingPath: ""

    signal loadDescriptionRequested(string description, string text)

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (!AppController.player.playing) {
                root.activePlayingPath = ""
            }
        }
        function onPlaybackFinished() {
            root.activePlayingPath = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Text {
                text: qsTr("Generation History")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
                Layout.fillWidth: true
            }

            Button {
                id: clearAllBtn
                visible: AppController.history.voiceDesignHistory.length > 0
                implicitWidth: 68
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
                    color: clearAllBtn.hovered ? Qt.rgba(239, 83, 80, 0.15) : "transparent"
                    border.color: clearAllBtn.hovered ? Qt.rgba(239, 83, 80, 0.35) : "transparent"
                    border.width: 1
                    radius: 6
                }
                onClicked: AppController.history.clearVoiceDesignHistory()
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }

        // Empty state
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: AppController.history.voiceDesignHistory.length === 0
            spacing: Theme.paddingMedium
            Layout.topMargin: 40

            LineIcon {
                name: "history"
                color: Theme.textSecondary
                opacity: 0.35
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
            }

            Text {
                text: qsTr("No history yet")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.alignment: Qt.AlignHCenter
            }
        }

        // List View
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            visible: AppController.history.voiceDesignHistory.length > 0

            ColumnLayout {
                width: parent.width - 8
                spacing: Theme.paddingSmall

                Repeater {
                    model: AppController.history.voiceDesignHistory

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        implicitHeight: itemLayout.implicitHeight + Theme.paddingSmall * 2
                        radius: Theme.radiusMedium
                        color: Qt.rgba(1, 1, 1, 0.015)
                        border.color: root.activePlayingPath === modelData.filePath ? Theme.accent : Qt.rgba(1, 1, 1, 0.04)
                        border.width: 1

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }

                        RowLayout {
                            id: itemLayout
                            anchors.fill: parent
                            anchors.margins: Theme.paddingMedium
                            spacing: Theme.paddingSmall

                            // Play/Stop Button
                            Button {
                                id: playBtn
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                flat: true

                                contentItem: LineIcon {
                                    name: root.activePlayingPath === modelData.filePath ? "stop" : "play"
                                    color: playBtn.hovered ? Theme.accentLight : Theme.textPrimary
                                    anchors.centerIn: parent
                                    width: 14
                                    height: 14
                                }
                                background: Rectangle {
                                    color: playBtn.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.15) : Qt.rgba(1, 1, 1, 0.03)
                                    radius: 8
                                }
                                onClicked: {
                                    if (root.activePlayingPath === modelData.filePath) {
                                        AppController.player.stop()
                                        root.activePlayingPath = ""
                                    } else {
                                        AppController.player.stop()
                                        AppController.player.playUrl(modelData.filePath)
                                        root.activePlayingPath = modelData.filePath
                                    }
                                }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: modelData.voiceDescription
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: qsTr("Text: %1").arg(modelData.text)
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall - 1
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: Theme.paddingSmall
                                    Text {
                                        text: modelData.durationText + " • " + (modelData.modelName || "")
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall - 2
                                    }
                                    Text {
                                        text: modelData.timestamp
                                        color: Theme.textSecondary
                                        opacity: 0.7
                                        font.pixelSize: Theme.fontSmall - 2
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }

                            // Actions
                            RowLayout {
                                visible: itemMouse.containsMouse
                                spacing: 2

                                Button {
                                    id: loadBtn
                                    implicitWidth: 26
                                    implicitHeight: 26
                                    flat: true
                                    background: Rectangle { color: loadBtn.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"; radius: 5 }
                                    contentItem: LineIcon {
                                        name: "spark"
                                        color: Theme.accentLight
                                        anchors.centerIn: parent
                                        width: 14
                                        height: 14
                                    }
                                    onClicked: root.loadDescriptionRequested(modelData.voiceDescription, modelData.text)
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    AppToolTip { text: qsTr("Load voice parameters") }
                                }

                                Button {
                                    id: deleteBtn
                                    implicitWidth: 26
                                    implicitHeight: 26
                                    flat: true
                                    background: Rectangle { color: deleteBtn.hovered ? Qt.rgba(239, 83, 80, 0.15) : "transparent"; radius: 5 }
                                    contentItem: LineIcon {
                                        name: "trash"
                                        color: deleteBtn.hovered ? Theme.danger : Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 14
                                        height: 14
                                    }
                                    onClicked: AppController.history.deleteVoiceDesignHistoryItem(modelData.id)
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    AppToolTip { text: qsTr("Delete item") }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
