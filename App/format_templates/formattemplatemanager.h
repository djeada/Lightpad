#ifndef FORMATTEMPLATEMANAGER_H
#define FORMATTEMPLATEMANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>

/**
 * @brief Represents a single format template configuration
 */
struct FormatTemplate {
    QString id;
    QString name;
    QString description;
    QString language;
    QStringList extensions;
    QString command;
    QStringList args;
    bool inPlace;  // Whether the formatter modifies the file in place
    
    bool isValid() const { return !id.isEmpty() && !command.isEmpty(); }
};

/**
 * @brief Represents a file-to-format-template assignment stored per directory
 */
struct FileFormatAssignment {
    QString filePath;
    QString templateId;
    QStringList customArgs;
};

/**
 * @brief Manages format templates for auto-formatting code
 * 
 * Provides:
 * - Loading of built-in format templates from JSON
 * - Loading of user-defined templates from config file
 * - Per-directory template assignments stored in .lightpad/format_config.json
 * - Variable substitution for file paths and names
 * 
 * @note This class is NOT thread-safe. All access should be from the main UI thread.
 */
class FormatTemplateManager : public QObject {
    Q_OBJECT

public:
    static FormatTemplateManager& instance();
    
    /**
     * @brief Load all templates from built-in resources and user config file
     * @return true if at least the built-in templates were loaded
     */
    bool loadTemplates();
    
    /**
     * @brief Get all available templates
     * @return List of all loaded templates
     */
    QList<FormatTemplate> getAllTemplates() const;
    
    /**
     * @brief Get templates suitable for a file extension
     * @param extension File extension (without dot)
     * @return List of matching templates
     */
    QList<FormatTemplate> getTemplatesForExtension(const QString& extension) const;
    
    /**
     * @brief Get a template by its ID
     * @param id Template identifier
     * @return The template, or invalid template if not found
     */
    FormatTemplate getTemplateById(const QString& id) const;
    
    /**
     * @brief Get the assigned template for a file
     * @param filePath Full path to the file
     * @return The assignment, or empty assignment if none
     */
    FileFormatAssignment getAssignmentForFile(const QString& filePath) const;
    
    /**
     * @brief Assign a template to a file
     * @param filePath Full path to the file
     * @param templateId ID of the template to assign
     * @param customArgs Optional custom arguments
     * @return true if the assignment was saved successfully
     */
    bool assignTemplateToFile(const QString& filePath, 
                              const QString& templateId,
                              const QStringList& customArgs = QStringList());
    
    /**
     * @brief Remove template assignment for a file
     * @param filePath Full path to the file
     * @return true if the assignment was removed
     */
    bool removeAssignment(const QString& filePath);
    
    /**
     * @brief Build the command to execute for formatting a file
     * @param filePath Full path to the file
     * @return A pair of (command, arguments) with variables substituted
     */
    QPair<QString, QStringList> buildCommand(const QString& filePath) const;
    
    /**
     * @brief Check if a format template is assigned to or available for a file
     * @param filePath Full path to the file
     * @return true if formatting is available
     */
    bool hasFormatTemplate(const QString& filePath) const;
    
    /**
     * @brief Substitute variables in a string
     * @param input String with variables like ${file}, ${fileDir}, etc.
     * @param filePath Full path to the current file
     * @return String with variables replaced
     */
    static QString substituteVariables(const QString& input, const QString& filePath);

    /**
     * @brief Save format assignments for a directory
     * @param dirPath Path to the directory
     * @return true if the assignments were saved successfully
     */
    bool saveAssignmentsToDir(const QString& dirPath) const;

signals:
    /**
     * @brief Emitted when templates are loaded or reloaded
     */
    void templatesLoaded();
    
    /**
     * @brief Emitted when a file assignment changes
     * @param filePath The file whose assignment changed
     */
    void assignmentChanged(const QString& filePath);

private:
    FormatTemplateManager();
    ~FormatTemplateManager() = default;
    FormatTemplateManager(const FormatTemplateManager&) = delete;
    FormatTemplateManager& operator=(const FormatTemplateManager&) = delete;
    
    bool loadBuiltInTemplates();
    bool loadUserTemplates();
    FormatTemplate parseTemplate(const QJsonObject& obj) const;
    
    QString getConfigDirForFile(const QString& filePath) const;
    QString getConfigFileForDir(const QString& dirPath) const;
    bool loadAssignmentsFromDir(const QString& dirPath) const;
    
    QList<FormatTemplate> m_templates;
    mutable QMap<QString, FileFormatAssignment> m_assignments; // filePath -> assignment
    mutable QSet<QString> m_loadedConfigDirs;
};

#endif // FORMATTEMPLATEMANAGER_H
