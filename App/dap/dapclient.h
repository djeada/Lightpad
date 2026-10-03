#ifndef DAPCLIENT_H
#define DAPCLIENT_H

#include <QByteArray>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMetaMethod>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTcpSocket>
#include <QVariant>

struct DapSource {
  QString name;
  QString path;
  int sourceReference = 0;

  QJsonObject toJson() const {
    QJsonObject obj;
    if (!name.isEmpty())
      obj["name"] = name;
    if (!path.isEmpty())
      obj["path"] = path;
    if (sourceReference > 0)
      obj["sourceReference"] = sourceReference;
    return obj;
  }

  static DapSource fromJson(const QJsonObject &obj) {
    DapSource src;
    src.name = obj["name"].toString();
    src.path = obj["path"].toString();
    src.sourceReference = obj["sourceReference"].toInt();

    if (src.name.isEmpty() && !src.path.isEmpty()) {
      src.name = QFileInfo(src.path).fileName();
    }
    return src;
  }
};

struct DapBreakpoint {
  int id = 0;
  bool verified = false;
  QString message;
  DapSource source;
  int line = 0;
  int column = 0;
  int endLine = 0;
  int endColumn = 0;

  static DapBreakpoint fromJson(const QJsonObject &obj) {
    DapBreakpoint bp;
    bp.id = obj["id"].toInt();
    bp.verified = obj["verified"].toBool();
    bp.message = obj["message"].toString();
    bp.line = obj["line"].toInt();
    bp.column = obj["column"].toInt();
    bp.endLine = obj["endLine"].toInt();
    bp.endColumn = obj["endColumn"].toInt();
    if (obj.contains("source")) {
      bp.source = DapSource::fromJson(obj["source"].toObject());
    }
    return bp;
  }
};

struct DapSourceBreakpoint {
  int line;
  int column = 0;
  QString condition;
  QString hitCondition;
  QString logMessage;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["line"] = line;
    if (column > 0)
      obj["column"] = column;
    if (!condition.isEmpty())
      obj["condition"] = condition;
    if (!hitCondition.isEmpty())
      obj["hitCondition"] = hitCondition;
    if (!logMessage.isEmpty())
      obj["logMessage"] = logMessage;
    return obj;
  }
};

struct DapStackFrame {
  int id = 0;
  QString name;
  DapSource source;
  int line = 0;
  int column = 0;
  int endLine = 0;
  int endColumn = 0;
  QString moduleId;
  QString presentationHint;

  static DapStackFrame fromJson(const QJsonObject &obj) {
    DapStackFrame frame;
    frame.id = obj["id"].toInt();
    frame.name = obj["name"].toString();
    frame.line = obj["line"].toInt();
    frame.column = obj["column"].toInt();
    frame.endLine = obj["endLine"].toInt();
    frame.endColumn = obj["endColumn"].toInt();
    frame.moduleId = obj["moduleId"].toString();
    frame.presentationHint = obj["presentationHint"].toString();
    if (obj.contains("source")) {
      frame.source = DapSource::fromJson(obj["source"].toObject());
    }
    return frame;
  }
};

struct DapScope {
  QString name;
  QString presentationHint;
  int variablesReference = 0;
  int namedVariables = 0;
  int indexedVariables = 0;
  bool expensive = false;
  DapSource source;
  int line = 0;
  int column = 0;
  int endLine = 0;
  int endColumn = 0;

  static DapScope fromJson(const QJsonObject &obj) {
    DapScope scope;
    scope.name = obj["name"].toString();
    scope.presentationHint = obj["presentationHint"].toString();
    scope.variablesReference = obj["variablesReference"].toInt();
    scope.namedVariables = obj["namedVariables"].toInt();
    scope.indexedVariables = obj["indexedVariables"].toInt();
    scope.expensive = obj["expensive"].toBool();
    scope.line = obj["line"].toInt();
    scope.column = obj["column"].toInt();
    scope.endLine = obj["endLine"].toInt();
    scope.endColumn = obj["endColumn"].toInt();
    if (obj.contains("source")) {
      scope.source = DapSource::fromJson(obj["source"].toObject());
    }
    return scope;
  }
};

