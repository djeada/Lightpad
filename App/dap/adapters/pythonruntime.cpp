#include "pythonruntime.h"

#include "adapterutils.h"
#include "../../python/pythonprojectenvironment.h"
#include "../debugconfiguration.h"

#include <QFileInfo>
namespace {
QString configuredPythonInterpreterSetting() {
  const QStringList keys = {"python", "pythonInterpreter", "interpreter"};
  for (const QString &key : keys) {
    const QString configured = debugAdapterSettingValue("python-debugpy", key);
    if (!configured.isEmpty()) {
      return configured;
    }
  }

  return {};
}

PythonEnvironmentPreference
preferenceFromConfiguration(const DebugConfiguration *configuration) {
  PythonEnvironmentPreference preference;
  if (!configuration) {
    return preference;
  }

  preference.mode = configuration->pythonMode;
  preference.customInterpreter = configuration->pythonInterpreter;
  preference.venvPath = configuration->pythonVenvPath;
  preference.requirementsFile = configuration->pythonRequirementsFile;

  const QString adapterInterpreter =
      configuration->adapterConfig["python"].toString().trimmed();
  if (!adapterInterpreter.isEmpty()) {
    preference.mode = PythonProjectEnvironment::customInterpreterMode();
    preference.customInterpreter = adapterInterpreter;
  }

  return preference;
}

bool pythonHasDebugpy(const QString &pythonExecutable) {
  const ProcessProbeResult probe =
      runProcessProbe(pythonExecutable, {"-c", "import debugpy; print('ok')"});
  return probe.success;
}
} // namespace

QString globalPythonInterpreter() {
  const QString configured = configuredPythonInterpreterSetting();
  const QString resolvedConfigured = resolveExecutablePath(configured);
  if (!resolvedConfigured.isEmpty()) {
    return resolvedConfigured;
  }

  return PythonProjectEnvironment::globalPythonInterpreter();
}

PythonInterpreterResolution
resolvePythonInterpreter(const DebugConfiguration *configuration,
                         const QString &filePath,
                         const QString &workingDirectory) {
  const QString workspaceFolder =
      DebugConfigurationManager::instance().workspaceFolder();
  const PythonEnvironmentPreference preference =
      preferenceFromConfiguration(configuration);
  const PythonEnvironmentInfo preferredEnvironment =
      PythonProjectEnvironment::resolve(
          preference, workspaceFolder,
          configuration && !configuration->program.isEmpty()
              ? configuration->program
              : filePath,
          configuration && !configuration->cwd.isEmpty() ? configuration->cwd
                                                         : workingDirectory);

  if (preferredEnvironment.found && !preferredEnvironment.interpreter.isEmpty()) {
    if (!pythonHasDebugpy(preferredEnvironment.interpreter)) {
      return {{},
              QString("debugpy not installed for %1. Run: %1 -m pip install "
                      "debugpy")
                  .arg(preferredEnvironment.interpreter)};
    }
    return {preferredEnvironment.interpreter,
            QString("Ready (%1)")
                .arg(QFileInfo(preferredEnvironment.interpreter).fileName())};
  }

  if (preference.mode == PythonProjectEnvironment::customInterpreterMode() &&
      !preference.customInterpreter.trimmed().isEmpty()) {
    return {{},
            QString("Configured Python interpreter not found: %1. "
                    "Set a valid interpreter override in Debug Configurations.")
                .arg(preference.customInterpreter)};
  }

  const QString globalPython = globalPythonInterpreter();
  const QString configuredGlobalPython = configuredPythonInterpreterSetting();
  if (!configuredGlobalPython.isEmpty() && globalPython.isEmpty()) {
    return {{},
            QString("Configured global Python interpreter not found: %1. "
                    "Fix adapters.json or set a Python interpreter override in "
                    "Debug Configurations.")
                .arg(configuredGlobalPython)};
  }

  if (!globalPython.isEmpty()) {
    if (pythonHasDebugpy(globalPython)) {
      return {globalPython,
              QString("Ready (%1)").arg(QFileInfo(globalPython).fileName())};
    }
    return {
        {},
        QString("debugpy not installed for global Python %1. "
                "Run: %1 -m pip install debugpy, or set a Python interpreter "
                "override in Debug Configurations.")
            .arg(globalPython)};
  }

  return {{},
          preferredEnvironment.statusMessage.isEmpty()
              ? "Python 3 not found. Install Python 3 or set a Python "
                "interpreter override in Debug Configurations."
              : preferredEnvironment.statusMessage};
}

QString
preferredPythonInterpreterCandidate(const DebugConfiguration *configuration,
                                    const QString &filePath,
                                    const QString &workingDirectory) {
  const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
      preferenceFromConfiguration(configuration),
      DebugConfigurationManager::instance().workspaceFolder(),
      configuration && !configuration->program.isEmpty() ? configuration->program
                                                         : filePath,
      configuration && !configuration->cwd.isEmpty() ? configuration->cwd
                                                     : workingDirectory);
  if (info.found && !info.interpreter.isEmpty()) {
    return info.interpreter;
  }

  const QString globalPython = globalPythonInterpreter();
  return globalPython;
}
