#ifndef DEBUGSETTINGS_H
#define DEBUGSETTINGS_H

#include <QDir>
#include <QJsonObject>
#include <QObject>
#include <QString>

class DebugSettings : public QObject {
  Q_OBJECT

public:
  static DebugSettings &instance();

  void initialize(const QString &workspaceFolder);

  QString workspaceFolder() const;

  QString debugSettingsDir() const;

  QString configFilePath(const QString &fileName) const;

  QString launchConfigPath() const;
  QString breakpointsConfigPath() const;
  QString watchesConfigPath() const;
  QString adaptersConfigPath() const;
  QString settingsConfigPath() const;

  void loadAll();

  void saveAll();

  void reload();

  QJsonObject generalSettings() const;

  void setGeneralSetting(const QString &key, const QJsonValue &value);

  QJsonObject adapterSettings() const;

  void setAdapterSettings(const QString &adapterId,
                          const QJsonObject &settings);

  QString defaultAdapterForExtension(const QString &extension) const;

  void setDefaultAdapterForExtension(const QString &extension,
                                     const QString &adapterId);

  static const QString KEY_STOP_ON_ENTRY;
  static const QString KEY_EXTERNAL_CONSOLE;
  static const QString KEY_SOURCE_MAP_PATH_OVERRIDES;
  static const QString KEY_JUST_MY_CODE;
  static const QString KEY_SHOW_RETURN_VALUE;
  static const QString KEY_AUTO_RELOAD;
  static const QString KEY_LOG_TO_FILE;
  static const QString KEY_LOG_FILE_PATH;

signals:

  void settingsChanged();

  void launchConfigChanged();

  void breakpointsChanged();

  void watchesChanged();

  void adapterSettingsChanged();

private:
  DebugSettings();
  ~DebugSettings() = default;
  DebugSettings(const DebugSettings &) = delete;
  DebugSettings &operator=(const DebugSettings &) = delete;

  void ensureDirectoryExists();
  void createDefaultLaunchConfig();
  void createDefaultBreakpointsConfig();
  void createDefaultWatchesConfig();
  void createDefaultAdaptersConfig();
  void createDefaultSettingsConfig();

  bool writeJsonFile(const QString &path, const QJsonObject &content);
  QJsonObject readJsonFile(const QString &path);

  QString m_workspaceFolder;
  QJsonObject m_generalSettings;
  QJsonObject m_adapterSettings;
};

#endif
