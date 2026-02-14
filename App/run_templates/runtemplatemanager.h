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

  bool assignTemplateToFile(
      const QString &filePath, const QString &templateId,
      const QStringList &customArgs = QStringList(),
      const QMap<QString, QString> &customEnv = QMap<QString, QString>());

  bool removeAssignment(const QString &filePath);

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

  bool saveAssignmentsToDir(const QString &dirPath) const;

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

  QString getConfigDirForFile(const QString &filePath) const;
  QString getConfigFileForDir(const QString &dirPath) const;
  bool loadAssignmentsFromDir(const QString &dirPath) const;

  QList<RunTemplate> m_templates;
  mutable QMap<QString, FileTemplateAssignment> m_assignments;
  mutable QSet<QString> m_loadedConfigDirs;
};

#endif
