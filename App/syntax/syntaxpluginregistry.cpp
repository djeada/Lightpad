#include "syntaxpluginregistry.h"
#include "../core/logging/logger.h"

SyntaxPluginRegistry &SyntaxPluginRegistry::instance() {
  static SyntaxPluginRegistry instance;
  return instance;
}

void SyntaxPluginRegistry::registerPlugin(
    std::unique_ptr<ISyntaxPlugin> plugin) {
  if (!plugin) {
    Logger::instance().warning("Attempted to register null syntax plugin");
    return;
  }

  QString langId = plugin->languageId();
  if (langId.isEmpty()) {
    Logger::instance().warning(
        "Attempted to register syntax plugin with empty language ID");
    return;
  }

  if (languagePlugins.find(langId) != languagePlugins.end()) {
    Logger::instance().warning(
        QString("Syntax plugin for language '%1' already registered, replacing")
            .arg(langId));
  }

  QStringList extensions = plugin->fileExtensions();
  for (const QString &ext : extensions) {
    if (!ext.isEmpty()) {
      extensionToLanguage[ext.toLower()] = langId;
    }
  }

  languagePlugins[langId] = std::move(plugin);

  Logger::instance().info(
      QString("Registered syntax plugin for language '%1' with %2 extension(s)")
          .arg(langId)
          .arg(extensions.size()));
}

ISyntaxPlugin *
SyntaxPluginRegistry::getPluginByLanguageId(const QString &languageId) const {
  auto it = languagePlugins.find(languageId);
  if (it != languagePlugins.end()) {
    return it->second.get();
  }
  return nullptr;
}

ISyntaxPlugin *
SyntaxPluginRegistry::getPluginByExtension(const QString &extension) const {
  QString ext = extension.toLower();

  if (ext.startsWith('.')) {
    ext = ext.mid(1);
  }

  auto it = extensionToLanguage.find(ext);
  if (it != extensionToLanguage.end()) {
    return getPluginByLanguageId(it.value());
  }
  return nullptr;
}

QStringList SyntaxPluginRegistry::getAllLanguageIds() const {
  QStringList ids;
  for (const auto &pair : languagePlugins) {
    ids.append(pair.first);
  }
  return ids;
}

QStringList SyntaxPluginRegistry::getAllExtensions() const {
  return extensionToLanguage.keys();
}

bool SyntaxPluginRegistry::isLanguageSupported(
    const QString &languageId) const {
  return languagePlugins.find(languageId) != languagePlugins.end();
}

bool SyntaxPluginRegistry::isExtensionSupported(
    const QString &extension) const {
  QString ext = extension.toLower();
  if (ext.startsWith('.')) {
    ext = ext.mid(1);
  }
  return extensionToLanguage.contains(ext);
}

void SyntaxPluginRegistry::clear() {
  languagePlugins.clear();
  extensionToLanguage.clear();
  Logger::instance().info("Cleared all syntax plugins from registry");
}
