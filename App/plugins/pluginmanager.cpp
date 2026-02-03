#include "pluginmanager.h"
#include "../core/logging/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QStandardPaths>

PluginManager &PluginManager::instance() {
  static PluginManager instance;
  return instance;
}

PluginManager::PluginManager() : QObject(nullptr) {
  // Add default plugin directories
  QString appDir = QCoreApplication::applicationDirPath();
  m_pluginDirs << appDir + "/plugins";

  // Add user plugin directory
  QString userPlugins =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/plugins";
  m_pluginDirs << userPlugins;

  // Add system plugin directory on Linux
#ifndef Q_OS_WIN
  m_pluginDirs << "/usr/lib/lightpad/plugins";
  m_pluginDirs << "/usr/local/lib/lightpad/plugins";
#endif
}

PluginManager::~PluginManager() { unloadAllPlugins(); }

QStringList PluginManager::pluginDirectories() const { return m_pluginDirs; }

void PluginManager::addPluginDirectory(const QString &path) {
  if (!m_pluginDirs.contains(path)) {
    m_pluginDirs << path;
    LOG_INFO(QString("Added plugin directory: %1").arg(path));
  }
}

QStringList PluginManager::discoverPlugins() {
  QStringList plugins;

  for (const QString &dirPath : m_pluginDirs) {
    QDir dir(dirPath);
    if (!dir.exists()) {
      continue;
    }

    // Look for shared library files
#ifdef Q_OS_WIN
    QStringList filters = {"*.dll"};
#elif defined(Q_OS_MAC)
    QStringList filters = {"*.dylib", "*.bundle"};
#else
    QStringList filters = {"*.so"};
#endif

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &file : files) {
      plugins << file.absoluteFilePath();
    }
  }

  LOG_INFO(QString("Discovered %1 plugin(s)").arg(plugins.size()));
  return plugins;
}

bool PluginManager::loadPlugin(const QString &filePath) {
  if (!QFileInfo(filePath).exists()) {
    QString error = QString("Plugin file does not exist: %1").arg(filePath);
    LOG_ERROR(error);
    emit pluginLoadError(filePath, error);
    return false;
  }

  QPluginLoader *loader = new QPluginLoader(filePath, this);

  if (!loader->load()) {
    QString error = loader->errorString();
    LOG_ERROR(QString("Failed to load plugin %1: %2").arg(filePath).arg(error));
    emit pluginLoadError(filePath, error);
    delete loader;
    return false;
  }

  QObject *pluginObject = loader->instance();
  if (!pluginObject) {
    QString error = "Failed to get plugin instance";
    LOG_ERROR(QString("Failed to get instance for plugin %1").arg(filePath));
    emit pluginLoadError(filePath, error);
    loader->unload();
    delete loader;
    return false;
  }

  IPlugin *plugin = qobject_cast<IPlugin *>(pluginObject);
  if (!plugin) {
    QString error = "Object does not implement IPlugin interface";
    LOG_ERROR(QString("Plugin %1 does not implement IPlugin").arg(filePath));
    emit pluginLoadError(filePath, error);
    loader->unload();
    delete loader;
    return false;
  }

  PluginMetadata meta = plugin->metadata();

  // Check if already loaded
  if (m_plugins.contains(meta.id)) {
    LOG_WARNING(QString("Plugin %1 is already loaded").arg(meta.id));
    loader->unload();
    delete loader;
    return false;
  }

  // Initialize the plugin
  if (!plugin->initialize()) {
    QString error = "Plugin initialization failed";
    LOG_ERROR(QString("Failed to initialize plugin %1").arg(meta.id));
    emit pluginLoadError(filePath, error);
    loader->unload();
    delete loader;
    return false;
  }

  // Store the plugin
  m_loaders[meta.id] = loader;
  m_plugins[meta.id] = plugin;

  // Check if it's a syntax plugin
  ISyntaxPlugin *syntaxPlugin = qobject_cast<ISyntaxPlugin *>(pluginObject);
  if (syntaxPlugin) {
    m_syntaxPlugins[syntaxPlugin->languageId()] = syntaxPlugin;
    LOG_INFO(QString("Loaded syntax plugin for language: %1")
                 .arg(syntaxPlugin->languageId()));
  }

  LOG_INFO(QString("Loaded plugin: %1 v%2").arg(meta.name).arg(meta.version));
  emit pluginLoaded(meta.id);

  return true;
}

bool PluginManager::unloadPlugin(const QString &pluginId) {
  if (!m_plugins.contains(pluginId)) {
    LOG_WARNING(QString("Plugin %1 is not loaded").arg(pluginId));
    return false;
  }

  IPlugin *plugin = m_plugins[pluginId];
  plugin->shutdown();

  // Remove from syntax plugins if applicable
  ISyntaxPlugin *syntaxPlugin = dynamic_cast<ISyntaxPlugin *>(plugin);
  if (syntaxPlugin) {
    m_syntaxPlugins.remove(syntaxPlugin->languageId());
  }

  // Unload and clean up
  QPluginLoader *loader = m_loaders[pluginId];
  loader->unload();
  delete loader;

  m_plugins.remove(pluginId);
  m_loaders.remove(pluginId);

  LOG_INFO(QString("Unloaded plugin: %1").arg(pluginId));
  emit pluginUnloaded(pluginId);

  return true;
}

int PluginManager::loadAllPlugins() {
  QStringList plugins = discoverPlugins();
  int loaded = 0;

  for (const QString &plugin : plugins) {
    if (loadPlugin(plugin)) {
      loaded++;
    }
  }

  LOG_INFO(QString("Loaded %1 of %2 discovered plugins")
               .arg(loaded)
               .arg(plugins.size()));
  return loaded;
}

void PluginManager::unloadAllPlugins() {
  QStringList pluginIds = m_plugins.keys();
  for (const QString &id : pluginIds) {
    unloadPlugin(id);
  }
}

IPlugin *PluginManager::plugin(const QString &pluginId) const {
  return m_plugins.value(pluginId, nullptr);
}

QMap<QString, IPlugin *> PluginManager::allPlugins() const { return m_plugins; }

QMap<QString, ISyntaxPlugin *> PluginManager::syntaxPlugins() const {
  return m_syntaxPlugins;
}

ISyntaxPlugin *
PluginManager::syntaxPluginForExtension(const QString &extension) const {
  for (ISyntaxPlugin *plugin : m_syntaxPlugins) {
    if (plugin->fileExtensions().contains(extension, Qt::CaseInsensitive)) {
      return plugin;
    }
  }
  return nullptr;
}

bool PluginManager::isLoaded(const QString &pluginId) const {
  return m_plugins.contains(pluginId);
}

PluginMetadata PluginManager::getPluginMetadata(const QString &filePath) const {
  PluginMetadata metadata;

  QPluginLoader loader(filePath);
  QJsonObject metaData = loader.metaData().value("MetaData").toObject();

  metadata.id = metaData.value("id").toString();
  metadata.name = metaData.value("name").toString();
  metadata.version = metaData.value("version").toString();
  metadata.author = metaData.value("author").toString();
  metadata.description = metaData.value("description").toString();
  metadata.category = metaData.value("category").toString();

  QJsonArray deps = metaData.value("dependencies").toArray();
  for (const QJsonValue &dep : deps) {
    metadata.dependencies << dep.toString();
  }

  return metadata;
}
