import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import "../shared"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property var languageCatalog
    required property string currentStepTitle
    signal adaptiveSetupRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: 136
    Layout.leftMargin: Theme.paddingMedium
    Layout.rightMargin: Theme.paddingMedium
    Layout.bottomMargin: Theme.paddingMedium
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        ColumnLayout {
            Layout.preferredWidth: 270
            Layout.fillHeight: true
            spacing: 4
            Text { text: qsTr("LANGUAGE & VOICE"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1 }
            RowLayout {
                Layout.fillWidth: true
                AppComboBox {
                    Layout.fillWidth: true
                    model: root.languageCatalog
                    textRole: "text"
                    secondaryTextRole: "detail"
                    searchable: model.length > 6
                    currentIndex: {
                        for (var i = 0; i < model.length; ++i)
                            if (model[i].value === root.dubbing.sourceLanguage) return i
                        return 0
                    }
                    onActivated: function(index) {
                        if (index >= 0 && index < model.length) root.dubbing.sourceLanguage = model[index].value
                    }
                }
                LineIcon { name: "chevron-right"; color: Theme.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                AppComboBox {
                    Layout.fillWidth: true
                    model: root.languageCatalog
                    textRole: "text"
                    secondaryTextRole: "detail"
                    searchable: model.length > 6
                    currentIndex: {
                        for (var i = 0; i < model.length; ++i)
                            if (model[i].value === root.dubbing.targetLanguage) return i
                        return 0
                    }
                    onActivated: function(index) {
                        if (index >= 0 && index < model.length) root.dubbing.targetLanguage = model[index].value
                    }
                }
            }
            PrimaryButton { text: qsTr("Add speaker"); iconName: "users"; quiet: true; onClicked: root.dubbing.addSpeaker() }
        }

        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        ColumnLayout {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            spacing: 4

            Text {
                text: qsTr("DUBBING QUALITY")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                font.letterSpacing: 1.1
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: Theme.radiusSmall
                color: Qt.rgba(1, 1, 1, 0.025)
                border.color: Qt.rgba(1, 1, 1, 0.08)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3

                    QualityModeButton {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        text: qsTr("Fast")
                        iconName: "activity"
                        selected: root.dubbing.dubbingQuality === "fast"
                        onClicked: root.dubbing.dubbingQuality = "fast"
                    }
                    QualityModeButton {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        text: qsTr("Adaptive")
                        iconName: "spark"
                        selected: root.dubbing.dubbingQuality === "adaptive"
                        warning: selected && !root.dubbing.adaptiveReady
                        onClicked: root.dubbing.dubbingQuality = "adaptive"
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.dubbing.dubbingQuality === "fast"
                      ? qsTr("Direct local workflow without LLM adaptation.")
                      : root.dubbing.adaptiveStatusText
                color: root.dubbing.dubbingQuality === "adaptive" && !root.dubbing.adaptiveReady
                       ? Theme.warning : Theme.textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
            }

            PrimaryButton {
                visible: root.dubbing.dubbingQuality === "adaptive"
                text: qsTr("Configure LLM")
                iconName: "settings"
                quiet: true
                onClicked: root.adaptiveSetupRequested()
            }
        }

        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("SPEAKERS"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1; Layout.fillWidth: true }
                Text { text: root.dubbing.speakers.length; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall
                Repeater {
                    model: root.dubbing.speakers
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: speakerLabel.implicitWidth + 22
                        height: 29
                        radius: 14
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
                        border.width: 1
                        Text { id: speakerLabel; anchors.centerIn: parent; text: modelData.name || qsTr("Speaker %1").arg(index + 1); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall }
                    }
                }
                Text { visible: root.dubbing.speakers.length === 0; text: qsTr("No speakers assigned"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            }
            Text { visible: root.dubbing.lastError.length > 0; text: root.dubbing.lastError; color: Theme.danger; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight; Layout.fillWidth: true }
        }

        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        ColumnLayout {
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            spacing: Theme.paddingSmall
            Text { text: qsTr("OUTPUT"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1 }
            Text { text: root.dubbing.workflowMode === "automatic" ? qsTr("Full workflow") : (root.dubbing.workflowMode === "step" ? qsTr("Manual node run") : qsTr("Choose an action")); color: root.dubbing.workflowMode === "idle" ? Theme.textSecondary : Theme.accentLight; font.pixelSize: Theme.fontSmall; font.bold: true }
            Text { Layout.fillWidth: true; text: qsTr("Current: %1").arg(root.currentStepTitle); color: root.dubbing.processing ? Theme.warning : Theme.textSecondary; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight }
            Text { Layout.fillWidth: true; text: root.dubbing.exportPath.length > 0 ? root.dubbing.exportPath : (root.dubbing.previewPath.length > 0 ? root.dubbing.previewPath : qsTr("Final output has not been created.")); color: root.dubbing.exportPath.length > 0 ? Theme.success : Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideMiddle }
            PrimaryButton { text: qsTr("Cancel processing"); visible: root.dubbing.processing; buttonColor: Theme.danger; onClicked: root.dubbing.cancelProcessing() }
        }
    }

    component QualityModeButton: Button {
        id: modeButton
        required property string iconName
        required property bool selected
        property bool warning: false
        implicitHeight: 32
        padding: 0
        contentItem: RowLayout {
            spacing: 6
            Item { Layout.fillWidth: true }
            LineIcon {
                name: modeButton.iconName
                color: modeButton.warning ? Theme.warning
                                           : (modeButton.selected ? Theme.accentLight : Theme.textSecondary)
                Layout.preferredWidth: 15
                Layout.preferredHeight: 15
            }
            Text {
                text: modeButton.text
                color: modeButton.selected ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                font.bold: modeButton.selected
            }
            Item { Layout.fillWidth: true }
        }
        background: Rectangle {
            radius: Theme.radiusSmall - 2
            color: modeButton.selected
                   ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                   : (modeButton.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
            border.color: modeButton.warning
                          ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.45)
                          : (modeButton.selected
                             ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                             : "transparent")
            border.width: modeButton.selected ? 1 : 0
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }
}
