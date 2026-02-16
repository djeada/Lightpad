#include "dapclient.h"
#include "../core/logging/logger.h"

#include <QDir>
#include <QJsonDocument>
#include <QTimer>

namespace {

constexpr int MAX_MESSAGE_PARSE_ITERATIONS = 100;

constexpr int MAX_DAP_BUFFER_BYTES = 4 * 1024 * 1024;

constexpr int MAX_DAP_MESSAGE_BYTES = 2 * 1024 * 1024;

constexpr int MAX_PENDING_REQUESTS = 2048;

int lastHeaderStart(const QByteArray &buffer) {
  return buffer.lastIndexOf("Content-Length:");
}
} // namespace

DapClient::DapClient(QObject *parent)
    : QObject(parent), m_process(nullptr), m_state(State::Disconnected),
      m_nextSeq(1), m_currentThreadId(0),
      m_functionBreakpointsConfigured(false),
      m_hasDeferredFunctionBreakpoints(false), m_dataBreakpointsSupported(true),
      m_dataBreakpointsConfigured(false), m_isAttach(false) {}

DapClient::~DapClient() { stop(); }

void DapClient::setAdapterMetadata(const QString &adapterId,
                                   const QString &adapterType) {
  m_adapterId = adapterId.trimmed();
  m_adapterType = adapterType.trimmed();
}

bool DapClient::start(const QString &program, const QStringList &arguments) {
  if (m_process) {
    LOG_WARNING("DAP client already started");
    return false;
  }

  m_process = new QProcess(this);
  m_process->setProgram(program);
  m_process->setArguments(arguments);

  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &DapClient::onReadyReadStandardOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &DapClient::onReadyReadStandardError);
  connect(m_process, &QProcess::errorOccurred, this,
          &DapClient::onProcessError);
  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &DapClient::onProcessFinished);

  setState(State::Connecting);
  m_process->start();

  if (!m_process->waitForStarted(5000)) {
    LOG_ERROR(QString("Failed to start debug adapter: %1").arg(program));
    setState(State::Error);
    emit error("Failed to start debug adapter");
    delete m_process;
    m_process = nullptr;
    return false;
  }

  LOG_INFO(QString("Started debug adapter: %1").arg(program));
  doInitialize();

  return true;
}

void DapClient::stop() {
  if (!m_process) {
    return;
  }

  m_process->disconnect(this);

  if (isDebugging()) {

    QJsonObject args;
    args["terminateDebuggee"] = true;
    sendRequest("disconnect", args, m_nextSeq++);
  }

  QProcess *proc = m_process;
  m_process = nullptr;

  if (proc->state() != QProcess::NotRunning) {
    proc->terminate();

    QTimer::singleShot(2000, proc, [proc]() {
      if (proc->state() != QProcess::NotRunning) {
        proc->kill();
      }
      proc->deleteLater();
    });
  } else {
    proc->deleteLater();
  }

  m_buffer.clear();
  m_pendingRequests.clear();
  m_currentThreadId = 0;
  m_capabilities = QJsonObject();
  m_functionBreakpointsConfigured = false;
  m_deferredFunctionBreakpoints.clear();
  m_hasDeferredFunctionBreakpoints = false;
  m_dataBreakpointsSupported = true;
  m_dataBreakpointsConfigured = false;
  setState(State::Disconnected);

  LOG_INFO("Debug adapter stopped");
}

DapClient::State DapClient::state() const { return m_state; }

bool DapClient::isReady() const {
  return m_state == State::Ready || m_state == State::Running ||
         m_state == State::Stopped;
}

bool DapClient::isDebugging() const {
  return m_state == State::Running || m_state == State::Stopped;
}

