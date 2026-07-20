#include "controllers/dubbing/DubbingTranslationJob.h"

#include "controllers/models/StudioConfigurationResolver.h"
#include "core/ModelManager.h"
#include "core/RuntimeManager.h"
#include "core/Logger.h"
#include "dubbing/DubbingDuration.h"
#include "dubbing/DubbingProject.h"
#include "dubbing/DubbingTimingProfile.h"
#include "dubbing/DubbingTranslationService.h"
#include "translation/TranslationEngine.h"
#include "translation/TranslationService.h"
#include "tts/TtsEngine.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace LAStudio {
namespace {
QString protectedTokens(const QString &text)
{
    QStringList tokens;
    const QRegularExpression re(QStringLiteral("(?:https?://\\S+|\\b\\d[\\d.,/%-]*|\\b[A-Z]{2,}\\b)"));
    auto match = re.globalMatch(text);
    while (match.hasNext()) tokens.append(match.next().captured(0));
    tokens.removeDuplicates();
    return tokens.join(QStringLiteral(", "));
}

QString rewriteStrategy(bool shorten, int candidateIndex)
{
    static const QStringList shorterStrategies = {
        QStringLiteral("Use a concise literal rendering. Remove redundancy and optional discourse words."),
        QStringLiteral("Use a compact, idiomatic paraphrase with shorter synonyms and syntax."),
        QStringLiteral("Compress aggressively while preserving every fact, relation, negation, name, and number.")
    };
    static const QStringList longerStrategies = {
        QStringLiteral("Use a natural literal rendering with explicit grammatical connections."),
        QStringLiteral("Use an idiomatic paraphrase with helpful modifiers that do not add new facts."),
        QStringLiteral("Expand implicit relations and context naturally without repetition or invented detail.")
    };
    const QStringList &strategies = shorten ? shorterStrategies : longerStrategies;
    return strategies.at(qBound(0, candidateIndex, strategies.size() - 1));
}
}

DubbingTranslationJob::DubbingTranslationJob(TranslationEngine *translation,
                                               ModelManager *models,
                                               RuntimeManager *runtimes,
                                               TtsEngine *tts, QObject *parent)
    : QObject(parent), m_translation(translation), m_models(models),
      m_runtimes(runtimes), m_tts(tts)
{
}

