#ifndef DEBUGCONFIGURATION_H
#define DEBUGCONFIGURATION_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

struct DebugConfiguration {
  QString name;
  QString type;
  QString request;

  QString program;
  QStringList args;
  QString cwd;
  QMap<QString, QString> env;
  bool stopOnEntry = false;

  int processId = 0;
  QString host;
  int port = 0;

  QString preLaunchTask;
  QString postDebugTask;

  QJsonObject adapterConfig;

  QString presentation;
  int order = 0;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["name"] = name;
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
    if (!host.isEmpty())
      obj["host"] = host;
    if (port > 0)
      obj["port"] = port;

    if (!preLaunchTask.isEmpty())
      obj["preLaunchTask"] = preLaunchTask;
    if (!postDebugTask.isEmpty())
      obj["postDebugTask"] = postDebugTask;

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

    cfg.processId = obj["processId"].toInt();
    cfg.host = obj["host"].toString();
    cfg.port = obj["port"].toInt();

    cfg.preLaunchTask = obj["preLaunchTask"].toString();
    cfg.postDebugTask = obj["postDebugTask"].toString();

    cfg.presentation = obj["presentation"].toString();
    cfg.order = obj["order"].toInt();

    static const QStringList knownKeys = {
        "name", "type",          "request",       "program",      "args",
        "cwd",  "env",           "stopOnEntry",   "processId",    "host",
        "port", "preLaunchTask", "postDebugTask", "presentation", "order"};
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      if (!knownKeys.contains(it.key())) {
        cfg.adapterConfig[it.key()] = it.value();
      }
    }

    return cfg;
  }
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

class DebugConfigurationManager : public QObject {
  Q_OBJECT

public:
  static DebugConfigurationManager &instance();

  bool loadFromFile(const QString &filePath);

  bool saveToFile(const QString &filePath);

  void addConfiguration(const DebugConfiguration &config);

  void removeConfiguration(const QString &name);

  void updateConfiguration(const QString &name,
                           const DebugConfiguration &config);

  DebugConfiguration configuration(const QString &name) const;

  QList<DebugConfiguration> allConfigurations() const;

  QList<DebugConfiguration> configurationsForType(const QString &type) const;

  void addCompoundConfiguration(const CompoundDebugConfiguration &config);

  QList<CompoundDebugConfiguration> allCompoundConfigurations() const;

  void setWorkspaceFolder(const QString &folder);

  DebugConfiguration resolveVariables(const DebugConfiguration &config,
                                      const QString &currentFile = {}) const;

  DebugConfiguration
  createQuickConfig(const QString &filePath,
                    const QString &languageId = QString()) const;

  bool loadFromLightpadDir();

  bool saveToLightpadDir();

  QString lightpadLaunchConfigPath() const;

signals:
  void configurationAdded(const QString &name);
  void configurationRemoved(const QString &name);
  void configurationChanged(const QString &name);
  void configurationsLoaded();

private:
  DebugConfigurationManager();
  ~DebugConfigurationManager() = default;
  DebugConfigurationManager(const DebugConfigurationManager &) = delete;
  DebugConfigurationManager &
  operator=(const DebugConfigurationManager &) = delete;

  QString substituteVariable(const QString &value,
                             const QString &currentFile) const;

  QMap<QString, DebugConfiguration> m_configurations;
  QMap<QString, CompoundDebugConfiguration> m_compoundConfigurations;
  QString m_workspaceFolder;
  QString m_configFilePath;
};

#endif
