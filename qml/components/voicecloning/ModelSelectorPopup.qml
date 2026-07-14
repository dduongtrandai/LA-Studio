import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

// Model selector popup
Popup {
    id: root

    width: 600
    height: 400
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 0

    property string currentModelName: ""
    property var selectedModel: null
    property string runtimeId: ""
    property var currentModelList: []

    signal modelSelected(var modelInfo)

    function updateModelList() {
        // We use an explicit function to avoid QML's flaky block-binding dependency tracking
        var currentVersion = AppController.models.version; // just to read it
        currentModelList = AppController.models.filteredVoiceCloneModels(
            modelSearchField.text,
            root.runtimeId !== "" ? root.runtimeId : AppController.settings.selectedTtsRuntime
        );
    }

    onRuntimeIdChanged: updateModelList()
    onOpened: updateModelList()

    Connections {
        target: AppController.models
        function onVersionChanged() {
            root.updateModelList()
        }
    }

    background: Rectangle {
        color: Theme.background
        radius: Theme.radiusLarge
        border.color: Theme.surfaceAlt
        border.width: 1
        clip: true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Search bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium

                TextField {
                    id: modelSearchField
                    Layout.fillWidth: true
                    placeholderText: "Filter models..."
                    color: Theme.textPrimary
                    background: null

                    font.pixelSize: Theme.fontMedium

                    onTextChanged: {
                        if (text.length > 0) {
                            AppLogger.debug("voicecloning", "Filtering voice models with query: '" + text + "'")
                        }
                        root.updateModelList()
                    }
                }

                Text {
                    text: "✕"
                    color: Theme.textSecondary
                    visible: modelSearchField.text.length > 0
                    font.pixelSize: Theme.fontMedium

                    TapHandler {
                        onTapped: {
                            AppLogger.debug("voicecloning", "Clearing voice model search filter")
                            modelSearchField.text = ""
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.surfaceAlt
        }

        // Models list
        ListView {
            id: modelsListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            model: root.currentModelList

            delegate: Rectangle {
                width: ListView.view.width
                height: 60
                color: itemHover.hovered ? Theme.surfaceAlt : "transparent"

                HoverHandler {
                    id: itemHover
                }

                 TapHandler {
                    onTapped: {
                        var m = modelsListView.model[index]
                        var mainFile = m.path
                        var tokenizerFile = m.tokenizerPath || ""
                        
                        AppLogger.info("voicecloning", "Popup Tapped. root.runtimeId: '" + root.runtimeId + "' | settings runtime: '" + AppController.settings.selectedTtsRuntime + "'");

                        var runtimeAsset = root.runtimeId ? root.runtimeId : AppController.settings.selectedTtsRuntime
                        var runtimePath = runtimeAsset ? AppController.runtimes.getRuntimePath(runtimeAsset) : ""

                        AppLogger.info("voicecloning", "User selected voice model: '" + m.displayName + "' | Path: " + mainFile + " | Tokenizer: " + (tokenizerFile || "None") + " | Runtime: " + runtimeAsset + " (" + runtimePath + ")")

                        root.currentModelName = m.displayName
                        root.selectedModel = m

                        AppController.tts.loadModel(mainFile, tokenizerFile, runtimePath)
                        root.modelSelected(m)
                        root.close()
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingMedium
                    spacing: Theme.paddingMedium

                    Rectangle {
                        width: 36
                        height: 36
                        radius: 18
                        color: Theme.accent
                        opacity: 0.1

                        Text {
                            anchors.centerIn: parent
                            text: "🎙️"
                            font.pixelSize: 18
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: modelData.displayName
                            color: Theme.textPrimary
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: modelData.author + " • " + (modelData.size / (1024*1024)).toFixed(1) + " MB"
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }
                    }
                }
            }

            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
