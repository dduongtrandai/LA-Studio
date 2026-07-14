import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Rectangle {
    color: Theme.background

    property var searchResults: []
    property var selectedModel: null

    Connections {
        target: AppController.hub
        function onSearchFinished(models) {
            searchResults = models
            if (models.length > 0 && !selectedModel) {
                // selectedModel = models[0] // Optional: auto-select first
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar (Search & Results)
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 400
            color: Theme.background
            border.color: Theme.surfaceAlt
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingLarge
                spacing: Theme.paddingLarge

                SearchBar {
                    id: searchBar
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search models...")
                    onAccepted: doSearch()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    CheckBox {
                        id: whisperOnlyCheck
                        text: qsTr("Whisper.cpp compatible only")
                        checked: false
                        onCheckedChanged: doSearch()
                        
                        contentItem: Text {
                            text: whisperOnlyCheck.text
                            font: whisperOnlyCheck.font
                            color: Theme.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: whisperOnlyCheck.indicator.width + whisperOnlyCheck.spacing
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ComboBox {
                        id: taskFilter
                        model: [qsTr("All Tasks"), "automatic-speech-recognition", "text-to-speech",
                                "text-to-audio", "audio-to-audio"]
                        implicitWidth: 150
                        implicitHeight: 30
                        onActivated: doSearch()
                        
                        background: Rectangle {
                            radius: 4
                            color: Theme.surfaceAlt
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("%1 models").arg(searchResults.length)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }
                    Item { Layout.fillWidth: true }
                }

                // Loading indicator
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: AppController.hub.searching
                    visible: AppController.hub.searching
                }

                ListView {
                    id: resultsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4
                    clip: true
                    model: searchResults

                    delegate: ModelListItem {
                        width: ListView.view.width
                        itemData: modelData
                        selected: selectedModel && selectedModel.id === itemData.id
                        onClicked: selectedModel = itemData
                    }

                    // Empty state
                    Text {
                        anchors.centerIn: parent
                        visible: !AppController.hub.searching && searchResults.length === 0
                        text: qsTr("Enter a search query to find models")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontMedium
                    }
                }
            }
        }

        // Main Content (Details)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.background
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingXL
                spacing: Theme.paddingLarge

                // Active downloads
                Repeater {
                    model: AppController.downloads.activeDownloads
                    DownloadProgressBar {
                        Layout.fillWidth: true
                        modelId: modelData.identifier
                        filename: modelData.filename
                        bytesReceived: modelData.bytesReceived
                        bytesTotal: modelData.bytesTotal
                    }
                }

                ModelDetailsView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    modelData: selectedModel
                    onDownloadRequested: function(filename) {
                        var dest = AppController.models.concreteModelDir(selectedModel.id)
                        AppController.downloads.enqueue(selectedModel.id, filename, dest)
                    }
                }
            }
        }
    }

    function doSearch() {
        var task = taskFilter.currentIndex > 0 ? taskFilter.currentText : ""
        AppController.hub.searchModels(searchBar.text, task, whisperOnlyCheck.checked)
    }
}

