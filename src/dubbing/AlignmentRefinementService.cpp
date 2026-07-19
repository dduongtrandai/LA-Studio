#include "dubbing/AlignmentRefinementService.h"

#include "audio/WavIO.h"
#include "core/ModelManager.h"
#include "core/PathUtils.h"
#include "core/RuntimeManager.h"
#include "core/Logger.h"
#include "runtimes/CrispAlignmentInterface.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QRegularExpression>
#include <QProcess>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QtMath>
#include <algorithm>

namespace LAStudio {

namespace {

struct Candidate
{
    QString modelPath;
    QString modelId;
    QString runtimePath;
    QString runtimeId;
    QString runtimeVersion;
    QString runtimeKind;
    QString runtimeExecutable;

    bool valid() const
    {
        if (modelPath.isEmpty()) return false;
        if (runtimeKind == QStringLiteral("process")) return !runtimeExecutable.isEmpty();
        return !runtimePath.isEmpty();
    }
};

bool cancelled(QAtomicInteger<bool> *flag)
{
    return flag && flag->loadAcquire();
}

QString normalizedText(const QString &text)
{
    QString result = text.toLower().normalized(QString::NormalizationForm_KC);
    result.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral(" "));
    return result.simplified();
}

QStringList tokens(const QString &text, const QString &language)
{
    const QString normalized = normalizedText(text);
    const QString lang = language.left(3).toLower();
    const bool cjk = lang == QStringLiteral("cmn") || lang == QStringLiteral("zho")
        || lang == QStringLiteral("jpn") || lang == QStringLiteral("kor");
    if (!cjk) return normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    QStringList result;
    for (const QChar ch : normalized) {
        if (!ch.isSpace()) result.append(QString(ch));
    }
    return result;
}

double tokenCoverage(const QString &source, const QStringList &actual, const QString &language)
{
    const QStringList expected = tokens(source, language);
    if (expected.isEmpty() || actual.isEmpty()) return 0.0;

    // Monotonic greedy matching is intentionally conservative: the aligner
    // may normalize punctuation, but it must not silently reorder content.
    int matched = 0;
    int cursor = 0;
    for (const QString &wanted : expected) {
        while (cursor < actual.size() && actual.at(cursor) != wanted) ++cursor;
        if (cursor >= actual.size()) break;
        ++matched;
        ++cursor;
    }
    return double(matched) / double(expected.size());
}

double tokenCoverage(const QString &source, const QVector<CrispAlignmentInterface::Word> &words,
                     const QString &language)
{
    QStringList actual;
    for (const auto &word : words) actual.append(tokens(word.text, language));
    return tokenCoverage(source, actual, language);
}

bool isAlignmentRuntime(const QVariantMap &runtime)
{
    const QString engine = runtime.value(QStringLiteral("engineFamily")).toString();
    const QString library = runtime.value(QStringLiteral("libraryPath")).toString();
    const QStringList capabilities = runtime.value(QStringLiteral("capabilities")).toStringList();
    return engine.compare(QStringLiteral("crispasr"), Qt::CaseInsensitive) == 0
        && QFileInfo(library).isFile()
        && (capabilities.isEmpty() || capabilities.contains(QStringLiteral("forced-alignment")));
}

QString alignmentModelArtifact(const QString &path);

Candidate findCandidate(ModelManager *models, RuntimeManager *runtimes)
{
    Candidate candidate;
    if (!models || !runtimes) return candidate;

    QVariantList runtimesList = runtimes->allRuntimes();
    std::stable_sort(runtimesList.begin(), runtimesList.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap left = a.toMap(), right = b.toMap();
        const bool leftProcess = left.value(QStringLiteral("kind")).toString() == QStringLiteral("process");
        const bool rightProcess = right.value(QStringLiteral("kind")).toString() == QStringLiteral("process");
        if (leftProcess != rightProcess) return leftProcess;
        const bool leftCpu = left.value(QStringLiteral("variant")).toString().contains(QStringLiteral("cpu"), Qt::CaseInsensitive);
        const bool rightCpu = right.value(QStringLiteral("variant")).toString().contains(QStringLiteral("cpu"), Qt::CaseInsensitive);
        return !leftCpu && rightCpu;
    });
    struct ScoredModel { QVariantMap map; int score = -1; };
    QVector<ScoredModel> choices;
    QVariantList alignmentModels = models->modelsForTask(QStringLiteral("forced-alignment"));
    for (const QVariant &entry : alignmentModels) {
        const QVariantMap model = entry.toMap();
        const QString id = model.value(QStringLiteral("id")).toString().toLower();
        const QString path = model.value(QStringLiteral("path")).toString();
        if (!QFileInfo(path).exists()) continue;
        int score = -1;
        if (id.contains(QStringLiteral("qwen3-forced"))) score = 100;
        else if (id.contains(QStringLiteral("forced-align"))) score = 95;
        else if (id.contains(QStringLiteral("canary-ctc"))) score = 90;
        else if (id.contains(QStringLiteral("wav2vec2-align"))) score = 80;
        if (score < 0 && id.contains(QStringLiteral("mms"))) score = 70;
        if (score >= 0) choices.append({model, score});
    }
    std::stable_sort(choices.begin(), choices.end(), [](const ScoredModel &a, const ScoredModel &b) {
        return a.score > b.score;
    });
    if (choices.isEmpty()) return candidate;

