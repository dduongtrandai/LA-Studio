import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root

    property string modelId: ""
    property string modelName: ""
    property string author: ""
    property string task: ""
    property int downloadCount: 0
    property int likeCount: 0
    property var fileList: []
    property bool isDownloading: false

    signal downloadRequested(string filename)

    implicitHeight: contentLayout.implicitHeight + 2 * Theme.paddingLarge
    radius: Theme.radiusMedium
    color: Theme.surface
    border.color: cardHover.hovered ? Theme.accent : Theme.surfaceAlt
    border.width: 1

    HoverHandler { id: cardHover }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Text {
                text: root.modelName || root.modelId
                color: Theme.textPrimary
                font.pixelSize: Theme.fontLarge
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Rectangle {
                visible: root.task.length > 0
                implicitWidth: taskLabel.implicitWidth + 16
                implicitHeight: 24
                radius: 12
                color: Theme.accent
                Text {
                    id: taskLabel
                    anchors.centerIn: parent
                    text: root.task
                    color: "#ffffff"
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Text {
            visible: root.author.length > 0
            text: qsTr("by %1").arg(root.author)
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
        }

        RowLayout {
            spacing: Theme.paddingLarge
            Text {
                text: "\u{2B07} " + root.downloadCount.toLocaleString()
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }
            Text {
                text: "\u{2764} " + root.likeCount
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }
        }

        // File list with download buttons
        Flow {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            visible: root.fileList.length > 0

            Repeater {
                model: root.fileList
                delegate: PrimaryButton {
                    required property var modelData
                    text: modelData.rfilename || modelData
                    implicitHeight: 30
                    font.pixelSize: Theme.fontSmall
                    enabled: !root.isDownloading
                    onClicked: root.downloadRequested(modelData.rfilename || modelData)
                }
            }
        }
    }
}

