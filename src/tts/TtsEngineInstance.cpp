#include "TtsEngineInstance.h"
#include "TtsTextPreprocessor.h"
#include "TtsWorker.h"
#include "TtsRequestValidator.h"
#include "core/Logger.h"
#include "core/HardwareManager.h"
#include "controllers/models/CapabilitySettingsSchema.h"
#include <runtimes/OmnivoiceInterface.h>

#include <QThread>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStringList>
#include <cstdint>
#include <algorithm>
#include <variant>
#include <cmath>

namespace LAStudio {

// Helper for std::visit
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace {

QString normalizedConfigValue(const QVariant &value)
{
    const QString text = value.toString();
    if (text.isEmpty())
        return text;

    if (text.contains(QLatin1Char('/')) || text.contains(QLatin1Char('\\')) ||
        text.contains(QLatin1Char(':'))) {
        return QDir::cleanPath(QDir::fromNativeSeparators(text));
    }

    return text;
}

bool isVoxCpm2FullPrecisionModel(const QString &modelPath, qint64 modelBytes)
{
    const QString filename = QFileInfo(modelPath).fileName().toLower();
    if (!filename.contains(QStringLiteral("voxcpm2"))) {
        return false;
    }
    if (filename.contains(QStringLiteral("f16")) ||
        filename.contains(QStringLiteral("fp16"))) {
        return true;
    }

    constexpr qint64 fullPrecisionBytes = 3LL * 1024LL * 1024LL * 1024LL;
    return modelBytes >= fullPrecisionBytes;
}

qint64 existingFileSize(const QString &path)
{
    if (path.isEmpty()) {
        return 0;
    }
    const QFileInfo info(QDir::fromNativeSeparators(path));
    if (!info.exists() || !info.isFile()) {
        return 0;
    }
    return info.size();
}

qint64 uniqueConfigAssetBytes(const QVariantMap &config)
{
    qint64 total = 0;
    QStringList seen;
    for (auto it = config.cbegin(); it != config.cend(); ++it) {
        if (it.key() == QStringLiteral("runtimePath")) {
            continue;
        }
        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(it.value().toString()));
        if (path.isEmpty() || seen.contains(path, Qt::CaseInsensitive)) {
            continue;
        }
        const qint64 size = existingFileSize(path);
        if (size <= 0) {
            continue;
        }
        seen.append(path);
        total += size;
    }
    return total;
}

QString progressLabelForStage(const QString &stage)
{
    if (stage.isEmpty()) {
        return QStringLiteral("Generating audio");
    }
    if (stage == QStringLiteral("tts_begin")) {
        return QStringLiteral("Preparing synthesis");
    }
    if (stage == QStringLiteral("prepare")) {
        return QStringLiteral("Preparing synthesis");
    }
    if (stage == QStringLiteral("prefill")) {
        return QStringLiteral("Running prompt prefill");
    }
    if (stage == QStringLiteral("generate_frame") || stage == QStringLiteral("generate_frames")) {
        return QStringLiteral("Generating speech frames");
    }
    if (stage == QStringLiteral("chunk")) {
        return QStringLiteral("Generating text chunk");
    }
    if (stage == QStringLiteral("decode_audio")) {
        return QStringLiteral("Decoding audio");
    }
    if (stage == QStringLiteral("complete")) {
        return QStringLiteral("Synthesis complete");
    }
    if (stage == QStringLiteral("maskgit")) {
        return QStringLiteral("Decoding semantic tokens");
    }
    if (stage == QStringLiteral("codec_decode")) {
        return QStringLiteral("Decoding audio");
    }
    if (stage == QStringLiteral("postprocess")) {
        return QStringLiteral("Finalizing audio");
    }

    QString label = stage;
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!label.isEmpty()) {
        label[0] = label[0].toUpper();
    }
    return label;
}

} // namespace

TtsEngineInstance::TtsEngineInstance(QObject *parent)
    : QObject(parent)
{
    m_worker = new TtsWorker;
    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &TtsWorker::modelLoaded, this, &TtsEngineInstance::onWorkerModelLoaded);
    connect(m_worker, &TtsWorker::finished, this, &TtsEngineInstance::onWorkerFinished);
    connect(m_worker, &TtsWorker::errorOccurred, this, &TtsEngineInstance::onWorkerError);
    connect(m_worker, &TtsWorker::progress, this, &TtsEngineInstance::onWorkerProgress);

    m_cpuTimer = new QTimer(this);
    m_cpuTimer->setInterval(1000);
    connect(m_cpuTimer, &QTimer::timeout, this, &TtsEngineInstance::updateCpuUsage);

    m_thread->start();
}

TtsEngineInstance::~TtsEngineInstance()
{
    m_thread->quit();
    if (!m_thread->wait(1000)) {
        Logger::warning("TtsEngineInstance", "TTS thread did not stop within timeout. Detaching thread to prevent shutdown hang/crash.");
        m_thread->setParent(nullptr);
        connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    }
}