void DapClient::launch(const QString &program, const QStringList &args,
                       const QString &cwd, const QMap<QString, QString> &env,
                       bool stopOnEntry) {
  if (!isReady()) {
    LOG_WARNING("DAP: Cannot launch, client not ready");
    return;
  }

  QJsonObject arguments;
  arguments["program"] = program;

  if (!args.isEmpty()) {
    QJsonArray argsArray;
    for (const auto &arg : args) {
      argsArray.append(arg);
    }
    arguments["args"] = argsArray;
  }

  if (!cwd.isEmpty()) {
    arguments["cwd"] = cwd;
  }

  if (!env.isEmpty()) {
    QJsonObject envObj;
    for (auto it = env.begin(); it != env.end(); ++it) {
      envObj[it.key()] = it.value();
    }
    arguments["env"] = envObj;
  }

  arguments["stopOnEntry"] = stopOnEntry;

  arguments["stopAtBeginningOfMainSubprogram"] = stopOnEntry;

  m_launchConfig = arguments;
  m_isAttach = false;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "launch";
  sendRequest("launch", arguments, seq);
}

void DapClient::attach(int processId) {
  if (!isReady()) {
    LOG_WARNING("DAP: Cannot attach, client not ready");
    return;
  }

  QJsonObject arguments;
  arguments["processId"] = processId;

  m_launchConfig = arguments;
  m_isAttach = true;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "attach";
  sendRequest("attach", arguments, seq);
}

void DapClient::attachRemote(const QString &host, int port) {
  if (!isReady()) {
    LOG_WARNING("DAP: Cannot attach, client not ready");
    return;
  }

  QJsonObject arguments;
  arguments["host"] = host;
  arguments["port"] = port;

  m_launchConfig = arguments;
  m_isAttach = true;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "attach";
  sendRequest("attach", arguments, seq);
}

void DapClient::disconnect(bool terminateDebuggee) {
  QJsonObject args;
  args["terminateDebuggee"] = terminateDebuggee;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "disconnect";
  sendRequest("disconnect", args, seq);
}

void DapClient::terminate() {
  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "terminate";
  sendRequest("terminate", {}, seq);
}

void DapClient::configurationDone() {
  if (!supportsConfigurationDoneRequest()) {
    LOG_DEBUG("DAP: Skipping configurationDone (adapter does not request it)");
    return;
  }
  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "configurationDone";
  sendRequest("configurationDone", {}, seq);
}

bool DapClient::supportsConfigurationDoneRequest() const {
  return m_capabilities.value("supportsConfigurationDoneRequest").toBool(false);
}

bool DapClient::supportsRestartRequest() const {
  return m_capabilities.value("supportsRestartRequest").toBool(false);
}

void DapClient::setBreakpoints(const QString &sourcePath,
                               const QList<DapSourceBreakpoint> &breakpoints) {
  QJsonObject source;
  source["path"] = sourcePath;

  QJsonArray bpArray;
  for (const auto &bp : breakpoints) {
    bpArray.append(bp.toJson());
  }

  QJsonObject args;
  args["source"] = source;
  args["breakpoints"] = bpArray;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = QString("setBreakpoints:%1").arg(sourcePath);
  sendRequest("setBreakpoints", args, seq);
}

void DapClient::setFunctionBreakpoints(const QStringList &functionNames) {

  if (m_state == State::Running) {
    m_deferredFunctionBreakpoints = functionNames;
    m_hasDeferredFunctionBreakpoints = true;
    LOG_DEBUG("DAP: Deferring function breakpoint sync until stopped");
    return;
  }

  if (functionNames.isEmpty() && !m_functionBreakpointsConfigured) {
    return;
  }

  QJsonArray bpArray;
  for (const auto &name : functionNames) {
    QJsonObject bp;
    bp["name"] = name;
    bpArray.append(bp);
  }

  QJsonObject args;
  args["breakpoints"] = bpArray;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "setFunctionBreakpoints";
  sendRequest("setFunctionBreakpoints", args, seq);
  m_functionBreakpointsConfigured = !functionNames.isEmpty();
}

void DapClient::setDataBreakpoints(const QList<QJsonObject> &dataBreakpoints) {

  if (dataBreakpoints.isEmpty() && !m_dataBreakpointsConfigured) {
    return;
  }

  if (!m_dataBreakpointsSupported) {
    LOG_DEBUG("DAP: Skipping data breakpoints request (not supported)");
    return;
  }

  QJsonArray bpArray;
  for (const auto &bp : dataBreakpoints) {
    bpArray.append(bp);
  }

  QJsonObject args;
  args["breakpoints"] = bpArray;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "setDataBreakpoints";
  sendRequest("setDataBreakpoints", args, seq);
  m_dataBreakpointsConfigured = !dataBreakpoints.isEmpty();
}

