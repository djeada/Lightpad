#ifndef RUNTEMPLATEMANAGER_H
#define RUNTEMPLATEMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>

struct RunTemplate {
  QString id;
  QString name;
  QString description;
  QString language;
  QString languageId;
  QStringList extensions;
  QString command;
  QStringList args;
  QString workingDirectory;
  QMap<QString, QString> env;

  bool isValid() const { return !id.isEmpty() && !command.isEmpty(); }
};

struct FileTemplateAssignment {
  QString filePath;
  QString templateId;
  QStringList customArgs;
  QMap<QString, QString> customEnv;
  QStringList sourceFiles;
  QString workingDirectory;
  QStringList compilerFlags;
  QString preRunCommand;
  QString postRunCommand;
};

class RunTemplateManager : public QObject {
  Q_OBJECT

public:
  static RunTemplateManager &instance();

  bool loadTemplates();

  QList<RunTemplate> getAllTemplates() const;

  QList<RunTemplate> getTemplatesForExtension(const QString &extension) const;

  QList<RunTemplate> getTemplatesForLanguageId(const QString &languageId) const;

  RunTemplate getTemplateById(const QString &id) const;

  FileTemplateAssignment getAssignmentForFile(const QString &filePath) const;

  bool assignTemplateToFile(const QString &filePath,
                            const FileTemplateAssignment &assignment);

  bool removeAssignment(const QString &filePath);

  void setWorkspaceFolder(const QString &folder);

  QPair<QString, QStringList>
  buildCommand(const QString &filePath,
               const QString &languageId = QString()) const;

  QString getWorkingDirectory(const QString &filePath,
                              const QString &languageId = QString()) const;

  QMap<QString, QString>
  getEnvironment(const QString &filePath,
                 const QString &languageId = QString()) const;

  static QString substituteVariables(const QString &input,
                                     const QString &filePath);

signals:

  void templatesLoaded();

  void assignmentChanged(const QString &filePath);

private:
  RunTemplateManager();
  ~RunTemplateManager() = default;
  RunTemplateManager(const RunTemplateManager &) = delete;
  RunTemplateManager &operator=(const RunTemplateManager &) = delete;

  bool loadBuiltInTemplates();
  bool loadUserTemplates();
  RunTemplate parseTemplate(const QJsonObject &obj) const;
  QString resolveTemplateIdForFile(const QString &filePath,
                                   const QString &languageId) const;

  bool loadAssignments() const;
  bool saveAssignments() const;

  QList<RunTemplate> m_templates;
  mutable QMap<QString, FileTemplateAssignment> m_assignments;
  mutable bool m_assignmentsLoaded = false;
  QString m_workspaceFolder;
};

#endif
