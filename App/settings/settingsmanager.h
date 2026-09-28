#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QFont>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>

class SettingsManager : public QObject {
  Q_OBJECT

public:
  static const int SETTINGS_VERSION = 1;

  static SettingsManager &instance();

  static int normalizeFontWeight(int weight) {
    if (weight <= 0)
      return QFont::Normal;
    if (weight >= 100)
      return qMin(weight, 1000);
    static const int qt5Weights[] = {0, 12, 25, 50, 57, 63, 75, 81, 87};
    static const int qt6Weights[] = {100, 200, 300, 400, 500,
                                     600, 700, 800, 900};
    int best = 0;
    for (int i = 1; i < 9; ++i) {
      if (qAbs(qt5Weights[i] - weight) < qAbs(qt5Weights[best] - weight))
        best = i;
    }
    return qt6Weights[best];
  }

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

  bool m_loaded = false;
};

#endif
