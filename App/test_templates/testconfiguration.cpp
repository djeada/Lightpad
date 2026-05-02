#include "testconfiguration.h"

#include "../python/pythonprojectenvironment.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QUuid>

#include "core/logging/logger.h"

TestConfigurationManager &TestConfigurationManager::instance() {
  static TestConfigurationManager mgr;
  return mgr;
}

TestConfigurationManager::TestConfigurationManager() {}

bool TestConfigurationManager::loadTemplates() {
  m_templates.clear();

  QStringList searchPaths;
  searchPaths << QCoreApplication::applicationDirPath() +
                     "/test_templates/test_templates.json";
  searchPaths << ":/test_templates/test_templates.json";
  searchPaths << QStandardPaths::writableLocation(
                     QStandardPaths::AppConfigLocation) +
                     "/test_templates.json";

  for (const QString &path : searchPaths) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      continue;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
      LOG_WARNING("Failed to parse " + path + ": " + error.errorString());
      continue;
    }

    QJsonObject root = doc.object();
    QJsonArray templates = root["templates"].toArray();
    for (const QJsonValue &v : templates) {
      TestConfiguration cfg = TestConfiguration::fromJson(v.toObject());
      if (cfg.isValid())
        m_templates.append(cfg);
    }

    if (!m_templates.isEmpty()) {
      LOG_INFO(QString("Loaded %1 test templates from %2")
                   .arg(m_templates.size())
                   .arg(path));
      emit templatesLoaded();
      return true;
    }
  }

  LOG_WARNING("No test templates found");
  return false;
}

bool TestConfigurationManager::loadUserConfigurations(
    const QString &workspaceFolder) {
  m_workspaceFolder = workspaceFolder;
  m_userConfigurations.clear();
  m_defaultConfigurationId.clear();
  m_defaultConfigurationName.clear();

  QString configPath = workspaceFolder + "/.lightpad/test/config.json";
  QFile file(configPath);
  if (!file.open(QIODevice::ReadOnly))
    return false;

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  file.close();

  if (error.error != QJsonParseError::NoError) {
    LOG_WARNING("Failed to parse user test config: " + error.errorString());
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray configs = root["configurations"].toArray();
  for (const QJsonValue &v : configs) {
    TestConfiguration cfg = TestConfiguration::fromJson(v.toObject());
    if (!cfg.id.isEmpty() && hasUserConfiguration(cfg.id))
      continue;
    if (cfg.id.isEmpty())
      cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (cfg.isValid())
      m_userConfigurations.append(cfg);
  }

  m_defaultConfigurationId = root["defaultConfigurationId"].toString();
  m_defaultConfigurationName = root["defaultConfiguration"].toString();
  if (m_defaultConfigurationId.isEmpty() && !m_defaultConfigurationName.isEmpty()) {
    const TestConfiguration cfg = configurationByName(m_defaultConfigurationName);
    if (cfg.isValid())
      m_defaultConfigurationId = cfg.id;
  }
  emit configurationsChanged();
  return true;
}

bool TestConfigurationManager::saveUserConfigurations(
    const QString &workspaceFolder) {
  QString dirPath = workspaceFolder + "/.lightpad/test";
  QDir().mkpath(dirPath);

  QString configPath = dirPath + "/config.json";
  QFile file(configPath);
  if (!file.open(QIODevice::WriteOnly))
    return false;

  QJsonObject root;
  QJsonArray configs;
  for (const TestConfiguration &cfg : m_userConfigurations)
    configs.append(cfg.toJson());
  root["configurations"] = configs;
  if (!m_defaultConfigurationId.isEmpty())
    root["defaultConfigurationId"] = m_defaultConfigurationId;
  const QString defaultName = defaultConfigurationName();
  if (!defaultName.isEmpty())
    root["defaultConfiguration"] = defaultName;

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.close();
  return true;
}

QList<TestConfiguration> TestConfigurationManager::allTemplates() const {
  return m_templates;
}

QList<TestConfiguration> TestConfigurationManager::userConfigurations() const {
  return m_userConfigurations;
}

QList<TestConfiguration> TestConfigurationManager::allConfigurations() const {
  QList<TestConfiguration> result = m_userConfigurations;
  QSet<QString> userIds;
  for (const TestConfiguration &cfg : m_userConfigurations) {
    if (!cfg.id.isEmpty())
      userIds.insert(cfg.id);
  }

  for (const TestConfiguration &cfg : m_templates) {
    if (!cfg.id.isEmpty() && userIds.contains(cfg.id))
      continue;
    result.append(cfg);
  }
  return result;
}

QList<TestConfiguration>
TestConfigurationManager::configurationsForExtension(const QString &ext) const {
  QList<TestConfiguration> result;
  for (const TestConfiguration &cfg : allConfigurations()) {
    if (cfg.extensions.contains(ext, Qt::CaseInsensitive))
      result.append(cfg);
  }
  return result;
}

TestConfiguration TestConfigurationManager::preferredConfigurationForPath(
    const QString &path) const {
  const QFileInfo info(path);
  if (info.isFile()) {
    const QList<TestConfiguration> matches =
        configurationsForExtension(info.suffix().toLower());
    return matches.isEmpty() ? TestConfiguration() : matches.first();
  }

  if (!info.isDir())
    return TestConfiguration();

  const QList<TestConfiguration> configs = allConfigurations();
  if (configs.isEmpty())
    return TestConfiguration();

  QHash<QString, int> scores;
  int scannedFiles = 0;
  QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext() && scannedFiles < 500) {
    const QString filePath = it.next();
    ++scannedFiles;

    const QFileInfo fileInfo(filePath);
    const QString ext = fileInfo.suffix().toLower();
    if (ext.isEmpty())
      continue;

    const QString fileName = fileInfo.fileName();
    for (const TestConfiguration &cfg : configs) {
      if (!cfg.extensions.contains(ext, Qt::CaseInsensitive))
        continue;

      int weight = 1;
      if (!cfg.testFilePattern.isEmpty() &&
          !cfg.testFilePattern.contains('{')) {
        const QRegularExpression pattern(
            QRegularExpression::wildcardToRegularExpression(
                cfg.testFilePattern,
                QRegularExpression::UnanchoredWildcardConversion));
        if (pattern.match(fileName).hasMatch())
          weight += 4;
      }
      scores[cfg.id] += weight;
    }
  }

  if (scores.isEmpty())
    return TestConfiguration();

  QString bestId;
  int bestScore = -1;
  for (const TestConfiguration &cfg : configs) {
    const int score = scores.value(cfg.id, 0);
    if (score > bestScore) {
      bestScore = score;
      bestId = cfg.id;
    }
  }

  for (const TestConfiguration &cfg : configs) {
    if (cfg.id == bestId)
      return cfg;
  }

  return TestConfiguration();
}

