#include "debugadapterregistry.h"
#include "../core/logging/logger.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

class PythonDebugAdapter : public IDebugAdapter {
public:
  DebugAdapterConfig config() const override {
    DebugAdapterConfig cfg;
    cfg.id = "python-debugpy";
    cfg.name = "Python (debugpy)";
    cfg.type = "debugpy";
    cfg.program = "python";
    cfg.arguments = QStringList() << "-m" << "debugpy.adapter";
    cfg.languages = QStringList() << "py";
    cfg.extensions = QStringList() << ".py" << ".pyw";
    cfg.supportsRestart = true;
    cfg.supportsFunctionBreakpoints = true;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsHitConditionalBreakpoints = true;
    cfg.supportsLogPoints = true;

    QJsonArray filters;
    QJsonObject raised;
    raised["filter"] = "raised";
    raised["label"] = "Raised Exceptions";
    raised["default"] = false;
    filters.append(raised);

    QJsonObject uncaught;
    uncaught["filter"] = "uncaught";
    uncaught["label"] = "Uncaught Exceptions";
    uncaught["default"] = true;
    filters.append(uncaught);

    cfg.exceptionBreakpointFilters = filters;

    return cfg;
  }

  bool isAvailable() const override {

    QProcess proc;
    proc.start("python", {"-c", "import debugpy; print('ok')"});
    if (!proc.waitForFinished(5000)) {
      return false;
    }
    return proc.exitCode() == 0;
  }

  QString statusMessage() const override {
    if (isAvailable()) {
      return "Ready";
    }
    return "debugpy not installed. Run: pip install debugpy";
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "debugpy";
    config["request"] = "launch";
    config["program"] = filePath;
    config["console"] = "integratedTerminal";

    if (!workingDir.isEmpty()) {
      config["cwd"] = workingDir;
    } else {
      QFileInfo fi(filePath);
      config["cwd"] = fi.absolutePath();
    }

    return config;
  }

  QJsonObject createAttachConfig(int processId, const QString &host,
                                 int port) const override {
    QJsonObject config;
    config["type"] = "debugpy";
    config["request"] = "attach";

    if (processId > 0) {
      config["processId"] = processId;
    } else if (!host.isEmpty() && port > 0) {
      QJsonObject connect;
      connect["host"] = host;
      connect["port"] = port;
      config["connect"] = connect;
    }

    return config;
  }

  QString installCommand() const override { return "pip install debugpy"; }

  QString documentationUrl() const override {
    return "https://github.com/microsoft/debugpy";
  }
};

class NodeDebugAdapter : public IDebugAdapter {
public:
  DebugAdapterConfig config() const override {
    DebugAdapterConfig cfg;
    cfg.id = "node-debug";
    cfg.name = "Node.js";
    cfg.type = "node";
    cfg.program = "node";
    cfg.arguments = QStringList() << "--inspect-brk";
    cfg.languages = QStringList() << "js" << "ts";
    cfg.extensions = QStringList()
                     << ".js" << ".mjs" << ".cjs" << ".ts" << ".mts";
    cfg.supportsRestart = true;
    cfg.supportsFunctionBreakpoints = false;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsLogPoints = true;

    return cfg;
  }

  bool isAvailable() const override {
    QProcess proc;
    proc.start("node", {"--version"});
    if (!proc.waitForFinished(5000)) {
      return false;
    }
    return proc.exitCode() == 0;
  }

  QString statusMessage() const override {
    if (isAvailable()) {
      QProcess proc;
      proc.start("node", {"--version"});
      proc.waitForFinished(5000);
      QString version =
          QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
      return QString("Ready (%1)").arg(version);
    }
    return "Node.js not installed";
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "node";
    config["request"] = "launch";
    config["program"] = filePath;
    config["console"] = "integratedTerminal";

    if (!workingDir.isEmpty()) {
      config["cwd"] = workingDir;
    } else {
      QFileInfo fi(filePath);
      config["cwd"] = fi.absolutePath();
    }

    return config;
  }

  QJsonObject createAttachConfig(int processId, const QString &host,
                                 int port) const override {
    QJsonObject config;
    config["type"] = "node";
    config["request"] = "attach";

    if (processId > 0) {
      config["processId"] = processId;
    } else {
      config["address"] = host.isEmpty() ? "127.0.0.1" : host;
      config["port"] = port > 0 ? port : 9229;
    }

    return config;
  }

