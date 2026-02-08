#ifndef RUNTEMPLATEMANAGER_H
#define RUNTEMPLATEMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>

/**
 * @brief Represents a single run template configuration
 */
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

/**
 * @brief Represents a file-to-template assignment stored per directory
 */
struct FileTemplateAssignment {
  QString filePath;
  QString templateId;
  QStringList customArgs;
  QMap<QString, QString> customEnv;
};

/**
 * @brief Manages run templates for executing programs
 *
 * Provides:
 * - Loading of built-in run templates from JSON
 * - Loading of user-defined templates
 * - Per-directory template assignments stored in .lightpad/run_config.json
 * - Variable substitution for file paths and names
 */
class RunTemplateManager : public QObject {
  Q_OBJECT

public:
  static RunTemplateManager &instance();

  /**
   * @brief Load all templates from built-in and user directories
   * @return true if at least the built-in templates were loaded
   */
  bool loadTemplates();

  /**
   * @brief Get all available templates
   * @return List of all loaded templates
   */
  QList<RunTemplate> getAllTemplates() const;

  /**
   * @brief Get templates suitable for a file extension
   * @param extension File extension (without dot)
   * @return List of matching templates
   */
  QList<RunTemplate> getTemplatesForExtension(const QString &extension) const;

  /**
   * @brief Get templates suitable for a language ID
   * @param languageId Canonical language ID (e.g. "cpp", "py", "js")
   * @return List of matching templates
   */
  QList<RunTemplate> getTemplatesForLanguageId(const QString &languageId) const;

  /**
   * @brief Get a template by its ID
   * @param id Template identifier
   * @return The template, or invalid template if not found
   */
  RunTemplate getTemplateById(const QString &id) const;

  /**
   * @brief Get the assigned template for a file
   * @param filePath Full path to the file
   * @return The assignment, or empty assignment if none
   */
  FileTemplateAssignment getAssignmentForFile(const QString &filePath) const;

  /**
   * @brief Assign a template to a file
   * @param filePath Full path to the file
   * @param templateId ID of the template to assign
   * @param customArgs Optional custom arguments
   * @param customEnv Optional custom environment variables
   * @return true if the assignment was saved successfully
   */
  bool assignTemplateToFile(
      const QString &filePath, const QString &templateId,
      const QStringList &customArgs = QStringList(),
      const QMap<QString, QString> &customEnv = QMap<QString, QString>());

  /**
   * @brief Remove template assignment for a file
   * @param filePath Full path to the file
   * @return true if the assignment was removed
   */
  bool removeAssignment(const QString &filePath);

  /**
   * @brief Build the command to execute for a file
   * @param filePath Full path to the file
   * @return A pair of (command, arguments) with variables substituted
   */
  QPair<QString, QStringList>
  buildCommand(const QString &filePath,
               const QString &languageId = QString()) const;

  /**
   * @brief Get the working directory for execution
   * @param filePath Full path to the file
   * @return Working directory path with variables substituted
   */
  QString getWorkingDirectory(const QString &filePath,
                              const QString &languageId = QString()) const;

  /**
   * @brief Get environment variables for execution
   * @param filePath Full path to the file
   * @return Map of environment variable name to value
   */
  QMap<QString, QString> getEnvironment(
      const QString &filePath, const QString &languageId = QString()) const;

  /**
   * @brief Substitute variables in a string
   * @param input String with variables like ${file}, ${fileDir}, etc.
   * @param filePath Full path to the current file
   * @return String with variables replaced
   */
  static QString substituteVariables(const QString &input,
                                     const QString &filePath);

  /**
   * @brief Save assignments for a directory to its config file
   * @param dirPath Path to the directory
   * @return true if the assignments were saved successfully
   */
  bool saveAssignmentsToDir(const QString &dirPath) const;

signals:
  /**
   * @brief Emitted when templates are loaded or reloaded
   */
  void templatesLoaded();

  /**
   * @brief Emitted when a file assignment changes
   * @param filePath The file whose assignment changed
   */
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
  mutable QMap<QString, FileTemplateAssignment>
      m_assignments; // filePath -> assignment
  mutable QSet<QString> m_loadedConfigDirs;
};

#endif // RUNTEMPLATEMANAGER_H
