#include "KokoroVietnameseBackend.h"
#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/KokoroVietnameseInterface.h>

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cstring>

namespace LAStudio {

namespace {

QString normalizePath(const QString &path)
{
    return QDir::toNativeSeparators(PathUtils::toNativeShortPath(path));
}

QString runtimeRootFromLibrary(const QString &runtimePath)
{
    QFileInfo libInfo(runtimePath);
    QDir dir(libInfo.absolutePath());
    if (dir.dirName().compare(QStringLiteral("bin"), Qt::CaseInsensitive) == 0) {
        dir.cdUp();
    }
    return QDir::toNativeSeparators(dir.absolutePath());
}

QString firstExistingFile(const QStringList &paths)
{
    for (const QString &path : paths) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QString();
}

QString firstExistingDir(const QStringList &paths)
{
    for (const QString &path : paths) {
        if (!path.isEmpty() && QFileInfo(path).isDir()) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QString();
}

QString voiceDisplayName(const QString &voiceId)
{
    if (voiceId == QStringLiteral("diem_trinh")) return QStringLiteral("Diem Trinh");
    if (voiceId == QStringLiteral("hung_thinh")) return QStringLiteral("Hung Thinh");

    QString name = voiceId;
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    name.replace(QLatin1Char('-'), QLatin1Char(' '));
    const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList titled;
    for (QString part : parts) {
        if (!part.isEmpty()) {
            part[0] = part[0].toUpper();
        }
        titled << part;
    }
    return titled.join(QLatin1Char(' '));
}

QStringList findVoiceIds(const QString &voicepackDir)
{
    QDir dir(voicepackDir);
    QStringList files = dir.entryList(QStringList{QStringLiteral("*.bin")}, QDir::Files, QDir::Name);
    QStringList ids;
    for (const QString &file : files) {
        ids << QFileInfo(file).completeBaseName();
    }
    ids.removeDuplicates();
    std::sort(ids.begin(), ids.end());
    return ids;
}

void appendIntParam(QVariantList &schema,
                    const QString &id,
                    const QString &name,
                    int min,
                    int max,
                    int def,
                    const QString &description)
{
    QVariantMap p;
    p[QStringLiteral("id")] = id;
    p[QStringLiteral("name")] = name;
    p[QStringLiteral("type")] = QStringLiteral("int");
    p[QStringLiteral("min")] = min;
    p[QStringLiteral("max")] = max;
    p[QStringLiteral("default")] = def;
    p[QStringLiteral("description")] = description;
    p[QStringLiteral("advanced")] = true;
    schema.append(p);
}

QString runtimeError(KokoroVietnameseInterface &ki, const QString &fallback)
{
    if (ki.kokoro_vi_last_error) {
        const char *err = ki.kokoro_vi_last_error();
        if (err && *err) {
            return QString::fromUtf8(err);
        }
    }
    return fallback;
}

} // namespace

KokoroVietnameseBackend::~KokoroVietnameseBackend()
{
    unload();
}

bool KokoroVietnameseBackend::load(const QVariantMap &config, QString &error, QVariantList &schema)
{
    const QString runtimePath = normalizePath(config.value(QStringLiteral("runtimePath")).toString());
    if (runtimePath.isEmpty() || !QFileInfo::exists(runtimePath)) {
        error = QStringLiteral("Kokoro-Vietnamese requires kokoro-vietnamese.dll from the selected runtime package.");
        Logger::error("KokoroVietnameseBackend", error);
        return false;
    }

    auto &ki = KokoroVietnameseInterface::instance();
    if (!ki.load(runtimePath)) {
        error = QStringLiteral("Failed to load Kokoro-Vietnamese runtime: ") + ki.errorString();
        Logger::error("KokoroVietnameseBackend", error);
        return false;
    }

    const QString runtimeRoot = runtimeRootFromLibrary(runtimePath);
    const QString catalogModel = normalizePath(config.value(QStringLiteral("model")).toString());
    const QString catalogConfig = normalizePath(config.value(QStringLiteral("config")).toString());
    const QString catalogVoices = normalizePath(config.value(QStringLiteral("voices")).toString());

    const QString runtimeModelDir = QDir(runtimeRoot).absoluteFilePath(QStringLiteral("models"));
    const QString catalogModelDir = catalogModel.isEmpty() ? QString() : QFileInfo(catalogModel).absolutePath();

    const QString onnxPath = firstExistingFile({
        QDir(runtimeModelDir).absoluteFilePath(QStringLiteral("kokoro_vi.onnx")),
        catalogModel,
        QDir(catalogModelDir).absoluteFilePath(QStringLiteral("kokoro_vi.onnx"))
    });
    const QString configPath = firstExistingFile({
        QDir(runtimeModelDir).absoluteFilePath(QStringLiteral("config.json")),
        catalogConfig,
        QDir(catalogModelDir).absoluteFilePath(QStringLiteral("config.json"))
    });

    const QString catalogVoiceDir = catalogVoices.isEmpty()
        ? QString()
        : (QFileInfo(catalogVoices).isDir() ? catalogVoices : QFileInfo(catalogVoices).absolutePath());
    QString voicepackDir = firstExistingDir({
        QDir(runtimeModelDir).absoluteFilePath(QStringLiteral("voicepacks")),
        catalogVoiceDir,
        QDir(catalogModelDir).absoluteFilePath(QStringLiteral("voicepacks"))
    });

    if (onnxPath.isEmpty() || configPath.isEmpty() || voicepackDir.isEmpty()) {
        error = QStringLiteral("Kokoro-Vietnamese runtime assets are incomplete. Expected models/kokoro_vi.onnx, models/config.json, and models/voicepacks/*.bin inside the runtime package.");
        Logger::error("KokoroVietnameseBackend", error);
        return false;
    }

    const QString g2pExePath = firstExistingFile({
        QDir(QFileInfo(runtimePath).absolutePath()).absoluteFilePath(QStringLiteral("kokoro_vi_g2p.exe")),
        QDir(runtimeRoot).absoluteFilePath(QStringLiteral("bin/kokoro_vi_g2p.exe"))
    });
    if (g2pExePath.isEmpty()) {
        error = QStringLiteral("Kokoro-Vietnamese runtime is missing bin/kokoro_vi_g2p.exe.");
        Logger::error("KokoroVietnameseBackend", error);
        return false;
    }

    const QStringList voiceIds = findVoiceIds(voicepackDir);
    if (voiceIds.isEmpty()) {
        error = QStringLiteral("Kokoro-Vietnamese voicepack directory contains no .bin voicepacks: ") + voicepackDir;
        Logger::error("KokoroVietnameseBackend", error);
        return false;
    }

    QByteArray modelDirBytes = QFileInfo(onnxPath).absolutePath().toUtf8();
    QByteArray onnxBytes = onnxPath.toUtf8();
    QByteArray configBytes = configPath.toUtf8();
    QByteArray voicepackBytes = voicepackDir.toUtf8();
    QByteArray g2pBytes = g2pExePath.toUtf8();

    kokoro_vi_init_params params{};
    ki.kokoro_vi_init_default_params(&params);
    params.model_dir = modelDirBytes.constData();
    params.onnx_path = onnxBytes.constData();
    params.config_path = configBytes.constData();
    params.voicepack_dir = voicepackBytes.constData();
    params.g2p_exe_path = g2pBytes.constData();
    params.n_threads = InferenceThreadPolicy::recommendedThreadCount(InferenceBackendProfile::Kokoro);

    m_context = ki.kokoro_vi_init(&params);
    if (!m_context) {
        error = QStringLiteral("Failed to initialize Kokoro-Vietnamese: ") +
                runtimeError(ki, QStringLiteral("runtime returned null context"));
        Logger::error("KokoroVietnameseBackend", error);
        return false;
    }

    m_loaded = true;
    m_runtimePath = runtimePath;
    m_modelDir = QFileInfo(onnxPath).absolutePath();
    m_currentVoiceId = voiceIds.contains(QStringLiteral("diem_trinh")) ? QStringLiteral("diem_trinh") : voiceIds.first();

    QVariantList voiceChoices;
    for (const QString &voiceId : voiceIds) {
        QVariantMap choice;
        choice[QStringLiteral("text")] = voiceDisplayName(voiceId);
        choice[QStringLiteral("value")] = voiceId;
        choice[QStringLiteral("detail")] = QStringLiteral("Vietnamese preset voice");
        voiceChoices.append(choice);
    }

    QVariantMap voiceParam;
    voiceParam[QStringLiteral("id")] = QStringLiteral("voice");
    voiceParam[QStringLiteral("name")] = QStringLiteral("Voice");
    voiceParam[QStringLiteral("type")] = QStringLiteral("choice");
    voiceParam[QStringLiteral("choices")] = voiceChoices;
    voiceParam[QStringLiteral("default")] = m_currentVoiceId;
    voiceParam[QStringLiteral("description")] = QStringLiteral("Selects the Kokoro-Vietnamese preset voice.");
    schema.append(voiceParam);

    appendIntParam(schema,
                   QStringLiteral("crossfade_ms"),
                   QStringLiteral("Crossfade"),
                   0,
                   250,
                   50,
                   QStringLiteral("Crossfade in milliseconds between sentence chunks."));
    appendIntParam(schema,
                   QStringLiteral("max_phonemes"),
                   QStringLiteral("Max Phonemes"),
                   64,
                   510,
                   510,
                   QStringLiteral("Maximum phonemes per chunk before splitting."));

    Logger::info("KokoroVietnameseBackend",
                 QStringLiteral("Loaded Kokoro-Vietnamese runtime %1 with model dir %2 and voicepack dir %3")
                     .arg(runtimePath, m_modelDir, voicepackDir));
    return true;
}

void KokoroVietnameseBackend::unload()
{
    if (m_context) {
        auto &ki = KokoroVietnameseInterface::instance();
        if (ki.isLoaded() && ki.kokoro_vi_free) {
            ki.kokoro_vi_free(m_context);
        }
        m_context = nullptr;
    }
    m_loaded = false;
    m_runtimePath.clear();
    m_modelDir.clear();
    m_currentVoiceId.clear();
}

void KokoroVietnameseBackend::cancelProcessing()
{
    m_cancelRequested = true;
}

bool KokoroVietnameseBackend::synthesize(const QString &text,
                                         float speed,
                                         const QVariantMap &settings,
                                         QVector<float> &samples,
                                         int &sampleRate,
                                         QString &error)
{
    m_cancelRequested = false;
    auto &ki = KokoroVietnameseInterface::instance();
    if (!ki.isLoaded() || !m_loaded || !m_context) {
        error = QStringLiteral("Kokoro-Vietnamese runtime was unloaded unexpectedly.");
        return false;
    }

    const QString requestedVoice = settings.value(QStringLiteral("voice"), m_currentVoiceId).toString();
    const QString voiceId = requestedVoice.isEmpty() ? m_currentVoiceId : requestedVoice;
    const float runtimeSpeed = speed > 0.0f ? speed : 1.0f;
    const int crossfadeMs = settings.value(QStringLiteral("crossfade_ms"), 50).toInt();
    const int maxPhonemes = settings.value(QStringLiteral("max_phonemes"), 510).toInt();

    QByteArray textBytes = text.toUtf8();
    QByteArray voiceBytes = voiceId.toUtf8();

    kokoro_vi_tts_params params{};
    ki.kokoro_vi_tts_default_params(&params);
    params.text = textBytes.constData();
    params.voice_id = voiceBytes.constData();
    params.speed = runtimeSpeed;
    params.crossfade_ms = std::max(0, std::min(crossfadeMs, 250));
    params.max_phonemes = std::max(64, std::min(maxPhonemes, 510));
    if (m_cancelRequested.load()) {
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }

    kokoro_vi_audio audio{};
    const int rc = ki.kokoro_vi_synthesize(m_context, &params, &audio);
    if (m_cancelRequested.load()) {
        if (audio.samples && ki.kokoro_vi_audio_free) {
            ki.kokoro_vi_audio_free(&audio);
        }
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }
    if (rc != 0 || !audio.samples || audio.n_samples <= 0) {
        error = QStringLiteral("Kokoro-Vietnamese synthesis failed: ") +
                runtimeError(ki, QStringLiteral("runtime returned no audio"));
        if (audio.samples && ki.kokoro_vi_audio_free) {
            ki.kokoro_vi_audio_free(&audio);
        }
        Logger::error("KokoroVietnameseBackend", error);
        return false;
    }

    samples.resize(audio.n_samples);
    std::memcpy(samples.data(), audio.samples, sizeof(float) * static_cast<size_t>(audio.n_samples));
    sampleRate = audio.sample_rate > 0 ? audio.sample_rate : 24000;
    if (ki.kokoro_vi_audio_free) {
        ki.kokoro_vi_audio_free(&audio);
    }

    m_currentVoiceId = voiceId;
    return true;
}

bool KokoroVietnameseBackend::cloneVoice(const QString &text,
                                         const QString &referencePath,
                                         const QVariantMap &settings,
                                         QVector<float> &samples,
                                         int &sampleRate,
                                         QString &error)
{
    Q_UNUSED(text);
    Q_UNUSED(referencePath);
    Q_UNUSED(settings);
    Q_UNUSED(samples);
    Q_UNUSED(sampleRate);
    error = QStringLiteral("Kokoro-Vietnamese supports preset voices only in this runtime.");
    return false;
}

} // namespace LAStudio
