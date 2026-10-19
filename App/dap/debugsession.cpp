#include "debugsession.h"
#include "../core/logging/logger.h"
#include "breakpointmanager.h"
#include "debugadapterregistry.h"

#include <QFileInfo>

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
  connect(m_client.get(), &DapClient::exceptionInfoReceived, this,
          &DebugSession::exceptionInfoReceived);
  connect(m_client.get(), &DapClient::processChanged, this,
          [this](int pid, const QString &) { m_processId = pid; });
  auto &breakpoints = BreakpointManager::instance();
  connect(&breakpoints, &BreakpointManager::fileBreakpointsChanged, this,
          [this](const QString &file) {
            if (m_adapterInitializedReceived && m_client->isReady() &&
                BreakpointManager::instance().dapClient() != m_client.get())
              syncSourceBreakpoints(file);
          });
  connect(&breakpoints, &BreakpointManager::allBreakpointsCleared, this,
          [this]() {
            if (m_client->isReady() &&
                BreakpointManager::instance().dapClient() != m_client.get()) {
              for (const QString &file : std::as_const(m_breakpointFiles))
                m_client->setBreakpoints(file, {});
            }
            m_breakpointFiles.clear();
          });
  connect(&breakpoints, &BreakpointManager::functionBreakpointsChanged, this,
          [this]() {
            if (m_adapterInitializedReceived && m_client->isReady() &&
                BreakpointManager::instance().dapClient() != m_client.get())
              syncFunctionBreakpoints();
          });
  connect(&breakpoints, &BreakpointManager::exceptionBreakpointsChanged, this,
          [this]() {
            if (m_adapterInitializedReceived && m_client->isReady() &&
                BreakpointManager::instance().dapClient() != m_client.get())
              m_client->setExceptionBreakpoints(
                  BreakpointManager::instance().enabledExceptionFilters());
          });
}

DebugSession::~DebugSession() {
  if (m_state != State::Idle && m_state != State::Terminated) {
    stop(true);
  }
}