TtsEngineInstance::State TtsEngineInstance::state() const
{
    return std::visit(overloaded{
        [](StateUnloaded) { return Unloaded; },
        [](StateLoading)  { return Loading; },
        [](StateReady)    { return Ready; },
        [](StateProcessing) { return Processing; },
        [](StateError)    { return Error; }
    }, m_state);
}

void TtsEngineInstance::loadVoice(const QVariantMap &config)
{
    dispatch(EventLoadVoice{config});
}

void TtsEngineInstance::setFamilyConfig(const QVariantMap &config)
{
    if (m_familyConfig == config)
        return;
    m_familyConfig = config;
    emit familyConfigChanged();
    emit schemaChanged(); // Force UI to refresh capability-based schemas
}

QVariantList TtsEngineInstance::schemaForCapability(const QString &capability) const
{
    return CapabilitySettingsSchema::merge(m_familyConfig, capability, m_currentSchema);
}

QVariantMap TtsEngineInstance::studioConfigForCapability(const QString &capability) const
{
    if (m_familyConfig.isEmpty())
        return QVariantMap();

    QVariantMap studio = m_familyConfig.value("studio").toMap();
    return studio.value(capability).toMap();
}

void TtsEngineInstance::loadModel(const QString &modelPath,
                          const QString &codecPath,
                          const QString &runtimePath)
{
    QVariantMap config;
    config.insert(QStringLiteral("model"), modelPath);
    if (!codecPath.isEmpty())
        config.insert(QStringLiteral("codec"), codecPath);
    if (!runtimePath.isEmpty())
        config.insert(QStringLiteral("runtimePath"), runtimePath);

    loadVoice(config);
}

void TtsEngineInstance::loadModel(const QVariantMap &filesByRole,
                          const QString &runtimePath)
{
    QVariantMap config = filesByRole;
    if (!config.contains(QStringLiteral("codec")) && config.contains(QStringLiteral("tokenizer"))) {
        config.insert(QStringLiteral("codec"), config.value(QStringLiteral("tokenizer")));
    }
    if (!runtimePath.isEmpty())
        config.insert(QStringLiteral("runtimePath"), runtimePath);

    loadVoice(config);
}

void TtsEngineInstance::setRuntimePath(const QString &runtimePath)
{
    if (runtimePath == m_lastRuntimePath)
        return;

    m_lastRuntimePath = runtimePath;

    if (runtimePath.isEmpty()) {
        if (!m_currentSchema.isEmpty()) {
            m_currentSchema.clear();
            emit schemaChanged();
        }
        return;
    }

    QVariantList schema;
    if (m_schemaCache.contains(runtimePath)) {
        schema = m_schemaCache.value(runtimePath);
    } else {
        schema = buildSchemaForRuntime(runtimePath);
        m_schemaCache.insert(runtimePath, schema);
    }

    if (!isModelLoaded() || m_currentSchema.isEmpty()) {
        if (m_currentSchema != schema) {
            m_currentSchema = schema;
            emit schemaChanged();
        }
    }
}

