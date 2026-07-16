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
                LineIcon { name: "history"; color: Theme.accent; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
                Text { text: qsTr("Translation History"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; Layout.fillWidth: true }
                Button { visible: translation.history.length > 0; text: qsTr("Clear"); flat: true; onClicked: translation.clearHistory() }
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
                                Button { text: qsTr("Load"); flat: true; onClicked: translation.loadHistoryItem(index) }
                                Button { text: qsTr("Delete"); flat: true; onClicked: translation.deleteHistoryItem(index) }
                            }
                        }
                    }
                }
            }
        }
    ]

    mainContent: [
        StackLayout {
            anchors.fill: parent
            currentIndex: root.studioReady ? 1 : 0
            Item {
                ColumnLayout {
                    anchors.centerIn: parent; width: Math.min(520, parent.width - Theme.paddingXL * 2); spacing: Theme.paddingLarge
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 178; radius: Theme.radiusMedium; color: Theme.surface; border.color: Qt.rgba(1,1,1,0.08); border.width: 1
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: Theme.paddingLarge; spacing: Theme.paddingMedium
                            LineIcon { name: "gallery"; color: Theme.accent; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                            Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Select a Translation model and runtime"); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true; wrapMode: Text.WordWrap }
                            Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Translation stays local and uses the installed CrispASR runtime."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap }
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
                    PrimaryButton { text: qsTr("Open"); iconName: "folder"; quiet: true; onClicked: openProjectDialog.open() }
                    PrimaryButton { text: qsTr("Import"); iconName: "download"; quiet: true; onClicked: importDialog.open() }
                    PrimaryButton { text: qsTr("Save"); iconName: "save"; quiet: true; onClicked: translation.projectPath === "" ? saveProjectDialog.open() : translation.saveProject() }
                    PrimaryButton { text: qsTr("Export"); iconName: "external-link"; quiet: true; enabled: translation.segments.length > 0; onClicked: exportDialog.open() }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1,1,1,0.07) }
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.paddingSmall
                    PrimaryButton { text: qsTr("New text"); iconName: "edit"; quiet: true; onClicked: textDialog.open() }
                    PrimaryButton { text: qsTr("Add segment"); iconName: "plus"; quiet: true; onClicked: translation.addSegment() }
                    Item { Layout.fillWidth: true }
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
                            Text { text: (index + 1).toString(); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 24; horizontalAlignment: Text.AlignHCenter }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: qsTr("SOURCE"); color: Theme.textSecondary; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                                AppTextArea { id: sourceArea; Layout.fillWidth: true; implicitHeight: Math.max(68, contentHeight + Theme.paddingMedium * 2); text: modelData.sourceText || ""; placeholderText: qsTr("Source text"); onActiveFocusChanged: if (!activeFocus) translation.updateSegment(index, { sourceText: text, state: "ready" }) }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: qsTr("TRANSLATION"); color: Theme.accentLight; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                                AppTextArea { Layout.fillWidth: true; implicitHeight: Math.max(68, contentHeight + Theme.paddingMedium * 2); text: modelData.targetText || ""; placeholderText: qsTr("Target translation"); onActiveFocusChanged: if (!activeFocus) translation.updateSegment(index, { targetText: text, state: "edited" }) }
                            }
                            ColumnLayout {
                                Layout.alignment: Qt.AlignTop; spacing: 4
                                PrimaryButton { text: qsTr("Run"); iconName: "translate"; quiet: true; implicitWidth: 58; enabled: !translation.processing && (modelData.sourceText || "").trim() !== ""; onClicked: translation.translateSegment(index) }
                                PrimaryButton { text: qsTr("Remove"); quiet: true; implicitWidth: 58; onClicked: translation.removeSegment(index) }
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
            Text { text: qsTr("Translation settings"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true }
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
