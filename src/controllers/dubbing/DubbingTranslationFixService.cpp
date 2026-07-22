#include "controllers/dubbing/DubbingTranslationFixService.h"

#include "core/Logger.h"
#include "core/PathUtils.h"
#include "dubbing/DubbingDuration.h"
#include "dubbing/EspeakNgPhonemizer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

namespace LAStudio {
namespace {

QString settingsPath()
{
    return PathUtils::dataDir() + QStringLiteral("/settings.ini");
}

QString normalizedServerBase(QString value)
{
    value = value.trimmed();
    while (value.endsWith(QLatin1Char('/'))) value.chop(1);
    const QStringList suffixes = {
        QStringLiteral("/api/v1/chat"),
        QStringLiteral("/v1/chat/completions"),
        QStringLiteral("/api/v1"),
        QStringLiteral("/v1")
    };
    for (const QString &suffix : suffixes) {
        if (!value.endsWith(suffix, Qt::CaseInsensitive)) continue;
        value.chop(suffix.size());
        break;
    }
    return value;
}

int actualPhonemeCount(const QVariantMap &segment, const QString &language)
{
    return EspeakNgPhonemizer::count(
        segment.value(QStringLiteral("targetText")).toString(), language);
}

bool isOverBudget(const QVariantMap &segment, const QString &language)
{
    const QVariantMap budget = segment.value(QStringLiteral("durationBudget")).toMap();
    if (budget.isEmpty()
        || segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty())
        return false;
    const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
    const int phonemes = actualPhonemeCount(segment, language);
    if (phonemes < 0) return false;
    return phonemes > maximum;
}

int distanceToBudget(int phonemes, int minimum, int maximum)
{
    if (phonemes < minimum) return minimum - phonemes;
    if (phonemes > maximum) return phonemes - maximum;
    return 0;
}

QString responseError(const QByteArray &body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) return QString::fromUtf8(body).trimmed();
    const QJsonValue error = document.object().value(QStringLiteral("error"));
    if (error.isString()) return error.toString();
    if (error.isObject())
        return error.toObject().value(QStringLiteral("message")).toString();
    return {};
}

} // namespace

DubbingTranslationFixService::DubbingTranslationFixService(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    m_configuration = normalizedConfiguration({
        {QStringLiteral("serverUrl"),
         settings.value(QStringLiteral("dubbing/translationFixServerUrl"),
                        QStringLiteral("http://127.0.0.1:1234")).toString()},
        {QStringLiteral("model"),
         settings.value(QStringLiteral("dubbing/translationFixModel"),
                        QStringLiteral("qwen3.5-2b")).toString()},
        {QStringLiteral("apiKey"),
         settings.value(QStringLiteral("dubbing/translationFixApiKey"),
                        QString()).toString()},
        {QStringLiteral("maxAttempts"),
         settings.value(QStringLiteral("dubbing/translationFixMaxAttempts"), 4).toInt()},
        {QStringLiteral("temperature"),
         settings.value(QStringLiteral("dubbing/translationFixTemperature"), 0.35).toDouble()}
    });
}

QVariantMap DubbingTranslationFixService::normalizedConfiguration(
    const QVariantMap &configuration)
{
    QVariantMap result;
    result.insert(QStringLiteral("serverUrl"),
                  configuration.value(QStringLiteral("serverUrl"),
                                      QStringLiteral("http://127.0.0.1:1234"))
                      .toString().trimmed());
    result.insert(QStringLiteral("model"),
                  configuration.value(QStringLiteral("model"),
                                      QStringLiteral("qwen3.5-2b"))
                      .toString().trimmed());
    result.insert(QStringLiteral("apiKey"),
                  configuration.value(QStringLiteral("apiKey")).toString().trimmed());
    result.insert(QStringLiteral("maxAttempts"),
                  qBound(1, configuration.value(QStringLiteral("maxAttempts"), 4).toInt(), 8));
    result.insert(QStringLiteral("temperature"),
                  qBound(0.0, configuration.value(QStringLiteral("temperature"), 0.35).toDouble(), 1.5));
    return result;
}

