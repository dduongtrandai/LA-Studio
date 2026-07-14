import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../base"

Dialog {
    id: root

    property string familyId: ""
    property string initialMode: "reference"
    property string currentReferenceAudioPath: ""
    property string currentReferenceText: ""
    property string currentDesignName: ""
    property string currentDesignDescription: ""
    property var referenceVoices: []
    property var designVoices: []
    property string activeMode: initialMode
    property string editingId: ""
    property string editingKind: ""
    property string pendingDeleteId: ""
    property string pendingDeleteKind: ""

    signal referenceVoiceSelected(string audioPath, string referenceText, string name)
    signal designVoiceSelected(string description, string name)

    title: ""
    modal: true
    width: Math.min(920, parent ? parent.width - Theme.paddingXL * 2 : 920)
    height: Math.min(700, parent ? parent.height - Theme.paddingXL * 2 : 700)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    padding: 0

    onOpened: {
        activeMode = initialMode
        refresh()
        resetEditor()
    }

    onFamilyIdChanged: refresh()

    Connections {
        target: AppController.voiceClonePresets
        function onPresetsChanged(famId) {
            if (famId === root.familyId)
                root.refresh()
        }
    }

    Connections {
        target: AppController.voiceDesignPresets
        function onPresetsChanged(famId) {
            if (famId === root.familyId)
                root.refresh()
        }
    }

    function refresh() {
        referenceVoices = familyId !== "" ? AppController.voiceClonePresets.presetsForFamily(familyId) : []
        designVoices = familyId !== "" ? AppController.voiceDesignPresets.presetsForFamily(familyId) : []
    }

    function resetEditor() {
        editingId = ""
        editingKind = activeMode
        voiceNameField.text = ""
        refAudioPathField.text = ""
        refTextField.text = ""
        designDescriptionField.text = ""
    }

    function editReferenceVoice(item) {
        activeMode = "reference"
        editingKind = "reference"
        editingId = item.id || ""
        voiceNameField.text = item.name || ""
        refAudioPathField.text = item.audioPath || ""
        refTextField.text = item.referenceText || ""
    }

    function editDesignVoice(item) {
        activeMode = "design"
        editingKind = "design"
        editingId = item.id || ""
        voiceNameField.text = item.name || ""
        designDescriptionField.text = item.description || ""
    }

    function saveReferenceVoice() {
        var name = voiceNameField.text.trim()
        var audioPath = refAudioPathField.text.trim()
        if (name === "" || audioPath === "") return
        if (editingKind === "reference" && editingId !== "") {
            AppController.voiceClonePresets.updatePreset(editingId, name, audioPath, refTextField.text)
        } else {
            AppController.voiceClonePresets.addPreset(familyId, name, audioPath, refTextField.text)
        }
        resetEditor()
    }

    function saveDesignVoice() {
        var name = voiceNameField.text.trim()
        var description = designDescriptionField.text.trim()
        if (name === "" || description === "") return
        if (editingKind === "design" && editingId !== "") {
            AppController.voiceDesignPresets.updatePreset(editingId, name, description)
        } else {
            AppController.voiceDesignPresets.addPreset(familyId, name, description)
        }
        resetEditor()
    }

    function requestDelete(kind, id) {
        pendingDeleteKind = kind
        pendingDeleteId = id
        deleteDialog.open()
    }

    function applyCurrentReference() {
        activeMode = "reference"
        editingKind = "reference"
        editingId = ""
        voiceNameField.text = currentReferenceAudioPath !== "" ? VoiceCloningUtils.fileNameFromPath(currentReferenceAudioPath) : ""
        refAudioPathField.text = currentReferenceAudioPath
        refTextField.text = currentReferenceText
    }

    function applyCurrentDesign() {
        activeMode = "design"
        editingKind = "design"
        editingId = ""
        voiceNameField.text = currentDesignName
        designDescriptionField.text = currentDesignDescription
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Theme.surfaceAlt
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            Layout.leftMargin: Theme.paddingLarge
            Layout.rightMargin: Theme.paddingLarge
            spacing: Theme.paddingMedium

            LineIcon {
                name: "users"
                color: Theme.accentLight
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("Voice Library")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
                Text {
                    text: qsTr("Manage reusable reference voices and voice design presets for this model.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }

            PrimaryButton {
                text: qsTr("Close")
                quiet: true
                implicitWidth: 84
                implicitHeight: 34
                onClicked: root.close()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                color: Qt.rgba(1, 1, 1, 0.02)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingMedium
                    spacing: Theme.paddingSmall

                    AppTabButton {
                        Layout.fillWidth: true
                        text: qsTr("Reference Voices")
                        iconName: "volume"
                        selected: root.activeMode === "reference"
                        onClicked: {
                            root.activeMode = "reference"
                            root.resetEditor()
                        }
                    }

                    AppTabButton {
                        Layout.fillWidth: true
                        text: qsTr("Designed Voices")
                        iconName: "spark"
                        selected: root.activeMode === "design"
                        onClicked: {
                            root.activeMode = "design"
                            root.resetEditor()
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt; Layout.topMargin: Theme.paddingSmall; Layout.bottomMargin: Theme.paddingSmall }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: root.activeMode === "reference" ? qsTr("Use current reference") : qsTr("Use current description")
                        iconName: "save"
                        quiet: true
                        enabled: root.activeMode === "reference" ? root.currentReferenceAudioPath !== "" : root.currentDesignDescription.trim() !== ""
                        onClicked: root.activeMode === "reference" ? root.applyCurrentReference() : root.applyCurrentDesign()
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.activeMode === "reference"
                              ? qsTr("Reference voices store audio plus transcript.")
                              : qsTr("Designed voices store reusable text descriptions.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.topMargin: Theme.paddingSmall
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.surfaceAlt }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Theme.paddingLarge
                spacing: Theme.paddingMedium

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.activeMode === "reference" ? 0 : 1

                    VoiceList {
                        items: root.referenceVoices
                        emptyText: qsTr("No reference voices saved yet.")
                        secondaryRole: "originalAudioName"
                        onUseRequested: function(item) {
                            root.referenceVoiceSelected(item.audioPath || "", item.referenceText || "", item.name || "")
                            root.close()
                        }
                        onEditRequested: function(item) { root.editReferenceVoice(item) }
                        onDeleteRequested: function(item) { root.requestDelete("reference", item.id || "") }
                        onPlayRequested: function(item) {
                            if (item.audioPath)
                                AppController.player.playFile(item.audioPath)
                        }
                    }

                    VoiceList {
                        items: root.designVoices
                        emptyText: qsTr("No designed voices saved yet.")
                        secondaryRole: "description"
                        onUseRequested: function(item) {
                            root.designVoiceSelected(item.description || "", item.name || "")
                            root.close()
                        }
                        onEditRequested: function(item) { root.editDesignVoice(item) }
                        onDeleteRequested: function(item) { root.requestDelete("design", item.id || "") }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: editorLayout.implicitHeight + Theme.paddingMedium * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(1, 1, 1, 0.025)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    ColumnLayout {
                        id: editorLayout
                        anchors.fill: parent
                        anchors.margins: Theme.paddingMedium
                        spacing: Theme.paddingSmall

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.paddingSmall
                            Text {
                                Layout.fillWidth: true
                                text: root.editingId === "" ? qsTr("Add Voice") : qsTr("Edit Voice")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontMedium
                                font.bold: true
                            }
                            PrimaryButton {
                                text: qsTr("New")
                                quiet: true
                                implicitWidth: 70
                                implicitHeight: 30
                                onClicked: root.resetEditor()
                            }
                        }

                        TextField {
                            id: voiceNameField
                            Layout.fillWidth: true
                            implicitHeight: 36
                            placeholderText: qsTr("Voice name")
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: Theme.surface
                                border.color: voiceNameField.activeFocus ? Theme.accent : Theme.surfaceAlt
                                border.width: 1
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.paddingSmall
                            visible: root.activeMode === "reference"

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.paddingSmall
                                TextField {
                                    id: refAudioPathField
                                    Layout.fillWidth: true
                                    implicitHeight: 36
                                    placeholderText: qsTr("Reference audio path")
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: Theme.surface
                                        border.color: refAudioPathField.activeFocus ? Theme.accent : Theme.surfaceAlt
                                        border.width: 1
                                    }
                                }
                                PrimaryButton {
                                    text: qsTr("Browse")
                                    iconName: "folder"
                                    quiet: true
                                    implicitWidth: 100
                                    implicitHeight: 36
                                    onClicked: refFileDialog.open()
                                }
                            }

                            AppTextArea {
                                id: refTextField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 92
                                placeholderText: qsTr("Reference transcript")
                                font.pixelSize: Theme.fontSmall
                            }
                        }

                        AppTextArea {
                            id: designDescriptionField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 112
                            visible: root.activeMode === "design"
                            placeholderText: qsTr("Voice description")
                            font.pixelSize: Theme.fontSmall
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.paddingSmall
                            Item { Layout.fillWidth: true }
                            PrimaryButton {
                                text: qsTr("Save")
                                iconName: "save"
                                implicitWidth: 110
                                implicitHeight: 36
                                enabled: root.activeMode === "reference"
                                         ? (voiceNameField.text.trim() !== "" && refAudioPathField.text.trim() !== "")
                                         : (voiceNameField.text.trim() !== "" && designDescriptionField.text.trim() !== "")
                                onClicked: root.activeMode === "reference" ? root.saveReferenceVoice() : root.saveDesignVoice()
                            }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: refFileDialog
        title: qsTr("Select Reference Audio")
        nameFilters: ["Audio files (*.wav *.mp3 *.flac *.m4a *.ogg)", "All files (*)"]
        onAccepted: refAudioPathField.text = AppController.files.urlToLocalPath(selectedFile.toString())
    }

    ConfirmationDialog {
        id: deleteDialog
        parent: Overlay.overlay
        titleText: qsTr("Delete Voice")
        messageText: qsTr("This saved voice will be removed from the local library.")
        confirmText: qsTr("Delete")
        isDestructive: true
        onConfirmed: {
            if (root.pendingDeleteKind === "reference")
                AppController.voiceClonePresets.deletePreset(root.pendingDeleteId)
            else if (root.pendingDeleteKind === "design")
                AppController.voiceDesignPresets.deletePreset(root.pendingDeleteId)
            root.pendingDeleteId = ""
            root.pendingDeleteKind = ""
            root.resetEditor()
        }
    }

    component VoiceList: Item {
        id: listRoot
        property var items: []
        property string emptyText: ""
        property string secondaryRole: ""
        signal useRequested(var item)
        signal editRequested(var item)
        signal deleteRequested(var item)
        signal playRequested(var item)

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.paddingSmall

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                visible: listRoot.items.length > 0

                ColumnLayout {
                    width: parent.width
                    spacing: Theme.paddingSmall

                    Repeater {
                        model: listRoot.items

                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: itemLayout.implicitHeight + Theme.paddingMedium * 2
                            radius: Theme.radiusSmall
                            color: itemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.02)
                            border.color: itemMouse.containsMouse ? Qt.rgba(0.49, 0.30, 1.0, 0.35) : Qt.rgba(1, 1, 1, 0.07)
                            border.width: 1

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: listRoot.useRequested(modelData)
                            }

                            RowLayout {
                                id: itemLayout
                                anchors.fill: parent
                                anchors.margins: Theme.paddingMedium
                                spacing: Theme.paddingMedium

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name || qsTr("Untitled voice")
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMedium
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData[listRoot.secondaryRole] || ""
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        elide: Text.ElideRight
                                        maximumLineCount: 2
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                RowLayout {
                                    spacing: Theme.paddingSmall
                                    PrimaryButton {
                                        text: qsTr("Use")
                                        implicitWidth: 70
                                        implicitHeight: 32
                                        onClicked: listRoot.useRequested(modelData)
                                    }
                                    PrimaryButton {
                                        text: qsTr("Play")
                                        quiet: true
                                        visible: root.activeMode === "reference"
                                        implicitWidth: 70
                                        implicitHeight: 32
                                        onClicked: listRoot.playRequested(modelData)
                                    }
                                    PrimaryButton {
                                        text: qsTr("Edit")
                                        quiet: true
                                        implicitWidth: 70
                                        implicitHeight: 32
                                        onClicked: listRoot.editRequested(modelData)
                                    }
                                    PrimaryButton {
                                        text: qsTr("Delete")
                                        quiet: true
                                        buttonColor: Theme.danger
                                        implicitWidth: 80
                                        implicitHeight: 32
                                        onClicked: listRoot.deleteRequested(modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: listRoot.items.length === 0
                spacing: Theme.paddingMedium
                LineIcon {
                    name: "users"
                    color: Theme.textSecondary
                    opacity: 0.35
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                }
                Text {
                    text: listRoot.emptyText
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }
}
