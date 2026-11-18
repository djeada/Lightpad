#ifndef RUNTEMPLATEMANAGER_H
#define RUNTEMPLATEMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>

#include "../python/pythonprojectenvironment.h"

struct RunTemplate {
  QString id;
  QString name;
  QString description;
  QString language;
  QString languageId;
  QStringList extensions;
  // Extensions for which this template wins over the language's first
  // template (C files share the "cpp" language with C++).
  QStringList defaultFor;
  QString command;
  QStringList args;
  QString workingDirectory;
  QStringList projectMarkers;
  QMap<QString, QString> env;
  // How Debug starts a file run by this template: a launch.json style object
  // ("type", "program", "module", ...) plus an optional "build" command line
  // that produces a debuggable program. Empty when the template has no
  // debugger recipe.
  QJsonObject debug;

  bool isValid() const { return !id.isEmpty() && !command.isEmpty(); }
  bool isDebuggable() const { return !debug.isEmpty(); }
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
  QString pythonMode;
  QString pythonInterpreter;
  QString pythonVenvPath;
  QString pythonRequirementsFile;

  // True when nothing differs from the automatic defaults.
  bool isEmpty() const {
    return templateId.isEmpty() && customArgs.isEmpty() &&
           customEnv.isEmpty() && sourceFiles.isEmpty() &&
           workingDirectory.isEmpty() && compilerFlags.isEmpty() &&
           preRunCommand.isEmpty() && postRunCommand.isEmpty() &&
           pythonMode.isEmpty() && pythonInterpreter.isEmpty() &&
           pythonVenvPath.isEmpty() && pythonRequirementsFile.isEmpty();
  }
};

class RunTemplateManager : public QObject {
  Q_OBJECT

public:
  static RunTemplateManager &instance();

  bool loadTemplates();

  QList<RunTemplate> getAllTemplates() const;

  QList<RunTemplate> getTemplatesForExtension(const QString &extension) const;
  QList<RunTemplate> getTemplatesForFilePath(const QString &filePath) const;

  QList<RunTemplate> getTemplatesForLanguageId(const QString &languageId) const;

  RunTemplate getTemplateById(const QString &id) const;

  FileTemplateAssignment getAssignmentForFile(const QString &filePath) const;

  bool assignTemplateToFile(const QString &filePath,
                            const FileTemplateAssignment &assignment);

  bool removeAssignment(const QString &filePath);

  void setWorkspaceFolder(const QString &folder);
  QString workspaceFolder() const { return m_workspaceFolder; }

  QPair<QString, QStringList>
  buildCommand(const QString &filePath,
               const QString &languageId = QString()) const;

  QString getWorkingDirectory(const QString &filePath,
                              const QString &languageId = QString()) const;

  QMap<QString, QString>
  getEnvironment(const QString &filePath,
                 const QString &languageId = QString()) const;

  QString
  effectiveTemplateIdForFile(const QString &filePath,
                             const QString &languageId = QString()) const;

  // The template Run picks for the file when none is assigned.
  QString
  automaticTemplateIdForFile(const QString &filePath,
                             const QString &languageId = QString()) const {
    return resolveTemplateIdForFile(filePath, languageId);
  }

  // While alive, getAssignmentForFile(filePath) returns the given assignment
  // instead of the saved one, so a configuration dialog can preview exactly
  // what Run and Debug would do with unsaved edits.
  class AssignmentPreview {
  public:
    AssignmentPreview(const QString &filePath,
                      const FileTemplateAssignment &assignment);
    ~AssignmentPreview();
    AssignmentPreview(const AssignmentPreview &) = delete;
    AssignmentPreview &operator=(const AssignmentPreview &) = delete;
  };

  static QString substituteVariables(const QString &input,
                                     const QString &filePath);
  static QString substituteCommandLineVariables(const QString &input,
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
  PythonEnvironmentPreference
  pythonPreferenceForAssignment(const FileTemplateAssignment &assignment) const;
  QString resolveTemplateIdForFile(const QString &filePath,
                                   const QString &languageId) const;

  bool loadAssignments() const;
  bool saveAssignments() const;

  QList<RunTemplate> m_templates;
  mutable QMap<QString, FileTemplateAssignment> m_assignments;
  mutable bool m_assignmentsLoaded = false;
  QString m_workspaceFolder;
  bool m_previewActive = false;
  QString m_previewFilePath;
  FileTemplateAssignment m_previewAssignment;
};

#endif
