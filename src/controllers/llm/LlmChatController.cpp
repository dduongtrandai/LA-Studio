#include "LlmChatController.h"
#include "llm/LlmChatEngine.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <utility>

namespace LAStudio {
namespace {
QString storePath() { return QDir(PathUtils::dataDir()).filePath(QStringLiteral("llm-conversations.json")); }
}

LlmChatController::LlmChatController(LlmChatEngine *engine, LlmChatModelSession *session, QObject *parent)
    : QObject(parent), m_engine(engine), m_session(session)
{
    if (m_engine) {
        connect(m_engine, &LlmChatEngine::tokenGenerated, this, &LlmChatController::onToken);
        connect(m_engine, &LlmChatEngine::generationFinished, this, &LlmChatController::onFinished);
        connect(m_engine, &LlmChatEngine::generationCancelled, this, &LlmChatController::onCancelled);
        connect(m_engine, &LlmChatEngine::errorOccurred, this, &LlmChatController::onEngineError);
    }
    load();
    ensureActive();
}
void LlmChatController::setSystemPrompt(const QString &v) { if (m_systemPrompt == v) return; m_systemPrompt = v; emit settingsChanged(); persist(); }
void LlmChatController::setContextTokens(int v) { v = qBound(512, v, 131072); if (m_contextTokens == v) return; m_contextTokens = v; emit settingsChanged(); persist(); }
void LlmChatController::setMaxTokens(int v) { v = qBound(1, v, 32768); if (m_maxTokens == v) return; m_maxTokens = v; emit settingsChanged(); persist(); }
void LlmChatController::setTemperature(double v) { v = qBound(0.01, v, 2.0); if (qFuzzyCompare(m_temperature, v)) return; m_temperature = v; emit settingsChanged(); persist(); }
void LlmChatController::setTopP(double v) { v = qBound(0.01, v, 1.0); if (qFuzzyCompare(m_topP, v)) return; m_topP = v; emit settingsChanged(); persist(); }
void LlmChatController::setTopK(int v) { v = qBound(1, v, 200); if (m_topK == v) return; m_topK = v; emit settingsChanged(); persist(); }
void LlmChatController::setRepeatPenalty(double v) { v = qBound(0.8, v, 2.0); if (qFuzzyCompare(m_repeatPenalty, v)) return; m_repeatPenalty = v; emit settingsChanged(); persist(); }

QString LlmChatController::newId() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
void LlmChatController::ensureActive()
{
    if (!m_activeId.isEmpty()) return;
    newConversation();
}
void LlmChatController::newConversation()
{
    if (m_generating) return;
    QVariantMap item{{QStringLiteral("id"), newId()}, {QStringLiteral("title"), QStringLiteral("New chat")},
                     {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    m_conversations.prepend(item);
    m_activeId = item.value(QStringLiteral("id")).toString();
    m_messages.clear();
    m_streamingMessageIndex = -1;
    emit conversationsChanged(); emit messagesChanged(); emit activeConversationChanged(); persist();
}
void LlmChatController::selectConversation(const QString &id)
{
    if (m_generating || id.isEmpty() || id == m_activeId) return;
    for (const QVariant &value : std::as_const(m_conversations)) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("id")).toString() == id) {
            m_activeId = id; m_messages = item.value(QStringLiteral("messages")).toList(); m_streamingMessageIndex = -1;
            m_systemPrompt = item.value(QStringLiteral("systemPrompt")).toString();
            emit activeConversationChanged(); emit messagesChanged(); emit settingsChanged(); return;
        }
    }
}
void LlmChatController::renameConversation(const QString &id, const QString &title)
{
    for (int i = 0; i < m_conversations.size(); ++i) {
        QVariantMap item = m_conversations.at(i).toMap();
        if (item.value(QStringLiteral("id")).toString() == id) { item.insert(QStringLiteral("title"), title.trimmed().isEmpty() ? QStringLiteral("New chat") : title.trimmed()); m_conversations[i] = item; emit conversationsChanged(); persist(); return; }
    }
}
void LlmChatController::deleteConversation(const QString &id)
{
    if (m_generating) return;
    for (int i = 0; i < m_conversations.size(); ++i) if (m_conversations.at(i).toMap().value(QStringLiteral("id")).toString() == id) { m_conversations.removeAt(i); break; }
    if (m_activeId == id) { m_activeId.clear(); m_messages.clear(); m_streamingMessageIndex = -1; ensureActive(); }
    emit conversationsChanged(); emit messagesChanged(); persist();
}
void LlmChatController::clearConversation() { if (m_generating) return; m_messages.clear(); m_streamingMessageIndex = -1; emit messagesChanged(); persist(); }