  QString documentationUrl() const override {
    return "https://nodejs.org/en/docs/guides/debugging-getting-started/";
  }
};

class GdbDebugAdapter : public IDebugAdapter {
public:
  DebugAdapterConfig config() const override {
    DebugAdapterConfig cfg;
    cfg.id = "cppdbg-gdb";
    cfg.name = "C/C++ (GDB)";
    cfg.type = "cppdbg";
    cfg.program = findSystemGdb();
    cfg.arguments = QStringList() << "--interpreter=dap";
    cfg.languages = QStringList()
                    << "cpp" << "c" << "fortran" << "rust" << "go" << "asm";
    cfg.extensions = QStringList() << ".cpp" << ".cxx" << ".cc" << ".c" << ".h"
                                   << ".hpp" << ".hxx" << ".f" << ".f90"
                                   << ".rs" << ".go" << ".s" << ".S";
    cfg.supportsRestart = true;
    cfg.supportsFunctionBreakpoints = true;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsHitConditionalBreakpoints = true;
    cfg.supportsLogPoints = false;

    cfg.defaultLaunchConfig = createDefaultLaunchConfig();
    cfg.defaultAttachConfig = createDefaultAttachConfig();

    return cfg;
  }

  bool isAvailable() const override {
    QString gdbPath = findSystemGdb();
    if (gdbPath.isEmpty() || !canExecute(gdbPath)) {
      return false;
    }
    return supportsDapInterpreter(gdbPath) && canTraceInferior(gdbPath);
  }

  QString statusMessage() const override {
    QString gdbPath = findSystemGdb();
    if (gdbPath.isEmpty()) {
      return "GDB not found on system";
    }

    if (!canExecute(gdbPath)) {
      return "GDB not found on system";
    }

    if (!supportsDapInterpreter(gdbPath)) {
      return "GDB found, but this build does not support DAP "
             "(--interpreter=dap). Install GDB 14+.";
    }

    if (!canTraceInferior(gdbPath)) {
      return "GDB DAP is available, but ptrace is restricted on this system. "
             "Debugging cannot control the target process.";
    }

    QProcess proc;
    proc.start(gdbPath, {"--interpreter=dap", "--version"});
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
      return "GDB DAP mode is unavailable";
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QString firstLine = output.split('\n').first().simplified();

    QString capabilities = getGdbCapabilities(gdbPath);

    return QString("Ready - %1%2")
        .arg(firstLine,
             capabilities.isEmpty() ? "" : QString(" (%1)").arg(capabilities));
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["name"] = QString("Debug %1").arg(QFileInfo(filePath).fileName());
    config["type"] = "cppdbg";
    config["request"] = "launch";
    config["program"] = filePath;
    config["MIMode"] = "gdb";
    config["miDebuggerPath"] = findSystemGdb();
    config["stopAtEntry"] = false;
    config["externalConsole"] = false;

    if (!workingDir.isEmpty()) {
      config["cwd"] = workingDir;
    } else {
      QFileInfo fi(filePath);
      config["cwd"] = fi.absolutePath();
    }

    config["args"] = QJsonArray();

    config["environment"] = QJsonArray();

    QJsonArray setupCommands;

    QJsonObject prettyPrint;
    prettyPrint["description"] = "Enable pretty-printing for gdb";
    prettyPrint["text"] = "-enable-pretty-printing";
    prettyPrint["ignoreFailures"] = true;
    setupCommands.append(prettyPrint);

    QJsonObject disablePagination;
    disablePagination["description"] = "Disable pagination";
    disablePagination["text"] = "set pagination off";
    disablePagination["ignoreFailures"] = true;
    setupCommands.append(disablePagination);

    config["setupCommands"] = setupCommands;

    return config;
  }

