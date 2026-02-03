#include "runtemplatemanager.h"
#include "../core/logging/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

RunTemplateManager &RunTemplateManager::instance() {
  static RunTemplateManager instance;
  return instance;
}

RunTemplateManager::RunTemplateManager() : QObject(nullptr) {}

bool RunTemplateManager::loadTemplates() {
  m_templates.clear();

  bool builtInLoaded = loadBuiltInTemplates();
  loadUserTemplates();

  if (builtInLoaded) {
    LOG_INFO(QString("Loaded %1 run templates").arg(m_templates.size()));
    emit templatesLoaded();
  }

  return builtInLoaded;
}

bool RunTemplateManager::loadBuiltInTemplates() {
  // Try to load from application directory first
  QString appDir = QCoreApplication::applicationDirPath();
  QStringList searchPaths = {
      appDir + "/run_templates/run_templates.json",
      appDir + "/../App/run_templates/run_templates.json",
      ":/run_templates/run_templates.json",
      QStandardPaths::locate(QStandardPaths::AppDataLocation,
                             "run_templates/run_templates.json")};

  QString filePath;
  for (const QString &path : searchPaths) {
    if (QFileInfo(path).exists()) {
      filePath = path;
      break;
    }
  }

  if (filePath.isEmpty()) {
    LOG_WARNING("Could not find built-in run templates file");
    return false;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR(QString("Failed to open run templates file: %1").arg(filePath));
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    LOG_ERROR(QString("Failed to parse run templates: %1")
                  .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray templatesArray = root.value("templates").toArray();

  for (const QJsonValue &value : templatesArray) {
    RunTemplate tmpl = parseTemplate(value.toObject());
    if (tmpl.isValid()) {
      m_templates.append(tmpl);
    }
  }

  LOG_INFO(QString("Loaded %1 built-in templates from %2")
               .arg(m_templates.size())
               .arg(filePath));
  return true;
}

bool RunTemplateManager::loadUserTemplates() {
  // User templates are stored in the config directory
  QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QString userTemplatesPath = configDir + "/run_templates.json";

  if (!QFileInfo(userTemplatesPath).exists()) {
    return true; // No user templates is not an error
  }

  QFile file(userTemplatesPath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARNING(QString("Failed to open user templates file: %1")
                    .arg(userTemplatesPath));
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    LOG_WARNING(QString("Failed to parse user templates: %1")
                    .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray templatesArray = root.value("templates").toArray();

  int userTemplateCount = 0;
  for (const QJsonValue &value : templatesArray) {
    RunTemplate tmpl = parseTemplate(value.toObject());
    if (tmpl.isValid()) {
      // User templates override built-in templates with the same ID
      bool found = false;
      for (int i = 0; i < m_templates.size(); ++i) {
        if (m_templates[i].id == tmpl.id) {
          m_templates[i] = tmpl;
          found = true;
          break;
        }
      }
      if (!found) {
        m_templates.append(tmpl);
      }
      userTemplateCount++;
    }
  }

  LOG_INFO(QString("Loaded %1 user templates").arg(userTemplateCount));
  return true;
}

RunTemplate RunTemplateManager::parseTemplate(const QJsonObject &obj) const {
  RunTemplate tmpl;

  tmpl.id = obj.value("id").toString();
  tmpl.name = obj.value("name").toString();
  tmpl.description = obj.value("description").toString();
  tmpl.language = obj.value("language").toString();
  tmpl.command = obj.value("command").toString();
  tmpl.workingDirectory = obj.value("workingDirectory").toString("${fileDir}");

  QJsonArray extArray = obj.value("extensions").toArray();
  for (const QJsonValue &ext : extArray) {
    tmpl.extensions.append(ext.toString());
  }

  QJsonArray argsArray = obj.value("args").toArray();
  for (const QJsonValue &arg : argsArray) {
    tmpl.args.append(arg.toString());
  }

  QJsonObject envObj = obj.value("env").toObject();
  for (auto it = envObj.begin(); it != envObj.end(); ++it) {
    tmpl.env[it.key()] = it.value().toString();
  }

  return tmpl;
}

QList<RunTemplate> RunTemplateManager::getAllTemplates() const {
  return m_templates;
}

QList<RunTemplate>
RunTemplateManager::getTemplatesForExtension(const QString &extension) const {
  QList<RunTemplate> result;
  QString ext = extension.toLower();
  if (ext.startsWith('.')) {
    ext = ext.mid(1);
  }

  for (const RunTemplate &tmpl : m_templates) {
    for (const QString &templateExt : tmpl.extensions) {
      if (templateExt.toLower() == ext) {
        result.append(tmpl);
        break;
      }
    }
  }

  return result;
}

RunTemplate RunTemplateManager::getTemplateById(const QString &id) const {
  for (const RunTemplate &tmpl : m_templates) {
    if (tmpl.id == id) {
      return tmpl;
    }
  }
  return RunTemplate();
}

QString RunTemplateManager::getConfigDirForFile(const QString &filePath) const {
  QFileInfo fileInfo(filePath);
  return fileInfo.absoluteDir().path() + "/.lightpad";
}

QString RunTemplateManager::getConfigFileForDir(const QString &dirPath) const {
  return dirPath + "/run_config.json";
}

bool RunTemplateManager::loadAssignmentsFromDir(const QString &dirPath) const {
  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/run_config.json";

  if (m_loadedConfigDirs.contains(configDir)) {
    return true;
  }

  if (!QFileInfo(configFile).exists()) {
    m_loadedConfigDirs.insert(configDir);
    return true;
  }

  QFile file(configFile);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARNING(QString("Failed to open run config: %1").arg(configFile));
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    LOG_WARNING(QString("Failed to parse run config: %1")
                    .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray assignments = root.value("assignments").toArray();

  for (const QJsonValue &value : assignments) {
    QJsonObject obj = value.toObject();
    FileTemplateAssignment assignment;
    assignment.filePath = obj.value("file").toString();
    assignment.templateId = obj.value("template").toString();

    // Make relative paths absolute
    if (!QFileInfo(assignment.filePath).isAbsolute()) {
      assignment.filePath = dirPath + "/" + assignment.filePath;
    }

    QJsonArray argsArray = obj.value("customArgs").toArray();
    for (const QJsonValue &arg : argsArray) {
      assignment.customArgs.append(arg.toString());
    }

    QJsonObject envObj = obj.value("customEnv").toObject();
    for (auto it = envObj.begin(); it != envObj.end(); ++it) {
      assignment.customEnv[it.key()] = it.value().toString();
    }

    m_assignments[assignment.filePath] = assignment;
  }

  m_loadedConfigDirs.insert(configDir);
  LOG_INFO(QString("Loaded %1 assignments from %2")
               .arg(assignments.size())
               .arg(configFile));
  return true;
}

bool RunTemplateManager::saveAssignmentsToDir(const QString &dirPath) const {
  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/run_config.json";

  // Collect assignments for this directory
  QJsonArray assignments;
  for (auto it = m_assignments.begin(); it != m_assignments.end(); ++it) {
    QFileInfo fileInfo(it.key());
    if (fileInfo.absoluteDir().path() == dirPath) {
      QJsonObject obj;
      obj["file"] = fileInfo.fileName();
      obj["template"] = it.value().templateId;

      if (!it.value().customArgs.isEmpty()) {
        QJsonArray argsArray;
        for (const QString &arg : it.value().customArgs) {
          argsArray.append(arg);
        }
        obj["customArgs"] = argsArray;
      }

      if (!it.value().customEnv.isEmpty()) {
        QJsonObject envObj;
        for (auto envIt = it.value().customEnv.begin();
             envIt != it.value().customEnv.end(); ++envIt) {
          envObj[envIt.key()] = envIt.value();
        }
        obj["customEnv"] = envObj;
      }

      assignments.append(obj);
    }
  }

  // Create config directory if needed
  QDir dir;
  if (!dir.exists(configDir)) {
    if (!dir.mkpath(configDir)) {
      LOG_ERROR(
          QString("Failed to create config directory: %1").arg(configDir));
      return false;
    }
  }

  // Save to file
  QJsonObject root;
  root["version"] = "1.0";
  root["assignments"] = assignments;

  QFile file(configFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to write run config: %1").arg(configFile));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  LOG_INFO(QString("Saved %1 assignments to %2")
               .arg(assignments.size())
               .arg(configFile));
  return true;
}

FileTemplateAssignment
RunTemplateManager::getAssignmentForFile(const QString &filePath) const {
  // Load assignments for the directory if not already loaded
  QFileInfo fileInfo(filePath);
  QString dirPath = fileInfo.absoluteDir().path();

  // Lazy loading - member variables are mutable to allow this in const methods
  loadAssignmentsFromDir(dirPath);

  auto it = m_assignments.find(filePath);
  if (it != m_assignments.end()) {
    return it.value();
  }

  return FileTemplateAssignment();
}

bool RunTemplateManager::assignTemplateToFile(
    const QString &filePath, const QString &templateId,
    const QStringList &customArgs, const QMap<QString, QString> &customEnv) {
  FileTemplateAssignment assignment;
  assignment.filePath = filePath;
  assignment.templateId = templateId;
  assignment.customArgs = customArgs;
  assignment.customEnv = customEnv;

  m_assignments[filePath] = assignment;

  QFileInfo fileInfo(filePath);
  QString dirPath = fileInfo.absoluteDir().path();

  bool saved = saveAssignmentsToDir(dirPath);

  if (saved) {
    emit assignmentChanged(filePath);
  }

  return saved;
}

bool RunTemplateManager::removeAssignment(const QString &filePath) {
  auto it = m_assignments.find(filePath);
  if (it == m_assignments.end()) {
    return true; // Nothing to remove
  }

  QFileInfo fileInfo(filePath);
  QString dirPath = fileInfo.absoluteDir().path();

  m_assignments.erase(it);

  bool saved = saveAssignmentsToDir(dirPath);

  if (saved) {
    emit assignmentChanged(filePath);
  }

  return saved;
}

QString RunTemplateManager::substituteVariables(const QString &input,
                                                const QString &filePath) {
  QString result = input;
  QFileInfo fileInfo(filePath);

  result.replace("${file}", filePath);
  result.replace("${fileDir}", fileInfo.absoluteDir().path());
  result.replace("${fileBasename}", fileInfo.fileName());
  result.replace("${fileBasenameNoExt}", fileInfo.completeBaseName());
  result.replace("${fileExt}", fileInfo.suffix());

  // Handle workspace folder (use file's directory as fallback)
  result.replace("${workspaceFolder}", fileInfo.absoluteDir().path());

  return result;
}

QPair<QString, QStringList>
RunTemplateManager::buildCommand(const QString &filePath) const {
  FileTemplateAssignment assignment = getAssignmentForFile(filePath);

  QString templateId = assignment.templateId;

  // If no assignment, try to find a default template based on extension
  if (templateId.isEmpty()) {
    QFileInfo fileInfo(filePath);
    QList<RunTemplate> templates = getTemplatesForExtension(fileInfo.suffix());
    if (!templates.isEmpty()) {
      templateId = templates.first().id;
    }
  }

  if (templateId.isEmpty()) {
    return qMakePair(QString(), QStringList());
  }

  RunTemplate tmpl = getTemplateById(templateId);
  if (!tmpl.isValid()) {
    return qMakePair(QString(), QStringList());
  }

  QString command = substituteVariables(tmpl.command, filePath);

  QStringList args;
  for (const QString &arg : tmpl.args) {
    args.append(substituteVariables(arg, filePath));
  }

  // Append custom args if any
  if (!assignment.customArgs.isEmpty()) {
    for (const QString &arg : assignment.customArgs) {
      args.append(substituteVariables(arg, filePath));
    }
  }

  return qMakePair(command, args);
}

QString RunTemplateManager::getWorkingDirectory(const QString &filePath) const {
  FileTemplateAssignment assignment = getAssignmentForFile(filePath);

  QString templateId = assignment.templateId;

  if (templateId.isEmpty()) {
    QFileInfo fileInfo(filePath);
    QList<RunTemplate> templates = getTemplatesForExtension(fileInfo.suffix());
    if (!templates.isEmpty()) {
      templateId = templates.first().id;
    }
  }

  if (templateId.isEmpty()) {
    return QFileInfo(filePath).absoluteDir().path();
  }

  RunTemplate tmpl = getTemplateById(templateId);
  if (!tmpl.isValid()) {
    return QFileInfo(filePath).absoluteDir().path();
  }

  return substituteVariables(tmpl.workingDirectory, filePath);
}

QMap<QString, QString>
RunTemplateManager::getEnvironment(const QString &filePath) const {
  FileTemplateAssignment assignment = getAssignmentForFile(filePath);

  QString templateId = assignment.templateId;

  if (templateId.isEmpty()) {
    QFileInfo fileInfo(filePath);
    QList<RunTemplate> templates = getTemplatesForExtension(fileInfo.suffix());
    if (!templates.isEmpty()) {
      templateId = templates.first().id;
    }
  }

  QMap<QString, QString> env;

  if (!templateId.isEmpty()) {
    RunTemplate tmpl = getTemplateById(templateId);
    if (tmpl.isValid()) {
      for (auto it = tmpl.env.begin(); it != tmpl.env.end(); ++it) {
        env[it.key()] = substituteVariables(it.value(), filePath);
      }
    }
  }

  // Custom env overrides template env
  for (auto it = assignment.customEnv.begin(); it != assignment.customEnv.end();
       ++it) {
    env[it.key()] = substituteVariables(it.value(), filePath);
  }

  return env;
}
