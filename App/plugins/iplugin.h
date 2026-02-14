#ifndef IPLUGIN_H
#define IPLUGIN_H

#include <QJsonObject>
#include <QObject>
#include <QString>

struct PluginMetadata {
  QString id;
  QString name;
  QString version;
  QString author;
  QString description;
  QString category;
  QStringList dependencies;
};

class IPlugin {
public:
  virtual ~IPlugin() = default;

  virtual PluginMetadata metadata() const = 0;

  virtual bool initialize() = 0;

  virtual void shutdown() = 0;

  virtual bool isLoaded() const = 0;

  virtual QJsonObject settings() const { return QJsonObject(); }

  virtual void setSettings(const QJsonObject &settings) { Q_UNUSED(settings); }
};

#define IPlugin_iid "org.lightpad.IPlugin/1.0"
Q_DECLARE_INTERFACE(IPlugin, IPlugin_iid)

#endif