  QJsonObject createAttachConfig(int processId, const QString &host,
                                 int port) const override {
    QJsonObject config;
    config["type"] = "cppdbg";
    config["request"] = "attach";
    config["MIMode"] = "gdb";
    config["miDebuggerPath"] = findSystemGdb();

    if (processId > 0) {

      config["name"] = QString("Attach to process %1").arg(processId);
      config["processId"] = QString::number(processId);
      config["program"] = "";
    } else if (!host.isEmpty() && port > 0) {

      config["name"] = QString("Remote debug %1:%2").arg(host).arg(port);
      config["miDebuggerServerAddress"] = QString("%1:%2").arg(host).arg(port);
      config["program"] = "";

      QJsonArray setupCommands;
      QJsonObject targetRemote;
      targetRemote["description"] = "Connect to remote gdbserver";
      targetRemote["text"] = QString("target remote %1:%2").arg(host).arg(port);
      targetRemote["ignoreFailures"] = false;
      setupCommands.append(targetRemote);
      config["setupCommands"] = setupCommands;
    } else {

      config["name"] = "Attach to process";
      config["processId"] = "${command:pickProcess}";
      config["program"] = "";
    }

    return config;
  }

  QJsonObject createRemoteConfig(const QString &host, int port,
                                 const QString &program,
                                 const QString &sysroot = {}) const {
    QJsonObject config;
    config["name"] = QString("Remote Debug %1:%2").arg(host).arg(port);
    config["type"] = "cppdbg";
    config["request"] = "launch";
    config["program"] = program;
    config["MIMode"] = "gdb";
    config["miDebuggerPath"] = findSystemGdb();
    config["miDebuggerServerAddress"] = QString("%1:%2").arg(host).arg(port);

    QJsonArray setupCommands;

    if (!sysroot.isEmpty()) {
      QJsonObject setSysroot;
      setSysroot["description"] = "Set sysroot for remote symbols";
      setSysroot["text"] = QString("set sysroot %1").arg(sysroot);
      setSysroot["ignoreFailures"] = false;
      setupCommands.append(setSysroot);
    }

    QJsonObject connectRemote;
    connectRemote["description"] = "Connect to gdbserver";
    connectRemote["text"] = QString("target remote %1:%2").arg(host).arg(port);
    connectRemote["ignoreFailures"] = false;
    setupCommands.append(connectRemote);

    config["setupCommands"] = setupCommands;

    return config;
  }

  QJsonObject createCoreDumpConfig(const QString &coreDumpPath,
                                   const QString &programPath) const {
    QJsonObject config;
    config["name"] = QString("Analyze core dump: %1")
                         .arg(QFileInfo(coreDumpPath).fileName());
    config["type"] = "cppdbg";
    config["request"] = "launch";
    config["program"] = programPath;
    config["coreDumpPath"] = coreDumpPath;
    config["MIMode"] = "gdb";
    config["miDebuggerPath"] = findSystemGdb();

    return config;
  }

  QString installCommand() const override {
#ifdef Q_OS_LINUX

    if (QFileInfo::exists("/usr/bin/apt") ||
        QFileInfo::exists("/usr/bin/apt-get")) {
      return "sudo apt install gdb";
    } else if (QFileInfo::exists("/usr/bin/dnf")) {
      return "sudo dnf install gdb";
    } else if (QFileInfo::exists("/usr/bin/yum")) {
      return "sudo yum install gdb";
    } else if (QFileInfo::exists("/usr/bin/pacman")) {
      return "sudo pacman -S gdb";
    } else if (QFileInfo::exists("/usr/bin/zypper")) {
      return "sudo zypper install gdb";
    }
#elif defined(Q_OS_MACOS)
    return "brew install gdb && codesign -s gdb-cert /usr/local/bin/gdb";
#endif
    return "Install GDB using your system's package manager";
  }

  QString documentationUrl() const override {
    return "https://sourceware.org/gdb/current/onlinedocs/gdb/";
  }

  QString gdbPath() const { return findSystemGdb(); }

  QString gdbVersion() const {
    QString path = findSystemGdb();
    if (path.isEmpty()) {
      return {};
    }

    QProcess proc;
    proc.start(path, {"--version"});
    if (!proc.waitForFinished(5000)) {
      return {};
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput());

    return output.split('\n').first().simplified();
  }

  bool supportsFeature(const QString &feature) const {
    QString caps = getGdbCapabilities(findSystemGdb());
    return caps.contains(feature, Qt::CaseInsensitive);
  }

private:
  bool canExecute(const QString &gdbPath) const {
    if (gdbPath.isEmpty()) {
      return false;
    }

    QProcess proc;
    proc.start(gdbPath, {"--version"});
    if (!proc.waitForFinished(5000)) {
      return false;
    }

    return proc.exitCode() == 0;
  }