void DapClient::setExceptionBreakpoints(const QStringList &filterIds) {
  QJsonArray filters;
  for (const auto &id : filterIds) {
    filters.append(id);
  }

  QJsonObject args;
  args["filters"] = filters;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "setExceptionBreakpoints";
  sendRequest("setExceptionBreakpoints", args, seq);
}

void DapClient::continueExecution(int threadId) {
  clearPendingInspectionRequests();

  QJsonObject args;
  args["threadId"] = threadId > 0 ? threadId : m_currentThreadId;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "continue";
  sendRequest("continue", args, seq);
}

void DapClient::pause(int threadId) {
  QJsonObject args;
  args["threadId"] = threadId > 0 ? threadId : m_currentThreadId;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "pause";
  sendRequest("pause", args, seq);
}

void DapClient::stepOver(int threadId) {
  clearPendingInspectionRequests();

  QJsonObject args;
  args["threadId"] = threadId > 0 ? threadId : m_currentThreadId;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "next";
  sendRequest("next", args, seq);
}

void DapClient::stepInto(int threadId) {
  clearPendingInspectionRequests();

  QJsonObject args;
  args["threadId"] = threadId > 0 ? threadId : m_currentThreadId;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "stepIn";
  sendRequest("stepIn", args, seq);
}

void DapClient::stepOut(int threadId) {
  clearPendingInspectionRequests();

  QJsonObject args;
  args["threadId"] = threadId > 0 ? threadId : m_currentThreadId;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "stepOut";
  sendRequest("stepOut", args, seq);
}

void DapClient::restart() {
  if (supportsRestartRequest()) {
    int seq = m_nextSeq++;
    m_pendingRequests[seq] = "restart";
    sendRequest("restart", m_launchConfig, seq);
  } else {
    if (!m_process) {
      LOG_WARNING("DAP: Cannot restart, adapter process is not running");
      emit error("Restart failed: debug adapter is not running");
      return;
    }

    const QString adapterProgram = m_process->program();
    const QStringList adapterArguments = m_process->arguments();

    stop();
    if (!start(adapterProgram, adapterArguments)) {
      emit error("Restart failed: could not relaunch debug adapter");
    }
  }
}

void DapClient::getThreads() {
  if (hasPendingRequestTag("threads")) {
    return;
  }
  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "threads";
  sendRequest("threads", {}, seq);
}

void DapClient::getStackTrace(int threadId, int startFrame, int levels) {
  const QString pendingTag = QString("stackTrace:%1").arg(threadId);
  if (hasPendingRequestTag(pendingTag)) {
    return;
  }

  QJsonObject args;
  args["threadId"] = threadId;
  if (startFrame > 0)
    args["startFrame"] = startFrame;
  if (levels > 0)
    args["levels"] = levels;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = pendingTag;
  sendRequest("stackTrace", args, seq);
}

void DapClient::getScopes(int frameId) {
  const QString pendingTag = QString("scopes:%1").arg(frameId);
  if (hasPendingRequestTag(pendingTag)) {
    return;
  }

  QJsonObject args;
  args["frameId"] = frameId;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = pendingTag;
  sendRequest("scopes", args, seq);
}

void DapClient::getVariables(int variablesReference, const QString &filter,
                             int start, int count) {
  const QString pendingTag = QString("variables:%1").arg(variablesReference);
  if (hasPendingRequestTag(pendingTag)) {
    return;
  }

  QJsonObject args;
  args["variablesReference"] = variablesReference;
  if (!filter.isEmpty())
    args["filter"] = filter;
  if (start > 0)
    args["start"] = start;
  if (count > 0)
    args["count"] = count;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = pendingTag;
  sendRequest("variables", args, seq);
}

void DapClient::evaluate(const QString &expression, int frameId,
                         const QString &context) {
  QJsonObject args;
  args["expression"] = expression;
  if (frameId >= 0)
    args["frameId"] = frameId;
  if (!context.isEmpty()) {
    args["context"] = context;
  }

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = QString("evaluate:%1").arg(expression);
  sendRequest("evaluate", args, seq);
}