QUrl DubbingTranslationFixService::chatUrl(const QString &serverUrl)
{
    return QUrl(normalizedServerBase(serverUrl)
                + QStringLiteral("/api/v1/chat"));
}

QUrl DubbingTranslationFixService::modelsUrl(const QString &serverUrl)
{
    return QUrl(normalizedServerBase(serverUrl) + QStringLiteral("/api/v1/models"));
}

QString DubbingTranslationFixService::cleanAssistantText(const QString &content)
{
    QString result = content.trimmed();
    result.remove(QRegularExpression(QStringLiteral("<think>.*?</think>"),
                                     QRegularExpression::DotMatchesEverythingOption
                                         | QRegularExpression::CaseInsensitiveOption));
    result = result.trimmed();
    if (result.startsWith(QStringLiteral("```"))) {
        result.remove(QRegularExpression(QStringLiteral("^```(?:text|json)?\\s*"),
                                         QRegularExpression::CaseInsensitiveOption));
        result.remove(QRegularExpression(QStringLiteral("\\s*```$")));
    }
    result.remove(QRegularExpression(
        QStringLiteral("^(?:translation|revised translation|bản dịch|câu viết lại)\\s*:\\s*"),
        QRegularExpression::CaseInsensitiveOption));
    result = result.trimmed();
    if (result.size() >= 2
        && ((result.startsWith(QLatin1Char('"')) && result.endsWith(QLatin1Char('"')))
            || (result.startsWith(QChar(0x201c)) && result.endsWith(QChar(0x201d)))))
        result = result.mid(1, result.size() - 2).trimmed();
    return result;
}

int DubbingTranslationFixService::eligibleSegmentCount(
    const QVariantList &segments, const QString &targetLanguage)
{
    int count = 0;
    for (const QVariant &value : segments) {
        if (isOverBudget(value.toMap(), targetLanguage)) ++count;
    }
    return count;
}

bool DubbingTranslationFixService::isCloserToBudget(
    int currentPhonemes, int candidatePhonemes, int minimum, int maximum)
{
    return distanceToBudget(candidatePhonemes, minimum, maximum)
        < distanceToBudget(currentPhonemes, minimum, maximum);
}

void DubbingTranslationFixService::saveConfiguration()
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("dubbing/translationFixServerUrl"),
                      m_configuration.value(QStringLiteral("serverUrl")));
    settings.setValue(QStringLiteral("dubbing/translationFixModel"),
                      m_configuration.value(QStringLiteral("model")));
    settings.setValue(QStringLiteral("dubbing/translationFixApiKey"),
                      m_configuration.value(QStringLiteral("apiKey")));
    settings.setValue(QStringLiteral("dubbing/translationFixMaxAttempts"),
                      m_configuration.value(QStringLiteral("maxAttempts")));
    settings.setValue(QStringLiteral("dubbing/translationFixTemperature"),
                      m_configuration.value(QStringLiteral("temperature")));
    settings.sync();
}