  bool supportsDapInterpreter(const QString &gdbPath) const {
    if (gdbPath.isEmpty()) {
      return false;
    }

    QProcess proc;
    proc.start(gdbPath, {"--interpreter=dap", "--version"});
    if (!proc.waitForFinished(5000)) {
      return false;
    }

    return proc.exitCode() == 0;
  }

  bool canTraceInferior(const QString &gdbPath) const {
    if (gdbPath.isEmpty()) {
      return false;
    }

#ifdef Q_OS_WIN
    Q_UNUSED(gdbPath);
    return true;
#else
    QProcess proc;
    proc.start(gdbPath,
               {"-q", "-batch", "-ex", "file /bin/true", "-ex", "starti"});
    if (!proc.waitForFinished(5000)) {
      return false;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput() +
                                             proc.readAllStandardError());
    const bool ptraceBlocked =
        output.contains("Could not trace the inferior process",
                        Qt::CaseInsensitive) ||
        output.contains("ptrace", Qt::CaseInsensitive) ||
        output.contains("Operation not permitted", Qt::CaseInsensitive);
    if (ptraceBlocked) {
      return false;
    }

    return proc.exitCode() == 0;
#endif
  }

  QString findSystemGdb() const {

    QStringList candidates = {"/usr/bin/gdb", "/usr/local/bin/gdb",
                              "/opt/homebrew/bin/gdb", "/opt/local/bin/gdb"};

    for (const QString &path : candidates) {
      if (QFileInfo::exists(path) && QFileInfo(path).isExecutable()) {
        return path;
      }
    }

    QProcess proc;
#ifdef Q_OS_WIN
    proc.start("where", {"gdb"});
#else
    proc.start("which", {"gdb"});
#endif
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
      QString path = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
      if (!path.isEmpty() && path.split('\n').first().length() > 0) {
        return path.split('\n').first().trimmed();
      }
    }

    return "gdb";
  }

  QString getGdbCapabilities(const QString &gdbPath) const {
    if (gdbPath.isEmpty()) {
      return {};
    }

    QStringList capabilities;

    QProcess proc;
    proc.start(gdbPath, {"-batch", "-ex", "python print('ok')"});
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
      capabilities << "Python";
    }

    proc.start(gdbPath, {"-batch", "-ex", "set architecture"});
    if (proc.waitForFinished(3000)) {
      QString output = QString::fromUtf8(proc.readAllStandardOutput() +
                                         proc.readAllStandardError());
      if (output.contains("auto")) {
        capabilities << "Multi-arch";
      }
    }

    return capabilities.join(", ");
  }

  QJsonObject createDefaultLaunchConfig() const {
    QJsonObject config;
    config["type"] = "cppdbg";
    config["request"] = "launch";
    config["program"] = "${workspaceFolder}/a.out";
    config["args"] = QJsonArray();
    config["stopAtEntry"] = false;
    config["cwd"] = "${workspaceFolder}";
    config["environment"] = QJsonArray();
    config["externalConsole"] = false;
    config["MIMode"] = "gdb";
    config["miDebuggerPath"] = findSystemGdb();

    QJsonArray setupCommands;
    QJsonObject prettyPrint;
    prettyPrint["description"] = "Enable pretty-printing for gdb";
    prettyPrint["text"] = "-enable-pretty-printing";
    prettyPrint["ignoreFailures"] = true;
    setupCommands.append(prettyPrint);
    config["setupCommands"] = setupCommands;

    return config;
  }

  QJsonObject createDefaultAttachConfig() const {
    QJsonObject config;
    config["type"] = "cppdbg";
    config["request"] = "attach";
    config["program"] = "${workspaceFolder}/a.out";
    config["processId"] = "${command:pickProcess}";
    config["MIMode"] = "gdb";
    config["miDebuggerPath"] = findSystemGdb();

    return config;
  }
};

class LldbDebugAdapter : public IDebugAdapter {
public:
  DebugAdapterConfig config() const override {
    DebugAdapterConfig cfg;
    cfg.id = "cppdbg-lldb";
    cfg.name = "C/C++ (LLDB)";
    cfg.type = "cppdbg";
    cfg.program = "lldb-vscode";
    cfg.languages = QStringList() << "cpp" << "c";
    cfg.extensions = QStringList()
                     << ".cpp" << ".cxx" << ".cc" << ".c" << ".h" << ".hpp";
    cfg.supportsRestart = false;
    cfg.supportsFunctionBreakpoints = true;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsHitConditionalBreakpoints = true;
    cfg.supportsLogPoints = false;

    return cfg;
  }