struct DapVariable {
  QString name;
  QString value;
  QString type;
  int variablesReference = 0;
  int namedVariables = 0;
  int indexedVariables = 0;
  QString evaluateName;
  QString memoryReference;

  static DapVariable fromJson(const QJsonObject &obj) {
    DapVariable var;
    var.name = obj["name"].toString();
    var.value = obj["value"].toString();
    var.type = obj["type"].toString();
    var.variablesReference = obj["variablesReference"].toInt();
    var.namedVariables = obj["namedVariables"].toInt();
    var.indexedVariables = obj["indexedVariables"].toInt();
    var.evaluateName = obj["evaluateName"].toString();
    var.memoryReference = obj["memoryReference"].toString();
    return var;
  }
};

struct DapThread {
  int id = 0;
  QString name;

  static DapThread fromJson(const QJsonObject &obj) {
    DapThread thread;
    thread.id = obj["id"].toInt();
    thread.name = obj["name"].toString();
    return thread;
  }
};

struct DapExceptionDetails {
  QString message;
  QString typeName;
  QString fullTypeName;
  QString evaluateName;
  QString stackTrace;
  QList<DapExceptionDetails> innerExceptions;

  static DapExceptionDetails fromJson(const QJsonObject &obj) {
    DapExceptionDetails details;
    details.message = obj["message"].toString();
    details.typeName = obj["typeName"].toString();
    details.fullTypeName = obj["fullTypeName"].toString();
    details.evaluateName = obj["evaluateName"].toString();
    details.stackTrace = obj["stackTrace"].toString();
    const QJsonArray inner = obj["innerException"].toArray();
    for (const auto &val : inner) {
      details.innerExceptions.append(
          DapExceptionDetails::fromJson(val.toObject()));
    }
    return details;
  }
};

struct DapExceptionInfo {
  QString exceptionId;
  QString description;
  QString breakMode;
  DapExceptionDetails details;

  static DapExceptionInfo fromJson(const QJsonObject &body) {
    DapExceptionInfo info;
    info.exceptionId = body["exceptionId"].toString();
    info.description = body["description"].toString();
    info.breakMode = body["breakMode"].toString();
    if (body.contains("details")) {
      info.details = DapExceptionDetails::fromJson(body["details"].toObject());
    }
    return info;
  }
};

struct DapCompletionItem {
  QString label;
  QString text;
  QString type;
  QString sortText;
  int start = -1;
  int length = 0;

  static DapCompletionItem fromJson(const QJsonObject &obj) {
    DapCompletionItem item;
    item.label = obj["label"].toString();
    item.text = obj["text"].toString();
    item.type = obj["type"].toString();
    item.sortText = obj["sortText"].toString();
    item.start = obj.contains("start") ? obj["start"].toInt() : -1;
    item.length = obj["length"].toInt();
    return item;
  }

  QString insertText() const { return text.isEmpty() ? label : text; }
};

struct DapDataBreakpointInfo {
  QString dataId;
  QString description;
  QStringList accessTypes;
  bool canPersist = false;

  bool isValid() const { return !dataId.isEmpty(); }

  static DapDataBreakpointInfo fromJson(const QJsonObject &obj) {
    DapDataBreakpointInfo info;
    info.dataId = obj["dataId"].isString() ? obj["dataId"].toString()
                                           : QString();
    info.description = obj["description"].toString();
    for (const auto &val : obj["accessTypes"].toArray()) {
      info.accessTypes.append(val.toString());
    }
    info.canPersist = obj["canPersist"].toBool();
    return info;
  }
};

struct DapMemoryChunk {
  QString address;
  int unreadableBytes = 0;
  QByteArray data;

  static DapMemoryChunk fromJson(const QJsonObject &obj) {
    DapMemoryChunk chunk;
    chunk.address = obj["address"].toString();
    chunk.unreadableBytes = obj["unreadableBytes"].toInt();
    chunk.data = QByteArray::fromBase64(obj["data"].toString().toLatin1());
    return chunk;
  }
};

