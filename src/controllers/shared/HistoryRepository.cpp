#include "HistoryRepository.h"

#include "core/PathUtils.h"
#include "audio/WavIO.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>
#include <algorithm>

namespace LAStudio {

QVariantList HistoryRepository::loadTtsHistory(const QString &dataDir)
{
    QVariantList list;
    QString historyPath = dataDir + QStringLiteral("/history/tts_history.json");
    QFile file(historyPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return list;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return list;
    }
    QJsonArray arr = doc.array();
    list.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        if (val.isObject()) {
            list.append(val.toObject().toVariantMap());
        }
    }
    return list;
}

bool HistoryRepository::addTtsHistoryItem(const QString &dataDir,
                                          const QString &text,
                                          const QString &modelName,
                                          const QString &voiceName,
                                          const QVector<float> &samples,
                                          int sampleRate,
                                          QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString audioDir = historyDir + QStringLiteral("/audio");
    QDir().mkpath(audioDir);

    QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString fileName = id + QStringLiteral(".wav");
    QString filePath = audioDir + QStringLiteral("/") + fileName;

    // Save the WAV file
    bool ok = WavIO::saveFloat(filePath, samples.constData(),
                                samples.size(), sampleRate);
    if (!ok) {
        errorMsg = QStringLiteral("Failed to save history audio file");
        return false;
    }

    // Compute duration
    double seconds = static_cast<double>(samples.size()) / sampleRate;
    QString durationText = QString::number(seconds, 'f', 1) + QStringLiteral("s");

    // Timestamp
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));

    // Create JSON object
    QJsonObject item;
    item[QStringLiteral("id")] = id;
    item[QStringLiteral("text")] = text;
    item[QStringLiteral("timestamp")] = timestamp;
    item[QStringLiteral("filePath")] = QUrl::fromLocalFile(filePath).toString();
    item[QStringLiteral("modelName")] = modelName;
    item[QStringLiteral("voiceName")] = voiceName;
    item[QStringLiteral("durationText")] = durationText;
    item[QStringLiteral("sampleRate")] = sampleRate;

    // Read current JSON list, prepend item
    QJsonArray arr;
    QString historyPath = historyDir + QStringLiteral("/tts_history.json");
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    arr.prepend(item);

    // Save back to JSON file
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write TTS history JSON");
        return false;
    }
    QJsonDocument doc(arr);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool HistoryRepository::deleteTtsHistoryItem(const QString &dataDir, const QString &id, QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString historyPath = historyDir + QStringLiteral("/tts_history.json");

    QJsonArray arr;
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    QJsonArray newArr;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        if (obj[QStringLiteral("id")].toString() == id) {
            QString pathUrl = obj[QStringLiteral("filePath")].toString();
            QString localPath = PathUtils::urlToLocalPath(pathUrl);
            QFile::remove(localPath);
        } else {
            newArr.append(obj);
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write TTS history JSON during deletion");
        return false;
    }
    QJsonDocument doc(newArr);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool HistoryRepository::clearTtsHistory(const QString &dataDir, QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString historyPath = historyDir + QStringLiteral("/tts_history.json");

    QJsonArray arr;
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        QString pathUrl = obj[QStringLiteral("filePath")].toString();
        QString localPath = PathUtils::urlToLocalPath(pathUrl);
        QFile::remove(localPath);
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write TTS history JSON during clear");
        return false;
    }
    QJsonDocument doc((QJsonArray()));
    file.write(doc.toJson());
    file.close();
    return true;
}

QVariantList HistoryRepository::loadSttHistory(const QString &dataDir)
{
    QVariantList list;
    QString historyPath = dataDir + QStringLiteral("/history/stt_history.json");
    QFile file(historyPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return list;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return list;
    }
    QJsonArray arr = doc.array();
    list.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        if (val.isObject()) {
            list.append(val.toObject().toVariantMap());
        }
    }
    return list;
}

