import QtQuick
import LAStudio

// Shared shell for Studios whose execution depends on multiple workflow stages.
// It keeps the standard Studio workspace and adds workflow state/action handling
// to the common top bar.
StudioShell {
    workflowMode: true
    workflowTitle: qsTr("Workflow")
    workflowStatusText: workflowReady ? qsTr("Workflow ready") : qsTr("Setup required")
    workflowActionText: workflowReady ? qsTr("View workflow") : qsTr("Set up workflow")
}
