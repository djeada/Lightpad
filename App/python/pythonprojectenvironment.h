#ifndef PYTHONPROJECTENVIRONMENT_H
#define PYTHONPROJECTENVIRONMENT_H

#include <QMap>
#include <QString>
#include <QStringList>

struct PythonEnvironmentPreference {
  QString mode;
  QString customInterpreter;
  QString venvPath;
  QString requirementsFile;
};

struct PythonEnvironmentInfo {
  QString interpreter;
  QString venvPath;
  QString venvBinPath;
  QString requirementsFile;
  QString statusMessage;
  bool found = false;
  bool fromWorkspace = false;

  bool isVirtualEnvironment() const { return !venvPath.isEmpty(); }
};

struct PythonEnvironmentDiagnostics {
  QString workspaceRoot;
  QString normalizedCustomInterpreter;
  QString normalizedConfiguredVenvPath;
  QString resolvedRequirementsFile;
  QString activeEnvironmentInterpreter;
  QString globalInterpreter;
  QStringList searchedVenvPaths;
  QStringList searchedRequirementsRoots;
};

struct PythonInstallPlan {
  QStringList arguments;
  QString description;
  QString workingDirectory;
};

class PythonProjectEnvironment {
public:
  static QString autoMode();
  static QString workspaceVenvMode();
  static QString customInterpreterMode();

  static QString normalizePath(const QString &path,
                               const QString &workspaceFolder = {},
                               const QString &filePath = {},
                               const QString &workingDirectory = {});

  static QString defaultVenvPath(const QString &workspaceFolder,
                                 const QString &filePath = {},
                                 const QString &workingDirectory = {});
  static QString defaultRequirementsPath(const QString &workspaceFolder,
                                         const QString &filePath = {},
                                         const QString &workingDirectory = {},
                                         const QString &configuredPath = {});

  static QString pythonExecutableInEnvironment(const QString &dirPath);
  static QString venvBinPath(const QString &venvPath);
  static QString globalPythonInterpreter();

  static PythonEnvironmentInfo
  resolve(const PythonEnvironmentPreference &preference,
          const QString &workspaceFolder = {}, const QString &filePath = {},
          const QString &workingDirectory = {});

  static PythonEnvironmentDiagnostics
  diagnostics(const PythonEnvironmentPreference &preference,
              const QString &workspaceFolder = {},
              const QString &filePath = {},
              const QString &workingDirectory = {});

  static QMap<QString, QString>
  activationEnvironment(const PythonEnvironmentInfo &info);

  static QString substituteVariables(
      const QString &input, const QString &workspaceFolder = {},
      const QString &filePath = {}, const QString &workingDirectory = {},
      const PythonEnvironmentPreference &preference = {});

  static PythonInstallPlan
  requirementsInstallPlan(const PythonEnvironmentInfo &info,
                          const QString &workspaceFolder = {},
                          const QString &filePath = {},
                          const QString &workingDirectory = {},
                          const QString &configuredPath = {});

private:
  static QString activeEnvironmentPythonInterpreter();
  static QStringList
  candidateVirtualEnvPaths(const QString &workspaceFolder,
                           const QString &filePath = {},
                           const QString &workingDirectory = {},
                           const QString &preferredVenvPath = {});
  static QString workspaceRootForContext(const QString &workspaceFolder,
                                         const QString &filePath = {},
                                         const QString &workingDirectory = {});
  static QStringList requirementsSearchRoots(const QString &workspaceFolder,
                                             const QString &filePath = {},
                                             const QString &workingDirectory = {});
  static QString parentVirtualEnv(const QString &interpreterPath);
};

#endif
