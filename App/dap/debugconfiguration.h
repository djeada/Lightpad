#ifndef DEBUGCONFIGURATION_H
#define DEBUGCONFIGURATION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QList>

/**
 * @brief A single debug configuration (launch or attach)
 * 
 * Similar to VS Code's launch.json configurations, this allows users
 * to define reusable debug configurations for their projects.
 */
struct DebugConfiguration {
    QString name;                    // User-visible name
    QString type;                    // Debug adapter type (e.g., "python", "node", "cppdbg")
    QString request;                 // "launch" or "attach"
    
    // Common launch properties
    QString program;                 // Program to debug
    QStringList args;                // Command line arguments
    QString cwd;                     // Working directory
    QMap<QString, QString> env;      // Environment variables
    bool stopOnEntry = false;        // Stop at entry point
    
    // Common attach properties
    int processId = 0;               // Process ID for local attach
    QString host;                    // Host for remote attach
    int port = 0;                    // Port for remote attach
    
    // Pre/post debug tasks
    QString preLaunchTask;           // Task to run before debug
    QString postDebugTask;           // Task to run after debug
    
    // Additional adapter-specific configuration
    QJsonObject adapterConfig;       // Extra configuration passed to adapter
    
    // Presentation
    QString presentation;            // "hidden", "normal" (default)
    int order = 0;                   // Sort order in UI
    
    /**
     * @brief Convert to JSON for saving
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["type"] = type;
        obj["request"] = request;
        
        if (!program.isEmpty()) obj["program"] = program;
        if (!args.isEmpty()) {
            QJsonArray argsArray;
            for (const QString& arg : args) {
                argsArray.append(arg);
            }
            obj["args"] = argsArray;
        }
        if (!cwd.isEmpty()) obj["cwd"] = cwd;
        if (!env.isEmpty()) {
            QJsonObject envObj;
            for (auto it = env.begin(); it != env.end(); ++it) {
                envObj[it.key()] = it.value();
            }
            obj["env"] = envObj;
        }
        if (stopOnEntry) obj["stopOnEntry"] = stopOnEntry;
        
        if (processId > 0) obj["processId"] = processId;
        if (!host.isEmpty()) obj["host"] = host;
        if (port > 0) obj["port"] = port;
        
        if (!preLaunchTask.isEmpty()) obj["preLaunchTask"] = preLaunchTask;
        if (!postDebugTask.isEmpty()) obj["postDebugTask"] = postDebugTask;
        
        // Merge adapter-specific config
        for (auto it = adapterConfig.begin(); it != adapterConfig.end(); ++it) {
            obj[it.key()] = it.value();
        }
        
        if (!presentation.isEmpty()) obj["presentation"] = presentation;
        if (order != 0) obj["order"] = order;
        
        return obj;
    }
    
    /**
     * @brief Load from JSON
     */
    static DebugConfiguration fromJson(const QJsonObject& obj) {
        DebugConfiguration cfg;
        cfg.name = obj["name"].toString();
        cfg.type = obj["type"].toString();
        cfg.request = obj["request"].toString("launch");
        
        cfg.program = obj["program"].toString();
        if (obj.contains("args")) {
            QJsonArray argsArray = obj["args"].toArray();
            for (const auto& arg : argsArray) {
                cfg.args.append(arg.toString());
            }
        }
        cfg.cwd = obj["cwd"].toString();
        if (obj.contains("env")) {
            QJsonObject envObj = obj["env"].toObject();
            for (auto it = envObj.begin(); it != envObj.end(); ++it) {
                cfg.env[it.key()] = it.value().toString();
            }
        }
        cfg.stopOnEntry = obj["stopOnEntry"].toBool();
        
        cfg.processId = obj["processId"].toInt();
        cfg.host = obj["host"].toString();
        cfg.port = obj["port"].toInt();
        
        cfg.preLaunchTask = obj["preLaunchTask"].toString();
        cfg.postDebugTask = obj["postDebugTask"].toString();
        
        cfg.presentation = obj["presentation"].toString();
        cfg.order = obj["order"].toInt();
        
        // Collect adapter-specific config (keys we didn't handle above)
        static const QStringList knownKeys = {
            "name", "type", "request", "program", "args", "cwd", "env",
            "stopOnEntry", "processId", "host", "port", "preLaunchTask",
            "postDebugTask", "presentation", "order"
        };
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (!knownKeys.contains(it.key())) {
                cfg.adapterConfig[it.key()] = it.value();
            }
        }
        
        return cfg;
    }
};

