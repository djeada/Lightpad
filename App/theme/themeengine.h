#ifndef THEMEENGINE_H
#define THEMEENGINE_H

#include "../settings/theme.h"
#include "themedefinition.h"
#include <QMap>
#include <QObject>
#include <QStringList>

class ThemeEngine : public QObject {
  Q_OBJECT
public:
  static ThemeEngine &instance();

  const ThemeDefinition &activeTheme() const;
  Theme classicTheme() const;

  void setActiveTheme(const QString &name);
  void setActiveTheme(const ThemeDefinition &theme);

  QStringList availableThemes() const;
  ThemeDefinition themeByName(const QString &name) const;
  bool hasTheme(const QString &name) const;

  bool importTheme(const QString &filePath);
  bool exportTheme(const QString &name, const QString &filePath) const;

  void loadUserThemes();
  void saveUserTheme(const ThemeDefinition &theme);

  int animationDuration() const;
  qreal glowIntensity() const;
  qreal chromeOpacity() const;
  int borderRadius() const;
  bool panelBordersEnabled() const;

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
