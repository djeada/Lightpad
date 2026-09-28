#include "pythonprojectenvironment.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
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

QString preferredBaseDir(const QString &workspaceFolder,
                         const QString &filePath,
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

QString PythonProjectEnvironment::normalizePath(
    const QString &path, const QString &workspaceFolder,
    const QString &filePath, const QString &workingDirectory) {
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

QString
PythonProjectEnvironment::defaultVenvPath(const QString &workspaceFolder,
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
  const QString explicitPath = normalizePath(configuredPath, workspaceFolder,
                                             filePath, workingDirectory);
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

QString PythonProjectEnvironment::pythonExecutableInEnvironment(
    const QString &dirPath) {
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

  const QString explicitInterpreter =
      normalizePath(preference.customInterpreter, workspaceFolder, filePath,
                    workingDirectory);
  if (mode == customInterpreterMode()) {
    const QString resolved = resolveExecutablePath(explicitInterpreter);
    if (!resolved.isEmpty()) {
      info.interpreter = resolved;
      info.venvPath = parentVirtualEnv(resolved);
      info.venvBinPath = venvBinPath(info.venvPath);
      info.found = true;
      info.statusMessage = QString("Using custom interpreter %1")
                               .arg(QFileInfo(resolved).fileName());
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
    info.statusMessage =
        targetPath.isEmpty()
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
    info.statusMessage = QString("Using active environment %1")
                             .arg(QFileInfo(envInterpreter).fileName());
    return info;
  }

  const QString globalInterpreter = globalPythonInterpreter();
  if (!globalInterpreter.isEmpty()) {
    info.interpreter = globalInterpreter;
    info.found = true;
    info.statusMessage = QString("Using system interpreter %1")
                             .arg(QFileInfo(globalInterpreter).fileName());
    return info;
  }

  info.statusMessage = "Python 3 was not found.";
  return info;
}

PythonEnvironmentDiagnostics PythonProjectEnvironment::diagnostics(
    const PythonEnvironmentPreference &preference,
    const QString &workspaceFolder, const QString &filePath,
    const QString &workingDirectory) {
  PythonEnvironmentDiagnostics diagnostics;
  diagnostics.workspaceRoot =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  diagnostics.normalizedCustomInterpreter =
      normalizePath(preference.customInterpreter, workspaceFolder, filePath,
                    workingDirectory);
  diagnostics.normalizedConfiguredVenvPath = normalizePath(
      preference.venvPath, workspaceFolder, filePath, workingDirectory);
  diagnostics.resolvedRequirementsFile = defaultRequirementsPath(
      workspaceFolder, filePath, workingDirectory, preference.requirementsFile);
  diagnostics.activeEnvironmentInterpreter =
      activeEnvironmentPythonInterpreter();
  diagnostics.globalInterpreter = globalPythonInterpreter();
  diagnostics.searchedVenvPaths =
      candidateVirtualEnvPaths(workspaceFolder, filePath, workingDirectory,
                               diagnostics.normalizedConfiguredVenvPath);
  diagnostics.searchedRequirementsRoots =
      requirementsSearchRoots(workspaceFolder, filePath, workingDirectory);
  return diagnostics;
}

QMap<QString, QString> PythonProjectEnvironment::activationEnvironment(
    const PythonEnvironmentInfo &info) {
  QMap<QString, QString> env;
  if (info.venvPath.isEmpty() || info.venvBinPath.isEmpty()) {
    return env;
  }

  env["VIRTUAL_ENV"] = info.venvPath;

  const QString currentPath =
      QProcessEnvironment::systemEnvironment().value("PATH");
  if (currentPath.isEmpty()) {
    env["PATH"] = info.venvBinPath;
  } else {
    env["PATH"] = info.venvBinPath + pathSeparator() + currentPath;
  }

  return env;
}

QMap<QString, QString> PythonProjectEnvironment::variables(
    const QString &workspaceFolder, const QString &filePath,
    const QString &workingDirectory,
    const PythonEnvironmentPreference &preference) {
  const QFileInfo fileInfo(filePath);
  const QString workspaceRoot =
      workspaceRootForContext(workspaceFolder, filePath, workingDirectory);
  const PythonEnvironmentInfo info =
      resolve(preference, workspaceFolder, filePath, workingDirectory);

  QMap<QString, QString> vars;
  vars.insert("file", filePath);
  vars.insert("fileDir", fileInfo.absoluteDir().path());
  vars.insert("fileBasename", fileInfo.fileName());
  vars.insert("fileBasenameNoExt", fileInfo.completeBaseName());
  vars.insert("fileExt", fileInfo.suffix());
  vars.insert("workspaceFolder", workspaceRoot);
  vars.insert("python", info.interpreter);
  vars.insert("pythonInterpreter", info.interpreter);
  vars.insert("venv", info.venvPath);
  vars.insert("venvBin", info.venvBinPath);
  vars.insert("requirementsFile", info.requirementsFile);
  return vars;
}

QString PythonProjectEnvironment::substituteVariables(
    const QString &input, const QString &workspaceFolder,
    const QString &filePath, const QString &workingDirectory,
    const PythonEnvironmentPreference &preference) {
  if (input.isEmpty()) {
    return input;
  }

  QString result = input;
  const QMap<QString, QString> vars =
      variables(workspaceFolder, filePath, workingDirectory, preference);
  for (auto it = vars.constBegin(); it != vars.constEnd(); ++it) {
    result.replace("${" + it.key() + "}", it.value());
  }

  return result;
}

QString PythonProjectEnvironment::shellQuote(const QString &value) {
  QString escaped = value;
  escaped.replace("'", "'\\''");
  return "'" + escaped + "'";
}

QString PythonProjectEnvironment::substituteShellVariables(
    const QString &script, const QMap<QString, QString> &variables) {
  enum class Context { Unquoted, Double, Single, Backtick };
  struct Frame {
    Context context;
    bool commandSubstitution;
    int parenDepth;
    bool inBacktick;
  };

  QList<Frame> stack;
  stack.append(Frame{Context::Unquoted, false, 0, false});
  QString result;
  result.reserve(script.size());
  const int n = script.size();
  int i = 0;
  while (i < n) {
    Frame &top = stack.last();
    const QChar c = script.at(i);

    if (c == '$' && i + 1 < n && script.at(i + 1) == '{') {
      const int close = script.indexOf('}', i + 2);
      if (close > 0) {
        const QString name = script.mid(i + 2, close - i - 2);
        if (variables.contains(name)) {
          QString quoted = shellQuote(variables.value(name));
          if (top.inBacktick) {
            quoted.replace("\\", "\\\\");
            quoted.replace("`", "\\`");
            quoted.replace("$", "\\$");
          }
          if (top.context == Context::Double) {
            result += "\"" + quoted + "\"";
          } else if (top.context == Context::Single) {
            result += "'" + quoted + "'";
          } else {
            result += quoted;
          }
          i = close + 1;
          continue;
        }
      }
    }

    if (top.context == Context::Single) {
      if (c == '\'') {
        stack.removeLast();
      }
      result += c;
      ++i;
      continue;
    }

    if (c == '\\' && i + 1 < n) {
      result += c;
      result += script.at(i + 1);
      i += 2;
      continue;
    }

    if (c == '$' && i + 1 < n && script.at(i + 1) == '(') {
      stack.append(Frame{Context::Unquoted, true, 0, top.inBacktick});
      result += "$(";
      i += 2;
      continue;
    }

    if (c == '`') {
      if (top.context == Context::Backtick) {
        stack.removeLast();
      } else {
        stack.append(Frame{Context::Backtick, false, 0, true});
      }
      result += c;
      ++i;
      continue;
    }

    if (top.context == Context::Double) {
      if (c == '"') {
        stack.removeLast();
      }
      result += c;
      ++i;
      continue;
    }

    if (c == '#' && (i == 0 || script.at(i - 1).isSpace())) {
      int end = script.indexOf('\n', i);
      if (end < 0) {
        end = n;
      }
      result += script.mid(i, end - i);
      i = end;
      continue;
    }

    if (c == '"') {
      stack.append(Frame{Context::Double, false, 0, top.inBacktick});
    } else if (c == '\'') {
      stack.append(Frame{Context::Single, false, 0, top.inBacktick});
    } else if (c == '(') {
      ++top.parenDepth;
    } else if (c == ')') {
      if (top.parenDepth > 0) {
        --top.parenDepth;
      } else if (top.commandSubstitution && stack.size() > 1) {
        stack.removeLast();
      }
    }
    result += c;
    ++i;
  }

  return result;
}

QString PythonProjectEnvironment::substituteCmdVariables(
    const QString &script, const QMap<QString, QString> &variables) {
  QString result;
  result.reserve(script.size());
  bool inDouble = false;
  const int n = script.size();
  int i = 0;
  while (i < n) {
    const QChar c = script.at(i);
    if (c == '$' && i + 1 < n && script.at(i + 1) == '{') {
      const int close = script.indexOf('}', i + 2);
      if (close > 0) {
        const QString name = script.mid(i + 2, close - i - 2);
        if (variables.contains(name)) {
          QString value = variables.value(name);
          value.remove('"');
          result += inDouble ? value : "\"" + value + "\"";
          i = close + 1;
          continue;
        }
      }
    }
    if (c == '"') {
      inDouble = !inDouble;
    }
    result += c;
    ++i;
  }
  return result;
}

QString PythonProjectEnvironment::substituteCommandLineVariables(
    const QString &script, const QMap<QString, QString> &variables) {
#ifdef Q_OS_WIN
  return substituteCmdVariables(script, variables);
#else
  return substituteShellVariables(script, variables);
#endif
}

int PythonProjectEnvironment::shellScriptArgumentIndex(
    const QString &command, const QStringList &args) {
  static const QStringList shells = {"bash", "sh", "zsh", "dash", "ksh"};
  QString program = QFileInfo(command.trimmed()).fileName();
  if (program.endsWith(".exe", Qt::CaseInsensitive)) {
    program.chop(4);
  }
  if (!shells.contains(program)) {
    return -1;
  }

  static const QRegularExpression commandFlag("^-[A-Za-z]*c[A-Za-z]*$");
  for (int i = 0; i + 1 < args.size(); ++i) {
    const QString &arg = args.at(i);
    if (commandFlag.match(arg).hasMatch()) {
      return i + 1;
    }
    if (arg == "-o" || arg == "+o" || arg == "-O" || arg == "+O") {
      ++i;
      continue;
    }
    if (!arg.startsWith('-') && !arg.startsWith('+')) {
      return -1;
    }
  }
  return -1;
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
      if (!limit.isEmpty() && QDir::cleanPath(dir.absolutePath()) == limit) {
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