    const QString modelDirOrFile = choices.constFirst().map.value(QStringLiteral("path")).toString();
    const bool hasOnnx = QFileInfo(modelDirOrFile).isFile()
        ? QFileInfo(modelDirOrFile).suffix().compare(QStringLiteral("onnx"), Qt::CaseInsensitive) == 0
        : !QDirIterator(modelDirOrFile, {QStringLiteral("*.onnx")}, QDir::Files,
                        QDirIterator::Subdirectories).next().isEmpty();
    const QString directArtifact = alignmentModelArtifact(modelDirOrFile);

    auto applyRuntime = [&candidate](const QVariantMap &runtime) {
        candidate.runtimePath = runtime.value(QStringLiteral("libraryPath")).toString();
        candidate.runtimeId = runtime.value(QStringLiteral("id")).toString();
        candidate.runtimeVersion = runtime.value(QStringLiteral("version")).toString();
        candidate.runtimeKind = runtime.value(QStringLiteral("kind")).toString();
        candidate.runtimeExecutable = runtime.value(QStringLiteral("executablePath")).toString();
    };

    QVariantMap processRuntime;
    QVariantMap crispRuntime;
    for (const QVariant &entry : runtimesList) {
        const QVariantMap runtime = entry.toMap();
        const QStringList capabilities = runtime.value(QStringLiteral("capabilities")).toStringList();
        const bool processAlignment = runtime.value(QStringLiteral("kind")).toString() == QStringLiteral("process")
            && runtime.value(QStringLiteral("type")).toString().compare(QStringLiteral("alignment"), Qt::CaseInsensitive) == 0
            && (capabilities.isEmpty() || capabilities.contains(QStringLiteral("forced-alignment")))
            && QFileInfo(runtime.value(QStringLiteral("executablePath")).toString()).isFile();
        if (processRuntime.isEmpty() && processAlignment) processRuntime = runtime;
        if (crispRuntime.isEmpty() && isAlignmentRuntime(runtime)) crispRuntime = runtime;
    }
    if (hasOnnx && !processRuntime.isEmpty()) {
        applyRuntime(processRuntime);
        candidate.modelPath = modelDirOrFile;
    } else if (!directArtifact.isEmpty() && !crispRuntime.isEmpty()) {
        applyRuntime(crispRuntime);
        candidate.modelPath = directArtifact;
    } else {
        return Candidate{};
    }
    candidate.modelId = choices.constFirst().map.value(QStringLiteral("id")).toString();
    return candidate;
}

QString cacheKey(const QString &audioPath, const QString &language, const QVariantList &segments,
                 const Candidate &candidate)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QFile audio(audioPath);
    if (audio.open(QIODevice::ReadOnly)) {
        while (!audio.atEnd()) hash.addData(audio.read(1024 * 1024));
    } else {
        hash.addData(audioPath.toUtf8());
    }
    hash.addData(language.toUtf8());
    hash.addData(candidate.modelId.toUtf8());
    hash.addData(candidate.runtimeId.toUtf8());
    hash.addData(candidate.runtimeVersion.toUtf8());
    hash.addData(QJsonDocument::fromVariant(segments).toJson(QJsonDocument::Compact));
    return QString::fromLatin1(hash.result().toHex());
}