void LlmChatController::sendMessage(const QString &text)
{
    const QString content = text.trimmed();
    if (content.isEmpty() || m_generating || !m_engine || !m_engine->isModelLoaded()) { if (!m_engine || !m_engine->isModelLoaded()) setError(QStringLiteral("Load an LLM model before sending a message.")); return; }
    QVariantMap user{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), content}, {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    m_messages.append(user);
    QVariantMap assistant{{QStringLiteral("role"), QStringLiteral("assistant")}, {QStringLiteral("content"), QString()}, {QStringLiteral("streaming"), true}};
    m_messages.append(assistant);
    m_streamingMessageIndex = m_messages.size() - 1;
    if (m_messages.size() == 2) { for (int i = 0; i < m_conversations.size(); ++i) { QVariantMap item = m_conversations.at(i).toMap(); if (item.value(QStringLiteral("id")).toString() == m_activeId) { item.insert(QStringLiteral("title"), content.left(48)); m_conversations[i] = item; break; } } emit conversationsChanged(); }
    QVariantList requestMessages;
    if (!m_systemPrompt.trimmed().isEmpty()) requestMessages.append(QVariantMap{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), m_systemPrompt}});
    for (const QVariant &value : m_messages) { const QVariantMap msg = value.toMap(); if (msg.value(QStringLiteral("role")).toString() != QStringLiteral("assistant") || !msg.value(QStringLiteral("streaming")).toBool()) requestMessages.append(msg); }
    QList<QVariantMap> messages; for (const QVariant &value : requestMessages) messages.append(value.toMap());
    m_requestId = newId(); setGenerating(true); m_error.clear(); emit errorTextChanged(); emit messagesChanged(); persist();
    m_engine->generate(messages, m_contextTokens, m_maxTokens, float(m_temperature), float(m_topP), m_topK, float(m_repeatPenalty), m_requestId);
}
void LlmChatController::stopGeneration() { if (m_engine && m_generating) m_engine->cancel(); }
void LlmChatController::regenerateLastResponse()
{
    if (m_generating || m_messages.size() < 2) return;
    const QVariantMap last = m_messages.last().toMap(); if (last.value(QStringLiteral("role")).toString() == QStringLiteral("assistant")) m_messages.removeLast();
    const QVariantMap user = m_messages.last().toMap(); if (user.value(QStringLiteral("role")).toString() == QStringLiteral("user")) { m_messages.removeLast(); sendMessage(user.value(QStringLiteral("content")).toString()); }
}
void LlmChatController::copyMessage(const QString &text) { if (QGuiApplication::clipboard()) QGuiApplication::clipboard()->setText(text); }
void LlmChatController::setError(const QString &message) { if (m_error == message) return; m_error = message; emit errorTextChanged(); emit statusChanged(); }
void LlmChatController::setGenerating(bool value) { if (m_generating == value) return; m_generating = value; emit generatingChanged(); emit statusChanged(); }
void LlmChatController::onToken(const QString &id, const QString &token)
{
    if (id != m_requestId || m_streamingMessageIndex < 0 || m_streamingMessageIndex >= m_messages.size()) return;
    QVariantMap item = m_messages.at(m_streamingMessageIndex).toMap();
    if (item.value(QStringLiteral("role")).toString() != QStringLiteral("assistant")) return;
    item.insert(QStringLiteral("content"), item.value(QStringLiteral("content")).toString() + token);
    m_messages[m_streamingMessageIndex] = item;
    emit messagesChanged();
}
void LlmChatController::onFinished(const QString &id, const QString &text)
{
    if (id != m_requestId) return;
    if (m_streamingMessageIndex >= 0 && m_streamingMessageIndex < m_messages.size()) {
        QVariantMap item = m_messages.at(m_streamingMessageIndex).toMap();
        if (item.value(QStringLiteral("role")).toString() == QStringLiteral("assistant")) {
            item.insert(QStringLiteral("content"), text);
            item.remove(QStringLiteral("streaming"));
            m_messages[m_streamingMessageIndex] = item;
        }
    }
    m_streamingMessageIndex = -1;
    setGenerating(false); persist(); emit messagesChanged();
}
void LlmChatController::onCancelled(const QString &id, const QString &text)
{
    if (id != m_requestId) return;
    if (m_streamingMessageIndex >= 0 && m_streamingMessageIndex < m_messages.size()) {
        QVariantMap item = m_messages.at(m_streamingMessageIndex).toMap();
        if (item.value(QStringLiteral("role")).toString() == QStringLiteral("assistant")) {
            item.insert(QStringLiteral("content"), text);
            item.insert(QStringLiteral("finishReason"), QStringLiteral("cancelled"));
            item.remove(QStringLiteral("streaming"));
            m_messages[m_streamingMessageIndex] = item;
        }
    }
    m_streamingMessageIndex = -1;
    setGenerating(false); persist(); emit messagesChanged();
}
void LlmChatController::onEngineError(const QString &message)
{
    setError(message); setGenerating(false);
    if (m_streamingMessageIndex >= 0 && m_streamingMessageIndex < m_messages.size()
        && m_messages.at(m_streamingMessageIndex).toMap().value(QStringLiteral("role")).toString() == QStringLiteral("assistant")) {
        m_messages.removeAt(m_streamingMessageIndex);
    }
    m_streamingMessageIndex = -1;
    emit messagesChanged(); persist();
}

