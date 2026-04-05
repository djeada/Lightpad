#include "builtinadapters.h"

#include "processbackeddebugadapter.h"

#include <QFileInfo>

namespace {
class RustDebugAdapter : public ProcessBackedDebugAdapter {
public:
  RustDebugAdapter()
      : ProcessBackedDebugAdapter(
            createBaseConfig(), "adapterCommand", "Adapter Command",
            "codelldb or /path/to/codelldb",
            {DebugAdapterOption{"cargoExecutable", "Cargo Path",
                                "cargo or /path/to/cargo", true}}) {}

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "lldb";
    config["request"] = "launch";
    config["program"] = filePath;
    config["cwd"] =
        workingDir.isEmpty() ? QFileInfo(filePath).absolutePath() : workingDir;
    config["args"] = QJsonArray();
    config["cargo"] = QJsonObject();
    return config;
  }

  QJsonObject createAttachConfig(int processId, const QString &host,
                                 int port) const override {
    QJsonObject config;
    config["type"] = "lldb";
    config["request"] = "attach";

    if (processId > 0) {
      config["pid"] = processId;
    } else if (!host.isEmpty() && port > 0) {
      QJsonObject connect;
      connect["host"] = host;
      connect["port"] = port;
      config["connect"] = connect;
    }

    return config;
  }

  QString installCommand() const override {
    return "Install the CodeLLDB extension or build from source: "
           "https://github.com/vadimcn/codelldb";
  }

  QString documentationUrl() const override {
    return "https://github.com/vadimcn/codelldb";
  }

protected:
  QStringList executableProbeArguments(
      const DebugConfiguration &configuration) const override {
    Q_UNUSED(configuration);
    return {"--help"};
  }

  QString adapterExecutableCandidate(
      const DebugConfiguration &configuration) const override {
    const QString configured =
        optionValue(configuration, adapterExecutableKey());
    if (!configured.isEmpty()) {
      return configured;
    }

    const QStringList candidates = {"codelldb", "lldb-vscode", "lldb-dap"};
    for (const QString &candidate : candidates) {
      if (!resolveExecutablePath(candidate).isEmpty()) {
        return candidate;
      }
    }
    return config().program;
  }

  QString missingExecutableMessage(const DebugConfiguration &configuration,
                                   const QString &candidate) const override {
    Q_UNUSED(configuration);
    if (!candidate.isEmpty()) {
      return QString("Rust adapter '%1' not found. Install CodeLLDB or set "
                     "adapterCommand to a compatible LLDB DAP adapter.")
          .arg(candidate);
    }
    return "Rust debug adapter not found. Install CodeLLDB or a compatible "
           "LLDB DAP adapter.";
  }

private:
  static DebugAdapterConfig createBaseConfig() {
    DebugAdapterConfig cfg;
    cfg.id = "rust-codelldb";
    cfg.name = "Rust (CodeLLDB)";
    cfg.type = "lldb";
    cfg.program = "codelldb";
    cfg.languages = QStringList() << "rust";
    cfg.extensions = QStringList() << ".rs";
    cfg.supportsRestart = false;
    cfg.supportsFunctionBreakpoints = true;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsHitConditionalBreakpoints = true;
    cfg.supportsLogPoints = true;
    return cfg;
  }
};
} // namespace

std::shared_ptr<IDebugAdapter> createRustDebugAdapter() {
  return std::make_shared<RustDebugAdapter>();
}
