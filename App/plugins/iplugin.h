#ifndef IPLUGIN_H
#define IPLUGIN_H

#include <QString>
#include <QObject>
#include <QJsonObject>

/**
 * @brief Plugin metadata structure
 */
struct PluginMetadata {
    QString id;           // Unique identifier
    QString name;         // Display name
    QString version;      // Version string (semver)
    QString author;       // Author name
    QString description;  // Brief description
    QString category;     // Plugin category (syntax, theme, tool, etc.)
    QStringList dependencies; // List of required plugin IDs
};

/**
 * @brief Base interface for all Lightpad plugins
 * 
 * All plugins must implement this interface. The plugin system
 * uses Qt's plugin mechanism for loading/unloading plugins.
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /**
     * @brief Get plugin metadata
     * @return Plugin metadata structure
     */
    virtual PluginMetadata metadata() const = 0;

    /**
     * @brief Initialize the plugin
     * @return true if initialization succeeded
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the plugin
     * Called before the plugin is unloaded
     */
    virtual void shutdown() = 0;

    /**
     * @brief Check if the plugin is loaded and ready
     * @return true if plugin is active
     */
    virtual bool isLoaded() const = 0;

    /**
     * @brief Get plugin settings
     * @return JSON object containing plugin settings
     */
    virtual QJsonObject settings() const { return QJsonObject(); }

    /**
     * @brief Update plugin settings
     * @param settings New settings to apply
     */
    virtual void setSettings(const QJsonObject& settings) { Q_UNUSED(settings); }
};

// Declare the interface for Qt's plugin system
#define IPlugin_iid "org.lightpad.IPlugin/1.0"
Q_DECLARE_INTERFACE(IPlugin, IPlugin_iid)

#endif // IPLUGIN_H
