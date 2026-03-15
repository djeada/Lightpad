#include "builtinadapters.h"

#include "processbackeddebugadapter.h"

#include <QFileInfo>

namespace {
class GoDebugAdapter : public ProcessBackedDebugAdapter {
public:
  GoDebugAdapter()
      : ProcessBackedDebugAdapter(
            createBaseConfig(), "dlvPath", "Delve Executable",
            "dlv or /path/to/dlv",
            {DebugAdapterOption{"goExecutable", "Go Runtime",
                                "go or /path/to/go", true}}) {}

  DebugAdapterConfig configForConfiguration(
      const DebugConfiguration &configuration) const override {
    DebugAdapterConfig cfg =
        ProcessBackedDebugAdapter::configForConfiguration(configuration);
    cfg.arguments = QStringList() << "dap";
    return cfg;
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "go";
    config["request"] = "launch";
    config["mode"] = "debug";
    config["program"] = filePath;
    config["cwd"] =
        workingDir.isEmpty() ? QFileInfo(filePath).absolutePath() : workingDir;

    const QString dlvPath = preferredDlvExecutable();
    if (!dlvPath.isEmpty()) {
      config["dlvToolPath"] = dlvPath;
    }

    return config;
  }

  QJsonObject createAttachConfig(int processId, const QString &host,
                                 int port) const override {
    QJsonObject config;
    config["type"] = "go";
    config["request"] = "attach";
    config["mode"] = "local";

    if (processId > 0) {
      config["processId"] = processId;
    } else if (!host.isEmpty() && port > 0) {
      config["mode"] = "remote";
      config["host"] = host;
      config["port"] = port;
    }

    return config;
  }

  QString installCommand() const override {
    return "go install github.com/go-delve/delve/cmd/dlv@latest";
  }

  QString documentationUrl() const override {
    return "https://github.com/go-delve/delve";
  }

protected:
  QStringList executableProbeArguments(
      const DebugConfiguration &configuration) const override {
    Q_UNUSED(configuration);
    return {"version"};
  }

  QString readyMessage(const DebugConfiguration &configuration,
                       const QString &executable,
                       const ProcessProbeResult &probe) const override {
    const QString line = firstOutputLine(probe.stdoutData, probe.stderrData);
    if (!line.isEmpty()) {
      return QString("Ready (%1)").arg(line);
    }
    return ProcessBackedDebugAdapter::readyMessage(configuration, executable,
                                                   probe);
  }

private:
  static DebugAdapterConfig createBaseConfig() {
    DebugAdapterConfig cfg;
    cfg.id = "go-delve";
    cfg.name = "Go (Delve)";
    cfg.type = "go";
    cfg.program = "dlv";
    cfg.arguments = QStringList() << "dap";
    cfg.languages = QStringList() << "go";
    cfg.extensions = QStringList() << ".go";
    cfg.supportsRestart = true;
    cfg.supportsFunctionBreakpoints = true;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsHitConditionalBreakpoints = true;
    cfg.supportsLogPoints = true;
    return cfg;
  }

  QString preferredDlvExecutable() const {
    const QString configured =
        debugAdapterSettingValue(config().id, "dlvPath");
    return configured.isEmpty() ? QStringLiteral("dlv") : configured;
  }
};
} // namespace

std::shared_ptr<IDebugAdapter> createGoDebugAdapter() {
  return std::make_shared<GoDebugAdapter>();
}