bool DubbingTranslationFixService::start(
    const QString &sourceLanguage, const QString &targetLanguage,
    const QVariantList &segments, const QVariantMap &configuration,
    int segmentIndex)
{
    if (m_busy || m_testing) return false;
    m_configuration = normalizedConfiguration(configuration.isEmpty()
                                                  ? m_configuration : configuration);
    const QUrl endpoint = chatUrl(
        m_configuration.value(QStringLiteral("serverUrl")).toString());
    if (!endpoint.isValid() || endpoint.host().isEmpty()) {
        setError(QStringLiteral("LM Studio server URL is invalid."));
        return false;
    }
    if (m_configuration.value(QStringLiteral("model")).toString().isEmpty()) {
        setError(QStringLiteral("LM Studio model identifier is required."));
        return false;
    }

    m_segments = segments;
    m_sourceLanguage = sourceLanguage;
    m_targetLanguage = targetLanguage;
    m_eligibleIndices.clear();
    for (int i = 0; i < m_segments.size(); ++i) {
        if (segmentIndex >= 0 && i != segmentIndex) continue;
        const QVariantMap segment = m_segments.at(i).toMap();
        if (actualPhonemeCount(segment, targetLanguage) < 0) {
            setError(QStringLiteral(
                "eSpeak NG is unavailable, so translated phonemes cannot be verified."));
            return false;
        }
        if (isOverBudget(segment, targetLanguage)) m_eligibleIndices.append(i);
    }
    if (m_eligibleIndices.isEmpty()) {
        setError(segmentIndex >= 0
                     ? QStringLiteral("This translation does not exceed its phoneme limit.")
                     : QStringLiteral("No translated segment exceeds its phoneme limit."));
        return false;
    }

    saveConfiguration();
    m_maxAttempts = m_configuration.value(QStringLiteral("maxAttempts")).toInt();
    m_segmentPosition = 0;
    m_fixedCount = 0;
    m_improvedCount = 0;
    m_unresolvedCount = 0;
    m_lastError.clear();
    setProgress(0);
    setBusy(true);
    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("Starting LM Studio rewrite endpoint=%1 model=%2 segments=%3 selectedIndex=%4 maxAttempts=%5 targetLanguage=%6")
            .arg(endpoint.toString(),
                 m_configuration.value(QStringLiteral("model")).toString())
            .arg(m_eligibleIndices.size()).arg(segmentIndex).arg(m_maxAttempts)
            .arg(targetLanguage));
    beginSegment();
    return true;
}

