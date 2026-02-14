#include "formattemplatemanager.h"
#include "../core/logging/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

FormatTemplateManager &FormatTemplateManager::instance() {
  static FormatTemplateManager instance;
  return instance;
}

FormatTemplateManager::FormatTemplateManager() : QObject(nullptr) {}

bool FormatTemplateManager::loadTemplates() {
  m_templates.clear();

  bool builtInLoaded = loadBuiltInTemplates();
  loadUserTemplates();

  if (builtInLoaded) {
    LOG_INFO(QString("Loaded %1 format templates").arg(m_templates.size()));
    emit templatesLoaded();
  }

  return builtInLoaded;
}

bool FormatTemplateManager::loadBuiltInTemplates() {

  QString appDir = QCoreApplication::applicationDirPath();
  QStringList searchPaths = {
      appDir + "/format_templates/format_templates.json",
      appDir + "/../App/format_templates/format_templates.json",
      ":/format_templates/format_templates.json",
      QStandardPaths::locate(QStandardPaths::AppDataLocation,
                             "format_templates/format_templates.json")};

  QString filePath;
  for (const QString &path : searchPaths) {
    if (QFileInfo(path).exists()) {
      filePath = path;
      break;
    }
  }

  if (filePath.isEmpty()) {
    LOG_WARNING("Could not find built-in format templates file");
    return false;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR(
        QString("Failed to open format templates file: %1").arg(filePath));
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    LOG_ERROR(QString("Failed to parse format templates: %1")
                  .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray templatesArray = root.value("templates").toArray();

  for (const QJsonValue &value : templatesArray) {
    FormatTemplate tmpl = parseTemplate(value.toObject());
    if (tmpl.isValid()) {
      m_templates.append(tmpl);
    }
  }

  LOG_INFO(QString("Loaded %1 built-in format templates from %2")
               .arg(m_templates.size())
               .arg(filePath));
  return true;
}

bool FormatTemplateManager::loadUserTemplates() {

  QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QString userTemplatesPath = configDir + "/format_templates.json";

  if (!QFileInfo(userTemplatesPath).exists()) {
    return true;
  }

  QFile file(userTemplatesPath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARNING(QString("Failed to open user format templates file: %1")
                    .arg(userTemplatesPath));
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    LOG_WARNING(QString("Failed to parse user format templates: %1")
                    .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray templatesArray = root.value("templates").toArray();

  int userTemplateCount = 0;
  for (const QJsonValue &value : templatesArray) {
    FormatTemplate tmpl = parseTemplate(value.toObject());
    if (tmpl.isValid()) {

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

  LOG_INFO(QString("Loaded %1 user format templates").arg(userTemplateCount));
  return true;
}

FormatTemplate
FormatTemplateManager::parseTemplate(const QJsonObject &obj) const {
  FormatTemplate tmpl;

  tmpl.id = obj.value("id").toString();
  tmpl.name = obj.value("name").toString();
  tmpl.description = obj.value("description").toString();
  tmpl.language = obj.value("language").toString();
  tmpl.command = obj.value("command").toString();
  tmpl.inPlace = obj.value("inPlace").toBool(true);

  QJsonArray extArray = obj.value("extensions").toArray();
  for (const QJsonValue &ext : extArray) {
    tmpl.extensions.append(ext.toString());
  }

  QJsonArray argsArray = obj.value("args").toArray();
  for (const QJsonValue &arg : argsArray) {
    tmpl.args.append(arg.toString());
  }

  return tmpl;
}

QList<FormatTemplate> FormatTemplateManager::getAllTemplates() const {
  return m_templates;
}

QList<FormatTemplate> FormatTemplateManager::getTemplatesForExtension(
    const QString &extension) const {
  QList<FormatTemplate> result;
  QString ext = extension.toLower();
  if (ext.startsWith('.')) {
    ext = ext.mid(1);
  }

  for (const FormatTemplate &tmpl : m_templates) {
    for (const QString &templateExt : tmpl.extensions) {
      if (templateExt.toLower() == ext) {
        result.append(tmpl);
        break;
      }
    }
  }

  return result;
}

FormatTemplate FormatTemplateManager::getTemplateById(const QString &id) const {
  for (const FormatTemplate &tmpl : m_templates) {
    if (tmpl.id == id) {
      return tmpl;
    }
  }
  return FormatTemplate();
}

QString
FormatTemplateManager::getConfigDirForFile(const QString &filePath) const {
  QFileInfo fileInfo(filePath);
  return fileInfo.absoluteDir().path() + "/.lightpad";
}

QString
FormatTemplateManager::getConfigFileForDir(const QString &dirPath) const {
  return dirPath + "/format_config.json";
}

bool FormatTemplateManager::loadAssignmentsFromDir(
    const QString &dirPath) const {
  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/format_config.json";

  if (m_loadedConfigDirs.contains(configDir)) {
    return true;
  }

  if (!QFileInfo(configFile).exists()) {
    m_loadedConfigDirs.insert(configDir);
    return true;
  }

  QFile file(configFile);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARNING(QString("Failed to open format config: %1").arg(configFile));
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    LOG_WARNING(QString("Failed to parse format config: %1")
                    .arg(parseError.errorString()));
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray assignments = root.value("assignments").toArray();

  for (const QJsonValue &value : assignments) {
    QJsonObject obj = value.toObject();
    FileFormatAssignment assignment;
    assignment.filePath = obj.value("file").toString();
    assignment.templateId = obj.value("template").toString();

    if (!QFileInfo(assignment.filePath).isAbsolute()) {
      assignment.filePath = dirPath + "/" + assignment.filePath;
    }

    QJsonArray argsArray = obj.value("customArgs").toArray();
    for (const QJsonValue &arg : argsArray) {
      assignment.customArgs.append(arg.toString());
    }

    m_assignments[assignment.filePath] = assignment;
  }

  m_loadedConfigDirs.insert(configDir);
  LOG_INFO(QString("Loaded %1 format assignments from %2")
               .arg(assignments.size())
               .arg(configFile));
  return true;
}

bool FormatTemplateManager::saveAssignmentsToDir(const QString &dirPath) const {
  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/format_config.json";

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

      assignments.append(obj);
    }
  }

  QDir dir;
  if (!dir.exists(configDir)) {
    if (!dir.mkpath(configDir)) {
      LOG_ERROR(
          QString("Failed to create config directory: %1").arg(configDir));
      return false;
    }
  }

  QJsonObject root;
  root["version"] = "1.0";
  root["assignments"] = assignments;

  QFile file(configFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to write format config: %1").arg(configFile));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  LOG_INFO(QString("Saved %1 format assignments to %2")
               .arg(assignments.size())
               .arg(configFile));
  return true;
}

FileFormatAssignment
FormatTemplateManager::getAssignmentForFile(const QString &filePath) const {

  QFileInfo fileInfo(filePath);
  QString dirPath = fileInfo.absoluteDir().path();

  loadAssignmentsFromDir(dirPath);

  auto it = m_assignments.find(filePath);
  if (it != m_assignments.end()) {
    return it.value();
  }

  return FileFormatAssignment();
}

bool FormatTemplateManager::assignTemplateToFile(
    const QString &filePath, const QString &templateId,
    const QStringList &customArgs) {
  FileFormatAssignment assignment;
  assignment.filePath = filePath;
  assignment.templateId = templateId;
  assignment.customArgs = customArgs;

  m_assignments[filePath] = assignment;

  QFileInfo fileInfo(filePath);
  QString dirPath = fileInfo.absoluteDir().path();

  bool saved = saveAssignmentsToDir(dirPath);

  if (saved) {
    emit assignmentChanged(filePath);
  }

  return saved;
}

bool FormatTemplateManager::removeAssignment(const QString &filePath) {
  auto it = m_assignments.find(filePath);
  if (it == m_assignments.end()) {
    return true;
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

QString FormatTemplateManager::substituteVariables(const QString &input,
                                                   const QString &filePath) {
  QString result = input;
  QFileInfo fileInfo(filePath);

  result.replace("${file}", filePath);
  result.replace("${fileDir}", fileInfo.absoluteDir().path());
  result.replace("${fileBasename}", fileInfo.fileName());
  result.replace("${fileBasenameNoExt}", fileInfo.completeBaseName());
  result.replace("${fileExt}", fileInfo.suffix());

  return result;
}

QPair<QString, QStringList>
FormatTemplateManager::buildCommand(const QString &filePath) const {
  FileFormatAssignment assignment = getAssignmentForFile(filePath);

  QString templateId = assignment.templateId;

  if (templateId.isEmpty()) {
    QFileInfo fileInfo(filePath);
    QList<FormatTemplate> templates =
        getTemplatesForExtension(fileInfo.suffix());
    if (!templates.isEmpty()) {
      templateId = templates.first().id;
    }
  }

  if (templateId.isEmpty()) {
    return qMakePair(QString(), QStringList());
  }

  FormatTemplate tmpl = getTemplateById(templateId);
  if (!tmpl.isValid()) {
    return qMakePair(QString(), QStringList());
  }

  QString command = substituteVariables(tmpl.command, filePath);

  QStringList args;
  for (const QString &arg : tmpl.args) {
    args.append(substituteVariables(arg, filePath));
  }

  if (!assignment.customArgs.isEmpty()) {
    for (const QString &arg : assignment.customArgs) {
      args.append(substituteVariables(arg, filePath));
    }
  }

  return qMakePair(command, args);
}

bool FormatTemplateManager::hasFormatTemplate(const QString &filePath) const {
  if (filePath.isEmpty()) {
    return false;
  }

  FileFormatAssignment assignment = getAssignmentForFile(filePath);
  if (!assignment.templateId.isEmpty()) {
    return true;
  }

  QFileInfo fileInfo(filePath);
  QList<FormatTemplate> templates = getTemplatesForExtension(fileInfo.suffix());
  return !templates.isEmpty();
}
