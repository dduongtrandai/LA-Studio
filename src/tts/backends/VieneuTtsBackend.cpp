#include "VieneuTtsBackend.h"
#include "audio/WavIO.h"
#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/VieneuTtsInterface.h>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <zlib.h>
#include <cmath>
#include <cstring>
#include <utility>

namespace LAStudio {

static QString s_sessionVieneuRuntimePath;

template<typename Audio>
static bool isValidRuntimeAudio(const Audio &audio)
{
    constexpr int maxReasonableSamples = 24000 * 60 * 30; // 30 minutes at 24 kHz.
    return audio.samples != nullptr &&
           audio.n_samples > 0 &&
           audio.n_samples <= maxReasonableSamples &&
           audio.sample_rate > 0 &&
           audio.sample_rate <= 384000;
}

static QVariantMap voiceChoice(const QString &name, const QString &detail)
{
    QVariantMap choice;
    choice[QStringLiteral("text")] = name;
    choice[QStringLiteral("value")] = name;
    choice[QStringLiteral("detail")] = detail;
    return choice;
}

static QVariantList vieneuV3BuiltInVoices()
{
    return {
        voiceChoice(QStringLiteral("Ng\u1ecdc Lan"), QStringLiteral("Female, soft / gentle")),
        voiceChoice(QStringLiteral("Ng\u1ecdc Linh"), QStringLiteral("Female, bright")),
        voiceChoice(QStringLiteral("Tr\u00fac Ly"), QStringLiteral("Female, youthful")),
        voiceChoice(QStringLiteral("M\u1ef9 Duy\u00ean"), QStringLiteral("Female, smooth")),
        voiceChoice(QStringLiteral("Xu\u00e2n V\u0129nh"), QStringLiteral("Male, upbeat")),
        voiceChoice(QStringLiteral("Th\u00e1i S\u01a1n"), QStringLiteral("Male, firm")),
        voiceChoice(QStringLiteral("Gia B\u1ea3o"), QStringLiteral("Male, smooth")),
        voiceChoice(QStringLiteral("\u0110\u1ee9c Tr\u00ed"), QStringLiteral("Male, clear")),
        voiceChoice(QStringLiteral("Tr\u1ecdng H\u1eefu"), QStringLiteral("Male, knowledgeable")),
        voiceChoice(QStringLiteral("B\u00ecnh An"), QStringLiteral("Male, even / calm"))
    };
}

static QVariantMap vieneuV3Preset(const QString &name, int reservedId)
{
    QVariantMap preset;
    preset.insert(QStringLiteral("reserved_id"), reservedId);
    preset.insert(QStringLiteral("description"), name);
    return preset;
}

static QString copyBundledVieneuV3VoicesJson(const QString &modelDir)
{
    if (modelDir.isEmpty()) {
        return QString();
    }

    const QString targetPath = QDir(modelDir).absoluteFilePath(QStringLiteral("voices_v3_turbo.json"));
    if (QFileInfo::exists(targetPath)) {
        return targetPath;
    }

    const QStringList resourcePaths = {
        QStringLiteral(":/LAStudio/data/vieneu/voices_v3_turbo.json"),
        QStringLiteral(":/data/vieneu/voices_v3_turbo.json")
    };
    QString sourcePath;
    for (const QString &candidate : resourcePaths) {
        if (QFileInfo::exists(candidate)) {
            sourcePath = candidate;
            break;
        }
    }
    if (sourcePath.isEmpty()) {
        return QString();
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Logger::warning(QStringLiteral("VieneuTtsBackend"),
                        QStringLiteral("Could not copy bundled VieNeu v3 voices JSON to: %1").arg(targetPath));
        return QString();
    }
    target.write(source.readAll());
    Logger::info(QStringLiteral("VieneuTtsBackend"),
                 QStringLiteral("Copied bundled VieNeu v3 voices JSON to: %1").arg(targetPath));
    return targetPath;
}

static QString writeVieneuV3FallbackVoicesJson(const QString &modelDir)
{
    if (modelDir.isEmpty()) {
        return QString();
    }

    const QString fallbackPath = QDir(modelDir).absoluteFilePath(QStringLiteral(".lastudio_vieneu_v3_voices.json"));

    QJsonObject presets;
    presets.insert(QStringLiteral("Ng\u1ecdc Lan"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("Ng\u1ecdc Lan"), 13)));
    presets.insert(QStringLiteral("Gia B\u1ea3o"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("Gia B\u1ea3o"), 16)));
    presets.insert(QStringLiteral("Th\u00e1i S\u01a1n"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("Th\u00e1i S\u01a1n"), 17)));
    presets.insert(QStringLiteral("\u0110\u1ee9c Tr\u00ed"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("\u0110\u1ee9c Tr\u00ed"), 21)));
    presets.insert(QStringLiteral("M\u1ef9 Duy\u00ean"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("M\u1ef9 Duy\u00ean"), 22)));
    presets.insert(QStringLiteral("Tr\u00fac Ly"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("Tr\u00fac Ly"), 30)));
    presets.insert(QStringLiteral("Xu\u00e2n V\u0129nh"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("Xu\u00e2n V\u0129nh"), 32)));
    presets.insert(QStringLiteral("Tr\u1ecdng H\u1eefu"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("Tr\u1ecdng H\u1eefu"), 36)));
    presets.insert(QStringLiteral("B\u00ecnh An"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("B\u00ecnh An"), 37)));
    presets.insert(QStringLiteral("Ng\u1ecdc Linh"), QJsonObject::fromVariantMap(vieneuV3Preset(QStringLiteral("Ng\u1ecdc Linh"), 41)));

    QJsonObject meta;
    meta.insert(QStringLiteral("spec"), QStringLiteral("lastudio.vieneu-v3.fallback-voices"));
    meta.insert(QStringLiteral("spec_version"), 2);
    meta.insert(QStringLiteral("source"), QStringLiteral("VieNeu-TTS src/vieneu/assets/voices_v3_turbo.json reserved_id map"));

    QJsonObject root;
    root.insert(QStringLiteral("meta"), meta);
    root.insert(QStringLiteral("default_voice"), QStringLiteral("Ng\u1ecdc Linh"));
    root.insert(QStringLiteral("presets"), presets);

    QFile file(fallbackPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Logger::warning(QStringLiteral("VieneuTtsBackend"),
                        QStringLiteral("Could not write VieNeu v3 fallback voices JSON: %1").arg(fallbackPath));
        return QString();
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return fallbackPath;
}

static QString ensureSentenceTerminatorForVieneuV3(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return text;
    }
    const QChar last = text.back();
    if (last == QLatin1Char('.') ||
        last == QLatin1Char('!') ||
        last == QLatin1Char('?') ||
        last == QChar(0x2026)) {
        return text;
    }
    return text + QLatin1Char('.');
}

static int vieneuV3SafeChunkCharLimit(const QVariantMap &settings)
{
    const int requestedMaxChars = settings.value(QStringLiteral("max_chars"), 0).toInt();
    const int baseLimit = requestedMaxChars > 0 ? requestedMaxChars : 384;
    return qBound(120, baseLimit, 384);
}

static QStringList splitVieneuV3TextForSafeDecode(const QString &text, const QVariantMap &settings)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const int maxChars = vieneuV3SafeChunkCharLimit(settings);
    if (trimmed.size() <= maxChars) {
        return { trimmed };
    }

    QStringList chunks;
    const QRegularExpression sentencePattern(QStringLiteral("[^.!?\\x{2026}]+[.!?\\x{2026}]*"));
    auto it = sentencePattern.globalMatch(trimmed);
    while (it.hasNext()) {
        QString sentence = it.next().captured(0).trimmed();
        if (sentence.isEmpty()) {
            continue;
        }

        while (sentence.size() > maxChars) {
            int splitAt = -1;
            const int softLimit = qMin(maxChars, sentence.size());
            for (int i = softLimit - 1; i >= maxChars / 2; --i) {
                const QChar ch = sentence.at(i);
                if (ch.isSpace() || ch == QLatin1Char(',') || ch == QLatin1Char(';') || ch == QLatin1Char(':')) {
                    splitAt = i + 1;
                    break;
                }
            }
            if (splitAt <= 0) {
                splitAt = softLimit;
            }

            const QString piece = sentence.left(splitAt).trimmed();
            if (!piece.isEmpty()) {
                chunks.append(piece);
            }
            sentence = sentence.mid(splitAt).trimmed();
        }

        if (sentence.isEmpty()) {
            continue;
        }

        if (!chunks.isEmpty() && chunks.last().size() + 1 + sentence.size() <= maxChars) {
            chunks.last() += QLatin1Char(' ') + sentence;
        } else {
            chunks.append(sentence);
        }
    }

    if (chunks.isEmpty()) {
        chunks.append(trimmed);
    }
    return chunks;
}

static int adaptiveVieneuV3FrameCap(const QString &text, int requestedFrames)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return requestedFrames > 0 ? qBound(16, requestedFrames, 1200) : requestedFrames;
    }

    const QStringList words = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const int wordCount = qMax(1, words.size());
    const int charCount = trimmed.size();
    int sentenceBreaks = 0;
    for (const QChar ch : trimmed) {
        if (ch == QLatin1Char('.') ||
            ch == QLatin1Char('!') ||
            ch == QLatin1Char('?') ||
            ch == QChar(0x2026) ||
            ch == QLatin1Char(',') ||
            ch == QLatin1Char(';') ||
            ch == QLatin1Char(':')) {
            ++sentenceBreaks;
        }
    }

    const int estimatedCap = qBound(96, wordCount * 9 + charCount / 3 + sentenceBreaks * 8 + 24, 1200);
    if (requestedFrames > 0) {
        // The v3 native decoder allocates its decode state from this value.  A
        // fixed UI default (currently 300) is unnecessarily large for short
        // dubbing segments and has caused the native runtime to fail before it
        // can return an error. Keep the user's value as an upper bound while
        // sizing short requests from their actual text length.
        return qBound(16, qMin(requestedFrames, estimatedCap), 1200);
    }
    return estimatedCap;
}

