#ifndef SYNTAXPLUGINREGISTRY_H
#define SYNTAXPLUGINREGISTRY_H

#include "../plugins/isyntaxplugin.h"
#include <QMap>
#include <QString>
#include <QStringList>
#include <map>
#include <memory>

/**
 * @brief Registry for syntax highlighting plugins
 * 
 * Manages the registration and retrieval of syntax plugins.
 * Provides mapping between file extensions and language plugins.
 */
class SyntaxPluginRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static SyntaxPluginRegistry& instance();

    /**
     * @brief Register a syntax plugin
     * @param plugin The syntax plugin to register (takes ownership)
     */
    void registerPlugin(std::unique_ptr<ISyntaxPlugin> plugin);

    /**
     * @brief Get a plugin by language ID
     * @param languageId The language identifier
     * @return Pointer to plugin or nullptr if not found
     */
    ISyntaxPlugin* getPluginByLanguageId(const QString& languageId) const;

    /**
     * @brief Get a plugin by file extension
     * @param extension File extension (without dot)
     * @return Pointer to plugin or nullptr if not found
     */
    ISyntaxPlugin* getPluginByExtension(const QString& extension) const;

    /**
     * @brief Get all registered language IDs
     */
    QStringList getAllLanguageIds() const;

    /**
     * @brief Get all supported file extensions
     */
    QStringList getAllExtensions() const;

    /**
     * @brief Check if a language is supported
     */
    bool isLanguageSupported(const QString& languageId) const;

    /**
     * @brief Check if a file extension is supported
     */
    bool isExtensionSupported(const QString& extension) const;

    /**
     * @brief Clear all registered plugins
     */
    void clear();

private:
    SyntaxPluginRegistry() = default;
    ~SyntaxPluginRegistry() = default;
    SyntaxPluginRegistry(const SyntaxPluginRegistry&) = delete;
    SyntaxPluginRegistry& operator=(const SyntaxPluginRegistry&) = delete;

    // Use std::map instead of QMap for unique_ptr compatibility
    std::map<QString, std::unique_ptr<ISyntaxPlugin>> languagePlugins;
    
    // Map from file extension to language ID
    QMap<QString, QString> extensionToLanguage;
};

#endif // SYNTAXPLUGINREGISTRY_H
