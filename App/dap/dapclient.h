#ifndef DAPCLIENT_H
#define DAPCLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QVariant>

/**
 * @brief Debug Adapter Protocol (DAP) source location
 */
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

/**
 * @brief Breakpoint representation
 */
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

/**
 * @brief Source breakpoint (for setting breakpoints)
 */
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

/**
 * @brief Stack frame representation
 */
struct DapStackFrame {
  int id = 0;
  QString name;
  DapSource source;
  int line = 0;
  int column = 0;
  int endLine = 0;
  int endColumn = 0;
  QString moduleId;
  QString presentationHint; // "normal", "label", "subtle"

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

/**
 * @brief Scope representation (for variables grouping)
 */
struct DapScope {
  QString name;
  QString presentationHint; // "arguments", "locals", "registers"
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

/**
 * @brief Variable representation
 */
struct DapVariable {
  QString name;
  QString value;
  QString type;
  int variablesReference = 0; // > 0 means structured/expandable
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

/**
 * @brief Thread representation
 */
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

/**
 * @brief Debug output event data
 */
struct DapOutputEvent {
  QString category; // "console", "stdout", "stderr", "telemetry", "important"
  QString output;
  QString group; // "start", "startCollapsed", "end"
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

/**
 * @brief Stopped event reasons
 */
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

/**
 * @brief Stopped event data
 */
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

/**
 * @brief Debug Adapter Protocol client
 *
 * Provides communication with debug adapters using the DAP protocol.
 * This is a language-agnostic client that can work with any DAP-compliant
 * debug adapter (Python debugpy, Node.js debug adapter, GDB via gdb-dap, etc.)
 *
 * Supports:
 * - Initialize/launch/attach/terminate lifecycle
 * - Breakpoint management (source, function, conditional, logpoints)
 * - Execution control (continue, step over/into/out, pause)
 * - Variable inspection and evaluation
 * - Stack trace navigation
 * - Multi-threaded debugging
 * - Debug console/REPL
 */
class DapClient : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Client state
   */
  enum class State {
    Disconnected,
    Connecting,
    Initializing,
    Ready,      // Initialized, but not debugging
    Running,    // Debugging, program is running
    Stopped,    // Debugging, program is stopped at breakpoint/step
    Terminated, // Debug session ended
    Error
  };
  Q_ENUM(State)

  explicit DapClient(QObject *parent = nullptr);
  ~DapClient();

  /**
   * @brief Start the debug adapter process
   * @param program Path to the debug adapter executable
   * @param arguments Command line arguments
   * @return true if adapter started
   */
  bool start(const QString &program, const QStringList &arguments = {});

  /**
   * @brief Stop the debug adapter and terminate debug session
   */
  void stop();

  /**
   * @brief Get current state
   * @return Client state
   */
  State state() const;

  /**
   * @brief Check if client is ready for debugging
   * @return true if initialized and ready
   */
  bool isReady() const;

  /**
   * @brief Check if currently debugging
   * @return true if in Running or Stopped state
   */
  bool isDebugging() const;

  // Lifecycle commands

  /**
   * @brief Launch a program for debugging
   * @param program Path to the program to debug
   * @param args Command line arguments for the program
   * @param cwd Working directory
   * @param env Environment variables (name -> value)
   * @param stopOnEntry Whether to stop at entry point
   */
  void launch(const QString &program, const QStringList &args = {},
              const QString &cwd = {}, const QMap<QString, QString> &env = {},
              bool stopOnEntry = false);

  /**
   * @brief Attach to a running process
   * @param processId Process ID to attach to
   */
  void attach(int processId);

  /**
   * @brief Attach to a remote debug target
   * @param host Host name or IP
   * @param port Port number
   */
  void attachRemote(const QString &host, int port);

  /**
   * @brief Disconnect from debug target
   * @param terminateDebuggee Whether to terminate the debuggee
   */
  void disconnect(bool terminateDebuggee = true);

  /**
   * @brief Terminate the debuggee
   */
  void terminate();

  // Breakpoint management

  /**
   * @brief Set source breakpoints for a file
   * @param sourcePath Path to the source file
   * @param breakpoints List of breakpoints to set
   *
   * Note: This replaces all breakpoints for the file
   */
  void setBreakpoints(const QString &sourcePath,
                      const QList<DapSourceBreakpoint> &breakpoints);

