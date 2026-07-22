#include "controllers/dubbing/DubbingTranscriptionJob.h"

#include "SttSessionController.h"
#include "core/Logger.h"
#include "dubbing/AlignmentRefinementService.h"
#include "dubbing/DubbingSegmentNormalizer.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>

namespace LAStudio {

DubbingTranscriptionJob::DubbingTranscriptionJob(SttSessionController *stt,
                                                 ModelManager *models,
                                                 RuntimeManager *runtimes,
                                                 QObject *parent)
    : QObject(parent), m_stt(stt), m_models(models), m_runtimes(runtimes)
{
    m_alignmentWatcher = new QFutureWatcher<QVariantMap>(this);
    connect(m_alignmentWatcher, &QFutureWatcher<QVariantMap>::finished,
            this, &DubbingTranscriptionJob::onAlignmentFinished);
    if (!m_stt) return;
    connect(m_stt, &SttSessionController::transcriptionFinished,
            this, &DubbingTranscriptionJob::onTranscriptionFinished);
    connect(m_stt, &SttSessionController::transcriptionFailed, this,
            [this](const QString &message) {
        if (m_running) fail(message);
    });
    connect(m_stt, &SttSessionController::progressChanged, this, [this]() {
        if (!m_running || m_waitingForInput) return;
        emit progressChanged(qBound(5, m_stt->progress(), 99));
    });
    connect(m_stt, &SttSessionController::inputErrorChanged, this, [this]() {
        if (m_running && !m_stt->inputError().isEmpty()) fail(m_stt->inputError());
    });
    connect(m_stt, &SttSessionController::inputLoadingChanged, this, [this]() {
        if (!m_running || !m_waitingForInput || m_stt->inputLoading()) return;
        if (!m_stt->inputError().isEmpty()) return;
        m_waitingForInput = false;
        emit progressChanged(5);
        m_stt->transcribeInput();
    });
}

DubbingTranscriptionJob::~DubbingTranscriptionJob()
{
    cancel();
    if (m_alignmentWatcher) {
        m_alignmentWatcher->cancel();
        m_alignmentWatcher->waitForFinished();
    }
}

bool DubbingTranscriptionJob::start(const QString &language, const QString &audioPath,
                                    const QString &fallbackAudioPath)
{
    if (m_running || (m_stt && m_stt->processing())) {
        fail(QStringLiteral("Speech transcription is already running."));
        return false;
    }
    if (!m_stt || audioPath.isEmpty()) {
        fail(QStringLiteral("Import media before starting transcription."));
        return false;
    }
    if (!m_stt->canTranscribe()) {
        fail(QStringLiteral("The STT model is not ready. Wait for model loading to finish and try again."));
        return false;
    }
    ++m_generation;
    m_running = true;
    m_audioPath = audioPath;
    m_fallbackAudioPath = fallbackAudioPath;
    m_retriedWithFallback = false;
    m_language = language;
    m_stt->setLanguage(language);
    m_stt->setTranslate(false);
    startAudioInput(audioPath);
    return true;
}

void DubbingTranscriptionJob::startAudioInput(const QString &audioPath)
{
    m_audioPath = audioPath;
    m_stt->selectFileInput(audioPath);
    m_waitingForInput = true;
    if (!m_stt->inputLoading() && m_stt->inputError().isEmpty()) {
        m_waitingForInput = false;
        emit progressChanged(5);
        m_stt->transcribeInput();
    }
}

void DubbingTranscriptionJob::cancel()
{
    if (!m_running) return;
    ++m_generation;
    if (m_stt && m_stt->processing()) m_stt->cancelProcessing();
    if (m_alignmentCancel) m_alignmentCancel->storeRelease(true);
    m_waitingForInput = false;
    m_running = false;
}

void DubbingTranscriptionJob::onTranscriptionFinished(const QString &text,
                                                      const QVariantList &segments)
{
    if (!m_running) return;
    QVariantList normalizedInput;
    for (const QVariant &entry : segments) {
        const QVariantMap source = entry.toMap();
        QVariantMap normalized{
            {QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {QStringLiteral("startMs"), qRound64(source.value(QStringLiteral("start")).toDouble() * 1000.0)},
            {QStringLiteral("endMs"), qRound64(source.value(QStringLiteral("end")).toDouble() * 1000.0)},
            {QStringLiteral("sourceText"), source.value(QStringLiteral("text")).toString().trimmed()},
            {QStringLiteral("targetText"), QString()},
            {QStringLiteral("speakerId"), QStringLiteral("speaker-1")},
            {QStringLiteral("timingSource"), QStringLiteral("asr")},
            {QStringLiteral("alignmentStatus"), QStringLiteral("pending")},
            {QStringLiteral("state"), QStringLiteral("transcribed")}};
        QVariantList words;
        for (const QVariant &wordEntry : source.value(QStringLiteral("words")).toList()) {
            const QVariantMap word = wordEntry.toMap();
            const QString wordText = word.value(QStringLiteral("text"),
                                                word.value(QStringLiteral("word"))).toString().trimmed();
            const qint64 startMs = qRound64(word.value(QStringLiteral("start")).toDouble() * 1000.0);
            const qint64 endMs = qRound64(word.value(QStringLiteral("end")).toDouble() * 1000.0);
            if (wordText.isEmpty() || endMs <= startMs) continue;
            words.append(QVariantMap{{QStringLiteral("text"), wordText},
                                     {QStringLiteral("startMs"), startMs},
                                     {QStringLiteral("endMs"), endMs},
                                     {QStringLiteral("confidence"),
                                      word.value(QStringLiteral("confidence"))}});
        }
        if (!words.isEmpty()) normalized.insert(QStringLiteral("words"), words);
        normalizedInput.append(normalized);
    }
    if (normalizedInput.isEmpty()) {
        const bool canRetry = !m_retriedWithFallback
            && !m_fallbackAudioPath.trimmed().isEmpty()
            && QFileInfo(m_fallbackAudioPath).exists()
            && QFileInfo(m_fallbackAudioPath).absoluteFilePath()
                != QFileInfo(m_audioPath).absoluteFilePath();
        if (canRetry) {
            m_retriedWithFallback = true;
            Logger::warning(QStringLiteral("DubbingPipeline"),
                            QStringLiteral("[transcription] no speech found in separated vocals; retrying normalized audio=%1")
                                .arg(m_fallbackAudioPath));
            emit progressChanged(5);
            startAudioInput(m_fallbackAudioPath);
            return;
        }
        fail(text.trimmed().isEmpty()
                 ? QStringLiteral("Whisper found no speech in either the vocals stem or normalized audio.")
                 : QStringLiteral("Whisper returned text without usable timestamp segments."));
        return;
    }
    beginAlignment(normalizedInput);
}

void DubbingTranscriptionJob::beginAlignment(const QVariantList &segments)
{
    if (!m_alignmentWatcher) {
        m_running = false;
        emit completed(DubbingSegmentNormalizer::normalize(segments));
        return;
    }
    const quint64 generation = m_generation;
    const QString effectiveLanguage = qEnvironmentVariable(
        "LASTUDIO_DUBBING_LANGUAGE", m_language.isEmpty() ? QStringLiteral("en") : m_language);
    const QString preset = qEnvironmentVariable("LASTUDIO_DUBBING_ALIGNMENT_PRESET",
                                                QStringLiteral("balanced"));
    const AlignmentRefinementConfiguration configuration =
        AlignmentRefinementService::resolveConfiguration(m_models, m_runtimes);
    m_alignmentCancel = std::make_shared<QAtomicInteger<bool>>(false);
    const auto cancel = m_alignmentCancel;
    const QString path = m_audioPath;
    m_alignmentWatcher->setFuture(QtConcurrent::run(
        [path, effectiveLanguage, segments, preset, configuration, cancel, generation]() {
        const AlignmentRefinementResult refined = AlignmentRefinementService::refine(
            path, effectiveLanguage, segments, configuration, preset, cancel.get());
        return QVariantMap{{QStringLiteral("generation"), QVariant::fromValue(generation)},
                           {QStringLiteral("segments"), refined.segments},
                           {QStringLiteral("status"), refined.status},
                           {QStringLiteral("diagnostic"), refined.diagnostic}};
    }));
}

void DubbingTranscriptionJob::onAlignmentFinished()
{
    const QVariantMap result = m_alignmentWatcher->result();
    if (!m_running || result.value(QStringLiteral("generation")).toULongLong() != m_generation) return;
    const QVariantList segments = DubbingSegmentNormalizer::normalize(
        result.value(QStringLiteral("segments")).toList());
    if (segments.isEmpty()) {
        fail(QStringLiteral("Forced alignment returned no transcript segments."));
        return;
    }
    m_running = false;
    m_alignmentCancel.reset();
    emit progressChanged(100);
    emit completed(segments);
}

void DubbingTranscriptionJob::fail(const QString &message)
{
    if (!m_running && message.isEmpty()) return;
    m_running = false;
    m_waitingForInput = false;
    ++m_generation;
    emit failed(message);
}

} // namespace LAStudio
