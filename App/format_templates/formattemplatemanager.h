#ifndef FORMATTEMPLATEMANAGER_H
#define FORMATTEMPLATEMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>

#include "../python/pythonprojectenvironment.h"

struct FormatTemplate {
  QString id;
  QString name;
  QString description;
  QString language;
  QStringList extensions;
  QString command;
  QStringList args;
  bool inPlace;

  bool isValid() const { return !id.isEmpty() && !command.isEmpty(); }
};

struct FileFormatAssignment {
  QString filePath;
  QString templateId;
  QStringList customArgs;
  QMap<QString, QString> customEnv;
  QString workingDirectory;
  QString preFormatCommand;
  QString postFormatCommand;
  QString pythonMode;
  QString pythonInterpreter;
  QString pythonVenvPath;
  QString pythonRequirementsFile;
};

class FormatTemplateManager : public QObject {
  Q_OBJECT

public:
  static FormatTemplateManager &instance();

  bool loadTemplates();

  QList<FormatTemplate> getAllTemplates() const;

  QList<FormatTemplate>
  getTemplatesForExtension(const QString &extension) const;

  FormatTemplate getTemplateById(const QString &id) const;

  FileFormatAssignment getAssignmentForFile(const QString &filePath) const;

  bool assignTemplateToFile(const QString &filePath,
                            const FileFormatAssignment &assignment);

  bool assignTemplateToFile(const QString &filePath, const QString &templateId,
                            const QStringList &customArgs = QStringList());

  bool removeAssignment(const QString &filePath);

  QPair<QString, QStringList> buildCommand(const QString &filePath) const;
  QString getWorkingDirectory(const QString &filePath) const;
  QMap<QString, QString> getEnvironment(const QString &filePath) const;

  bool hasFormatTemplate(const QString &filePath) const;

  static QString substituteVariables(const QString &input,
                                     const QString &filePath);

  void setWorkspaceFolder(const QString &folder) { m_workspaceFolder = folder; }
  QString workspaceFolder() const { return m_workspaceFolder; }

  bool saveAssignmentsToDir(const QString &dirPath) const;

signals:

  void templatesLoaded();

  void assignmentChanged(const QString &filePath);

private:
  FormatTemplateManager();
  ~FormatTemplateManager() = default;
  FormatTemplateManager(const FormatTemplateManager &) = delete;
  FormatTemplateManager &operator=(const FormatTemplateManager &) = delete;

  bool loadBuiltInTemplates();
  bool loadUserTemplates();
  FormatTemplate parseTemplate(const QJsonObject &obj) const;
  PythonEnvironmentPreference
  pythonPreferenceForAssignment(const FileFormatAssignment &assignment) const;

  QString getConfigDirForFile(const QString &filePath) const;
  QString getConfigFileForDir(const QString &dirPath) const;
  bool loadAssignmentsFromDir(const QString &dirPath) const;

  QList<FormatTemplate> m_templates;
  mutable QMap<QString, FileFormatAssignment> m_assignments;
  mutable QSet<QString> m_loadedConfigDirs;
  QString m_workspaceFolder;
};

#endif
