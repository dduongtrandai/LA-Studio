import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Dialog {
    id: root

    property string modelId: ""
    property string modelName: ""
    property string virtualPath: ""
    property string concretePath: ""
    property string format: ""
    property string quant: ""

    width: Math.min(580, parent ? parent.width - 32 : 580)
    height: contentLayout.implicitHeight + Theme.paddingLarge * 2
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    padding: Theme.paddingLarge
    title: ""

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
        id: contentLayout
        spacing: Theme.paddingLarge

        // Title Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingMedium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Model dependencies")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("Select the model components to reveal")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                    Layout.fillWidth: true
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

        // List of Items
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            // Row 1: Virtual Metadata Folder
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium

                RowLayout {
                    spacing: Theme.paddingSmall
                    Layout.maximumWidth: parent.width * 0.6

                    LineIcon {
                        name: "folder"
                        color: Theme.accentLight
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        strokeWidth: 1.8
                    }

                    Text {
                        text: root.modelId
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }

                // Dotted Leader Line
                Canvas {
                    id: dottedLine1
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 8

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.3)
                        ctx.lineWidth = 1
                        ctx.setLineDash([1, 3])
                        ctx.beginPath()
                        ctx.moveTo(0, 0)
                        ctx.lineTo(width, 0)
                        ctx.stroke()
                    }
                    onWidthChanged: requestPaint()
                }

                PrimaryButton {
                    text: qsTr("Open in File Explorer")
                    quiet: true
                    implicitWidth: 154
                    implicitHeight: 28
                    textColor: hovered ? Theme.textPrimary : Theme.textSecondary
                    onClicked: {
                        Qt.openUrlExternally("file:///" + root.virtualPath)
                        root.accept()
                    }
                }
            }

            // Row 2: Concrete Weights Folder
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium

                RowLayout {
                    spacing: Theme.paddingSmall
                    Layout.maximumWidth: parent.width * 0.6

                    // Curved Branch Drawing
                    Canvas {
                        id: branchCanvas
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 20
                        Layout.leftMargin: 4

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.5)
                            ctx.lineWidth = 1.2
                            ctx.beginPath()
                            ctx.moveTo(8, 0)
                            ctx.lineTo(8, 10)
                            ctx.lineTo(16, 10)
                            ctx.stroke()
                        }
                    }

                    LineIcon {
                        name: "file"
                        color: Theme.textSecondary
                        Layout.preferredWidth: 15
                        Layout.preferredHeight: 15
                        strokeWidth: 1.6
                    }

                    Text {
                        text: root.modelName
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }

                    // Quant label
                    Text {
                        visible: root.quant !== ""
                        text: root.quant
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                    }

                    // Format Badge
                    Rectangle {
                        visible: root.format !== ""
                        width: formatText.implicitWidth + 10
                        height: 18
                        radius: 4
                        color: Qt.rgba(0.49, 0.30, 1.0, 0.12)
                        border.color: Qt.rgba(0.49, 0.30, 1.0, 0.3)
                        border.width: 1

                        Text {
                            id: formatText
                            anchors.centerIn: parent
                            text: root.format
                            color: Theme.accentLight
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                // Dotted Leader Line
                Canvas {
                    id: dottedLine2
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 8

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.3)
                        ctx.lineWidth = 1
                        ctx.setLineDash([1, 3])
                        ctx.beginPath()
                        ctx.moveTo(0, 0)
                        ctx.lineTo(width, 0)
                        ctx.stroke()
                    }
                    onWidthChanged: requestPaint()
                }

                PrimaryButton {
                    text: qsTr("Open in File Explorer")
                    quiet: true
                    implicitWidth: 154
                    implicitHeight: 28
                    textColor: hovered ? Theme.textPrimary : Theme.textSecondary
                    onClicked: {
                        Qt.openUrlExternally("file:///" + root.concretePath)
                        root.accept()
                    }
                }
            }
        }
    }
}