bool DubbingTranslationJob::prepareRequest(const QString &sourceLanguage,
                                           const QString &targetLanguage,
                                           const QVariantMap &configuration,
                                           TranslationRequest &request,
                                           QString *error) const
{
    Logger::info(QStringLiteral("DubbingTranslation"),
                 QStringLiteral("Preparing request source=%1 target=%2 configured=%3 family=%4 runtime=%5 version=%6")
                     .arg(sourceLanguage, targetLanguage)
                     .arg(configuration.isEmpty() ? QStringLiteral("false") : QStringLiteral("true"))
                     .arg(configuration.value(QStringLiteral("familyId")).toString(),
                          configuration.value(QStringLiteral("runtimeId")).toString(),
                          configuration.value(QStringLiteral("runtimeVersion")).toString()));
    DubbingTranslationService service(m_models, m_runtimes);
    bool prepared = false;
    if (!configuration.isEmpty()) {
        StudioConfiguration selected;
        selected.capabilityId = QStringLiteral("translation");
        selected.familyId = configuration.value(QStringLiteral("familyId")).toString();
        selected.runtimeId = configuration.value(QStringLiteral("runtimeId")).toString();
        selected.runtimeVersion = configuration.value(QStringLiteral("runtimeVersion")).toString();
        selected.selectedFiles = configuration.value(QStringLiteral("selectedFiles")).toMap();
        const ResolvedConfiguration resolved = StudioConfigurationResolver::resolve(selected);
        Logger::info(QStringLiteral("DubbingTranslation"),
                     QStringLiteral("Configuration resolution valid=%1 signature=%2 runtime=%3 roles=%4")
                         .arg(resolved.isValid ? QStringLiteral("true") : QStringLiteral("false"),
                              resolved.signature, resolved.runtimePath,
                              resolved.resolvedPaths.keys().join(QLatin1Char(','))));
        if (resolved.isValid) {
            SessionConfiguration session;
            session.capabilityId = selected.capabilityId;
            session.selection.capabilityId = selected.capabilityId;
            session.selection.familyId = selected.familyId;
            session.selection.runtimeId = selected.runtimeId;
            session.selection.runtimeVersion = selected.runtimeVersion;
            session.familyConfig = resolved.family;
            session.runtimePath = resolved.runtimePath;
            for (auto it = resolved.resolvedPaths.cbegin(); it != resolved.resolvedPaths.cend(); ++it) {
                if (it.key() == QStringLiteral("familyId") || it.key() == QStringLiteral("backend")
                    || it.key() == QStringLiteral("pipelineProfile")) continue;
                session.resolvedPathsByRole.insert(it.key(), it.value().toString());
                if (it.value().toString().endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive))
                    session.resolvedModelPaths.append(it.value().toString());
            }
            TranslationService::prepareConfiguration(session, sourceLanguage, targetLanguage,
                                                     request, error);
            prepared = !request.modelPath.isEmpty();
            Logger::info(QStringLiteral("DubbingTranslation"),
                         QStringLiteral("Configured request prepared=%1 backend=%2 model=%3 modelExists=%4 runtime=%5 runtimeExists=%6 gpu=%7 error=%8")
                             .arg(prepared ? QStringLiteral("true") : QStringLiteral("false"),
                                  request.backend, request.modelPath)
                             .arg(QFileInfo::exists(request.modelPath) ? QStringLiteral("true") : QStringLiteral("false"))
                             .arg(request.runtimePath)
                             .arg(QFileInfo::exists(request.runtimePath) ? QStringLiteral("true") : QStringLiteral("false"))
                             .arg(request.useGpu ? QStringLiteral("true") : QStringLiteral("false"))
                             .arg(error ? *error : QString()));
        }
    }
    if (!prepared) {
        Logger::warning(QStringLiteral("DubbingTranslation"),
                        QStringLiteral("Configured request unavailable; trying installed translation fallback."));
        prepared = service.prepare(sourceLanguage, targetLanguage, request, error);
    }
    Logger::info(QStringLiteral("DubbingTranslation"),
                 QStringLiteral("Request preparation finished prepared=%1 backend=%2 model=%3 runtime=%4 error=%5")
                     .arg(prepared ? QStringLiteral("true") : QStringLiteral("false"),
                          request.backend, request.modelPath, request.runtimePath,
                          error ? *error : QString()));
    return prepared;
}

