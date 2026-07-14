import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../shared/settings"
import "../base"

Rectangle {
    id: root
    Layout.preferredWidth: visible ? 300 : 0
    Layout.fillHeight: true
    color: Theme.surface
    radius: Theme.radiusLarge
    clip: true

    property var sttSession: null
    property var family: null
    property var dynamicSettings: ({})
    property var capabilitySchema: []
    property var basicSchema: []
    property var advancedSchema: []
    property var studioConfig: ({})
    property bool advancedOpen: false
    readonly property bool hasLanguageInput: studioConfig && studioConfig.inputs ? studioConfig.inputs.indexOf("language") !== -1 : true


    signal closeRequested()

    function splitSchema(schema, advanced) {
        var result = []
        for (var i = 0; i < schema.length; ++i) {
            var item = schema[i] || {}
            if (!!item.advanced === advanced) result.push(item)
        }
        return result
    }

    function refreshFromCatalog() {
        var schema = []
        var studio = (family && family.studio && family.studio.stt) ? family.studio.stt : {}
        var definitions = (family && family.parameterDefinitions) ? family.parameterDefinitions : {}
        var ids = studio.parameters || []
        if (!ids || ids.length === 0) ids = ["threads", "translate"]
        for (var i = 0; i < ids.length; ++i) {
            var id = ids[i]
            var def = definitions[id]
            if (!def) continue
            var item = {}
            for (var k in def) item[k] = def[k]
            item.id = id
            if (id === "threads") {
                item.min = 0
                item.default = 0
                item.autoLabel = qsTr("Auto")
            }
            schema.push(item)
        }

        capabilitySchema = schema
        studioConfig = studio
        basicSchema = splitSchema(schema, false)
        advancedSchema = splitSchema(schema, true)

        var defaults = {}
        for (var j = 0; j < schema.length; ++j) {
            var p = schema[j] || {}
            if (p.id && p["default"] !== undefined) defaults[p.id] = p["default"]
        }
        if (sttSession) {
            defaults["threads"] = sttSession.threads
            defaults["translate"] = sttSession.translate
            defaults["language"] = sttSession.language
        }
        dynamicSettings = defaults
        if (sttSession) sttSession.dynamicSettings = dynamicSettings
    }

    function updateDynamicSetting(parameterId, value) {
        var settings = JSON.parse(JSON.stringify(dynamicSettings))
        settings[parameterId] = value
        dynamicSettings = settings
        if (sttSession) sttSession.dynamicSettings = dynamicSettings

        if (!sttSession) return
        if (parameterId === "threads") {
            var n = Math.max(0, Math.min(64, Math.round(Number(value))))
            if (sttSession.threads !== n) sttSession.threads = n
            return
        }
        if (parameterId === "translate") {
            var b = !!value
            if (sttSession.translate !== b) sttSession.translate = b
        }
    }

    Component.onCompleted: refreshFromCatalog()

    onFamilyChanged: refreshFromCatalog()

    Connections {
        target: sttSession
        ignoreUnknownSignals: true
        function onThreadsChanged() {
            if (root.dynamicSettings["threads"] !== sttSession.threads) root.updateDynamicSetting("threads", sttSession.threads)
        }
        function onTranslateChanged() {
            if (root.dynamicSettings["translate"] !== sttSession.translate) root.updateDynamicSetting("translate", sttSession.translate)
        }
        function onLanguageChanged() {
            if (langSelector.language !== sttSession.language) langSelector.language = sttSession.language
            if (root.dynamicSettings["language"] !== sttSession.language) {
                root.updateDynamicSetting("language", sttSession.language)
            }
        }
    }

    Behavior on Layout.preferredWidth {
        NumberAnimation { duration: 250; easing.type: Easing.InOutQuad }
    }

    ColumnLayout {
        width: 300 // Keep internal width fixed to avoid layout jumping
        height: parent.height
        anchors.left: parent.left
        anchors.margins: 0
        
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingLarge

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Model Settings")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                    Layout.fillWidth: true
                }
                
                Button {
                    width: 32; height: 32
                    flat: true
                    onClicked: root.closeRequested()
                    contentItem: LineIcon {
                        name: "close"
                        color: Theme.textSecondary
                        anchors.centerIn: parent
                        width: 14; height: 14
                    }
                    background: Rectangle {
                        radius: 16
                        color: parent.hovered ? Qt.rgba(1,1,1,0.05) : "transparent"
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
                    width: parent.width - 4
                    spacing: Theme.paddingMedium

                    SettingsSection {
                        title: qsTr("Core")
                        iconName: "file"
                        visible: root.hasLanguageInput

                        LanguageSelector {
                            id: langSelector
                            Layout.fillWidth: true
                            family: root.family
                            hasLanguageInput: root.hasLanguageInput
                            useTextFieldFallback: true
                            language: sttSession ? sttSession.language : "auto"
                            onLanguageChanged: {
                                if (sttSession && sttSession.language !== language) {
                                    sttSession.language = language
                                }
                            }
                        }
                    }

                    SettingsSection {
                        title: qsTr("Model Parameters")
                        iconName: "sliders"
                        visible: root.basicSchema.length > 0

                        ModelParameterControls {
                            schema: root.basicSchema
                            dynamicSettings: root.dynamicSettings
                            onParameterChanged: function(parameterId, value) {
                                root.updateDynamicSetting(parameterId, value)
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
                            schema: root.advancedSchema
                            dynamicSettings: root.dynamicSettings
                            onParameterChanged: function(parameterId, value) {
                                root.updateDynamicSetting(parameterId, value)
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
