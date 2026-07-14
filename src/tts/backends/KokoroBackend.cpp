#include "KokoroBackend.h"
#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/CrispKokoroInterface.h>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <algorithm>

namespace LAStudio {

static QString kokoroVoiceDisplayName(const QString &voiceKey)
{
    if (voiceKey == QStringLiteral("af_heart")) return QStringLiteral("af_heart - American-Female");
    if (voiceKey == QStringLiteral("ef_dora")) return QStringLiteral("ef_dora - Spanish-Female");
    if (voiceKey == QStringLiteral("ff_siwis")) return QStringLiteral("ff_siwis - French-Female");
    if (voiceKey == QStringLiteral("df_victoria")) return QStringLiteral("df_victoria - German-Female");
    if (voiceKey == QStringLiteral("df_eva")) return QStringLiteral("df_eva - German-Female");
    if (voiceKey == QStringLiteral("dm_martin")) return QStringLiteral("dm_martin - German-Male");
    if (voiceKey == QStringLiteral("dm_bernd")) return QStringLiteral("dm_bernd - German-Male");
    return voiceKey;
}

static QString kokoroVoiceCodeLegend()
{
    return QStringLiteral("Code legend: a=American (Mỹ), b=British (Anh), j=Japanese (Nhật), z=Chinese (Trung Quốc); f=Female (Nữ), m=Male (Nam)");
}

static QString kokoroVoiceReadableLabel(const QString &voiceKey)
{
    if (voiceKey == QStringLiteral("af_heart")) return QStringLiteral("American voice, Female, English (US)");
    if (voiceKey == QStringLiteral("ef_dora")) return QStringLiteral("Spanish voice, Female");
    if (voiceKey == QStringLiteral("ff_siwis")) return QStringLiteral("French voice, Female");
    if (voiceKey == QStringLiteral("df_victoria")) return QStringLiteral("German voice, Female");
    if (voiceKey == QStringLiteral("df_eva")) return QStringLiteral("German voice, Female");
    if (voiceKey == QStringLiteral("dm_martin")) return QStringLiteral("German voice, Male");
    if (voiceKey == QStringLiteral("dm_bernd")) return QStringLiteral("German voice, Male");
    return QStringLiteral("Kokoro voice pack");
}

static QString kokoroVoiceDetail(const QString &voiceKey)
{
    if (voiceKey == QStringLiteral("af_heart")) return QStringLiteral("Default English voice");
    if (voiceKey == QStringLiteral("ef_dora")) return QStringLiteral("Spanish voice");
    if (voiceKey == QStringLiteral("ff_siwis")) return QStringLiteral("French fallback voice");
    if (voiceKey == QStringLiteral("df_victoria")) return QStringLiteral("German voice");
    if (voiceKey == QStringLiteral("df_eva")) return QStringLiteral("German alternate voice");
    if (voiceKey == QStringLiteral("dm_martin")) return QStringLiteral("German male voice");
    if (voiceKey == QStringLiteral("dm_bernd")) return QStringLiteral("German male alternate voice");
    return QStringLiteral("Kokoro voice pack");
}

static QString normalizeKokoroText(QString text)
{
    text = text.normalized(QString::NormalizationForm_KC);
    text.replace(QChar(0x00A0), QLatin1Char(' '));
    text.replace(QChar(0x2009), QLatin1Char(' '));
    text.replace(QChar(0x2018), QLatin1Char('\''));
    text.replace(QChar(0x2019), QLatin1Char('\''));
    text.replace(QChar(0x201C), QLatin1Char('"'));
    text.replace(QChar(0x201D), QLatin1Char('"'));
    text.replace(QChar(0x2013), QLatin1Char('-'));
    text.replace(QChar(0x2014), QLatin1Char('-'));
    text.replace(QChar(0x2026), QStringLiteral("..."));

    for (QChar &ch : text) {
        if (ch.isPrint() || ch.isSpace()) continue;
        ch = QLatin1Char(' ');
    }

    text = text.simplified();
    return text;
}

KokoroBackend::~KokoroBackend()
{
    unload();
}

bool KokoroBackend::load(const QVariantMap &config, QString &error, QVariantList &schema)
{
    QString modelPath = PathUtils::toNativeShortPath(config.value("model").toString());
    QString codecPath = PathUtils::toNativeShortPath(config.value("codec").toString());
    QString runtimePath = PathUtils::toNativeShortPath(config.value("runtimePath").toString());

    QVariantMap lengthScale;
    lengthScale["id"] = "length_scale";
    lengthScale["name"] = "Length Scale";
    lengthScale["type"] = "float";
    lengthScale["min"] = 0.25;
    lengthScale["max"] = 4.0;
    lengthScale["default"] = 1.0;
    lengthScale["description"] = "Duration multiplier. Values above 1.0 speak slower; below 1.0 speak faster.";
    schema.append(lengthScale);

    auto& ki = CrispKokoroInterface::instance();
    if (!runtimePath.isEmpty()) {
        if (!ki.load(runtimePath)) {
            error = QString("Failed to load CrispASR Kokoro runtime: ") + ki.errorString();
            Logger::error("KokoroBackend", error);
            return false;
        }
    }

    if (!ki.isLoaded()) {
        error = QStringLiteral("No CrispASR Kokoro runtime loaded. Please select one in settings.");
        Logger::error("KokoroBackend", error);
        return false;
    }

    if (!ki.hasKokoroPhonemizer()) {
        error = ki.kokoroPhonemizerErrorString();
        Logger::error("KokoroBackend", error);
        return false;
    }

    QString resolvedModelPath = modelPath;
    QByteArray langBytes = config.value(QStringLiteral("lang"), QStringLiteral("en-us")).toString().toUtf8();
    if (ki.crispasr_kokoro_resolve_model_for_lang) {
        QByteArray modelPathBytes = modelPath.toUtf8();
        char resolved[2048] = {0};
        int rc = ki.crispasr_kokoro_resolve_model_for_lang(
            modelPathBytes.constData(), langBytes.constData(), resolved, static_cast<int>(sizeof(resolved)));
        if (rc >= 0 && resolved[0] != '\0') {
            resolvedModelPath = QString::fromUtf8(resolved);
        }
    }

    m_session = ki.openKokoroSession(
        resolvedModelPath,
        InferenceThreadPolicy::recommendedThreadCount(InferenceBackendProfile::Kokoro),
        false,
        true);
    if (!m_session) {
        error = QStringLiteral("Failed to initialize Kokoro model via CrispASR runtime.");
        Logger::error("KokoroBackend", error);
        return false;
    }

    m_modelPath = modelPath;

    QString voicePath = codecPath;
    if (voicePath.isEmpty()) {
        QFileInfo modelInfo(resolvedModelPath);
        const QString siblingVoice = modelInfo.dir().absoluteFilePath(QStringLiteral("kokoro-voice-af_heart.gguf"));
        if (QFileInfo::exists(siblingVoice)) {
            voicePath = siblingVoice;
        }
    }

    if (voicePath.isEmpty() && ki.crispasr_kokoro_resolve_fallback_voice) {
        QByteArray modelPathBytes = resolvedModelPath.toUtf8();
        char resolvedVoice[2048] = {0};
        char picked[64] = {0};
        int rc = ki.crispasr_kokoro_resolve_fallback_voice(
            modelPathBytes.constData(), langBytes.constData(),
            resolvedVoice, static_cast<int>(sizeof(resolvedVoice)),
            picked, static_cast<int>(sizeof(picked)));
        if (rc == 0 && resolvedVoice[0] != '\0') {
            voicePath = QString::fromUtf8(resolvedVoice);
        }
    }

    if (voicePath.isEmpty() || !QFileInfo::exists(voicePath)) {
        error = QStringLiteral("Kokoro requires a kokoro-voice-*.gguf voice pack next to the model or selected as tokenizer/voice file.");
        Logger::error("KokoroBackend", error);
        unload();
        return false;
    }

    if (ki.loadVoicePack(m_session, voicePath) != 0) {
        error = QStringLiteral("Failed to load Kokoro voice pack: ") + voicePath;
        Logger::error("KokoroBackend", error);
        unload();
        return false;
    }

    m_currentVoicePath = QDir::toNativeSeparators(voicePath);

    // Discover sibling voices for the schema
    QFileInfo modelInfo(resolvedModelPath);
    QDir modelDir = modelInfo.dir();
    
    QSet<QString> uniqueVoicePaths;
    uniqueVoicePaths.insert(m_currentVoicePath);

    QStringList filters;
    filters << QStringLiteral("*.gguf") << QStringLiteral("*.bin");
    QStringList potentialFiles = modelDir.entryList(filters, QDir::Files, QDir::Name);
    
    QString modelFileName = QFileInfo(resolvedModelPath).fileName();
    for (const QString& fileName : potentialFiles) {
        if (fileName == modelFileName || fileName == QStringLiteral("tokenizer.gguf") || fileName == QStringLiteral("config.json"))
            continue;
            
        uniqueVoicePaths.insert(QDir::toNativeSeparators(modelDir.absoluteFilePath(fileName)));
    }

    Logger::info("KokoroBackend", QString("Discovered %1 potential Kokoro voice packs in %2").arg(uniqueVoicePaths.size()).arg(modelDir.absolutePath()));
    
    QVariantList voiceChoices;
    QStringList sortedVoices = uniqueVoicePaths.values();
    std::sort(sortedVoices.begin(), sortedVoices.end());

    for (const QString& fullPath : sortedVoices) {
        QVariantMap choice;
        QString fileName = QFileInfo(fullPath).fileName();
        QString displayName = fileName;
        displayName.remove(QStringLiteral("kokoro-voice-"));
        displayName.remove(QStringLiteral(".gguf"));
        displayName.remove(QStringLiteral(".bin"));
        
        choice["text"] = kokoroVoiceDisplayName(displayName);
        choice["value"] = fullPath;
        choice["detail"] = kokoroVoiceReadableLabel(displayName) + QStringLiteral(" · ") + kokoroVoiceDetail(displayName) + QStringLiteral(" · ") + kokoroVoiceCodeLegend();
        voiceChoices.append(choice);
    }

    if (!voiceChoices.isEmpty()) {
        QVariantMap voiceParam;
        voiceParam["id"] = "voice";
        voiceParam["name"] = "Voice";
        voiceParam["type"] = "choice";
        voiceParam["choices"] = voiceChoices;
        voiceParam["default"] = m_currentVoicePath;
        voiceParam["description"] = "Select a voice pack for Kokoro.";
        schema.append(voiceParam);
    }

    Logger::info("KokoroBackend", QString("Kokoro schema built with %1 parameters").arg(schema.size()));
    return true;
}

void KokoroBackend::unload()
{
    if (m_session) {
        auto& ki = CrispKokoroInterface::instance();
        if (ki.isLoaded()) {
            ki.closeKokoroSession(m_session);
        }
        m_session = nullptr;
    }
    m_currentVoicePath.clear();
    m_modelPath.clear();
}

void KokoroBackend::cancelProcessing()
{
    m_cancelRequested = true;
}

bool KokoroBackend::synthesize(const QString &text, float speed, const QVariantMap &settings, 
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    m_cancelRequested = false;
    auto& ki = CrispKokoroInterface::instance();
    if (!ki.isLoaded() || !m_session) {
        error = QStringLiteral("CrispASR Kokoro runtime was unloaded unexpectedly.");
        return false;
    }

    // Apply voice if changed in settings
    const QString voice = QDir::toNativeSeparators(PathUtils::toNativeShortPath(settings.value(QStringLiteral("voice")).toString()));
    if (!voice.isEmpty() && voice != m_currentVoicePath) {
        if (ki.loadVoicePack(m_session, voice) == 0) {
            Logger::info("KokoroBackend", "Switched Kokoro voice to: " + voice);
            m_currentVoicePath = voice;
        } else {
            error = QStringLiteral("Failed to switch Kokoro voice to: ") + voice;
            Logger::error("KokoroBackend", error);
            return false;
        }
    }

    const float lengthScale = settings.contains(QStringLiteral("length_scale"))
        ? settings.value(QStringLiteral("length_scale")).toFloat()
        : (speed > 0.0f ? (1.0f / speed) : 1.0f);
    if (ki.setLengthScale(m_session, lengthScale) != 0) {
        error = QStringLiteral("Failed to apply Kokoro length_scale=%1.").arg(lengthScale);
        Logger::error("KokoroBackend", error);
        return false;
    }
    if (m_cancelRequested.load()) {
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }

    QString kokoroText = normalizeKokoroText(text);
    QByteArray textBytes = kokoroText.toUtf8();
    int n = 0;
    float *pcm = ki.synthesize(m_session, textBytes.constData(), &n);
    if (m_cancelRequested.load()) {
        if (pcm) ki.freePcm(pcm);
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }
    if (!pcm || n <= 0) {
        if (pcm) ki.freePcm(pcm);
        error = QStringLiteral("Kokoro synthesis returned empty audio.");
        return false;
    }

    samples.resize(n);
    memcpy(samples.data(), pcm, sizeof(float) * n);
    ki.freePcm(pcm);
    
    sampleRate = 24000;
    return true;
}

bool KokoroBackend::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(text);
    Q_UNUSED(referencePath);
    Q_UNUSED(settings);
    Q_UNUSED(samples);
    Q_UNUSED(sampleRate);
    error = QStringLiteral("Kokoro uses preset voice packs and does not support reference-audio voice cloning.");
    return false;
}

} // namespace LAStudio
