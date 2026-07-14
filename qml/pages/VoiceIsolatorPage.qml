import QtQuick
import QtQuick.Controls
import "../components/shared"
import "../components/voiceisolator"

StudioPageFrame {
    id: isolatorPageFrame
    capabilityId: "voice-isolation"

    contentView: Component {
        VoiceIsolatorStudioView {
            studioController: isolatorPageFrame.studioController
            family: {
                var families = studioController.families
                for (var i = 0; i < families.length; ++i) {
                    if (families[i].id === studioController.selectedFamilyId) return families[i]
                }
                return null
            }
            families: studioController.families
            selectedFamilyId: studioController.selectedFamilyId
            studioReady: studioController.studioReady
            studioTitle: studioController.studioHeaderTitle
            modalSelectionTitle: studioController.modalSelectionTitle
            modalSelectionValue: studioController.modalSelectionValue
            modalSelectionDetail: studioController.modalSelectionDetail

            onBackToGallery: isolatorPageFrame.openConfiguration(selectedFamilyId)
            onReloadRequested: studioController.reload()
            onEjectRequested: studioController.unload()
            onModelSwitchRequested: function(nextFamilyId) {
                studioController.selectFamily(nextFamilyId)
                studioController.commitSelection()
            }
            onRuntimeSwitchRequested: function(nextRuntimeId) {
                studioController.selectRuntime(nextRuntimeId, "")
                studioController.commitSelection()
            }
        }
    }
}