void LlmChatController::load()
{
    QFile file(storePath()); if (!file.open(QIODevice::ReadOnly)) return;
    QJsonParseError error; const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error); if (error.error != QJsonParseError::NoError || !doc.isObject()) return;
    const QJsonObject root = doc.object(); m_activeId = root.value(QStringLiteral("activeId")).toString(); m_systemPrompt = root.value(QStringLiteral("systemPrompt")).toString(); m_contextTokens = root.value(QStringLiteral("contextTokens")).toInt(m_contextTokens); m_maxTokens = root.value(QStringLiteral("maxTokens")).toInt(m_maxTokens); m_temperature = root.value(QStringLiteral("temperature")).toDouble(m_temperature); m_topP = root.value(QStringLiteral("topP")).toDouble(m_topP); m_topK = root.value(QStringLiteral("topK")).toInt(m_topK); m_repeatPenalty = root.value(QStringLiteral("repeatPenalty")).toDouble(m_repeatPenalty);
    m_conversations = root.value(QStringLiteral("conversations")).toArray().toVariantList();
    for (const QVariant &value : std::as_const(m_conversations)) if (value.toMap().value(QStringLiteral("id")).toString() == m_activeId) { m_messages = value.toMap().value(QStringLiteral("messages")).toList(); break; }
}
void LlmChatController::persist()
{
    for (int i = 0; i < m_conversations.size(); ++i) { QVariantMap item = m_conversations.at(i).toMap(); if (item.value(QStringLiteral("id")).toString() == m_activeId) { item.insert(QStringLiteral("messages"), m_messages); item.insert(QStringLiteral("systemPrompt"), m_systemPrompt); item.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)); m_conversations[i] = item; break; } }
    QJsonObject root; root.insert(QStringLiteral("schemaVersion"), 1); root.insert(QStringLiteral("activeId"), m_activeId); root.insert(QStringLiteral("systemPrompt"), m_systemPrompt); root.insert(QStringLiteral("contextTokens"), m_contextTokens); root.insert(QStringLiteral("maxTokens"), m_maxTokens); root.insert(QStringLiteral("temperature"), m_temperature); root.insert(QStringLiteral("topP"), m_topP); root.insert(QStringLiteral("topK"), m_topK); root.insert(QStringLiteral("repeatPenalty"), m_repeatPenalty); root.insert(QStringLiteral("conversations"), QJsonArray::fromVariantList(m_conversations));
    QSaveFile file(storePath()); if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)); file.commit(); }
}
} // namespace LAStudio