static QString findVieneuSeaG2pDict(const QString &runtimePath,
                                    const QString &modelDir,
                                    const QString &configPath)
{
    QStringList baseDirs;
    if (!runtimePath.isEmpty()) {
        baseDirs.append(QFileInfo(runtimePath).absolutePath());
    }
    if (!modelDir.isEmpty()) {
        baseDirs.append(modelDir);
    }
    if (!configPath.isEmpty()) {
        baseDirs.append(QFileInfo(configPath).absolutePath());
    }
    baseDirs.append(QDir::currentPath());

    const QStringList relativePaths = {
        QStringLiteral("sea_g2p.bin"),
        QStringLiteral("assets/sea_g2p.bin"),
        QStringLiteral("sea-g2p/sea_g2p.bin")
    };

    QSet<QString> checked;
    for (const QString &baseDir : baseDirs) {
        if (baseDir.isEmpty()) {
            continue;
        }
        const QDir dir(baseDir);
        for (const QString &relativePath : relativePaths) {
            const QString candidate = QDir::cleanPath(dir.absoluteFilePath(relativePath));
            if (checked.contains(candidate)) {
                continue;
            }
            checked.insert(candidate);
            if (QFileInfo::exists(candidate)) {
                return PathUtils::toNativeShortPath(candidate);
            }
        }
    }
    return QString();
}

static void configureVieneuSeaG2pDict(const QString &runtimePath,
                                      const QString &modelDir,
                                      const QString &configPath)
{
    const QByteArray existing = qgetenv("VIENEU_SEA_G2P_DICT");
    if (!existing.isEmpty()) {
        const QString existingPath = QString::fromLocal8Bit(existing);
        if (QFileInfo::exists(existingPath)) {
            Logger::info(QStringLiteral("VieneuTtsBackend"),
                         QStringLiteral("Using existing VIENEU_SEA_G2P_DICT: %1").arg(existingPath));
            return;
        }
        Logger::warning(QStringLiteral("VieneuTtsBackend"),
                        QStringLiteral("VIENEU_SEA_G2P_DICT is set but does not exist: %1").arg(existingPath));
    }

    const QString dictPath = findVieneuSeaG2pDict(runtimePath, modelDir, configPath);
    if (dictPath.isEmpty()) {
        Logger::warning(QStringLiteral("VieneuTtsBackend"),
                        QStringLiteral("sea_g2p.bin was not found next to the VieNeu runtime or model; VieNeu runtime may fall back to its internal rule-based phonemizer."));
        return;
    }

    qputenv("VIENEU_SEA_G2P_DICT", QDir::toNativeSeparators(dictPath).toUtf8());
    Logger::info(QStringLiteral("VieneuTtsBackend"),
                 QStringLiteral("Configured VIENEU_SEA_G2P_DICT: %1").arg(dictPath));
}

static void configureVieneuV3NativeQualityEnv()
{
    qputenv("VIENEU_ACOUSTIC_DIRECT_LINEAR", "0");
    qputenv("VIENEU_ACOUSTIC_GGML_LINEAR", "1");
    qputenv("VIENEU_GGML_FUSE_FFN", "1");
    qputenv("VIENEU_ACOUSTIC_Q8_FFN", "0");
    qputenv("VIENEU_ACOUSTIC_GGML_HEADS", "1");
    Logger::info(QStringLiteral("VieneuTtsBackend"),
                 QStringLiteral("Configured VieNeu v3 native benchmark-parity mode: DIRECT_LINEAR=0, GGML_LINEAR=1, FUSE_FFN=1, Q8_FFN=0, GGML_HEADS=1, BENCHMARK=0."));
    qputenv("VIENEU_V3_NATIVE_BENCHMARK", "0");
}

static quint16 readLe16(const QByteArray &bytes, int offset)
{
    return static_cast<quint16>(static_cast<uchar>(bytes[offset])) |
           (static_cast<quint16>(static_cast<uchar>(bytes[offset + 1])) << 8);
}

static quint32 readLe32(const QByteArray &bytes, int offset)
{
    return static_cast<quint32>(static_cast<uchar>(bytes[offset])) |
           (static_cast<quint32>(static_cast<uchar>(bytes[offset + 1])) << 8) |
           (static_cast<quint32>(static_cast<uchar>(bytes[offset + 2])) << 16) |
           (static_cast<quint32>(static_cast<uchar>(bytes[offset + 3])) << 24);
}

static void appendLe16(QByteArray &bytes, quint16 value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
}

static void appendLe32(QByteArray &bytes, quint32 value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
    bytes.append(static_cast<char>((value >> 16) & 0xff));
    bytes.append(static_cast<char>((value >> 24) & 0xff));
}

static bool readAt(QFile &file, qint64 offset, qint64 length, QByteArray &out)
{
    if (offset < 0 || length < 0 || !file.seek(offset)) {
        return false;
    }
    out = file.read(length);
    return out.size() == length;
}

static bool validateZipContainer(const QString &path, QString &error);

static bool validateZipEntryPayload(QFile &file,
                                    qint64 dataOffset,
                                    quint32 compressedSize,
                                    quint32 uncompressedSize,
                                    quint32 expectedCrc,
                                    quint16 compressionMethod,
                                    const QString &entryName,
                                    QString &error)
{
    if (!file.seek(dataOffset)) {
        error = QStringLiteral("Could not seek to NPZ entry data.");
        return false;
    }

    constexpr qsizetype chunkSize = 64 * 1024;
    QByteArray input(chunkSize, Qt::Uninitialized);
    QByteArray output(chunkSize, Qt::Uninitialized);
    quint32 crc = static_cast<quint32>(crc32(0L, Z_NULL, 0));
    qint64 remaining = compressedSize;
    qint64 produced = 0;

    auto entryLabel = [&entryName]() {
        return entryName.isEmpty() ? QStringLiteral("<unnamed>") : entryName;
    };

    if (compressionMethod == 0) {
        while (remaining > 0) {
            const qsizetype toRead = static_cast<qsizetype>(qMin<qint64>(remaining, chunkSize));
            const qint64 read = file.read(input.data(), toRead);
            if (read != toRead) {
                error = QStringLiteral("NPZ entry %1 is truncated.").arg(entryLabel());
                return false;
            }
            crc = static_cast<quint32>(crc32(crc, reinterpret_cast<const Bytef *>(input.constData()), static_cast<uInt>(read)));
            produced += read;
            remaining -= read;
        }
    } else if (compressionMethod == 8) {
        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));
        int zret = inflateInit2(&stream, -MAX_WBITS);
        if (zret != Z_OK) {
            error = QStringLiteral("Could not initialize NPZ deflate validator.");
            return false;
        }

        bool ended = false;
        while (remaining > 0 && !ended) {
            const qsizetype toRead = static_cast<qsizetype>(qMin<qint64>(remaining, chunkSize));
            const qint64 read = file.read(input.data(), toRead);
            if (read != toRead) {
                inflateEnd(&stream);
                error = QStringLiteral("NPZ entry %1 is truncated.").arg(entryLabel());
                return false;
            }
            remaining -= read;
            stream.next_in = reinterpret_cast<Bytef *>(input.data());
            stream.avail_in = static_cast<uInt>(read);

            do {
                stream.next_out = reinterpret_cast<Bytef *>(output.data());
                stream.avail_out = static_cast<uInt>(output.size());
                zret = inflate(&stream, Z_NO_FLUSH);
                if (zret != Z_OK && zret != Z_STREAM_END) {
                    inflateEnd(&stream);
                    error = QStringLiteral("NPZ entry %1 cannot be decompressed; the file is likely incomplete or corrupted.").arg(entryLabel());
                    return false;
                }

                const qsizetype have = output.size() - static_cast<qsizetype>(stream.avail_out);
                if (have > 0) {
                    crc = static_cast<quint32>(crc32(crc, reinterpret_cast<const Bytef *>(output.constData()), static_cast<uInt>(have)));
                    produced += have;
                }
                if (zret == Z_STREAM_END) {
                    ended = true;
                    break;
                }
            } while (stream.avail_out == 0);
        }

        if (!ended) {
            inflateEnd(&stream);
            error = QStringLiteral("NPZ entry %1 ended before the deflate stream completed.").arg(entryLabel());
            return false;
        }
        if (stream.avail_in != 0 || remaining != 0) {
            inflateEnd(&stream);
            error = QStringLiteral("NPZ entry %1 has unexpected trailing compressed data.").arg(entryLabel());
            return false;
        }
        inflateEnd(&stream);
    } else {
        error = QStringLiteral("NPZ entry %1 uses unsupported compression method %2.")
                    .arg(entryLabel())
                    .arg(compressionMethod);
        return false;
    }

    if (produced != static_cast<qint64>(uncompressedSize)) {
        error = QStringLiteral("NPZ entry %1 has an invalid uncompressed size.").arg(entryLabel());
        return false;
    }
    if (crc != expectedCrc) {
        error = QStringLiteral("NPZ entry %1 failed CRC validation.").arg(entryLabel());
        return false;
    }

    return true;
}
static bool validateZipContainer(const QString &path, QString &error)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        error = QStringLiteral("File does not exist.");
        return false;
    }
    if (info.size() < 22) {
        error = QStringLiteral("File is too small to be a valid NPZ archive.");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Could not open file for validation.");
        return false;
    }

    const qint64 fileSize = info.size();
    const qint64 tailSize = qMin<qint64>(fileSize, 22 + 65535);
    QByteArray tail;
    if (!readAt(file, fileSize - tailSize, tailSize, tail)) {
        error = QStringLiteral("Could not read NPZ archive footer.");
        return false;
    }

    int eocdOffsetInTail = -1;
    for (int i = tail.size() - 22; i >= 0; --i) {
        if (readLe32(tail, i) == 0x06054b50u) {
            eocdOffsetInTail = i;
            break;
        }
    }

    if (eocdOffsetInTail < 0) {
        error = QStringLiteral("Missing ZIP end-of-directory footer; the download is likely incomplete.");
        return false;
    }

    const quint16 diskNumber = readLe16(tail, eocdOffsetInTail + 4);
    const quint16 centralDirectoryDisk = readLe16(tail, eocdOffsetInTail + 6);
    const quint16 entriesOnDisk = readLe16(tail, eocdOffsetInTail + 8);
    const quint16 totalEntries = readLe16(tail, eocdOffsetInTail + 10);
    const quint32 centralDirectorySize = readLe32(tail, eocdOffsetInTail + 12);
    const quint32 centralDirectoryOffset = readLe32(tail, eocdOffsetInTail + 16);

    if (diskNumber != 0 || centralDirectoryDisk != 0 || entriesOnDisk != totalEntries) {
        error = QStringLiteral("Multi-disk ZIP archives are not supported for NPZ model files.");
        return false;
    }
    if (totalEntries == 0) {
        error = QStringLiteral("NPZ archive contains no entries.");
        return false;
    }
    if (centralDirectoryOffset == 0xffffffffu || centralDirectorySize == 0xffffffffu) {
        error = QStringLiteral("ZIP64 NPZ archives are not supported by this validator.");
        return false;
    }

    const qint64 cdOffset = static_cast<qint64>(centralDirectoryOffset);
    const qint64 cdSize = static_cast<qint64>(centralDirectorySize);
    if (cdOffset < 0 || cdSize < 0 || cdOffset + cdSize > fileSize) {
        error = QStringLiteral("Central directory points past the end of the file.");
        return false;
    }

    QByteArray cd;
    if (!readAt(file, cdOffset, cdSize, cd)) {
        error = QStringLiteral("Could not read NPZ archive directory.");
        return false;
    }

    qint64 pos = 0;
    for (quint16 entry = 0; entry < totalEntries; ++entry) {
        if (pos + 46 > cd.size() || readLe32(cd, static_cast<int>(pos)) != 0x02014b50u) {
            error = QStringLiteral("Invalid ZIP central directory entry.");
            return false;
        }

        const quint16 compressionMethod = readLe16(cd, static_cast<int>(pos + 10));
        const quint32 expectedCrc = readLe32(cd, static_cast<int>(pos + 16));
        const quint32 compressedSize = readLe32(cd, static_cast<int>(pos + 20));
        const quint32 uncompressedSize = readLe32(cd, static_cast<int>(pos + 24));
        const quint32 localHeaderOffset = readLe32(cd, static_cast<int>(pos + 42));
        const quint16 fileNameLength = readLe16(cd, static_cast<int>(pos + 28));
        const quint16 extraLength = readLe16(cd, static_cast<int>(pos + 30));
        const quint16 commentLength = readLe16(cd, static_cast<int>(pos + 32));

        if (compressedSize == 0xffffffffu || uncompressedSize == 0xffffffffu || localHeaderOffset == 0xffffffffu) {
            error = QStringLiteral("ZIP64 NPZ entries are not supported by this validator.");
            return false;
        }

        const qint64 nextEntry = pos + 46 + fileNameLength + extraLength + commentLength;
        if (nextEntry > cd.size()) {
            error = QStringLiteral("Central directory entry is truncated.");
            return false;
        }

        const QString entryName = QString::fromUtf8(cd.mid(static_cast<int>(pos + 46), fileNameLength));
        QByteArray localHeader;
        if (!readAt(file, static_cast<qint64>(localHeaderOffset), 30, localHeader) ||
            readLe32(localHeader, 0) != 0x04034b50u) {
            error = QStringLiteral("Invalid ZIP local file header.");
            return false;
        }

        const quint16 localNameLength = readLe16(localHeader, 26);
        const quint16 localExtraLength = readLe16(localHeader, 28);
        const qint64 dataOffset = static_cast<qint64>(localHeaderOffset) + 30 + localNameLength + localExtraLength;
        if (dataOffset < 0 || dataOffset + static_cast<qint64>(compressedSize) > fileSize) {
            error = QStringLiteral("A ZIP entry is truncated before its compressed data ends.");
            return false;
        }

        QString entryError;
        if (!validateZipEntryPayload(file,
                                     dataOffset,
                                     compressedSize,
                                     uncompressedSize,
                                     expectedCrc,
                                     compressionMethod,
                                     entryName,
                                     entryError)) {
            error = entryError;
            return false;
        }

        pos = nextEntry;
    }

    if (pos != cd.size()) {
        error = QStringLiteral("Central directory has unexpected trailing data.");
        return false;
    }

    return true;
}