void DubbingTranslationFixService::testConnection(
    const QVariantMap &configuration)
{
    if (m_busy || m_testing) return;
    m_configuration = normalizedConfiguration(configuration.isEmpty()
                                                  ? m_configuration : configuration);
    const QUrl endpoint = modelsUrl(
        m_configuration.value(QStringLiteral("serverUrl")).toString());
    if (!endpoint.isValid() || endpoint.host().isEmpty()) {
        emit connectionTested(false, QStringLiteral("LM Studio server URL is invalid."));
        return;
    }
    saveConfiguration();
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LA-Studio"));
    request.setRawHeader("Accept", "application/json");
    const QString apiKey = m_configuration.value(QStringLiteral("apiKey")).toString();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(10000);
    m_testing = true;
    emit stateChanged();
    QNetworkReply *pending = m_network->get(request);
    m_reply = pending;
    connect(pending, &QNetworkReply::finished, this, [this, pending]() {
        if (m_reply == pending) m_reply = nullptr;
        if (!m_testing) {
            pending->deleteLater();
            return;
        }
        m_testing = false;
        QNetworkReply *reply = pending;
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString model = m_configuration.value(QStringLiteral("model")).toString();
        bool found = false;
        const QJsonArray models = QJsonDocument::fromJson(body).object()
                                      .value(QStringLiteral("models")).toArray();
        for (const QJsonValue &value : models) {
            const QJsonObject modelObject = value.toObject();
            if (modelObject.value(QStringLiteral("key")).toString() == model) {
                found = true;
                break;
            }
            const QJsonArray instances =
                modelObject.value(QStringLiteral("loaded_instances")).toArray();
            for (const QJsonValue &instance : instances) {
                if (instance.toObject().value(QStringLiteral("id")).toString() == model) {
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        const bool success = reply->error() == QNetworkReply::NoError
            && status >= 200 && status < 300 && found;
        QString message;
        if (success)
            message = QStringLiteral("Connected. Model \"%1\" is available.").arg(model);
        else if (reply->error() != QNetworkReply::NoError)
            message = QStringLiteral("Connection failed: %1").arg(reply->errorString());
        else if (!found)
            message = QStringLiteral("Connected, but model \"%1\" was not listed by LM Studio.")
                          .arg(model);
        else
            message = QStringLiteral("LM Studio returned HTTP %1: %2")
                          .arg(status).arg(responseError(body));
        Logger::info(QStringLiteral("DubbingTranslationFix"),
                     QStringLiteral("Connection test success=%1 endpoint=%2 model=%3 message=%4")
                         .arg(success ? QStringLiteral("true") : QStringLiteral("false"),
                              reply->url().toString(), model, message));
        reply->deleteLater();
        emit stateChanged();
        emit connectionTested(success, message);
    });
}

void DubbingTranslationFixService::cancel()
{
    if (!m_busy && !m_testing) return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_testing = false;
    if (m_busy) {
        setStatus(QStringLiteral("Translation fix cancelled."));
        setBusy(false);
    } else {
        emit stateChanged();
    }
    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
}

void DubbingTranslationFixService::clearError()
{
    if (m_lastError.isEmpty()) return;
    m_lastError.clear();
    emit stateChanged();
}

void DubbingTranslationFixService::beginSegment()
{
    if (!m_busy) return;
    if (m_segmentPosition >= m_eligibleIndices.size()) {
        finishRun();
        return;
    }
    const QVariantMap segment =
        m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
    m_originalTranslation =
        segment.value(QStringLiteral("targetText")).toString().trimmed();
    m_promptTranslation = m_originalTranslation;
    m_lastCandidate.clear();
    m_bestCandidate.clear();
    m_seenCandidates.clear();
    m_lastCandidatePhonemes = actualPhonemeCount(segment, m_targetLanguage);
    m_bestCandidatePhonemes = m_lastCandidatePhonemes;
    m_promptPhonemes = m_lastCandidatePhonemes;
    m_attempt = 0;
    setStatus(QStringLiteral("Fixing segment %1 of %2")
                  .arg(m_segmentPosition + 1).arg(m_eligibleIndices.size()));
    requestAttempt();
}

void DubbingTranslationFixService::requestAttempt()
{
    if (!m_busy) return;
    const QVariantMap segment =
        m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
    QJsonObject payload;
    payload.insert(QStringLiteral("model"),
                   m_configuration.value(QStringLiteral("model")).toString());
    payload.insert(QStringLiteral("temperature"),
                   m_configuration.value(QStringLiteral("temperature")).toDouble());
    payload.insert(QStringLiteral("max_output_tokens"), 384);
    payload.insert(QStringLiteral("stream"), false);
    payload.insert(QStringLiteral("store"), false);
    payload.insert(QStringLiteral("reasoning"), QStringLiteral("off"));
    payload.insert(QStringLiteral("top_p"), 0.8);
    payload.insert(QStringLiteral("top_k"), 20);
    payload.insert(QStringLiteral("system_prompt"),
                   QStringLiteral(
                       "You repair translations for timed dubbing. Preserve the complete source "
                       "meaning and the meaning of the current translation: facts, names, numbers, "
                       "rank/order, time, comparison, causality, and negation. Rewrite naturally in "
                       "the requested target language while meeting the supplied eSpeak NG phoneme "
                       "maximum. Never invent or omit information. Return only the rewritten "
                       "translation, without analysis, labels, quotes, or a phoneme count."));
    payload.insert(QStringLiteral("input"), buildPrompt(segment));

    QNetworkRequest request(chatUrl(
        m_configuration.value(QStringLiteral("serverUrl")).toString()));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LA-Studio"));
    request.setRawHeader("Accept", "application/json");
    const QString apiKey = m_configuration.value(QStringLiteral("apiKey")).toString();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(120000);

    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("Request segment=%1 attempt=%2/%3 currentPhonemes=%4")
            .arg(segment.value(QStringLiteral("id")).toString())
            .arg(m_attempt + 1).arg(m_maxAttempts).arg(m_lastCandidatePhonemes));
    QNetworkReply *pending = m_network->post(
        request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_reply = pending;
    connect(pending, &QNetworkReply::finished, this, [this, pending]() {
        if (m_reply == pending) m_reply = nullptr;
        handleAttemptResponse(pending);
    });
}

void DubbingTranslationFixService::handleAttemptResponse(QNetworkReply *reply)
{
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!m_busy) {
        reply->deleteLater();
        return;
    }
    if (reply->error() != QNetworkReply::NoError
        || status < 200 || status >= 300) {
        const QString detail = reply->error() != QNetworkReply::NoError
            ? reply->errorString() : responseError(body);
        reply->deleteLater();
        setError(QStringLiteral("LM Studio request failed (HTTP %1): %2")
                     .arg(status).arg(detail));
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    QString content;
    const QJsonArray output = root.value(QStringLiteral("output")).toArray();
    for (const QJsonValue &value : output) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("type")).toString()
            == QStringLiteral("message")) {
            content = item.value(QStringLiteral("content")).toString();
            if (!content.isEmpty()) break;
        }
    }
    const QString candidate = cleanAssistantText(content);
    reply->deleteLater();
    if (candidate.isEmpty()) {
        setError(QStringLiteral("LM Studio returned an empty translation."));
        return;
    }

    const QString candidateKey = candidate.simplified().toCaseFolded();
    if (m_seenCandidates.contains(candidateKey)) {
        ++m_attempt;
        Logger::warning(QStringLiteral("DubbingTranslationFix"),
                        QStringLiteral("Repeated rewrite rejected at attempt %1/%2")
                            .arg(m_attempt).arg(m_maxAttempts));
        if (m_attempt < m_maxAttempts) {
            requestAttempt();
            return;
        }
        if (!m_bestCandidate.isEmpty()) {
            const QVariantMap current =
                m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
            QVariantMap improved = current;
            applyCandidate(improved, m_bestCandidate, m_bestCandidatePhonemes, false);
            m_segments[m_eligibleIndices.at(m_segmentPosition)] = improved;
            finishSegment(false, true);
        } else {
            finishSegment(false);
        }
        return;
    }
    m_seenCandidates.insert(candidateKey);

    const QVariantMap segment =
        m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
    const QVariantMap budget =
        segment.value(QStringLiteral("durationBudget")).toMap();
    const int phonemes = EspeakNgPhonemizer::count(candidate, m_targetLanguage);
    if (phonemes < 0) {
        setError(QStringLiteral(
            "eSpeak NG became unavailable while validating the rewritten translation."));
        return;
    }
    const QStringList tokens = protectedTokens(
        segment.value(QStringLiteral("sourceText")).toString());
    const bool tokensPreserved = preservesProtectedTokens(candidate, tokens);
    const double semanticScore =
        DubbingDurationPlanner::semanticFidelityScore(
            m_originalTranslation, candidate);
    const bool semanticGuardPassed = semanticScore >= 0.25;
    const bool withinBudget =
        phonemes >= budget.value(QStringLiteral("minUnits")).toInt()
        && phonemes <= budget.value(QStringLiteral("maxUnits")).toInt();
    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("Response segment=%1 attempt=%2 chars=%3 phonemes=%4 range=%5-%6 withinBudget=%7 protectedTokens=%8 semanticScore=%9 semanticGuard=%10")
            .arg(segment.value(QStringLiteral("id")).toString())
            .arg(m_attempt + 1).arg(candidate.size()).arg(phonemes)
            .arg(budget.value(QStringLiteral("minUnits")).toInt())
            .arg(budget.value(QStringLiteral("maxUnits")).toInt())
            .arg(withinBudget ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(tokensPreserved ? QStringLiteral("preserved")
                                 : QStringLiteral("missing"))
            .arg(semanticScore, 0, 'f', 3)
            .arg(semanticGuardPassed ? QStringLiteral("passed")
                                     : QStringLiteral("rejected")));

    m_lastCandidate = candidate;
    m_lastCandidatePhonemes = phonemes;
    ++m_attempt;
    if (withinBudget && tokensPreserved && semanticGuardPassed) {
        QVariantMap accepted = segment;
        applyCandidate(accepted, candidate, phonemes, true);
        m_segments[m_eligibleIndices.at(m_segmentPosition)] = accepted;
        finishSegment(true);
        return;
    }

    const int minimum = budget.value(QStringLiteral("minUnits")).toInt();
    const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
    if (tokensPreserved && semanticGuardPassed
        && isCloserToBudget(m_bestCandidatePhonemes, phonemes, minimum, maximum)) {
        m_bestCandidate = candidate;
        m_bestCandidatePhonemes = phonemes;
        m_promptTranslation = candidate;
        m_promptPhonemes = phonemes;
    }
    if (m_attempt < m_maxAttempts) {
        requestAttempt();
        return;
    }
    if (!m_bestCandidate.isEmpty()) {
        QVariantMap improved = segment;
        applyCandidate(improved, m_bestCandidate, m_bestCandidatePhonemes, false);
        m_segments[m_eligibleIndices.at(m_segmentPosition)] = improved;
        Logger::info(
            QStringLiteral("DubbingTranslationFix"),
            QStringLiteral("Keeping closest safe rewrite segment=%1 phonemes=%2 range=%3-%4")
                .arg(segment.value(QStringLiteral("id")).toString())
                .arg(m_bestCandidatePhonemes).arg(minimum).arg(maximum));
        finishSegment(false, true);
        return;
    }
    finishSegment(false);
}

