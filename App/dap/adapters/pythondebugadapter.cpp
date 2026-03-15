#include "builtinadapters.h"

#include "abstractdebugadapter.h"
#include "pythonruntime.h"

#include <QFileInfo>

namespace {
class PythonDebugAdapter : public AbstractDebugAdapter {
public:
  PythonDebugAdapter()
      : AbstractDebugAdapter(createBaseConfig(), "python", "Python Interpreter",
                             "/usr/bin/python3 or /path/to/venv/bin/python") {}

  DebugAdapterConfig configForConfiguration(
      const DebugConfiguration &configuration) const override {
    DebugAdapterConfig cfg = config();
    const QString python = preferredPythonInterpreterCandidate(&configuration);
    if (!python.isEmpty()) {
      cfg.program = python;
    }
    return cfg;
  }

  bool isAvailable() const override {
    return !resolvePythonInterpreter().interpreter.isEmpty();
  }

  bool isAvailableForConfiguration(
      const DebugConfiguration &configuration) const override {
    return !resolvePythonInterpreter(&configuration).interpreter.isEmpty();
  }

  QString statusMessage() const override {
    return resolvePythonInterpreter().statusMessage;
  }

  QString statusMessageForConfiguration(
      const DebugConfiguration &configuration) const override {
    return resolvePythonInterpreter(&configuration).statusMessage;
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "debugpy";
    config["request"] = "launch";
    config["program"] = filePath;
    config["console"] = "integratedTerminal";
    const QString python = preferredPythonInterpreterCandidate();
    if (!python.isEmpty()) {
      config["python"] = python;
    }

    if (!workingDir.isEmpty()) {
      config["cwd"] = workingDir;
    } else {
      config["cwd"] = QFileInfo(filePath).absolutePath();
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

  QString installCommand() const override {
    const QString python = globalPythonInterpreter();
    if (python.isEmpty()) {
      return "Install Python 3, then run: python3 -m pip install debugpy";
    }
    return QString("%1 -m pip install debugpy").arg(python);
  }

  QString documentationUrl() const override {
    return "https://github.com/microsoft/debugpy";
  }

private:
  static DebugAdapterConfig createBaseConfig() {
    DebugAdapterConfig cfg;
    cfg.id = "python-debugpy";
    cfg.name = "Python (debugpy)";
    cfg.type = "debugpy";
    cfg.program = preferredPythonInterpreterCandidate();
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
};
} // namespace

std::shared_ptr<IDebugAdapter> createPythonDebugAdapter() {
  return std::make_shared<PythonDebugAdapter>();
}
