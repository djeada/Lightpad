#ifndef DEBUGCONFIGURATION_H
#define DEBUGCONFIGURATION_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

#include "../python/pythonprojectenvironment.h"
struct TestConfiguration;

struct DebugConfiguration {
  QString name;
  QString adapterId;
  QString type;
  QString request;

  QString program;
  QStringList args;
  QString cwd;
  QMap<QString, QString> env;
  bool stopOnEntry = false;

  int processId = 0;
  QString processIdExpression;
  QString host;
  int port = 0;

  QString preLaunchTask;
  QString postDebugTask;
  QString pythonMode;
  QString pythonInterpreter;
  QString pythonVenvPath;
  QString pythonRequirementsFile;

  QJsonObject adapterConfig;

  QString presentation;
  int order = 0;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    if (!adapterId.isEmpty())
      obj["adapterId"] = adapterId;
    obj["type"] = type;
    obj["request"] = request;

    if (!program.isEmpty())
      obj["program"] = program;
    if (!args.isEmpty()) {
      QJsonArray argsArray;
      for (const QString &arg : args) {
        argsArray.append(arg);
      }
      obj["args"] = argsArray;
    }
    if (!cwd.isEmpty())
      obj["cwd"] = cwd;
    if (!env.isEmpty()) {
      QJsonObject envObj;
      for (auto it = env.begin(); it != env.end(); ++it) {
        envObj[it.key()] = it.value();
      }
      obj["env"] = envObj;
    }
    if (stopOnEntry)
      obj["stopOnEntry"] = stopOnEntry;

    if (processId > 0)
      obj["processId"] = processId;
    else if (!processIdExpression.isEmpty())
      obj["processId"] = processIdExpression;
    if (!host.isEmpty())
      obj["host"] = host;
    if (port > 0)
      obj["port"] = port;

    if (!preLaunchTask.isEmpty())
      obj["preLaunchTask"] = preLaunchTask;
    if (!postDebugTask.isEmpty())
      obj["postDebugTask"] = postDebugTask;
    if (!pythonMode.isEmpty())
      obj["pythonMode"] = pythonMode;
    if (!pythonInterpreter.isEmpty())
      obj["pythonInterpreter"] = pythonInterpreter;
    if (!pythonVenvPath.isEmpty())
      obj["pythonVenvPath"] = pythonVenvPath;
    if (!pythonRequirementsFile.isEmpty())
      obj["pythonRequirementsFile"] = pythonRequirementsFile;

    for (auto it = adapterConfig.begin(); it != adapterConfig.end(); ++it) {
      obj[it.key()] = it.value();
    }

    if (!presentation.isEmpty())
      obj["presentation"] = presentation;
    if (order != 0)
      obj["order"] = order;

    return obj;
  }

  static DebugConfiguration fromJson(const QJsonObject &obj) {
    DebugConfiguration cfg;
    cfg.name = obj["name"].toString();
    cfg.adapterId = obj["adapterId"].toString();
    cfg.type = obj["type"].toString();
    cfg.request = obj["request"].toString("launch");

    cfg.program = obj["program"].toString();
    if (obj.contains("args")) {
      QJsonArray argsArray = obj["args"].toArray();
      for (const auto &arg : argsArray) {
        cfg.args.append(arg.toString());
      }
    }
    cfg.cwd = obj["cwd"].toString();
    if (obj.contains("env")) {
      QJsonObject envObj = obj["env"].toObject();
      for (auto it = envObj.begin(); it != envObj.end(); ++it) {
        cfg.env[it.key()] = it.value().toString();
      }
    }
    cfg.stopOnEntry = obj["stopOnEntry"].toBool();

    if (obj["processId"].isString()) {
      cfg.processIdExpression = obj["processId"].toString();
    } else {
      cfg.processId = obj["processId"].toInt();
    }
    cfg.host = obj["host"].toString();
    cfg.port = obj["port"].toInt();

    cfg.preLaunchTask = obj["preLaunchTask"].toString();
    cfg.postDebugTask = obj["postDebugTask"].toString();
    cfg.pythonMode = obj["pythonMode"].toString();
    cfg.pythonInterpreter = obj["pythonInterpreter"].toString();
    cfg.pythonVenvPath = obj["pythonVenvPath"].toString();
    cfg.pythonRequirementsFile = obj["pythonRequirementsFile"].toString();

    cfg.presentation = obj["presentation"].toString();
    cfg.order = obj["order"].toInt();

    static const QStringList knownKeys = {"name",
                                          "adapterId",
                                          "type",
                                          "request",
                                          "program",
                                          "args",
                                          "cwd",
                                          "env",
                                          "stopOnEntry",
                                          "processId",
                                          "host",
                                          "port",
                                          "preLaunchTask",
                                          "postDebugTask",
                                          "pythonMode",
                                          "pythonInterpreter",
                                          "pythonVenvPath",
                                          "pythonRequirementsFile",
                                          "presentation",
                                          "order"};
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (!knownKeys.contains(it.key())) {
        cfg.adapterConfig[it.key()] = it.value();
      }
    }

    return cfg;
  }
};