QVariantList TtsEngineInstance::buildSchemaForRuntime(const QString &runtimePath) const
{
    bool isVibe = runtimePath.contains("vibevoice", Qt::CaseInsensitive) || runtimePath.contains("vibe", Qt::CaseInsensitive);
    bool isOmni = runtimePath.contains("omnivoice", Qt::CaseInsensitive) || runtimePath.contains("omni", Qt::CaseInsensitive);
    bool isKokoroVietnamese = runtimePath.contains("kokoro-vietnamese", Qt::CaseInsensitive) ||
                              runtimePath.contains("kokoro_vi", Qt::CaseInsensitive);
    bool isKokoro = !isKokoroVietnamese &&
                    (runtimePath.contains("crispasr", Qt::CaseInsensitive) ||
                     runtimePath.contains("kokoro", Qt::CaseInsensitive));
    bool isSpeechLmTts = runtimePath.contains("speech-lm", Qt::CaseInsensitive) ||
                         runtimePath.contains("speechlm", Qt::CaseInsensitive) ||
                         runtimePath.contains("vieneu", Qt::CaseInsensitive) ||
                         runtimePath.contains("slm", Qt::CaseInsensitive);

    QVariantList schema;

    if (isKokoroVietnamese) {
        QVariantMap voice;
        voice["id"] = "voice";
        voice["name"] = "Voice";
        voice["type"] = "choice";
        QVariantList choices;
        QVariantMap diem;
        diem["text"] = "Diem Trinh";
        diem["value"] = "diem_trinh";
        choices.append(diem);
        QVariantMap hung;
        hung["text"] = "Hung Thinh";
        hung["value"] = "hung_thinh";
        choices.append(hung);
        voice["choices"] = choices;
        voice["default"] = "diem_trinh";
        voice["description"] = "Selects the Kokoro-Vietnamese preset voice.";
        schema.append(voice);
    } else if (isKokoro) {
        QVariantMap lengthScale;
        lengthScale["id"] = "length_scale";
        lengthScale["name"] = "Length Scale";
        lengthScale["type"] = "float";
        lengthScale["min"] = 0.25;
        lengthScale["max"] = 4.0;
        lengthScale["default"] = 1.0;
        lengthScale["description"] = "Duration multiplier. Values above 1.0 speak slower; below 1.0 speak faster.";
        schema.append(lengthScale);
    } else if (isVibe) {
        QVariantMap steps;
        steps["id"] = "n_diffusion_steps";
        steps["name"] = "Diffusion Steps";
        steps["type"] = "int";
        steps["min"] = 10;
        steps["max"] = 100;
        steps["default"] = 20;
        steps["description"] = "Number of diffusion steps. Higher = better quality but slower.";
        schema.append(steps);

        QVariantMap cfg;
        cfg["id"] = "cfg_scale";
        cfg["name"] = "CFG Scale";
        cfg["type"] = "float";
        cfg["min"] = 1.0;
        cfg["max"] = 10.0;
        cfg["default"] = 4.0;
        cfg["description"] = "Classifier-free guidance scale. Controls adherence to reference.";
        schema.append(cfg);
    } else if (isOmni) {
        QVariantMap steps;
        steps["id"] = "mg_num_step";
        steps["name"] = "Inference Steps";
        steps["type"] = "int";
        steps["min"] = 10;
        steps["max"] = 100;
        steps["default"] = 32;
        steps["description"] = "Number of iterative decoding steps.";
        schema.append(steps);

        QVariantMap guidance;
        guidance["id"] = "mg_guidance_scale";
        guidance["name"] = "CFG Guidance";
        guidance["type"] = "float";
        guidance["min"] = 1.0;
        guidance["max"] = 5.0;
        guidance["default"] = 2.0;
        guidance["description"] = "Controls style adherence.";
        schema.append(guidance);
    } else if (isSpeechLmTts) {
        QVariantMap temp;
        temp["id"] = "temperature";
        temp["name"] = "Temperature";
        temp["type"] = "float";
        temp["min"] = 0.1;
        temp["max"] = 2.0;
        temp["default"] = 0.4;
        temp["description"] = "Randomness of generation. Recommended: 0.3 - 0.5.";
        schema.append(temp);

        QVariantMap topk;
        topk["id"] = "top_k";
        topk["name"] = "Top K";
        topk["type"] = "int";
        topk["min"] = 1;
        topk["max"] = 100;
        topk["default"] = 50;
        topk["description"] = "Limit vocabulary selection to top K candidates.";
        schema.append(topk);

        QVariantMap maxchars;
        maxchars["id"] = "max_chars";
        maxchars["name"] = "Max Chars";
        maxchars["type"] = "int";
        maxchars["min"] = 50;
        maxchars["max"] = 1000;
        maxchars["default"] = 256;
        maxchars["description"] = "Maximum characters per text segment.";
        maxchars["advanced"] = true;
        schema.append(maxchars);

        QVariantMap maxtokens;
        maxtokens["id"] = "max_tokens";
        maxtokens["name"] = "Max Tokens";
        maxtokens["type"] = "int";
        maxtokens["min"] = 128;
        maxtokens["max"] = 4096;
        maxtokens["default"] = 2048;
        maxtokens["description"] = "Maximum tokens generated per chunk.";
        maxtokens["advanced"] = true;
        schema.append(maxtokens);

        QVariantMap skipnorm;
        skipnorm["id"] = "skip_normalize";
        skipnorm["name"] = "Skip Normalization";
        skipnorm["type"] = "bool";
        skipnorm["default"] = false;
        skipnorm["description"] = "Bypass text normalization.";
        skipnorm["advanced"] = true;
        schema.append(skipnorm);

        QVariantMap skipphone;
        skipphone["id"] = "skip_phonemize";
        skipphone["name"] = "Skip Phonemization";
        skipphone["type"] = "bool";
        skipphone["default"] = false;
        skipphone["description"] = "Assume input text is already phonemes.";
        skipphone["advanced"] = true;
        schema.append(skipphone);

        QVariantMap watermark;
        watermark["id"] = "apply_watermark";
        watermark["name"] = "Apply Watermark";
        watermark["type"] = "bool";
        watermark["default"] = true;
        watermark["description"] = "Apply invisible watermark to output audio.";
        watermark["advanced"] = true;
        schema.append(watermark);
    }

    return schema;
}

void TtsEngineInstance::unloadVoice()
{
    dispatch(EventUnload{});
}