struct ZipEntryPayload {
    QString name;
    QByteArray data;
    quint32 crc = 0;
    bool wasStored = false;
};

static bool inflateRaw(const QByteArray &compressed, quint32 uncompressedSize, QByteArray &out, QString &error)
{
    out.resize(static_cast<qsizetype>(uncompressedSize));
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    int zret = inflateInit2(&stream, -MAX_WBITS);
    if (zret != Z_OK) {
        error = QStringLiteral("Could not initialize NPZ inflater.");
        return false;
    }

    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef *>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());

    zret = inflate(&stream, Z_FINISH);
    const bool ok = zret == Z_STREAM_END && stream.avail_in == 0 && stream.total_out == uncompressedSize;
    inflateEnd(&stream);
    if (!ok) {
        error = QStringLiteral("Could not decompress NPZ entry.");
        return false;
    }
    return true;
}

static bool readZipEntryPayloads(const QString &path, QList<ZipEntryPayload> &entries, bool &needsRuntimeRewrite, QString &error)
{
    needsRuntimeRewrite = false;
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        error = QStringLiteral("File does not exist.");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Could not open NPZ file.");
        return false;
    }

    const qint64 fileSize = info.size();
    const qint64 tailSize = qMin<qint64>(fileSize, 22 + 65535);
    QByteArray tail;
    if (!readAt(file, fileSize - tailSize, tailSize, tail)) {
        error = QStringLiteral("Could not read NPZ footer.");
        return false;
    }

    int eocdOffsetInTail = -1;
    for (int i = tail.size() - 22; i >= 0; --i) {
        if (readLe32(tail, i) == 0x06054b50u) {
            eocdOffsetInTail = i;
            break;
        }
    }
    if (eocdOffsetInTail < 0) {
        error = QStringLiteral("Missing ZIP end-of-directory footer.");
        return false;
    }

    const quint16 totalEntries = readLe16(tail, eocdOffsetInTail + 10);
    const quint32 centralDirectorySize = readLe32(tail, eocdOffsetInTail + 12);
    const quint32 centralDirectoryOffset = readLe32(tail, eocdOffsetInTail + 16);
    if (centralDirectoryOffset == 0xffffffffu || centralDirectorySize == 0xffffffffu) {
        error = QStringLiteral("ZIP64 NPZ archives are not supported by this normalizer.");
        return false;
    }

    QByteArray cd;
    if (!readAt(file, centralDirectoryOffset, centralDirectorySize, cd)) {
        error = QStringLiteral("Could not read NPZ directory.");
        return false;
    }

    qint64 pos = 0;
    for (quint16 entry = 0; entry < totalEntries; ++entry) {
        if (pos + 46 > cd.size() || readLe32(cd, static_cast<int>(pos)) != 0x02014b50u) {
            error = QStringLiteral("Invalid ZIP central directory entry.");
            return false;
        }

        const quint16 compressionMethod = readLe16(cd, static_cast<int>(pos + 10));
        const quint32 expectedCrc = readLe32(cd, static_cast<int>(pos + 16));
        const quint32 compressedSize = readLe32(cd, static_cast<int>(pos + 20));
        const quint32 uncompressedSize = readLe32(cd, static_cast<int>(pos + 24));
        const quint32 localHeaderOffset = readLe32(cd, static_cast<int>(pos + 42));
        const quint16 fileNameLength = readLe16(cd, static_cast<int>(pos + 28));
        const quint16 extraLength = readLe16(cd, static_cast<int>(pos + 30));
        const quint16 commentLength = readLe16(cd, static_cast<int>(pos + 32));
        if (compressionMethod != 0 || extraLength > 0 || commentLength > 0) {
            needsRuntimeRewrite = true;
        }
        const qint64 nextEntry = pos + 46 + fileNameLength + extraLength + commentLength;
        if (nextEntry > cd.size()) {
            error = QStringLiteral("Central directory entry is truncated.");
            return false;
        }

        QByteArray localHeader;
        if (!readAt(file, localHeaderOffset, 30, localHeader) ||
            readLe32(localHeader, 0) != 0x04034b50u) {
            error = QStringLiteral("Invalid ZIP local file header.");
            return false;
        }

        const quint16 localNameLength = readLe16(localHeader, 26);
        const quint16 localExtraLength = readLe16(localHeader, 28);
        if (localExtraLength > 0) {
            needsRuntimeRewrite = true;
        }
        const qint64 dataOffset = static_cast<qint64>(localHeaderOffset) + 30 + localNameLength + localExtraLength;
        QByteArray compressed;
        if (!readAt(file, dataOffset, compressedSize, compressed)) {
            error = QStringLiteral("A ZIP entry is truncated before its compressed data ends.");
            return false;
        }

        ZipEntryPayload payload;
        payload.name = QString::fromUtf8(cd.mid(static_cast<int>(pos + 46), fileNameLength));
        payload.crc = expectedCrc;
        payload.wasStored = compressionMethod == 0;
        if (compressionMethod == 0) {
            payload.data = compressed;
        } else if (compressionMethod == 8) {
            if (!inflateRaw(compressed, uncompressedSize, payload.data, error)) {
                return false;
            }
        } else {
            error = QStringLiteral("NPZ entry %1 uses unsupported compression method %2.")
                        .arg(payload.name)
                        .arg(compressionMethod);
            return false;
        }

        if (payload.data.size() != static_cast<qsizetype>(uncompressedSize)) {
            error = QStringLiteral("NPZ entry %1 has an invalid uncompressed size.").arg(payload.name);
            return false;
        }
        const quint32 actualCrc = static_cast<quint32>(crc32(0L,
            reinterpret_cast<const Bytef *>(payload.data.constData()),
            static_cast<uInt>(payload.data.size())));
        if (actualCrc != expectedCrc) {
            error = QStringLiteral("NPZ entry %1 failed CRC validation.").arg(payload.name);
            return false;
        }

        entries.append(payload);
        pos = nextEntry;
    }

    return true;
}