QString cachePath(const QString &key)
{
    const QString root = PathUtils::cacheDir() + QStringLiteral("/dubbing/alignment");
    QDir().mkpath(root);
    return root + QLatin1Char('/') + key + QStringLiteral(".json");
}

QString alignmentModelArtifact(const QString &path)
{
    if (QFileInfo(path).isFile()) return path;
    if (!QFileInfo(path).isDir()) return {};
    QDirIterator it(path, {QStringLiteral("*.gguf"), QStringLiteral("*.bin")},
                    QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

QVariantList markSkipped(const QVariantList &segments, const QString &diagnostic)
{
    QVariantList result;
    result.reserve(segments.size());
    for (const QVariant &entry : segments) {
        QVariantMap segment = entry.toMap();
        segment.insert(QStringLiteral("timingSource"), QStringLiteral("asr"));
        segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("skipped"));
        if (!diagnostic.isEmpty()) segment.insert(QStringLiteral("alignmentDiagnostic"), diagnostic);
        result.append(segment);
    }
    return result;
}

bool loadCache(const QString &path, AlignmentRefinementResult &result)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    const QJsonObject object = document.object();
    if (!object.value(QStringLiteral("accepted")).toBool()) return false;
    result.segments = object.value(QStringLiteral("segments")).toArray().toVariantList();
    result.attempted = true;
    result.changed = true;
    result.fromCache = true;
    result.status = QStringLiteral("aligned");
    result.diagnostic = QStringLiteral("Loaded forced-alignment result from cache.");
    return !result.segments.isEmpty();
}

void saveCache(const QString &path, const QVariantList &segments)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    const QJsonObject object{{QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("accepted"), true},
                             {QStringLiteral("segments"), QJsonArray::fromVariantList(segments)}};
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.commit();
}

bool runProcessAlignment(const Candidate &candidate, const QString &audioPath,
                         const QString &transcript, const QString &language,
                         QVariantList &segments, QString &error,
                         QAtomicInteger<bool> *cancel)
{
    if (candidate.runtimeKind != QStringLiteral("process")
        || !QFileInfo(candidate.runtimeExecutable).isFile()
        || !QFileInfo(candidate.modelPath).isDir()) {
        error = QStringLiteral("MMS alignment runtime or model directory is unavailable.");
        return false;
    }

    const QJsonObject request{
        {QStringLiteral("operation"), QStringLiteral("align")},
        {QStringLiteral("audio"), QDir::fromNativeSeparators(audioPath)},
        {QStringLiteral("transcript"), transcript},
        {QStringLiteral("language"), language.trimmed().isEmpty() ? QStringLiteral("en") : language.trimmed()},
        {QStringLiteral("model_dir"), QDir::fromNativeSeparators(candidate.modelPath)},
        {QStringLiteral("timestamp_unit"), QStringLiteral("word")},
        {QStringLiteral("output_format"), QStringLiteral("json")},
        {QStringLiteral("normalize_transcript"), true}
    };

    QProcess process;
    process.setProgram(candidate.runtimeExecutable);
    process.setWorkingDirectory(QFileInfo(candidate.runtimeExecutable).absolutePath());
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(10000)) {
        error = QStringLiteral("MMS alignment runtime could not be started.");
        return false;
    }
    process.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
    process.write("\n");
    process.closeWriteChannel();
    QElapsedTimer timer;
    timer.start();
    while (!process.waitForFinished(100)) {
        if (cancelled(cancel)) {
            process.kill();
            process.waitForFinished(1000);
            error = QStringLiteral("MMS alignment was cancelled.");
            return false;
        }
        if (timer.elapsed() >= 300000) {
            process.kill();
            process.waitForFinished(1000);
            error = QStringLiteral("MMS alignment runtime timed out.");
            return false;
        }
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("MMS alignment runtime returned an invalid response.");
        return false;
    }
    const QJsonObject response = document.object();
    if (!response.value(QStringLiteral("ok")).toBool()) {
        const QJsonObject runtimeError = response.value(QStringLiteral("error")).toObject();
        error = runtimeError.value(QStringLiteral("message")).toString(
            QStringLiteral("MMS alignment runtime failed."));
        return false;
    }
    segments = response.value(QStringLiteral("segments")).toArray().toVariantList();
    if (segments.isEmpty()) {
        error = QStringLiteral("MMS alignment runtime returned no segments.");
        return false;
    }
    return true;
}

