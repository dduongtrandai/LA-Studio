import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../base"

ColumnLayout {
    id: root

    property string activeTab: "file"
    property string audioLabel: qsTr("Audio file")
    property string audioHint: qsTr("WAV, MP3, FLAC supported")
    property string fileDialogTitle: qsTr("Select Audio File")
    property var fileNameFilters: [qsTr("Audio files (*.wav *.mp3 *.flac)"), qsTr("All files (*)")]
    property bool showSystemSource: true
    property bool showMicrophone: true
    property bool busy: false
    property bool recording: false
    property bool saving: false
    property real recordingLevel: 0.0

    signal audioSelected(string path)
    signal startRecordingRequested(bool systemAudio)
    signal stopRecordingRequested()

    spacing: Theme.paddingMedium

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: tabRow.implicitWidth + 6
        implicitHeight: 40
        radius: Theme.radiusSmall
        color: Qt.rgba(1, 1, 1, 0.035)
        border.color: Qt.rgba(1, 1, 1, 0.07)
        border.width: 1

        RowLayout {
            id: tabRow
            anchors.centerIn: parent
            spacing: 4

            AppTabButton {
                text: qsTr("File")
                iconName: "file"
                selected: root.activeTab === "file"
                onClicked: if (!root.recording && !root.busy) root.activeTab = "file"
            }

            AppTabButton {
                visible: root.showMicrophone
                text: qsTr("Mic")
                iconName: "mic"
                selected: root.activeTab === "mic"
                onClicked: if (!root.recording && !root.busy) root.activeTab = "mic"
            }

            AppTabButton {
                visible: root.showSystemSource
                text: qsTr("System")
                iconName: "volume"
                selected: root.activeTab === "system"
                onClicked: if (!root.recording && !root.busy) root.activeTab = "system"
            }
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 70
        currentIndex: root.activeTab === "file" ? 0 : (root.activeTab === "mic" && root.showMicrophone ? 1 : 2)

        Item {
            DropArea {
                id: fileDropArea
                anchors.fill: parent
                enabled: !root.recording && !root.busy
                keys: ["text/uri-list"]

                onDropped: function(drop) {
                    if (drop.hasUrls && drop.urls.length > 0) {
                        root.audioSelected(AppController.files.urlToLocalPath(drop.urls[0].toString()))
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                border.color: Theme.accent
                border.width: 1
                radius: Theme.radiusSmall
                visible: fileDropArea.containsDrag
            }

            RowLayout {
                anchors.centerIn: parent
                spacing: Theme.paddingMedium

                SourceIcon { iconName: "file" }

                ColumnLayout {
                    spacing: 3

                    Text {
                        text: root.audioLabel
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                    }

                    Text {
                        text: root.audioHint
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }
                }

                PrimaryButton {
                    text: qsTr("Choose file")
                    iconName: "file"
                    implicitWidth: 128
                    implicitHeight: 34
                    enabled: !root.recording && !root.busy
                    onClicked: fileDialogLoader.active = true
                }
            }
        }

        RecorderPane {
            visible: root.showMicrophone
            title: root.saving ? qsTr("Saving...") : (root.recording ? qsTr("Recording microphone") : qsTr("Record microphone"))
            iconName: "mic"
            recording: root.recording
            meterVisible: root.recording && root.activeTab === "mic"
            interactive: !root.busy || root.recording
            onToggleRecording: {
                if (root.recording) root.stopRecordingRequested()
                else root.startRecordingRequested(false)
            }
        }

        RecorderPane {
            title: root.saving ? qsTr("Saving...") : (root.recording ? qsTr("Recording system audio") : qsTr("Record system audio"))
            iconName: "volume"
            recording: root.recording
            meterVisible: root.recording && root.activeTab === "system"
            interactive: !root.busy || root.recording
            onToggleRecording: {
                if (root.recording) root.stopRecordingRequested()
                else root.startRecordingRequested(true)
            }
        }
    }

    Loader {
        id: fileDialogLoader
        active: false
        sourceComponent: fileDialogComponent
    }

    Component {
        id: fileDialogComponent

        FileDialog {
            title: root.fileDialogTitle
            nameFilters: root.fileNameFilters

            Component.onCompleted: open()

            onAccepted: {
                root.audioSelected(AppController.files.urlToLocalPath(selectedFile.toString()))
                fileDialogLoader.active = false
            }
            onRejected: fileDialogLoader.active = false
        }
    }

    component SourceIcon: Rectangle {
        property string iconName: "file"

        implicitWidth: 44
        implicitHeight: 44
        radius: 22
        color: Qt.rgba(0.49, 0.30, 1.0, 0.12)
        border.color: Qt.rgba(0.49, 0.30, 1.0, 0.26)
        border.width: 1

        LineIcon {
            anchors.centerIn: parent
            name: parent.iconName
            color: Theme.accentLight
            width: 19
            height: 19
        }
    }

    component RecorderPane: Item {
        id: recorderPane

        property string title: qsTr("Record")
        property string iconName: "mic"
        property bool recording: false
        property bool meterVisible: false
        property bool interactive: true

        signal toggleRecording()

        RowLayout {
            anchors.centerIn: parent
            spacing: Theme.paddingMedium

            Rectangle {
                width: 44
                height: 44
                radius: 22
                color: recorderPane.recording ? Qt.rgba(0.94, 0.33, 0.31, 0.18) : Qt.rgba(0.49, 0.30, 1.0, 0.12)
                border.color: recorderPane.recording ? Theme.danger : Qt.rgba(0.49, 0.30, 1.0, 0.35)
                border.width: 1
                opacity: recorderPane.interactive ? 1.0 : 0.5

                LineIcon {
                    anchors.centerIn: parent
                    name: recorderPane.recording ? "stop" : recorderPane.iconName
                    color: recorderPane.recording ? Theme.danger : Theme.accentLight
                    width: 19
                    height: 19
                }

                TapHandler {
                    enabled: recorderPane.interactive
                    onTapped: recorderPane.toggleRecording()
                }
                HoverHandler {
                    enabled: recorderPane.interactive
                    cursorShape: Qt.PointingHandCursor
                }
            }

            ColumnLayout {
                spacing: 6

                Text {
                    text: recorderPane.title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }

                Rectangle {
                    Layout.preferredWidth: 260
                    height: 4
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.10)
                    visible: recorderPane.meterVisible

                    Rectangle {
                        width: parent.width * Math.min(root.recordingLevel * 5, 1.0)
                        height: parent.height
                        radius: 2
                        color: Theme.success
                        Behavior on width { NumberAnimation { duration: 50 } }
                    }
                }
            }
        }
    }
}