struct DapDisassembledInstruction {
  QString address;
  QString instructionBytes;
  QString instruction;
  QString symbol;
  DapSource source;
  int line = 0;

  static DapDisassembledInstruction fromJson(const QJsonObject &obj) {
    DapDisassembledInstruction ins;
    ins.address = obj["address"].toString();
    ins.instructionBytes = obj["instructionBytes"].toString();
    ins.instruction = obj["instruction"].toString();
    ins.symbol = obj["symbol"].toString();
    ins.line = obj["line"].toInt();
    if (obj.contains("location")) {
      ins.source = DapSource::fromJson(obj["location"].toObject());
    }
    return ins;
  }
};

struct DapOutputEvent {
  QString category;
  QString output;
  QString group;
  int variablesReference = 0;
  DapSource source;
  int line = 0;
  int column = 0;

  static DapOutputEvent fromJson(const QJsonObject &obj) {
    DapOutputEvent evt;
    evt.category = obj["category"].toString("console");
    evt.output = obj["output"].toString();
    evt.group = obj["group"].toString();
    evt.variablesReference = obj["variablesReference"].toInt();
    evt.line = obj["line"].toInt();
    evt.column = obj["column"].toInt();
    if (obj.contains("source")) {
      evt.source = DapSource::fromJson(obj["source"].toObject());
    }
    return evt;
  }
};

enum class DapStoppedReason {
  Step,
  Breakpoint,
  Exception,
  Pause,
  Entry,
  Goto,
  FunctionBreakpoint,
  DataBreakpoint,
  InstructionBreakpoint,
  Unknown
};

struct DapStoppedEvent {
  DapStoppedReason reason = DapStoppedReason::Unknown;
  QString rawReason;
  QString description;
  int threadId = 0;
  bool preserveFocusHint = false;
  QString text;
  bool allThreadsStopped = false;
  QList<int> hitBreakpointIds;

  static DapStoppedEvent fromJson(const QJsonObject &obj) {
    DapStoppedEvent evt;
    QString reasonStr = obj["reason"].toString();
    evt.rawReason = reasonStr;
    if (reasonStr == "step")
      evt.reason = DapStoppedReason::Step;
    else if (reasonStr == "breakpoint")
      evt.reason = DapStoppedReason::Breakpoint;
    else if (reasonStr == "exception")
      evt.reason = DapStoppedReason::Exception;
    else if (reasonStr == "pause")
      evt.reason = DapStoppedReason::Pause;
    else if (reasonStr == "entry")
      evt.reason = DapStoppedReason::Entry;
    else if (reasonStr == "goto")
      evt.reason = DapStoppedReason::Goto;
    else if (reasonStr == "function breakpoint")
      evt.reason = DapStoppedReason::FunctionBreakpoint;
    else if (reasonStr == "data breakpoint")
      evt.reason = DapStoppedReason::DataBreakpoint;
    else if (reasonStr == "instruction breakpoint")
      evt.reason = DapStoppedReason::InstructionBreakpoint;
    else if (reasonStr.compare("signal", Qt::CaseInsensitive) == 0)

      evt.reason = DapStoppedReason::Exception;

    evt.description = obj["description"].toString();
    evt.threadId = obj["threadId"].toInt();
    evt.preserveFocusHint = obj["preserveFocusHint"].toBool();
    evt.text = obj["text"].toString();
    evt.allThreadsStopped = obj["allThreadsStopped"].toBool();

    QJsonArray hitBps = obj["hitBreakpointIds"].toArray();
    for (const auto &val : hitBps) {
      evt.hitBreakpointIds.append(val.toInt());
    }
    return evt;
  }
};

class DapClient : public QObject {
  Q_OBJECT

public:
  enum class State {
    Disconnected,
    Connecting,
    Initializing,
    Ready,
    Running,
    Stopped,
    Terminated,
    Error
  };
  Q_ENUM(State)

  explicit DapClient(QObject *parent = nullptr);
  ~DapClient();

  void setAdapterMetadata(const QString &adapterId, const QString &adapterType);
  QString adapterId() const { return m_adapterId; }
  QString adapterType() const { return m_adapterType; }

