#ifndef RUNCONFIGURATION_H
#define RUNCONFIGURATION_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

enum class RunConfigurationType { Build, Run, Test };

struct RunConfiguration {
  QString id;
  QString name;
  RunConfigurationType type = RunConfigurationType::Run;
  QString command;
  QStringList args;
  QString workingDirectory;
  QMap<QString, QString> env;

  bool isValid() const { return !name.isEmpty() && !command.isEmpty(); }

  QJsonObject toJson() const {
    QJsonObject obj;
    if (!id.isEmpty())
      obj["id"] = id;
    obj["name"] = name;
    obj["type"] = static_cast<int>(type);
    obj["command"] = command;
    if (!args.isEmpty()) {
      QJsonArray a;
      for (const QString &arg : args)
        a.append(arg);
      obj["args"] = a;
    }
    if (!workingDirectory.isEmpty())
      obj["workingDirectory"] = workingDirectory;
    if (!env.isEmpty()) {
      QJsonObject envObj;
      for (auto it = env.begin(); it != env.end(); ++it)
        envObj[it.key()] = it.value();
      obj["env"] = envObj;
    }
    return obj;
  }

  static RunConfiguration fromJson(const QJsonObject &obj) {
    RunConfiguration cfg;
    cfg.id = obj["id"].toString();
    cfg.name = obj["name"].toString();
    int typeInt = obj["type"].toInt(1);
    if (typeInt >= 0 && typeInt <= 2)
      cfg.type = static_cast<RunConfigurationType>(typeInt);
    cfg.command = obj["command"].toString();
    for (const auto &v : obj["args"].toArray())
      cfg.args.append(v.toString());
    cfg.workingDirectory = obj["workingDirectory"].toString();
    if (obj.contains("env")) {
      QJsonObject envObj = obj["env"].toObject();
      for (auto it = envObj.begin(); it != envObj.end(); ++it)
        cfg.env[it.key()] = it.value().toString();
    }
    return cfg;
  }
};

class RunConfigurationManager : public QObject {
  Q_OBJECT

public:
  static RunConfigurationManager &instance();

  bool loadConfigurations(const QString &workspaceFolder);
  bool saveConfigurations(const QString &workspaceFolder);

  QList<RunConfiguration> allConfigurations() const;
  QList<RunConfiguration> configurationsOfType(RunConfigurationType type) const;
  RunConfiguration configurationByName(const QString &name) const;
  RunConfiguration configurationById(const QString &id) const;

  void addConfiguration(const RunConfiguration &config);
  void removeConfiguration(const QString &id);

  void setWorkspaceFolder(const QString &folder);
  QString workspaceFolder() const;

signals:
  void configurationsChanged();

private:
  RunConfigurationManager();
  ~RunConfigurationManager() = default;
  RunConfigurationManager(const RunConfigurationManager &) = delete;
  RunConfigurationManager &operator=(const RunConfigurationManager &) = delete;

  QList<RunConfiguration> m_configurations;
  QString m_workspaceFolder;
};

#endif
