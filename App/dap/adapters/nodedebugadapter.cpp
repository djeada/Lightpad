#include "builtinadapters.h"

#include "processbackeddebugadapter.h"

#include <QFileInfo>

namespace {
class NodeDebugAdapter : public ProcessBackedDebugAdapter {
public:
  NodeDebugAdapter()
      : ProcessBackedDebugAdapter(
            createBaseConfig(), "adapterCommand", "Adapter Command",
            "js-debug-adapter or /path/to/js-debug-adapter",
            {DebugAdapterOption{"runtimeExecutable", "Node Runtime",
                                "node or /path/to/node", true}}) {}

  bool isAvailableForConfiguration(
      const DebugConfiguration &configuration) const override {
    if (!ProcessBackedDebugAdapter::isAvailableForConfiguration(configuration)) {
      return false;
    }
    if (configuration.request.compare("launch", Qt::CaseInsensitive) != 0) {
      return true;
    }
    return !resolvedRuntimeExecutable(configuration).isEmpty();
  }

  QString statusMessageForConfiguration(
      const DebugConfiguration &configuration) const override {
    if (!ProcessBackedDebugAdapter::isAvailableForConfiguration(configuration)) {
      const QString candidate = adapterExecutableCandidate(configuration);
      if (candidate.isEmpty()) {
        return "Node DAP adapter command not configured. Set adapterCommand "
               "to a js-debug compatible adapter.";
      }
      return QString("Node DAP adapter '%1' not found. Set adapterCommand to "
                     "a js-debug compatible adapter.")
          .arg(candidate);
    }

    if (configuration.request.compare("launch", Qt::CaseInsensitive) == 0) {
      const QString runtimeCandidate = runtimeExecutableCandidate(configuration);
      if (resolvedRuntimeExecutable(configuration).isEmpty()) {
        if (runtimeCandidate.isEmpty()) {
          return "Node runtime not configured. Set runtimeExecutable to a "
                 "Node.js binary.";
        }
        return QString("Node runtime '%1' not found. Set runtimeExecutable to "
                       "a valid Node.js binary.")
            .arg(runtimeCandidate);
      }
    }

    return ProcessBackedDebugAdapter::statusMessageForConfiguration(configuration);
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "node";
    config["request"] = "launch";
    config["program"] = filePath;
    config["console"] = "integratedTerminal";
    config["cwd"] = workingDir.isEmpty() ? QFileInfo(filePath).absolutePath()
                                         : workingDir;
    const QString runtimeExecutable = preferredRuntimeExecutable();
    if (!runtimeExecutable.isEmpty()) {
      config["runtimeExecutable"] = runtimeExecutable;
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

protected:
  QStringList
  executableProbeArguments(const DebugConfiguration &configuration) const override {
    Q_UNUSED(configuration);
    return {"--help"};
  }

  QString readyMessage(const DebugConfiguration &configuration,
                       const QString &executable,
                       const ProcessProbeResult &probe) const override {
    const QString runtime = QFileInfo(resolvedRuntimeExecutable(configuration))
                                .fileName();
    const QString adapter = QFileInfo(executable).fileName();
    if (!runtime.isEmpty()) {
      return QString("Ready (%1 via %2)").arg(runtime, adapter);
    }
    return ProcessBackedDebugAdapter::readyMessage(configuration, executable,
                                                   probe);
  }

private:
  static DebugAdapterConfig createBaseConfig() {
    DebugAdapterConfig cfg;
    cfg.id = "node-debug";
    cfg.name = "Node.js";
    cfg.type = "node";
    cfg.program = "js-debug-adapter";
    cfg.languages = QStringList() << "js" << "ts";
    cfg.extensions = QStringList()
                     << ".js" << ".mjs" << ".cjs" << ".ts" << ".mts";
    cfg.supportsRestart = true;
    cfg.supportsConditionalBreakpoints = true;
    cfg.supportsLogPoints = true;
    return cfg;
  }

  QString preferredRuntimeExecutable() const {
    const QString configured =
        debugAdapterSettingValue(config().id, "runtimeExecutable");
    return configured.isEmpty() ? QStringLiteral("node") : configured;
  }

  QString runtimeExecutableCandidate(
      const DebugConfiguration &configuration) const {
    const QString configured = optionValue(configuration, "runtimeExecutable");
    if (!configured.isEmpty()) {
      return configured;
    }
    return preferredRuntimeExecutable();
  }

  QString resolvedRuntimeExecutable(
      const DebugConfiguration &configuration) const {
    return resolveExecutablePath(runtimeExecutableCandidate(configuration));
  }
};
} // namespace

std::shared_ptr<IDebugAdapter> createNodeDebugAdapter() {
  return std::make_shared<NodeDebugAdapter>();
}