void TtsEngineInstance::unloadVoiceSync()
{
    QMetaObject::invokeMethod(m_worker, "unloadVoice", Qt::BlockingQueuedConnection);
    m_lastSamples.clear();
    m_lastSamplePreview.clear();
    m_lastPcm.clear();
    emit outputChanged();
    m_memoryBaselineArmed = false;
    m_memoryBaselineRamGb = 0.0;
    m_memoryBaselineVramGb = 0.0;
    if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
        m_estimatedRamBytes = 0;
        m_estimatedVramBytes = 0;
        emit memoryUsageChanged();
    }
    clearVoiceConfigTracking();
    applyState(StateUnloaded{});
}

void TtsEngineInstance::clearLastSamples()
{
    m_lastSamples.clear();
    m_lastSamplePreview.clear();
    m_lastPcm.clear();
    emit outputChanged();
}

void TtsEngineInstance::synthesize(const QString &text, int speakerId, float speed, const QVariantMap &settings)
{
    QVariantMap normalizedSettings;
    QString validationError;
    const QVariantList schema = schemaForCapability(QStringLiteral("tts"));
    const QVariantMap studioConfig = studioConfigForCapability(QStringLiteral("tts"));
    if (!TtsRequestValidator::normalize(schema, studioConfig, settings,
                                        normalizedSettings, validationError)) {
        Logger::error("TtsEngineInstance", QStringLiteral("Rejected TTS synthesis request: %1").arg(validationError));
        emit errorOccurred(validationError);
        return;
    }

    QStringList settingParts;
    for (auto it = normalizedSettings.constBegin(); it != normalizedSettings.constEnd(); ++it) {
        settingParts.append(QStringLiteral("%1: %2").arg(it.key(), it.value().toString()));
    }
    const QString familyId = m_familyConfig.value(QStringLiteral("id")).toString();
    Logger::info("TtsEngineInstance",
                 QStringLiteral("Normalized TTS request: family=\"%1\", runtime=\"%2\", settings={%3}")
                     .arg(familyId, m_lastRuntimePath, settingParts.join(QStringLiteral(", "))));

    QString normalizationProfile;
    const QString preparedText = TtsTextPreprocessor::prepare(text, studioConfig, normalizedSettings,
                                                                &normalizationProfile);
    // A number-only utterance cannot be language-detected by the UI and is
    // otherwise sent to multilingual backends with their English default.
    // The vietnorm profile is explicitly Vietnamese, so keep numeric TTS in
    // Vietnamese while leaving explicit language choices for real sentences.
    const QRegularExpression numericOnly(QStringLiteral("^[\\s\\d.,%/+\\-]+$"));
    if (normalizationProfile.compare(QStringLiteral("safe-vi-tts-v1"), Qt::CaseInsensitive) == 0
        && numericOnly.match(text).hasMatch()
        && normalizedSettings.value(QStringLiteral("lang")).toString().compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0) {
        normalizedSettings.insert(QStringLiteral("lang"), QStringLiteral("vi"));
        Logger::info("TtsEngineInstance", QStringLiteral("Selected lang=vi for numeric Vietnamese normalization input"));
    }
    if (preparedText != text) {
        Logger::info("TtsEngineInstance", QStringLiteral("Applied text normalization profile=\"%1\" chars=%2->%3")
                     .arg(normalizationProfile).arg(text.size()).arg(preparedText.size()));
    }

    m_isCloneAction = false;
    m_lastGenerationMode = QStringLiteral("tts");
    emit lastGenerationModeChanged();

    dispatch(EventSynthesize{preparedText, speakerId, speed, normalizedSettings});
}

void TtsEngineInstance::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings)
{
    QString cleanPath = referencePath;
    if (cleanPath.startsWith(QStringLiteral("file:///")))
        cleanPath = cleanPath.mid(8);
    else if (cleanPath.startsWith(QStringLiteral("file://")))
        cleanPath = cleanPath.mid(7);

    const QVariantMap studioConfig = studioConfigForCapability(QStringLiteral("voice-cloning"));
    QString normalizationProfile;
    const QString preparedText = TtsTextPreprocessor::prepare(text, studioConfig, settings,
                                                               &normalizationProfile);
    if (preparedText != text) {
        Logger::info("TtsEngineInstance",
                     QStringLiteral("Applied voice-cloning text normalization profile=\"%1\" chars=%2->%3")
                         .arg(normalizationProfile).arg(text.size()).arg(preparedText.size()));
    }

    m_isCloneAction = true;
    m_lastGenerationMode = QStringLiteral("voice-cloning");
    emit lastGenerationModeChanged();

    dispatch(EventCloneVoice{preparedText, cleanPath, settings});
}

