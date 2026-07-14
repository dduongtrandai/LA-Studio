import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root

    property string modelId: ""
    property string filename: ""
    property real bytesReceived: 0
    property real bytesTotal: 0
    property real progress: bytesTotal > 0 ? bytesReceived / bytesTotal : 0

    implicitHeight: 56
    radius: Theme.radiusSmall
    color: Theme.surfaceAlt

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: root.filename
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Text {
                text: {
                    if (root.bytesTotal <= 0) return "..."
                    var pct = (root.progress * 100).toFixed(1)
                    var mb = (root.bytesReceived / 1048576).toFixed(1)
                    var total = (root.bytesTotal / 1048576).toFixed(1)
                    return mb + " / " + total + " MB (" + pct + "%)"
                }
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 6
            radius: 3
            color: Theme.surface

            Rectangle {
                width: parent.width * root.progress
                height: parent.height
                radius: 3
                color: Theme.accent

                Behavior on width { NumberAnimation { duration: 200 } }
            }
        }
    }
}

