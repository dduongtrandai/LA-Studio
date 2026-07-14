#include "StudioActions.h"
#include "TtsSharedModelSession.h"

namespace LAStudio {

CapabilityStudioAction::CapabilityStudioAction(const QString &capabilityId, IModelSession *session, QObject *parent)
    : IStudioAction(parent)
    , m_capabilityId(capabilityId)
    , m_session(session)
{
    if (m_session) {
        connect(m_session, &IModelSession::stateChanged, this, &CapabilityStudioAction::stateChanged);
        connect(m_session, &IModelSession::activeConfigurationChanged, this, &CapabilityStudioAction::activeConfigurationChanged);
        connect(m_session, &IModelSession::errorOccurred, this, [this](const QString &msg) {
            m_errorDetail = msg;
            emit errorOccurred(msg);
        });
    }
}

QString CapabilityStudioAction::activeConfigurationSignature() const
{
    if (auto *ttsSession = qobject_cast<TtsSharedModelSession*>(m_session)) {
        if (!ttsSession->isOwnerActive(m_capabilityId)) {
            return QString();
        }
    }
    return m_session ? m_session->activeSignature() : QString();
}

StudioState CapabilityStudioAction::state() const
{
    if (!m_session) return StudioState::Unloaded;
    if (auto *ttsSession = qobject_cast<TtsSharedModelSession*>(m_session)) {
        if (!ttsSession->isOwnerActive(m_capabilityId) &&
            !ttsSession->isOwnerPending(m_capabilityId))
        {
            return StudioState::Unloaded;
        }
    }
    switch (m_session->state()) {
    case ModelSessionState::Unconfigured: return StudioState::Unloaded;
    case ModelSessionState::Unloaded: return StudioState::Unloaded;
    case ModelSessionState::Loading: return StudioState::Loading;
    case ModelSessionState::Unloading: return StudioState::Unloading;
    case ModelSessionState::Ready: return StudioState::Ready;
    case ModelSessionState::Processing: return StudioState::Processing;
    case ModelSessionState::Error: return StudioState::Error;
    }
    return StudioState::Unloaded;
}

QString CapabilityStudioAction::errorDetail() const
{
    return m_errorDetail;
}

void CapabilityStudioAction::load(const StudioConfiguration &configuration)
{
    m_errorDetail.clear();
    if (m_session) {
        m_session->requestLoad(m_capabilityId, configuration);
    }
}

void CapabilityStudioAction::unload()
{
    m_errorDetail.clear();
    if (m_session) {
        m_session->requestUnload(m_capabilityId);
    }
}

void CapabilityStudioAction::reload()
{
    m_errorDetail.clear();
    if (m_session) {
        m_session->requestReload(m_capabilityId);
    }
}

} // namespace LAStudio