void DapClient::setVariable(int variablesReference, const QString &name,
                            const QString &value) {
  QJsonObject args;
  args["variablesReference"] = variablesReference;
  args["name"] = name;
  args["value"] = value;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "setVariable";
  sendRequest("setVariable", args, seq);
}

void DapClient::sendRequest(const QString &command,
                            const QJsonObject &arguments, int seq) {
  if (m_pendingRequests.size() > MAX_PENDING_REQUESTS) {
    int toDrop = m_pendingRequests.size() - MAX_PENDING_REQUESTS;
    auto it = m_pendingRequests.begin();
    while (toDrop > 0 && it != m_pendingRequests.end()) {
      it = m_pendingRequests.erase(it);
      --toDrop;
    }
    LOG_WARNING("DAP: Pruned stale pending requests to prevent memory growth");
  }

  QJsonObject message;
  message["seq"] = seq;
  message["type"] = "request";
  message["command"] = command;
  if (!arguments.isEmpty()) {
    message["arguments"] = arguments;
  }

  QJsonDocument doc(message);
  QByteArray content = doc.toJson(QJsonDocument::Compact);

  if (!m_process) {
    LOG_WARNING("DAP: Cannot send request, process not started");
    return;
  }

  QString header = QString("Content-Length: %1\r\n\r\n").arg(content.size());
  m_process->write(header.toUtf8());
  m_process->write(content);

  LOG_DEBUG(QString("DAP request: %1 (seq=%2)").arg(command).arg(seq));
}

void DapClient::sendResponse(int requestSeq, const QString &command,
                             bool success, const QJsonObject &body,
                             const QString &message) {
  QJsonObject response;
  response["seq"] = m_nextSeq++;
  response["type"] = "response";
  response["request_seq"] = requestSeq;
  response["success"] = success;
  response["command"] = command;
  if (!body.isEmpty()) {
    response["body"] = body;
  }
  if (!success && !message.isEmpty()) {
    response["message"] = message;
  }

  QJsonDocument doc(response);
  QByteArray content = doc.toJson(QJsonDocument::Compact);

  if (m_process) {
    QString header = QString("Content-Length: %1\r\n\r\n").arg(content.size());
    m_process->write(header.toUtf8());
    m_process->write(content);
  }
}

void DapClient::onReadyReadStandardOutput() {
  if (!m_process) {
    return;
  }
  m_buffer += m_process->readAllStandardOutput();

  if (m_buffer.size() > MAX_DAP_BUFFER_BYTES) {
    const int headerPos = lastHeaderStart(m_buffer);
    if (headerPos >= 0) {
      m_buffer = m_buffer.mid(headerPos);
    } else {
      LOG_WARNING("DAP: Discarding oversized non-protocol stdout buffer");
      m_buffer.clear();
      return;
    }

    if (m_buffer.size() > MAX_DAP_BUFFER_BYTES) {
      LOG_WARNING("DAP: Trimming oversized protocol buffer tail");
      m_buffer = m_buffer.right(MAX_DAP_BUFFER_BYTES);
    }
  }

  int iterations = 0;

  while (iterations < MAX_MESSAGE_PARSE_ITERATIONS) {
    ++iterations;

    const int headerEnd = m_buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
      break;
    }

    const QByteArray header = m_buffer.left(headerEnd);
    int contentLength = 0;
    bool lengthOk = false;

    const QList<QByteArray> lines = header.split('\n');
    for (QByteArray line : lines) {
      line = line.trimmed();
      if (line.toLower().startsWith("content-length:")) {
        contentLength = line.mid(15).trimmed().toInt(&lengthOk);
        break;
      }
    }

    if (!lengthOk || contentLength <= 0) {
      LOG_WARNING("DAP message without Content-Length, skipping header");
      m_buffer = m_buffer.mid(headerEnd + 4);
      continue;
    }
    if (contentLength > MAX_DAP_MESSAGE_BYTES) {
      LOG_WARNING(QString("DAP message too large (%1 bytes), discarding frame")
                      .arg(contentLength));

      const int messageStart = headerEnd + 4;
      const int messageEnd = messageStart + contentLength;
      if (m_buffer.size() >= messageEnd) {
        m_buffer = m_buffer.mid(messageEnd);
      } else {

        m_buffer.clear();
      }
      continue;
    }

    const int messageStart = headerEnd + 4;
    const int messageEnd = messageStart + contentLength;

    if (m_buffer.size() < messageEnd) {

      break;
    }

    const QByteArray content = m_buffer.mid(messageStart, contentLength);
    m_buffer = m_buffer.mid(messageEnd);

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
      LOG_ERROR(QString("Failed to parse DAP message: %1")
                    .arg(parseError.errorString()));
      continue;
    }

    handleMessage(doc.object());
  }
}

