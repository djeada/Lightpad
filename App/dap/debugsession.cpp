#include "debugsession.h"
#include "../core/logging/logger.h"
#include "breakpointmanager.h"
#include "debugadapterregistry.h"

namespace {
QString
adapterConfigurationHint(const std::shared_ptr<IDebugAdapter> &adapter) {
  if (!adapter) {
    return {};
  }

  QStringList parts;
  const QString install = adapter->installCommand().trimmed();
  if (!install.isEmpty()) {
    parts.append(QString("Install: %1").arg(install));
  }

  QStringList optionLabels;
  for (const DebugAdapterOption &option : adapter->configurationOptions()) {
    if (!option.label.trimmed().isEmpty()) {
      optionLabels.append(option.label.trimmed());
    }
  }
  optionLabels.removeDuplicates();
  if (!optionLabels.isEmpty()) {
    parts.append(QString("Override in Debug Configurations: %1")
                     .arg(optionLabels.join(", ")));
  }

  return parts.join(". ");
}

QString adapterDiagnostic(const std::shared_ptr<IDebugAdapter> &adapter,
                          const DebugConfiguration &configuration) {
  if (!adapter) {
    return {};
  }

  QString diagnostic = QString("%1: %2").arg(
      adapter->config().name,
      adapter->statusMessageForConfiguration(configuration));
  const QString hint = adapterConfigurationHint(adapter);
  if (!hint.isEmpty()) {
    diagnostic += QString(". %1").arg(hint);
  }
  return diagnostic;
}
} // namespace

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
  m_lastError.clear();
  m_launchRequestSent = false;
  m_adapterInitializedReceived = false;
  m_configurationDoneSent = false;

  if (!m_adapter) {
    m_lastError = "No debug adapter specified";
    emit error("No debug adapter specified");
    return false;
  }

  if (!m_adapter->isAvailableForConfiguration(config)) {
    m_lastError = QString("Debug adapter not available. %1")
                      .arg(adapterDiagnostic(m_adapter, config));
    emit error(m_lastError);
    return false;
  }

  setState(State::Starting);

  DebugAdapterConfig adapterConfig = m_adapter->configForConfiguration(config);
  m_client->setAdapterMetadata(adapterConfig.id, adapterConfig.type);
  if (!m_client->start(adapterConfig.program, adapterConfig.arguments)) {
    m_lastError = "Failed to start debug adapter";
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

  m_client->stop(terminate);
  setState(State::Terminated);
  emit terminated();
}

void DebugSession::restart() {
  if (m_state == State::Idle) {
    return;
  }

  if (!m_client->supportsRestartRequest()) {

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
        LOG_DEBUG("DAP session: adapter already initialized, syncing "
                  "breakpoints before launch");
        BreakpointManager::instance().syncAllBreakpoints();
      }

      if (m_configuration.request == "attach") {
        LOG_DEBUG("DAP session: sending attach request");
        m_client->attach(m_adapter->attachArguments(m_configuration));
      } else {
        LOG_DEBUG("DAP session: sending launch request");
        m_client->launch(m_adapter->launchArguments(m_configuration));
      }
      m_launchRequestSent = true;

      if (adapterReadyForConfiguration && !m_configurationDoneSent) {
        m_client->configurationDone();
        m_configurationDoneSent = true;
      }
    }
    break;

  case DapClient::State::Running:
    LOG_DEBUG("DAP session: client entered Running state");
    setState(State::Running);
    break;

  case DapClient::State::Stopped:
    LOG_DEBUG("DAP session: client entered Stopped state");
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
  LOG_DEBUG(QString("DAP session: initialized event received "
                    "(launchSent=%1, configDone=%2)")
                .arg(m_launchRequestSent)
                .arg(m_configurationDoneSent));
  m_adapterInitializedReceived = true;
  if (m_launchRequestSent && !m_configurationDoneSent) {
    LOG_DEBUG("DAP session: syncing breakpoints and sending configurationDone");
    BreakpointManager::instance().setDapClient(m_client.get());
    BreakpointManager::instance().syncAllBreakpoints();
    m_client->configurationDone();
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
  m_lastError = message;
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
  m_lastError.clear();
  const auto adapters =
      DebugAdapterRegistry::instance().adaptersForConfiguration(config);
  if (adapters.isEmpty()) {
    if (!config.adapterId.trimmed().isEmpty()) {
      m_lastError = QString("No debug adapter found for adapterId: %1")
                        .arg(config.adapterId);
    } else {
      m_lastError =
          QString("No debug adapter found for type: %1").arg(config.type);
    }
    LOG_ERROR(m_lastError);
    return {};
  }

  std::shared_ptr<IDebugAdapter> adapter =
      DebugAdapterRegistry::instance().preferredAdapterForConfiguration(config);
  if (!adapter) {
    QStringList diagnostics;
    for (const auto &candidate : adapters) {
      if (!candidate) {
        continue;
      }
      diagnostics.append(adapterDiagnostic(candidate, config));
    }
    if (diagnostics.isEmpty()) {
      diagnostics.append(
          QString("No available debug adapter for type: %1").arg(config.type));
    }
    m_lastError =
        QString("No available debug adapter. %1").arg(diagnostics.join(" | "));
    LOG_ERROR(m_lastError);
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
            m_lastError = message;
            emit sessionError(sessionId, message);
          });

  if (!session->start(config, adapter)) {
    if (!session->lastError().isEmpty()) {
      m_lastError = session->lastError();
    } else {
      m_lastError = QString("Failed to start debug session '%1'.")
                        .arg(config.name.isEmpty() ? config.type : config.name);
    }
    delete session;
    return {};
  }

  m_lastError.clear();

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
    m_lastError =
        QString("Could not create debug configuration for: %1").arg(filePath);
    LOG_ERROR(m_lastError);
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
