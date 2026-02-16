#include "debugsession.h"
#include "../core/logging/logger.h"
#include "breakpointmanager.h"
#include "debugadapterregistry.h"

DebugSession::DebugSession(const QString &id, QObject *parent)
    : QObject(parent), m_id(id), m_state(State::Idle),
      m_client(std::make_unique<DapClient>(this)), m_launchRequestSent(false),
      m_adapterInitializedReceived(false), m_configurationDoneSent(false) {

  connect(m_client.get(), &DapClient::stateChanged, this,
          &DebugSession::onClientStateChanged);
  connect(m_client.get(), &DapClient::adapterInitialized, this,
          &DebugSession::onClientAdapterInitialized);
  connect(m_client.get(), &DapClient::stopped, this,
          &DebugSession::onClientStopped);
  connect(m_client.get(), &DapClient::terminated, this,
          &DebugSession::onClientTerminated);
  connect(m_client.get(), &DapClient::output, this,
          &DebugSession::onClientOutput);
  connect(m_client.get(), &DapClient::error, this,
          &DebugSession::onClientError);
}

DebugSession::~DebugSession() {
  if (m_state != State::Idle && m_state != State::Terminated) {
    stop(true);
  }
}

bool DebugSession::start(const DebugConfiguration &config,
                         std::shared_ptr<IDebugAdapter> adapter) {
  if (m_state != State::Idle) {
    LOG_WARNING(QString("Session %1 already started").arg(m_id));
    return false;
  }

  m_configuration = config;
  m_adapter = adapter;
  m_launchRequestSent = false;
  m_adapterInitializedReceived = false;
  m_configurationDoneSent = false;

  if (!m_adapter) {
    emit error("No debug adapter specified");
    return false;
  }

  if (!m_adapter->isAvailable()) {
    emit error(QString("Debug adapter not available: %1")
                   .arg(m_adapter->statusMessage()));
    return false;
  }

  setState(State::Starting);

  DebugAdapterConfig adapterConfig = m_adapter->config();
  m_client->setAdapterMetadata(adapterConfig.id, adapterConfig.type);
  if (!m_client->start(adapterConfig.program, adapterConfig.arguments)) {
    setState(State::Idle);
    emit error("Failed to start debug adapter");
    return false;
  }

  LOG_INFO(QString("Started debug session %1 with adapter %2")
               .arg(m_id)
               .arg(adapterConfig.name));

  return true;
}

void DebugSession::stop(bool terminate) {
  if (m_state == State::Idle || m_state == State::Terminated) {
    return;
  }

  Q_UNUSED(terminate);
  m_client->stop();
  setState(State::Terminated);
  emit terminated();
}

void DebugSession::restart() {
  if (m_state == State::Idle) {
    return;
  }

  if (!m_client->supportsRestartRequest()) {
    // Manual restart path: relaunch adapter process, then re-run launch/attach.
    m_launchRequestSent = false;
    m_adapterInitializedReceived = false;
    m_configurationDoneSent = false;
    setState(State::Starting);
  }

  m_client->restart();
}

void DebugSession::onClientStateChanged(DapClient::State state) {
  switch (state) {
  case DapClient::State::Ready:

    if (m_state == State::Starting) {
      BreakpointManager::instance().setDapClient(m_client.get());

      const bool adapterReadyForConfiguration = m_adapterInitializedReceived;
      if (adapterReadyForConfiguration) {
        BreakpointManager::instance().syncAllBreakpoints();
      }

      if (m_configuration.request == "attach") {
        if (m_configuration.processId > 0) {
          m_client->attach(m_configuration.processId);
        } else if (!m_configuration.host.isEmpty()) {
          m_client->attachRemote(m_configuration.host, m_configuration.port);
        }
      } else {

        m_client->launch(m_configuration.program, m_configuration.args,
                         m_configuration.cwd, m_configuration.env,
                         m_configuration.stopOnEntry);
      }
      m_launchRequestSent = true;

      if (adapterReadyForConfiguration && !m_configurationDoneSent) {
        if (m_client->supportsConfigurationDoneRequest()) {
          m_client->configurationDone();
        }
        m_configurationDoneSent = true;
      }
    }
    break;

  case DapClient::State::Running:
    setState(State::Running);
    break;

  case DapClient::State::Stopped:
    setState(State::Stopped);
    break;

  case DapClient::State::Terminated:
    setState(State::Terminated);
    break;

  case DapClient::State::Error:
    setState(State::Terminated);
    break;

  default:
    break;
  }
}

void DebugSession::onClientAdapterInitialized() {
  m_adapterInitializedReceived = true;
  if (m_launchRequestSent && !m_configurationDoneSent) {
    BreakpointManager::instance().setDapClient(m_client.get());
    BreakpointManager::instance().syncAllBreakpoints();
    if (m_client->supportsConfigurationDoneRequest()) {
      m_client->configurationDone();
    }
    m_configurationDoneSent = true;
  }
}

void DebugSession::onClientStopped(const DapStoppedEvent &event) {
  setState(State::Stopped);
  emit stopped(event);
}

void DebugSession::onClientTerminated() {
  setState(State::Terminated);
  emit terminated();
}