void DubbingTranslationFixService::finishSegment(bool fixed, bool improved)
{
    if (fixed) ++m_fixedCount;
    else {
        ++m_unresolvedCount;
        if (improved) ++m_improvedCount;
    }
    ++m_segmentPosition;
    setProgress(qRound(m_segmentPosition * 100.0
                       / qMax(1, m_eligibleIndices.size())));
    beginSegment();
}

void DubbingTranslationFixService::finishRun()
{
    setProgress(100);
    setStatus(QStringLiteral("Fixed %1 segment(s); improved %2; %3 still need review.")
                  .arg(m_fixedCount).arg(m_improvedCount).arg(m_unresolvedCount));
    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("Rewrite completed fixed=%1 improved=%2 unresolved=%3 total=%4")
            .arg(m_fixedCount).arg(m_improvedCount).arg(m_unresolvedCount)
            .arg(m_eligibleIndices.size()));
    setBusy(false);
    emit completed(m_segments, m_fixedCount, m_unresolvedCount);
}

QString DubbingTranslationFixService::buildPrompt(
    const QVariantMap &segment) const
{
    const QVariantMap budget =
        segment.value(QStringLiteral("durationBudget")).toMap();
    const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
    const QString direction = m_promptPhonemes > maximum
        ? QStringLiteral("Shorten the wording without dropping any source meaning.")
        : QStringLiteral("Keep the wording inside the required range.");
    QString feedback;
    if (m_attempt > 0) {
        feedback = QStringLiteral(
            "\nThe previous rewrite had %1 phonemes and did not pass validation. "
            "Use a different construction and correct the length.")
                       .arg(m_lastCandidatePhonemes);
    }
    const QString tokens = protectedTokens(
        segment.value(QStringLiteral("sourceText")).toString())
                               .join(QStringLiteral(", "));
    return QStringLiteral(
               "Source language: %1\nTarget language: %2\n"
               "Original source:\n%3\n\n"
               "Faithful current translation:\n%4\n\n"
               "Rewrite starting point:\n%5\n\n"
               "External eSpeak NG measurement: %6 phonemes.\n"
               "Required range: %7-%8 phonemes; ideal target: %9 phonemes.\n"
               "Protected tokens that must remain exactly unchanged: %10\n"
               "Keep the translation as semantically faithful as possible. %11%12")
        .arg(m_sourceLanguage, m_targetLanguage,
             segment.value(QStringLiteral("sourceText")).toString(),
             m_originalTranslation, m_promptTranslation)
        .arg(m_promptPhonemes)
        .arg(budget.value(QStringLiteral("minUnits")).toInt())
        .arg(budget.value(QStringLiteral("maxUnits")).toInt())
        .arg(budget.value(QStringLiteral("targetUnits")).toInt())
        .arg(tokens.isEmpty() ? QStringLiteral("(none)") : tokens,
             direction, feedback);
}

