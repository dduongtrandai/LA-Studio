import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Dialog {
    id: root

    property var nodes: []
    property var nodeConfigurations: ({})
    property string nodeId: ""
    property string capabilityId: ""
    signal configurationAccepted(string nodeId, string familyId, string runtimeId,
                                 string runtimeVersion, var selectedFiles)

    function openFor(value) {
        var item = root.nodes ? root.nodes.find(function(entry) { return entry.id === value }) : null
        if (!item || item.configurable !== true) return

        openForCapability(value, item.capabilityId || "stt")
    }

    function openForCapability(value, capability) {
        root.nodeId = value
        root.capabilityId = capability
        modelController.familiesModel.setCapability(root.capabilityId)

        var saved = root.nodeConfigurations[root.nodeId] || {}
        var preferredFamilyId = saved.familyId || item.selectedFamilyId
                                || item.defaultFamilyId || ""
        var recommended = preferredFamilyId !== ""
                          ? modelController.familiesModel.configurationForFamily(preferredFamilyId)
                          : modelController.familiesModel.recommendedConfiguration()
        var familyId = preferredFamilyId || recommended.familyId
            || modelController.familiesModel.firstFamilyId()

        modelController.openConfiguration(familyId)
        gallery.selectedFamilyId = familyId
        gallery.initialSelectedFiles = saved.selectedFiles || recommended.selectedFiles || ({})
        gallery.pendingRuntimeId = saved.runtimeId || recommended.runtimeId || ""
        gallery.pendingRuntimeVersion = saved.runtimeVersion || recommended.runtimeVersion || ""
        root.open()
    }

    StudioPageController {
        id: modelController
        capabilityId: root.capabilityId
        autoLoadOnSync: false
    }

    parent: Overlay.overlay
    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(1260, Math.max(980, parent ? parent.width - Theme.paddingXL * 2 : 1100))
    height: Math.min(780, Math.max(560, parent ? parent.height - Theme.paddingXL * 2 : 680))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    standardButtons: Dialog.NoButton

    background: Rectangle {
        color: Theme.background
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1
    }

    contentItem: CapabilityGallery {
        id: gallery
        capability: root.capabilityId
        modalMode: true
        familiesModel: modelController.familiesModel
        selectedFamilyId: modelController.selectedFamilyId
        initialSelectedFiles: modelController.selectedFiles

        onFamilySelected: function(familyId) {
            modelController.selectFamily(familyId)
        }

        onConfigurationAccepted: function(familyId, runtimeId, runtimeVersion, selectedFiles) {
            root.configurationAccepted(root.nodeId, familyId, runtimeId, runtimeVersion, selectedFiles)
            root.close()
        }
    }
}