  bool isAvailable() const override {

    QStringList candidates;
    candidates << "lldb-vscode" << "lldb-dap";
    for (const QString &name : candidates) {
      QProcess proc;
      proc.start(name, {"--help"});
      if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
        return true;
      }
    }
    return false;
  }

  QString statusMessage() const override {
    if (isAvailable()) {
      return "Ready";
    }
    return "lldb-vscode not found. Install LLDB with DAP support.";
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "cppdbg";
    config["request"] = "launch";
    config["program"] = filePath;
    config["stopOnEntry"] = false;

    if (!workingDir.isEmpty()) {
      config["cwd"] = workingDir;
    } else {
      QFileInfo fi(filePath);
      config["cwd"] = fi.absolutePath();
    }

    return config;
  }

  QJsonObject createAttachConfig(int processId, const QString &host,
                                 int port) const override {
    Q_UNUSED(host);
    Q_UNUSED(port);

    QJsonObject config;
    config["type"] = "cppdbg";
    config["request"] = "attach";
    config["pid"] = processId;

    return config;
  }

  QString documentationUrl() const override { return "https://lldb.llvm.org/"; }
};

DebugAdapterRegistry &DebugAdapterRegistry::instance() {
  static DebugAdapterRegistry instance;
  return instance;
}

DebugAdapterRegistry::DebugAdapterRegistry() : QObject(nullptr) {
  registerBuiltinAdapters();
}

void DebugAdapterRegistry::registerBuiltinAdapters() {

  registerAdapter(std::make_shared<PythonDebugAdapter>());
  registerAdapter(std::make_shared<NodeDebugAdapter>());
  registerAdapter(std::make_shared<GdbDebugAdapter>());
  registerAdapter(std::make_shared<LldbDebugAdapter>());

  LOG_INFO("Registered built-in debug adapters");
}

void DebugAdapterRegistry::registerAdapter(
    std::shared_ptr<IDebugAdapter> adapter) {
  if (!adapter) {
    return;
  }

  QString id = adapter->config().id;
  m_adapters[id] = adapter;

  LOG_INFO(QString("Registered debug adapter: %1").arg(id));
  emit adapterRegistered(id);
}

void DebugAdapterRegistry::unregisterAdapter(const QString &adapterId) {
  if (m_adapters.remove(adapterId) > 0) {
    LOG_INFO(QString("Unregistered debug adapter: %1").arg(adapterId));
    emit adapterUnregistered(adapterId);
  }
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::allAdapters() const {
  return m_adapters.values();
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::availableAdapters() const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapter : m_adapters) {
    if (adapter->isAvailable()) {
      result.append(adapter);
    }
  }
  return result;
}

std::shared_ptr<IDebugAdapter>
DebugAdapterRegistry::adapter(const QString &adapterId) const {
  return m_adapters.value(adapterId);
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::adaptersForFile(const QString &filePath) const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapter : m_adapters) {
    if (adapter->canDebug(filePath)) {
      result.append(adapter);
    }
  }
  return result;
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::adaptersForLanguage(const QString &languageId) const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapter : m_adapters) {
    if (adapter->supportsLanguage(languageId)) {
      result.append(adapter);
    }
  }
  return result;
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::adaptersForType(const QString &type) const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapter : m_adapters) {
    if (adapter &&
        adapter->config().type.compare(type, Qt::CaseInsensitive) == 0) {
      result.append(adapter);
    }
  }
  return result;
}

std::shared_ptr<IDebugAdapter>
DebugAdapterRegistry::preferredAdapterForFile(const QString &filePath) const {
  for (const auto &adapter : m_adapters) {
    if (adapter->canDebug(filePath) && adapter->isAvailable()) {
      return adapter;
    }
  }
  return nullptr;
}

std::shared_ptr<IDebugAdapter>
DebugAdapterRegistry::preferredAdapterForLanguage(
    const QString &languageId) const {
  for (const auto &adapter : m_adapters) {
    if (adapter->supportsLanguage(languageId) && adapter->isAvailable()) {
      return adapter;
    }
  }
  return nullptr;
}

void DebugAdapterRegistry::refreshAvailability() { emit availabilityChanged(); }
