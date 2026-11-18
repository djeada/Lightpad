#include "builtinadapters.h"

#include "abstractdebugadapter.h"
#include "pythonruntime.h"
#include "test_templates/pytestintegration.h"

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
    const QString python = preferredPythonInterpreterCandidate(
        &configuration, configuration.program, configuration.cwd);
    if (!python.isEmpty()) {
      cfg.program = python;
    }
    return cfg;
  }

  bool launchesBeforeInitialized() const override { return true; }

  bool isAvailable() const override {
    return !resolvePythonInterpreter().interpreter.isEmpty();
  }

  bool isAvailableForConfiguration(
      const DebugConfiguration &configuration) const override {
    return !resolvePythonInterpreter(&configuration, configuration.program,
                                     configuration.cwd)
                .interpreter.isEmpty();
  }

  QString statusMessage() const override {
    return resolvePythonInterpreter().statusMessage;
  }

  QString statusMessageForConfiguration(
      const DebugConfiguration &configuration) const override {
    return resolvePythonInterpreter(&configuration, configuration.program,
                                    configuration.cwd)
        .statusMessage;
  }

  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override {
    QJsonObject config;
    config["type"] = "debugpy";
    config["request"] = "launch";
    config["program"] = filePath;
    config["console"] = "integratedTerminal";
    config["justMyCode"] = false;
    config["subProcess"] = true;
    const QString python =
        preferredPythonInterpreterCandidate(nullptr, filePath, workingDir);
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

  QJsonObject
  launchArguments(const DebugConfiguration &configuration) const override {
    QJsonObject arguments =
        AbstractDebugAdapter::launchArguments(configuration);
    if (!arguments.value("module").toString().isEmpty())
      arguments.remove("program");

    const QString python = preferredPythonInterpreterCandidate(
        &configuration, configuration.program, configuration.cwd);
    if (!python.isEmpty()) {
      arguments["python"] = python;
    }

    QJsonObject env = arguments["env"].toObject();
    if (!env.contains("PYTHONUNBUFFERED")) {
      env["PYTHONUNBUFFERED"] = "1";
    }
    if (arguments.value("module") == "pytest" &&
        env.contains("LIGHTPAD_PYTEST_SELECTION")) {
      // Keep the selection plugin available for concurrent sessions and
      // restarts.
      static QTemporaryDir reporterDirectory;
      QProcessEnvironment reporterEnvironment;
      reporterEnvironment.insert(
          "PYTHONPATH",
          env.value("PYTHONPATH").toString(qEnvironmentVariable("PYTHONPATH")));
      QStringList pytestArgs;
      for (const auto &arg : arguments.value("args").toArray())
        pytestArgs << arg.toString();
      if (PytestIntegration::install(reporterDirectory, reporterEnvironment,
                                     pytestArgs, "pytest")) {
        arguments["args"] = QJsonArray::fromStringList(pytestArgs);
        env["PYTHONPATH"] = reporterEnvironment.value("PYTHONPATH");
        env["LIGHTPAD_PYTEST_REPORTING"] = "0";
      }
    }
    arguments["env"] = env;

    return arguments;
  }

  RunCommand
  runCommand(const DebugConfiguration &configuration) const override {
    RunCommand command = AbstractDebugAdapter::runCommand(configuration);
    const QString module =
        configuration.adapterConfig.value("module").toString().trimmed();
    if (configuration.request.compare("attach", Qt::CaseInsensitive) == 0) {
      return command;
    }
    command.error.clear();
    QString python =
        configuration.adapterConfig.value("python").toString().trimmed();
    if (python.isEmpty()) {
      python = preferredPythonInterpreterCandidate(
          &configuration, configuration.program, configuration.cwd);
    }
    if (python.isEmpty()) {
      command.error = QStringLiteral("No Python interpreter was found.");
      return command;
    }
    command.program = python;
    command.arguments.clear();
    if (!module.isEmpty()) {
      command.arguments << "-m" << module;
    } else if (!configuration.program.isEmpty()) {
      command.arguments << configuration.program;
    } else {
      command.error =
          QStringLiteral("Set the script or module the configuration runs.");
      return command;
    }
    command.arguments << configuration.args;
    const PythonEnvironmentPreference preference = {
        configuration.pythonMode, configuration.pythonInterpreter,
        configuration.pythonVenvPath, configuration.pythonRequirementsFile};
    command.environment = PythonProjectEnvironment::activationEnvironment(
        PythonProjectEnvironment::resolve(
            preference, DebugConfigurationManager::instance().workspaceFolder(),
            configuration.program, configuration.cwd));
    command.environment.insert("PYTHONUNBUFFERED", "1");
    return command;
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
