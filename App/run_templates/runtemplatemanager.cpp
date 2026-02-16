#include "runtemplatemanager.h"
#include "../core/logging/logger.h"
#include "../language/languagecatalog.h"

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

  QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QString userTemplatesPath = configDir + "/run_templates.json";

  if (!QFileInfo(userTemplatesPath).exists()) {
    return true;
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
  tmpl.languageId =
      LanguageCatalog::normalize(obj.value("languageId").toString());
  if (tmpl.languageId.isEmpty()) {
    tmpl.languageId = LanguageCatalog::normalize(tmpl.language);
  }
  if (tmpl.language.isEmpty() && !tmpl.languageId.isEmpty()) {
    tmpl.language = LanguageCatalog::displayName(tmpl.languageId);
  }
  if (tmpl.language.isEmpty()) {
    tmpl.language = tmpl.languageId;
  }
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

QList<RunTemplate>
RunTemplateManager::getTemplatesForLanguageId(const QString &languageId) const {
  QList<RunTemplate> result;
  QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (canonicalLanguageId.isEmpty()) {
    return result;
  }

  for (const RunTemplate &tmpl : m_templates) {
    if (LanguageCatalog::normalize(tmpl.languageId) == canonicalLanguageId) {
      result.append(tmpl);
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

void RunTemplateManager::setWorkspaceFolder(const QString &folder) {
  if (m_workspaceFolder != folder) {
    m_workspaceFolder = folder;
    m_assignmentsLoaded = false;
    m_assignments.clear();
  }
}

bool RunTemplateManager::loadAssignments() const {
  if (m_assignmentsLoaded) {
    return true;
  }

  if (m_workspaceFolder.isEmpty()) {
    return false;
  }

  QString configFile = m_workspaceFolder + "/.lightpad/run_config.json";

  if (!QFileInfo(configFile).exists()) {
    m_assignmentsLoaded = true;
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

    if (!QFileInfo(assignment.filePath).isAbsolute()) {
      assignment.filePath = m_workspaceFolder + "/" + assignment.filePath;
    }

    QJsonArray argsArray = obj.value("customArgs").toArray();
    for (const QJsonValue &arg : argsArray) {
      assignment.customArgs.append(arg.toString());
    }

    QJsonObject envObj = obj.value("customEnv").toObject();
    for (auto it = envObj.begin(); it != envObj.end(); ++it) {
      assignment.customEnv[it.key()] = it.value().toString();
    }

    QJsonArray srcArray = obj.value("sourceFiles").toArray();
    for (const QJsonValue &src : srcArray) {
      assignment.sourceFiles.append(src.toString());
    }

    assignment.workingDirectory = obj.value("workingDirectory").toString();

    QJsonArray flagsArray = obj.value("compilerFlags").toArray();
    for (const QJsonValue &flag : flagsArray) {
      assignment.compilerFlags.append(flag.toString());
    }

    assignment.preRunCommand = obj.value("preRunCommand").toString();
    assignment.postRunCommand = obj.value("postRunCommand").toString();

    m_assignments[assignment.filePath] = assignment;
  }

  m_assignmentsLoaded = true;
  LOG_INFO(QString("Loaded %1 run assignments from %2")
               .arg(assignments.size())
               .arg(configFile));
  return true;
}

bool RunTemplateManager::saveAssignments() const {
  if (m_workspaceFolder.isEmpty()) {
    LOG_WARNING("Cannot save run config: workspace folder not set");
    return false;
  }

  QString configDir = m_workspaceFolder + "/.lightpad";
  QString configFile = configDir + "/run_config.json";

  QJsonArray assignmentsArray;
  for (auto it = m_assignments.begin(); it != m_assignments.end(); ++it) {
    QJsonObject obj;

    QString storedPath = it.key();
    if (storedPath.startsWith(m_workspaceFolder + "/")) {
      storedPath = storedPath.mid(m_workspaceFolder.length() + 1);
    }
    obj["file"] = storedPath;
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

    if (!it.value().sourceFiles.isEmpty()) {
      QJsonArray srcArray;
      for (const QString &src : it.value().sourceFiles) {
        srcArray.append(src);
      }
      obj["sourceFiles"] = srcArray;
    }

    if (!it.value().workingDirectory.isEmpty()) {
      obj["workingDirectory"] = it.value().workingDirectory;
    }

    if (!it.value().compilerFlags.isEmpty()) {
      QJsonArray flagsArray;
      for (const QString &flag : it.value().compilerFlags) {
        flagsArray.append(flag);
      }
      obj["compilerFlags"] = flagsArray;
    }

    if (!it.value().preRunCommand.isEmpty()) {
      obj["preRunCommand"] = it.value().preRunCommand;
    }

    if (!it.value().postRunCommand.isEmpty()) {
      obj["postRunCommand"] = it.value().postRunCommand;
    }

    assignmentsArray.append(obj);
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
  root["assignments"] = assignmentsArray;

  QFile file(configFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to write run config: %1").arg(configFile));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  LOG_INFO(QString("Saved %1 run assignments to %2")
               .arg(assignmentsArray.size())
               .arg(configFile));
  return true;
}

FileTemplateAssignment
RunTemplateManager::getAssignmentForFile(const QString &filePath) const {

  loadAssignments();

  auto it = m_assignments.find(filePath);
  if (it != m_assignments.end()) {
    return it.value();
  }

  return FileTemplateAssignment();
}

bool RunTemplateManager::assignTemplateToFile(
    const QString &filePath, const FileTemplateAssignment &assignment) {
  FileTemplateAssignment stored = assignment;
  stored.filePath = filePath;

  m_assignments[filePath] = stored;

  bool saved = saveAssignments();

  if (saved) {
    emit assignmentChanged(filePath);
  }

  return saved;
}

bool RunTemplateManager::removeAssignment(const QString &filePath) {
  auto it = m_assignments.find(filePath);
  if (it == m_assignments.end()) {
    return true;
  }

  m_assignments.erase(it);

  bool saved = saveAssignments();

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

  result.replace("${workspaceFolder}", fileInfo.absoluteDir().path());

  return result;
}

QPair<QString, QStringList>
RunTemplateManager::buildCommand(const QString &filePath,
                                 const QString &languageId) const {
  FileTemplateAssignment assignment = getAssignmentForFile(filePath);

  QString templateId = assignment.templateId;
  if (templateId.isEmpty()) {
    templateId = resolveTemplateIdForFile(filePath, languageId);
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

  QStringList extraFlags;
  for (const QString &flag : assignment.compilerFlags) {
    extraFlags.append(substituteVariables(flag, filePath));
  }

  QStringList extraArgs;
  for (const QString &arg : assignment.customArgs) {
    extraArgs.append(substituteVariables(arg, filePath));
  }

  QStringList extraSources;
  for (const QString &src : assignment.sourceFiles) {
    extraSources.append(substituteVariables(src, filePath));
  }

  bool hasExtras =
      !extraFlags.isEmpty() || !extraArgs.isEmpty() || !extraSources.isEmpty();

  bool isBashC = hasExtras && (command == "bash" || command == "sh") &&
                 args.contains("-c");

  if (isBashC) {
    int cIdx = args.indexOf("-c");
    if (cIdx >= 0 && cIdx + 1 < args.size()) {
      QString shellCmd = args[cIdx + 1];

      QStringList injection;
      for (const QString &f : extraFlags) {
        injection.append(f);
      }
      for (const QString &a : extraArgs) {
        injection.append(a);
      }
      for (const QString &s : extraSources) {
        injection.append("\"" + s + "\"");
      }

      if (!injection.isEmpty()) {
        QString extra = " " + injection.join(" ");

        int andIdx = shellCmd.indexOf("&&");
        if (andIdx > 0) {
          shellCmd.insert(andIdx, extra + " ");
        } else {
          shellCmd.append(extra);
        }
        args[cIdx + 1] = shellCmd;
      }
    }
  } else {

    args.append(extraFlags);
    args.append(extraArgs);
    args.append(extraSources);
  }

  return qMakePair(command, args);
}

QString
RunTemplateManager::getWorkingDirectory(const QString &filePath,
                                        const QString &languageId) const {
  FileTemplateAssignment assignment = getAssignmentForFile(filePath);

  if (!assignment.workingDirectory.isEmpty()) {
    return substituteVariables(assignment.workingDirectory, filePath);
  }

  QString templateId = assignment.templateId;
  if (templateId.isEmpty()) {
    templateId = resolveTemplateIdForFile(filePath, languageId);
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
RunTemplateManager::getEnvironment(const QString &filePath,
                                   const QString &languageId) const {
  FileTemplateAssignment assignment = getAssignmentForFile(filePath);

  QString templateId = assignment.templateId;
  if (templateId.isEmpty()) {
    templateId = resolveTemplateIdForFile(filePath, languageId);
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

  for (auto it = assignment.customEnv.begin(); it != assignment.customEnv.end();
       ++it) {
    env[it.key()] = substituteVariables(it.value(), filePath);
  }

  return env;
}

QString
RunTemplateManager::resolveTemplateIdForFile(const QString &filePath,
                                             const QString &languageId) const {
  QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (!canonicalLanguageId.isEmpty()) {
    QList<RunTemplate> languageTemplates =
        getTemplatesForLanguageId(canonicalLanguageId);
    if (!languageTemplates.isEmpty()) {
      return languageTemplates.first().id;
    }
  }

  QFileInfo fileInfo(filePath);
  QList<RunTemplate> extensionTemplates =
      getTemplatesForExtension(fileInfo.suffix());
  if (!extensionTemplates.isEmpty()) {
    return extensionTemplates.first().id;
  }

  return {};
}
