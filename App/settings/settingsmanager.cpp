#include "settingsmanager.h"
#include "../core/logging/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>
#include <QCoreApplication>

SettingsManager& SettingsManager::instance()
{
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager()
    : QObject(nullptr)
    , m_dirty(false)
{
    initializeDefaults();
}

void SettingsManager::initializeDefaults()
{
    // Font defaults
    m_defaults["fontFamily"] = "Monospace";
    m_defaults["fontSize"] = 12;
    m_defaults["fontWeight"] = 50;  // Normal weight
    m_defaults["fontItalic"] = false;

    // Editor defaults
    m_defaults["autoIndent"] = true;
    m_defaults["showLineNumberArea"] = true;
    m_defaults["lineHighlighted"] = true;
    m_defaults["matchingBracketsHighlighted"] = true;
    m_defaults["tabWidth"] = 4;

    // Theme defaults
    QJsonObject themeDefaults;
    themeDefaults["backgroundColor"] = "#000000";
    themeDefaults["foregroundColor"] = "#d3d3d3";
    themeDefaults["highlightColor"] = "#2a2a2a";
    themeDefaults["keywordFormat_1"] = "#b8860b";
    themeDefaults["keywordFormat_2"] = "#ee82ee";
    m_defaults["theme"] = themeDefaults;

    // Settings version for migration
    m_defaults["settingsVersion"] = SETTINGS_VERSION;

    // Initialize settings with defaults
    m_settings = m_defaults;
}

QString SettingsManager::getSettingsDirectory() const
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    
    if (configPath.isEmpty()) {
        // Fallback to home directory
        configPath = QDir::homePath() + "/.config/lightpad";
    }

    return configPath;
}

QString SettingsManager::getSettingsFilePath() const
{
    return getSettingsDirectory() + "/settings.json";
}

bool SettingsManager::ensureSettingsDirectoryExists()
{
    QString dirPath = getSettingsDirectory();
    QDir dir(dirPath);
    
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR(QString("Failed to create settings directory: %1").arg(dirPath));
            return false;
        }
        LOG_INFO(QString("Created settings directory: %1").arg(dirPath));
    }
    
    return true;
}

bool SettingsManager::loadSettings()
{
    QString filePath = getSettingsFilePath();
    
    // First, try to migrate from old location if new file doesn't exist
    if (!QFileInfo(filePath).exists()) {
        QString oldPath = "settings.json";
        if (QFileInfo(oldPath).exists()) {
            LOG_INFO("Found old settings file, attempting migration...");
            migrateFromOldPath(oldPath);
        }
    }

    QFile file(filePath);
    
    if (!file.exists()) {
        LOG_INFO("Settings file does not exist, using defaults");
        m_settings = m_defaults;
        emit settingsLoaded();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open settings file: %1").arg(filePath));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("Failed to parse settings: %1").arg(parseError.errorString()));
        m_settings = m_defaults;
        emit settingsLoaded();
        return false;
    }

    m_settings = doc.object();

    // Check for settings version and migrate if needed
    int version = m_settings.value("settingsVersion").toInt(0);
    if (version < SETTINGS_VERSION) {
        migrateSettings(version);
    }

    // Merge with defaults to ensure all keys exist
    for (auto it = m_defaults.begin(); it != m_defaults.end(); ++it) {
        if (!m_settings.contains(it.key())) {
            m_settings[it.key()] = it.value();
        }
    }

    LOG_INFO(QString("Settings loaded from: %1").arg(filePath));
    emit settingsLoaded();
    return true;
}

bool SettingsManager::saveSettings()
{
    if (!ensureSettingsDirectoryExists()) {
        return false;
    }

    QString filePath = getSettingsFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Cannot open settings file for writing: %1").arg(filePath));
        return false;
    }

    // Update version
    m_settings["settingsVersion"] = SETTINGS_VERSION;

    QJsonDocument doc(m_settings);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_dirty = false;
    LOG_INFO(QString("Settings saved to: %1").arg(filePath));
    emit settingsSaved();
    return true;
}

QVariant SettingsManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    // Support dot notation for nested keys
    QStringList keys = key.split('.');
    QJsonValue value = m_settings;

    for (const QString& k : keys) {
        if (!value.isObject()) {
            return defaultValue;
        }
        value = value.toObject().value(k);
        if (value.isUndefined()) {
            return defaultValue;
        }
    }

    return value.toVariant();
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    // For now, only support top-level keys
    // TODO: Add support for nested keys
    m_settings[key] = QJsonValue::fromVariant(value);
    m_dirty = true;
    emit settingChanged(key, value);
}

bool SettingsManager::hasKey(const QString& key) const
{
    QStringList keys = key.split('.');
    QJsonValue value = m_settings;

    for (const QString& k : keys) {
        if (!value.isObject()) {
            return false;
        }
        value = value.toObject().value(k);
        if (value.isUndefined()) {
            return false;
        }
    }

    return true;
}

void SettingsManager::resetToDefaults()
{
    m_settings = m_defaults;
    m_dirty = true;
    LOG_INFO("Settings reset to defaults");
}

const QJsonObject& SettingsManager::getSettingsObject() const
{
    return m_settings;
}

bool SettingsManager::migrateFromOldPath(const QString& oldPath)
{
    QFile oldFile(oldPath);
    if (!oldFile.exists()) {
        return false;
    }

    if (!oldFile.open(QIODevice::ReadOnly)) {
        LOG_WARNING(QString("Cannot open old settings file: %1").arg(oldPath));
        return false;
    }

    QByteArray data = oldFile.readAll();
    oldFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        LOG_WARNING(QString("Failed to parse old settings: %1").arg(parseError.errorString()));
        return false;
    }

    m_settings = doc.object();
    
    // Save to new location
    if (saveSettings()) {
        LOG_INFO(QString("Successfully migrated settings from %1 to %2")
            .arg(oldPath).arg(getSettingsFilePath()));
        return true;
    }

    return false;
}

void SettingsManager::migrateSettings(int fromVersion)
{
    LOG_INFO(QString("Migrating settings from version %1 to %2")
        .arg(fromVersion).arg(SETTINGS_VERSION));

    // Add migration logic here for future versions
    // Example:
    // if (fromVersion < 2) {
    //     // Migration from v1 to v2
    // }

    m_settings["settingsVersion"] = SETTINGS_VERSION;
    m_dirty = true;
}