void TtsEngineInstance::designVoice(const QString &text, const QVariantMap &settings)
{
    if (text.isEmpty()) {
        emit errorOccurred(QStringLiteral("Text cannot be empty"));
        return;
    }

    QVariantMap normalizedSettings;
    QString validationError;
    const QVariantList schema = schemaForCapability(QStringLiteral("voice-design"));
    const QVariantMap studioConfig = studioConfigForCapability(QStringLiteral("voice-design"));
    if (!TtsRequestValidator::normalize(schema, studioConfig, settings,
                                        normalizedSettings, validationError)) {
        Logger::error("TtsEngineInstance", QStringLiteral("Rejected Voice Design request: %1").arg(validationError));
        emit errorOccurred(validationError);
        return;
    }

    m_isCloneAction = false;
    m_lastGenerationMode = QStringLiteral("voice-design");
    emit lastGenerationModeChanged();

    QStringList settingParts;
    for (auto it = normalizedSettings.constBegin(); it != normalizedSettings.constEnd(); ++it) {
        settingParts.append(QStringLiteral("%1: %2").arg(it.key(), it.value().toString()));
    }
    const QString familyId = m_familyConfig.value(QStringLiteral("id")).toString();
    Logger::info("TtsEngineInstance",
                 QStringLiteral("Normalized Voice Design request: family=\"%1\", runtime=\"%2\", settings={%3}")
                     .arg(familyId, m_lastRuntimePath, settingParts.join(QStringLiteral(", "))));

    dispatch(EventSynthesize{text, 0, 1.0f, normalizedSettings});
}

void TtsEngineInstance::cancelProcessing()
{
    dispatch(EventCancelProcessing{});
}

void TtsEngineInstance::onWorkerModelLoaded(bool success, const QString &error, const QVariantList &schema)
{
    dispatch(EventWorkerLoaded{success, error, schema});
}

void TtsEngineInstance::onWorkerFinished(const QVector<float> &samples, int sampleRate)
{
    dispatch(EventWorkerFinished{samples, sampleRate});
}

void TtsEngineInstance::onWorkerError(const QString &error)
{
    dispatch(EventWorkerError{error});
}

void TtsEngineInstance::onWorkerProgress(int current, int total, const QString &stage, int chunkIndex, int chunkCount)
{
    if (!isProcessing()) {
        return;
    }

    QString label = progressLabelForStage(stage);
    if (chunkCount > 1 && chunkIndex >= 0) {
        label += QStringLiteral(" (%1/%2)").arg(chunkIndex + 1).arg(chunkCount);
    }

    if (total <= 0) {
        setGenerationProgress(m_generationProgress, true, label);
        return;
    }

    const int clampedCurrent = qBound(0, current, total);
    const int percent = qBound(0, static_cast<int>(std::round((double(clampedCurrent) * 100.0) / double(total))), 100);
    setGenerationProgress(percent, false, label);
}

