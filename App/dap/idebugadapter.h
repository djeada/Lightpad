#ifndef IDEBUGADAPTER_H
#define IDEBUGADAPTER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>

/**
 * @brief Debug adapter configuration
 * 
 * Contains all the information needed to start and configure
 * a debug adapter for a specific language/runtime.
 */
struct DebugAdapterConfig {
    QString id;              // Unique identifier (e.g., "python-debugpy", "node-debug")
    QString name;            // Display name (e.g., "Python (debugpy)")
    QString type;            // Adapter type matching DAP type field
    QString program;         // Path to debug adapter executable
    QStringList arguments;   // Command line arguments
    QStringList languages;   // Supported language IDs (e.g., ["python"], ["javascript", "typescript"])
    QStringList extensions;  // Supported file extensions (e.g., [".py"], [".js", ".ts"])
    
    // Default launch/attach configurations
    QJsonObject defaultLaunchConfig;
    QJsonObject defaultAttachConfig;
    
    // Exception breakpoint filters supported by this adapter
    QJsonArray exceptionBreakpointFilters;
    
    bool supportsRestart = false;
    bool supportsTerminate = true;
    bool supportsFunctionBreakpoints = false;
    bool supportsConditionalBreakpoints = true;
    bool supportsHitConditionalBreakpoints = true;
    bool supportsLogPoints = true;
};

/**
 * @brief Interface for debug adapter plugins
 * 
 * Implement this interface to provide debugging support for a specific
 * language or runtime. The debug adapter plugin is responsible for:
 * - Detecting if the adapter is available (e.g., installed)
 * - Providing the adapter configuration
 * - Creating launch configurations for specific files/projects
 * 
 * This is a plugin-like interface to support any language. Built-in
 * adapters and third-party plugins can both implement this.
 */
class IDebugAdapter {
public:
    virtual ~IDebugAdapter() = default;

    /**
     * @brief Get the debug adapter configuration
     * @return Adapter configuration
     */
    virtual DebugAdapterConfig config() const = 0;

    /**
     * @brief Check if the debug adapter is available
     * 
     * This should check if the debug adapter executable exists and
     * is accessible (e.g., debugpy is installed for Python).
     * 
     * @return true if the adapter can be used
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Get a status message about the adapter availability
     * 
     * @return Human-readable status (e.g., "Ready", "Not installed", "Python 3.7+ required")
     */
    virtual QString statusMessage() const = 0;

    /**
     * @brief Create a launch configuration for a specific file
     * 
     * @param filePath Path to the file to debug
     * @param workingDir Working directory for the debug session
     * @return JSON launch configuration, or empty object if not applicable
     */
    virtual QJsonObject createLaunchConfig(const QString& filePath,
                                           const QString& workingDir = {}) const = 0;

    /**
     * @brief Create an attach configuration
     * 
     * @param processId Process ID to attach to, or 0 for remote attach
     * @param host Host for remote attach (empty for local)
     * @param port Port for remote attach
     * @return JSON attach configuration
     */
    virtual QJsonObject createAttachConfig(int processId = 0,
                                           const QString& host = {},
                                           int port = 0) const = 0;

    /**
     * @brief Check if this adapter can debug a specific file
     * 
     * @param filePath Path to the file
     * @return true if this adapter can debug the file
     */
    virtual bool canDebug(const QString& filePath) const {
        const DebugAdapterConfig& cfg = config();
        for (const QString& ext : cfg.extensions) {
            if (filePath.endsWith(ext, Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Check if this adapter supports a specific language
     * 
     * @param languageId Language identifier (e.g., "python", "javascript")
     * @return true if supported
     */
    virtual bool supportsLanguage(const QString& languageId) const {
        return config().languages.contains(languageId, Qt::CaseInsensitive);
    }

    /**
     * @brief Get command to install the debug adapter
     * 
     * @return Installation command or instructions, empty if not applicable
     */
    virtual QString installCommand() const { return {}; }

    /**
     * @brief Get documentation URL for the adapter
     * 
     * @return URL to documentation, or empty string
     */
    virtual QString documentationUrl() const { return {}; }
};

// Declare the interface for Qt's plugin system
#define IDebugAdapter_iid "org.lightpad.IDebugAdapter/1.0"
Q_DECLARE_INTERFACE(IDebugAdapter, IDebugAdapter_iid)

#endif // IDEBUGADAPTER_H
