#ifndef DAPCLIENT_H
#define DAPCLIENT_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
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
  QString description;
  int threadId = 0;
  bool preserveFocusHint = false;
  QString text;
  bool allThreadsStopped = false;
  QList<int> hitBreakpointIds;

  static DapStoppedEvent fromJson(const QJsonObject &obj) {
    DapStoppedEvent evt;
    QString reasonStr = obj["reason"].toString();
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

  void stop();

  State state() const;

  bool isReady() const;

  bool isDebugging() const;

  void launch(const QString &program, const QStringList &args = {},
              const QString &cwd = {}, const QMap<QString, QString> &env = {},
              bool stopOnEntry = false);

  void attach(int processId);

  void attachRemote(const QString &host, int port);

  void disconnect(bool terminateDebuggee = true);

  void terminate();

  void configurationDone();
  bool supportsConfigurationDoneRequest() const;
  bool supportsRestartRequest() const;

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

  void evaluate(const QString &expression, int frameId = -1,
                const QString &context = "repl");

  void setVariable(int variablesReference, const QString &name,
                   const QString &value);

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

  void threadsReceived(const QList<DapThread> &threads);
  void stackTraceReceived(int threadId, const QList<DapStackFrame> &frames,
                          int totalFrames);
  void scopesReceived(int frameId, const QList<DapScope> &scopes);
  void variablesReceived(int variablesReference,
                         const QList<DapVariable> &variables);
  void evaluateResult(const QString &expression, const QString &result,
                      const QString &type, int variablesReference);
  void evaluateError(const QString &expression, const QString &errorMessage);
  void variableSet(const QString &name, const QString &newValue,
                   const QString &type);

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

  QProcess *m_process;
  State m_state;
  int m_nextSeq;
  QByteArray m_buffer;
  QMap<int, QString> m_pendingRequests;

  int m_currentThreadId;
  QString m_adapterId;
  QJsonObject m_capabilities;
  bool m_functionBreakpointsConfigured;
  QStringList m_deferredFunctionBreakpoints;
  bool m_hasDeferredFunctionBreakpoints;
  bool m_dataBreakpointsSupported;
  bool m_dataBreakpointsConfigured;

  QJsonObject m_launchConfig;
  bool m_isAttach;
  QString m_adapterType;
};

#endif
