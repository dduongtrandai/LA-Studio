#include "alignment/AlignmentWorkflowResolver.h"

#include "core/ModelManager.h"
#include "core/RuntimeManager.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace LAStudio {
namespace {

QString findFile(const QString &rootOrFile, const QStringList &names)
{
    const QFileInfo direct(rootOrFile);
    if (direct.isFile()) return direct.absoluteFilePath();
    QDirIterator it(rootOrFile, names, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

QString sttBackendForModel(const QVariantMap &model)
{
    const QString id = model.value(QStringLiteral("id")).toString().toLower();
    if (id.contains(QStringLiteral("qwen"))) return QStringLiteral("qwen3");
    if (id.contains(QStringLiteral("nemotron"))) return QStringLiteral("nemotron");
    return QStringLiteral("whisper");
}

QString sttLanguageForBackend(const QString &language, const QString &backend)
{
    const QString normalized = language.trimmed().toLower();
    if (backend != QStringLiteral("nemotron")) return normalized;
    static const QHash<QString, QString> iso3ToIso1{
        {QStringLiteral("eng"), QStringLiteral("en")}, {QStringLiteral("spa"), QStringLiteral("es")},
        {QStringLiteral("deu"), QStringLiteral("de")}, {QStringLiteral("ger"), QStringLiteral("de")},
        {QStringLiteral("fra"), QStringLiteral("fr")}, {QStringLiteral("fre"), QStringLiteral("fr")},
        {QStringLiteral("ita"), QStringLiteral("it")}, {QStringLiteral("ara"), QStringLiteral("ar")},
        {QStringLiteral("jpn"), QStringLiteral("ja")}, {QStringLiteral("kor"), QStringLiteral("ko")},
        {QStringLiteral("por"), QStringLiteral("pt")}, {QStringLiteral("rus"), QStringLiteral("ru")},
        {QStringLiteral("hin"), QStringLiteral("hi")}, {QStringLiteral("zho"), QStringLiteral("zh")},
        {QStringLiteral("chi"), QStringLiteral("zh")}, {QStringLiteral("cmn"), QStringLiteral("zh")},
        {QStringLiteral("vie"), QStringLiteral("vi")}, {QStringLiteral("heb"), QStringLiteral("he")},
        {QStringLiteral("nld"), QStringLiteral("nl")}, {QStringLiteral("dut"), QStringLiteral("nl")},
        {QStringLiteral("ces"), QStringLiteral("cs")}, {QStringLiteral("cze"), QStringLiteral("cs")},
        {QStringLiteral("dan"), QStringLiteral("da")}, {QStringLiteral("pol"), QStringLiteral("pl")},
        {QStringLiteral("nor"), QStringLiteral("no")}, {QStringLiteral("swe"), QStringLiteral("sv")},
        {QStringLiteral("tha"), QStringLiteral("th")}, {QStringLiteral("tur"), QStringLiteral("tr")},
        {QStringLiteral("bul"), QStringLiteral("bg")}, {QStringLiteral("ell"), QStringLiteral("el")},
        {QStringLiteral("gre"), QStringLiteral("el")}, {QStringLiteral("est"), QStringLiteral("et")},
        {QStringLiteral("fin"), QStringLiteral("fi")}, {QStringLiteral("hrv"), QStringLiteral("hr")},
        {QStringLiteral("hun"), QStringLiteral("hu")}, {QStringLiteral("lit"), QStringLiteral("lt")},
        {QStringLiteral("lav"), QStringLiteral("lv")}, {QStringLiteral("ron"), QStringLiteral("ro")},
        {QStringLiteral("rum"), QStringLiteral("ro")}, {QStringLiteral("slk"), QStringLiteral("sk")},
        {QStringLiteral("slo"), QStringLiteral("sk")}, {QStringLiteral("ukr"), QStringLiteral("uk")},
        {QStringLiteral("mlt"), QStringLiteral("mt")}, {QStringLiteral("slv"), QStringLiteral("sl")}
    };
    return iso3ToIso1.value(normalized, normalized);
}

QString signatureFor(const QVariantMap &request)
{
    const QVariantMap identity{{QStringLiteral("mode"), request.value(QStringLiteral("mode"))},
                               {QStringLiteral("runtimeId"), request.value(QStringLiteral("runtimeId"))},
                               {QStringLiteral("runtimeVersion"), request.value(QStringLiteral("runtimeVersion"))},
                               {QStringLiteral("sttRuntimeId"), request.value(QStringLiteral("sttRuntimeId"))},
                               {QStringLiteral("sttRuntimeVersion"), request.value(QStringLiteral("sttRuntimeVersion"))},
                               {QStringLiteral("modelId"), request.value(QStringLiteral("modelId"))},
                               {QStringLiteral("sttModel"), request.value(QStringLiteral("sttModel"))},
                               {QStringLiteral("selectedFiles"), request.value(QStringLiteral("selectedFiles"))}};
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(QJsonObject::fromVariantMap(identity)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}

WorkflowResource resource(const QString &id, WorkflowNodeKind kind, const QString &title,
                          const QString &resourceId, const QString &path, const QString &status,
                          bool ready, const QString &errorCode)
{
    WorkflowResource item;
    item.id = id;
    item.kind = kind;
    item.title = title;
    item.resourceId = resourceId;
    item.resolvedPath = path;
    item.state = ready ? WorkflowNodeState::Ready : WorkflowNodeState::Missing;
    item.errorCode = ready ? QString() : errorCode;
    item.statusText = status;
    return item;
}

WorkflowPlanNode modelStage(const QString &id, const QString &title, const QString &modelId,
                        const QString &modelPath, const QString &missingModelText,
                        const QString &modelErrorCode, const WorkflowResource &provider)
{
    const bool modelReady = QFileInfo(modelPath).exists();
    WorkflowPlanNode node;
    node.id = id;
    node.kind = WorkflowNodeKind::Model;
    node.title = title;
    node.resourceId = modelId;
    node.resolvedPath = modelPath;
    node.providerResourceId = provider.id;
    node.providerName = provider.title;
    node.providerState = provider.state;
    node.providerStatusText = provider.statusText;
    if (!modelReady) {
        node.state = WorkflowNodeState::Missing;
        node.errorCode = modelErrorCode;
        node.statusText = missingModelText;
    } else if (!provider.isReady()) {
        node.state = WorkflowNodeState::Blocked;
        node.errorCode = provider.errorCode;
        node.statusText = modelId;
    } else {
        node.state = WorkflowNodeState::Ready;
        node.statusText = modelId;
    }
    return node;
}

WorkflowPlanNode builtInStage(const QString &id, WorkflowNodeKind kind, const QString &title, const QString &detail)
{
    WorkflowPlanNode node;
    node.id = id;
    node.kind = kind;
    node.title = title;
    node.state = WorkflowNodeState::Ready;
    node.statusText = detail;
    node.providerName = QStringLiteral("LA Studio");
    node.providerState = WorkflowNodeState::Ready;
    node.providerStatusText = QStringLiteral("Built-in");
    return node;
}

void connectLinear(WorkflowPlan &plan)
{
    for (qsizetype index = 1; index < plan.nodes.size(); ++index)
        plan.edges.append({plan.nodes[index - 1].id, plan.nodes[index].id});
}

} // namespace

AlignmentWorkflowResolver::AlignmentWorkflowResolver(const RuntimeManager *runtimes, const ModelManager *models)
    : m_runtimes(runtimes), m_models(models)
{
}

WorkflowResolution AlignmentWorkflowResolver::resolve(const QVariantMap &request) const
{
    WorkflowResolution resolution;
    WorkflowPlan &plan = resolution.plan;
    plan.studioId = QStringLiteral("alignment");
    plan.workflowId = request.value(QStringLiteral("mode"), QStringLiteral("canonical")).toString() == QStringLiteral("canonical")
        ? QStringLiteral("canonical-alignment") : QStringLiteral("automatic-alignment");
    plan.signature = signatureFor(request);

    auto payload = std::make_shared<AlignmentWorkflowPayload>();
    payload->executionRequest = request;
    resolution.payload = payload;

    const QString runtimeId = request.value(QStringLiteral("runtimeId")).toString();
    const QString runtimeVersion = request.value(QStringLiteral("runtimeVersion")).toString();
    const QString sttRuntimeId = request.value(QStringLiteral("sttRuntimeId"), runtimeId).toString();
    const QString sttRuntimeVersion = request.value(QStringLiteral("sttRuntimeVersion"), runtimeVersion).toString();
    const QString modelId = request.value(QStringLiteral("modelId")).toString();
    const QString mode = request.value(QStringLiteral("mode"), QStringLiteral("canonical")).toString();
    const QString modelDir = m_models ? m_models->concreteModelDir(modelId) : QString();
    const bool modelReady = QFileInfo(modelDir).isDir();

    if (mode == QStringLiteral("canonical") && m_runtimes
        && m_runtimes->getRuntimeKindForVersion(runtimeId, runtimeVersion) == QStringLiteral("process")) {
        payload->directProcessExecution = true;
        const QString executable = m_runtimes->getRuntimeExecutablePathForVersion(runtimeId, runtimeVersion);
        const WorkflowResource runtime = resource(QStringLiteral("alignment-runtime"), WorkflowNodeKind::Runtime,
            QStringLiteral("Alignment runtime"), runtimeId, executable,
            QFileInfo(executable).isFile() ? runtimeId + QStringLiteral(" ") + runtimeVersion
                                           : QStringLiteral("Selected process runtime is not installed."),
            QFileInfo(executable).isFile(), QStringLiteral("RUNTIME_NOT_INSTALLED"));
        const WorkflowResource aligner = resource(QStringLiteral("aligner-model"), WorkflowNodeKind::Model,
            QStringLiteral("Forced aligner model"), modelId, modelDir,
            modelReady ? modelId : QStringLiteral("Selected alignment model is not installed."),
            modelReady, QStringLiteral("MODEL_DIRECTORY_MISSING"));
        plan.resources.append(runtime);
        plan.resources.append(aligner);
        plan.nodes.append(modelStage(QStringLiteral("aligner"), QStringLiteral("Forced aligner"), modelId, modelDir,
            QStringLiteral("Selected alignment model is not installed."), QStringLiteral("MODEL_DIRECTORY_MISSING"), runtime));
        plan.nodes.append(builtInStage(QStringLiteral("output"), WorkflowNodeKind::Output,
            QStringLiteral("Timestamp output"), QStringLiteral("Word and character timestamps")));
        connectLinear(plan);
        return resolution;
    }

    QVariantList crispCandidates;
    if (m_runtimes) {
        for (const QVariant &entry : m_runtimes->allRuntimes()) {
            const QVariantMap runtime = entry.toMap();
            if (runtime.value(QStringLiteral("engineFamily")).toString() == QStringLiteral("crispasr")
                && QFileInfo(runtime.value(QStringLiteral("libraryPath")).toString()).isFile())
                crispCandidates.append(runtime);
        }
    }
    QVariantMap selectedCrisp;
    for (const QVariant &entry : crispCandidates) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == sttRuntimeId
            && (sttRuntimeVersion.isEmpty() || candidate.value(QStringLiteral("version")).toString() == sttRuntimeVersion)) {
            selectedCrisp = candidate;
            break;
        }
    }
    if (selectedCrisp.isEmpty() && crispCandidates.size() == 1) selectedCrisp = crispCandidates.constFirst().toMap();
    const QString crispRuntimePath = selectedCrisp.value(QStringLiteral("libraryPath")).toString();
    const QString runtimeStatus = !crispRuntimePath.isEmpty()
        ? QFileInfo(crispRuntimePath).fileName()
        : (crispCandidates.size() > 1 ? QStringLiteral("Select a compatible CrispASR runtime version.")
                                      : QStringLiteral("CrispASR runtime is not installed."));

    const QVariantMap selectedFiles = request.value(QStringLiteral("selectedFiles")).toMap();
    const QString vadFile = selectedFiles.value(QStringLiteral("vad"), QStringLiteral("ggml-silero-v6.2.0.bin")).toString();
    QString vadModelPath = m_models ? m_models->filePath(modelId, vadFile) : QString();
    if (!QFileInfo(vadModelPath).isFile() && m_models)
        vadModelPath = findFile(m_models->modelsRoot(), {vadFile, QStringLiteral("*silero*.bin"), QStringLiteral("*silero*.gguf")});
    const QVariantMap sttModel = request.value(QStringLiteral("sttModel")).toMap();
    QString sttPath = sttModel.value(QStringLiteral("path")).toString();
    if (QFileInfo(sttPath).isDir()) {
        const QString sttId = sttModel.value(QStringLiteral("id")).toString().toLower();
        QStringList patterns;
        if (sttId.contains(QStringLiteral("nemotron"))) {
            patterns = {QStringLiteral("*q4_k*.gguf"), QStringLiteral("*q8_0*.gguf"),
                        QStringLiteral("*.gguf")};
        } else if (sttId.contains(QStringLiteral("whisper"))) {
            patterns = {QStringLiteral("*.bin"), QStringLiteral("*.gguf")};
        } else {
            patterns = {QStringLiteral("*.gguf"), QStringLiteral("*.bin")};
        }
        sttPath = findFile(sttPath, patterns);
    }
    const QString sttBackend = sttBackendForModel(sttModel);
    const QString sttLanguage = sttLanguageForBackend(
        request.value(QStringLiteral("language"), QStringLiteral("eng")).toString(), sttBackend);
    const QString sttRuntimeIdentity = selectedCrisp.value(QStringLiteral("id")).toString().toLower();
    const bool sttUseGpu = sttRuntimeIdentity.contains(QStringLiteral("cuda"))
        || sttRuntimeIdentity.contains(QStringLiteral("vulkan"))
        || selectedCrisp.value(QStringLiteral("accelerator")).toString().compare(
            QStringLiteral("cpu"), Qt::CaseInsensitive) != 0
            && !selectedCrisp.value(QStringLiteral("accelerator")).toString().isEmpty();

    const WorkflowResource runtime = resource(QStringLiteral("crispasr-runtime"), WorkflowNodeKind::Runtime,
        QStringLiteral("CrispASR"), selectedCrisp.value(QStringLiteral("id")).toString(), crispRuntimePath,
        runtimeStatus, !crispRuntimePath.isEmpty(), QStringLiteral("CRISPASR_RUNTIME_MISSING"));
    const WorkflowResource vadResource = resource(QStringLiteral("vad-model"), WorkflowNodeKind::Model,
        QStringLiteral("Voice activity detection model"), vadFile, vadModelPath,
        QFileInfo(vadModelPath).isFile() ? QFileInfo(vadModelPath).fileName() : QStringLiteral("Silero VAD model is not installed."),
        QFileInfo(vadModelPath).isFile(), QStringLiteral("VAD_MODEL_MISSING"));
    const WorkflowResource sttResource = resource(QStringLiteral("stt-model"), WorkflowNodeKind::Model,
        QStringLiteral("STT anchor model"), sttModel.value(QStringLiteral("id")).toString(), sttPath,
        QFileInfo(sttPath).isFile() ? sttModel.value(QStringLiteral("id")).toString() : QStringLiteral("Select an installed STT anchor model file."),
        QFileInfo(sttPath).isFile(), QStringLiteral("STT_MODEL_FILE_MISSING"));
    const WorkflowResource alignerResource = resource(QStringLiteral("aligner-model"), WorkflowNodeKind::Model,
        QStringLiteral("Forced aligner model"), modelId, modelDir,
        modelReady ? modelId : QStringLiteral("Selected alignment model is not installed."),
        modelReady, QStringLiteral("ALIGNER_MODEL_MISSING"));
    plan.resources = {runtime, vadResource, sttResource, alignerResource};

    plan.nodes.append(modelStage(QStringLiteral("vad"), QStringLiteral("Voice activity detection"), vadFile, vadModelPath,
        QStringLiteral("Silero VAD model is not installed."), QStringLiteral("VAD_MODEL_MISSING"), runtime));
    plan.nodes.append(modelStage(QStringLiteral("stt"), QStringLiteral("STT anchor"), sttModel.value(QStringLiteral("id")).toString(), sttPath,
        QStringLiteral("Select an installed STT anchor model."), QStringLiteral("STT_MODEL_MISSING"), runtime));
    plan.nodes.append(builtInStage(QStringLiteral("matching"), WorkflowNodeKind::Transform,
        QStringLiteral("Transcript matching"), QStringLiteral("Match transcript segments to speech anchors")));
    plan.nodes.append(modelStage(QStringLiteral("aligner"), QStringLiteral("Forced aligner"), modelId, modelDir,
        QStringLiteral("Selected alignment model is not installed."), QStringLiteral("ALIGNER_MODEL_MISSING"), runtime));
    plan.nodes.append(builtInStage(QStringLiteral("output"), WorkflowNodeKind::Output,
        QStringLiteral("Timestamp output"), QStringLiteral("Word and character timestamps")));
    connectLinear(plan);

    payload->executionRequest.insert(QStringLiteral("alignerConfiguration"), QVariantMap{
        {QStringLiteral("modelPath"), modelDir},
        {QStringLiteral("runtimePath"), crispRuntimePath},
        {QStringLiteral("executable"), m_runtimes
            ? m_runtimes->getRuntimeExecutablePathForVersion(runtimeId, runtimeVersion) : QString()}});
    payload->executionRequest.insert(QStringLiteral("sttConfiguration"), QVariantMap{
        {QStringLiteral("modelPath"), sttPath}, {QStringLiteral("backend"), sttBackend},
        {QStringLiteral("language"), sttLanguage}, {QStringLiteral("useGpu"), sttUseGpu}});
    payload->executionRequest.insert(QStringLiteral("vadModelPath"), vadModelPath);
    return resolution;
}

} // namespace LAStudio