void DapClient::onReadyReadStandardError() {
  QString stderrText = QString::fromUtf8(m_process->readAllStandardError());
  LOG_DEBUG(QString("DAP stderr: %1").arg(stderrText.trimmed()));

  DapOutputEvent evt;
  evt.category = "stderr";
  evt.output = stderrText;
  emit output(evt);
}

void DapClient::onProcessError(QProcess::ProcessError processError) {
  Q_UNUSED(processError);
  QString errorMsg = m_process ? m_process->errorString() : "Unknown error";
  LOG_ERROR(QString("DAP process error: %1").arg(errorMsg));
  setState(State::Error);
  emit error(errorMsg);
}

void DapClient::onProcessFinished(int exitCode,
                                  QProcess::ExitStatus exitStatus) {
  Q_UNUSED(exitStatus);
  LOG_INFO(QString("Debug adapter exited with code: %1").arg(exitCode));
  setState(State::Disconnected);
}

void DapClient::handleMessage(const QJsonObject &message) {
  QString type = message["type"].toString();

  if (type == "response") {
    int requestSeq = message["request_seq"].toInt();
    QString command = message["command"].toString();
    bool success = message["success"].toBool();
    handleResponse(requestSeq, command, success, message["body"],
                   message["message"].toString());
  } else if (type == "event") {
    QString event = message["event"].toString();
    handleEvent(event, message["body"].toObject());
  } else if (type == "request") {

    int seq = message["seq"].toInt();
    QString command = message["command"].toString();
    handleReverseRequest(seq, command, message["arguments"].toObject());
  }
}