void DebugSession::onClientOutput(const DapOutputEvent &event) {
  emit outputReceived(event);
}

void DebugSession::onClientError(const QString &message) {
  emit error(message);
}

void DebugSession::setState(State state) {
  if (m_state != state) {
    m_state = state;
    emit stateChanged(state);

    if (state == State::Running || state == State::Stopped) {
      emit started();
    }
  }
}

DebugSessionManager &DebugSessionManager::instance() {
  static DebugSessionManager instance;
  return instance;
}

DebugSessionManager::DebugSessionManager()
    : QObject(nullptr), m_nextSessionNumber(1) {}

QString DebugSessionManager::startSession(const DebugConfiguration &config) {

  auto adapters = DebugAdapterRegistry::instance().adaptersForType(config.type);
  if (adapters.isEmpty()) {
    auto explicitAdapter =
        DebugAdapterRegistry::instance().adapter(config.type);
    if (explicitAdapter) {
      adapters.append(explicitAdapter);
    }
  }
  if (adapters.isEmpty()) {
    LOG_ERROR(QString("No debug adapter found for type: %1").arg(config.type));
    return {};
  }

  std::shared_ptr<IDebugAdapter> adapter;
  for (const auto &a : adapters) {
    if (a->isAvailable()) {
      adapter = a;
      break;
    }
  }

  if (!adapter) {
    LOG_ERROR(
        QString("No available debug adapter for type: %1").arg(config.type));
    return {};
  }

  return startSession(config, adapter);
}

QString
DebugSessionManager::startSession(const DebugConfiguration &config,
                                  std::shared_ptr<IDebugAdapter> adapter) {
  QString sessionId = generateSessionId();

  DebugSession *session = new DebugSession(sessionId, this);

  connect(session, &DebugSession::stateChanged, this,
          &DebugSessionManager::onSessionStateChanged);
  connect(session, &DebugSession::terminated, this,
          &DebugSessionManager::onSessionTerminated);
  connect(session, &DebugSession::stopped, this,
          [this, sessionId](const DapStoppedEvent &event) {
            emit sessionStopped(sessionId, event);
          });
  connect(session, &DebugSession::error, this,
          [this, sessionId](const QString &message) {
            emit sessionError(sessionId, message);
          });

  if (!session->start(config, adapter)) {
    delete session;
    return {};
  }

  m_sessions[sessionId] = session;

  if (m_focusedSessionId.isEmpty()) {
    setFocusedSession(sessionId);
  }

  emit sessionStarted(sessionId);

  return sessionId;
}

QString DebugSessionManager::quickStart(const QString &filePath,
                                        const QString &languageId) {
  DebugConfiguration config =
      DebugConfigurationManager::instance().createQuickConfig(filePath,
                                                              languageId);
  if (config.name.isEmpty()) {
    LOG_ERROR(
        QString("Could not create debug configuration for: %1").arg(filePath));
    return {};
  }

  return startSession(config);
}

void DebugSessionManager::stopSession(const QString &sessionId,
                                      bool terminate) {
  auto it = m_sessions.find(sessionId);
  if (it == m_sessions.end()) {
    return;
  }

  it.value()->stop(terminate);
}

void DebugSessionManager::stopAllSessions(bool terminate) {
  for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
    it.value()->stop(terminate);
  }
}

DebugSession *DebugSessionManager::session(const QString &sessionId) const {
  auto it = m_sessions.find(sessionId);
  if (it != m_sessions.end()) {
    return it.value();
  }
  return nullptr;
}

QList<DebugSession *> DebugSessionManager::allSessions() const {
  return m_sessions.values();
}

DebugSession *DebugSessionManager::focusedSession() const {
  return session(m_focusedSessionId);
}

void DebugSessionManager::setFocusedSession(const QString &sessionId) {
  if (m_focusedSessionId != sessionId) {
    m_focusedSessionId = sessionId;
    emit focusedSessionChanged(sessionId);
  }
}

bool DebugSessionManager::hasActiveSessions() const {
  for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
    DebugSession::State state = it.value()->state();
    if (state != DebugSession::State::Idle &&
        state != DebugSession::State::Terminated) {
      return true;
    }
  }
  return false;
}

void DebugSessionManager::onSessionStateChanged(DebugSession::State state) {
  Q_UNUSED(state);
}

void DebugSessionManager::onSessionTerminated() {
  DebugSession *senderSession = qobject_cast<DebugSession *>(this->sender());
  if (!senderSession) {
    return;
  }

  QString sessionId = senderSession->id();

  QMetaObject::invokeMethod(
      this,
      [this, sessionId, senderSession]() {
        m_sessions.remove(sessionId);
        delete senderSession;
        emit sessionTerminated(sessionId);

        if (m_focusedSessionId == sessionId) {
          if (m_sessions.isEmpty()) {
            m_focusedSessionId.clear();
            emit allSessionsEnded();
          } else {
            setFocusedSession(m_sessions.firstKey());
          }
        }
      },
      Qt::QueuedConnection);
}

QString DebugSessionManager::generateSessionId() {
  return QString("session-%1").arg(m_nextSessionNumber++);
}