QVariantList normalizeProcessSegments(const QVariantList &sourceSegments,
                                       const QVariantList &alignedSegments,
                                       const QString &modelId,
                                       const QString &runtimeId,
                                       const QString &language,
                                       QString &diagnostic)
{
    if (sourceSegments.size() != alignedSegments.size()) {
        diagnostic = QStringLiteral("MMS alignment returned %1 segments for %2 transcript segments.")
            .arg(alignedSegments.size()).arg(sourceSegments.size());
        return {};
    }

    QVariantList result;
    result.reserve(sourceSegments.size());
    double previousSegmentEnd = -1.0;
    for (int i = 0; i < sourceSegments.size(); ++i) {
        QVariantMap segment = sourceSegments.at(i).toMap();
        const QVariantMap aligned = alignedSegments.at(i).toMap();
        const QVariantList rawWords = aligned.value(QStringLiteral("words")).toList();
        QVariantList words;
        double first = aligned.value(QStringLiteral("start")).toDouble();
        double last = aligned.value(QStringLiteral("end")).toDouble();
        for (const QVariant &raw : rawWords) {
            const QVariantMap word = raw.toMap();
            const QString text = word.value(QStringLiteral("text"),
                                            word.value(QStringLiteral("word"))).toString().trimmed();
            const double start = word.value(QStringLiteral("start"),
                                            word.value(QStringLiteral("startSec"))).toDouble();
            const double end = word.value(QStringLiteral("end"),
                                          word.value(QStringLiteral("endSec"))).toDouble();
            if (text.isEmpty() || end <= start || start < 0.0 || start < last && !words.isEmpty()) continue;
            if (words.isEmpty()) first = start;
            last = end;
            words.append(QVariantMap{{QStringLiteral("text"), text},
                                     {QStringLiteral("startMs"), qRound64(start * 1000.0)},
                                     {QStringLiteral("endMs"), qRound64(end * 1000.0)},
                                     {QStringLiteral("timestampSource"), QStringLiteral("mms-ctc")}});
        }
        QStringList actualText;
        for (const QVariant &word : words) actualText.append(word.toMap().value(QStringLiteral("text")).toString());
        const double coverage = tokenCoverage(
            segment.value(QStringLiteral("sourceText")).toString(), actualText, language);
        if (words.isEmpty() || last <= first || coverage < 0.75
            || (previousSegmentEnd >= 0.0 && first < previousSegmentEnd - 0.02)) {
            diagnostic = QStringLiteral("MMS alignment returned invalid word timing for segment %1.").arg(i);
            return {};
        }
        previousSegmentEnd = last;
        segment.insert(QStringLiteral("startMs"), qRound64(first * 1000.0));
        segment.insert(QStringLiteral("endMs"), qRound64(last * 1000.0));
        segment.insert(QStringLiteral("words"), words);
        segment.insert(QStringLiteral("timingSource"), QStringLiteral("mms-ctc"));
        segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("aligned"));
        segment.insert(QStringLiteral("alignmentCoverage"), coverage);
        segment.insert(QStringLiteral("alignmentMatchScore"), aligned.value(QStringLiteral("matchScore"), coverage));
        segment.insert(QStringLiteral("alignmentModel"), modelId);
        segment.insert(QStringLiteral("alignmentRuntime"), runtimeId);
        result.append(segment);
    }
    return result;
}

} // namespace

