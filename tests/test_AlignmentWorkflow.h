#pragma once

#include <QObject>

namespace LAStudio {

class TestAlignmentWorkflow final : public QObject {
    Q_OBJECT

private slots:
    void missingDependenciesAreExposedAsWorkflowNodes();
    void resolverProducesTypedGraphAndStableSignature();
    void resolverNormalizesNemotronLanguageForSttStage();
    void installedAnchorModelsResolveConcreteArtifactFiles();
    void sessionCanBeInvalidated();
};

} // namespace LAStudio
