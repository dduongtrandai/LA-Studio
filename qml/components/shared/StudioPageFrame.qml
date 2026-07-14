import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Item {
    id: root

    property string capabilityId: ""
    property Component contentView: null
    property alias studioController: studioController
    property alias studioContext: studioContext

    StudioPageController {
        id: studioController
        capabilityId: root.capabilityId
    }

    StudioContext {
        id: studioContext
        capability: root.capabilityId
        familyId: studioController.selectedFamilyId
        runtimeId: studioController.runtimeId
        runtimeVersion: studioController.runtimeVersion
    }

    function openConfiguration(familyId) {
        studioController.openConfiguration(familyId)
        configurationDialog.open()
    }

    Connections {
        target: studioController
        
        function onConfigurationDialogClosed() {
            configurationDialog.close()
        }
        
        function onConfigurationGalleryRequestReset() {
            configurationGallery.searchText = ""
            configurationGallery.pendingRuntimeId = studioController.runtimeId
            configurationGallery.pendingRuntimeVersion = studioController.runtimeVersion
            configurationGallery.initialSelectedFiles = studioController.selectedFiles
        }
    }

    Dialog {
        id: configurationDialog
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        parent: Overlay.overlay
        width: Math.min(1260, Math.max(980, parent.width - 48))
        height: Math.min(780, Math.max(560, parent.height - 48))
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        background: Rectangle {
            color: Qt.rgba(0.06, 0.06, 0.09, 0.92)
            radius: Theme.radiusMedium
            border.color: Qt.rgba(1, 1, 1, 0.10)
            border.width: 1
        }

        contentItem: Item {
            width: configurationDialog.width
            height: configurationDialog.height

            CapabilityGallery {
                id: configurationGallery
                anchors.fill: parent
                capability: root.capabilityId
                modalMode: true
                familiesModel: studioController.familiesModel
                selectedFamilyId: studioController.selectedFamilyId
                onFamilySelected: function(familyId) {
                    initialSelectedFiles = ({})
                    studioController.selectFamily(familyId)
                }
                onConfigurationAccepted: function(familyId, runtimeId, runtimeVersion, selectedFiles) {
                    studioController.commitConfigurationSelection(familyId, runtimeId, runtimeVersion, selectedFiles)
                }
            }
        }
    }

    Loader {
        id: contentLoader
        anchors.fill: parent
        sourceComponent: root.contentView

        Binding {
            target: contentLoader.item
            property: "studioContext"
            value: studioContext
        }
        Binding {
            target: contentLoader.item
            property: "studioController"
            value: studioController
        }
    }
}