static bool writeStoredZip(const QList<ZipEntryPayload> &entries, const QString &path, QString &error)
{
    QByteArray output;
    QByteArray centralDirectory;

    for (const ZipEntryPayload &entry : entries) {
        if (entry.name.toUtf8().size() > 0xffff ||
            entry.data.size() > 0xffffffffLL ||
            output.size() > 0xffffffffLL) {
            error = QStringLiteral("NPZ entry is too large for non-ZIP64 rewrite.");
            return false;
        }

        const QByteArray nameBytes = entry.name.toUtf8();
        const quint32 localOffset = static_cast<quint32>(output.size());

        appendLe32(output, 0x04034b50u);
        appendLe16(output, 20);
        appendLe16(output, 0);
        appendLe16(output, 0);
        appendLe16(output, 0);
        appendLe16(output, 0);
        appendLe32(output, entry.crc);
        appendLe32(output, static_cast<quint32>(entry.data.size()));
        appendLe32(output, static_cast<quint32>(entry.data.size()));
        appendLe16(output, static_cast<quint16>(nameBytes.size()));
        appendLe16(output, 0);
        output.append(nameBytes);
        output.append(entry.data);

        appendLe32(centralDirectory, 0x02014b50u);
        appendLe16(centralDirectory, 20);
        appendLe16(centralDirectory, 20);
        appendLe16(centralDirectory, 0);
        appendLe16(centralDirectory, 0);
        appendLe16(centralDirectory, 0);
        appendLe16(centralDirectory, 0);
        appendLe32(centralDirectory, entry.crc);
        appendLe32(centralDirectory, static_cast<quint32>(entry.data.size()));
        appendLe32(centralDirectory, static_cast<quint32>(entry.data.size()));
        appendLe16(centralDirectory, static_cast<quint16>(nameBytes.size()));
        appendLe16(centralDirectory, 0);
        appendLe16(centralDirectory, 0);
        appendLe16(centralDirectory, 0);
        appendLe16(centralDirectory, 0);
        appendLe32(centralDirectory, 0);
        appendLe32(centralDirectory, localOffset);
        centralDirectory.append(nameBytes);
    }

    const quint32 centralDirectoryOffset = static_cast<quint32>(output.size());
    output.append(centralDirectory);

    appendLe32(output, 0x06054b50u);
    appendLe16(output, 0);
    appendLe16(output, 0);
    appendLe16(output, static_cast<quint16>(entries.size()));
    appendLe16(output, static_cast<quint16>(entries.size()));
    appendLe32(output, static_cast<quint32>(centralDirectory.size()));
    appendLe32(output, centralDirectoryOffset);
    appendLe16(output, 0);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("Could not write normalized NPZ file.");
        return false;
    }
    if (file.write(output) != output.size()) {
        error = QStringLiteral("Could not write all normalized NPZ bytes.");
        return false;
    }
    return true;
}

static bool normalizeStoredNpzForRuntime(const QString &path, QString &error)
{
    QList<ZipEntryPayload> entries;
    bool needsRuntimeRewrite = false;
    if (!readZipEntryPayloads(path, entries, needsRuntimeRewrite, error)) {
        return false;
    }
    if (!needsRuntimeRewrite) {
        return true;
    }

    const QString tempPath = path + QStringLiteral(".lastudio.tmp");
    const QString backupPath = path + QStringLiteral(".lastudio.bak");
    QFile::remove(tempPath);
    QFile::remove(backupPath);

    if (!writeStoredZip(entries, tempPath, error)) {
        QFile::remove(tempPath);
        return false;
    }

    QString validationError;
    if (!validateZipContainer(tempPath, validationError)) {
        QFile::remove(tempPath);
        error = QStringLiteral("Normalized NPZ failed validation: %1").arg(validationError);
        return false;
    }

    if (!QFile::rename(path, backupPath)) {
        QFile::remove(tempPath);
        error = QStringLiteral("Could not prepare original NPZ for replacement.");
        return false;
    }
    if (!QFile::rename(tempPath, path)) {
        QFile::rename(backupPath, path);
        QFile::remove(tempPath);
        error = QStringLiteral("Could not replace NPZ with normalized runtime-compatible copy.");
        return false;
    }
    QFile::remove(backupPath);
    Logger::info(QStringLiteral("VieneuTtsBackend"),
                 QStringLiteral("Normalized NPZ entries to stored non-ZIP64 layout for runtime compatibility: %1").arg(path));
    return true;
}

static bool validateNpzInputs(const QVariantMap &config, QString &error)
{
    QSet<QString> checkedPaths;
    for (auto it = config.cbegin(); it != config.cend(); ++it) {
        const QString path = PathUtils::toNativeShortPath(it.value().toString());
        if (path.isEmpty() || !path.endsWith(QStringLiteral(".npz"), Qt::CaseInsensitive)) {
            continue;
        }
        if (checkedPaths.contains(path)) {
            continue;
        }
        checkedPaths.insert(path);

        QString validationError;
        if (!validateZipContainer(path, validationError)) {
            error = QStringLiteral("The model file %1 appears to be incomplete or corrupted (%2). Delete this file and download it again: %3")
                        .arg(QFileInfo(path).fileName(), validationError, path);
            return false;
        }
    }

    return true;
}
VieneuTtsBackend::~VieneuTtsBackend()
{
    unload();
}

void VieneuTtsBackend::setProgressCallback(std::function<bool(int current,
                                                              int total,
                                                              const QString &stage,
                                                              int chunkIndex,
                                                              int chunkCount)> callback)
{
    m_progressCallback = std::move(callback);
}

void VieneuTtsBackend::handleProgress(const vieneu_progress *progress, void *userData)
{
    auto *self = static_cast<VieneuTtsBackend *>(userData);
    if (!self || self->m_cancelRequested.load() || !self->m_progressCallback || !progress) {
        return;
    }

    int current = progress->current;
    int total = progress->total;
    if (total <= 0 && progress->progress >= 0.0f) {
        total = 1000;
        current = qBound(0, static_cast<int>(std::round(progress->progress * total)), total);
    }

    self->m_progressCallback(current,
                             total,
                             QString::fromUtf8(progress->stage ? progress->stage : ""),
                             -1,
                             0);
}

QString VieneuTtsBackend::resolveVieneuV3Style(const QVariantMap &settings) const
{
    const QString explicitStyle = settings.value(QStringLiteral("style")).toString().trimmed();
    if (!explicitStyle.isEmpty()) {
        return explicitStyle;
    }

    QString voiceId = settings.value(QStringLiteral("voice")).toString();
    if (voiceId.isEmpty()) {
        voiceId = m_vieneuV3DefaultVoiceId;
    }

    const QString presetStyle = m_vieneuV3VoiceStyles.value(voiceId).trimmed();
    return presetStyle.isEmpty() ? QStringLiteral("tu_nhien") : presetStyle;
}

void VieneuTtsBackend::cancelProcessing()
{
    m_cancelRequested = true;
}