bool DebugSession::start(const DebugConfiguration &config,
                         std::shared_ptr<IDebugAdapter> adapter,
                         const QString &host, quint16 port) {
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
  const bool started =
      !host.isEmpty() && port > 0 ? m_client->startSocket(host, port)
      : adapterConfig.serverTransport
          ? m_client->startServer(adapterConfig.program,
                                  adapterConfig.arguments)
          : m_client->start(adapterConfig.program, adapterConfig.arguments);
  if (!started) {
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
  reportTermination();
}

void DebugSession::restart() {
  if (m_state == State::Idle) {
    return;
  }

  if (!m_client->supportsRestartRequest()) {

    m_launchRequestSent = false;
    m_adapterInitializedReceived = false;
    m_configurationDoneSent = false;
    m_terminationReported = false;
    setState(State::Starting);
    m_restarting = true;
  }

  m_client->restart();
  m_restarting = false;

  const DapClient::State clientState = m_client->state();
  if (clientState == DapClient::State::Error ||
      clientState == DapClient::State::Disconnected) {
    setState(State::Terminated);
    reportTermination();
  }
}

void DebugSession::onClientStateChanged(DapClient::State state) {
  if (m_restarting) {
    return;
  }

  switch (state) {
  case DapClient::State::Ready:

    if (m_state == State::Starting) {
      const bool launchesFirst =
          m_adapter && m_adapter->launchesBeforeInitialized();

      if (m_adapterInitializedReceived) {

        startConfigurationSequence();
      } else if (launchesFirst) {

        LOG_DEBUG("DAP session: sending early launch/attach for "
                  "launch-first adapter");
        sendLaunchOrAttach();
      } else {
        LOG_DEBUG("DAP session: waiting for initialized event before "
                  "configuration");
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
  case DapClient::State::Error:
  case DapClient::State::Disconnected:
    if (m_state == State::Idle) {
      break;
    }
    setState(State::Terminated);
    QMetaObject::invokeMethod(
        this,
        [this]() {
          if (m_state == State::Terminated && !m_restarting) {
            reportTermination();
          }
        },
        Qt::QueuedConnection);
    break;

  default:
    break;
  }
}

void DebugSession::onClientAdapterInitialized() {
  if (m_restarting) {
    return;
  }
  LOG_DEBUG(QString("DAP session: initialized event received "
                    "(launchSent=%1, configDone=%2)")
                .arg(m_launchRequestSent)
                .arg(m_configurationDoneSent));
  m_adapterInitializedReceived = true;

  if (m_launchRequestSent) {

    syncSessionBreakpoints();
    if (!m_configurationDoneSent) {
      m_client->configurationDone();
      m_configurationDoneSent = true;
    }
    return;
  }

  startConfigurationSequence();
}

void DebugSession::startConfigurationSequence() {
  if (m_configurationDoneSent && m_launchRequestSent) {
    return;
  }

  syncSessionBreakpoints();

  if (!m_configurationDoneSent) {
    m_client->configurationDone();
    m_configurationDoneSent = true;
  }

  sendLaunchOrAttach();
}

void DebugSession::syncSourceBreakpoints(const QString &filePath) {
  QList<DapSourceBreakpoint> source;
  for (const Breakpoint &bp :
       BreakpointManager::instance().breakpointsForFile(filePath)) {
    if (!bp.enabled)
      continue;
    DapSourceBreakpoint entry;
    entry.line = bp.line;
    entry.column = bp.column;
    entry.condition = bp.condition;
    entry.hitCondition = bp.hitCondition;
    entry.logMessage = bp.logMessage;
    source << entry;
  }
  m_breakpointFiles.insert(filePath);
  m_client->setBreakpoints(filePath, source);
}

void DebugSession::syncFunctionBreakpoints() {
  QStringList names;
  for (const auto &bp : BreakpointManager::instance().allFunctionBreakpoints())
    if (bp.enabled)
      names << bp.functionName;
  m_client->setFunctionBreakpoints(names);
}

void DebugSession::syncSessionBreakpoints() {
  auto &manager = BreakpointManager::instance();
  for (const auto &bp : manager.allBreakpoints())
    m_breakpointFiles.insert(bp.filePath);
  if (manager.dapClient() == m_client.get()) {
    manager.syncAllBreakpoints();
  } else {
    for (const QString &file : std::as_const(m_breakpointFiles))
      syncSourceBreakpoints(file);
    syncFunctionBreakpoints();
    m_client->setExceptionBreakpoints(manager.enabledExceptionFilters());
  }
}

void DebugSession::sendLaunchOrAttach() {
  if (m_launchRequestSent) {
    return;
  }

  if (m_configuration.request == "attach") {
    LOG_DEBUG("DAP session: sending attach request");
    m_client->attach(m_adapter ? m_adapter->attachArguments(m_configuration)
                               : QJsonObject{});
  } else {
    LOG_DEBUG("DAP session: sending launch request");
    m_client->launch(m_adapter ? m_adapter->launchArguments(m_configuration)
                               : QJsonObject{});
  }
  m_launchRequestSent = true;
}

void DebugSession::onClientStopped(const DapStoppedEvent &event) {
  setState(State::Stopped);
  emit stopped(event);

  if (event.reason == DapStoppedReason::Exception && m_client) {
    m_client->exceptionInfo(event.threadId);
  }
}

void DebugSession::onClientTerminated() {
  if (m_restarting) {
    return;
  }
  setState(State::Terminated);
  reportTermination();
}

void DebugSession::reportTermination() {

  if (m_terminationReported) {
    return;
  }
  m_terminationReported = true;
  emit terminated();
}

void DebugSession::onClientOutput(const DapOutputEvent &event) {
  if (event.category != QLatin1String("telemetry")) {
    m_outputHistory += event.output;
    if (m_outputHistory.size() > 131072)
      m_outputHistory = m_outputHistory.right(131072);
  }
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
  return startSessionImpl(config, adapter, {});
}

QString DebugSessionManager::startSessionImpl(
    const DebugConfiguration &config, std::shared_ptr<IDebugAdapter> adapter,
    const QString &parentId, const QString &host, quint16 port) {
  QString sessionId = generateSessionId();

  DebugSession *session = new DebugSession(sessionId, this);
  session->m_parentSessionId = parentId;
  connect(session, &DebugSession::outputReceived, this,
          [this, sessionId]() { emit sessionOutputChanged(sessionId); });
  connect(
      session->client(), &DapClient::childSessionRequested, this,
      [this, sessionId, client = session->client()](
          int seq, const QString &request, const QJsonObject &configuration) {
        const QString child =
            startChildSession(sessionId, request, configuration);
        if (seq >= 0)
          client->respondToStartDebugging(seq, !child.isEmpty(), m_lastError);
      });
  connect(session->client(), &DapClient::processChanged, this,
          [this]() { emit sessionsChanged(); });

  connect(session, &DebugSession::stateChanged, this,
          &DebugSessionManager::onSessionStateChanged);
  connect(session, &DebugSession::terminated, this,
          &DebugSessionManager::onSessionTerminated);
  connect(session, &DebugSession::stopped, this,
          [this, sessionId](const DapStoppedEvent &event) {
            DebugSession *focused = focusedSession();
            if (!focused || focused->state() != DebugSession::State::Stopped)
              setFocusedSession(sessionId);
            emit sessionStopped(sessionId, event);
          });
  connect(session, &DebugSession::error, this,
          [this, sessionId](const QString &message) {
            m_lastError = message;
            emit sessionError(sessionId, message);
          });

  if (!session->start(config, adapter, host, port)) {
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
  emit sessionsChanged();

  return sessionId;
}

QString DebugSessionManager::quickStart(const QString &filePath,
                                        const QString &languageId,
                                        const QString &programOverride) {
  DebugConfiguration config =
      DebugConfigurationManager::instance().createQuickConfig(filePath,
                                                              languageId);
  if (!programOverride.isEmpty()) {
    config.program = programOverride;
    if (config.cwd.isEmpty()) {
      config.cwd = QFileInfo(programOverride).absolutePath();
    }
  }
  if (config.name.isEmpty()) {
    m_lastError =
        QString("Could not create debug configuration for: %1").arg(filePath);
    LOG_ERROR(m_lastError);
    return {};
  }

  return startSession(config);
}

QString
DebugSessionManager::startChildSession(const QString &parentId,
                                       const QString &request,
                                       const QJsonObject &configuration) {
  DebugSession *parent = session(parentId);
  if (!parent || !parent->adapter() ||
      (request != "attach" && request != "launch")) {
    m_lastError = "Invalid child debug session request";
    return {};
  }
  const int pid = configuration.value("subProcessId").toInt();
  if (pid > 0) {
    for (DebugSession *existing : allSessions())
      if (existing->parentSessionId() == parentId &&
          existing->configuration()
                  .adapterConfig.value("subProcessId")
                  .toInt() == pid)
        return existing->id();
  }
  if (m_sessions.size() >= 128) {
    m_lastError = "Debug session limit reached (128)";
    emit sessionError(parentId, m_lastError);
    return {};
  }
  DebugConfiguration child = parent->configuration();
  child.request = request;
  child.name = configuration.value("name").toString(
      pid > 0 ? QString("Process %1").arg(pid) : "Child process");
  child.adapterConfig = configuration;

  if (parent->client()->adapterType() == "debugpy" &&
      !child.adapterConfig.contains("redirectOutput"))
    child.adapterConfig["redirectOutput"] = true;
  child.processId = 0;
  child.processIdExpression.clear();
  child.stopOnEntry = false;
  const QJsonObject connection = configuration.value("connect").toObject();
  QString host;
  int port = 0;
  if (request == "attach" && !connection.isEmpty()) {
    host = connection.value("host").toString("127.0.0.1");
    port = connection.value("port").toInt();
    if (host.isEmpty() || port < 1 || port > 65535) {
      m_lastError = "Child adapter returned an invalid connection";
      emit sessionError(parentId, m_lastError);
      return {};
    }
  }
  return startSessionImpl(child, parent->adapter(), parentId, host,
                          static_cast<quint16>(port));
}

void DebugSessionManager::stopSession(const QString &sessionId,
                                      bool terminate) {

  DebugSession *session = m_sessions.value(sessionId);
  if (!session) {
    return;
  }

  for (DebugSession *child : allSessions())
    if (child->parentSessionId() == sessionId)
      stopSession(child->id(), terminate);
  session->stop(terminate);
}

void DebugSessionManager::stopAllSessions(bool terminate) {
  const QList<DebugSession *> sessions = m_sessions.values();
  for (DebugSession *session : sessions) {
    session->stop(terminate);
  }
}

void DebugSessionManager::continueAllSessions() {
  for (DebugSession *session : allSessions())
    if (session->state() == DebugSession::State::Stopped ||
        session->client()->hasPendingPause())
      session->continueExecution();
}

void DebugSessionManager::pauseAllSessions() {
  for (DebugSession *session : allSessions())
    if (session->state() == DebugSession::State::Running ||
        session->state() == DebugSession::State::Stopped)
      session->client()->pauseAllThreads();
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
  if (!sessionId.isEmpty() && !m_sessions.contains(sessionId))
    return;
  if (m_focusedSessionId != sessionId) {
    m_focusedSessionId = sessionId;
    DebugSession *focused = session(sessionId);
    BreakpointManager::instance().setDapClient(focused ? focused->client()
                                                       : nullptr);
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
  emit sessionsChanged();
}

void DebugSessionManager::onSessionTerminated() {
  DebugSession *senderSession = qobject_cast<DebugSession *>(this->sender());
  if (!senderSession) {
    return;
  }

  const QString sessionId = senderSession->id();

  if (m_sessions.value(sessionId) != senderSession) {
    return;
  }

  m_recentSessions.prepend({sessionId, senderSession->parentSessionId(),
                            senderSession->name(), senderSession->processId(),
                            senderSession->outputHistory()});
  while (m_recentSessions.size() > 20)
    m_recentSessions.removeLast();
  m_sessions.remove(sessionId);
  emit sessionsChanged();
  senderSession->disconnect(this);

  senderSession->deleteLater();

  QMetaObject::invokeMethod(
      this,
      [this, sessionId]() {
        emit sessionTerminated(sessionId);

        if (m_focusedSessionId == sessionId) {
          if (m_sessions.isEmpty()) {
            m_focusedSessionId.clear();
            BreakpointManager::instance().setDapClient(nullptr);
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