  /**
   * @brief Set function breakpoints
   * @param functionNames List of function names to break on
   */
  void setFunctionBreakpoints(const QStringList &functionNames);

  /**
   * @brief Set exception breakpoints
   * @param filterIds List of exception filter IDs (e.g., "uncaught", "raised")
   */
  void setExceptionBreakpoints(const QStringList &filterIds);

  // Execution control

  /**
   * @brief Continue execution
   * @param threadId Thread to continue (0 = all threads)
   */
  void continueExecution(int threadId = 0);

  /**
   * @brief Pause execution
   * @param threadId Thread to pause (0 = all threads)
   */
  void pause(int threadId = 0);

  /**
   * @brief Step over (next line)
   * @param threadId Thread to step
   */
  void stepOver(int threadId);

  /**
   * @brief Step into function call
   * @param threadId Thread to step
   */
  void stepInto(int threadId);

  /**
   * @brief Step out of current function
   * @param threadId Thread to step
   */
  void stepOut(int threadId);

  /**
   * @brief Restart the debug session
   */
  void restart();

  // Inspection

  /**
   * @brief Get list of threads
   */
  void getThreads();

  /**
   * @brief Get stack trace for a thread
   * @param threadId Thread ID
   * @param startFrame Starting frame index
   * @param levels Number of frames to retrieve (0 = all)
   */
  void getStackTrace(int threadId, int startFrame = 0, int levels = 0);

  /**
   * @brief Get scopes for a stack frame
   * @param frameId Stack frame ID
   */
  void getScopes(int frameId);

  /**
   * @brief Get variables for a scope or structured variable
   * @param variablesReference Reference from scope or parent variable
   * @param filter Filter type ("indexed", "named", or empty for both)
   * @param start Start index (for indexed variables)
   * @param count Number of variables to fetch
   */
  void getVariables(int variablesReference, const QString &filter = {},
                    int start = 0, int count = 0);

  /**
   * @brief Evaluate an expression
   * @param expression Expression to evaluate
   * @param frameId Stack frame context (0 = global)
   * @param context Evaluation context ("watch", "repl", "hover", "clipboard")
   */
  void evaluate(const QString &expression, int frameId = 0,
                const QString &context = "repl");

  /**
   * @brief Set a variable's value
   * @param variablesReference Parent variable reference
   * @param name Variable name
   * @param value New value
   */
  void setVariable(int variablesReference, const QString &name,
                   const QString &value);

  /**
   * @brief Get the current thread ID (from last stopped event)
   */
  int currentThreadId() const { return m_currentThreadId; }

signals:
  void stateChanged(State state);
  void initialized();
  void error(const QString &message);

  // Lifecycle events
  void launched();
  void attached();
  void terminated();
  void exited(int exitCode);

  // Breakpoint events
  void breakpointsSet(const QString &sourcePath,
                      const QList<DapBreakpoint> &breakpoints);
  void breakpointChanged(const DapBreakpoint &breakpoint,
                         const QString &reason);

  // Execution events
  void stopped(const DapStoppedEvent &event);
  void continued(int threadId, bool allThreadsContinued);
  void threadEvent(int threadId, const QString &reason); // "started", "exited"

  // Debug output
  void output(const DapOutputEvent &event);

  // Inspection responses
  void threadsReceived(const QList<DapThread> &threads);
  void stackTraceReceived(int threadId, const QList<DapStackFrame> &frames,
                          int totalFrames);
  void scopesReceived(int frameId, const QList<DapScope> &scopes);
  void variablesReceived(int variablesReference,
                         const QList<DapVariable> &variables);
  void evaluateResult(const QString &expression, const QString &result,
                      const QString &type, int variablesReference);

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

  QProcess *m_process;
  State m_state;
  int m_nextSeq;
  QString m_buffer;
  QMap<int, QString> m_pendingRequests; // seq -> command

  int m_currentThreadId;
  QString m_adapterId;
  QJsonObject m_capabilities;

  // Stored launch/attach configuration for restart
  QJsonObject m_launchConfig;
  bool m_isAttach;
};

#endif // DAPCLIENT_H