void TtsEngineInstance::dispatch(const EngineEvent &event)
{
    std::visit(overloaded {
        // --- State: Unloaded ---
        [this](StateUnloaded&, const EventLoadVoice& e) {
            rememberLoadingVoiceConfig(e.config);

            auto *hw = HardwareManager::instance();
            if (hw) {
                m_memoryBaselineArmed = true;
                m_memoryBaselineRamGb = hw->ramUsed();
                m_memoryBaselineVramGb = hw->vramUsed();
            } else {
                m_memoryBaselineArmed = false;
            }

            applyState(StateLoading{});
            QMetaObject::invokeMethod(m_worker, "loadVoice", Qt::QueuedConnection, Q_ARG(QVariantMap, e.config));
        },
        [this](StateUnloaded&, const auto&) {},

        // --- State: Loading ---
        [this](StateLoading& s, const EventUnload&) {
            s.cancelRequested = true;
        },
        [this](StateLoading&, const EventLoadVoice& e) {
            const QString signature = voiceConfigSignature(e.config);
            if (!signature.isEmpty() && signature == m_loadingVoiceSignature) {
                Logger::debug("TtsEngineInstance", "Ignoring duplicate load request while model is already loading.");
                return;
            }

            Logger::warning("TtsEngineInstance", "Ignoring load request while another model is still loading.");
        },
        [this](StateLoading& s, const EventWorkerLoaded& e) {
            m_currentSchema = e.schema;
            emit schemaChanged();

            if (s.cancelRequested) {
                QMetaObject::invokeMethod(m_worker, "unloadVoice", Qt::QueuedConnection);
                m_memoryBaselineArmed = false;
                if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
                    m_estimatedRamBytes = 0;
                    m_estimatedVramBytes = 0;
                    emit memoryUsageChanged();
                }
                clearVoiceConfigTracking();
                applyState(StateUnloaded{});
            } else {
                if (e.success) {
                    bool updatedFromDelta = false;
                    auto *hw = HardwareManager::instance();
                    if (m_memoryBaselineArmed && hw) {
                        const double deltaRamGb = std::max(0.0, hw->ramUsed() - m_memoryBaselineRamGb);
                        const double deltaVramGb = std::max(0.0, hw->vramUsed() - m_memoryBaselineVramGb);
                        const qint64 newRam = static_cast<qint64>(deltaRamGb * 1024.0 * 1024.0 * 1024.0);
                        const qint64 newVram = static_cast<qint64>(deltaVramGb * 1024.0 * 1024.0 * 1024.0);

                        if (newRam > 0 || newVram > 0) {
                            m_estimatedRamBytes = newRam;
                            m_estimatedVramBytes = newVram;
                            emit memoryUsageChanged();
                            updatedFromDelta = true;
                        }
                    }

                    updateMemoryUsageEstimates();

                    m_memoryBaselineArmed = false;
                    rememberLoadedVoiceConfig();
                    applyState(StateReady{});
                } else {
                    m_memoryBaselineArmed = false;
                    if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
                        m_estimatedRamBytes = 0;
                        m_estimatedVramBytes = 0;
                        emit memoryUsageChanged();
                    }
                    clearVoiceConfigTracking();
                    applyState(StateError{e.error});
                    emit errorOccurred(e.error);
                }
            }
        },
        [this](StateLoading&, const auto&) {},

        // --- State: Ready ---
        [this](StateReady&, const EventLoadVoice& e) {
            const QString signature = voiceConfigSignature(e.config);
            if (!signature.isEmpty() && signature == m_loadedVoiceSignature) {
                Logger::debug("TtsEngineInstance", "Ignoring duplicate load request for the active model/runtime.");
                return;
            }

            rememberLoadingVoiceConfig(e.config);

            auto *hw = HardwareManager::instance();
            if (hw) {
                m_memoryBaselineArmed = true;
                m_memoryBaselineRamGb = hw->ramUsed();
                m_memoryBaselineVramGb = hw->vramUsed();
            }

            applyState(StateLoading{});
            QMetaObject::invokeMethod(m_worker, "loadVoice", Qt::QueuedConnection, Q_ARG(QVariantMap, e.config));
        },
        [this](StateReady&, const EventUnload&) {
            QMetaObject::invokeMethod(m_worker, "unloadVoice", Qt::QueuedConnection);
            m_lastSamples.clear();
            m_lastSamplePreview.clear();
            m_lastPcm.clear();
            emit outputChanged();
            m_memoryBaselineArmed = false;
            m_memoryBaselineRamGb = 0.0;
            m_memoryBaselineVramGb = 0.0;
            if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
                m_estimatedRamBytes = 0;
                m_estimatedVramBytes = 0;
                emit memoryUsageChanged();
            }
            clearVoiceConfigTracking();
            applyState(StateUnloaded{});
        },
        [this](StateReady&, const EventSynthesize& e) {
            m_isCloneAction = false;
            m_lastSamples.clear();
            m_lastSamplePreview.clear();
            m_lastPcm.clear();
            emit outputChanged();
            resetGenerationProgress();
            applyState(StateProcessing{});
            QMetaObject::invokeMethod(m_worker, "synthesize", Qt::QueuedConnection,
                                      Q_ARG(QString, e.text), Q_ARG(int, e.speakerId),
                                      Q_ARG(float, e.speed), Q_ARG(QVariantMap, e.settings));
        },
        [this](StateReady&, const EventCloneVoice& e) {
            m_isCloneAction = true;
            m_lastSamples.clear();
            m_lastSamplePreview.clear();
            m_lastPcm.clear();
            emit outputChanged();
            resetGenerationProgress();
            applyState(StateProcessing{});
            QMetaObject::invokeMethod(m_worker, "cloneVoice", Qt::QueuedConnection,
                                       Q_ARG(QString, e.text), Q_ARG(QString, e.referencePath),
                                       Q_ARG(QVariantMap, e.settings));
        },
        [this](StateReady&, const auto&) {},

        // --- State: Processing ---
        [this](StateProcessing& s, const EventUnload&) {
            s.unloadRequested = true;
            QMetaObject::invokeMethod(m_worker, "cancelProcessing", Qt::DirectConnection);
        },
        [this](StateProcessing& s, const EventCancelProcessing&) {
            s.stopRequested = true;
            QMetaObject::invokeMethod(m_worker, "cancelProcessing", Qt::DirectConnection);
        },
        [this](StateProcessing& s, const EventWorkerFinished& e) {
            if (s.stopRequested) {
                applyState(StateReady{});
                return;
            }

            if (s.unloadRequested) {
                QMetaObject::invokeMethod(m_worker, "unloadVoice", Qt::QueuedConnection);
                m_memoryBaselineArmed = false;
                if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
                    m_estimatedRamBytes = 0;
                    m_estimatedVramBytes = 0;
                    emit memoryUsageChanged();
                }
                clearVoiceConfigTracking();
                applyState(StateUnloaded{});
                return;
            }

            m_sampleRate = e.sampleRate;
            m_lastSamples = e.samples;
            m_lastSamplePreview.clear();
            if (!m_lastSamples.isEmpty()) {
                const int step = std::max<int>(1, m_lastSamples.size() / 1000);
                m_lastSamplePreview.reserve(m_lastSamples.size() / step + 1);
                for (int i = 0; i < m_lastSamples.size(); i += step) {
                    const float sample = std::isfinite(m_lastSamples[i]) ? m_lastSamples[i] : 0.0f;
                    m_lastSamplePreview.append(sample);
                }
            }
            m_lastPcm.resize(e.samples.size() * 2);
            auto *out = reinterpret_cast<int16_t *>(m_lastPcm.data());
            for (int i = 0; i < e.samples.size(); ++i) {
                float sample = std::isfinite(e.samples[i]) ? std::clamp(e.samples[i], -1.0f, 1.0f) : 0.0f;
                out[i] = static_cast<int16_t>(sample * 32767.0f);
            }

            emit sampleRateChanged();
            emit outputChanged();
            emit synthesisFinished(m_lastPcm, e.sampleRate);

            applyState(StateReady{});
        },
        [this](StateProcessing& s, const EventWorkerError& e) {
            if (s.stopRequested) {
                applyState(StateReady{});
                return;
            }

            if (s.unloadRequested) {
                QMetaObject::invokeMethod(m_worker, "unloadVoice", Qt::QueuedConnection);
                m_memoryBaselineArmed = false;
                if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
                    m_estimatedRamBytes = 0;
                    m_estimatedVramBytes = 0;
                    emit memoryUsageChanged();
                }
                clearVoiceConfigTracking();
                applyState(StateUnloaded{});
            } else {
                emit errorOccurred(e.error);
                applyState(StateReady{});
            }
        },
        [this](StateProcessing&, const auto&) {},

        // --- State: Error ---
        [this](StateError&, const EventLoadVoice& e) {
            rememberLoadingVoiceConfig(e.config);
            applyState(StateLoading{});
            QMetaObject::invokeMethod(m_worker, "loadVoice", Qt::QueuedConnection, Q_ARG(QVariantMap, e.config));
        },
        [this](StateError&, const EventUnload&) {
            QMetaObject::invokeMethod(m_worker, "unloadVoice", Qt::QueuedConnection);
            m_memoryBaselineArmed = false;
            if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
                m_estimatedRamBytes = 0;
                m_estimatedVramBytes = 0;
                emit memoryUsageChanged();
            }
            clearVoiceConfigTracking();
            applyState(StateUnloaded{});
        },
        [this](StateError&, const auto&) {}

    }, m_state, event);
}

