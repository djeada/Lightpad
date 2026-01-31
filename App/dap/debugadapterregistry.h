#ifndef DEBUGADAPTERREGISTRY_H
#define DEBUGADAPTERREGISTRY_H

#include <QObject>
#include <QMap>
#include <QList>
#include <memory>

#include "idebugadapter.h"

/**
 * @brief Registry for debug adapters
 * 
 * Manages registration and lookup of debug adapters. This allows
 * the IDE to support multiple debuggers for different languages
 * in a plugin-like manner.
 * 
 * Built-in adapters for common languages (Python, Node.js, C++) are
 * registered by default. Additional adapters can be registered
 * dynamically through plugins.
 */
class DebugAdapterRegistry : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the registry
     */
    static DebugAdapterRegistry& instance();

    /**
     * @brief Register a debug adapter
     * 
     * @param adapter Shared pointer to the adapter (registry takes ownership)
     */
    void registerAdapter(std::shared_ptr<IDebugAdapter> adapter);

    /**
     * @brief Unregister a debug adapter by ID
     * 
     * @param adapterId Adapter identifier
     */
    void unregisterAdapter(const QString& adapterId);

    /**
     * @brief Get all registered adapters
     * 
     * @return List of all adapters
     */
    QList<std::shared_ptr<IDebugAdapter>> allAdapters() const;

    /**
     * @brief Get all available (installed) adapters
     * 
     * @return List of adapters that are ready to use
     */
    QList<std::shared_ptr<IDebugAdapter>> availableAdapters() const;

    /**
     * @brief Find an adapter by ID
     * 
     * @param adapterId Adapter identifier
     * @return Adapter or nullptr if not found
     */
    std::shared_ptr<IDebugAdapter> adapter(const QString& adapterId) const;

    /**
     * @brief Find adapters that can debug a specific file
     * 
     * @param filePath Path to the file
     * @return List of compatible adapters (may be empty)
     */
    QList<std::shared_ptr<IDebugAdapter>> adaptersForFile(const QString& filePath) const;

    /**
     * @brief Find adapters for a specific language
     * 
     * @param languageId Language identifier
     * @return List of compatible adapters
     */
    QList<std::shared_ptr<IDebugAdapter>> adaptersForLanguage(const QString& languageId) const;

    /**
     * @brief Get the preferred adapter for a file
     * 
     * Returns the first available adapter that can handle the file.
     * 
     * @param filePath Path to the file
     * @return Best adapter or nullptr if none available
     */
    std::shared_ptr<IDebugAdapter> preferredAdapterForFile(const QString& filePath) const;

    /**
     * @brief Refresh availability status of all adapters
     * 
     * Call this after installing/uninstalling debug tools.
     */
    void refreshAvailability();

signals:
    /**
     * @brief Emitted when an adapter is registered
     */
    void adapterRegistered(const QString& adapterId);

    /**
     * @brief Emitted when an adapter is unregistered
     */
    void adapterUnregistered(const QString& adapterId);

    /**
     * @brief Emitted when adapter availability changes
     */
    void availabilityChanged();

private:
    DebugAdapterRegistry();
    ~DebugAdapterRegistry() = default;
    DebugAdapterRegistry(const DebugAdapterRegistry&) = delete;
    DebugAdapterRegistry& operator=(const DebugAdapterRegistry&) = delete;

    void registerBuiltinAdapters();

    QMap<QString, std::shared_ptr<IDebugAdapter>> m_adapters;
};

#endif // DEBUGADAPTERREGISTRY_H
