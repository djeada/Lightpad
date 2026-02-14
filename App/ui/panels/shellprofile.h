#ifndef SHELLPROFILE_H
#define SHELLPROFILE_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct ShellProfile {
  QString name;
  QString command;
  QStringList arguments;
  QMap<QString, QString> environment;
  QString icon;

  ShellProfile() = default;

  ShellProfile(const QString &profileName, const QString &cmd,
               const QStringList &args = QStringList())
      : name(profileName), command(cmd), arguments(args) {}

  bool isValid() const { return !command.isEmpty(); }
};

class ShellProfileManager {
public:
  static ShellProfileManager &instance();

  QVector<ShellProfile> availableProfiles() const;

  ShellProfile defaultProfile() const;

  ShellProfile profileByName(const QString &name) const;

  void addProfile(const ShellProfile &profile);

  bool removeProfile(const QString &name);

  void detectSystemShells();

private:
  ShellProfileManager();
  ~ShellProfileManager() = default;

  ShellProfileManager(const ShellProfileManager &) = delete;
  ShellProfileManager &operator=(const ShellProfileManager &) = delete;

  QVector<ShellProfile> m_profiles;
  QString m_defaultProfileName;
};

#endif