bool VieneuTtsBackend::load(const QVariantMap &config, QString &error, QVariantList &schema)
{
    m_vieneuV3DefaultVoiceId.clear();
    m_vieneuV3VoiceStyles.clear();

    QString modelPath = PathUtils::toNativeShortPath(config.value("model").toString());
    QString encoderPath = PathUtils::toNativeShortPath(config.value("encoder").toString());
    QString decoderPath = PathUtils::toNativeShortPath(config.value("decoder").toString());
    QString voicesPath = PathUtils::toNativeShortPath(config.value("voices").toString());
    QString runtimePath = PathUtils::toNativeShortPath(config.value("runtimePath").toString());
    const QString pipelineProfile = config.value(QStringLiteral("pipelineProfile")).toString();
    const QString backendId = config.value(QStringLiteral("backend")).toString();
    const bool hasVieneuV3NativeAssets =
        config.contains(QStringLiteral("heads")) ||
        config.contains(QStringLiteral("acoustic")) ||
        QFileInfo(modelPath).fileName().compare(QStringLiteral("backbone.gguf"), Qt::CaseInsensitive) == 0;
    QString effectivePipelineProfile = pipelineProfile;
    if (effectivePipelineProfile.isEmpty() &&
        (backendId.contains(QStringLiteral("vieneu"), Qt::CaseInsensitive) ||
         runtimePath.contains(QStringLiteral("vieneu-tts"), Qt::CaseInsensitive)) &&
        hasVieneuV3NativeAssets) {
        effectivePipelineProfile = QStringLiteral("vieneu-v3-native");
    }
    m_pipelineProfile = effectivePipelineProfile;
    m_useAbiV2 = effectivePipelineProfile == QStringLiteral("vieneu-v3-onnx") ||
                 effectivePipelineProfile == QStringLiteral("vieneu-v3-native");
    QString configPath = PathUtils::toNativeShortPath(config.value(QStringLiteral("config")).toString());
    QString tokenizerPath = PathUtils::toNativeShortPath(config.value(QStringLiteral("tokenizer")).toString());
    QString prefillPath = PathUtils::toNativeShortPath(config.value(QStringLiteral("prefill")).toString());
    QString codecDecodePath = PathUtils::toNativeShortPath(config.value(QStringLiteral("codec_decode")).toString());
    QString abiV2ModelDir = !configPath.isEmpty()
        ? QFileInfo(configPath).absolutePath()
        : QFileInfo(modelPath).absolutePath();
    QString abiV2OnnxDir = !prefillPath.isEmpty()
        ? QFileInfo(prefillPath).absolutePath()
        : QFileInfo(modelPath).absolutePath();
    QString abiV2CodecDir = !codecDecodePath.isEmpty()
        ? QFileInfo(codecDecodePath).absolutePath()
        : QFileInfo(decoderPath).absolutePath();

    if (!validateNpzInputs(config, error)) {
        Logger::error(QStringLiteral("VieneuTtsBackend"), error);
        return false;
    }
    if (m_useAbiV2) {
        const QString headsPath = PathUtils::toNativeShortPath(config.value(QStringLiteral("heads")).toString());
        if (!headsPath.isEmpty() && headsPath.endsWith(QStringLiteral(".npz"), Qt::CaseInsensitive)) {
            if (!normalizeStoredNpzForRuntime(headsPath, error)) {
                error = QStringLiteral("Failed to normalize VieNeu v3 heads NPZ for runtime compatibility: %1").arg(error);
                Logger::error(QStringLiteral("VieneuTtsBackend"), error);
                return false;
            }
        }
    }

    QString resolvedVoicesPath = voicesPath;
    if (resolvedVoicesPath.isEmpty() && !modelPath.isEmpty()) {
        QFileInfo modelInfo(modelPath);
        const QStringList siblingVoiceFiles = {
            QStringLiteral("voices.json"),
            QStringLiteral("voices_v3_turbo.json")
        };
        for (const QString &fileName : siblingVoiceFiles) {
            QString siblingVoices = modelInfo.dir().absoluteFilePath(fileName);
            if (QFileInfo::exists(siblingVoices)) {
                resolvedVoicesPath = siblingVoices;
                break;
            }
        }
    }
    if (resolvedVoicesPath.isEmpty() && m_useAbiV2) {
        const QString modelDir = !configPath.isEmpty()
            ? QFileInfo(configPath).absolutePath()
            : QFileInfo(config.value(QStringLiteral("prefill")).toString()).absolutePath();
        const QStringList siblingVoiceFiles = {
            QStringLiteral("voices_v3_turbo.json"),
            QStringLiteral("voices.json")
        };
        for (const QString &fileName : siblingVoiceFiles) {
            const QString siblingVoices = QDir(modelDir).absoluteFilePath(fileName);
            if (QFileInfo::exists(siblingVoices)) {
                resolvedVoicesPath = siblingVoices;
                break;
            }
        }
        if (resolvedVoicesPath.isEmpty()) {
            resolvedVoicesPath = copyBundledVieneuV3VoicesJson(modelDir);
        }
        if (resolvedVoicesPath.isEmpty()) {
            resolvedVoicesPath = writeVieneuV3FallbackVoicesJson(modelDir);
        }
    }

    QVariantList speakerChoices;
    QString defaultVoiceId;
    if (!resolvedVoicesPath.isEmpty() && QFileInfo::exists(resolvedVoicesPath)) {
        QFile file(resolvedVoicesPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                QJsonObject rootObj = doc.object();
                defaultVoiceId = rootObj.value(QStringLiteral("default_voice")).toString();
                m_vieneuV3DefaultVoiceId = defaultVoiceId;
                QJsonObject presets = rootObj.value(QStringLiteral("presets")).toObject();
                if (!presets.isEmpty()) {
                    for (auto it = presets.begin(); it != presets.end(); ++it) {
                        QString key = it.key();
                        QJsonObject voiceData = it.value().toObject();
                        const QString description = voiceData.value(QStringLiteral("description")).toString();
                        const QString style = voiceData.value(QStringLiteral("style")).toString();
                        const QString displayText = description.isEmpty()
                            ? key
                            : QStringLiteral("%1 \u00b7 %2").arg(key, description);
                        QVariantMap choice;
                        choice[QStringLiteral("text")] = displayText;
                        choice[QStringLiteral("value")] = key;
                        if (!description.isEmpty()) {
                            choice[QStringLiteral("detail")] = description;
                        }
                        if (!style.isEmpty()) {
                            choice[QStringLiteral("style")] = style;
                            m_vieneuV3VoiceStyles.insert(key, style);
                        }
                        speakerChoices.append(choice);
                    }
                } else {
                    for (auto it = rootObj.begin(); it != rootObj.end(); ++it) {
                        const QString key = it.key();
                        if (key == QStringLiteral("spec") || key == QStringLiteral("version") || key == QStringLiteral("default_voice")) {
                            continue;
                        }
                        if (!it.value().isObject()) {
                            continue;
                        }
                        QJsonObject voiceData = it.value().toObject();
                        if (!voiceData.contains(QStringLiteral("codes")) && !voiceData.contains(QStringLiteral("embedding"))) {
                            continue;
                        }
                        const QString description = voiceData.value(QStringLiteral("description")).toString();
                        const QString style = voiceData.value(QStringLiteral("style")).toString();
                        const QString displayText = description.isEmpty()
                            ? key
                            : QStringLiteral("%1 \u00b7 %2").arg(key, description);
                        QVariantMap choice;
                        choice[QStringLiteral("text")] = displayText;
                        choice[QStringLiteral("value")] = key;
                        if (!description.isEmpty()) {
                            choice[QStringLiteral("detail")] = description;
                        }
                        if (!style.isEmpty()) {
                            choice[QStringLiteral("style")] = style;
                            m_vieneuV3VoiceStyles.insert(key, style);
                        }
                        speakerChoices.append(choice);
                    }
                }
            }
        }
    }

    if (speakerChoices.isEmpty() && m_useAbiV2) {
        speakerChoices = vieneuV3BuiltInVoices();
        defaultVoiceId = QStringLiteral("Ng\u1ecdc Linh");
    }

    if (!speakerChoices.isEmpty()) {
        QVariantMap voiceParam;
        voiceParam["id"] = "voice";
        voiceParam["name"] = "Voice";
        voiceParam["type"] = "choice";
        voiceParam["choices"] = speakerChoices;
        if (defaultVoiceId.isEmpty()) {
            defaultVoiceId = speakerChoices.first().toMap().value(QStringLiteral("value")).toString();
        } else {
            bool defaultExists = false;
            for (const QVariant &choiceVar : speakerChoices) {
                if (choiceVar.toMap().value(QStringLiteral("value")).toString() == defaultVoiceId) {
                    defaultExists = true;
                    break;
                }
            }
            if (!defaultExists) {
                defaultVoiceId = speakerChoices.first().toMap().value(QStringLiteral("value")).toString();
            }
        }
        voiceParam["default"] = defaultVoiceId;
        voiceParam["description"] = m_useAbiV2
            ? "Select a built-in VieNeu v3 preset voice."
            : "Select a voice preset from voices.json.";
        schema.append(voiceParam);
    } else {
        if (resolvedVoicesPath.isEmpty()) {
            Logger::warning(QStringLiteral("VieneuTtsBackend"), "VieNeu preset voices unavailable: no voices file was provided or found next to the model.");
        } else {
            Logger::warning(QStringLiteral("VieneuTtsBackend"), QString("VieNeu preset voices unavailable: could not parse voices from %1").arg(resolvedVoicesPath));
        }
    }

    if (m_useAbiV2 && effectivePipelineProfile == QStringLiteral("vieneu-v3-native")) {
        const QStringList requiredNativeFiles = {
            QDir(abiV2ModelDir).absoluteFilePath(QStringLiteral("backbone.gguf")),
            QDir(abiV2ModelDir).absoluteFilePath(QStringLiteral("vieneu_v3_heads.npz")),
            QDir(abiV2ModelDir).absoluteFilePath(QStringLiteral("acoustic/vieneu_acoustic_weights.npz")),
            QDir(abiV2ModelDir).absoluteFilePath(QStringLiteral("speaker_encoder.onnx")),
            QDir(abiV2CodecDir).absoluteFilePath(QStringLiteral("moss_audio_tokenizer_encode.onnx")),
            QDir(abiV2CodecDir).absoluteFilePath(QStringLiteral("moss_audio_tokenizer_encode.data")),
            QDir(abiV2CodecDir).absoluteFilePath(QStringLiteral("moss_audio_tokenizer_decode_full.onnx")),
            QDir(abiV2CodecDir).absoluteFilePath(QStringLiteral("moss_audio_tokenizer_decode_shared.data"))
        };
        for (const QString &requiredFile : requiredNativeFiles) {
            if (!QFileInfo::exists(requiredFile)) {
                error = QStringLiteral("VieNeu-TTS v3 native asset is missing: %1").arg(requiredFile);
                Logger::error(QStringLiteral("VieneuTtsBackend"), error);
                return false;
            }
        }
    }

    {
        auto& vieneu = VieneuTtsInterface::instance();
        const QString g2pModelDir = !configPath.isEmpty()
            ? QFileInfo(configPath).absolutePath()
            : QFileInfo(modelPath).absolutePath();
        configureVieneuSeaG2pDict(runtimePath, g2pModelDir, configPath);
        if (m_useAbiV2 && effectivePipelineProfile == QStringLiteral("vieneu-v3-native")) {
            configureVieneuV3NativeQualityEnv();
        }

        if (!runtimePath.isEmpty()) {
            if (!s_sessionVieneuRuntimePath.isEmpty() &&
                s_sessionVieneuRuntimePath != runtimePath) {
                error = QStringLiteral("Switching VieNeu TTS runtime backend (CPU/CUDA/Vulkan) in one running session is unstable. Please restart LA Studio after changing runtime.");
                Logger::error(QStringLiteral("VieneuTtsBackend"), error + QStringLiteral(" Previous: %1 | Requested: %2")
                              .arg(s_sessionVieneuRuntimePath, runtimePath));
                return false;
            }
            if (!vieneu.load(runtimePath)) {
                error = QString("Failed to load VieNeu TTS runtime: ") + vieneu.errorString();
                Logger::error(QStringLiteral("VieneuTtsBackend"), error);
                return false;
            }
        }

        if (!vieneu.isLoaded()) {
            error = QStringLiteral("No VieNeu TTS runtime loaded. Please select one in settings.");
            Logger::error(QStringLiteral("VieneuTtsBackend"), error);
            return false;
        }
        if (m_useAbiV2 && !vieneu.hasAbiV2()) {
            error = QStringLiteral("Selected VieNeu TTS runtime does not expose ABI v2 required for VieNeu-TTS v3.");
            Logger::error(QStringLiteral("VieneuTtsBackend"), error);
            return false;
        }
        if (!m_useAbiV2 && !vieneu.hasAbiV1()) {
            error = QStringLiteral("Selected VieNeu TTS runtime does not expose ABI v1 required for VieNeu-TTS v2.");
            Logger::error(QStringLiteral("VieneuTtsBackend"), error);
            return false;
        }

        if (m_useAbiV2) {
            QByteArray profileBytes = effectivePipelineProfile.isEmpty()
                ? QByteArrayLiteral("vieneu-v3-onnx")
                : effectivePipelineProfile.toUtf8();
            QByteArray modelDirBytes = PathUtils::toNativeShortPath(abiV2ModelDir).toUtf8();
            QByteArray onnxDirBytes = PathUtils::toNativeShortPath(abiV2OnnxDir).toUtf8();
            QByteArray codecDirBytes = PathUtils::toNativeShortPath(abiV2CodecDir).toUtf8();
            QByteArray configPathBytes = configPath.toUtf8();
            QByteArray tokenizerPathBytes = tokenizerPath.toUtf8();
            QByteArray voicesPathBytes = resolvedVoicesPath.toUtf8();

            Logger::info(QStringLiteral("VieneuTtsBackend"),
                         QStringLiteral("Initializing VieNeu ABI v2 profile=%1, modelDir=%2, onnxDir=%3, codecDir=%4, voices=%5")
                             .arg(QString::fromUtf8(profileBytes), abiV2ModelDir, abiV2OnnxDir, abiV2CodecDir, resolvedVoicesPath));

            vieneu_init_params_v2 params;
            vieneu.vieneu_init_v2_default_params(&params);
            params.profile = profileBytes.constData();
            params.model_dir = modelDirBytes.isEmpty() ? nullptr : modelDirBytes.constData();
            params.onnx_dir = onnxDirBytes.isEmpty() ? nullptr : onnxDirBytes.constData();
            params.codec_dir = codecDirBytes.isEmpty() ? nullptr : codecDirBytes.constData();
            params.config_path = configPathBytes.isEmpty() ? nullptr : configPathBytes.constData();
            params.tokenizer_path = tokenizerPathBytes.isEmpty() ? nullptr : tokenizerPathBytes.constData();
            params.voices_json_path = voicesPathBytes.isEmpty() ? nullptr : voicesPathBytes.constData();
            params.n_threads = effectivePipelineProfile == QStringLiteral("vieneu-v3-native")
                ? InferenceThreadPolicy::recommendedThreadCount(InferenceBackendProfile::VieNeuNative)
                : InferenceThreadPolicy::recommendedThreadCount(InferenceBackendProfile::CrispAsrCpu);

            m_context = vieneu.vieneu_init_v2(&params);
        } else {
            QByteArray modelPathBytes = modelPath.toUtf8();
            QByteArray encoderPathBytes = encoderPath.toUtf8();
            QByteArray decoderPathBytes = decoderPath.toUtf8();
            QByteArray voicesPathBytes = resolvedVoicesPath.toUtf8();

            vieneu_init_params params;
            vieneu.vieneu_init_default_params(&params);
            params.model_path = modelPathBytes.constData();
            params.encoder_path = encoderPathBytes.isEmpty() ? nullptr : encoderPathBytes.constData();
            params.decoder_path = decoderPathBytes.isEmpty() ? nullptr : decoderPathBytes.constData();
            params.voices_json_path = voicesPathBytes.isEmpty() ? nullptr : voicesPathBytes.constData();
            params.n_ctx = 2048;
            params.n_threads = InferenceThreadPolicy::recommendedThreadCount(InferenceBackendProfile::CrispAsrCpu);

            m_context = vieneu.vieneu_init(&params);
        }

        if (!m_context) {
            error = QString::fromUtf8(vieneu.vieneu_last_error());
            Logger::error(QStringLiteral("VieneuTtsBackend"), "Failed to initialize VieNeu TTS model: " + error);
            unload();
            return false;
        }
        if (vieneu.vieneu_set_progress_callback) {
            vieneu.vieneu_set_progress_callback((struct vieneu_context*)m_context, &VieneuTtsBackend::handleProgress, this);
        }

        if (!runtimePath.isEmpty())
            s_sessionVieneuRuntimePath = runtimePath;

        Logger::info(QStringLiteral("VieneuTtsBackend"), QString("VieNeu TTS profile loaded successfully"));
        return true;
    }

}