// How a launch configuration starts without a debugger: Run uses this,
// Debug hands the configuration to a debug adapter instead.
struct RunCommand {
  QString program;
  QStringList arguments;
  // Added on top of the configuration's own environment.
  QMap<QString, QString> environment;
  // Set when the configuration cannot run without a debugger (attach
  // requests, for example).
  QString error;

  bool isValid() const { return error.isEmpty() && !program.isEmpty(); }
};

struct CompoundDebugConfiguration {
  QString name;
  QStringList configurations;
  bool stopAll = true;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    QJsonArray cfgArray;
    for (const QString &cfg : configurations) {
      cfgArray.append(cfg);
    }
    obj["configurations"] = cfgArray;
    obj["stopAll"] = stopAll;
    return obj;
  }

  static CompoundDebugConfiguration fromJson(const QJsonObject &obj) {
    CompoundDebugConfiguration cfg;
    cfg.name = obj["name"].toString();
    QJsonArray cfgArray = obj["configurations"].toArray();
    for (const auto &c : cfgArray) {
      cfg.configurations.append(c.toString());
    }
    cfg.stopAll = obj["stopAll"].toBool(true);
    return cfg;
  }
};

// Named launch configurations of a workspace, stored in
// .lightpad/debug/launch.json in VS Code's launch.json format. Every
// configuration can be run (without a debugger) or debugged; compounds start
// several configurations together. Order follows the file.
class DebugConfigurationManager : public QObject {
  Q_OBJECT

public:
  static DebugConfigurationManager &instance();

  bool loadFromFile(const QString &filePath);

  bool saveToFile(const QString &filePath);

  void addConfiguration(const DebugConfiguration &config);

  void removeConfiguration(const QString &name);

  bool updateConfiguration(const QString &name,
                           const DebugConfiguration &config);

  void replaceConfigurations(const QList<DebugConfiguration> &configurations);

  static QString joinCommandLineArguments(const QStringList &arguments);
  static QStringList splitCommandLineArguments(const QString &commandLine);

  DebugConfiguration configuration(const QString &name) const;
  bool hasConfiguration(const QString &name) const;

  QList<DebugConfiguration> allConfigurations() const;

  QList<DebugConfiguration> configurationsForType(const QString &type) const;

  void addCompoundConfiguration(const CompoundDebugConfiguration &config);
  void replaceCompoundConfigurations(
      const QList<CompoundDebugConfiguration> &compounds);
  CompoundDebugConfiguration compoundConfiguration(const QString &name) const;
  bool hasCompoundConfiguration(const QString &name) const;

  QList<CompoundDebugConfiguration> allCompoundConfigurations() const;

  // A name based on baseName that no configuration or compound uses yet.
  QString uniqueName(const QString &baseName) const;

  void setWorkspaceFolder(const QString &folder);
  QString workspaceFolder() const { return m_workspaceFolder; }

  DebugConfiguration resolveVariables(const DebugConfiguration &config,
                                      const QString &currentFile = {}) const;

  DebugConfiguration
  createQuickConfig(const QString &filePath,
                    const QString &languageId = QString()) const;
  DebugConfiguration
  createPytestConfig(const QStringList &targets,
                     const TestConfiguration &testConfig) const;

  // Loads the workspace's launch.json. A missing file means "no saved
  // configurations"; nothing is written until the user saves one.
  bool loadFromLightpadDir();

  bool saveToLightpadDir();

  QString lightpadLaunchConfigPath() const;

  // Earlier versions seeded every project with these example entries. They
  // are hidden when unmodified so they do not crowd the launch menus.
  static bool isLegacySampleConfiguration(const QJsonObject &object);
  static bool isLegacySampleCompound(const QJsonObject &object);

signals:
  void configurationAdded(const QString &name);
  void configurationRemoved(const QString &name);
  void configurationChanged(const QString &name);
  void configurationsLoaded();
  // Emitted after any change to configurations or compounds.
  void configurationsChanged();

private:
  DebugConfigurationManager();
  ~DebugConfigurationManager() = default;
  DebugConfigurationManager(const DebugConfigurationManager &) = delete;
  DebugConfigurationManager &
  operator=(const DebugConfigurationManager &) = delete;

  int indexOf(const QString &name) const;
  int compoundIndexOf(const QString &name) const;
  QString
  substituteVariable(const QString &value, const QString &currentFile,
                     const PythonEnvironmentPreference &pythonPreference,
                     const QString &workingDirectory = {}) const;

  QList<DebugConfiguration> m_configurations;
  QList<CompoundDebugConfiguration> m_compoundConfigurations;
  QString m_workspaceFolder;
  QString m_configFilePath;
};

#endif
