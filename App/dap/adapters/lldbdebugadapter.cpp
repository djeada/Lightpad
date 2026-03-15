#include "builtinadapters.h"

#include "processbackeddebugadapter.h"

#include <QFileInfo>

namespace {
class LldbDebugAdapter : public ProcessBackedDebugAdapter {
public:
  LldbDebugAdapter()
      : ProcessBackedDebugAdapter(createBaseConfig(), "miDebuggerPath",
                                  "Debugger Executable",
                                  "lldb-vscode or /path/to/lldb-vscode") {}

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "cppdbg";
    config["request"] = "launch";
    config["program"] = filePath;
    config["stopOnEntry"] = false;
    config["cwd"] = workingDir.isEmpty() ? QFileInfo(filePath).absolutePath()
                                         : workingDir;
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

private:
  static DebugAdapterConfig createBaseConfig() {
    DebugAdapterConfig cfg;
    cfg.id = "cppdbg-lldb";
    cfg.name = "C/C++ (LLDB)";
    cfg.type = "cppdbg";
    cfg.program = "lldb-vscode";
    cfg.languages = QStringList() << "cpp" << "c";
    cfg.extensions = QStringList()
                     << ".cpp" << ".cxx" << ".cc" << ".c" << ".h" << ".hpp";
    cfg.supportsFunctionBreakpoints = true;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsHitConditionalBreakpoints = true;
    return cfg;
  }

  QString
  adapterExecutableCandidate(const DebugConfiguration &configuration) const override {
    const QString configured = optionValue(configuration, adapterExecutableKey());
    if (!configured.isEmpty()) {
      return configured;
    }

    const QStringList candidates = {"lldb-vscode", "lldb-dap"};
    for (const QString &candidate : candidates) {
      if (!resolveExecutablePath(candidate).isEmpty()) {
        return candidate;
      }
    }
    return config().program;
  }

  QStringList
  executableProbeArguments(const DebugConfiguration &configuration) const override {
    Q_UNUSED(configuration);
    return {"--help"};
  }

  QString missingExecutableMessage(const DebugConfiguration &configuration,
                                   const QString &candidate) const override {
    Q_UNUSED(configuration);
    if (!candidate.isEmpty()) {
      return QString("LLDB adapter '%1' not found. Install LLDB with DAP support.")
          .arg(candidate);
    }
    return "LLDB adapter not found. Install LLDB with DAP support.";
  }
};
} // namespace

std::shared_ptr<IDebugAdapter> createLldbDebugAdapter() {
  return std::make_shared<LldbDebugAdapter>();
}