  bool start(const QString &program, const QStringList &arguments = {});

  bool startSocket(const QString &host, quint16 port);

  bool startServer(const QString &program, const QStringList &arguments = {});

  bool isSocketTransport() const { return m_socket != nullptr; }

  void feedAdapterData(const QByteArray &data);

  void stop(bool terminateDebuggee = true);

  State state() const;

  bool isReady() const;

  bool isDebugging() const;

  void launch(const QString &program, const QStringList &args = {},
              const QString &cwd = {}, const QMap<QString, QString> &env = {},
              bool stopOnEntry = false);
  void launch(const QJsonObject &arguments);

  void attach(int processId);

  void attachRemote(const QString &host, int port);
  void attach(const QJsonObject &arguments);

  void disconnect(bool terminateDebuggee = true);

  void terminate();

  void configurationDone();
  bool supportsConfigurationDoneRequest() const;
  bool supportsRestartRequest() const;
  bool supportsTerminateRequest() const;
  bool supportsSetVariable() const;
  bool supportsExceptionInfoRequest() const;
  bool supportsCompletionsRequest() const;
  bool supportsReadMemoryRequest() const;
  bool supportsDisassembleRequest() const;
  bool supportsDataBreakpoints() const;
  QJsonObject capabilities() const { return m_capabilities; }

  void setBreakpoints(const QString &sourcePath,
                      const QList<DapSourceBreakpoint> &breakpoints);

  void setFunctionBreakpoints(const QStringList &functionNames);

  void setDataBreakpoints(const QList<QJsonObject> &dataBreakpoints);

  void setExceptionBreakpoints(const QStringList &filterIds);

  void continueExecution(int threadId = 0);

  void pause(int threadId = 0);

  void stepOver(int threadId);

  void stepInto(int threadId);

  void stepOut(int threadId);

  void restart();

  void getThreads();

  void getStackTrace(int threadId, int startFrame = 0, int levels = 0);

  void getScopes(int frameId);

  void getVariables(int variablesReference, const QString &filter = {},
                    int start = 0, int count = 0);

  int evaluate(const QString &expression, int frameId = -1,
               const QString &context = "repl");

  void setVariable(int variablesReference, const QString &name,
                   const QString &value);

  void exceptionInfo(int threadId);

  int completions(const QString &text, int column, int frameId = -1);

  int dataBreakpointInfo(int variablesReference, const QString &name,
                         int frameId = -1);

  int dataBreakpointInfoForExpression(const QString &expression,
                                      int frameId = -1);

  int readMemory(const QString &memoryReference, int count, int offset = 0);

  int disassemble(const QString &memoryReference, int instructionCount,
                  int instructionOffset = 0, int offset = 0,
                  bool resolveSymbols = true);

  void setRequestTimeoutMs(int ms) { m_requestTimeoutMs = ms; }
  int requestTimeoutMs() const { return m_requestTimeoutMs; }

  void respondToRunInTerminal(int requestSeq, bool success,
                              qint64 processId = 0,
                              const QString &message = {});

  int currentThreadId() const { return m_currentThreadId; }

signals:
  void stateChanged(State state);
  void initialized();
  void adapterInitialized();
  void error(const QString &message);

  void launched();
  void attached();
  void terminated();
  void exited(int exitCode);

  void breakpointsSet(const QString &sourcePath,
                      const QList<DapBreakpoint> &breakpoints);
  void breakpointChanged(const DapBreakpoint &breakpoint,
                         const QString &reason);

  void stopped(const DapStoppedEvent &event);
  void continued(int threadId, bool allThreadsContinued);
  void threadEvent(int threadId, const QString &reason);

  void output(const DapOutputEvent &event);
  void runInTerminalRequested(int requestSeq, const QString &kind,
                              const QString &title, const QStringList &args,
                              const QString &cwd,
                              const QMap<QString, QString> &env);

