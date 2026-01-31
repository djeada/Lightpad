#include "debugconfiguration.h"
#include "../core/logging/logger.h"
#include "debugadapterregistry.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>

DebugConfigurationManager& DebugConfigurationManager::instance()
{
    static DebugConfigurationManager instance;
    return instance;
}

DebugConfigurationManager::DebugConfigurationManager()
    : QObject(nullptr)
{
}

bool DebugConfigurationManager::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARNING(QString("Failed to open debug configuration file: %1").arg(filePath));
        return false;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("Failed to parse debug configuration: %1").arg(parseError.errorString()));
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Clear existing configurations
    m_configurations.clear();
    m_compoundConfigurations.clear();
    
    // Load configurations
    QJsonArray cfgArray = root["configurations"].toArray();
    for (const auto& val : cfgArray) {
        DebugConfiguration cfg = DebugConfiguration::fromJson(val.toObject());
        if (!cfg.name.isEmpty()) {
            m_configurations[cfg.name] = cfg;
        }
    }
    
    // Load compound configurations
    QJsonArray compoundArray = root["compounds"].toArray();
    for (const auto& val : compoundArray) {
        CompoundDebugConfiguration cfg = CompoundDebugConfiguration::fromJson(val.toObject());
        if (!cfg.name.isEmpty()) {
            m_compoundConfigurations[cfg.name] = cfg;
        }
    }
    
    m_configFilePath = filePath;
    
    LOG_INFO(QString("Loaded %1 debug configurations from %2")
             .arg(m_configurations.size()).arg(filePath));
    
    emit configurationsLoaded();
    return true;
}

bool DebugConfigurationManager::saveToFile(const QString& filePath)
{
    QJsonObject root;
    root["version"] = "0.2.0";
    
    // Save configurations
    QJsonArray cfgArray;
    for (const auto& cfg : m_configurations) {
        cfgArray.append(cfg.toJson());
    }
    root["configurations"] = cfgArray;
    
    // Save compound configurations
    if (!m_compoundConfigurations.isEmpty()) {
        QJsonArray compoundArray;
        for (const auto& cfg : m_compoundConfigurations) {
            compoundArray.append(cfg.toJson());
        }
        root["compounds"] = compoundArray;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Failed to save debug configuration file: %1").arg(filePath));
        return false;
    }
    
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    
    m_configFilePath = filePath;
    LOG_INFO(QString("Saved %1 debug configurations to %2")
             .arg(m_configurations.size()).arg(filePath));
    
    return true;
}

void DebugConfigurationManager::addConfiguration(const DebugConfiguration& config)
{
    m_configurations[config.name] = config;
    emit configurationAdded(config.name);
}

void DebugConfigurationManager::removeConfiguration(const QString& name)
{
    if (m_configurations.remove(name) > 0) {
        emit configurationRemoved(name);
    }
}

void DebugConfigurationManager::updateConfiguration(const QString& name,
                                                     const DebugConfiguration& config)
{
    if (name != config.name && m_configurations.contains(name)) {
        m_configurations.remove(name);
    }
    m_configurations[config.name] = config;
    emit configurationChanged(config.name);
}

DebugConfiguration DebugConfigurationManager::configuration(const QString& name) const
{
    return m_configurations.value(name);
}

QList<DebugConfiguration> DebugConfigurationManager::allConfigurations() const
{
    return m_configurations.values();
}

QList<DebugConfiguration> DebugConfigurationManager::configurationsForType(const QString& type) const
{
    QList<DebugConfiguration> result;
    for (const auto& cfg : m_configurations) {
        if (cfg.type == type) {
            result.append(cfg);
        }
    }
    return result;
}

void DebugConfigurationManager::addCompoundConfiguration(const CompoundDebugConfiguration& config)
{
    m_compoundConfigurations[config.name] = config;
}

QList<CompoundDebugConfiguration> DebugConfigurationManager::allCompoundConfigurations() const
{
    return m_compoundConfigurations.values();
}

void DebugConfigurationManager::setWorkspaceFolder(const QString& folder)
{
    m_workspaceFolder = folder;
}

DebugConfiguration DebugConfigurationManager::resolveVariables(const DebugConfiguration& config,
                                                                const QString& currentFile) const
{
    DebugConfiguration resolved = config;
    
    resolved.program = substituteVariable(config.program, currentFile);
    resolved.cwd = substituteVariable(config.cwd, currentFile);
    
    resolved.args.clear();
    for (const QString& arg : config.args) {
        resolved.args.append(substituteVariable(arg, currentFile));
    }
    
    // Substitute in environment variables
    for (auto it = config.env.begin(); it != config.env.end(); ++it) {
        resolved.env[it.key()] = substituteVariable(it.value(), currentFile);
    }
    
    // Substitute in adapter config
    for (auto it = config.adapterConfig.begin(); it != config.adapterConfig.end(); ++it) {
        if (it.value().isString()) {
            resolved.adapterConfig[it.key()] = substituteVariable(it.value().toString(), currentFile);
        } else {
            resolved.adapterConfig[it.key()] = it.value();
        }
    }
    
    return resolved;
}

QString DebugConfigurationManager::substituteVariable(const QString& value,
                                                       const QString& currentFile) const
{
    QString result = value;
    
    // ${workspaceFolder}
    if (!m_workspaceFolder.isEmpty()) {
        result.replace("${workspaceFolder}", m_workspaceFolder);
    }
    
    // ${file} - full path to current file
    if (!currentFile.isEmpty()) {
        result.replace("${file}", currentFile);
        
        QFileInfo fi(currentFile);
        
        // ${fileBasename} - file name without path
        result.replace("${fileBasename}", fi.fileName());
        
        // ${fileBasenameNoExtension} - file name without path or extension
        result.replace("${fileBasenameNoExtension}", fi.completeBaseName());
        
        // ${fileDirname} - directory containing the file
        result.replace("${fileDirname}", fi.absolutePath());
        
        // ${fileExtname} - file extension
        result.replace("${fileExtname}", fi.suffix().isEmpty() ? "" : "." + fi.suffix());
        
        // ${relativeFile} - file relative to workspace
        if (!m_workspaceFolder.isEmpty() && currentFile.startsWith(m_workspaceFolder)) {
            QString relative = currentFile.mid(m_workspaceFolder.length());
            if (relative.startsWith('/') || relative.startsWith('\\')) {
                relative = relative.mid(1);
            }
            result.replace("${relativeFile}", relative);
        }
    }
    
    // ${pathSeparator}
#ifdef Q_OS_WIN
    result.replace("${pathSeparator}", "\\");
#else
    result.replace("${pathSeparator}", "/");
#endif
    
    return result;
}

DebugConfiguration DebugConfigurationManager::createQuickConfig(const QString& filePath) const
{
    DebugConfiguration config;
    
    // Find a suitable adapter
    auto adapter = DebugAdapterRegistry::instance().preferredAdapterForFile(filePath);
    if (!adapter) {
        return config;
    }
    
    // Use adapter's default launch config as base
    QJsonObject launchConfig = adapter->createLaunchConfig(filePath,
                                                            QFileInfo(filePath).absolutePath());
    config = DebugConfiguration::fromJson(launchConfig);
    
    // Set a name based on the file
    QFileInfo fi(filePath);
    config.name = QString("Debug %1").arg(fi.fileName());
    
    return config;
}
