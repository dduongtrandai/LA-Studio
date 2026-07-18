import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../../base"

ColumnLayout {
    id: root

    property var family: null
    property string language: ""
    property string labelText: qsTr("Language")
    property bool useTextFieldFallback: false
    property bool hasLanguageInput: true
    property bool syncingSelection: false

    visible: hasLanguageInput
    spacing: Theme.paddingSmall
    Layout.fillWidth: true

    readonly property var defaultLanguages: AppController.catalog.languageSet("default")

    function normalizeLanguageItems(items) {
        if (!items) return []
        var result = []
        for (var i = 0; i < items.length; i++) {
            var item = items[i] || {}
            var code = item.value || item.code || ""
            var label = item.text || item.name || code
            if (item.name && item.code) {
                label = item.name + " (" + item.code + ")"
            }
            result.push({
                text: label,
                value: code,
                detail: item.detail || (code !== "" ? qsTr("Language code: %1").arg(code) : "")
            })
        }
        return result
    }

    readonly property var supportedLanguages: {
        if (family) {
            if (family.supportedLanguageSetId) {
                var list = AppController.catalog.languageSet(family.supportedLanguageSetId)
                if (list && list.length > 0) {
                    return normalizeLanguageItems(list)
                }
            }
            if (family.supportedLanguages && family.supportedLanguages.length > 0) {
                return normalizeLanguageItems(family.supportedLanguages)
            }
            if (family.featuredLanguages && family.featuredLanguages.length > 0) {
                return normalizeLanguageItems(family.featuredLanguages)
            }
        }
        
        if (useTextFieldFallback) {
            return []
        }
        
        return normalizeLanguageItems(defaultLanguages)
    }

    readonly property bool showComboBox: supportedLanguages.length > 0
    readonly property bool showTextField: !showComboBox && useTextFieldFallback

    function languageIndex() {
        if (!langCombo.model || langCombo.model.length === 0) return 0
        for (var i = 0; i < langCombo.model.length; ++i) {
            if (langCombo.model[i].value === root.language) return i
        }
        return 0
    }

    function syncComboSelection() {
        if (!root.showComboBox) return
        var index = root.languageIndex()
        if (langCombo.currentIndex === index) return
        root.syncingSelection = true
        langCombo.currentIndex = index
        root.syncingSelection = false
    }

    FieldLabel {
        text: root.labelText
        visible: root.showComboBox || root.showTextField
        Layout.fillWidth: true
    }

    AppComboBox {
        id: langCombo
        Layout.fillWidth: true
        visible: root.showComboBox
        model: root.supportedLanguages
        textRole: "text"
        secondaryTextRole: "detail"
        searchable: model.length > 15
        Component.onCompleted: root.syncComboSelection()
        onModelChanged: root.syncComboSelection()
        onCurrentIndexChanged: {
            if (root.syncingSelection) return
            if (!model || currentIndex < 0 || currentIndex >= model.length) return
            var val = model[currentIndex].value
            if (root.language !== val) {
                root.language = val
            }
        }
    }

    TextField {
        id: langField
        Layout.fillWidth: true
        visible: root.showTextField
        text: root.language
        placeholderText: qsTr("e.g. en, vi, auto")
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontMedium
        onTextChanged: {
            if (root.language !== text) {
                root.language = text
            }
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surfaceAlt
            border.color: langField.activeFocus ? Qt.rgba(0.49, 0.30, 1.0, 0.75) : Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
        }
    }

    onLanguageChanged: {
        root.syncComboSelection()
        if (showTextField && langField.text !== root.language) {
            langField.text = root.language
        }
    }
}