  void threadsReceived(const QList<DapThread> &threads);
  void stackTraceReceived(int threadId, const QList<DapStackFrame> &frames,
                          int totalFrames);
  void scopesReceived(int frameId, const QList<DapScope> &scopes);
  void variablesReceived(int variablesReference,
                         const QList<DapVariable> &variables);
  void evaluateResponse(int requestSeq, const QString &expression,
                        const QString &result, const QString &type,
                        int variablesReference);
  void evaluateResult(const QString &expression, const QString &result,
                      const QString &type, int variablesReference);
  void evaluateResponseError(int requestSeq, const QString &expression,
                             const QString &errorMessage);
  void evaluateError(const QString &expression, const QString &errorMessage);
  void variableSet(const QString &name, const QString &newValue,
                   const QString &type);
  void exceptionInfoReceived(int threadId, const DapExceptionInfo &info);
  void exceptionInfoError(int threadId, const QString &errorMessage);
  void completionsReceived(int requestSeq,
                           const QList<DapCompletionItem> &items);
  void completionsFailed(int requestSeq, const QString &errorMessage);
  void dataBreakpointInfoReceived(int requestSeq,
                                  const DapDataBreakpointInfo &info);
  void dataBreakpointInfoFailed(int requestSeq, const QString &errorMessage);
  void memoryReceived(int requestSeq, const DapMemoryChunk &chunk);
  void memoryFailed(int requestSeq, const QString &errorMessage);
  void disassemblyReceived(int requestSeq,
                           const QList<DapDisassembledInstruction> &items);
  void disassemblyFailed(int requestSeq, const QString &errorMessage);
  void requestTimedOut(const QString &command);

private slots:
  void onReadyReadStandardOutput();
  void onReadyReadStandardError();
  void onProcessError(QProcess::ProcessError error);
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
  void sendRequest(const QString &command, const QJsonObject &arguments,
                   int seq);
  void sendResponse(int requestSeq, const QString &command, bool success,
                    const QJsonObject &body = {}, const QString &message = {});
  bool writeToAdapter(const QByteArray &bytes);
  bool hasLiveChannel() const;
  void handleAdapterData(const QByteArray &data);
  void shutdownChannel();
  void onChannelClosed();
  void handleMessage(const QJsonObject &message);
  void handleResponse(int requestSeq, const QString &command, bool success,
                      const QJsonValue &body, const QString &message);
  void handleEvent(const QString &event, const QJsonObject &body);
  void handleReverseRequest(int seq, const QString &command,
                            const QJsonObject &arguments);

  void doInitialize();
  void setState(State state);
  static bool isLikelyUnsupportedRequestMessage(const QString &message);
  bool hasPendingRequestTag(const QString &tag) const;
  void clearPendingInspectionRequests();
  void failDiscardedMessage(const QByteArray &sample, const QString &reason);
  bool connectSocket(const QString &host, quint16 port);
  void onServerOutput(bool isStderr);
  void onServerFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void shutdownServerProcess();
  void resetSessionState();
  void abortStartup(const QString &message);

  QProcess *m_process;
  QTcpSocket *m_socket = nullptr;
  QString m_socketHost;
  quint16 m_socketPort = 0;
  State m_state;
  int m_nextSeq;
  QByteArray m_buffer;
  qint64 m_skipBytes = 0;
  QByteArray m_skipHead;
  QByteArray m_skipTail;
  bool m_parseScheduled = false;
  QMap<int, QString> m_pendingRequests;
  QSet<int> m_staleRequests;
  QProcess *m_serverProcess = nullptr;
  QString m_serverProgram;
  QStringList m_serverArguments;
  QByteArray m_serverOutput;

  int m_currentThreadId;
  QString m_adapterId;
  QJsonObject m_capabilities;
  bool m_functionBreakpointsConfigured;
  QStringList m_deferredFunctionBreakpoints;
  bool m_hasDeferredFunctionBreakpoints;
  bool m_dataBreakpointsSupported;
  bool m_dataBreakpointsConfigured;
  bool m_pausePending;
  bool m_stopping = false;
  int m_requestTimeoutMs = 30000;

  QJsonObject m_launchConfig;
  bool m_isAttach;
  QString m_adapterType;
};

#endif
