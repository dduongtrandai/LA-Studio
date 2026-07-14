import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../../components/base"
import "../../components/shared/settings"

ScrollView {
    id: root

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    property bool wideLayout: width >= 760
    property real panelColorAlpha: 0.035
    property real panelBorderAlpha: 0.055

    function gb(value, digits) {
        return Number(value).toFixed(digits === undefined ? 2 : digits) + " GB"
    }

    function gpuSummary(count) {
        if (count === 0) {
            return qsTr("No GPU detected")
        }
        return count === 1 ? qsTr("1 GPU detected") : qsTr("%1 GPUs detected").arg(count)
    }

    ColumnLayout {
        width: Math.max(0, root.availableWidth - Theme.paddingLarge * 2)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.paddingLarge

        Item { Layout.preferredHeight: 2 }

        GridLayout {
            Layout.fillWidth: true
            columns: root.wideLayout ? 2 : 1
            columnSpacing: Theme.paddingMedium
            rowSpacing: Theme.paddingLarge

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.paddingSmall

                SectionHeader {
                    title: qsTr("CPU")
                    trailing: qsTr("Compatible")
                    trailingColor: Theme.success
                    showCheck: true
                }

                InfoPanel {
                    KeyValueRow {
                        label: qsTr("Name")
                        value: HardwareManager.cpuName === "" ? qsTr("Unknown") : HardwareManager.cpuName
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingMedium

                        Text {
                            text: qsTr("Architecture")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSmall
                            Layout.preferredWidth: 92
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Chip {
                                text: HardwareManager.cpuArchitecture === "" ? qsTr("Unknown") : HardwareManager.cpuArchitecture
                                highlighted: true
                            }

                            Repeater {
                                model: HardwareManager.cpuFlags === "" ? [] : HardwareManager.cpuFlags.split(" ")
                                delegate: Chip {
                                    required property string modelData
                                    text: modelData
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.paddingSmall

                SectionHeader { title: qsTr("Memory Capacity") }

                InfoPanel {
                    KeyValueRow {
                        label: qsTr("RAM")
                        value: root.gb(HardwareManager.ramTotal, 2)
                    }

                    KeyValueRow {
                        label: qsTr("VRAM")
                        value: root.gb(HardwareManager.vramTotal, 2)
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            SectionHeader { title: qsTr("GPUs") }

            InfoPanel {
                spacing: Theme.paddingMedium

                Text {
                    text: root.gpuSummary(HardwareManager.gpus.length)
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    Layout.fillWidth: true
                }

                Rectangle {
                    visible: HardwareManager.gpus.length === 0
                    Layout.fillWidth: true
                    implicitHeight: 42
                    radius: Theme.radiusSmall
                    color: Qt.rgba(0, 0, 0, 0.10)
                    border.color: Qt.rgba(255, 255, 255, root.panelBorderAlpha)
                    border.width: 1

                    Text {
                        anchors.fill: parent
                        anchors.margins: Theme.paddingMedium
                        text: qsTr("CPU runtimes remain available.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Repeater {
                    model: HardwareManager.gpus
                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        implicitHeight: gpuRow.implicitHeight + Theme.paddingMedium * 2
                        radius: Theme.radiusSmall
                        color: Qt.rgba(0, 0, 0, 0.14)
                        border.color: Qt.rgba(255, 255, 255, root.panelBorderAlpha)
                        border.width: 1

                        RowLayout {
                            id: gpuRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Theme.paddingMedium
                            spacing: Theme.paddingMedium

                            LineIcon {
                                name: "cpu"
                                color: Theme.accentLight
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: modelData.name || qsTr("Unknown GPU")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMedium
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: qsTr("VRAM Capacity: %1 - deviceId: %2").arg(root.gb(modelData.vram || 0, 2)).arg(index)
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                ThinDivider {}

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: qsTr("Offload KV Cache to GPU Memory")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: AppController.settings.offloadKvCache
                                ? qsTr("ON: Runtime may place compatible cache memory on detected GPU memory.")
                                : qsTr("OFF: KV cache remains on system memory when the runtime allows it.")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    ToggleRow {
                        text: ""
                        checked: AppController.settings.offloadKvCache
                        Layout.fillWidth: false
                        Layout.preferredWidth: 42
                        onToggled: AppController.settings.offloadKvCache = checked
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            SectionHeader { title: qsTr("Resource Monitor") }

            GridLayout {
                Layout.fillWidth: true
                columns: root.wideLayout ? 2 : 1
                columnSpacing: Theme.paddingMedium
                rowSpacing: Theme.paddingMedium

                MetricPanel {
                    title: qsTr("RAM + VRAM")
                    value: root.gb(HardwareManager.ramUsed + HardwareManager.vramUsed, 1)
                    detail: qsTr("of %1").arg(root.gb(HardwareManager.ramTotal + HardwareManager.vramTotal, 1))
                }

                MetricPanel {
                    title: qsTr("CPU")
                    value: Number(HardwareManager.cpuUsage).toFixed(2) + "%"
                    detail: qsTr("current system usage")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            SectionHeader { title: qsTr("Guardrails") }

            InfoPanel {
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Model loading guardrails")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                }

                ButtonGroup { id: guardrailGroup }

                Repeater {
                    model: [
                        { label: qsTr("OFF (Not Recommended)"), desc: qsTr("No precautions against system overload"), value: 0 },
                        { label: qsTr("Relaxed"), desc: qsTr("Warn only when memory risk is high"), value: 1 },
                        { label: qsTr("Balanced"), desc: qsTr("Recommended bounds for most local models"), value: 2 },
                        { label: qsTr("Strict"), desc: qsTr("Block loads likely to exhaust RAM or VRAM"), value: 3 },
                        { label: qsTr("Custom"), desc: qsTr("Use manual memory limits"), value: 4 }
                    ]

                    delegate: Rectangle {
                        id: guardItem
                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: Math.max(38, guardRow.implicitHeight + Theme.paddingSmall)
                        radius: Theme.radiusSmall
                        color: radioControl.checked ? Qt.rgba(124, 77, 255, 0.10) : "transparent"
                        border.color: radioControl.checked ? Qt.rgba(124, 77, 255, 0.25) : "transparent"
                        border.width: 1

                        RowLayout {
                            id: guardRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.paddingSmall
                            anchors.rightMargin: Theme.paddingSmall
                            spacing: Theme.paddingSmall

                            RadioButton {
                                id: radioControl
                                ButtonGroup.group: guardrailGroup
                                checked: AppController.settings.guardrailMode === guardItem.modelData.value
                                onToggled: {
                                    if (checked) {
                                        AppController.settings.guardrailMode = guardItem.modelData.value
                                    }
                                }
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                Layout.alignment: Qt.AlignTop

                                indicator: Rectangle {
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    radius: 8
                                    color: "transparent"
                                    border.color: radioControl.checked ? Theme.accentLight : Qt.rgba(255, 255, 255, 0.28)
                                    border.width: 1.4
                                    anchors.centerIn: parent

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: Theme.accentLight
                                        visible: radioControl.checked
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    text: guardItem.modelData.label
                                    color: radioControl.checked ? Theme.textPrimary : Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: radioControl.checked
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: guardItem.modelData.desc
                                    color: Theme.textSecondary
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                radioControl.checked = true
                                AppController.settings.guardrailMode = guardItem.modelData.value
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.paddingSmall }
    }

    component SectionHeader: RowLayout {
        property string title: ""
        property string trailing: ""
        property color trailingColor: Theme.textSecondary
        property bool showCheck: false

        Layout.fillWidth: true
        spacing: 6

        Text {
            text: title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.bold: true
        }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            color: "transparent"
            border.color: Qt.rgba(255, 255, 255, 0.18)
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "?"
                color: Theme.textSecondary
                font.pixelSize: 10
                font.bold: true
            }
        }

        RowLayout {
            visible: trailing !== ""
            spacing: 4

            LineIcon {
                visible: showCheck
                name: "check"
                color: trailingColor
                Layout.preferredWidth: 13
                Layout.preferredHeight: 13
                strokeWidth: 2
            }

            Text {
                text: trailing
                color: trailingColor
                font.pixelSize: Theme.fontSmall
            }
        }

        Item { Layout.fillWidth: true }
    }

    component InfoPanel: Rectangle {
        default property alias content: contentLayout.data
        property alias spacing: contentLayout.spacing

        Layout.fillWidth: true
        implicitHeight: contentLayout.implicitHeight + Theme.paddingMedium * 2
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.035)
        border.color: Qt.rgba(255, 255, 255, 0.055)
        border.width: 1

        ColumnLayout {
            id: contentLayout
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall
        }
    }

    component KeyValueRow: RowLayout {
        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        spacing: Theme.paddingMedium

        Text {
            text: label
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSmall
            Layout.preferredWidth: 92
        }

        Text {
            text: value
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSmall
            horizontalAlignment: Text.AlignRight
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    component Chip: Rectangle {
        property string text: ""
        property bool highlighted: false

        visible: text !== ""
        implicitWidth: chipText.implicitWidth + 12
        implicitHeight: 20
        radius: 5
        color: highlighted ? Qt.rgba(124, 77, 255, 0.16) : Qt.rgba(255, 255, 255, 0.055)
        border.color: highlighted ? Qt.rgba(124, 77, 255, 0.34) : Qt.rgba(255, 255, 255, 0.08)
        border.width: 1

        Text {
            id: chipText
            anchors.centerIn: parent
            text: parent.text
            color: parent.highlighted ? Theme.accentLight : Theme.textSecondary
            font.pixelSize: 10
            font.bold: true
        }
    }

    component ThinDivider: Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Qt.rgba(255, 255, 255, 0.07)
    }

    component MetricPanel: Rectangle {
        property string title: ""
        property string value: ""
        property string detail: ""

        Layout.fillWidth: true
        implicitHeight: 54
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.035)
        border.color: Qt.rgba(255, 255, 255, 0.055)
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingMedium

            Text {
                text: title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            ColumnLayout {
                spacing: 0
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                Text {
                    text: value
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight
                }

                Text {
                    visible: detail !== ""
                    text: detail
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight
                }
            }
        }
    }
}
