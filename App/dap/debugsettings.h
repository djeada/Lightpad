#ifndef DEBUGSETTINGS_H
#define DEBUGSETTINGS_H

#include <QDir>
#include <QJsonObject>
#include <QObject>
#include <QString>

/**
 * @brief Debugger settings exposed through JSON files in .lightpad directory
 *
 * All debugger configuration is stored in user-editable JSON files:
 *
 * .lightpad/
 * ├── debug/
 * │   ├── launch.json          - Debug launch configurations
 * │   ├── breakpoints.json     - Source, function, data breakpoints
 * │   ├── watches.json         - Watch expressions
 * │   ├── adapters.json        - Debug adapter settings and overrides
 * │   └── settings.json        - General debugger preferences
 *
 * Users can edit these files directly to configure the debugger.
 * The application will monitor for changes and reload automatically.
 */
class DebugSettings : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static DebugSettings &instance();

  /**
   * @brief Initialize the debug settings directory
   *
   * Creates .lightpad/debug/ directory structure if it doesn't exist
   * and generates default configuration files.
   *
   * @param workspaceFolder Root folder of the workspace/project
   */
  void initialize(const QString &workspaceFolder);

  /**
   * @brief Get the workspace folder
   */
  QString workspaceFolder() const;

  /**
   * @brief Get the debug settings directory (.lightpad/debug/)
   */
  QString debugSettingsDir() const;

  /**
   * @brief Get path to a specific config file
   */
  QString configFilePath(const QString &fileName) const;

  // File paths for different config types
  QString launchConfigPath() const;
  QString breakpointsConfigPath() const;
  QString watchesConfigPath() const;
  QString adaptersConfigPath() const;
  QString settingsConfigPath() const;

  /**
   * @brief Load all debug settings from JSON files
   */
  void loadAll();

  /**
   * @brief Save all debug settings to JSON files
   */
  void saveAll();

  /**
   * @brief Reload settings from disk
   */
  void reload();

  // Settings accessors

  /**
   * @brief Get general debug settings
   */
  QJsonObject generalSettings() const;

  /**
   * @brief Set a general debug setting
   */
  void setGeneralSetting(const QString &key, const QJsonValue &value);

  /**
   * @brief Get adapter settings/overrides
   */
  QJsonObject adapterSettings() const;

  /**
   * @brief Set adapter-specific settings
   */
  void setAdapterSettings(const QString &adapterId,
                          const QJsonObject &settings);

  /**
   * @brief Get the default adapter ID for a file extension
   */
  QString defaultAdapterForExtension(const QString &extension) const;

  /**
   * @brief Set the default adapter for a file extension
   */
  void setDefaultAdapterForExtension(const QString &extension,
                                     const QString &adapterId);

  // Settings keys
  static const QString KEY_STOP_ON_ENTRY;
  static const QString KEY_EXTERNAL_CONSOLE;
  static const QString KEY_SOURCE_MAP_PATH_OVERRIDES;
  static const QString KEY_JUST_MY_CODE;
  static const QString KEY_SHOW_RETURN_VALUE;
  static const QString KEY_AUTO_RELOAD;
  static const QString KEY_LOG_TO_FILE;
  static const QString KEY_LOG_FILE_PATH;

signals:
  /**
   * @brief Emitted when any settings file changes
   */
  void settingsChanged();

  /**
   * @brief Emitted when launch configurations change
   */
  void launchConfigChanged();

  /**
   * @brief Emitted when breakpoints change
   */
  void breakpointsChanged();

  /**
   * @brief Emitted when watches change
   */
  void watchesChanged();

  /**
   * @brief Emitted when adapter settings change
   */
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

#endif // DEBUGSETTINGS_H
