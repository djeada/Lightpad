#ifndef THEMEENGINE_H
#define THEMEENGINE_H

#include "themedefinition.h"
#include "../settings/theme.h"
#include <QObject>
#include <QMap>
#include <QStringList>

class ThemeEngine : public QObject {
  Q_OBJECT
public:
  static ThemeEngine &instance();

  // Active theme
  const ThemeDefinition &activeTheme() const;
  Theme classicTheme() const;

  // Apply a theme by name
  void setActiveTheme(const QString &name);
  void setActiveTheme(const ThemeDefinition &theme);

  // Preset management
  QStringList availableThemes() const;
  ThemeDefinition themeByName(const QString &name) const;
  bool hasTheme(const QString &name) const;

  // User themes
  bool importTheme(const QString &filePath);
  bool exportTheme(const QString &name, const QString &filePath) const;

  // Load user themes from disk
  void loadUserThemes();
  void saveUserTheme(const ThemeDefinition &theme);

  // UI config helpers
  int animationDuration() const;
  qreal glowIntensity() const;
  int borderRadius() const;

signals:
  void themeChanged(const ThemeDefinition &theme);

private:
  ThemeEngine();
  ~ThemeEngine() = default;
  ThemeEngine(const ThemeEngine &) = delete;
  ThemeEngine &operator=(const ThemeEngine &) = delete;

  void registerBuiltinThemes();
  QString userThemesDirectory() const;

  ThemeDefinition m_activeTheme;
  QMap<QString, ThemeDefinition> m_themes;
};

#endif