QString TtsEngineInstance::voiceConfigSignature(const QVariantMap &config) const
{
    QStringList keys = config.keys();
    keys.sort(Qt::CaseSensitive);

    QStringList parts;
    parts.reserve(keys.size());
    for (const QString &key : keys) {
        parts.append(key + QLatin1Char('=') + normalizedConfigValue(config.value(key)));
    }

    return parts.join(QLatin1Char('|'));
}

void TtsEngineInstance::rememberLoadingVoiceConfig(const QVariantMap &config)
{
    m_lastVoiceConfig = config;
    m_lastModelPath = config.value(QStringLiteral("model")).toString();
    m_lastCodecPath = config.value(QStringLiteral("codec")).toString();
    if (config.contains(QStringLiteral("runtimePath"))) {
        m_lastRuntimePath = config.value(QStringLiteral("runtimePath")).toString();
    }

    m_loadingVoiceSignature = voiceConfigSignature(config);
}

void TtsEngineInstance::rememberLoadedVoiceConfig()
{
    m_loadedVoiceSignature = m_loadingVoiceSignature;
    m_loadingVoiceSignature.clear();
}

void TtsEngineInstance::clearVoiceConfigTracking()
{
    m_lastVoiceConfig.clear();
    m_loadingVoiceSignature.clear();
    m_loadedVoiceSignature.clear();
}

void TtsEngineInstance::resetGenerationProgress()
{
    setGenerationProgress(0, true, QStringLiteral("Generating audio"));
}

void TtsEngineInstance::setGenerationProgress(int progress, bool estimated, const QString &label)
{
    const int clamped = qBound(0, progress, 100);
    const QString effectiveLabel = label.isEmpty() ? QStringLiteral("Generating audio") : label;

    if (m_generationProgress == clamped &&
        m_generationProgressEstimated == estimated &&
        m_generationProgressLabel == effectiveLabel) {
        return;
    }

    m_generationProgress = clamped;
    m_generationProgressEstimated = estimated;
    m_generationProgressLabel = effectiveLabel;
    emit generationProgressChanged();
}

