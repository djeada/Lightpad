#include "themeengine.h"
#include "themepresets.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtGlobal>

namespace {
QString sanitizedThemeFileName(const QString &name) {
  QString safeName = name;
  safeName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
  if (safeName != name) {
    const QByteArray hash =
        QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(8);
    safeName += QLatin1Char('-') + QString::fromLatin1(hash);
  }
  return safeName;
}

void removeThemeFilesNamed(const QString &directory, const QString &name) {
  QDir dir(directory);
  if (!dir.exists())
    return;
  const QStringList files = dir.entryList({"*.json"}, QDir::Files);
  for (const QString &fileName : files) {
    QFile file(dir.absoluteFilePath(fileName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isObject() && doc.object().value("name").toString() == name)
      QFile::remove(dir.absoluteFilePath(fileName));
  }
}
} // namespace

ThemeEngine &ThemeEngine::instance() {
  static ThemeEngine engine;
  return engine;
}

ThemeEngine::ThemeEngine() : QObject(nullptr) {
  registerBuiltinThemes();
  loadUserThemes();
  activate(m_themes.value("Hacker Dark"));
}

void ThemeEngine::activate(const ThemeDefinition &theme) {
  m_activeThemeSource = theme;
  m_activeThemeSource.normalize();
  m_activeTheme = m_activeThemeSource.readable();
}

void ThemeEngine::registerBuiltinThemes() {
  auto reg = [this](ThemeDefinition (*fn)()) {
    ThemeDefinition t = fn();
    t.normalize();
    m_themes.insert(t.name, t);
    m_builtinThemes.insert(t.name);
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

const ThemeDefinition &ThemeEngine::activeThemeSource() const {
  return m_activeThemeSource;
}

Theme ThemeEngine::classicTheme() const {
  return m_activeTheme.toClassicTheme();
}

void ThemeEngine::setActiveTheme(const QString &name) {
  if (m_themes.contains(name)) {
    activate(m_themes.value(name));
    emit themeChanged(m_activeTheme);
  }
}

void ThemeEngine::setActiveTheme(const ThemeDefinition &theme) {
  activate(theme);
  m_themes.insert(m_activeThemeSource.name, m_activeThemeSource);
  emit themeChanged(m_activeTheme);
}

QStringList ThemeEngine::availableThemes() const { return m_themes.keys(); }

ThemeDefinition ThemeEngine::themeByName(const QString &name) const {
  return m_themes.value(name);
}

bool ThemeEngine::hasTheme(const QString &name) const {
  return m_themes.contains(name);
}

bool ThemeEngine::isBuiltinTheme(const QString &name) const {
  return m_builtinThemes.contains(name);
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

  theme.normalize();
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

QString ThemeEngine::userThemeFilePath(const QString &name) const {
  QDir dir(userThemesDirectory());
  return dir.absoluteFilePath(sanitizedThemeFileName(name) + ".json");
}

QString ThemeEngine::makeUniqueCustomThemeName(const QString &baseName) const {
  const QString trimmedBase = baseName.trimmed().isEmpty()
                                  ? QStringLiteral("Custom Theme")
                                  : baseName.trimmed();
  if (!m_themes.contains(trimmedBase))
    return trimmedBase;

  int suffix = 2;
  while (true) {
    const QString candidate =
        QStringLiteral("%1 %2").arg(trimmedBase).arg(suffix);
    if (!m_themes.contains(candidate))
      return candidate;
    ++suffix;
  }
}

void ThemeEngine::loadUserThemes() {
  for (auto it = m_themes.begin(); it != m_themes.end();) {
    if (!m_builtinThemes.contains(it.key())) {
      it = m_themes.erase(it);
    } else {
      ++it;
    }
  }

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
    theme.normalize();
    if (!theme.name.isEmpty())
      m_themes.insert(theme.name, theme);
  }
}

ThemeDefinition ThemeEngine::saveUserTheme(const ThemeDefinition &theme) {
  QDir dir(userThemesDirectory());
  if (!dir.exists())
    dir.mkpath(".");

  ThemeDefinition normalized = theme;
  normalized.normalize();
  normalized.type = QStringLiteral("custom");

  if (normalized.name.trimmed().isEmpty())
    normalized.name = QStringLiteral("Custom Theme");

  if (m_builtinThemes.contains(normalized.name)) {
    normalized.name =
        makeUniqueCustomThemeName(normalized.name + QStringLiteral(" Custom"));
  }

  QJsonObject json;
  normalized.write(json);

  removeThemeFilesNamed(userThemesDirectory(), normalized.name);
  QFile file(userThemeFilePath(normalized.name));
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    file.close();
  }

  m_themes.insert(normalized.name, normalized);
  return normalized;
}

bool ThemeEngine::deleteUserTheme(const QString &name) {
  if (name.isEmpty() || m_builtinThemes.contains(name) ||
      !m_themes.contains(name))
    return false;

  const bool wasActive = m_activeTheme.name == name;
  m_themes.remove(name);
  QFile::remove(userThemeFilePath(name));
  removeThemeFilesNamed(userThemesDirectory(), name);

  if (wasActive) {
    const QString fallback = m_builtinThemes.contains("Hacker Dark")
                                 ? QStringLiteral("Hacker Dark")
                             : m_themes.isEmpty() ? QString()
                                                  : m_themes.firstKey();
    if (!fallback.isEmpty()) {
      activate(m_themes.value(fallback));
      emit themeChanged(m_activeTheme);
    }
  }

  return true;
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
