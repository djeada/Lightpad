#include "pythonprojectenvironment.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSet>
#include <QStandardPaths>

namespace {
QString resolveExecutablePath(const QString &candidate) {
  if (candidate.trimmed().isEmpty()) {
    return {};
  }

  const QFileInfo fileInfo(candidate);
  if (fileInfo.exists() && fileInfo.isExecutable()) {
    return fileInfo.absoluteFilePath();
  }

  return QStandardPaths::findExecutable(candidate.trimmed());
}

QString pathSeparator() {
#ifdef Q_OS_WIN
  return ";";
#else
  return ":";
#endif
}

QStringList venvDirectoryNames() { return {".venv", "venv", "env"}; }

QStringList requirementsCandidates() {
  return {"requirements.txt", "requirements-dev.txt", "pyproject.toml",
          "setup.py"};
}

QString preferredBaseDir(const QString &workspaceFolder, const QString &filePath,
                         const QString &workingDirectory) {
  if (!workspaceFolder.trimmed().isEmpty()) {
    return QFileInfo(workspaceFolder).absoluteFilePath();
  }

  if (!workingDirectory.trimmed().isEmpty()) {
    return QFileInfo(workingDirectory).absoluteFilePath();
  }

  if (!filePath.trimmed().isEmpty()) {
    return QFileInfo(filePath).absolutePath();
  }

  return {};
}
} // namespace

QString PythonProjectEnvironment::autoMode() { return "auto"; }

QString PythonProjectEnvironment::workspaceVenvMode() {
  return "workspaceVenv";
}

QString PythonProjectEnvironment::customInterpreterMode() {
  return "customInterpreter";
}

QString PythonProjectEnvironment::normalizePath(const QString &path,
                                                const QString &workspaceFolder,
                                                const QString &filePath,
                                                const QString &workingDirectory) {
  QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  const QString workspaceRoot =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  const QFileInfo fileInfo(filePath);
  trimmed.replace("${workspaceFolder}", workspaceRoot);
  trimmed.replace("${fileDir}", fileInfo.absolutePath());
  trimmed.replace("${file}", filePath);
  trimmed.replace("${fileBasename}", fileInfo.fileName());
  trimmed.replace("${fileBasenameNoExt}", fileInfo.completeBaseName());

  const QFileInfo directInfo(trimmed);
  if (directInfo.isAbsolute()) {
    return directInfo.absoluteFilePath();
  }

  if (!trimmed.contains('/') && !trimmed.contains('\\')) {
    return trimmed;
  }

  const QString baseDir = workspaceRoot;
  if (baseDir.isEmpty()) {
    return QDir::cleanPath(trimmed);
  }

  return QDir(baseDir).absoluteFilePath(trimmed);
}

QString PythonProjectEnvironment::defaultVenvPath(const QString &workspaceFolder,
                                                  const QString &filePath,
                                                  const QString &workingDirectory) {
  const QString root =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  if (root.isEmpty()) {
    return {};
  }
  return QDir(root).absoluteFilePath(".venv");
}

QString PythonProjectEnvironment::defaultRequirementsPath(
    const QString &workspaceFolder, const QString &filePath,
    const QString &workingDirectory, const QString &configuredPath) {
  const QString explicitPath =
      normalizePath(configuredPath, workspaceFolder, filePath, workingDirectory);
  if (!explicitPath.isEmpty()) {
    return explicitPath;
  }

  const QStringList searchRoots =
      requirementsSearchRoots(workspaceFolder, filePath, workingDirectory);
  QSet<QString> seen;
  for (const QString &root : searchRoots) {
    const QString normalizedRoot = QDir::cleanPath(root);
    if (normalizedRoot.isEmpty() || seen.contains(normalizedRoot)) {
      continue;
    }
    seen.insert(normalizedRoot);

    const QDir dir(normalizedRoot);
    for (const QString &candidateName : requirementsCandidates()) {
      const QString candidatePath = dir.absoluteFilePath(candidateName);
      if (QFileInfo(candidatePath).isFile()) {
        return candidatePath;
      }
    }
  }

  return {};
}

