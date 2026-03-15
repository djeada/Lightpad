#include "builtinadapters.h"

#include "processbackeddebugadapter.h"

#include <QFileInfo>

namespace {
class GdbDebugAdapter : public ProcessBackedDebugAdapter {
public:
  GdbDebugAdapter()
      : ProcessBackedDebugAdapter(createBaseConfig(), "miDebuggerPath",
                                  "Debugger Executable",
                                  "gdb or /usr/bin/gdb") {}

  bool isAvailable() const override {
    const QString gdbPath = config().program;
    if (gdbPath.isEmpty() || !canExecute(gdbPath)) {
      return false;
    }
    return supportsDapInterpreter(gdbPath) && canTraceInferior(gdbPath);
  }

  bool isAvailableForConfiguration(
      const DebugConfiguration &configuration) const override {
    const QString gdbPath = configForConfiguration(configuration).program;
    if (gdbPath.isEmpty() || !canExecute(gdbPath)) {
      return false;
    }
    return supportsDapInterpreter(gdbPath) && canTraceInferior(gdbPath);
  }

  QString statusMessage() const override {
    return statusMessageForConfiguration(DebugConfiguration{});
  }

  QString statusMessageForConfiguration(
      const DebugConfiguration &configuration) const override {
    const QString gdbPath = configForConfiguration(configuration).program;
    if (gdbPath.isEmpty() || !canExecute(gdbPath)) {
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

    const ProcessProbeResult probe =
        runProcessProbe(gdbPath, {"--interpreter=dap", "--version"});
    if (!probe.success) {
      return "GDB DAP mode is unavailable";
    }

    const QString firstLine = firstOutputLine(probe.stdoutData);
    const QString capabilities = getGdbCapabilities(gdbPath);
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
    config["cwd"] = workingDir.isEmpty() ? QFileInfo(filePath).absolutePath()
                                         : workingDir;
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

private:
  static DebugAdapterConfig createBaseConfig() {
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

  bool canExecute(const QString &gdbPath) const {
    if (gdbPath.isEmpty()) {
      return false;
    }
    return runProcessProbe(gdbPath, {"--version"}).success;
  }

  bool supportsDapInterpreter(const QString &gdbPath) const {
    if (gdbPath.isEmpty()) {
      return false;
    }
    return runProcessProbe(gdbPath, {"--interpreter=dap", "--version"}).success;
  }

  bool canTraceInferior(const QString &gdbPath) const {
    if (gdbPath.isEmpty()) {
      return false;
    }
#ifdef Q_OS_WIN
    Q_UNUSED(gdbPath);
    return true;
#else
    const ProcessProbeResult probe =
        runProcessProbe(gdbPath,
                        {"-q", "-batch", "-ex", "file /bin/true", "-ex", "starti"});
    if (probe.stdoutData.isEmpty() && probe.stderrData.isEmpty() && !probe.success) {
      return false;
    }
    const QString output = QString::fromUtf8(probe.stdoutData + probe.stderrData);
    const bool ptraceBlocked =
        output.contains("Could not trace the inferior process",
                        Qt::CaseInsensitive) ||
        output.contains("ptrace", Qt::CaseInsensitive) ||
        output.contains("Operation not permitted", Qt::CaseInsensitive);
    if (ptraceBlocked) {
      return false;
    }
    return probe.success;
#endif
  }

  static QString findSystemGdb() {
    const QStringList candidates = {"/usr/bin/gdb", "/usr/local/bin/gdb",
                                    "/opt/homebrew/bin/gdb",
                                    "/opt/local/bin/gdb"};
    for (const QString &path : candidates) {
      if (QFileInfo::exists(path) && QFileInfo(path).isExecutable()) {
        return path;
      }
    }

#ifdef Q_OS_WIN
    const ProcessProbeResult probe = runProcessProbe("where", {"gdb"}, 3000);
#else
    const ProcessProbeResult probe = runProcessProbe("which", {"gdb"}, 3000);
#endif
    if (probe.success) {
      const QString path = QString::fromUtf8(probe.stdoutData).trimmed();
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
    if (runProcessProbe(gdbPath, {"-batch", "-ex", "python print('ok')"}, 3000)
            .success) {
      capabilities << "Python";
    }

    const ProcessProbeResult archProbe =
        runProcessProbe(gdbPath, {"-batch", "-ex", "set architecture"}, 3000);
    if (!archProbe.stdoutData.isEmpty() || !archProbe.stderrData.isEmpty() ||
        archProbe.success) {
      const QString output =
          QString::fromUtf8(archProbe.stdoutData + archProbe.stderrData);
      if (output.contains("auto")) {
        capabilities << "Multi-arch";
      }
    }

    return capabilities.join(", ");
  }

  static QJsonObject createDefaultLaunchConfig() {
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

  static QJsonObject createDefaultAttachConfig() {
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
} // namespace

std::shared_ptr<IDebugAdapter> createGdbDebugAdapter() {
  return std::make_shared<GdbDebugAdapter>();
}
