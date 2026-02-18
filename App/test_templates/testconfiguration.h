#ifndef TESTCONFIGURATION_H
#define TESTCONFIGURATION_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

enum class TestStatus { Queued, Running, Passed, Failed, Skipped, Errored };

struct TestResult {
  QString id;
  QString name;
  QString suite;
  QString filePath;
  int line = -1;
  TestStatus status = TestStatus::Queued;
  int durationMs = -1;
  QString message;
  QString stackTrace;
  QString stdoutOutput;
  QString stderrOutput;
};

struct RunSingleTestOverride {
  QStringList args;

  QJsonObject toJson() const {
    QJsonObject obj;
    QJsonArray a;
    for (const QString &arg : args)
      a.append(arg);
    obj["args"] = a;
    return obj;
  }

  static RunSingleTestOverride fromJson(const QJsonObject &obj) {
    RunSingleTestOverride o;
    for (const auto &v : obj["args"].toArray())
      o.args.append(v.toString());
    return o;
  }
};

struct TestConfiguration {
  QString id;
  QString name;
  QString language;
  QStringList extensions;
  QString command;
  QStringList args;
  QString workingDirectory;
  QString outputFormat;
  QString testFilePattern;
  QMap<QString, QString> env;
  QString preLaunchTask;
  QString postRunTask;
  QString templateId;
  RunSingleTestOverride runSingleTest;

  bool isValid() const { return !name.isEmpty() && !command.isEmpty(); }

  QJsonObject toJson() const {
    QJsonObject obj;
    if (!id.isEmpty())
      obj["id"] = id;
    obj["name"] = name;
    if (!language.isEmpty())
      obj["language"] = language;
    if (!extensions.isEmpty()) {
      QJsonArray ext;
      for (const QString &e : extensions)
        ext.append(e);
      obj["extensions"] = ext;
    }
    obj["command"] = command;
    if (!args.isEmpty()) {
      QJsonArray a;
      for (const QString &arg : args)
        a.append(arg);
      obj["args"] = a;
    }
    if (!workingDirectory.isEmpty())
      obj["workingDirectory"] = workingDirectory;
    if (!outputFormat.isEmpty())
      obj["outputFormat"] = outputFormat;
    if (!testFilePattern.isEmpty())
      obj["testFilePattern"] = testFilePattern;
    if (!env.isEmpty()) {
      QJsonObject envObj;
      for (auto it = env.begin(); it != env.end(); ++it)
        envObj[it.key()] = it.value();
      obj["env"] = envObj;
    }
    if (!preLaunchTask.isEmpty())
      obj["preLaunchTask"] = preLaunchTask;
    if (!postRunTask.isEmpty())
      obj["postRunTask"] = postRunTask;
    if (!templateId.isEmpty())
      obj["templateId"] = templateId;
    if (!runSingleTest.args.isEmpty())
      obj["runSingleTest"] = runSingleTest.toJson();
    return obj;
  }

  static TestConfiguration fromJson(const QJsonObject &obj) {
    TestConfiguration cfg;
    cfg.id = obj["id"].toString();
    cfg.name = obj["name"].toString();
    cfg.language = obj["language"].toString();
    for (const auto &v : obj["extensions"].toArray())
      cfg.extensions.append(v.toString());
    cfg.command = obj["command"].toString();
    for (const auto &v : obj["args"].toArray())
      cfg.args.append(v.toString());
    cfg.workingDirectory = obj["workingDirectory"].toString();
    cfg.outputFormat = obj["outputFormat"].toString();
    cfg.testFilePattern = obj["testFilePattern"].toString();
    if (obj.contains("env")) {
      QJsonObject envObj = obj["env"].toObject();
      for (auto it = envObj.begin(); it != envObj.end(); ++it)
        cfg.env[it.key()] = it.value().toString();
    }
    cfg.preLaunchTask = obj["preLaunchTask"].toString();
    cfg.postRunTask = obj["postRunTask"].toString();
    cfg.templateId = obj["templateId"].toString();
    if (obj.contains("runSingleTest"))
      cfg.runSingleTest =
          RunSingleTestOverride::fromJson(obj["runSingleTest"].toObject());
    return cfg;
  }
};

class TestConfigurationManager : public QObject {
  Q_OBJECT

public:
  static TestConfigurationManager &instance();

  bool loadTemplates();
  bool loadUserConfigurations(const QString &workspaceFolder);
  bool saveUserConfigurations(const QString &workspaceFolder);

  QList<TestConfiguration> allTemplates() const;
  QList<TestConfiguration> allConfigurations() const;
  QList<TestConfiguration> configurationsForExtension(const QString &ext) const;
  TestConfiguration configurationByName(const QString &name) const;
  TestConfiguration templateById(const QString &id) const;

  void addConfiguration(const TestConfiguration &config);
  void removeConfiguration(const QString &name);

  void setDefaultConfiguration(const QString &name);
  QString defaultConfigurationName() const;

  void setWorkspaceFolder(const QString &folder);
  QString workspaceFolder() const;

  static QString substituteVariables(const QString &input,
                                     const QString &filePath,
                                     const QString &workspaceFolder,
                                     const QString &testName = QString());

signals:
  void templatesLoaded();
  void configurationsChanged();

private:
  TestConfigurationManager();
  ~TestConfigurationManager() = default;
  TestConfigurationManager(const TestConfigurationManager &) = delete;
  TestConfigurationManager &
  operator=(const TestConfigurationManager &) = delete;

  QList<TestConfiguration> m_templates;
  QList<TestConfiguration> m_userConfigurations;
  QString m_defaultConfiguration;
  QString m_workspaceFolder;
};

#endif
