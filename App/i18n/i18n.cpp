#include "i18n.h"
#include "../core/logging/logger.h"

#include <QDir>
#include <QDirIterator>
#include <QLibraryInfo>
#include <QStandardPaths>

I18n &I18n::instance() {
  static I18n instance;
  return instance;
}

I18n::I18n()
    : QObject(nullptr), m_app(nullptr), m_translator(new QTranslator(this)),
      m_qtTranslator(new QTranslator(this)), m_currentLanguage("en") {
  // Initialize available languages with at least English
  m_availableLanguages["en"] = "English";
}

I18n::~I18n() {}

void I18n::initialize(QCoreApplication *app) {
  m_app = app;

  // Load available languages
  loadAvailableLanguages();

  // Try to load system language, fall back to English
  QString sysLang = systemLanguage();
  if (isLanguageAvailable(sysLang)) {
    setLanguage(sysLang);
  } else {
    setLanguage("en");
  }

  LOG_INFO(
      QString("I18n initialized with language: %1").arg(m_currentLanguage));
}

QString I18n::currentLanguage() const { return m_currentLanguage; }

bool I18n::setLanguage(const QString &languageCode) {
  if (!m_app) {
    LOG_ERROR("I18n: Application not initialized");
    return false;
  }

  if (languageCode == m_currentLanguage) {
    return true; // Already using this language
  }

  if (!isLanguageAvailable(languageCode) && languageCode != "en") {
    LOG_WARNING(QString("Language not available: %1").arg(languageCode));
    return false;
  }

  // Remove current translators
  m_app->removeTranslator(m_translator);
  m_app->removeTranslator(m_qtTranslator);

  // Load new translation
  if (languageCode != "en") {
    if (!loadTranslation(languageCode)) {
      // Fall back to English
      m_currentLanguage = "en";
      emit languageChanged(m_currentLanguage);
      return false;
    }
  }

  m_currentLanguage = languageCode;
  LOG_INFO(QString("Language changed to: %1").arg(languageCode));
  emit languageChanged(m_currentLanguage);

  return true;
}

QMap<QString, QString> I18n::availableLanguages() const {
  return m_availableLanguages;
}

QString I18n::systemLanguage() const {
  QLocale locale;
  QString lang = locale.name().left(
      2); // Get first two characters (e.g., "en" from "en_US")
  return lang;
}

bool I18n::isLanguageAvailable(const QString &languageCode) const {
  return m_availableLanguages.contains(languageCode);
}

QString I18n::translationsDirectory() const {
  // Check multiple possible locations
  QStringList paths;

  // Application directory
  paths << QCoreApplication::applicationDirPath() + "/translations";

  // Resource directory
  paths << ":/translations";

  // User data directory
  paths << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
               "/translations";

  // System directories
#ifndef Q_OS_WIN
  paths << "/usr/share/lightpad/translations";
  paths << "/usr/local/share/lightpad/translations";
#endif

  for (const QString &path : paths) {
    if (QDir(path).exists()) {
      return path;
    }
  }

  return QCoreApplication::applicationDirPath() + "/translations";
}

void I18n::loadAvailableLanguages() {
  // Always have English
  m_availableLanguages["en"] = "English";

  // Add other common languages that could be translated
  m_availableLanguages["de"] = "Deutsch";
  m_availableLanguages["es"] = "Español";
  m_availableLanguages["fr"] = "Français";
  m_availableLanguages["it"] = "Italiano";
  m_availableLanguages["ja"] = "日本語";
  m_availableLanguages["ko"] = "한국어";
  m_availableLanguages["pl"] = "Polski";
  m_availableLanguages["pt"] = "Português";
  m_availableLanguages["ru"] = "Русский";
  m_availableLanguages["zh"] = "中文";

  // Check which translations actually exist
  QString transDir = translationsDirectory();
  QDir dir(transDir);

  if (dir.exists()) {
    QStringList filters = {"lightpad_*.qm"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &file : files) {
      QString baseName = file.baseName();
      QString langCode = baseName.mid(9); // Remove "lightpad_" prefix

      if (!m_availableLanguages.contains(langCode)) {
        // Add language with code as name if not in our list
        m_availableLanguages[langCode] = langCode;
      }
    }
  }

  LOG_DEBUG(
      QString("Found %1 available languages").arg(m_availableLanguages.size()));
}

bool I18n::loadTranslation(const QString &languageCode) {
  QString transDir = translationsDirectory();

  // Load application translation
  QString appTransFile = transDir + "/lightpad_" + languageCode + ".qm";
  if (m_translator->load(appTransFile)) {
    m_app->installTranslator(m_translator);
    LOG_DEBUG(QString("Loaded translation: %1").arg(appTransFile));
  } else {
    // Try loading from resources
    if (m_translator->load(":/translations/lightpad_" + languageCode + ".qm")) {
      m_app->installTranslator(m_translator);
      LOG_DEBUG(
          QString("Loaded translation from resources: %1").arg(languageCode));
    } else {
      LOG_WARNING(
          QString("Could not load translation for: %1").arg(languageCode));
      return false;
    }
  }

  // Load Qt's built-in translations
  QString qtTransFile = QLibraryInfo::path(QLibraryInfo::TranslationsPath) +
                        "/qt_" + languageCode + ".qm";
  if (m_qtTranslator->load(qtTransFile)) {
    m_app->installTranslator(m_qtTranslator);
  }

  return true;
}
