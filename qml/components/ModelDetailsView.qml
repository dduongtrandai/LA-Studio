import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Rectangle {
    id: root
    
    property var modelData: null
    signal downloadRequested(string filename)

    color: Theme.surface
    radius: Theme.radiusMedium
    clip: true

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width - Theme.paddingXL * 2
            anchors.margins: Theme.paddingXL
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: Theme.paddingLarge
            visible: modelData !== null

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium

                Rectangle {
                    width: 32; height: 32; radius: 16
                    color: Theme.surfaceAlt
                    Text { anchors.centerIn: parent; text: "🤖"; font.pixelSize: 18 }
                }

                Text {
                    text: root.modelData ? (root.modelData.id || root.modelData.modelId || "") : ""
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    flat: true
                    text: "📋"
                    onClicked: AppController.copyToClipboard(root.modelData.id)
                    AppToolTip {
                        visible: parent.hovered
                        text: qsTr("Copy Model ID")
                    }
                }

                Button {
                    flat: true
                    text: "✕"
                    onClicked: root.modelData = null
                }
            }

            // Stats & Format
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingLarge

                RowLayout {
                    spacing: 4
                    Rectangle {
                        width: 80; height: 32; radius: 6; color: Theme.surfaceAlt
                        RowLayout {
                            anchors.centerIn: parent; spacing: 6
                            Text { text: "⬇"; color: Theme.textSecondary; font.pixelSize: 12 }
                            Text {
                                text: (root.modelData ? (root.modelData.downloads || 0) : 0).toLocaleString()
                                color: Theme.textPrimary; font.pixelSize: 12; font.bold: true
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 4
                    Rectangle {
                        width: 60; height: 32; radius: 6; color: Theme.surfaceAlt
                        RowLayout {
                            anchors.centerIn: parent; spacing: 6
                            Text { text: "❤"; color: Theme.textSecondary; font.pixelSize: 12 }
                            Text {
                                text: (root.modelData ? (root.modelData.likes || 0) : 0).toLocaleString()
                                color: Theme.textPrimary; font.pixelSize: 12; font.bold: true
                            }
                        }
                    }
                }

                Text {
                    text: qsTr("Last updated: %1").arg(root.modelData ? formatTime(root.modelData.lastModified) : "")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                Item { Layout.fillWidth: true }
                
                PrimaryButton {
                    text: qsTr("Model Card ↗")
                    buttonColor: Theme.surfaceAlt
                    textColor: Theme.textPrimary
                    onClicked: {
                        var id = root.modelData.id || root.modelData.modelId || ""
                        if (id) Qt.openUrlExternally("https://huggingface.co/" + id)
                    }
                }
            }

            RowLayout {
                spacing: 8
                Text { text: qsTr("Format"); color: Theme.textSecondary; font.pixelSize: 12 }
                Rectangle {
                    implicitWidth: 60; implicitHeight: 20; color: "#3b82f6"; radius: 4
                    Text { 
                        anchors.centerIn: parent
                        text: {
                            if (!root.modelData) return qsTr("UNKNOWN")
                            var tags = root.modelData.tags || []
                            if (tags.includes("gguf")) return "GGUF"
                            if (tags.includes("ggml")) return "GGML"
                            if (tags.includes("safetensors")) return "Safetensors"
                            if (tags.includes("onnx")) return "ONNX"
                            return qsTr("Bin")
                        }
                        color: "white"; font.pixelSize: 9; font.bold: true 
                    }
                }
            }

            // Download Options
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: contentColumn.implicitHeight + Theme.paddingLarge * 2
                color: Theme.surfaceAlt
                radius: Theme.radiusMedium
                border.color: Theme.background
                border.width: 1

                ColumnLayout {
                    id: contentColumn
                    anchors.fill: parent
                    anchors.margins: Theme.paddingLarge
                    spacing: Theme.paddingMedium

                    Text {
                        text: qsTr("📦 Download Options")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontLarge
                        font.bold: true
                    }

                    ListView {
                        id: fileList
                        Layout.fillWidth: true
                        implicitHeight: Math.min(300, contentHeight)
                        clip: true
                        spacing: 8
                        model: root.modelData ? root.modelData.files : []
                        visible: count > 0
                        
                        delegate: Rectangle {
                            width: fileList.width
                            height: 50
                            color: Theme.surface
                            radius: Theme.radiusSmall
                            border.color: Theme.background
                            border.width: 1
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.paddingMedium
                                
                                Rectangle {
                                    implicitWidth: 60
                                    implicitHeight: 24
                                    color: "transparent"
                                    border.color: Theme.textSecondary
                                    border.width: 1
                                    radius: 4
                                    Text {
                                        anchors.centerIn: parent
                                        text: qsTr("File")
                                        color: Theme.textPrimary
                                        font.pixelSize: 10
                                    }
                                }

                                Text {
                                    text: modelData.rfilename || ""
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMedium
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: formatSize(modelData.size)
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                }

                                PrimaryButton {
                                    text: qsTr("Download")
                                    implicitWidth: 100
                                    implicitHeight: 32
                                    font.pixelSize: Theme.fontSmall
                                    onClicked: root.downloadRequested(modelData.rfilename)
                                }
                            }
                        }
                    }

                    Text {
                        visible: fileList.count === 0
                        text: qsTr("No downloadable files found for this model.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // Model Info
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium

                Text {
                    text: qsTr("📄 Model Info")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: infoText.height + Theme.paddingLarge
                    color: "transparent"

                    Text {
                        id: infoText
                        width: parent.width
                        wrapMode: Text.WordWrap
                        textFormat: Text.RichText
                        text: root.modelData ? qsTr("<b>Task:</b> %1<br><br><b>Tags:</b> %2").arg(root.modelData.task || qsTr("Unknown")).arg(root.modelData.tags ? root.modelData.tags.join(', ') : qsTr("None")) : ""
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontMedium
                    }
                }
            }
        }
    }

    function formatTime(isoString) {
        if (!isoString) return qsTr("unknown")
        var date = new Date(isoString)
        var now = new Date()
        var diff = now - date
        var days = Math.floor(diff / (1000 * 60 * 60 * 24))
        if (days === 0) return qsTr("today")
        if (days === 1) return qsTr("yesterday")
        return qsTr("%1 days ago").arg(days)
    }

    function formatSize(s) {
        if (!s) return "unknown"
        if (s > 1024*1024*1024) return (s/(1024*1024*1024)).toFixed(2) + " GB"
        if (s > 1024*1024) return (s/(1024*1024)).toFixed(2) + " MB"
        return (s/1024).toFixed(2) + " KB"
    }

    // Empty state
    ColumnLayout {
        anchors.centerIn: parent
        visible: modelData === null
        spacing: Theme.paddingLarge
        
        Text {
            text: qsTr("Select a model to see details")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontLarge
            Layout.alignment: Qt.AlignHCenter
        }
    }
}

