#ifndef SYNTAXPLUGINREGISTRY_H
#define SYNTAXPLUGINREGISTRY_H

#include "../plugins/isyntaxplugin.h"
#include <QMap>
#include <QString>
#include <QStringList>
#include <map>
#include <memory>

class SyntaxPluginRegistry {
public:
  static SyntaxPluginRegistry &instance();

  void registerPlugin(std::unique_ptr<ISyntaxPlugin> plugin);

  ISyntaxPlugin *getPluginByLanguageId(const QString &languageId) const;

  ISyntaxPlugin *getPluginByExtension(const QString &extension) const;

  QStringList getAllLanguageIds() const;

  QStringList getAllExtensions() const;

  bool isLanguageSupported(const QString &languageId) const;

  bool isExtensionSupported(const QString &extension) const;

  void clear();

private:
  SyntaxPluginRegistry() = default;
  ~SyntaxPluginRegistry() = default;
  SyntaxPluginRegistry(const SyntaxPluginRegistry &) = delete;
  SyntaxPluginRegistry &operator=(const SyntaxPluginRegistry &) = delete;

  std::map<QString, std::unique_ptr<ISyntaxPlugin>> languagePlugins;

  QMap<QString, QString> extensionToLanguage;
};

#endif
