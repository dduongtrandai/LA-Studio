import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root

    property alias dropEnabled: dropArea.enabled
    property bool showInfo: true
    signal fileDropped(string filePath)

    implicitHeight: 180
    radius: Theme.radiusLarge
    color: dropArea.containsDrag ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1) : Theme.surface
    border.color: dropArea.containsDrag ? Theme.accent : Theme.surfaceAlt
    border.width: 2

    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }

    DropArea {
        id: dropArea
        anchors.fill: parent
        keys: ["text/uri-list"]

        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                var url = drop.urls[0].toString()
                if (url.startsWith("file:///"))
                    url = url.substring(8)
                else if (url.startsWith("file://"))
                    url = url.substring(7)
                
                // Fix for Windows paths
                url = url.replace(/^\/([a-zA-Z]:)/, "$1")
                
                root.fileDropped(url)
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.paddingSmall
        width: parent.width - 40
        visible: root.showInfo

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "\u{EA3F}" 
            font.family: "Material Design Icons"
            font.pixelSize: 48
            color: dropArea.containsDrag ? Theme.accent : Theme.textSecondary
            opacity: 0.8
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Drop audio file here")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.bold: true
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("WAV, MP3, FLAC supported")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
        }
    }

    // Overlay for drag state
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: Theme.accent
        border.width: 3
        radius: Theme.radiusLarge
        visible: dropArea.containsDrag
        
        Rectangle {
            anchors.fill: parent
            color: Theme.accent
            opacity: 0.1
            radius: Theme.radiusLarge
        }
    }
}


