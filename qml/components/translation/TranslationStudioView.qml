import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LAStudio
import "../shared"
import "../base"

StudioShell {
    id: root
    property var translation: AppController.translation
    property string editorViewMode: "bilingual"
    property string pendingHistoryDeleteId: ""
    readonly property bool showSourceEditor: editorViewMode !== "translation"
    readonly property bool showTranslationEditor: editorViewMode !== "source"
    property var family: {
        if (!studioController) return null
        var families = studioController.families
        for (var i = 0; i < families.length; ++i)
            if (families[i].id === studioController.selectedFamilyId) return families[i]
        return null
    }

    families: studioController ? studioController.families : []
    capability: "translation"
    studioTitle: qsTr("Translation Studio")
    studioIconName: "translate"
    studioReady: studioController ? studioController.studioReady : false
    selectedFamilyId: studioController ? studioController.selectedFamilyId : ""
    modalSelectionMode: true
    showSwitcher: false
    showLeftPanel: true
    modalSelectionTitle: family ? family.title : qsTr("Model + Runtime")
    modalSelectionValue: studioController ? studioController.runtimeDisplayText : qsTr("Select model and runtime")
    modalSelectionDetail: studioController ? studioController.statusDetail : ""
    backToolTip: qsTr("Change model and runtime")
    signal backToGallery()

    onRequestBack: root.backToGallery()
    onRequestConfigurationPicker: root.backToGallery()
    onRequestReload: if (studioController) studioController.reload()
    onRequestEject: if (studioController) studioController.unload()

    leftPanelContent: [
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.paddingMedium
            RowLayout {
                Layout.fillWidth: true
                Button {
                    id: closeHistoryButton
                    implicitWidth: 30
                    implicitHeight: 30
                    flat: true

                    AppToolTip {
                        text: qsTr("Hide history")
                        visible: parent.hovered
                    }

                    contentItem: LineIcon {
                        name: "chevron-left"
                        color: closeHistoryButton.hovered ? Theme.accent : Theme.textSecondary
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                    }

                    background: Rectangle {
                        radius: 7
                        color: closeHistoryButton.hovered ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.025)
                        border.color: closeHistoryButton.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.55) : Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                    }

                    onClicked: root.isLeftPanelOpen = false
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
                LineIcon { name: "history"; color: Theme.accent; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
                Text { text: qsTr("Translation History"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; Layout.fillWidth: true }
                PrimaryButton {
                    visible: translation.history.length > 0
                    text: qsTr("Clear")
                    iconName: "trash"
                    quiet: true
                    textColor: Theme.danger
                    borderColor: Qt.rgba(0.94, 0.33, 0.31, 0.32)
                    implicitWidth: 72
                    implicitHeight: 30
                    onClicked: clearHistoryDialog.open()
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.07) }
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                ColumnLayout {
                    anchors.centerIn: parent; width: parent.width - Theme.paddingLarge * 2; spacing: Theme.paddingSmall
                    visible: translation.history.length === 0
                    LineIcon { name: "history"; color: Theme.textSecondary; opacity: 0.6; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 30; Layout.preferredHeight: 30 }
                    Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Project workspace"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; wrapMode: Text.WordWrap }
                    Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Save projects to return to translated segments later."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap }
                }
                ListView {
                    anchors.fill: parent; visible: translation.history.length > 0; model: translation.history; spacing: Theme.paddingSmall; clip: true
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        width: parent.width; height: entry.implicitHeight + Theme.paddingMedium * 2; radius: Theme.radiusSmall; color: Qt.rgba(1, 1, 1, 0.025); border.color: Qt.rgba(1, 1, 1, 0.07); border.width: 1
                        ColumnLayout {
                            id: entry; anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: 3
                            Text { Layout.fillWidth: true; text: modelData.sourcePreview || ""; color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; maximumLineCount: 2; elide: Text.ElideRight; wrapMode: Text.WordWrap }
                            Text { Layout.fillWidth: true; text: (modelData.sourceLanguage || "") + " → " + (modelData.targetLanguage || "") + " · " + (modelData.timestamp || ""); color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                PrimaryButton {
                                    text: qsTr("Open")
                                    iconName: "edit"
                                    quiet: true
                                    textColor: Theme.accentLight
                                    borderColor: Qt.rgba(0.49, 0.30, 1.0, 0.38)
                                    implicitWidth: 88
                                    implicitHeight: 32
                                    enabled: !translation.processing
                                    onClicked: {
                                        if (translation.loadHistoryItem(modelData.id || ""))
                                            root.isLeftPanelOpen = false
                                    }
                                }
                                PrimaryButton {
                                    text: qsTr("Delete")
                                    iconName: "trash"
                                    quiet: true
                                    textColor: Theme.danger
                                    borderColor: Qt.rgba(0.94, 0.33, 0.31, 0.38)
                                    implicitWidth: 88
                                    implicitHeight: 32
                                    onClicked: {
                                        root.pendingHistoryDeleteId = modelData.id || ""
                                        deleteHistoryDialog.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    ]

    ConfirmationDialog {
        id: deleteHistoryDialog
        parent: Overlay.overlay
        titleText: qsTr("Delete history item")
        messageText: qsTr("This saved translation snapshot will be permanently removed from history.")
        confirmText: qsTr("Delete")
        isDestructive: true
        onConfirmed: {
            translation.deleteHistoryItem(root.pendingHistoryDeleteId)
            root.pendingHistoryDeleteId = ""
        }
        onRejected: root.pendingHistoryDeleteId = ""
    }

    ConfirmationDialog {
        id: clearHistoryDialog
        parent: Overlay.overlay
        titleText: qsTr("Clear translation history")
        messageText: qsTr("All saved translation snapshots will be permanently removed.")
        confirmText: qsTr("Clear all")
        isDestructive: true
        onConfirmed: translation.clearHistory()
    }

    mainContent: [
        StackLayout {
            anchors.fill: parent
            currentIndex: 1
            Item {
                ColumnLayout {
                    anchors.centerIn: parent; width: Math.min(520, parent.width - Theme.paddingXL * 2); spacing: Theme.paddingLarge
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 178; radius: Theme.radiusMedium; color: Theme.surface; border.color: Qt.rgba(1,1,1,0.08); border.width: 1
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.paddingLarge; spacing: Theme.paddingMedium
                            LineIcon { name: "gallery"; color: Theme.accent; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                            Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Select a Translation model and runtime"); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true; wrapMode: Text.WordWrap }
                            Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Translation stays local and uses the installed translation runtime."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap }
                        }
                    }
                    PrimaryButton { Layout.fillWidth: true; text: qsTr("Choose model and runtime"); iconName: "gallery"; onClicked: root.backToGallery() }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: Theme.paddingLarge; spacing: Theme.paddingMedium
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.paddingSmall
                    Text { text: translation.projectPath === "" ? qsTr("Untitled translation") : translation.projectPath.split(/[\\/]/).pop(); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true; Layout.fillWidth: true; elide: Text.ElideMiddle }
                    Text { text: translation.dirty ? qsTr("Unsaved") : qsTr("Saved"); color: translation.dirty ? Theme.warning : Theme.success; font.pixelSize: Theme.fontSmall; font.bold: true }
                    PrimaryButton { text: qsTr("Open project"); iconName: "folder"; quiet: true; onClicked: openProjectDialog.open() }
                    PrimaryButton { text: qsTr("Import text/subtitles"); iconName: "download"; quiet: true; onClicked: importDialog.open() }
                    PrimaryButton { text: qsTr("Save"); iconName: "save"; quiet: true; onClicked: translation.projectPath === "" ? saveProjectDialog.open() : translation.saveProject() }
                    PrimaryButton { text: qsTr("Export"); iconName: "external-link"; quiet: true; enabled: translation.segments.length > 0; onClicked: exportDialog.open() }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.paddingSmall
                    PrimaryButton { text: qsTr("New text"); iconName: "edit"; quiet: true; onClicked: textDialog.open() }
                    PrimaryButton { text: qsTr("Add segment"); iconName: "plus"; quiet: true; onClicked: translation.addSegment() }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        Layout.preferredWidth: 258
                        Layout.minimumWidth: 210
                        Layout.maximumWidth: 258
                        Layout.preferredHeight: 36
                        radius: Theme.radiusSmall
                        color: Qt.rgba(0, 0, 0, 0.16)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2

                            Repeater {
                                model: [
                                    { label: qsTr("Source"), mode: "source" },
                                    { label: qsTr("Bilingual"), mode: "bilingual" },
                                    { label: qsTr("Translation"), mode: "translation" }
                                ]

                                delegate: Button {
                                    id: viewModeButton
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    checkable: true
                                    checked: root.editorViewMode === modelData.mode
                                    padding: 0
                                    onClicked: root.editorViewMode = modelData.mode

                                    contentItem: Text {
                                        anchors.fill: parent
                                        text: modelData.label
                                        color: viewModeButton.checked ? Theme.textPrimary : Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        font.bold: viewModeButton.checked
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        radius: 5
                                        color: viewModeButton.checked ? Theme.surfaceAlt : (viewModeButton.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
                                        border.color: viewModeButton.checked ? Qt.rgba(1, 1, 1, 0.10) : "transparent"
                                        border.width: 1
                                    }

                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                }
                            }
                        }
                    }
                    Text { text: translation.statusText; color: translation.processing ? Theme.warning : Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    PrimaryButton { text: translation.processing ? qsTr("Cancel") : qsTr("Translate all"); iconName: translation.processing ? "stop" : "translate"; enabled: translation.processing || translation.segments.length > 0; onClicked: translation.processing ? translation.cancel() : translation.translateAll() }
                }
                Text { visible: translation.errorText !== ""; Layout.fillWidth: true; text: translation.errorText; color: Theme.danger; font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap }
                ListView {
                    id: editorList
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: Theme.paddingSmall; model: translation.segments
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        width: editorList.width; height: segmentRow.implicitHeight + Theme.paddingMedium * 2; radius: Theme.radiusSmall; color: Theme.surface; border.color: Qt.rgba(1,1,1,0.08); border.width: 1
                        RowLayout {
                            id: segmentRow; anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingSmall
                            Text { text: (index + 1).toString(); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 24; Layout.minimumWidth: 24; Layout.maximumWidth: 24; horizontalAlignment: Text.AlignHCenter }
                            ColumnLayout {
                                visible: root.showSourceEditor
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredWidth: 1
                                Layout.minimumWidth: 0
                                spacing: 4
                                Text { text: qsTr("SOURCE"); color: Theme.textSecondary; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                                AppTextArea { id: sourceArea; Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 68; implicitHeight: Math.max(68, contentHeight + Theme.paddingMedium * 2); text: modelData.sourceText || ""; placeholderText: qsTr("Source text"); onActiveFocusChanged: if (!activeFocus) translation.updateSegment(index, { sourceText: text, state: "ready" }) }
                            }
                            ColumnLayout {
                                visible: root.showTranslationEditor
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredWidth: 1
                                Layout.minimumWidth: 0
                                spacing: 4
                                Text { text: qsTr("TRANSLATION"); color: Theme.accentLight; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                                AppTextArea { Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 68; implicitHeight: Math.max(68, contentHeight + Theme.paddingMedium * 2); text: modelData.targetText || ""; placeholderText: qsTr("Target translation"); onActiveFocusChanged: if (!activeFocus) translation.updateSegment(index, { targetText: text, state: "edited" }) }
                            }
                            ColumnLayout {
                                Layout.alignment: Qt.AlignTop; spacing: 4
                                PrimaryButton {
                                    readonly property bool segmentRunning: translation.processing && translation.activeSegmentId === (modelData.id || "")
                                    text: qsTr("Run")
                                    iconName: "translate"
                                    quiet: true
                                    loading: segmentRunning
                                    textColor: Theme.accentLight
                                    borderColor: Qt.rgba(0.49, 0.30, 1.0, 0.38)
                                    implicitWidth: 88
                                    implicitHeight: 34
                                    enabled: !translation.processing && (modelData.sourceText || "").trim() !== ""
                                    onClicked: translation.translateSegment(index)
                                }
                                PrimaryButton {
                                    text: qsTr("Remove")
                                    iconName: "trash"
                                    quiet: true
                                    textColor: Theme.danger
                                    borderColor: Qt.rgba(0.94, 0.33, 0.31, 0.38)
                                    implicitWidth: 88
                                    implicitHeight: 34
                                    enabled: !translation.processing
                                    onClicked: translation.removeSegment(index)
                                }
                            }
                        }
                    }
                    footer: Item { width: editorList.width; height: Theme.paddingMedium }
                }
            }
        }
    ]

    settingsContent: [
        ColumnLayout {
            anchors.fill: parent; anchors.margins: Theme.paddingLarge; spacing: Theme.paddingMedium
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Translation settings"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; Layout.fillWidth: true }
                Button {
                    id: closeSettingsButton
                    implicitWidth: 30
                    implicitHeight: 30
                    flat: true

                    AppToolTip {
                        text: qsTr("Hide settings")
                        visible: parent.hovered
                    }

                    contentItem: LineIcon {
                        name: "chevron-right"
                        color: closeSettingsButton.hovered ? Theme.accent : Theme.textSecondary
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                    }

                    background: Rectangle {
                        radius: 7
                        color: closeSettingsButton.hovered ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.025)
                        border.color: closeSettingsButton.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.55) : Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                    }

                    onClicked: root.isSettingsOpen = false
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }
            LanguageSelector { Layout.fillWidth: true; family: root.family; labelText: qsTr("Source language"); language: translation.sourceLanguage; onLanguageChanged: translation.sourceLanguage = language }
            PrimaryButton { Layout.fillWidth: true; text: qsTr("Swap languages"); iconName: "swap"; quiet: true; onClicked: translation.swapLanguages() }
            LanguageSelector { Layout.fillWidth: true; family: root.family; labelText: qsTr("Target language"); language: translation.targetLanguage; onLanguageChanged: translation.targetLanguage = language }
            Item { Layout.fillHeight: true }
            Text { Layout.fillWidth: true; text: qsTr("Model and runtime are managed from the header. Processing stays on this device."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap }
        }
    ]

    Dialog {
        id: textDialog; modal: true; title: qsTr("New text"); width: Math.min(680, root.width - 80); height: Math.min(540, root.height - 80); anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: translation.importText(textInput.text)
        contentItem: AppTextArea { id: textInput; placeholderText: qsTr("Paste text. Empty lines create separate translation segments.") }
    }
    FileDialog { id: openProjectDialog; title: qsTr("Open Translation project"); nameFilters: [qsTr("Translation projects (*.lastudio-translation.json)"), qsTr("JSON files (*.json)")]; onAccepted: translation.openProject(AppController.files.urlToLocalPath(selectedFile.toString())) }
    FileDialog { id: importDialog; title: qsTr("Import text or subtitles"); nameFilters: [qsTr("Text and subtitles (*.txt *.srt *.vtt)"), qsTr("All files (*)")]; onAccepted: translation.importFile(AppController.files.urlToLocalPath(selectedFile.toString())) }
    FileDialog { id: saveProjectDialog; title: qsTr("Save Translation project"); fileMode: FileDialog.SaveFile; defaultSuffix: "lastudio-translation.json"; nameFilters: [qsTr("Translation projects (*.lastudio-translation.json)")]; onAccepted: translation.saveProjectAs(AppController.files.urlToLocalPath(selectedFile.toString())) }
    FileDialog { id: exportDialog; title: qsTr("Export translation"); fileMode: FileDialog.SaveFile; nameFilters: [qsTr("Text (*.txt)"), qsTr("Translation JSON (*.json)"), qsTr("SubRip subtitles (*.srt)"), qsTr("WebVTT subtitles (*.vtt)")]; onAccepted: translation.exportResult(AppController.files.urlToLocalPath(selectedFile.toString())) }
}
