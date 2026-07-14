import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

ColumnLayout {
    spacing: Theme.paddingXL + 4

    property var sttSession: null
    property alias transcriptText: transcriptArea.text

    // --- Transcription Output ---
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 300
        color: Theme.surface
        radius: Theme.radiusLarge
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingXL
            spacing: Theme.paddingMedium

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Transcription")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                    Layout.fillWidth: true
                }
                
                Button {
                    id: copyBtn
                    visible: transcriptArea.text.length > 0
                    implicitWidth: 32
                    implicitHeight: 32
                    flat: true
                    
                    AppToolTip {
                        text: qsTr("Copy transcription")
                        visible: parent.hovered
                    }
                    
                    contentItem: LineIcon {
                        name: "copy"
                        color: copyBtn.hovered ? Theme.accent : Theme.textSecondary
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                    }
                    background: Rectangle {
                        color: copyBtn.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                        border.color: copyBtn.hovered ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                        border.width: 1
                        radius: 6
                    }
                    onClicked: {
                        if (sttSession) {
                            sttSession.copyTranscript()
                        }
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }

                Button {
                    id: clearBtn
                    visible: transcriptArea.text.length > 0
                    implicitWidth: 32
                    implicitHeight: 32
                    flat: true
                    
                    AppToolTip {
                        text: qsTr("Clear transcription")
                        visible: parent.hovered
                    }
                    
                    contentItem: LineIcon {
                        name: "trash"
                        color: clearBtn.hovered ? Theme.danger : Theme.textSecondary
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                    }
                    background: Rectangle {
                        color: clearBtn.hovered ? Qt.rgba(0.937, 0.325, 0.314, 0.08) : "transparent"
                        border.color: clearBtn.hovered ? Qt.rgba(0.937, 0.325, 0.314, 0.15) : "transparent"
                        border.width: 1
                        radius: 6
                    }
                    onClicked: {
                        if (sttSession) {
                            sttSession.clearTranscript()
                        }
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                TextArea {
                    id: transcriptArea
                    text: sttSession ? sttSession.transcript : ""
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    wrapMode: Text.Wrap
                    readOnly: true
                    placeholderText: qsTr("Transcript will appear here after processing...")
                    placeholderTextColor: Theme.textSecondary
                    background: null
                    padding: 0
                    selectByMouse: true
                }
            }
        }
    }
}
