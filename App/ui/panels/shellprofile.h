#ifndef SHELLPROFILE_H
#define SHELLPROFILE_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Represents a shell profile configuration
 *
 * A shell profile contains the shell executable path, arguments,
 * environment variables, and display name.
 */
struct ShellProfile {
  QString name;                       ///< Display name for the profile
  QString command;                    ///< Shell executable path
  QStringList arguments;              ///< Command-line arguments for the shell
  QMap<QString, QString> environment; ///< Additional environment variables
  QString icon;                       ///< Optional icon name/path

  ShellProfile() = default;

  ShellProfile(const QString &profileName, const QString &cmd,
               const QStringList &args = QStringList())
      : name(profileName), command(cmd), arguments(args) {}

  bool isValid() const { return !command.isEmpty(); }
};

/**
 * @brief Manager for shell profiles
 *
 * Provides access to available shell profiles on the system
 * and allows adding custom profiles.
 */
class ShellProfileManager {
public:
  static ShellProfileManager &instance();

  /**
   * @brief Get all available shell profiles
   * @return Vector of shell profiles
   */
  QVector<ShellProfile> availableProfiles() const;

  /**
   * @brief Get the default shell profile
   * @return Default shell profile based on system
   */
  ShellProfile defaultProfile() const;

  /**
   * @brief Get a profile by name
   * @param name Profile name
   * @return Shell profile, or invalid profile if not found
   */
  ShellProfile profileByName(const QString &name) const;

  /**
   * @brief Add a custom shell profile
   * @param profile Shell profile to add
   */
  void addProfile(const ShellProfile &profile);

  /**
   * @brief Remove a profile by name
   * @param name Profile name to remove
   * @return true if profile was removed
   */
  bool removeProfile(const QString &name);

  /**
   * @brief Detect available shells on the system
   *
   * Scans the system for available shells and adds them as profiles.
   */
  void detectSystemShells();

private:
  ShellProfileManager();
  ~ShellProfileManager() = default;

  // Disable copy
  ShellProfileManager(const ShellProfileManager &) = delete;
  ShellProfileManager &operator=(const ShellProfileManager &) = delete;

  QVector<ShellProfile> m_profiles;
  QString m_defaultProfileName;
};

#endif // SHELLPROFILE_H
