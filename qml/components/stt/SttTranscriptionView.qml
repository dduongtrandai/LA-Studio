import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
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

                Text {
                    visible: sttSession && sttSession.segments && sttSession.segments.length > 0
                    text: sttSession ? qsTr("%1 cues").arg(sttSession.segments.length) : ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }

                Button {
                    visible: sttSession && sttSession.segments && sttSession.segments.length > 0
                    text: qsTr("Export SRT")
                    onClicked: srtDialog.open()
                }

                Button {
                    visible: sttSession && sttSession.segments && sttSession.segments.length > 0
                    text: qsTr("Export VTT")
                    onClicked: vttDialog.open()
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

                ListView {
                    id: cueList
                    anchors.fill: parent
                    visible: sttSession && sttSession.segments && sttSession.segments.length > 0
                    model: sttSession ? sttSession.segments : []
                    spacing: Theme.paddingSmall
                    delegate: Rectangle {
                        width: cueList.width
                        height: cueColumn.implicitHeight + Theme.paddingMedium * 2
                        color: Theme.surfaceAlt
                        radius: Theme.radiusSmall
                        property var cue: modelData
                        ColumnLayout {
                            id: cueColumn
                            anchors.fill: parent
                            anchors.margins: Theme.paddingSmall
                            spacing: Theme.paddingSmall
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: startField
                                    text: formatCueTime(cue.startMs)
                                    Layout.preferredWidth: 105
                                    onEditingFinished: if (sttSession) sttSession.updateSegment(cue.id, parseCueTime(text), cue.endMs, cueText.text)
                                }
                                Text { text: "→"; color: Theme.textSecondary }
                                TextField {
                                    id: endField
                                    text: formatCueTime(cue.endMs)
                                    Layout.preferredWidth: 105
                                    onEditingFinished: if (sttSession) sttSession.updateSegment(cue.id, cue.startMs, parseCueTime(text), cueText.text)
                                }
                                Item { Layout.fillWidth: true }
                                Text { text: cue.timingSource || "asr"; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                            }
                            TextArea {
                                id: cueText
                                Layout.fillWidth: true
                                text: cue.text || cue.sourceText || ""
                                color: Theme.textPrimary
                                wrapMode: Text.Wrap
                                background: null
                                onEditingFinished: if (sttSession) sttSession.updateSegment(cue.id, cue.startMs, cue.endMs, text)
                            }
                        }
                    }
                }

                TextArea {
                    id: transcriptArea
                    text: sttSession ? sttSession.transcript : ""
                    visible: !(sttSession && sttSession.segments && sttSession.segments.length > 0)
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

    function formatCueTime(ms) {
        var total = Math.max(0, Number(ms) || 0)
        var s = Math.floor(total / 1000)
        var h = Math.floor(s / 3600); s %= 3600
        var m = Math.floor(s / 60); s %= 60
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s + "." + (total % 1000 < 100 ? "0" : "") + (total % 1000 < 10 ? "0" : "") + (total % 1000)
    }

    function parseCueTime(value) {
        var parts = String(value).trim().replace(",", ".").split(":")
        if (parts.length !== 3) return -1
        var sec = Number(parts[2])
        if (!isFinite(sec)) return -1
        return Math.round((Number(parts[0]) * 3600 + Number(parts[1]) * 60 + sec) * 1000)
    }

    FileDialog {
        id: srtDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["SubRip subtitle (*.srt)"]
        onAccepted: if (sttSession) sttSession.exportSrt(selectedFile)
    }
    FileDialog {
        id: vttDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["WebVTT subtitle (*.vtt)"]
        onAccepted: if (sttSession) sttSession.exportVtt(selectedFile)
    }
}
