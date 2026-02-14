#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>

class SettingsManager : public QObject {
  Q_OBJECT

public:
  static const int SETTINGS_VERSION = 1;

  static SettingsManager &instance();

  QString getSettingsDirectory() const;

  QString getSettingsFilePath() const;

  bool loadSettings();

  bool saveSettings();

  QVariant getValue(const QString &key,
                    const QVariant &defaultValue = QVariant()) const;

  void setValue(const QString &key, const QVariant &value);

  bool hasKey(const QString &key) const;

  void resetToDefaults();

  const QJsonObject &getSettingsObject() const;

  bool migrateFromOldPath(const QString &oldPath);

signals:

  void settingChanged(const QString &key, const QVariant &value);

  void settingsLoaded();

  void settingsSaved();

private:
  SettingsManager();
  ~SettingsManager() = default;
  SettingsManager(const SettingsManager &) = delete;
  SettingsManager &operator=(const SettingsManager &) = delete;

  void initializeDefaults();
  void migrateSettings(int fromVersion);
  bool ensureSettingsDirectoryExists();

  QJsonObject m_settings;
  QJsonObject m_defaults;
  bool m_dirty;
};

#endif