void VieneuTtsBackend::unload()
{
    if (m_context) {
        auto& vieneu = VieneuTtsInterface::instance();
        if (vieneu.isLoaded() && vieneu.vieneu_free) {
            vieneu.vieneu_free(static_cast<vieneu_context*>(m_context));
        }
        m_context = nullptr;
    }
    VieneuTtsInterface::instance().unload();
    m_useAbiV2 = false;
    m_pipelineProfile.clear();
    m_vieneuV3DefaultVoiceId.clear();
    m_vieneuV3VoiceStyles.clear();
}

bool VieneuTtsBackend::synthesize(const QString &text, float speed, const QVariantMap &settings,
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(speed);
    if (text.trimmed().isEmpty()) {
        error = QStringLiteral("VieNeu TTS cannot synthesize empty text.");
        return false;
    }
    const bool internalChunk = settings.value(QStringLiteral("__lastudio_internal_chunk"), false).toBool();
    if (!internalChunk) {
        m_cancelRequested = false;
    }

    if (m_useAbiV2 && m_pipelineProfile != QStringLiteral("vieneu-v3-native") && !internalChunk) {
        const QStringList chunks = splitVieneuV3TextForSafeDecode(text, settings);
        if (chunks.size() > 1) {
            Logger::info(QStringLiteral("VieneuTtsBackend"),
                         QStringLiteral("Splitting VieNeu v3 request into %1 safe chunks (text_chars=%2, max_chunk_chars=%3).")
                             .arg(chunks.size())
                             .arg(text.trimmed().size())
                             .arg(vieneuV3SafeChunkCharLimit(settings)));

            QVector<float> combined;
            int combinedSampleRate = 0;
            for (int i = 0; i < chunks.size(); ++i) {
                if (m_cancelRequested.load()) {
                    error = QStringLiteral("TTS synthesis was canceled.");
                    return false;
                }

                if (m_progressCallback) {
                    m_progressCallback(i, chunks.size(), QStringLiteral("text_chunk"), i, chunks.size());
                }

                QVariantMap chunkSettings = settings;
                chunkSettings.insert(QStringLiteral("__lastudio_internal_chunk"), true);
                chunkSettings.insert(QStringLiteral("max_chars"), vieneuV3SafeChunkCharLimit(settings));

                QVector<float> chunkSamples;
                int chunkSampleRate = 0;
                QString chunkError;
                if (!synthesize(chunks.at(i), 1.0f, chunkSettings, chunkSamples, chunkSampleRate, chunkError)) {
                    error = chunkError.isEmpty()
                                ? QStringLiteral("VieNeu TTS failed while synthesizing chunk %1/%2.")
                                      .arg(i + 1)
                                      .arg(chunks.size())
                                : QStringLiteral("%1 (chunk %2/%3)").arg(chunkError).arg(i + 1).arg(chunks.size());
                    return false;
                }

                if (chunkSamples.isEmpty() || chunkSampleRate <= 0) {
                    error = QStringLiteral("VieNeu TTS returned empty audio for chunk %1/%2.")
                                .arg(i + 1)
                                .arg(chunks.size());
                    return false;
                }
                if (combinedSampleRate == 0) {
                    combinedSampleRate = chunkSampleRate;
                } else if (combinedSampleRate != chunkSampleRate) {
                    error = QStringLiteral("VieNeu TTS returned inconsistent sample rates across chunks.");
                    return false;
                }
                combined.append(chunkSamples);
            }

            if (m_progressCallback) {
                m_progressCallback(chunks.size(), chunks.size(), QStringLiteral("text_chunk"), chunks.size() - 1, chunks.size());
            }

            samples = std::move(combined);
            sampleRate = combinedSampleRate;
            return !samples.isEmpty() && sampleRate > 0;
        }
    }

    {
        auto& vieneu = VieneuTtsInterface::instance();
        if (!vieneu.isLoaded() || !m_context) {
            error = QStringLiteral("VieNeu TTS runtime was unloaded unexpectedly.");
            return false;
        }

        const QString synthesisText = m_useAbiV2 ? ensureSentenceTerminatorForVieneuV3(text) : text;
        QByteArray textBytes = synthesisText.toUtf8();
        QByteArray voiceIdBytes;
        vieneu_audio out;
        memset(&out, 0, sizeof(out));
        int status = -1;

        if (m_useAbiV2) {
            const bool useNativeAbiV3 = m_pipelineProfile == QStringLiteral("vieneu-v3-native") && vieneu.hasAbiV3();
            if (useNativeAbiV3) {
                QByteArray styleBytes = resolveVieneuV3Style(settings).toUtf8();
                vieneu_tts_params_v3 params;
                vieneu.vieneu_tts_v3_default_params(&params);
                params.text = textBytes.constData();
                params.style = styleBytes.constData();
                if (settings.contains(QStringLiteral("voice"))) {
                    voiceIdBytes = settings.value(QStringLiteral("voice")).toString().toUtf8();
                    params.voice_id = voiceIdBytes.constData();
                }
                if (settings.contains(QStringLiteral("temperature"))) params.temperature = settings.value(QStringLiteral("temperature")).toFloat();
                if (settings.contains(QStringLiteral("top_k"))) params.top_k = settings.value(QStringLiteral("top_k")).toInt();
                if (settings.contains(QStringLiteral("top_p"))) params.top_p = settings.value(QStringLiteral("top_p")).toFloat();
                if (settings.contains(QStringLiteral("max_new_frames"))) params.max_new_frames = settings.value(QStringLiteral("max_new_frames")).toInt();
                if (settings.contains(QStringLiteral("repetition_penalty"))) params.repetition_penalty = settings.value(QStringLiteral("repetition_penalty")).toFloat();
                if (settings.contains(QStringLiteral("max_chars"))) params.max_chars = settings.value(QStringLiteral("max_chars")).toInt();
                if (settings.contains(QStringLiteral("denoise_ref"))) params.denoise_ref = settings.value(QStringLiteral("denoise_ref")).toBool();
                if (settings.contains(QStringLiteral("use_ref_codes"))) params.use_ref_codes = settings.value(QStringLiteral("use_ref_codes")).toBool();
                if (settings.contains(QStringLiteral("apply_watermark"))) params.apply_watermark = settings.value(QStringLiteral("apply_watermark")).toBool();
                const int requestedFrames = params.max_new_frames;
                params.max_new_frames = adaptiveVieneuV3FrameCap(synthesisText, params.max_new_frames);
                if (params.max_new_frames != requestedFrames) {
                    Logger::info(QStringLiteral("VieneuTtsBackend"),
                                 QStringLiteral("Adaptive VieNeu v3 frame cap: %1 -> %2")
                                 .arg(requestedFrames)
                                 .arg(params.max_new_frames));
                }
                status = vieneu.vieneu_synthesize_v3((struct vieneu_context*)m_context, &params, &out);
            } else {
                vieneu_tts_params_v2 params;
                vieneu.vieneu_tts_v2_default_params(&params);
                params.text = textBytes.constData();
                if (settings.contains(QStringLiteral("voice"))) {
                    voiceIdBytes = settings.value(QStringLiteral("voice")).toString().toUtf8();
                    params.voice_id = voiceIdBytes.constData();
                }
                if (settings.contains(QStringLiteral("temperature"))) params.temperature = settings.value(QStringLiteral("temperature")).toFloat();
                if (settings.contains(QStringLiteral("top_k"))) params.top_k = settings.value(QStringLiteral("top_k")).toInt();
                if (settings.contains(QStringLiteral("top_p"))) params.top_p = settings.value(QStringLiteral("top_p")).toFloat();
                if (settings.contains(QStringLiteral("max_new_frames"))) params.max_new_frames = settings.value(QStringLiteral("max_new_frames")).toInt();
                if (settings.contains(QStringLiteral("repetition_penalty"))) params.repetition_penalty = settings.value(QStringLiteral("repetition_penalty")).toFloat();
                if (settings.contains(QStringLiteral("max_chars"))) params.max_chars = settings.value(QStringLiteral("max_chars")).toInt();
                if (settings.contains(QStringLiteral("apply_watermark"))) params.apply_watermark = settings.value(QStringLiteral("apply_watermark")).toBool();
                const int requestedFrames = params.max_new_frames;
                params.max_new_frames = adaptiveVieneuV3FrameCap(synthesisText, params.max_new_frames);
                if (params.max_new_frames != requestedFrames) {
                    Logger::info(QStringLiteral("VieneuTtsBackend"),
                                 QStringLiteral("Adaptive VieNeu v3 frame cap: %1 -> %2")
                                 .arg(requestedFrames)
                                 .arg(params.max_new_frames));
                }
                status = vieneu.vieneu_synthesize_v2((struct vieneu_context*)m_context, &params, &out);
            }
        } else {
            vieneu_tts_params params;
            vieneu.vieneu_tts_default_params(&params);
            params.text = textBytes.constData();
            if (settings.contains(QStringLiteral("voice"))) {
                voiceIdBytes = settings.value(QStringLiteral("voice")).toString().toUtf8();
                params.voice_id = voiceIdBytes.constData();
            }
            if (settings.contains(QStringLiteral("temperature"))) params.temperature = settings.value(QStringLiteral("temperature")).toFloat();
            if (settings.contains(QStringLiteral("top_k"))) params.top_k = settings.value(QStringLiteral("top_k")).toInt();
            if (settings.contains(QStringLiteral("max_chars"))) params.max_chars = settings.value(QStringLiteral("max_chars")).toInt();
            if (settings.contains(QStringLiteral("max_tokens"))) params.max_tokens = settings.value(QStringLiteral("max_tokens")).toInt();
            if (settings.contains(QStringLiteral("skip_normalize"))) params.skip_normalize = settings.value(QStringLiteral("skip_normalize")).toBool();
            if (settings.contains(QStringLiteral("skip_phonemize"))) params.skip_phonemize = settings.value(QStringLiteral("skip_phonemize")).toBool();
            if (settings.contains(QStringLiteral("apply_watermark"))) params.apply_watermark = settings.value(QStringLiteral("apply_watermark")).toBool();
            status = vieneu.vieneu_synthesize((struct vieneu_context*)m_context, &params, &out);
        }

        Logger::info(QStringLiteral("VieneuTtsBackend"), QString("VieNeu TTS synthesize returned: status=%1, samples=%2, sampleRate=%3")
                     .arg(status)
                     .arg(out.n_samples)
                     .arg(out.sample_rate));

        if (m_cancelRequested.load()) {
            if (out.samples) {
                vieneu.vieneu_audio_free(&out);
            }
            error = QStringLiteral("TTS synthesis was canceled.");
            return false;
        }

        if (status != 0) {
            error = QString::fromUtf8(vieneu.vieneu_last_error());
            Logger::error(QStringLiteral("VieneuTtsBackend"), "VieNeu TTS synthesis failed: " + error);
            return false;
        }

        if (!isValidRuntimeAudio(out)) {
            Logger::error(QStringLiteral("VieneuTtsBackend"), QString("VieNeu TTS returned invalid audio buffer: samples=%1, sampleRate=%2, ptr=%3")
                          .arg(out.n_samples)
                          .arg(out.sample_rate)
                          .arg(QString::number(reinterpret_cast<quintptr>(out.samples), 16)));
            vieneu.vieneu_audio_free(&out);
            error = QStringLiteral("VieNeu TTS returned an invalid or empty audio buffer.");
            return false;
        }

        samples.resize(out.n_samples);
        memcpy(samples.data(), out.samples, sizeof(float) * out.n_samples);
        sampleRate = out.sample_rate;

        vieneu.vieneu_audio_free(&out);
        return true;
    }

}