QString
PythonProjectEnvironment::pythonExecutableInEnvironment(const QString &dirPath) {
  if (dirPath.trimmed().isEmpty()) {
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

QString PythonProjectEnvironment::venvBinPath(const QString &venvPath) {
  if (venvPath.trimmed().isEmpty()) {
    return {};
  }

#ifdef Q_OS_WIN
  return QDir(venvPath).absoluteFilePath("Scripts");
#else
  return QDir(venvPath).absoluteFilePath("bin");
#endif
}

QString PythonProjectEnvironment::globalPythonInterpreter() {
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

PythonEnvironmentInfo
PythonProjectEnvironment::resolve(const PythonEnvironmentPreference &preference,
                                  const QString &workspaceFolder,
                                  const QString &filePath,
                                  const QString &workingDirectory) {
  PythonEnvironmentInfo info;
  info.requirementsFile = defaultRequirementsPath(
      workspaceFolder, filePath, workingDirectory, preference.requirementsFile);

  const QString mode =
      preference.mode.trimmed().isEmpty() ? autoMode() : preference.mode;

  const QString explicitInterpreter = normalizePath(
      preference.customInterpreter, workspaceFolder, filePath, workingDirectory);
  if (mode == customInterpreterMode()) {
    const QString resolved = resolveExecutablePath(explicitInterpreter);
    if (!resolved.isEmpty()) {
      info.interpreter = resolved;
      info.venvPath = parentVirtualEnv(resolved);
      info.venvBinPath = venvBinPath(info.venvPath);
      info.found = true;
      info.statusMessage =
          QString("Using custom interpreter %1").arg(QFileInfo(resolved).fileName());
      return info;
    }

    info.statusMessage = explicitInterpreter.isEmpty()
                             ? "Select a custom Python interpreter."
                             : QString("Custom interpreter not found: %1")
                                   .arg(explicitInterpreter);
    return info;
  }

  const QString configuredVenv = normalizePath(
      preference.venvPath, workspaceFolder, filePath, workingDirectory);
  const QStringList venvCandidates = candidateVirtualEnvPaths(
      workspaceFolder, filePath, workingDirectory, configuredVenv);
  for (const QString &candidate : venvCandidates) {
    const QString interpreter = pythonExecutableInEnvironment(candidate);
    if (!interpreter.isEmpty()) {
      info.interpreter = interpreter;
      info.venvPath = candidate;
      info.venvBinPath = venvBinPath(candidate);
      info.found = true;
      info.fromWorkspace = true;
      info.statusMessage = QString("Using project environment %1")
                               .arg(QFileInfo(candidate).fileName());
      return info;
    }
  }

  if (mode == workspaceVenvMode()) {
    const QString targetPath =
        configuredVenv.isEmpty()
            ? defaultVenvPath(workspaceFolder, filePath, workingDirectory)
            : configuredVenv;
    info.venvPath = targetPath;
    info.venvBinPath = venvBinPath(targetPath);
    info.statusMessage = targetPath.isEmpty()
                             ? "Workspace virtual environment is not configured."
                             : QString("Workspace virtual environment not found: %1")
                                   .arg(targetPath);
    return info;
  }

  const QString envInterpreter = activeEnvironmentPythonInterpreter();
  if (!envInterpreter.isEmpty()) {
    info.interpreter = envInterpreter;
    info.venvPath = parentVirtualEnv(envInterpreter);
    info.venvBinPath = venvBinPath(info.venvPath);
    info.found = true;
    info.statusMessage =
        QString("Using active environment %1").arg(QFileInfo(envInterpreter).fileName());
    return info;
  }

  const QString globalInterpreter = globalPythonInterpreter();
  if (!globalInterpreter.isEmpty()) {
    info.interpreter = globalInterpreter;
    info.found = true;
    info.statusMessage =
        QString("Using system interpreter %1")
            .arg(QFileInfo(globalInterpreter).fileName());
    return info;
  }

  info.statusMessage = "Python 3 was not found.";
  return info;
}

PythonEnvironmentDiagnostics PythonProjectEnvironment::diagnostics(
    const PythonEnvironmentPreference &preference, const QString &workspaceFolder,
    const QString &filePath, const QString &workingDirectory) {
  PythonEnvironmentDiagnostics diagnostics;
  diagnostics.workspaceRoot =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  diagnostics.normalizedCustomInterpreter = normalizePath(
      preference.customInterpreter, workspaceFolder, filePath, workingDirectory);
  diagnostics.normalizedConfiguredVenvPath = normalizePath(
      preference.venvPath, workspaceFolder, filePath, workingDirectory);
  diagnostics.resolvedRequirementsFile = defaultRequirementsPath(
      workspaceFolder, filePath, workingDirectory, preference.requirementsFile);
  diagnostics.activeEnvironmentInterpreter = activeEnvironmentPythonInterpreter();
  diagnostics.globalInterpreter = globalPythonInterpreter();
  diagnostics.searchedVenvPaths =
      candidateVirtualEnvPaths(workspaceFolder, filePath, workingDirectory,
                               diagnostics.normalizedConfiguredVenvPath);
  diagnostics.searchedRequirementsRoots =
      requirementsSearchRoots(workspaceFolder, filePath, workingDirectory);
  return diagnostics;
}

QMap<QString, QString>
PythonProjectEnvironment::activationEnvironment(const PythonEnvironmentInfo &info) {
  QMap<QString, QString> env;
  if (info.venvPath.isEmpty() || info.venvBinPath.isEmpty()) {
    return env;
  }

  env["VIRTUAL_ENV"] = info.venvPath;

  const QString currentPath = QProcessEnvironment::systemEnvironment().value("PATH");
  if (currentPath.isEmpty()) {
    env["PATH"] = info.venvBinPath;
  } else {
    env["PATH"] = info.venvBinPath + pathSeparator() + currentPath;
  }

  return env;
}

QString PythonProjectEnvironment::substituteVariables(
    const QString &input, const QString &workspaceFolder, const QString &filePath,
    const QString &workingDirectory, const PythonEnvironmentPreference &preference) {
  if (input.isEmpty()) {
    return input;
  }

  QString result = input;
  const QFileInfo fileInfo(filePath);
  const QString workspaceRoot =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  const PythonEnvironmentInfo info =
      resolve(preference, workspaceFolder, filePath, workingDirectory);

  result.replace("${file}", filePath);
  result.replace("${fileDir}", fileInfo.absoluteDir().path());
  result.replace("${fileBasename}", fileInfo.fileName());
  result.replace("${fileBasenameNoExt}", fileInfo.completeBaseName());
  result.replace("${fileExt}", fileInfo.suffix());
  result.replace("${workspaceFolder}", workspaceRoot);
  result.replace("${python}", info.interpreter);
  result.replace("${pythonInterpreter}", info.interpreter);
  result.replace("${venv}", info.venvPath);
  result.replace("${venvBin}", info.venvBinPath);
  result.replace("${requirementsFile}", info.requirementsFile);

  return result;
}

PythonInstallPlan PythonProjectEnvironment::requirementsInstallPlan(
    const PythonEnvironmentInfo &info, const QString &workspaceFolder,
    const QString &filePath, const QString &workingDirectory,
    const QString &configuredPath) {
  PythonInstallPlan plan;
  if (!info.found || info.interpreter.isEmpty()) {
    return plan;
  }

  const QString requirementsPath = defaultRequirementsPath(
      workspaceFolder, filePath, workingDirectory, configuredPath);
  if (requirementsPath.isEmpty()) {
    return plan;
  }

  const QFileInfo requirementsInfo(requirementsPath);
  const QString baseName = requirementsInfo.fileName();
  if (baseName == "pyproject.toml" || baseName == "setup.py") {
    const QString root = requirementsInfo.absoluteDir().absolutePath();
    plan.arguments = {"-m", "pip", "install", "-e", root};
    plan.description = QString("Install editable project from %1").arg(root);
    plan.workingDirectory = root;
    return plan;
  }

  plan.arguments = {"-m", "pip", "install", "-r", requirementsPath};
  plan.description =
      QString("Install requirements from %1").arg(requirementsPath);
  plan.workingDirectory = requirementsInfo.absolutePath();
  return plan;
}

QString PythonProjectEnvironment::activeEnvironmentPythonInterpreter() {
  const QString virtualEnv = qEnvironmentVariable("VIRTUAL_ENV");
  if (!virtualEnv.isEmpty()) {
    const QString venvPython = pythonExecutableInEnvironment(virtualEnv);
    if (!venvPython.isEmpty()) {
      return venvPython;
    }
  }

  const QString condaEnv = qEnvironmentVariable("CONDA_PREFIX");
  if (!condaEnv.isEmpty()) {
    const QString condaPython = pythonExecutableInEnvironment(condaEnv);
    if (!condaPython.isEmpty()) {
      return condaPython;
    }
  }

  return {};
}

QStringList PythonProjectEnvironment::candidateVirtualEnvPaths(
    const QString &workspaceFolder, const QString &filePath,
    const QString &workingDirectory, const QString &preferredVenvPath) {
  QStringList candidates;
  QSet<QString> seen;

  auto addCandidate = [&candidates, &seen](const QString &path) {
    const QString normalized = QDir::cleanPath(path);
    if (normalized.isEmpty() || seen.contains(normalized)) {
      return;
    }
    seen.insert(normalized);
    candidates.append(normalized);
  };

  if (!preferredVenvPath.trimmed().isEmpty()) {
    addCandidate(preferredVenvPath);
  }

  const QString workspaceRoot =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  if (!workspaceRoot.isEmpty()) {
    for (const QString &name : venvDirectoryNames()) {
      addCandidate(QDir(workspaceRoot).absoluteFilePath(name));
    }
  }

  QString currentDirPath =
      !filePath.trimmed().isEmpty()
          ? QFileInfo(filePath).absolutePath()
          : (!workingDirectory.trimmed().isEmpty()
                 ? QFileInfo(workingDirectory).absoluteFilePath()
                 : QString());

  if (!currentDirPath.isEmpty()) {
    QDir dir(currentDirPath);
    const QString limit =
        workspaceRoot.isEmpty() ? QString() : QDir::cleanPath(workspaceRoot);
    while (dir.exists()) {
      for (const QString &name : venvDirectoryNames()) {
        addCandidate(dir.absoluteFilePath(name));
      }
      if (!limit.isEmpty() &&
          QDir::cleanPath(dir.absolutePath()) == limit) {
        break;
      }
      if (!dir.cdUp()) {
        break;
      }
    }
  }

  return candidates;
}

QString PythonProjectEnvironment::workspaceRootForContext(
    const QString &workspaceFolder, const QString &filePath,
    const QString &workingDirectory) {
  const QString preferred =
      preferredBaseDir(workspaceFolder, filePath, workingDirectory);
  return preferred.isEmpty() ? QString() : QDir::cleanPath(preferred);
}

QStringList PythonProjectEnvironment::requirementsSearchRoots(
    const QString &workspaceFolder, const QString &filePath,
    const QString &workingDirectory) {
  QStringList searchRoots;
  const QString workspaceRoot =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  if (!workspaceRoot.isEmpty()) {
    searchRoots.append(workspaceRoot);
  }

  if (!workingDirectory.trimmed().isEmpty()) {
    searchRoots.append(QFileInfo(workingDirectory).absoluteFilePath());
  }

  if (!filePath.trimmed().isEmpty()) {
    QDir dir(QFileInfo(filePath).absolutePath());
    const QString limit = workspaceRoot.isEmpty() ? QString() : workspaceRoot;
    while (dir.exists()) {
      searchRoots.append(dir.absolutePath());
      if (!limit.isEmpty() &&
          QDir::cleanPath(dir.absolutePath()) == QDir::cleanPath(limit)) {
        break;
      }
      if (!dir.cdUp()) {
        break;
      }
    }
  }

  return searchRoots;
}

QString
PythonProjectEnvironment::parentVirtualEnv(const QString &interpreterPath) {
  QFileInfo info(interpreterPath);
  if (!info.exists()) {
    return {};
  }

  QDir dir = info.absoluteDir();
  const QString dirName = dir.dirName();
  if (dirName == "bin" || dirName == "Scripts") {
    if (dir.cdUp()) {
      return dir.absolutePath();
    }
  }

  return {};
}