TestConfiguration
TestConfigurationManager::configurationById(const QString &id) const {
  if (id.isEmpty())
    return TestConfiguration();

  for (const TestConfiguration &cfg : m_userConfigurations) {
    if (cfg.id == id)
      return cfg;
  }
  for (const TestConfiguration &cfg : m_templates) {
    if (cfg.id == id && !hasUserConfiguration(id))
      return cfg;
  }
  for (const TestConfiguration &cfg : m_templates) {
    if (cfg.id == id)
      return cfg;
  }
  return TestConfiguration();
}

TestConfiguration
TestConfigurationManager::configurationByName(const QString &name) const {
  for (const TestConfiguration &cfg : allConfigurations()) {
    if (cfg.name == name)
      return cfg;
  }
  return TestConfiguration();
}

TestConfiguration
TestConfigurationManager::templateById(const QString &id) const {
  for (const TestConfiguration &cfg : m_templates) {
    if (cfg.id == id)
      return cfg;
  }
  return TestConfiguration();
}

bool TestConfigurationManager::hasUserConfiguration(const QString &id) const {
  if (id.isEmpty())
    return false;

  for (const TestConfiguration &cfg : m_userConfigurations) {
    if (cfg.id == id)
      return true;
  }
  return false;
}

void TestConfigurationManager::addConfiguration(
    const TestConfiguration &config) {
  TestConfiguration cfg = config;
  if (cfg.id.isEmpty())
    cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

  for (int i = 0; i < m_userConfigurations.size(); ++i) {
    if (m_userConfigurations[i].id == cfg.id) {
      m_userConfigurations[i] = cfg;
      emit configurationsChanged();
      return;
    }
  }
  m_userConfigurations.append(cfg);
  emit configurationsChanged();
}

void TestConfigurationManager::removeConfiguration(const QString &id) {
  for (int i = 0; i < m_userConfigurations.size(); ++i) {
    if (m_userConfigurations[i].id == id ||
        m_userConfigurations[i].name == id) {
      const QString removedId = m_userConfigurations[i].id;
      m_userConfigurations.removeAt(i);
      if (m_defaultConfigurationId == removedId &&
          !templateById(removedId).isValid()) {
        m_defaultConfigurationId.clear();
        m_defaultConfigurationName.clear();
      }
      emit configurationsChanged();
      return;
    }
  }
}

void TestConfigurationManager::setDefaultConfigurationId(const QString &id) {
  if (m_defaultConfigurationId == id &&
      (id.isEmpty() || m_defaultConfigurationName == defaultConfigurationName())) {
    return;
  }

  m_defaultConfigurationId = id;
  m_defaultConfigurationName.clear();
  if (!id.isEmpty()) {
    const TestConfiguration cfg = configurationById(id);
    if (cfg.isValid())
      m_defaultConfigurationName = cfg.name;
  }
  emit configurationsChanged();
}

QString TestConfigurationManager::defaultConfigurationId() const {
  if (!m_defaultConfigurationId.isEmpty())
    return m_defaultConfigurationId;

  if (!m_defaultConfigurationName.isEmpty()) {
    const TestConfiguration cfg = configurationByName(m_defaultConfigurationName);
    if (cfg.isValid())
      return cfg.id;
  }

  return QString();
}

void TestConfigurationManager::setDefaultConfiguration(const QString &name) {
  m_defaultConfigurationName = name;
  const TestConfiguration cfg = configurationByName(name);
  m_defaultConfigurationId = cfg.id;
  emit configurationsChanged();
}

QString TestConfigurationManager::defaultConfigurationName() const {
  if (!m_defaultConfigurationId.isEmpty()) {
    const TestConfiguration cfg = configurationById(m_defaultConfigurationId);
    if (cfg.isValid())
      return cfg.name;
  }
  return m_defaultConfigurationName;
}

void TestConfigurationManager::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

QString TestConfigurationManager::workspaceFolder() const {
  return m_workspaceFolder;
}

QString TestConfigurationManager::substituteVariables(
    const QString &input, const QString &filePath,
    const QString &workspaceFolder, const QString &testName) {
  QString workingDirectory = workspaceFolder;
  if (!filePath.isEmpty())
    workingDirectory = QFileInfo(filePath).absolutePath();

  QString result = PythonProjectEnvironment::substituteVariables(
      input, workspaceFolder, filePath, workingDirectory);
  if (!testName.isEmpty())
    result.replace("${testName}", testName);
  return result;
}