bool VieneuTtsBackend::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings,
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    const bool internalChunk = settings.value(QStringLiteral("__lastudio_internal_chunk"), false).toBool();
    if (!internalChunk) {
        m_cancelRequested = false;
    }

    if (m_useAbiV2 && m_pipelineProfile != QStringLiteral("vieneu-v3-native") && !internalChunk) {
        const QStringList chunks = splitVieneuV3TextForSafeDecode(text, settings);
        if (chunks.size() > 1) {
            Logger::info(QStringLiteral("VieneuTtsBackend"),
                         QStringLiteral("Splitting VieNeu v3 clone request into %1 safe chunks (text_chars=%2, max_chunk_chars=%3).")
                             .arg(chunks.size())
                             .arg(text.trimmed().size())
                             .arg(vieneuV3SafeChunkCharLimit(settings)));

            QVector<float> combined;
            int combinedSampleRate = 0;
            for (int i = 0; i < chunks.size(); ++i) {
                if (m_cancelRequested.load()) {
                    error = QStringLiteral("TTS synthesis was canceled.");
                    return false;
                }

                if (m_progressCallback) {
                    m_progressCallback(i, chunks.size(), QStringLiteral("text_chunk"), i, chunks.size());
                }

                QVariantMap chunkSettings = settings;
                chunkSettings.insert(QStringLiteral("__lastudio_internal_chunk"), true);
                chunkSettings.insert(QStringLiteral("max_chars"), vieneuV3SafeChunkCharLimit(settings));

                QVector<float> chunkSamples;
                int chunkSampleRate = 0;
                QString chunkError;
                if (!cloneVoice(chunks.at(i), referencePath, chunkSettings, chunkSamples, chunkSampleRate, chunkError)) {
                    error = chunkError.isEmpty()
                                ? QStringLiteral("VieNeu TTS failed while synthesizing clone chunk %1/%2.")
                                      .arg(i + 1)
                                      .arg(chunks.size())
                                : QStringLiteral("%1 (chunk %2/%3)").arg(chunkError).arg(i + 1).arg(chunks.size());
                    return false;
                }

                if (chunkSamples.isEmpty() || chunkSampleRate <= 0) {
                    error = QStringLiteral("VieNeu TTS returned empty audio for clone chunk %1/%2.")
                                .arg(i + 1)
                                .arg(chunks.size());
                    return false;
                }
                if (combinedSampleRate == 0) {
                    combinedSampleRate = chunkSampleRate;
                } else if (combinedSampleRate != chunkSampleRate) {
                    error = QStringLiteral("VieNeu TTS returned inconsistent sample rates across clone chunks.");
                    return false;
                }
                combined.append(chunkSamples);
            }

            if (m_progressCallback) {
                m_progressCallback(chunks.size(), chunks.size(), QStringLiteral("text_chunk"), chunks.size() - 1, chunks.size());
            }

            samples = std::move(combined);
            sampleRate = combinedSampleRate;
            return !samples.isEmpty() && sampleRate > 0;
        }
    }

    {
        auto& vieneu = VieneuTtsInterface::instance();
        if (!vieneu.isLoaded() || !m_context) {
            error = QStringLiteral("VieNeu TTS runtime was unloaded unexpectedly.");
            return false;
        }

        if (m_useAbiV2) {
            const QString synthesisText = ensureSentenceTerminatorForVieneuV3(text);
            QByteArray textBytes = synthesisText.toUtf8();
            QByteArray refPathBytes = QDir::toNativeSeparators(PathUtils::toNativeShortPath(referencePath)).toUtf8();
            QByteArray styleBytes = resolveVieneuV3Style(settings).toUtf8();

            vieneu_audio out;
            memset(&out, 0, sizeof(out));
            if (m_pipelineProfile != QStringLiteral("vieneu-v3-native")) {
                Logger::warning(QStringLiteral("VieneuTtsBackend"),
                                QStringLiteral("VieNeu v3 clone is running with profile=%1, expected vieneu-v3-native.")
                                    .arg(m_pipelineProfile.isEmpty() ? QStringLiteral("<empty>") : m_pipelineProfile));
            }
            int status = -1;
            const bool useNativeAbiV3 = m_pipelineProfile == QStringLiteral("vieneu-v3-native") && vieneu.hasAbiV3();
            if (useNativeAbiV3) {
                vieneu_tts_params_v3 params;
                vieneu.vieneu_tts_v3_default_params(&params);
                params.text = textBytes.constData();
                params.ref_audio_path = refPathBytes.constData();
                params.style = styleBytes.constData();
                if (settings.contains(QStringLiteral("temperature"))) params.temperature = settings.value(QStringLiteral("temperature")).toFloat();
                if (settings.contains(QStringLiteral("top_k"))) params.top_k = settings.value(QStringLiteral("top_k")).toInt();
                if (settings.contains(QStringLiteral("top_p"))) params.top_p = settings.value(QStringLiteral("top_p")).toFloat();
                if (settings.contains(QStringLiteral("max_new_frames"))) params.max_new_frames = settings.value(QStringLiteral("max_new_frames")).toInt();
                if (settings.contains(QStringLiteral("repetition_penalty"))) params.repetition_penalty = settings.value(QStringLiteral("repetition_penalty")).toFloat();
                if (settings.contains(QStringLiteral("max_chars"))) params.max_chars = settings.value(QStringLiteral("max_chars")).toInt();
                if (settings.contains(QStringLiteral("denoise_ref"))) params.denoise_ref = settings.value(QStringLiteral("denoise_ref")).toBool();
                if (settings.contains(QStringLiteral("use_ref_codes"))) params.use_ref_codes = settings.value(QStringLiteral("use_ref_codes")).toBool();
                if (settings.contains(QStringLiteral("apply_watermark"))) params.apply_watermark = settings.value(QStringLiteral("apply_watermark")).toBool();
                const int requestedFrames = params.max_new_frames;
                params.max_new_frames = adaptiveVieneuV3FrameCap(synthesisText, params.max_new_frames);
                if (params.max_new_frames != requestedFrames) {
                    Logger::info(QStringLiteral("VieneuTtsBackend"),
                                 QStringLiteral("Adaptive VieNeu v3 cloned voice frame cap: %1 -> %2")
                                 .arg(requestedFrames)
                                 .arg(params.max_new_frames));
                }
                Logger::info(QStringLiteral("VieneuTtsBackend"),
                             QStringLiteral("VieNeu v3 clone via ABI v3 profile=%1, ref=%2, temperature=%3, top_k=%4, top_p=%5, max_new_frames=%6, repetition_penalty=%7, max_chars=%8")
                                 .arg(m_pipelineProfile)
                                 .arg(referencePath)
                                 .arg(params.temperature)
                                 .arg(params.top_k)
                                 .arg(params.top_p)
                                 .arg(params.max_new_frames)
                                 .arg(params.repetition_penalty)
                                 .arg(params.max_chars));
                status = vieneu.vieneu_synthesize_v3((struct vieneu_context*)m_context, &params, &out);
            } else {
                vieneu_tts_params_v2 params;
                vieneu.vieneu_tts_v2_default_params(&params);
                params.text = textBytes.constData();
                params.ref_audio_path = refPathBytes.constData();
                if (settings.contains(QStringLiteral("temperature"))) params.temperature = settings.value(QStringLiteral("temperature")).toFloat();
                if (settings.contains(QStringLiteral("top_k"))) params.top_k = settings.value(QStringLiteral("top_k")).toInt();
                if (settings.contains(QStringLiteral("top_p"))) params.top_p = settings.value(QStringLiteral("top_p")).toFloat();
                if (settings.contains(QStringLiteral("max_new_frames"))) params.max_new_frames = settings.value(QStringLiteral("max_new_frames")).toInt();
                if (settings.contains(QStringLiteral("repetition_penalty"))) params.repetition_penalty = settings.value(QStringLiteral("repetition_penalty")).toFloat();
                if (settings.contains(QStringLiteral("max_chars"))) params.max_chars = settings.value(QStringLiteral("max_chars")).toInt();
                if (settings.contains(QStringLiteral("apply_watermark"))) params.apply_watermark = settings.value(QStringLiteral("apply_watermark")).toBool();
                const int requestedFrames = params.max_new_frames;
                params.max_new_frames = adaptiveVieneuV3FrameCap(synthesisText, params.max_new_frames);
                if (params.max_new_frames != requestedFrames) {
                    Logger::info(QStringLiteral("VieneuTtsBackend"),
                                 QStringLiteral("Adaptive VieNeu v3 cloned voice frame cap: %1 -> %2")
                                 .arg(requestedFrames)
                                 .arg(params.max_new_frames));
                }
                Logger::info(QStringLiteral("VieneuTtsBackend"),
                             QStringLiteral("VieNeu v3 clone via ABI v2 profile=%1, ref=%2, temperature=%3, top_k=%4, top_p=%5, max_new_frames=%6, repetition_penalty=%7, max_chars=%8")
                                 .arg(m_pipelineProfile)
                                 .arg(referencePath)
                                 .arg(params.temperature)
                                 .arg(params.top_k)
                                 .arg(params.top_p)
                                 .arg(params.max_new_frames)
                                 .arg(params.repetition_penalty)
                                 .arg(params.max_chars));
                status = vieneu.vieneu_synthesize_v2((struct vieneu_context*)m_context, &params, &out);
            }
            if (status != 0) {
                error = QString::fromUtf8(vieneu.vieneu_last_error());
                Logger::error(QStringLiteral("VieneuTtsBackend"), "Failed to synthesize VieNeu v3 cloned voice: " + error);
                return false;
            }
            if (m_cancelRequested.load()) {
                if (out.samples) {
                    vieneu.vieneu_audio_free(&out);
                }
                error = QStringLiteral("TTS synthesis was canceled.");
                return false;
            }
            if (!isValidRuntimeAudio(out)) {
                vieneu.vieneu_audio_free(&out);
                error = QStringLiteral("VieNeu TTS returned an invalid or empty audio buffer.");
                return false;
            }
            samples.resize(out.n_samples);
            memcpy(samples.data(), out.samples, sizeof(float) * out.n_samples);
            sampleRate = out.sample_rate;
            vieneu.vieneu_audio_free(&out);
            return true;
        }

        QByteArray refPathBytes = QDir::toNativeSeparators(PathUtils::toNativeShortPath(referencePath)).toUtf8();
        float embedding[128];
        memset(embedding, 0, sizeof(embedding));

        int rc = vieneu.vieneu_encode_reference((struct vieneu_context*)m_context, refPathBytes.constData(), embedding);
        if (rc != 0) {
            error = QString::fromUtf8(vieneu.vieneu_last_error());
            Logger::error(QStringLiteral("VieneuTtsBackend"), "Failed to encode VieNeu reference audio: " + error);
            return false;
        }

        QByteArray textBytes = text.toUtf8();
        vieneu_tts_params params;
        vieneu.vieneu_tts_default_params(&params);
        params.text = textBytes.constData();
        params.voice_embedding = embedding;

        if (settings.contains(QStringLiteral("temperature"))) params.temperature = settings.value(QStringLiteral("temperature")).toFloat();
        if (settings.contains(QStringLiteral("top_k"))) params.top_k = settings.value(QStringLiteral("top_k")).toInt();
        if (settings.contains(QStringLiteral("max_chars"))) params.max_chars = settings.value(QStringLiteral("max_chars")).toInt();
        if (settings.contains(QStringLiteral("max_tokens"))) params.max_tokens = settings.value(QStringLiteral("max_tokens")).toInt();
        if (settings.contains(QStringLiteral("skip_normalize"))) params.skip_normalize = settings.value(QStringLiteral("skip_normalize")).toBool();
        if (settings.contains(QStringLiteral("skip_phonemize"))) params.skip_phonemize = settings.value(QStringLiteral("skip_phonemize")).toBool();
        if (settings.contains(QStringLiteral("apply_watermark"))) params.apply_watermark = settings.value(QStringLiteral("apply_watermark")).toBool();

        vieneu_audio out;
        memset(&out, 0, sizeof(out));
        int status = vieneu.vieneu_synthesize((struct vieneu_context*)m_context, &params, &out);
        if (status != 0) {
            error = QString::fromUtf8(vieneu.vieneu_last_error());
            Logger::error(QStringLiteral("VieneuTtsBackend"), "Failed to synthesize VieNeu cloned voice: " + error);
            return false;
        }
        if (m_cancelRequested.load()) {
            if (out.samples) {
                vieneu.vieneu_audio_free(&out);
            }
            error = QStringLiteral("TTS synthesis was canceled.");
            return false;
        }
        if (!isValidRuntimeAudio(out)) {
            vieneu.vieneu_audio_free(&out);
            error = QStringLiteral("VieNeu TTS returned an invalid or empty audio buffer.");
            return false;
        }

        samples.resize(out.n_samples);
        memcpy(samples.data(), out.samples, sizeof(float) * out.n_samples);
        sampleRate = out.sample_rate;
        vieneu.vieneu_audio_free(&out);
        Logger::info(QStringLiteral("VieneuTtsBackend"), QString("VieNeu TTS voice cloning successful"));
        return true;
    }

}

} // namespace LAStudio
