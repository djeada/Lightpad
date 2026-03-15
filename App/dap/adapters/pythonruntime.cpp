#include "pythonruntime.h"

#include "adapterutils.h"

#include <QDir>
#include <QFileInfo>

namespace {
QString pythonExecutableInDir(const QString &dirPath) {
  if (dirPath.isEmpty()) {
    return {};
  }

  const QFileInfo baseDir(dirPath);
  if (!baseDir.exists() || !baseDir.isDir()) {
    return {};
  }

#ifdef Q_OS_WIN
  const QString pythonPath = QDir(dirPath).filePath("Scripts/python.exe");
#else
  const QString pythonPath = QDir(dirPath).filePath("bin/python");
#endif
  const QFileInfo pythonInfo(pythonPath);
  if (pythonInfo.exists() && pythonInfo.isExecutable()) {
    return pythonInfo.absoluteFilePath();
  }
  return {};
}

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

QString activeEnvironmentPythonInterpreter() {
  const QString virtualEnv = qEnvironmentVariable("VIRTUAL_ENV");
  if (!virtualEnv.isEmpty()) {
    const QString venvPython = pythonExecutableInDir(virtualEnv);
    if (!venvPython.isEmpty()) {
      return venvPython;
    }
  }

  const QString condaEnv = qEnvironmentVariable("CONDA_PREFIX");
  if (!condaEnv.isEmpty()) {
    const QString condaPython = pythonExecutableInDir(condaEnv);
    if (!condaPython.isEmpty()) {
      return condaPython;
    }
  }

  return {};
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

  const QStringList candidates = {
#ifdef Q_OS_WIN
      "py.exe",
      "python.exe",
      "python3.exe",
#else
      "/usr/bin/python3",
      "/usr/local/bin/python3",
      "python3",
      "python",
#endif
  };

  for (const QString &candidate : candidates) {
    const QString resolved = resolveExecutablePath(candidate);
    if (!resolved.isEmpty()) {
      return resolved;
    }
  }

  return {};
}

PythonInterpreterResolution
resolvePythonInterpreter(const DebugConfiguration *configuration) {
  if (configuration) {
    const QString configuredInterpreter =
        configuration->adapterConfig["python"].toString().trimmed();
    if (!configuredInterpreter.isEmpty()) {
      const QString resolved = resolveExecutablePath(configuredInterpreter);
      if (resolved.isEmpty()) {
        return {{},
                QString("Configured Python interpreter not found: %1. "
                        "Set a valid interpreter override in Debug Configurations.")
                    .arg(configuredInterpreter)};
      }
      if (!pythonHasDebugpy(resolved)) {
        return {{},
                QString("debugpy not installed for configured Python %1. "
                        "Run: %1 -m pip install debugpy")
                    .arg(resolved)};
      }
      return {resolved,
              QString("Ready (%1)").arg(QFileInfo(resolved).fileName())};
    }
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
    return {{},
            QString("debugpy not installed for global Python %1. "
                    "Run: %1 -m pip install debugpy, or set a Python interpreter "
                    "override in Debug Configurations.")
                .arg(globalPython)};
  }

  const QString envPython = activeEnvironmentPythonInterpreter();
  if (!envPython.isEmpty()) {
    if (pythonHasDebugpy(envPython)) {
      return {envPython,
              QString("Ready (%1)").arg(QFileInfo(envPython).fileName())};
    }
    return {{},
            QString("debugpy not installed for environment Python %1. "
                    "Run: %1 -m pip install debugpy, or set a Python interpreter "
                    "override in Debug Configurations.")
                .arg(envPython)};
  }

  return {{},
          "Python 3 not found. Install Python 3 or set a Python interpreter "
          "override in Debug Configurations."};
}

QString preferredPythonInterpreterCandidate(
    const DebugConfiguration *configuration) {
  if (configuration) {
    const QString configuredInterpreter =
        configuration->adapterConfig["python"].toString().trimmed();
    if (!configuredInterpreter.isEmpty()) {
      return resolveExecutablePath(configuredInterpreter);
    }
  }

  const QString globalPython = globalPythonInterpreter();
  if (!globalPython.isEmpty()) {
    return globalPython;
  }

  return activeEnvironmentPythonInterpreter();
}
