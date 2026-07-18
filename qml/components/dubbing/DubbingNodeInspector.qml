import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import "../shared/settings"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property string nodeId
    required property var node
    required property string nodeTitle

    property bool advancedOpen: false
    readonly property var parameterSchema: node && node.parameterSchema ? node.parameterSchema : []
    readonly property var dynamicSettings: node && node.parameters ? node.parameters : ({})
    readonly property var studioConfig: node && node.studioConfig ? node.studioConfig : ({})
    readonly property var basicSchema: splitSchema(parameterSchema, false)
    readonly property var advancedSchema: splitSchema(parameterSchema, true)
    readonly property bool hasLanguageInput: studioConfig && studioConfig.inputs
                                                     ? studioConfig.inputs.indexOf("language") !== -1
                                                     : nodeId === "transcribe"

    signal closeRequested()

    Layout.preferredWidth: 332
    Layout.minimumWidth: 290
    Layout.fillHeight: true
    visible: node && node.configurable === true
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    clip: true

    function splitSchema(schema, advanced) {
        var result = []
        for (var i = 0; i < schema.length; ++i) {
            var item = schema[i] || {}
            if (!!item.advanced === advanced) result.push(item)
        }
        return result
    }

    function updateParameter(parameterId, value) {
        var patch = ({})
        patch[parameterId] = value
        root.dubbing.setWorkflowNodeParameters(root.nodeId, patch)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            LineIcon {
                name: "sliders"
                color: Theme.accent
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 Settings").arg(root.nodeTitle)
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: root.node && root.node.providerName
                          ? root.node.providerName : qsTr("No model configured")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            Button {
                id: closeButton
                implicitWidth: 30
                implicitHeight: 30
                flat: true
                onClicked: root.closeRequested()
                contentItem: LineIcon {
                    name: "chevron-right"
                    color: closeButton.hovered ? Theme.accent : Theme.textSecondary
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                }
                background: Rectangle {
                    radius: 7
                    color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.025)
                    border.color: closeButton.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                                                      : Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: Math.max(0, parent.width - Theme.paddingSmall)
                spacing: Theme.paddingMedium

                SettingsSection {
                    title: qsTr("Core")
                    iconName: "file"
                    visible: root.nodeId === "translate" || root.hasLanguageInput

                    LanguageSelector {
                        Layout.fillWidth: true
                        visible: root.nodeId === "translate"
                        family: null
                        labelText: qsTr("Source language")
                        language: root.dubbing.sourceLanguage
                        onLanguageChanged: root.dubbing.sourceLanguage = language
                    }

                    LanguageSelector {
                        Layout.fillWidth: true
                        family: null
                        labelText: root.nodeId === "translate" ? qsTr("Target language") : qsTr("Language")
                        language: root.nodeId === "translate"
                                  ? root.dubbing.targetLanguage
                                  : String(root.dynamicSettings["language"] !== undefined
                                           ? root.dynamicSettings["language"] : "auto")
                        onLanguageChanged: {
                            if (root.nodeId === "translate")
                                root.dubbing.targetLanguage = language
                            else
                                root.updateParameter("language", language)
                        }
                    }
                }

                SettingsSection {
                    title: qsTr("Model Parameters")
                    iconName: "sliders"
                    visible: root.basicSchema.length > 0

                    ModelParameterControls {
                        enabled: !root.dubbing.processing
                        schema: root.basicSchema
                        dynamicSettings: root.dynamicSettings
                        onParameterChanged: function(parameterId, value) {
                            root.updateParameter(parameterId, value)
                        }
                    }
                }

                CollapsibleSettingsSection {
                    title: qsTr("Advanced")
                    iconName: "sliders"
                    visible: root.advancedSchema.length > 0
                    expanded: root.advancedOpen
                    onToggled: root.advancedOpen = !root.advancedOpen

                    ModelParameterControls {
                        enabled: !root.dubbing.processing
                        schema: root.advancedSchema
                        dynamicSettings: root.dynamicSettings
                        onParameterChanged: function(parameterId, value) {
                            root.updateParameter(parameterId, value)
                        }
                    }
                }

                SettingsSection {
                    title: qsTr("Model & Runtime")
                    iconName: "cpu"

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            Layout.fillWidth: true
                            text: root.node && root.node.providerName
                                  ? root.node.providerName : qsTr("Not configured")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSmall
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.node && root.node.selectedRuntimeId
                                  ? root.node.selectedRuntimeId : qsTr("Runtime not selected")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Model and runtime files are changed from Configure.")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