void TtsEngineInstance::updateCpuUsage()
{
    if (state() != Processing) {
        if (m_cpuUsage != 0) {
            m_cpuUsage = 0;
            emit cpuUsageChanged();
        }
        return;
    }

    auto *hw = HardwareManager::instance();
    if (hw) {
        // Since synthesis is usually single-threaded or uses a fixed number of threads,
        // we can use the system CPU usage as an approximation during the processing state,
        // or refine it if needed. For now, we show the actual system load during synthesis.
        double usage = hw->cpuUsage();
        if (m_cpuUsage != usage) {
            m_cpuUsage = usage;
            emit cpuUsageChanged();
        }
    }
}

void TtsEngineInstance::applyState(const EngineState &newState)
{
    State oldType = state();
    bool oldLoaded = isModelLoaded();
    bool oldProcessing = isProcessing();

    m_state = newState;

    State newType = state();
    if (newType == Processing) {
        m_cpuTimer->start();
    } else {
        m_cpuTimer->stop();
        m_cpuUsage = 0;
        emit cpuUsageChanged();
    }

    if (oldType != newType) {
        emit stateChanged();
    }
    if (isModelLoaded() != oldLoaded) {
        emit modelLoadedChanged();
    }
    if (isProcessing() != oldProcessing) {
        emit processingChanged();
    }
}

QString TtsEngineInstance::estimatedRamUsage() const
{
    return formatBytes(m_estimatedRamBytes);
}

QString TtsEngineInstance::estimatedVramUsage() const
{
    return formatBytes(m_estimatedVramBytes);
}

void TtsEngineInstance::updateMemoryUsageEstimates()
{
    qint64 modelBytes = 0;
    qint64 codecBytes = 0;

    if (!m_lastModelPath.isEmpty()) {
        QFileInfo info(m_lastModelPath);
        if (info.exists() && info.isFile()) {
            modelBytes = info.size();
        }
    }

    if (!m_lastCodecPath.isEmpty()) {
        QFileInfo info(m_lastCodecPath);
        if (info.exists() && info.isFile()) {
            codecBytes = info.size();
        }
    }

    qint64 baseBytes = qMax(modelBytes + codecBytes, uniqueConfigAssetBytes(m_lastVoiceConfig));
    if (baseBytes <= 0) {
        if (m_estimatedRamBytes != 0 || m_estimatedVramBytes != 0) {
            m_estimatedRamBytes = 0;
            m_estimatedVramBytes = 0;
            emit memoryUsageChanged();
        }
        return;
    }

    const QString runtime = m_lastRuntimePath.toLower();
    const QString familyId = m_familyConfig.value(QStringLiteral("id")).toString().toLower();
    const bool gpuRuntime = runtime.contains(QStringLiteral("cuda")) ||
                            runtime.contains(QStringLiteral("gpu")) ||
                            runtime.contains(QStringLiteral("directml")) ||
                            runtime.contains(QStringLiteral("vulkan")) ||
                            runtime.contains(QStringLiteral("metal"));
    const bool vieneuNative = familyId.contains(QStringLiteral("vieneu-tts-v3")) ||
                              (runtime.contains(QStringLiteral("vieneu")) &&
                               m_lastVoiceConfig.contains(QStringLiteral("acoustic")));

    qint64 newRam = 0;
    qint64 newVram = 0;

    if (vieneuNative) {
        if (gpuRuntime) {
            newVram = static_cast<qint64>(baseBytes * 0.35);
            newRam = static_cast<qint64>(baseBytes * 2.60);
        } else {
            newRam = static_cast<qint64>(baseBytes * 2.80);
            newVram = 0;
        }
    } else if (gpuRuntime) {
        if (isVoxCpm2FullPrecisionModel(m_lastModelPath, modelBytes)) {
            constexpr qint64 minRecommendedVramBytes = 8LL * 1024LL * 1024LL * 1024LL;
            newVram = qMax(static_cast<qint64>(baseBytes * 1.55), minRecommendedVramBytes);
            newRam = static_cast<qint64>(baseBytes * 1.05);
        } else {
            newVram = static_cast<qint64>(baseBytes * 0.95);
            newRam = static_cast<qint64>(baseBytes * 0.30);
        }
    } else {
        newRam = static_cast<qint64>(baseBytes * 1.12);
        newVram = 0;
    }

    const qint64 effectiveRam = qMax(newRam, m_estimatedRamBytes);
    const qint64 effectiveVram = qMax(newVram, m_estimatedVramBytes);
    if (effectiveRam != m_estimatedRamBytes || effectiveVram != m_estimatedVramBytes) {
        m_estimatedRamBytes = effectiveRam;
        m_estimatedVramBytes = effectiveVram;
        emit memoryUsageChanged();
    }
}

QString TtsEngineInstance::formatBytes(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("0 MB");
    }

    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb < 1024.0) {
        return QString::number(mb, 'f', mb >= 100.0 ? 0 : 1) + QStringLiteral(" MB");
    }

    const double gb = mb / 1024.0;
    return QString::number(gb, 'f', gb >= 100.0 ? 0 : 2) + QStringLiteral(" GB");
}

} // namespace LAStudio