/**
 * @brief Compound launch configuration
 * 
 * Allows launching multiple debug configurations together
 * (e.g., server + client debugging)
 */
struct CompoundDebugConfiguration {
    QString name;
    QStringList configurations;  // Names of configurations to launch
    bool stopAll = true;         // Stop all when one stops
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        QJsonArray cfgArray;
        for (const QString& cfg : configurations) {
            cfgArray.append(cfg);
        }
        obj["configurations"] = cfgArray;
        obj["stopAll"] = stopAll;
        return obj;
    }
    
    static CompoundDebugConfiguration fromJson(const QJsonObject& obj) {
        CompoundDebugConfiguration cfg;
        cfg.name = obj["name"].toString();
        QJsonArray cfgArray = obj["configurations"].toArray();
        for (const auto& c : cfgArray) {
            cfg.configurations.append(c.toString());
        }
        cfg.stopAll = obj["stopAll"].toBool(true);
        return cfg;
    }
};

/**
 * @brief Manages debug configurations for a project/workspace
 * 
 * Provides functionality similar to VS Code's launch.json:
 * - Store and retrieve debug configurations
 * - Support multiple configurations per project
 * - Compound configurations (launch multiple together)
 * - Variable substitution (${workspaceFolder}, ${file}, etc.)
 */
class DebugConfigurationManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     */
    static DebugConfigurationManager& instance();

    /**
     * @brief Load configurations from a file (launch.json equivalent)
     * @param filePath Path to configuration file
     * @return true if loaded successfully
     */
    bool loadFromFile(const QString& filePath);

    /**
     * @brief Save configurations to a file
     * @param filePath Path to configuration file
     * @return true if saved successfully
     */
    bool saveToFile(const QString& filePath);

    /**
     * @brief Add a configuration
     * @param config Configuration to add
     */
    void addConfiguration(const DebugConfiguration& config);

    /**
     * @brief Remove a configuration by name
     * @param name Configuration name
     */
    void removeConfiguration(const QString& name);

    /**
     * @brief Update an existing configuration
     * @param name Original name
     * @param config Updated configuration
     */
    void updateConfiguration(const QString& name, const DebugConfiguration& config);

    /**
     * @brief Get a configuration by name
     * @param name Configuration name
     * @return Configuration, or empty if not found
     */
    DebugConfiguration configuration(const QString& name) const;

    /**
     * @brief Get all configurations
     */
    QList<DebugConfiguration> allConfigurations() const;

    /**
     * @brief Get configurations for a specific adapter type
     * @param type Adapter type (e.g., "python", "node")
     */
    QList<DebugConfiguration> configurationsForType(const QString& type) const;

    /**
     * @brief Add a compound configuration
     */
    void addCompoundConfiguration(const CompoundDebugConfiguration& config);

    /**
     * @brief Get all compound configurations
     */
    QList<CompoundDebugConfiguration> allCompoundConfigurations() const;

    /**
     * @brief Set the workspace folder for variable substitution
     */
    void setWorkspaceFolder(const QString& folder);

    /**
     * @brief Substitute variables in a configuration
     * 
     * Replaces ${workspaceFolder}, ${file}, ${fileBasename}, etc.
     * 
     * @param config Original configuration
     * @param currentFile Currently active file (for ${file} etc.)
     * @return Configuration with variables substituted
     */
    DebugConfiguration resolveVariables(const DebugConfiguration& config,
                                         const QString& currentFile = {}) const;

    /**
     * @brief Create a quick debug configuration for a file
     * 
     * Auto-generates a configuration based on file type and available adapters.
     * 
     * @param filePath Path to the file to debug
     * @return Generated configuration, or empty if no adapter available
     */
    DebugConfiguration createQuickConfig(const QString& filePath) const;

signals:
    void configurationAdded(const QString& name);
    void configurationRemoved(const QString& name);
    void configurationChanged(const QString& name);
    void configurationsLoaded();

private:
    DebugConfigurationManager();
    ~DebugConfigurationManager() = default;
    DebugConfigurationManager(const DebugConfigurationManager&) = delete;
    DebugConfigurationManager& operator=(const DebugConfigurationManager&) = delete;

    QString substituteVariable(const QString& value, const QString& currentFile) const;

    QMap<QString, DebugConfiguration> m_configurations;
    QMap<QString, CompoundDebugConfiguration> m_compoundConfigurations;
    QString m_workspaceFolder;
    QString m_configFilePath;
};

#endif // DEBUGCONFIGURATION_H
