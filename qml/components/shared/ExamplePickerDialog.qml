import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"
import ".."

Dialog {
    id: root

    property var examples: []
    property string taskTitle: qsTr("Examples")
    property string emptyText: qsTr("No examples are available for the selected model.")

    signal exampleSelected(var example)

    width: Math.min(760, parent ? parent.width - 48 : 760)
    height: Math.min(620, parent ? parent.height - 48 : 620)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    padding: Theme.paddingLarge
    title: ""

    function capabilitiesText(example) {
        var caps = example && example.capabilities ? example.capabilities : []
        return caps.length > 0 ? caps.join("  ·  ") : qsTr("No capability metadata")
    }

    function modelText(example) {
        var model = example && example.model ? example.model : {}
        if (model.required && model.familyId) return qsTr("Requires %1").arg(model.familyId)
        if (model.familyId) return qsTr("Suggested: %1").arg(model.familyId)
        return qsTr("Generic example")
    }

    function inputPreview(example) {
        var inputs = example && example.inputs ? example.inputs : {}
        var text = inputs.text || ""
        return text.length > 0 ? text : qsTr("No prompt preview")
    }

    function settingsText(example) {
        var settings = example && example.settings ? example.settings : {}
        var parts = []
        var keys = ["lang", "voice", "temperature", "top_k", "top_p", "max_new_frames"]
        for (var i = 0; i < keys.length; ++i) {
            var key = keys[i]
            if (settings[key] !== undefined) parts.push(key + ": " + settings[key])
        }
        return parts.length > 0 ? parts.join("  ·  ") : qsTr("Default settings")
    }

    function referenceText(example) {
        var inputs = example && example.inputs ? example.inputs : {}
        if (inputs.referenceAudioPath) return qsTr("Reference audio ready")
        if (inputs.referenceAudio) return qsTr("Reference audio: %1").arg(inputs.referenceAudio)
        return ""
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Theme.surfaceAlt
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.45)
            border.width: 1
            radius: Theme.radiusMedium + 1
            z: -1
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.paddingLarge

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingMedium

            LineIcon {
                name: "file"
                color: Theme.accentLight
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: root.taskTitle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 examples available").arg(root.examples.length)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
            }

            Button {
                id: closeButton
                implicitWidth: 32
                implicitHeight: 32
                onClicked: root.close()
                contentItem: LineIcon {
                    name: "close"
                    color: closeButton.hovered ? Theme.textPrimary : Theme.textSecondary
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                }
                background: Rectangle {
                    radius: 6
                    color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.surfaceAlt
        }

        Text {
            Layout.fillWidth: true
            visible: root.examples.length === 0
            text: root.emptyText
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        ListView {
            id: examplesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.paddingMedium
            model: root.examples

            delegate: Rectangle {
                width: examplesList.width
                implicitHeight: cardLayout.implicitHeight + Theme.paddingMedium * 2
                radius: Theme.radiusSmall
                color: exampleMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.045) : Qt.rgba(1, 1, 1, 0.025)
                border.color: exampleMouse.containsMouse ? Qt.rgba(0.49, 0.30, 1.0, 0.55) : Qt.rgba(1, 1, 1, 0.08)
                border.width: 1

                MouseArea {
                    id: exampleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onDoubleClicked: {
                        root.exampleSelected(modelData)
                        root.accept()
                    }
                }

                ColumnLayout {
                    id: cardLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.paddingMedium
                    spacing: Theme.paddingSmall

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall

                        Text {
                            Layout.fillWidth: true
                            text: modelData.title || modelData.id
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMedium
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            radius: 5
                            color: Qt.rgba(0.49, 0.30, 1.0, 0.13)
                            border.color: Qt.rgba(0.49, 0.30, 1.0, 0.35)
                            border.width: 1
                            implicitWidth: modelLabel.implicitWidth + 12
                            implicitHeight: 22

                            Text {
                                id: modelLabel
                                anchors.centerIn: parent
                                text: root.modelText(modelData)
                                color: Theme.accentLight
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.inputPreview(modelData)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.capabilitiesText(modelData)
                        color: Theme.accentLight
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.settingsText(modelData)
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall

                        Text {
                            Layout.fillWidth: true
                            text: root.referenceText(modelData)
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        PrimaryButton {
                            text: qsTr("Use Example")
                            iconName: "spark"
                            implicitWidth: 126
                            implicitHeight: 30
                            onClicked: {
                                root.exampleSelected(modelData)
                                root.accept()
                            }
                        }
                    }
                }
            }
        }
    }
}

