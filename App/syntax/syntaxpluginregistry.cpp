#include "syntaxpluginregistry.h"
#include "../core/logging/logger.h"

SyntaxPluginRegistry& SyntaxPluginRegistry::instance()
{
    static SyntaxPluginRegistry instance;
    return instance;
}

void SyntaxPluginRegistry::registerPlugin(std::unique_ptr<ISyntaxPlugin> plugin)
{
    if (!plugin) {
        Logger::instance().logWarning("Attempted to register null syntax plugin");
        return;
    }

    QString langId = plugin->languageId();
    if (langId.isEmpty()) {
        Logger::instance().logWarning("Attempted to register syntax plugin with empty language ID");
        return;
    }

    // Check if already registered
    if (languagePlugins.contains(langId)) {
        Logger::instance().logWarning(QString("Syntax plugin for language '%1' already registered, replacing").arg(langId));
    }

    // Register extension mappings
    QStringList extensions = plugin->fileExtensions();
    for (const QString& ext : extensions) {
        if (!ext.isEmpty()) {
            extensionToLanguage[ext.toLower()] = langId;
        }
    }

    // Store the plugin
    languagePlugins[langId] = std::move(plugin);
    
    Logger::instance().logInfo(QString("Registered syntax plugin for language '%1' with %2 extension(s)")
        .arg(langId).arg(extensions.size()));
}

ISyntaxPlugin* SyntaxPluginRegistry::getPluginByLanguageId(const QString& languageId) const
{
    auto it = languagePlugins.find(languageId);
    if (it != languagePlugins.end()) {
        return it.value().get();
    }
    return nullptr;
}

ISyntaxPlugin* SyntaxPluginRegistry::getPluginByExtension(const QString& extension) const
{
    QString ext = extension.toLower();
    
    // Remove leading dot if present
    if (ext.startsWith('.')) {
        ext = ext.mid(1);
    }

    auto it = extensionToLanguage.find(ext);
    if (it != extensionToLanguage.end()) {
        return getPluginByLanguageId(it.value());
    }
    return nullptr;
}

QStringList SyntaxPluginRegistry::getAllLanguageIds() const
{
    return languagePlugins.keys();
}

QStringList SyntaxPluginRegistry::getAllExtensions() const
{
    return extensionToLanguage.keys();
}

bool SyntaxPluginRegistry::isLanguageSupported(const QString& languageId) const
{
    return languagePlugins.contains(languageId);
}

bool SyntaxPluginRegistry::isExtensionSupported(const QString& extension) const
{
    QString ext = extension.toLower();
    if (ext.startsWith('.')) {
        ext = ext.mid(1);
    }
    return extensionToLanguage.contains(ext);
}

void SyntaxPluginRegistry::clear()
{
    languagePlugins.clear();
    extensionToLanguage.clear();
    Logger::instance().logInfo("Cleared all syntax plugins from registry");
}
