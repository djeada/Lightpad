#include "runconfiguration.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

RunConfigurationManager &RunConfigurationManager::instance() {
  static RunConfigurationManager inst;
  return inst;
}

RunConfigurationManager::RunConfigurationManager() : QObject(nullptr) {}

bool RunConfigurationManager::loadConfigurations(
    const QString &workspaceFolder) {
  m_configurations.clear();

  if (workspaceFolder.isEmpty())
    return false;

  QString path =
      QDir(workspaceFolder).filePath(".lightpad/run_configurations.json");

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return false;

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject())
    return false;

  QJsonArray arr = doc.object().value("configurations").toArray();
  const int maxConfigurations = 1000;
  for (const QJsonValue &v : arr) {
    if (m_configurations.size() >= maxConfigurations)
      break;
    RunConfiguration cfg = RunConfiguration::fromJson(v.toObject());
    if (cfg.isValid())
      m_configurations.append(cfg);
  }

  emit configurationsChanged();
  return true;
}

bool RunConfigurationManager::saveConfigurations(
    const QString &workspaceFolder) {
  if (workspaceFolder.isEmpty())
    return false;

  QDir dir(workspaceFolder);
  if (!dir.mkpath(".lightpad"))
    return false;

  QString path = dir.filePath(".lightpad/run_configurations.json");

  QJsonArray arr;
  for (const RunConfiguration &cfg : m_configurations)
    arr.append(cfg.toJson());

  QJsonObject root;
  root["configurations"] = arr;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    return false;

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

QList<RunConfiguration> RunConfigurationManager::allConfigurations() const {
  return m_configurations;
}

QList<RunConfiguration>
RunConfigurationManager::configurationsOfType(RunConfigurationType type) const {
  QList<RunConfiguration> result;
  for (const RunConfiguration &cfg : m_configurations) {
    if (cfg.type == type)
      result.append(cfg);
  }
  return result;
}

RunConfiguration
RunConfigurationManager::configurationByName(const QString &name) const {
  for (const RunConfiguration &cfg : m_configurations) {
    if (cfg.name == name)
      return cfg;
  }
  return RunConfiguration();
}

RunConfiguration
RunConfigurationManager::configurationById(const QString &id) const {
  for (const RunConfiguration &cfg : m_configurations) {
    if (cfg.id == id)
      return cfg;
  }
  return RunConfiguration();
}

void RunConfigurationManager::addConfiguration(const RunConfiguration &config) {
  RunConfiguration cfg = config;
  if (cfg.id.isEmpty())
    cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

  for (int i = 0; i < m_configurations.size(); ++i) {
    if (m_configurations[i].id == cfg.id) {
      m_configurations[i] = cfg;
      emit configurationsChanged();
      return;
    }
  }

  m_configurations.append(cfg);
  emit configurationsChanged();
}

void RunConfigurationManager::removeConfiguration(const QString &id) {
  for (int i = 0; i < m_configurations.size(); ++i) {
    if (m_configurations[i].id == id) {
      m_configurations.removeAt(i);
      emit configurationsChanged();
      return;
    }
  }
}

void RunConfigurationManager::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

QString RunConfigurationManager::workspaceFolder() const {
  return m_workspaceFolder;
}
