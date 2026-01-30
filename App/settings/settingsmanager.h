#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QString>
#include <QObject>
#include <QVariant>
#include <QJsonObject>

/**
 * @brief Manages application settings with OS-specific storage
 * 
 * Provides centralized settings management with:
 * - OS-specific config directories (XDG_CONFIG_HOME on Linux, AppData on Windows)
 * - Settings validation and fallback to defaults
 * - Version-based settings migration
 */
class SettingsManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Current settings schema version for migration
     */
    static const int SETTINGS_VERSION = 1;

    /**
     * @brief Get the singleton instance
     * @return Reference to the SettingsManager instance
     */
    static SettingsManager& instance();

    /**
     * @brief Get the OS-specific settings directory
     * @return Path to the settings directory
     */
    QString getSettingsDirectory() const;

    /**
     * @brief Get the full path to the settings file
     * @return Path to settings.json
     */
    QString getSettingsFilePath() const;

    /**
     * @brief Load settings from the config file
     * @return true if settings were loaded successfully
     */
    bool loadSettings();

    /**
     * @brief Save settings to the config file
     * @return true if settings were saved successfully
     */
    bool saveSettings();

    /**
     * @brief Get a setting value
     * @param key Setting key (supports dot notation for nested keys)
     * @param defaultValue Default value if key doesn't exist
     * @return The setting value or default
     */
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief Set a setting value
     * @param key Setting key (top-level keys only, nested keys not yet supported)
     * @param value Value to set
     */
    void setValue(const QString& key, const QVariant& value);

    /**
     * @brief Check if a setting key exists
     * @param key Setting key
     * @return true if the key exists
     */
    bool hasKey(const QString& key) const;

    /**
     * @brief Reset all settings to defaults
     */
    void resetToDefaults();

    /**
     * @brief Get the entire settings object
     * @return Reference to the settings JSON object
     */
    const QJsonObject& getSettingsObject() const;

    /**
     * @brief Migrate settings from old location to new location
     * @param oldPath Path to old settings file
     * @return true if migration was successful
     */
    bool migrateFromOldPath(const QString& oldPath);

signals:
    /**
     * @brief Emitted when a setting value changes
     * @param key The changed key
     * @param value The new value
     */
    void settingChanged(const QString& key, const QVariant& value);

    /**
     * @brief Emitted when settings are loaded
     */
    void settingsLoaded();

    /**
     * @brief Emitted when settings are saved
     */
    void settingsSaved();

private:
    SettingsManager();
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    void initializeDefaults();
    void migrateSettings(int fromVersion);
    bool ensureSettingsDirectoryExists();

    QJsonObject m_settings;
    QJsonObject m_defaults;
    bool m_dirty;
};

#endif // SETTINGSMANAGER_H
