import QtQuick
import "../components/shared"
import "../components/llm"
import LAStudio

StudioPageFrame {
    id: frame
    capabilityId: "llm-chat"
    contentView: Component {
        LlmChatStudioView {
            studioController: frame.studioController
            onBackToGallery: frame.openConfiguration(frame.studioController ? frame.studioController.selectedFamilyId : "")
        }
    }
}