void DapClient::handleResponse(int requestSeq, const QString &command,
                               bool success, const QJsonValue &body,
                               const QString &message) {
  QString pendingCommand = m_pendingRequests.take(requestSeq);

  if (!success) {
    if (command == "configurationDone") {
      LOG_WARNING(QString("DAP: configurationDone failed: %1")
                      .arg(message.isEmpty() ? "Unknown error" : message));
      return;
    }

    if (command == "setDataBreakpoints") {
      if (isLikelyUnsupportedRequestMessage(message)) {
        m_dataBreakpointsSupported = false;
        m_dataBreakpointsConfigured = false;
        LOG_WARNING("DAP: Adapter does not support setDataBreakpoints; "
                    "disabling data breakpoint sync");
      } else {
        LOG_WARNING(QString("DAP: setDataBreakpoints failed: %1")
                        .arg(message.isEmpty() ? "Unknown error" : message));
      }
      return;
    }

    if (command == "setFunctionBreakpoints") {
      const QString lowered = message.toLower();
      if (lowered.contains("notstopped")) {
        LOG_WARNING(
            QString("DAP: setFunctionBreakpoints rejected while running: "
                    "%1")
                .arg(message.isEmpty() ? "Unknown error" : message));
        return;
      }
    }

    if (command == "variables") {
      int varRef = 0;
      if (pendingCommand.startsWith("variables:")) {
        varRef = pendingCommand.mid(10).toInt();
      }
      LOG_WARNING(QString("DAP: variables request failed: %1")
                      .arg(message.isEmpty() ? "Unknown error" : message));
      emit variablesReceived(varRef, {});
      return;
    }

    LOG_ERROR(QString("DAP error for %1: %2").arg(command).arg(message));

    if (command == "evaluate") {
      QString expression = pendingCommand.startsWith("evaluate:")
                               ? pendingCommand.mid(9)
                               : command;
      emit evaluateError(expression, message);
    } else {
      emit error(QString("%1 failed: %2").arg(command).arg(message));
    }
    return;
  }

  QJsonObject bodyObj = body.toObject();

  if (command == "initialize") {
    m_capabilities = bodyObj;
    m_functionBreakpointsConfigured = false;
    m_deferredFunctionBreakpoints.clear();
    m_hasDeferredFunctionBreakpoints = false;
    if (m_capabilities.contains("supportsDataBreakpoints")) {
      m_dataBreakpointsSupported =
          m_capabilities["supportsDataBreakpoints"].toBool();
    } else {

      m_dataBreakpointsSupported = true;
    }
    m_dataBreakpointsConfigured = false;
    setState(State::Ready);

    QJsonObject initEvent;
    initEvent["seq"] = m_nextSeq++;
    initEvent["type"] = "event";
    initEvent["event"] = "initialized";

    emit initialized();
    LOG_INFO("DAP client initialized");
  } else if (command == "launch") {
    setState(State::Running);
    emit launched();
    LOG_INFO("Debug session launched");
  } else if (command == "attach") {
    setState(State::Running);
    emit attached();
    LOG_INFO("Attached to debug target");
  } else if (command == "disconnect" || command == "terminate") {
    setState(State::Ready);
    emit terminated();
  } else if (command == "setBreakpoints") {
    QString sourcePath;
    if (pendingCommand.startsWith("setBreakpoints:")) {
      sourcePath = pendingCommand.mid(15);
    }

    QList<DapBreakpoint> breakpoints;
    QJsonArray bpArray = bodyObj["breakpoints"].toArray();
    for (const auto &val : bpArray) {
      breakpoints.append(DapBreakpoint::fromJson(val.toObject()));
    }
    emit breakpointsSet(sourcePath, breakpoints);
  } else if (command == "threads") {
    QList<DapThread> threads;
    QJsonArray threadArray = bodyObj["threads"].toArray();
    for (const auto &val : threadArray) {
      threads.append(DapThread::fromJson(val.toObject()));
    }
    emit threadsReceived(threads);
  } else if (command == "stackTrace") {
    int threadId = 0;
    if (pendingCommand.startsWith("stackTrace:")) {
      threadId = pendingCommand.mid(11).toInt();
    }

    QList<DapStackFrame> frames;
    QJsonArray frameArray = bodyObj["stackFrames"].toArray();
    for (const auto &val : frameArray) {
      frames.append(DapStackFrame::fromJson(val.toObject()));
    }
    int totalFrames = bodyObj["totalFrames"].toInt(frames.size());
    emit stackTraceReceived(threadId, frames, totalFrames);
  } else if (command == "scopes") {
    int frameId = 0;
    if (pendingCommand.startsWith("scopes:")) {
      frameId = pendingCommand.mid(7).toInt();
    }

    QList<DapScope> scopes;
    QJsonArray scopeArray = bodyObj["scopes"].toArray();
    for (const auto &val : scopeArray) {
      scopes.append(DapScope::fromJson(val.toObject()));
    }
    emit scopesReceived(frameId, scopes);
  } else if (command == "variables") {
    int varRef = 0;
    if (pendingCommand.startsWith("variables:")) {
      varRef = pendingCommand.mid(10).toInt();
    }

    QList<DapVariable> variables;
    QJsonArray varArray = bodyObj["variables"].toArray();
    for (const auto &val : varArray) {
      variables.append(DapVariable::fromJson(val.toObject()));
    }
    emit variablesReceived(varRef, variables);
  } else if (command == "evaluate") {
    QString expression;
    if (pendingCommand.startsWith("evaluate:")) {
      expression = pendingCommand.mid(9);
    }

    emit evaluateResult(expression, bodyObj["result"].toString(),
                        bodyObj["type"].toString(),
                        bodyObj["variablesReference"].toInt());
  } else if (command == "continue") {
    clearPendingInspectionRequests();
    setState(State::Running);
    emit continued(bodyObj["threadId"].toInt(),
                   bodyObj["allThreadsContinued"].toBool(true));
  } else if (command == "setVariable") {
    emit variableSet(bodyObj["name"].toString(), bodyObj["value"].toString(),
                     bodyObj["type"].toString());
  } else if (command == "setDataBreakpoints") {
    LOG_DEBUG(QString("Data breakpoints set: %1")
                  .arg(bodyObj["breakpoints"].toArray().count()));
  }
}