bool HistoryRepository::addSttHistoryItem(const QString &dataDir,
                                          const QString &text,
                                          const QString &modelName,
                                          const QVector<float> &samples,
                                          QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString audioDir = historyDir + QStringLiteral("/stt_audio");
    QDir().mkpath(audioDir);

    QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString fileName = id + QStringLiteral(".wav");
    QString filePath = audioDir + QStringLiteral("/") + fileName;

    QVector<int16_t> pcm16(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        float s = std::clamp(samples[i], -1.0f, 1.0f);
        pcm16[i] = static_cast<int16_t>(s * 32767.0f);
    }

    bool ok = WavIO::savePcm16(filePath, pcm16.constData(), pcm16.size(), 16000, 1);
    if (!ok) {
        errorMsg = QStringLiteral("Failed to save history audio file");
        return false;
    }

    double seconds = static_cast<double>(samples.size()) / 16000.0;

    QString durationText = QString::number(seconds, 'f', 1) + QStringLiteral("s");
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));

    QJsonObject item;
    item[QStringLiteral("id")] = id;
    item[QStringLiteral("text")] = text;
    item[QStringLiteral("timestamp")] = timestamp;
    item[QStringLiteral("filePath")] = QUrl::fromLocalFile(filePath).toString();
    item[QStringLiteral("modelName")] = modelName;
    item[QStringLiteral("durationText")] = durationText;

    QJsonArray arr;
    QString historyPath = historyDir + QStringLiteral("/stt_history.json");
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    arr.prepend(item);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write STT history JSON");
        return false;
    }
    QJsonDocument doc(arr);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool HistoryRepository::deleteSttHistoryItem(const QString &dataDir, const QString &id, QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString historyPath = historyDir + QStringLiteral("/stt_history.json");

    QJsonArray arr;
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    QJsonArray newArr;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        if (obj[QStringLiteral("id")].toString() == id) {
            QString pathUrl = obj[QStringLiteral("filePath")].toString();
            QString localPath = PathUtils::urlToLocalPath(pathUrl);
            QFile::remove(localPath);
        } else {
            newArr.append(obj);
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write STT history JSON during deletion");
        return false;
    }
    QJsonDocument doc(newArr);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool HistoryRepository::clearSttHistory(const QString &dataDir, QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString historyPath = historyDir + QStringLiteral("/stt_history.json");

    QJsonArray arr;
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        QString pathUrl = obj[QStringLiteral("filePath")].toString();
        QString localPath = PathUtils::urlToLocalPath(pathUrl);
        QFile::remove(localPath);
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write STT history JSON during clear");
        return false;
    }
    QJsonDocument doc((QJsonArray()));
    file.write(doc.toJson());
    file.close();
    return true;
}

QVariantList HistoryRepository::loadVoiceDesignHistory(const QString &dataDir)
{
    QVariantList list;
    QString historyPath = dataDir + QStringLiteral("/history/voice_design_history.json");
    QFile file(historyPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return list;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return list;
    }
    QJsonArray arr = doc.array();
    list.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        if (val.isObject()) {
            list.append(val.toObject().toVariantMap());
        }
    }
    return list;
}

bool HistoryRepository::addVoiceDesignHistoryItem(const QString &dataDir,
                                                  const QString &text,
                                                  const QString &voiceDescription,
                                                  const QString &presetName,
                                                  const QString &familyId,
                                                  const QString &modelName,
                                                  const QVector<float> &samples,
                                                  int sampleRate,
                                                  QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString audioDir = historyDir + QStringLiteral("/voice_design_audio");
    QDir().mkpath(audioDir);

    QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString fileName = id + QStringLiteral(".wav");
    QString filePath = audioDir + QStringLiteral("/") + fileName;

    // Save the WAV file
    bool ok = WavIO::saveFloat(filePath, samples.constData(),
                                samples.size(), sampleRate);
    if (!ok) {
        errorMsg = QStringLiteral("Failed to save history audio file");
        return false;
    }

    // Compute duration
    double seconds = static_cast<double>(samples.size()) / sampleRate;
    QString durationText = QString::number(seconds, 'f', 1) + QStringLiteral("s");

    // Timestamp
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));

    // Create JSON object
    QJsonObject item;
    item[QStringLiteral("id")] = id;
    item[QStringLiteral("text")] = text;
    item[QStringLiteral("voiceDescription")] = voiceDescription;
    item[QStringLiteral("presetName")] = presetName;
    item[QStringLiteral("familyId")] = familyId;
    item[QStringLiteral("modelName")] = modelName;
    item[QStringLiteral("filePath")] = QUrl::fromLocalFile(filePath).toString();
    item[QStringLiteral("durationText")] = durationText;
    item[QStringLiteral("sampleRate")] = sampleRate;
    item[QStringLiteral("timestamp")] = timestamp;

    // Read current JSON list, prepend item
    QJsonArray arr;
    QString historyPath = historyDir + QStringLiteral("/voice_design_history.json");
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    arr.prepend(item);

    // Save back to JSON file
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write Voice Design history JSON");
        return false;
    }
    QJsonDocument doc(arr);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool HistoryRepository::deleteVoiceDesignHistoryItem(const QString &dataDir, const QString &id, QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString historyPath = historyDir + QStringLiteral("/voice_design_history.json");

    QJsonArray arr;
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    QJsonArray newArr;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        if (obj[QStringLiteral("id")].toString() == id) {
            QString pathUrl = obj[QStringLiteral("filePath")].toString();
            QString localPath = PathUtils::urlToLocalPath(pathUrl);
            QFile::remove(localPath);
        } else {
            newArr.append(obj);
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write Voice Design history JSON during deletion");
        return false;
    }
    QJsonDocument doc(newArr);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool HistoryRepository::clearVoiceDesignHistory(const QString &dataDir, QString &errorMsg)
{
    QString historyDir = dataDir + QStringLiteral("/history");
    QString historyPath = historyDir + QStringLiteral("/voice_design_history.json");

    QJsonArray arr;
    QFile file(historyPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            arr = doc.array();
        }
        file.close();
    }

    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        QString pathUrl = obj[QStringLiteral("filePath")].toString();
        QString localPath = PathUtils::urlToLocalPath(pathUrl);
        QFile::remove(localPath);
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Failed to write Voice Design history JSON during clear");
        return false;
    }
    QJsonDocument doc((QJsonArray()));
    file.write(doc.toJson());
    file.close();
    return true;
}

} // namespace LAStudio
