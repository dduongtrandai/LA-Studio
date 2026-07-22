import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../shared"
import "../base"

StudioShell {
    id: root
    property var chat: AppController.llmChat
    signal backToGallery()
    onRequestBack: root.backToGallery()
    onRequestConfigurationPicker: root.backToGallery()

    component NumberField: TextField {
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        padding: Theme.paddingSmall
        selectByMouse: true
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.035)
            border.color: parent.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.09)
            border.width: parent.activeFocus ? 2 : 1
        }
    }
    property var family: {
        if (!studioController) return null
        var list = studioController.families
        for (var i = 0; i < list.length; ++i)
            if (list[i].id === studioController.selectedFamilyId) return list[i]
        return null
    }
    families: studioController ? studioController.families : []
    capability: "llm-chat"
    studioTitle: qsTr("LLM Chat Studio")
    studioIconName: "chat"
    studioReady: studioController ? studioController.studioReady : false
    showLeftPanel: true
    isLeftPanelOpen: true
    showSettingsPanel: true
    isSettingsOpen: true
    settingsRequiresReady: false
    modalSelectionMode: true
    modalSelectionTitle: family ? family.title : qsTr("Model + Runtime")
    selectedFamilyId: studioController ? studioController.selectedFamilyId : ""
    modalSelectionValue: studioController ? studioController.statusText : qsTr("Select model and runtime")
    modalSelectionDetail: studioController ? studioController.statusDetail : ""
    backToolTip: qsTr("Change model and runtime")

    leftPanelContent: [
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.paddingMedium
            RowLayout {
                Layout.fillWidth: true
                LineIcon { name: "chat"; color: Theme.accentLight; Layout.preferredWidth: Theme.iconSize; Layout.preferredHeight: Theme.iconSize }
                Text { text: qsTr("Conversations"); color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.fontMedium; Layout.fillWidth: true }
                PrimaryButton { text: qsTr("New"); iconName: "plus"; quiet: true; onClicked: chat.newConversation() }
            }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: Theme.paddingSmall
                model: chat.conversations
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: Theme.paddingXL + Theme.paddingLarge; radius: Theme.radiusSmall
                    color: modelData.id === chat.activeConversationId ? Theme.surfaceAlt : "transparent"
                    border.color: modelData.id === chat.activeConversationId ? Theme.accent : "transparent"
                    RowLayout { anchors.fill: parent; anchors.margins: Theme.paddingSmall; spacing: Theme.paddingSmall
                        Text { text: modelData.title || qsTr("New chat"); color: Theme.textPrimary; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: Theme.fontSmall }
                        PrimaryButton { iconName: "trash"; iconOnly: true; quiet: true; toolTip: qsTr("Delete conversation"); onClicked: chat.deleteConversation(modelData.id) }
                    }
                    MouseArea { anchors.fill: parent; anchors.rightMargin: 40; onClicked: chat.selectConversation(modelData.id) }
                }
            }
        }
    ]

    mainContent: [
        ColumnLayout {
            anchors.fill: parent; anchors.margins: Theme.paddingLarge; spacing: Theme.paddingMedium
            RowLayout {
                Layout.fillWidth: true
                LineIcon { name: "chat"; color: Theme.accentLight; Layout.preferredWidth: Theme.iconSize; Layout.preferredHeight: Theme.iconSize }
                Text { text: qsTr("Local conversation"); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true; Layout.fillWidth: true }
                Text { text: chat.generating ? qsTr("Generating") : (studioController && studioController.studioReady ? qsTr("Ready") : qsTr("Setup required")); color: chat.generating ? Theme.warning : (studioController && studioController.studioReady ? Theme.success : Theme.warning); font.pixelSize: Theme.fontSmall; font.bold: true }
                PrimaryButton { text: qsTr("Clear"); iconName: "trash"; quiet: true; enabled: !chat.generating; onClicked: chat.clearConversation() }
            }
            Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: Theme.radiusSmall; color: Qt.rgba(0,0,0,0.12); border.color: Qt.rgba(1,1,1,0.07); border.width: 1
                ListView {
                    id: messageList; anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingSmall; clip: true; model: chat.messages
                    delegate: Rectangle {
                        required property var modelData
                        width: messageList.width; implicitHeight: messageLayout.implicitHeight + Theme.paddingMedium * 2; radius: Theme.radiusSmall
                        color: modelData.role === "user" ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) : Theme.surface
                        border.color: Qt.rgba(1,1,1,0.07); border.width: 1
                        ColumnLayout { id: messageLayout; anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingSmall
                            RowLayout {
                                id: messageHeader
                                Layout.fillWidth: true
                                Text { text: modelData.role === "user" ? qsTr("You") : qsTr("Assistant"); color: modelData.role === "user" ? Theme.accentLight : Theme.textSecondary; font.bold: true; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                                PrimaryButton { text: qsTr("Copy"); iconName: "copy"; quiet: true; implicitWidth: 76; implicitHeight: 30; onClicked: chat.copyMessage(modelData.content || "") }
                            }
                            Text { id: messageText; Layout.fillWidth: true; text: modelData.content || ""; textFormat: modelData.role === "assistant" ? Text.MarkdownText : Text.PlainText; color: Theme.textPrimary; wrapMode: Text.Wrap; font.pixelSize: Theme.fontMedium }
                        }
                    }
                    onCountChanged: Qt.callLater(function(){ positionViewAtEnd() })
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: Theme.paddingSmall
                AppTextArea { id: composer; Layout.fillWidth: true; Layout.minimumHeight: Theme.paddingXL * 3; placeholderText: qsTr("Message the local model..."); enabled: !chat.generating; Keys.onReturnPressed: function(event) { if (!(event.modifiers & Qt.ShiftModifier)) { chat.sendMessage(text); text = ""; event.accepted = true } } }
                PrimaryButton { text: chat.generating ? qsTr("Stop") : qsTr("Send"); iconName: chat.generating ? "stop" : "send"; enabled: chat.generating || composer.text.trim() !== ""; onClicked: chat.generating ? chat.stopGeneration() : (chat.sendMessage(composer.text), composer.text = "") }
            }
            Text { visible: chat.errorText !== ""; Layout.fillWidth: true; text: chat.errorText; color: Theme.danger; wrapMode: Text.Wrap; font.pixelSize: Theme.fontSmall }
        }
    ]

    settingsContent: [
        ScrollView {
            anchors.fill: parent
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: Theme.paddingMedium

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    LineIcon { name: "settings"; color: Theme.accentLight; Layout.preferredWidth: Theme.iconSize; Layout.preferredHeight: Theme.iconSize }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text { text: qsTr("Chat settings"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true }
                        Text { text: qsTr("Generation controls"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

                Text { text: qsTr("System prompt"); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true }
                Text { Layout.fillWidth: true; text: qsTr("Optional instructions applied before the conversation."); color: Theme.textSecondary; wrapMode: Text.WordWrap; font.pixelSize: Theme.fontSmall }
                AppTextArea {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    text: chat.systemPrompt
                    placeholderText: qsTr("Optional instructions")
                    onTextChanged: if (!activeFocus || text !== chat.systemPrompt) chat.systemPrompt = text
                }

                Text { text: qsTr("Generation limits"); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.paddingSmall
                    rowSpacing: Theme.paddingSmall
                    Text { text: qsTr("Context"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                    NumberField { text: chat.contextTokens.toString(); Layout.preferredWidth: 108; inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: chat.contextTokens = parseInt(text) || 4096 }
                    Text { text: qsTr("Max output"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                    NumberField { text: chat.maxTokens.toString(); Layout.preferredWidth: 108; inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: chat.maxTokens = parseInt(text) || 1024 }
                }

                Text { text: qsTr("Sampling"); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.paddingSmall
                    rowSpacing: Theme.paddingSmall
                    Text { text: qsTr("Temperature"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                    NumberField { text: chat.temperature.toFixed(2); Layout.preferredWidth: 108; onEditingFinished: chat.temperature = parseFloat(text) || 0.7 }
                    Text { text: qsTr("Top P"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                    NumberField { text: chat.topP.toFixed(2); Layout.preferredWidth: 108; onEditingFinished: chat.topP = parseFloat(text) || 0.8 }
                    Text { text: qsTr("Top K"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                    NumberField { text: chat.topK.toString(); Layout.preferredWidth: 108; inputMethodHints: Qt.ImhDigitsOnly; onEditingFinished: chat.topK = parseInt(text) || 20 }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    LineIcon { name: "cpu"; color: Theme.success; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                    Text { Layout.fillWidth: true; text: qsTr("Local llama.cpp runtime"); color: Theme.textSecondary; wrapMode: Text.WordWrap; font.pixelSize: Theme.fontSmall }
                }
                Item { Layout.preferredHeight: Theme.paddingSmall }
            }
        }
    ]
}
