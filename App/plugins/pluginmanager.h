#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "iplugin.h"
#include "isyntaxplugin.h"
#include <QDir>
#include <QMap>
#include <QObject>
#include <QPluginLoader>
#include <QString>

class PluginManager : public QObject {
  Q_OBJECT

public:
  static PluginManager &instance();

  QStringList pluginDirectories() const;

  void addPluginDirectory(const QString &path);

  QStringList discoverPlugins();

  bool loadPlugin(const QString &filePath);

  bool unloadPlugin(const QString &pluginId);

  int loadAllPlugins();

  void unloadAllPlugins();

  IPlugin *plugin(const QString &pluginId) const;

  QMap<QString, IPlugin *> allPlugins() const;

  QMap<QString, ISyntaxPlugin *> syntaxPlugins() const;

  ISyntaxPlugin *syntaxPluginForExtension(const QString &extension) const;

  bool isLoaded(const QString &pluginId) const;

  PluginMetadata getPluginMetadata(const QString &filePath) const;

signals:

  void pluginLoaded(const QString &pluginId);

  void pluginUnloaded(const QString &pluginId);

  void pluginLoadError(const QString &filePath, const QString &error);

private:
  PluginManager();
  ~PluginManager();
  PluginManager(const PluginManager &) = delete;
  PluginManager &operator=(const PluginManager &) = delete;

  QStringList m_pluginDirs;
  QMap<QString, QPluginLoader *> m_loaders;
  QMap<QString, IPlugin *> m_plugins;
  QMap<QString, ISyntaxPlugin *> m_syntaxPlugins;
};

#endif
