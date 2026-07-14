import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root
    
    property var itemData: null
    property bool selected: false
    signal clicked()

    height: 80
    radius: Theme.radiusSmall
    color: selected ? Theme.accent : (hoverHandler.hovered ? Theme.surfaceAlt : "transparent")
    
    HoverHandler { id: hoverHandler }
    TapHandler { onTapped: root.clicked() }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        // Icon/Emoji
        Rectangle {
            width: 40
            height: 40
            radius: 20
            color: root.selected ? Qt.rgba(1,1,1,0.2) : Theme.surface
            Text {
                anchors.centerIn: parent
                text: "🤖"
                font.pixelSize: 20
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                property string modelName: {
                    if (!root.itemData) return qsTr("Unnamed Model")
                    var id = root.itemData.id || root.itemData.modelId || ""
                    if (!id) return qsTr("Unnamed Model")
                    return id.split('/').pop()
                }
                text: modelName
                color: root.selected ? "#ffffff" : Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                text: root.itemData ? (root.itemData.author || "unknown") : ""
                color: root.selected ? "#e0e0e0" : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            spacing: 4
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignRight
                
                RowLayout {
                    spacing: 2
                    Text { text: "❤"; color: root.selected ? "#ffffff" : Theme.textSecondary; font.pixelSize: 10 }
                    Text {
                         text: root.itemData ? (root.itemData.likes !== undefined ? root.itemData.likes : 0) : 0
                         color: root.selected ? "#ffffff" : Theme.textSecondary
                         font.pixelSize: 10
                    }
                }
                RowLayout {
                    spacing: 2
                    Text { text: "⬇"; color: root.selected ? "#ffffff" : Theme.textSecondary; font.pixelSize: 10 }
                    Text {
                        text: root.itemData ? (root.itemData.downloads !== undefined ? root.itemData.downloads.toLocaleString() : 0) : 0
                        color: root.selected ? "#ffffff" : Theme.textSecondary
                        font.pixelSize: 10
                    }
                }
            }
            
            Text {
                text: root.itemData ? formatTime(root.itemData.lastModified) : ""
                color: root.selected ? "#ffffff" : Theme.textSecondary
                font.pixelSize: 10
                Layout.alignment: Qt.AlignRight
            }
        }
    }

    function formatTime(isoString) {
        if (!isoString) return ""
        var date = new Date(isoString)
        if (isNaN(date.getTime())) return ""
        var now = new Date()
        var diff = now - date
        var days = Math.floor(diff / (1000 * 60 * 60 * 24))
        if (days < 0) return qsTr("just now")
        if (days === 0) return qsTr("today")
        if (days === 1) return qsTr("yesterday")
        return qsTr("%1 days ago").arg(days)
    }
}