QStringList DubbingTranslationFixService::protectedTokens(
    const QString &text) const
{
    QStringList result;
    const QRegularExpression expression(
        QStringLiteral("(?:https?://\\S+|\\b\\d[\\d.,/%-]*|\\b[A-Z]{2,}\\b)"));
    auto matches = expression.globalMatch(text);
    while (matches.hasNext()) result.append(matches.next().captured(0));
    result.removeDuplicates();
    return result;
}

bool DubbingTranslationFixService::preservesProtectedTokens(
    const QString &candidate, const QStringList &tokens) const
{
    for (const QString &token : tokens) {
        if (!candidate.contains(token, Qt::CaseInsensitive)) return false;
    }
    return true;
}

void DubbingTranslationFixService::applyCandidate(
    QVariantMap &segment, const QString &candidate, int phonemes,
    bool withinBudget) const
{
    const QVariantMap budget =
        segment.value(QStringLiteral("durationBudget")).toMap();
    if (!segment.contains(QStringLiteral("referenceTranslation")))
        segment.insert(QStringLiteral("referenceTranslation"), m_originalTranslation);
    segment.insert(QStringLiteral("targetText"), candidate);
    segment.insert(QStringLiteral("durationUnits"), phonemes);
    segment.insert(QStringLiteral("phonemeDistance"),
                   qAbs(phonemes - budget.value(QStringLiteral("targetUnits")).toInt()));
    segment.insert(QStringLiteral("durationStatus"), withinBudget
                       ? QStringLiteral("within-budget")
                       : QStringLiteral("needs-review"));
    segment.insert(QStringLiteral("durationMetric"), QStringLiteral("phoneme-distance"));
    segment.insert(QStringLiteral("candidateSelectionMetric"), withinBudget
                       ? QStringLiteral("lm-studio-qwen-rewrite-v1")
                       : QStringLiteral("lm-studio-closest-safe-rewrite-v1"));
    segment.insert(QStringLiteral("rewriteProvider"), QStringLiteral("lm-studio"));
    segment.insert(QStringLiteral("rewriteModel"),
                   m_configuration.value(QStringLiteral("model")));
    segment.insert(QStringLiteral("rewriteAttempts"), m_attempt);
    segment.insert(QStringLiteral("targetChunks"),
                   DubbingDurationPlanner::pauseChunks(
                       candidate, budget.value(QStringLiteral("pauses")).toList()));
    segment.insert(QStringLiteral("pauseAligned"), true);
    segment.insert(QStringLiteral("pauseAlignmentMethod"),
                   QStringLiteral("deterministic-even-split-v1"));
    segment.insert(QStringLiteral("state"), QStringLiteral("translated"));
}

void DubbingTranslationFixService::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit stateChanged();
}

void DubbingTranslationFixService::setProgress(int progress)
{
    const int normalized = qBound(0, progress, 100);
    if (m_progress == normalized) return;
    m_progress = normalized;
    emit stateChanged();
}

void DubbingTranslationFixService::setStatus(const QString &status)
{
    if (m_statusText == status) return;
    m_statusText = status;
    emit stateChanged();
}

void DubbingTranslationFixService::setError(const QString &message)
{
    m_lastError = message;
    m_statusText = message;
    Logger::error(QStringLiteral("DubbingTranslationFix"), message);
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    setBusy(false);
    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
    emit stateChanged();
    emit failed(message);
}

} // namespace LAStudio