void DapClient::handleEvent(const QString &event, const QJsonObject &body) {
  LOG_DEBUG(QString("DAP event: %1").arg(event));

  if (event == "stopped") {
    DapStoppedEvent evt = DapStoppedEvent::fromJson(body);
    m_currentThreadId = evt.threadId;
    setState(State::Stopped);
    emit stopped(evt);
    if (m_hasDeferredFunctionBreakpoints) {
      const QStringList deferred = m_deferredFunctionBreakpoints;
      m_deferredFunctionBreakpoints.clear();
      m_hasDeferredFunctionBreakpoints = false;
      setFunctionBreakpoints(deferred);
    }
  } else if (event == "continued") {
    clearPendingInspectionRequests();
    setState(State::Running);
    emit continued(body["threadId"].toInt(),
                   body["allThreadsContinued"].toBool(true));
  } else if (event == "exited") {
    emit exited(body["exitCode"].toInt());
  } else if (event == "terminated") {
    setState(State::Terminated);
    emit terminated();
  } else if (event == "thread") {
    emit threadEvent(body["threadId"].toInt(), body["reason"].toString());
  } else if (event == "output") {
    DapOutputEvent evt = DapOutputEvent::fromJson(body);
    emit output(evt);
  } else if (event == "breakpoint") {
    QString reason = body["reason"].toString();
    DapBreakpoint bp = DapBreakpoint::fromJson(body["breakpoint"].toObject());
    emit breakpointChanged(bp, reason);
  } else if (event == "initialized") {

    LOG_DEBUG("DAP: Adapter initialized, ready for configuration");
    emit adapterInitialized();
  }
}

void DapClient::handleReverseRequest(int seq, const QString &command,
                                     const QJsonObject &arguments) {
  LOG_DEBUG(QString("DAP reverse request: %1").arg(command));

  if (command == "runInTerminal") {

    QJsonObject body;
    body["processId"] = 0;
    sendResponse(seq, command, true, body);
  } else {

    sendResponse(seq, command, false, {}, "Not supported");
  }
}

void DapClient::doInitialize() {
  setState(State::Initializing);

  QJsonObject args;
  args["clientID"] = "lightpad";
  args["clientName"] = "Lightpad IDE";
  args["adapterID"] = m_adapterId.isEmpty() ? "generic" : m_adapterId;
  args["locale"] = "en-US";
  args["linesStartAt1"] = true;
  args["columnsStartAt1"] = true;
  args["pathFormat"] = "path";

  args["supportsVariableType"] = true;
  args["supportsVariablePaging"] = true;
  args["supportsRunInTerminalRequest"] = true;
  args["supportsMemoryReferences"] = true;
  args["supportsProgressReporting"] = true;

  int seq = m_nextSeq++;
  m_pendingRequests[seq] = "initialize";
  sendRequest("initialize", args, seq);
}

void DapClient::setState(State state) {
  if (m_state != state) {
    m_state = state;
    emit stateChanged(state);
  }
}

bool DapClient::isLikelyUnsupportedRequestMessage(const QString &message) {
  const QString lowered = message.toLower();
  return lowered.contains("not supported") || lowered.contains("unsupported") ||
         lowered.contains("unknown") || lowered.contains("unrecognized") ||
         lowered.contains("not implemented");
}

bool DapClient::hasPendingRequestTag(const QString &tag) const {
  return m_pendingRequests.values().contains(tag);
}

void DapClient::clearPendingInspectionRequests() {
  auto it = m_pendingRequests.begin();
  while (it != m_pendingRequests.end()) {
    const QString &tag = it.value();
    const bool isInspectionRequest =
        tag == "threads" || tag.startsWith("stackTrace:") ||
        tag.startsWith("scopes:") || tag.startsWith("variables:") ||
        tag.startsWith("evaluate:");
    if (isInspectionRequest) {
      it = m_pendingRequests.erase(it);
      continue;
    }
    ++it;
  }
}
