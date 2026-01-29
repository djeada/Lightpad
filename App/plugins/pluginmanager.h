#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QDir>
#include <QPluginLoader>
#include "iplugin.h"
#include "isyntaxplugin.h"

/**
 * @brief Manages plugin lifecycle and provides access to loaded plugins
 * 
 * The PluginManager is responsible for:
 * - Discovering plugins in plugin directories
 * - Loading and unloading plugins
 * - Managing plugin dependencies
 * - Providing access to loaded plugins by type
 */
class PluginManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the PluginManager
     */
    static PluginManager& instance();

    /**
     * @brief Get plugin directories
     * @return List of directories where plugins are searched
     */
    QStringList pluginDirectories() const;

    /**
     * @brief Add a plugin directory
     * @param path Path to add to plugin search paths
     */
    void addPluginDirectory(const QString& path);

    /**
     * @brief Discover all available plugins
     * @return List of discovered plugin file paths
     */
    QStringList discoverPlugins();

    /**
     * @brief Load a plugin from file
     * @param filePath Path to the plugin file
     * @return true if loaded successfully
     */
    bool loadPlugin(const QString& filePath);

    /**
     * @brief Unload a plugin by ID
     * @param pluginId Plugin identifier
     * @return true if unloaded successfully
     */
    bool unloadPlugin(const QString& pluginId);

    /**
     * @brief Load all discovered plugins
     * @return Number of plugins loaded
     */
    int loadAllPlugins();

    /**
     * @brief Unload all plugins
     */
    void unloadAllPlugins();

    /**
     * @brief Get a plugin by ID
     * @param pluginId Plugin identifier
     * @return Pointer to plugin or nullptr
     */
    IPlugin* plugin(const QString& pluginId) const;

    /**
     * @brief Get all loaded plugins
     * @return Map of plugin ID to plugin pointer
     */
    QMap<QString, IPlugin*> allPlugins() const;

    /**
     * @brief Get all syntax plugins
     * @return Map of language ID to syntax plugin
     */
    QMap<QString, ISyntaxPlugin*> syntaxPlugins() const;

    /**
     * @brief Get syntax plugin for a file extension
     * @param extension File extension (without dot)
     * @return Pointer to syntax plugin or nullptr
     */
    ISyntaxPlugin* syntaxPluginForExtension(const QString& extension) const;

    /**
     * @brief Check if a plugin is loaded
     * @param pluginId Plugin identifier
     * @return true if loaded
     */
    bool isLoaded(const QString& pluginId) const;

    /**
     * @brief Get plugin metadata without loading
     * @param filePath Path to plugin file
     * @return Plugin metadata or empty struct on failure
     */
    PluginMetadata getPluginMetadata(const QString& filePath) const;

signals:
    /**
     * @brief Emitted when a plugin is loaded
     * @param pluginId The loaded plugin's ID
     */
    void pluginLoaded(const QString& pluginId);

    /**
     * @brief Emitted when a plugin is unloaded
     * @param pluginId The unloaded plugin's ID
     */
    void pluginUnloaded(const QString& pluginId);

    /**
     * @brief Emitted when plugin loading fails
     * @param filePath Path to the plugin
     * @param error Error message
     */
    void pluginLoadError(const QString& filePath, const QString& error);

private:
    PluginManager();
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    QStringList m_pluginDirs;
    QMap<QString, QPluginLoader*> m_loaders;
    QMap<QString, IPlugin*> m_plugins;
    QMap<QString, ISyntaxPlugin*> m_syntaxPlugins;
};

#endif // PLUGINMANAGER_H