AlignmentRefinementConfiguration AlignmentRefinementService::resolveConfiguration(
    ModelManager *models, RuntimeManager *runtimes)
{
    const Candidate candidate = findCandidate(models, runtimes);
    AlignmentRefinementConfiguration result;
    result.modelPath = candidate.modelPath;
    result.modelId = candidate.modelId;
    result.runtimePath = candidate.runtimePath;
    result.runtimeId = candidate.runtimeId;
    result.runtimeVersion = candidate.runtimeVersion;
    result.runtimeKind = candidate.runtimeKind;
    result.runtimeExecutable = candidate.runtimeExecutable;
    return result;
}

AlignmentRefinementResult AlignmentRefinementService::refine(
    const QString &audioPath, const QString &language, const QVariantList &segments,
    ModelManager *models, RuntimeManager *runtimes, const QString &preset,
    QAtomicInteger<bool> *cancel)
{
    return refine(audioPath, language, segments,
                  resolveConfiguration(models, runtimes), preset, cancel);
}

AlignmentRefinementResult AlignmentRefinementService::refine(
    const QString &audioPath, const QString &language, const QVariantList &segments,
    const AlignmentRefinementConfiguration &configuration, const QString &preset,
    QAtomicInteger<bool> *cancel)
{
    AlignmentRefinementResult result;
    result.segments = segments;
    if (preset.compare(QStringLiteral("fast"), Qt::CaseInsensitive) == 0) {
        result.status = QStringLiteral("skipped");
        result.diagnostic = QStringLiteral("Forced alignment skipped by Fast preset.");
        result.segments = markSkipped(segments, result.diagnostic);
        return result;
    }
    if (cancelled(cancel)) {
        result.status = QStringLiteral("cancelled");
        return result;
    }

    Candidate candidate;
    candidate.modelPath = configuration.modelPath;
    candidate.modelId = configuration.modelId;
    candidate.runtimePath = configuration.runtimePath;
    candidate.runtimeId = configuration.runtimeId;
    candidate.runtimeVersion = configuration.runtimeVersion;
    candidate.runtimeKind = configuration.runtimeKind;
    candidate.runtimeExecutable = configuration.runtimeExecutable;
    if (!candidate.valid()) {
        result.status = QStringLiteral("skipped");
        result.diagnostic = QStringLiteral("No compatible CrispASR forced-alignment runtime/model is installed.");
        result.segments = markSkipped(segments, result.diagnostic);
        return result;
    }

    const QString key = cacheKey(audioPath, language, segments, candidate);
    if (loadCache(cachePath(key), result)) return result;

    if (candidate.runtimeKind == QStringLiteral("process")) {
        QStringList transcriptLines;
        transcriptLines.reserve(segments.size());
        for (const QVariant &entry : segments)
            transcriptLines.append(entry.toMap().value(QStringLiteral("sourceText")).toString().trimmed());
        QVariantList aligned;
        QString processError;
        result.attempted = true;
        if (!runProcessAlignment(candidate, audioPath, transcriptLines.join(QLatin1Char('\n')),
                                 language, aligned, processError, cancel)) {
            result.status = QStringLiteral("skipped");
            result.diagnostic = processError;
            result.segments = markSkipped(segments, processError);
            return result;
        }
        QString normalizationError;
        const QVariantList normalized = normalizeProcessSegments(
            segments, aligned, candidate.modelId, candidate.runtimeId, language, normalizationError);
        if (normalized.isEmpty()) {
            result.status = QStringLiteral("fallback");
            result.diagnostic = normalizationError;
            result.segments = markSkipped(segments, normalizationError);
            return result;
        }
        result.segments = normalized;
        result.changed = true;
        result.status = QStringLiteral("aligned");
        result.diagnostic = QStringLiteral("MMS process forced alignment completed.");
        saveCache(cachePath(key), normalized);
        return result;
    }

    const WavIO::WavData audio = WavIO::loadAsFloatMono16k(audioPath);
    if (audio.samples.isEmpty()) {
        result.status = QStringLiteral("skipped");
        result.diagnostic = QStringLiteral("Analysis audio could not be decoded for forced alignment.");
        result.segments = markSkipped(segments, result.diagnostic);
        return result;
    }

    CrispAlignmentInterface crisp;
    if (!crisp.load(candidate.runtimePath)) {
        result.status = QStringLiteral("skipped");
        result.diagnostic = crisp.errorString();
        result.segments = markSkipped(segments, result.diagnostic);
        return result;
    }

    result.attempted = true;
    result.status = QStringLiteral("fallback");
    QVariantList refined;
    refined.reserve(segments.size());
    int accepted = 0;
    int rejected = 0;
    const double duration = double(audio.samples.size()) / 16000.0;

    for (const QVariant &entry : segments) {
        if (cancelled(cancel)) {
            result.status = QStringLiteral("cancelled");
            result.diagnostic = QStringLiteral("Forced alignment was cancelled.");
            return result;
        }
        QVariantMap segment = entry.toMap();
        const QString text = segment.value(QStringLiteral("sourceText")).toString().trimmed();
        const double originalStart = segment.value(QStringLiteral("startMs")).toDouble() / 1000.0;
        const double originalEnd = segment.value(QStringLiteral("endMs")).toDouble() / 1000.0;
        if (text.isEmpty() || originalEnd <= originalStart) {
            segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("skipped"));
            segment.insert(QStringLiteral("timingSource"), QStringLiteral("asr"));
            refined.append(segment);
            ++rejected;
            continue;
        }

        const double windowStart = qMax(0.0, originalStart - 0.75);
        const double windowEnd = qMin(duration, originalEnd + 0.75);
        const int begin = qBound(0, qRound64(windowStart * 16000.0), audio.samples.size());
        const int end = qBound(begin, qRound64(windowEnd * 16000.0), audio.samples.size());
        const QVector<float> chunk = audio.samples.mid(begin, end - begin);
        const auto words = crisp.align(candidate.modelPath, text, chunk,
                                       qRound64(windowStart * 100.0), 4);
        const double coverage = tokenCoverage(text, words, language);
        bool valid = !words.isEmpty() && coverage >= 0.75;
        double first = 0.0, last = 0.0;
        if (valid) {
            first = words.constFirst().start;
            last = words.constLast().end;
            valid = first >= 0.0 && last > first && last <= duration + 0.25;
            for (int i = 1; valid && i < words.size(); ++i)
                valid = words.at(i - 1).end <= words.at(i).start + 0.02;
        }

        if (!valid) {
            segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("ambiguous"));
            segment.insert(QStringLiteral("timingSource"), QStringLiteral("asr"));
            segment.insert(QStringLiteral("alignmentCoverage"), coverage);
            segment.insert(QStringLiteral("alignmentDiagnostic"), QStringLiteral("CTC alignment rejected by coverage/timing gate."));
            refined.append(segment);
            ++rejected;
            continue;
        }

        QVariantList wordList;
        for (const auto &word : words) {
            wordList.append(QVariantMap{{QStringLiteral("text"), word.text},
                                        {QStringLiteral("startMs"), qRound64(word.start * 1000.0)},
                                        {QStringLiteral("endMs"), qRound64(word.end * 1000.0)},
                                        {QStringLiteral("timestampSource"), QStringLiteral("ctc")}});
        }
        segment.insert(QStringLiteral("startMs"), qRound64(first * 1000.0));
        segment.insert(QStringLiteral("endMs"), qRound64(last * 1000.0));
        segment.insert(QStringLiteral("words"), wordList);
        segment.insert(QStringLiteral("timingSource"), QStringLiteral("ctc"));
        segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("aligned"));
        segment.insert(QStringLiteral("alignmentCoverage"), coverage);
        segment.insert(QStringLiteral("alignmentMatchScore"), coverage);
        segment.insert(QStringLiteral("alignmentModel"), candidate.modelId);
        segment.insert(QStringLiteral("alignmentRuntime"), candidate.runtimeId);
        refined.append(segment);
        ++accepted;
    }

    result.segments = refined;
    result.changed = accepted > 0;
    result.status = accepted > 0 ? (rejected > 0 ? QStringLiteral("partial") : QStringLiteral("aligned"))
                                 : QStringLiteral("fallback");
    result.diagnostic = QStringLiteral("Forced alignment accepted %1/%2 segment(s); %3 rejected.")
        .arg(accepted).arg(segments.size()).arg(rejected);
    if (accepted > 0) saveCache(cachePath(key), refined);
    return result;
}

} // namespace LAStudio
