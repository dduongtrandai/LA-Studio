import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../../base"
import "."

ColumnLayout {
    id: root

    property var schema: []
    property var dynamicSettings: ({})

    signal settingsChanged()
    signal parameterChanged(string parameterId, var value)

    function updateParameter(parameterId, value) {
        root.parameterChanged(parameterId, value)
        root.settingsChanged()
    }

    Layout.fillWidth: true
    spacing: Theme.paddingMedium

    Repeater {
        model: root.schema
        delegate: ColumnLayout {
            visible: item.type !== "bool"
            Layout.fillWidth: true
            spacing: 6
            property var item: modelData
            property var choices: item.choices || item.options || []
            property bool syncingChoiceIndex: false

            function resolveChoiceIndex() {
                if (!choices || item.type !== "choice" || choices.length === 0) return -1
                var currentVal = root.dynamicSettings[item.id]
                if (currentVal !== undefined) {
                    for (var j = 0; j < choices.length; j++) {
                        if (choices[j].value === currentVal) return j
                    }
                }
                for (var i = 0; i < choices.length; i++) {
                    if (choices[i].value === item.default) return i
                }
                return 0
            }

            function syncChoiceIndex() {
                if (item.type !== "choice")
                    return
                syncingChoiceIndex = true
                choiceControl.currentIndex = resolveChoiceIndex()
                syncingChoiceIndex = false
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: item.name
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    implicitWidth: valueLabel.implicitWidth + 12
                    implicitHeight: 20
                    radius: 4
                    color: Qt.rgba(0.49, 0.30, 1.0, 0.12)
                    visible: item.type === "float" || item.type === "int"

                    Text {
                        id: valueLabel
                        anchors.centerIn: parent
                        text: item.type === "float"
                              ? control.value.toFixed(2)
                              : (item.type === "int"
                                 ? ((item.autoLabel !== undefined && Math.round(control.value) === (item.autoValue !== undefined ? item.autoValue : 0)) ? item.autoLabel : control.value)
                                 : "")
                        color: Theme.accentLight
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }
            }

            ParameterSlider {
                id: control
                Layout.fillWidth: true
                visible: item.type === "float" || item.type === "int"
                from: item.min !== undefined ? item.min : 0
                to: item.max !== undefined ? item.max : 100
                value: root.dynamicSettings[item.id] !== undefined ? root.dynamicSettings[item.id] : (item.default !== undefined ? item.default : 0)
                stepSize: item.type === "float" ? (item.step || 0.1) : (item.step || 1)
                onMoved: {
                    root.updateParameter(item.id, value)
                }
            }

            AppComboBox {
                id: choiceControl
                Layout.fillWidth: true
                visible: item.type === "choice"
                model: choices
                textRole: "text"
                secondaryTextRole: "detail"
                onCurrentIndexChanged: {
                    if (syncingChoiceIndex)
                        return
                    if (!model || currentIndex < 0 || currentIndex >= model.length || !model[currentIndex])
                        return
                    var val = model[currentIndex].value
                    if (root.dynamicSettings[item.id] === val)
                        return
                    root.updateParameter(item.id, val)
                }
                Component.onCompleted: {
                    syncChoiceIndex()
                }
                onModelChanged: syncChoiceIndex()
            }

            Connections {
                target: root
                function onDynamicSettingsChanged() {
                    syncChoiceIndex()
                }
            }

            Text {
                text: item.description || ""
                color: Theme.textSecondary
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                visible: text !== ""
                Layout.fillWidth: true
            }
        }
    }

    Repeater {
        model: root.schema
        delegate: ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: item.type === "bool"
            property var item: modelData

            ToggleRow {
                Layout.fillWidth: true
                text: item.name || item.id
                checked: root.dynamicSettings[item.id] !== undefined
                         ? !!root.dynamicSettings[item.id]
                         : !!item.default
                onClicked: {
                    if (root.dynamicSettings[item.id] === checked)
                        return
                    root.updateParameter(item.id, checked)
                }
            }

            Text {
                text: item.description || ""
                color: Theme.textSecondary
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                visible: text !== ""
                Layout.fillWidth: true
            }
        }
    }
}