bool DubbingTranslationJob::start(const QString &sourceLanguage, const QString &targetLanguage,
                                  const QVariantList &segments, const QVariantMap &configuration,
                                  const QString &runId)
{
    Logger::info(QStringLiteral("DubbingTranslation"),
                 QStringLiteral("Start requested run=%1 generation=%2 source=%3 target=%4 segments=%5 running=%6 instanceProcessing=%7")
                     .arg(runId).arg(m_generation + 1).arg(sourceLanguage, targetLanguage)
                     .arg(segments.size())
                     .arg(m_running ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(m_instance && m_instance->isProcessing()
                              ? QStringLiteral("true") : QStringLiteral("false")));
    if (m_running || (m_instance && m_instance->isProcessing())) {
        fail(QStringLiteral("A translation request is already running."));
        return false;
    }
    if (!m_translation) { fail(QStringLiteral("Translation engine is unavailable.")); return false; }
    TranslationRequest request;
    QString error;
    if (!prepareRequest(sourceLanguage, targetLanguage, configuration, request, &error)) {
        fail(error.isEmpty() ? QStringLiteral("Could not resolve a translation model and runtime.") : error);
        return false;
    }
    m_generation++;
    m_running = true;
    m_runId = runId;
    m_sourceLanguage = sourceLanguage;
    m_targetLanguage = targetLanguage;
    m_durationSettings = DubbingDurationSettings();
    QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
    if (parameters.isEmpty()) parameters = configuration;
    if (parameters.contains(QStringLiteral("durationControl")))
        m_durationSettings = DubbingDurationSettings::fromVariantMap(
            parameters.value(QStringLiteral("durationControl")).toMap());
    if (parameters.contains(QStringLiteral("durationAware")))
        m_durationSettings.enabled = parameters.value(QStringLiteral("durationAware")).toBool();
    m_durationAware = m_durationSettings.enabled
        && targetLanguage.compare(QStringLiteral("vi"), Qt::CaseInsensitive) == 0
        && request.backend.contains(QStringLiteral("llama"), Qt::CaseInsensitive);
    Logger::info(QStringLiteral("DubbingTranslation"),
                 QStringLiteral("Request ready run=%1 backend=%2 gpu=%3 maxTokens=%4 durationEnabled=%5 durationAware=%6 modelSize=%7 runtimeSize=%8")
                     .arg(m_runId, request.backend)
                     .arg(request.useGpu ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(request.maxTokens)
                     .arg(m_durationSettings.enabled ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(m_durationAware ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(QFileInfo(request.modelPath).size())
                     .arg(QFileInfo(request.runtimePath).size()));
    m_durationIteration = 0;
    m_durationRate = 10.0;
    if (m_durationAware && m_tts && !m_tts->activeSignature().isEmpty()) {
        DubbingTimingProfile profile;
        const QString id = DubbingTimingProfileStore::profileId(m_tts->activeSignature(), targetLanguage);
        if (DubbingTimingProfileStore::load(id, profile)) m_durationRate = profile.phonemesPerSecond;
    }

    m_inputSegments.clear();
    for (const QVariant &value : segments) {
        QVariantMap segment = value.toMap();
        if (m_durationAware) {
            const DubbingSpeechBudget budget = DubbingDurationPlanner::plan(
                segment, m_durationRate, m_durationSettings);
            segment.insert(QStringLiteral("durationBudget"), budget.toVariantMap());
            segment.insert(QStringLiteral("protectedTokens"),
                           protectedTokens(segment.value(QStringLiteral("sourceText")).toString()));
            segment.insert(QStringLiteral("durationPrompt"),
                           QStringLiteral("Use between %1 and %2 target-language phonemes (predicted %3).")
                               .arg(budget.minUnits).arg(budget.maxUnits).arg(budget.targetUnits));
        }
        m_inputSegments.append(segment);
    }

    SessionConfiguration session;
    session.capabilityId = QStringLiteral("translation");
    session.selection.capabilityId = session.capabilityId;
    session.selection.familyId = request.backend;
    session.selection.runtimeId = request.useGpu ? QStringLiteral("translation-cuda") : QStringLiteral("translation-cpu");
    session.familyConfig = QVariantMap{{QStringLiteral("id"), request.backend},
                                       {QStringLiteral("backend"), request.backend}};
    session.runtimePath = request.runtimePath;
    session.resolvedModelPaths = {request.modelPath};
    session.resolvedPathsByRole.insert(QStringLiteral("model"), request.modelPath);
    session.signature = QStringLiteral("dubbing|%1|%2|%3").arg(request.backend, request.modelPath, request.runtimePath);

    m_instance = m_translation->instance(session.signature);
    if (!m_instance) {
        const QString requestedModel = QFileInfo(request.modelPath).absoluteFilePath();
        for (TranslationEngineInstance *candidate : m_translation->loadedInstances()) {
            if (!candidate || !candidate->isModelLoaded()) continue;
            const SessionConfiguration existing = candidate->configuration();
            if (existing.runtimePath != request.runtimePath) continue;
            for (const QString &path : existing.resolvedModelPaths) {
                if (QFileInfo(path).absoluteFilePath().compare(requestedModel, Qt::CaseInsensitive) == 0) {
                    m_instance = candidate;
                    break;
                }
            }
            if (m_instance) break;
        }
    }
    if (!m_instance) m_instance = m_translation->loadInstance(session, false);
    if (!m_instance) { fail(QStringLiteral("Failed to create Translation instance.")); return false; }
    Logger::info(QStringLiteral("DubbingTranslation"),
                 QStringLiteral("Translation instance selected signature=%1 state=%2 loaded=%3 reused=%4")
                     .arg(m_instance->signature()).arg(m_instance->state())
                     .arg(m_instance->isModelLoaded() ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(m_translation->instance(session.signature) == m_instance
                              ? QStringLiteral("true") : QStringLiteral("false")));

    TranslationInferenceRequest inference;
    inference.segments = m_inputSegments;
    inference.sourceLanguage = sourceLanguage;
    inference.targetLanguage = targetLanguage;
    inference.task = m_durationAware ? QStringLiteral("duration-translate")
                                     : QStringLiteral("translate");
    inference.maxTokens = request.maxTokens;
    m_pendingRequest = inference;
    m_phase = QStringLiteral("reference");

    QObject::disconnect(m_finishedConnection);
    QObject::disconnect(m_errorConnection);
    QObject::disconnect(m_loadedConnection);
    QObject::disconnect(m_progressConnection);
    const quint64 generation = m_generation;
    m_finishedConnection = connect(m_instance, &TranslationEngineInstance::translationFinished,
                                   this, [this, generation](const QVariantList &patches) {
        if (!m_running || generation != m_generation || m_phase.isEmpty()) return;
        onTranslationFinished(patches, generation);
    });
    m_errorConnection = connect(m_instance, &TranslationEngineInstance::errorOccurred,
                                this, [this, generation](const QString &message) {
        if (m_running && generation == m_generation && !m_phase.isEmpty()) fail(message);
    });
    m_progressConnection = connect(m_instance, &TranslationEngineInstance::progressChanged,
                                   this, &DubbingTranslationJob::setProgressForWorker);
    auto startWhenReady = [this, generation]() {
        if (!m_instance || !m_running || generation != m_generation) return;
        Logger::info(QStringLiteral("DubbingTranslation"),
                     QStringLiteral("Instance readiness changed run=%1 generation=%2 state=%3 loaded=%4 phase=%5")
                         .arg(m_runId).arg(generation).arg(m_instance->state())
                         .arg(m_instance->isModelLoaded() ? QStringLiteral("true") : QStringLiteral("false"))
                         .arg(m_phase));
        if (m_instance->state() == TranslationEngineInstance::Ready) {
            Logger::info(QStringLiteral("DubbingTranslation"),
                         QStringLiteral("Dispatching inference run=%1 task=%2 segments=%3 maxTokens=%4")
                             .arg(m_runId, m_pendingRequest.task)
                             .arg(m_pendingRequest.segments.size())
                             .arg(m_pendingRequest.maxTokens));
            m_instance->translate(m_pendingRequest);
        } else if (m_instance->state() == TranslationEngineInstance::Error) {
            fail(QStringLiteral("Translation model failed to load."));
        }
    };
    if (m_instance->isModelLoaded()) startWhenReady();
    else m_loadedConnection = connect(m_instance, &TranslationEngineInstance::modelLoadedChanged, this, startWhenReady);
    return true;
}

void DubbingTranslationJob::cancel()
{
    if (!m_running) return;
    ++m_generation;
    if (m_instance && m_instance->isProcessing()) m_instance->cancelProcessing();
    m_running = false;
    m_phase.clear();
}

void DubbingTranslationJob::setProgressForWorker()
{
    if (!m_running || !m_instance) return;
    const int worker = m_instance->progress();
    if (m_phase == QStringLiteral("reference")) emit progressChanged(qBound(0, worker * 30 / 100, 30));
    else if (m_phase == QStringLiteral("rewrite")) {
        const int base = 30 + m_durationIteration * 15;
        emit progressChanged(qBound(base, base + worker * 15 / 100, 85));
    } else if (m_phase == QStringLiteral("pause-align")) emit progressChanged(qBound(85, 85 + worker * 10 / 100, 95));
}

void DubbingTranslationJob::onTranslationFinished(const QVariantList &patches, quint64 generation)
{
    if (!m_running || generation != m_generation) return;
    Logger::info(QStringLiteral("DubbingTranslation"),
                 QStringLiteral("Inference finished run=%1 generation=%2 phase=%3 patches=%4")
                     .arg(m_runId).arg(generation).arg(m_phase).arg(patches.size()));
    if (patches.isEmpty()) { fail(QStringLiteral("Translation returned no result.")); return; }
    QVariantList result = patches;
    if (!result.isEmpty() && result.constFirst().toMap().contains(QStringLiteral("error"))) {
        fail(result.constFirst().toMap().value(QStringLiteral("error")).toString());
        return;
    }
    if (m_phase == QStringLiteral("reference")) {
        QVariantList merged; QString error;
        if (!DubbingProject::mergeSegmentPatches(m_inputSegments, result, merged, &error)) { fail(error); return; }
        for (QVariant &value : merged) {
            QVariantMap segment = value.toMap();
            const QString reference = segment.value(QStringLiteral("targetText")).toString().trimmed();
            segment.insert(QStringLiteral("referenceTranslation"), reference);
            segment.insert(QStringLiteral("durationUnits"), DubbingDurationPlanner::countPhonemes(reference, m_targetLanguage));
            segment.insert(QStringLiteral("durationIteration"), 0);
            value = segment;
        }
        int nonEmpty = 0;
        for (const QVariant &value : std::as_const(merged)) {
            if (!value.toMap().value(QStringLiteral("targetText")).toString().trimmed().isEmpty())
                ++nonEmpty;
        }
        Logger::info(QStringLiteral("DubbingTranslation"),
                     QStringLiteral("Reference translations merged run=%1 total=%2 nonEmpty=%3 durationAware=%4")
                         .arg(m_runId).arg(merged.size()).arg(nonEmpty)
                         .arg(m_durationAware ? QStringLiteral("true") : QStringLiteral("false")));
        m_inputSegments = merged;
        if (!m_durationAware) { finishDurationTranslation(); return; }
        requestDurationCandidates();
        return;
    }
    if (m_phase == QStringLiteral("rewrite")) {
        QHash<QString, QStringList> candidatesBySegment;
        QHash<QString, QVariantMap> requestById;
        for (const QVariant &value : m_pendingRequest.segments) {
            const QVariantMap request = value.toMap();
            requestById.insert(request.value(QStringLiteral("id")).toString(), request);
        }
        for (const QVariant &value : result) {
            const QVariantMap patch = value.toMap();
            const QVariantMap request = requestById.value(patch.value(QStringLiteral("id")).toString());
            const QString baseId = request.value(QStringLiteral("baseSegmentId")).toString();
            if (!baseId.isEmpty()) candidatesBySegment[baseId].append(patch.value(QStringLiteral("targetText")).toString());
        }
        for (QVariant &value : m_inputSegments) {
            QVariantMap segment = value.toMap();
            const QVariantMap budget = segment.value(QStringLiteral("durationBudget")).toMap();
            const QString selected = DubbingDurationPlanner::selectBestCandidate(
                segment.value(QStringLiteral("sourceText")).toString(), segment.value(QStringLiteral("referenceTranslation")).toString(),
                segment.value(QStringLiteral("targetText")).toString(), candidatesBySegment.value(segment.value(QStringLiteral("id")).toString()),
                budget.value(QStringLiteral("targetUnits")).toInt(),
                budget.value(QStringLiteral("minUnits")).toInt(),
                budget.value(QStringLiteral("maxUnits")).toInt(),
                segment.value(QStringLiteral("protectedTokens")).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts), m_targetLanguage);
            if (!selected.isEmpty()) segment.insert(QStringLiteral("targetText"), selected);
            segment.insert(QStringLiteral("durationIteration"), m_durationIteration);
            value = segment;
        }
        // Keep trying with a new seed and rewrite strategy even when the previous
        // batch made no progress. maxPreTtsIterations is a strict attempt limit,
        // not a requirement that every preceding batch improve the translation.
        requestDurationCandidates();
        return;
    }
    if (m_phase == QStringLiteral("pause-align")) {
        QHash<QString, QString> aligned;
        for (const QVariant &value : result) { const QVariantMap patch = value.toMap(); aligned.insert(patch.value(QStringLiteral("id")).toString(), patch.value(QStringLiteral("targetText")).toString()); }
        for (QVariant &value : m_inputSegments) {
            QVariantMap segment = value.toMap();
            const QString target = segment.value(QStringLiteral("targetText")).toString();
            QString marked = aligned.value(segment.value(QStringLiteral("id")).toString(), target);
            const QString stripped = DubbingDurationPlanner::textWithoutPauseMarkers(marked);
            if (DubbingDurationPlanner::semanticFidelityScore(target, stripped) < 0.98) marked = target;
            const QVariantList pauses = segment.value(QStringLiteral("durationBudget")).toMap().value(QStringLiteral("pauses")).toList();
            segment.insert(QStringLiteral("targetChunks"), DubbingDurationPlanner::pauseChunks(marked, pauses));
            segment.insert(QStringLiteral("pauseAligned"), true);
            value = segment;
        }
        finishDurationTranslation();
        return;
    }
    fail(QStringLiteral("Unknown duration-translation phase: %1").arg(m_phase));
}

void DubbingTranslationJob::requestDurationCandidates()
{
    QVariantList requests;
    if (m_durationIteration < m_durationSettings.maxPreTtsIterations) {
        for (const QVariant &value : m_inputSegments) {
            const QVariantMap segment = value.toMap();
            const QVariantMap budget = segment.value(QStringLiteral("durationBudget")).toMap();
            const int predicted = budget.value(QStringLiteral("targetUnits")).toInt();
            const int minimum = budget.value(QStringLiteral("minUnits")).toInt();
            const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
            const int current = DubbingDurationPlanner::countPhonemes(segment.value(QStringLiteral("targetText")).toString(), m_targetLanguage);
            const int tolerance = qMax(1, qRound(predicted * m_durationSettings.toleranceRatio));
            if (qAbs(current - predicted) <= tolerance) continue;
            for (int candidate = 0; candidate < m_durationSettings.candidatesPerIteration; ++candidate) {
                const bool shorten = current > predicted;
                const int currentWords = qMax(
                    1, DubbingDurationPlanner::countVietnameseSyllables(
                           segment.value(QStringLiteral("targetText")).toString()));
                const int targetWords = qMax(1, qRound(
                    currentWords * predicted / static_cast<double>(qMax(1, current))));
                const int minWords = qMax(1, qRound(
                    currentWords * minimum / static_cast<double>(qMax(1, current))));
                const int maxWords = qMax(minWords, qRound(
                    currentWords * maximum / static_cast<double>(qMax(1, current))));
                QVariantMap request = segment;
                request.insert(QStringLiteral("id"), QStringLiteral("%1::dt::%2::%3").arg(segment.value(QStringLiteral("id")).toString()).arg(m_durationIteration + 1).arg(candidate));
                request.insert(QStringLiteral("baseSegmentId"), segment.value(QStringLiteral("id")));
                request.insert(QStringLiteral("candidateIndex"), candidate);
                request.insert(QStringLiteral("durationIteration"), m_durationIteration + 1);
                request.insert(QStringLiteral("durationPrompt"),
                               QStringLiteral(
                                   "%1 the translation. An external counter reports %2 phonemes; "
                                   "target %3, acceptable range %4-%5. Aim for about %6 "
                                   "space-separated Vietnamese words (roughly %7-%8). %9")
                                   .arg(shorten ? QStringLiteral("Shorten") : QStringLiteral("Lengthen"))
                                   .arg(current).arg(predicted).arg(minimum).arg(maximum)
                                   .arg(targetWords).arg(minWords).arg(maxWords)
                                   .arg(rewriteStrategy(shorten, candidate)));
                requests.append(request);
            }
        }
    }
    if (requests.isEmpty()) { requestPauseAlignment(); return; }
    ++m_durationIteration;
    m_pendingRequest = TranslationInferenceRequest();
    m_pendingRequest.segments = requests;
    m_pendingRequest.sourceLanguage = m_sourceLanguage;
    m_pendingRequest.targetLanguage = m_targetLanguage;
    m_pendingRequest.task = QStringLiteral("duration-rewrite");
    m_pendingRequest.maxTokens = 512;
    m_phase = QStringLiteral("rewrite");
    m_instance->translate(m_pendingRequest);
}

void DubbingTranslationJob::requestPauseAlignment()
{
    QVariantList requests;
    for (QVariant &value : m_inputSegments) {
        QVariantMap segment = value.toMap();
        const QVariantList pauses = segment.value(QStringLiteral("durationBudget")).toMap().value(QStringLiteral("pauses")).toList();
        int internal = 0;
        for (const QVariant &pause : pauses) if (pause.toMap().value(QStringLiteral("kind")).toString() == QStringLiteral("internal")) ++internal;
        if (internal <= 0) {
            segment.insert(QStringLiteral("targetChunks"), DubbingDurationPlanner::pauseChunks(segment.value(QStringLiteral("targetText")).toString(), pauses));
            segment.insert(QStringLiteral("pauseAligned"), true);
            value = segment;
            continue;
        }
        segment.insert(QStringLiteral("internalPauseCount"), internal);
        requests.append(segment);
    }
    if (requests.isEmpty()) { finishDurationTranslation(); return; }
    m_pendingRequest = TranslationInferenceRequest();
    m_pendingRequest.segments = requests;
    m_pendingRequest.sourceLanguage = m_sourceLanguage;
    m_pendingRequest.targetLanguage = m_targetLanguage;
    m_pendingRequest.task = QStringLiteral("pause-align");
    m_pendingRequest.maxTokens = 512;
    m_phase = QStringLiteral("pause-align");
    m_instance->translate(m_pendingRequest);
}

void DubbingTranslationJob::finishDurationTranslation()
{
    QStringList violations;
    for (QVariant &value : m_inputSegments) {
        QVariantMap segment = value.toMap();
        const QVariantMap budget = segment.value(QStringLiteral("durationBudget")).toMap();
        if (!budget.isEmpty()) {
            const int units = DubbingDurationPlanner::countPhonemes(segment.value(QStringLiteral("targetText")).toString(), m_targetLanguage);
            const int predicted = budget.value(QStringLiteral("targetUnits")).toInt();
            const int minimum = budget.value(QStringLiteral("minUnits")).toInt();
            const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
            const bool withinBudget = units >= minimum && units <= maximum;
            segment.insert(QStringLiteral("durationUnits"), units);
            segment.insert(QStringLiteral("phonemeDistance"), qAbs(units - predicted));
            segment.insert(QStringLiteral("durationStatus"), withinBudget
                               ? QStringLiteral("within-budget")
                               : QStringLiteral("needs-review"));
            segment.insert(QStringLiteral("durationMetric"), QStringLiteral("phoneme-distance"));
            segment.insert(QStringLiteral("candidateSelectionMetric"),
                           QStringLiteral("budget-first-semantic-proxy-v3"));
            if (!withinBudget) {
                violations.append(
                    QStringLiteral("%1 (%2 phonemes; required %3-%4)")
                        .arg(segment.value(QStringLiteral("id")).toString())
                        .arg(units).arg(minimum).arg(maximum));
            }
        }
        value = segment;
    }
    if (!violations.isEmpty()) {
        fail(QStringLiteral(
                 "Strict duration translation could not satisfy the phoneme budget after %1 "
                 "rewrite iterations. No out-of-budget translation was accepted. Segments: %2")
                 .arg(m_durationIteration)
                 .arg(violations.join(QStringLiteral(", "))));
        return;
    }
    m_phase.clear();
    m_running = false;
    Logger::info(QStringLiteral("DubbingTranslation"),
                 QStringLiteral("Job completed run=%1 segments=%2 durationIterations=%3")
                     .arg(m_runId).arg(m_inputSegments.size()).arg(m_durationIteration));
    emit progressChanged(100);
    emit completed(m_inputSegments);
}

void DubbingTranslationJob::fail(const QString &message)
{
    Logger::error(QStringLiteral("DubbingTranslation"),
                  QStringLiteral("Job failed run=%1 generation=%2 phase=%3 message=%4")
                      .arg(m_runId).arg(m_generation).arg(m_phase, message));
    m_running = false;
    m_phase.clear();
    emit failed(message);
}

} // namespace LAStudio
