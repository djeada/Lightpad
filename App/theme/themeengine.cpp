#include "themeengine.h"
#include "themepresets.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QtGlobal>

ThemeEngine &ThemeEngine::instance() {
  static ThemeEngine engine;
  return engine;
}

ThemeEngine::ThemeEngine() : QObject(nullptr) {
  registerBuiltinThemes();
  m_activeTheme = m_themes.value("Hacker Dark");
}

void ThemeEngine::registerBuiltinThemes() {
  auto reg = [this](ThemeDefinition (*fn)()) {
    ThemeDefinition t = fn();
    m_themes.insert(t.name, t);
  };
  reg(ThemePresets::hackerDark);
  reg(ThemePresets::minimalDark);
  reg(ThemePresets::githubDark);
  reg(ThemePresets::midnightBlue);
  reg(ThemePresets::dracula);
  reg(ThemePresets::monokaiPro);
  reg(ThemePresets::nord);
  reg(ThemePresets::solarizedDark);
  reg(ThemePresets::cyberpunk);
  reg(ThemePresets::matrix);
  reg(ThemePresets::ghost);
  reg(ThemePresets::daylight);
}

const ThemeDefinition &ThemeEngine::activeTheme() const {
  return m_activeTheme;
}

Theme ThemeEngine::classicTheme() const {
  return m_activeTheme.toClassicTheme();
}

void ThemeEngine::setActiveTheme(const QString &name) {
  if (m_themes.contains(name)) {
    m_activeTheme = m_themes.value(name);
    emit themeChanged(m_activeTheme);
  }
}

void ThemeEngine::setActiveTheme(const ThemeDefinition &theme) {
  m_activeTheme = theme;
  if (!m_themes.contains(theme.name))
    m_themes.insert(theme.name, theme);
  emit themeChanged(m_activeTheme);
}

QStringList ThemeEngine::availableThemes() const { return m_themes.keys(); }

ThemeDefinition ThemeEngine::themeByName(const QString &name) const {
  return m_themes.value(name);
}

bool ThemeEngine::hasTheme(const QString &name) const {
  return m_themes.contains(name);
}

bool ThemeEngine::importTheme(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  file.close();

  if (err.error != QJsonParseError::NoError || !doc.isObject())
    return false;

  ThemeDefinition theme;
  theme.read(doc.object());
  if (theme.name.isEmpty())
    return false;

  m_themes.insert(theme.name, theme);
  saveUserTheme(theme);
  return true;
}

bool ThemeEngine::exportTheme(const QString &name,
                              const QString &filePath) const {
  if (!m_themes.contains(name))
    return false;

  QJsonObject json;
  m_themes.value(name).write(json);

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
  file.close();
  return true;
}

QString ThemeEngine::userThemesDirectory() const {
  QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  return configDir + "/Lightpad/themes";
}

void ThemeEngine::loadUserThemes() {
  QDir dir(userThemesDirectory());
  if (!dir.exists())
    return;

  const QStringList files = dir.entryList({"*.json"}, QDir::Files);
  for (const QString &fileName : files) {
    QFile file(dir.absoluteFilePath(fileName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject())
      continue;

    ThemeDefinition theme;
    theme.read(doc.object());
    if (!theme.name.isEmpty())
      m_themes.insert(theme.name, theme);
  }
}

void ThemeEngine::saveUserTheme(const ThemeDefinition &theme) {
  QDir dir(userThemesDirectory());
  if (!dir.exists())
    dir.mkpath(".");

  QString safeName = theme.name;
  safeName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");

  QJsonObject json;
  theme.write(json);

  QFile file(dir.absoluteFilePath(safeName + ".json"));
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    file.close();
  }
}

int ThemeEngine::animationDuration() const {
  const QString &speed = m_activeTheme.ui.animationSpeed;
  if (speed == "off")
    return 0;
  if (speed == "subtle")
    return 80;
  if (speed == "normal")
    return 150;
  if (speed == "fancy")
    return 300;
  return 150;
}

qreal ThemeEngine::glowIntensity() const {
  return m_activeTheme.ui.glowIntensity;
}

qreal ThemeEngine::chromeOpacity() const {
  return qBound(0.0, m_activeTheme.ui.chromeOpacity, 1.0);
}

int ThemeEngine::borderRadius() const { return m_activeTheme.ui.borderRadius; }

bool ThemeEngine::panelBordersEnabled() const {
  return m_activeTheme.ui.panelBorders;
}
