import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Dialog {
    id: root

    property var loadedModels: []
    property string statusText: qsTr("Ready")
    property color statusColor: Theme.success
    property bool processing: false
    property string searchText: ""

    signal activateRequested(string modelId)
    signal unloadRequested(string modelId)
    signal configureCurrentRequested()
    signal loadAnotherRequested()

    function filteredModels() {
        var models = root.loadedModels || []
        var query = root.searchText.trim().toLowerCase()
        if (query === "") return models

        var out = []
        for (var i = 0; i < models.length; i++) {
            var item = models[i]
            var haystack = ((item.title || "") + " " + (item.modelId || "") + " " + (item.runtime || "") + " " + (item.runtimeVersion || "")).toLowerCase()
            if (haystack.indexOf(query) >= 0) out.push(item)
        }
        return out
    }

    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    parent: Overlay.overlay
    width: Math.min(940, Math.max(680, parent ? parent.width - 112 : 800))
    height: Math.min(640, Math.max(460, parent ? parent.height - 136 : 540))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    onOpened: root.searchText = ""

    background: Rectangle {
        color: Qt.rgba(0.075, 0.075, 0.115, 0.98)
        radius: 14
        border.color: Qt.rgba(1, 1, 1, 0.14)
        border.width: 1
    }

    contentItem: ColumnLayout {
        width: root.width
        height: root.height
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 126
            Layout.leftMargin: Theme.paddingMedium
            Layout.rightMargin: Theme.paddingMedium
            Layout.topMargin: Theme.paddingMedium
            Layout.bottomMargin: Theme.paddingSmall
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium

                Rectangle {
                    width: 42
                    height: 42
                    radius: 12
                    color: Qt.rgba(0.49, 0.30, 1.0, 0.16)
                    border.color: Qt.rgba(0.70, 0.55, 1.0, 0.34)

                    LineIcon {
                        anchors.centerIn: parent
                        name: "gallery"
                        color: Theme.accentLight
                        width: 20
                        height: 20
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: qsTr("Loaded Models")
                        color: Theme.textPrimary
                        font.pixelSize: 19
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Switch the workflow default or unload resident model instances.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                }

                StatusBadge {
                    text: root.statusText
                    labelColor: root.statusColor
                    borderColor: Qt.rgba(root.statusColor.r, root.statusColor.g, root.statusColor.b, 0.42)
                    fillColor: Qt.rgba(root.statusColor.r, root.statusColor.g, root.statusColor.b, 0.13)
                }

                IconButton {
                            iconName: "close"
                    toolTip: qsTr("Close")
                    onClicked: root.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    placeholderText: qsTr("Filter loaded models...")
                    text: root.searchText
                    selectByMouse: true
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    background: Rectangle {
                        color: Qt.rgba(1, 1, 1, 0.045)
                        border.color: searchField.activeFocus ? Qt.rgba(0.49, 0.30, 1.0, 0.62) : Qt.rgba(1, 1, 1, 0.085)
                        border.width: 1
                        radius: 10
                    }
                    leftPadding: 14
                    rightPadding: 14
                    onTextChanged: root.searchText = text
                }

                Rectangle {
                    Layout.preferredWidth: 148
                    Layout.preferredHeight: 38
                    radius: 10
                    color: Qt.rgba(1, 1, 1, 0.035)
                    border.color: Qt.rgba(1, 1, 1, 0.075)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: qsTr("Resident")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.loadedModels ? root.loadedModels.length : 0
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMedium
                            font.bold: true
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: root.width - Theme.paddingMedium * 2
                x: Theme.paddingMedium
                spacing: 10

                Item { Layout.fillWidth: true; Layout.preferredHeight: 4 }

                Repeater {
                    model: root.filteredModels()

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 82
                        radius: 12
                        color: modelData.active ? Qt.rgba(0.49, 0.30, 1.0, 0.15) : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(1, 1, 1, 0.028))
                        border.color: modelData.active ? Qt.rgba(0.70, 0.55, 1.0, 0.36) : Qt.rgba(1, 1, 1, 0.075)
                        border.width: 1

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            radius: 2
                            visible: modelData.active
                            color: Theme.accentLight
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.paddingMedium + 4
                            anchors.rightMargin: Theme.paddingSmall
                            spacing: Theme.paddingMedium

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: 11
                                color: modelData.active ? Qt.rgba(0.49, 0.30, 1.0, 0.22) : Qt.rgba(1, 1, 1, 0.045)
                                border.color: modelData.active ? Qt.rgba(0.70, 0.55, 1.0, 0.34) : Qt.rgba(1, 1, 1, 0.075)

                                LineIcon {
                                    anchors.centerIn: parent
                                    name: modelData.active ? "check" : "cpu"
                                    color: modelData.active ? Theme.accentLight : Theme.textSecondary
                                    width: 18
                                    height: 18
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 7

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingSmall

                                    Text {
                                        text: modelData.title || qsTr("Loaded model")
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMedium
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    StatusBadge {
                                        text: modelData.status || qsTr("Loaded")
                                        labelColor: Theme.success
                                        fillColor: Qt.rgba(0.22, 0.62, 0.39, 0.12)
                                        borderColor: Qt.rgba(0.22, 0.62, 0.39, 0.34)
                                    }

                                    StatusBadge {
                                        visible: modelData.active
                                        text: qsTr("Default")
                                        labelColor: Theme.success
                                        fillColor: Qt.rgba(0.22, 0.62, 0.39, 0.16)
                                        borderColor: Qt.rgba(0.22, 0.62, 0.39, 0.42)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Text {
                                        text: modelData.runtime || qsTr("Runtime not reported")
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        elide: Text.ElideRight
                                        Layout.maximumWidth: 220
                                    }

                                    Rectangle {
                                        width: 4
                                        height: 4
                                        radius: 2
                                        color: Qt.rgba(1, 1, 1, 0.22)
                                    }

                                    Text {
                                        visible: (modelData.modelId || "") !== ""
                                        text: modelData.modelId || ""
                                        color: Theme.textSecondary
                                        opacity: 0.8
                                        font.pixelSize: Theme.fontSmall
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            DialogButton {
                                text: modelData.active ? qsTr("Default") : qsTr("Use")
                                enabled: !modelData.active && !root.processing
                                implicitWidth: 104
                                buttonColor: modelData.active ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0.49, 0.30, 1.0, 0.18)
                                borderColor: modelData.active ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0.49, 0.30, 1.0, 0.42)
                                labelColor: modelData.active ? Theme.textSecondary : Theme.textPrimary
                                onClicked: {
                                    root.close()
                                    root.activateRequested(modelData.id || "")
                                }
                            }

                            DialogButton {
                                text: qsTr("Unload")
                                enabled: !root.processing && modelData.status !== "Loading" && modelData.status !== "Unloading"
                                implicitWidth: 104
                                buttonColor: Qt.rgba(0.93, 0.24, 0.24, 0.11)
                                borderColor: Qt.rgba(0.93, 0.24, 0.24, 0.38)
                                labelColor: "#ff8a8a"
                                onClicked: {
                                    root.close()
                                    root.unloadRequested(modelData.id || "")
                                }
                            }
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: Theme.paddingLarge
                    visible: root.filteredModels().length === 0
                    spacing: Theme.paddingSmall

                    Text {
                        Layout.fillWidth: true
                        text: (root.loadedModels && root.loadedModels.length > 0) ? qsTr("No models match the current filter") : qsTr("No loaded models")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Load a model to keep it available for quick switching in this workflow.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: 4 }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.leftMargin: Theme.paddingMedium
            Layout.rightMargin: Theme.paddingMedium
            spacing: Theme.paddingSmall

            Text {
                Layout.fillWidth: true
                text: qsTr("Resident models stay in memory. The Default model is used by this workflow and API calls without an explicit model.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }

            DialogButton {
                text: qsTr("Configure")
                enabled: root.loadedModels && root.loadedModels.length > 0
                implicitWidth: 116
                onClicked: {
                    root.close()
                    root.configureCurrentRequested()
                }
            }

            DialogButton {
                text: qsTr("Load Model...")
                implicitWidth: 128
                buttonColor: Qt.rgba(0.49, 0.30, 1.0, 0.84)
                borderColor: Qt.rgba(0.70, 0.55, 1.0, 0.55)
                labelColor: "#ffffff"
                onClicked: {
                    root.close()
                    root.loadAnotherRequested()
                }
            }
        }
    }

    component IconButton: Button {
        id: iconButton
        property string iconName: ""
        property string toolTip: ""

        implicitWidth: 36
        implicitHeight: 36
        flat: true

        AppToolTip {
            text: iconButton.toolTip
            visible: iconButton.hovered && iconButton.toolTip !== ""
        }

        contentItem: LineIcon {
            name: iconButton.iconName
            color: iconButton.hovered ? Theme.textPrimary : Theme.textSecondary
            anchors.centerIn: parent
            width: 17
            height: 17
        }

        background: Rectangle {
            radius: 8
            color: iconButton.hovered ? Qt.rgba(1, 1, 1, 0.07) : "transparent"
        }
    }

    component DialogButton: Button {
        id: dialogButton
        property color buttonColor: Qt.rgba(1, 1, 1, 0.055)
        property color borderColor: Qt.rgba(1, 1, 1, 0.12)
        property color labelColor: Theme.textPrimary

        implicitWidth: 96
        implicitHeight: 34
        padding: 0

        contentItem: Text {
            text: dialogButton.text
            color: dialogButton.enabled ? dialogButton.labelColor : Theme.textSecondary
            opacity: dialogButton.enabled ? 1.0 : 0.55
            font.pixelSize: Theme.fontSmall
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8
            color: dialogButton.enabled && dialogButton.hovered ? Qt.lighter(dialogButton.buttonColor, 1.18) : dialogButton.buttonColor
            border.color: dialogButton.borderColor
            border.width: 1
        }

        HoverHandler { cursorShape: dialogButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }

    component StatusBadge: Rectangle {
        id: badge
        property alias text: badgeText.text
        property color labelColor: Theme.textSecondary
        property color fillColor: Qt.rgba(1, 1, 1, 0.05)
        property color borderColor: Qt.rgba(1, 1, 1, 0.12)

        radius: 6
        color: fillColor
        border.color: borderColor
        border.width: 1
        implicitWidth: badgeText.implicitWidth + 12
        implicitHeight: 22

        Text {
            id: badgeText
            anchors.centerIn: parent
            color: badge.labelColor
            font.pixelSize: 10
            font.bold: true
        }
    }
}
