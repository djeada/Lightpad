#include "debugsettings.h"
#include "../core/logging/logger.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>

// Static keys for settings
const QString DebugSettings::KEY_STOP_ON_ENTRY = "stopOnEntry";
const QString DebugSettings::KEY_EXTERNAL_CONSOLE = "externalConsole";
const QString DebugSettings::KEY_SOURCE_MAP_PATH_OVERRIDES = "sourceMapPathOverrides";
const QString DebugSettings::KEY_JUST_MY_CODE = "justMyCode";
const QString DebugSettings::KEY_SHOW_RETURN_VALUE = "showReturnValue";
const QString DebugSettings::KEY_AUTO_RELOAD = "autoReload";
const QString DebugSettings::KEY_LOG_TO_FILE = "logToFile";
const QString DebugSettings::KEY_LOG_FILE_PATH = "logFilePath";

DebugSettings& DebugSettings::instance()
{
    static DebugSettings instance;
    return instance;
}

DebugSettings::DebugSettings()
{
}

void DebugSettings::initialize(const QString& workspaceFolder)
{
    m_workspaceFolder = workspaceFolder;
    ensureDirectoryExists();
    loadAll();
    
    LOG_INFO(QString("Debug settings initialized in: %1").arg(debugSettingsDir()));
}

QString DebugSettings::workspaceFolder() const
{
    return m_workspaceFolder;
}

QString DebugSettings::debugSettingsDir() const
{
    return m_workspaceFolder + "/.lightpad/debug";
}

QString DebugSettings::configFilePath(const QString& fileName) const
{
    return debugSettingsDir() + "/" + fileName;
}

QString DebugSettings::launchConfigPath() const
{
    return configFilePath("launch.json");
}

QString DebugSettings::breakpointsConfigPath() const
{
    return configFilePath("breakpoints.json");
}

QString DebugSettings::watchesConfigPath() const
{
    return configFilePath("watches.json");
}

QString DebugSettings::adaptersConfigPath() const
{
    return configFilePath("adapters.json");
}

QString DebugSettings::settingsConfigPath() const
{
    return configFilePath("settings.json");
}

void DebugSettings::ensureDirectoryExists()
{
    QDir dir(debugSettingsDir());
    if (!dir.exists()) {
        dir.mkpath(".");
        LOG_INFO(QString("Created debug settings directory: %1").arg(debugSettingsDir()));
    }
    
    // Create default config files if they don't exist
    if (!QFile::exists(launchConfigPath())) {
        createDefaultLaunchConfig();
    }
    if (!QFile::exists(breakpointsConfigPath())) {
        createDefaultBreakpointsConfig();
    }
    if (!QFile::exists(watchesConfigPath())) {
        createDefaultWatchesConfig();
    }
    if (!QFile::exists(adaptersConfigPath())) {
        createDefaultAdaptersConfig();
    }
    if (!QFile::exists(settingsConfigPath())) {
        createDefaultSettingsConfig();
    }
}

void DebugSettings::createDefaultLaunchConfig()
{
    QJsonObject config;
    config["version"] = "1.0.0";
    config["_comment"] = "Debug launch configurations. Edit this file to add your own configurations.";
    
    QJsonArray configurations;
    
    // Example Python configuration
    QJsonObject pythonConfig;
    pythonConfig["name"] = "Python: Current File";
    pythonConfig["type"] = "python";
    pythonConfig["request"] = "launch";
    pythonConfig["program"] = "${file}";
    pythonConfig["console"] = "integratedTerminal";
    pythonConfig["cwd"] = "${workspaceFolder}";
    pythonConfig["_comment"] = "Example Python debug configuration";
    configurations.append(pythonConfig);
    
    // Example C++ GDB configuration
    QJsonObject cppConfig;
    cppConfig["name"] = "C++: GDB Launch";
    cppConfig["type"] = "cppdbg";
    cppConfig["request"] = "launch";
    cppConfig["program"] = "${workspaceFolder}/a.out";
    cppConfig["args"] = QJsonArray();
    cppConfig["stopAtEntry"] = false;
    cppConfig["cwd"] = "${workspaceFolder}";
    cppConfig["environment"] = QJsonArray();
    cppConfig["externalConsole"] = false;
    cppConfig["MIMode"] = "gdb";
    cppConfig["miDebuggerPath"] = "gdb";
    
    QJsonArray setupCommands;
    QJsonObject prettyPrint;
    prettyPrint["description"] = "Enable pretty-printing for gdb";
    prettyPrint["text"] = "-enable-pretty-printing";
    prettyPrint["ignoreFailures"] = true;
    setupCommands.append(prettyPrint);
    cppConfig["setupCommands"] = setupCommands;
    cppConfig["_comment"] = "Example C++ GDB debug configuration";
    configurations.append(cppConfig);
    
    // Example Node.js configuration
    QJsonObject nodeConfig;
    nodeConfig["name"] = "Node.js: Current File";
    nodeConfig["type"] = "node";
    nodeConfig["request"] = "launch";
    nodeConfig["program"] = "${file}";
    nodeConfig["cwd"] = "${workspaceFolder}";
    nodeConfig["console"] = "integratedTerminal";
    nodeConfig["_comment"] = "Example Node.js debug configuration";
    configurations.append(nodeConfig);
    
    config["configurations"] = configurations;
    
    // Compound configurations
    QJsonArray compounds;
    QJsonObject exampleCompound;
    exampleCompound["name"] = "Server + Client";
    exampleCompound["configurations"] = QJsonArray({"Server", "Client"});
    exampleCompound["stopAll"] = true;
    exampleCompound["_comment"] = "Example compound configuration for debugging multiple targets";
    compounds.append(exampleCompound);
    config["compounds"] = compounds;
    
    writeJsonFile(launchConfigPath(), config);
    LOG_INFO("Created default launch.json");
}

void DebugSettings::createDefaultBreakpointsConfig()
{
    QJsonObject config;
    config["version"] = "1.0.0";
    config["_comment"] = "Breakpoints configuration. This file is auto-saved but can be manually edited.";
    
    // Source breakpoints by file
    config["sourceBreakpoints"] = QJsonObject();
    
    // Function breakpoints
    config["functionBreakpoints"] = QJsonArray();
    
    // Data breakpoints (watchpoints)
    config["dataBreakpoints"] = QJsonArray();
    
    // Exception breakpoints
    QJsonObject exceptionBreakpoints;
    exceptionBreakpoints["_comment"] = "Exception breakpoint filters. Set to true to enable.";
    exceptionBreakpoints["uncaught"] = true;
    exceptionBreakpoints["raised"] = false;
    config["exceptionBreakpoints"] = exceptionBreakpoints;
    
    writeJsonFile(breakpointsConfigPath(), config);
    LOG_INFO("Created default breakpoints.json");
}

void DebugSettings::createDefaultWatchesConfig()
{
    QJsonObject config;
    config["version"] = "1.0.0";
    config["_comment"] = "Watch expressions. Add expressions to monitor during debugging.";
    
    QJsonArray watches;
    // Example watches (commented out in description)
    config["watches"] = watches;
    
    QJsonArray examples;
    examples.append("myVariable");
    examples.append("array.length");
    examples.append("object.property");
    config["_examples"] = examples;
    
    writeJsonFile(watchesConfigPath(), config);
    LOG_INFO("Created default watches.json");
}

void DebugSettings::createDefaultAdaptersConfig()
{
    QJsonObject config;
    config["version"] = "1.0.0";
    config["_comment"] = "Debug adapter configuration. Customize adapter settings and paths here.";
    
    // Default adapter mappings by extension
    QJsonObject defaultAdapters;
    defaultAdapters["_comment"] = "Map file extensions to debug adapter IDs";
    defaultAdapters[".py"] = "python-debugpy";
    defaultAdapters[".pyw"] = "python-debugpy";
    defaultAdapters[".js"] = "node-debug";
    defaultAdapters[".mjs"] = "node-debug";
    defaultAdapters[".ts"] = "node-debug";
    defaultAdapters[".cpp"] = "cppdbg-gdb";
    defaultAdapters[".c"] = "cppdbg-gdb";
    defaultAdapters[".cc"] = "cppdbg-gdb";
    defaultAdapters[".cxx"] = "cppdbg-gdb";
    defaultAdapters[".rs"] = "cppdbg-gdb";
    defaultAdapters[".go"] = "cppdbg-gdb";
    config["defaultAdapters"] = defaultAdapters;
    
    // Adapter-specific overrides
    QJsonObject adapters;
    
    // GDB adapter settings
    QJsonObject gdbSettings;
    gdbSettings["_comment"] = "GDB debug adapter settings";
    gdbSettings["miDebuggerPath"] = "gdb";
    gdbSettings["setupCommands"] = QJsonArray();
    
    QJsonArray gdbSetupCommands;
    QJsonObject prettyPrint;
    prettyPrint["description"] = "Enable pretty-printing";
    prettyPrint["text"] = "-enable-pretty-printing";
    prettyPrint["ignoreFailures"] = true;
    gdbSetupCommands.append(prettyPrint);
    
    QJsonObject disablePagination;
    disablePagination["description"] = "Disable pagination";
    disablePagination["text"] = "set pagination off";
    disablePagination["ignoreFailures"] = true;
    gdbSetupCommands.append(disablePagination);
    gdbSettings["setupCommands"] = gdbSetupCommands;
    
    adapters["cppdbg-gdb"] = gdbSettings;
    
    // LLDB adapter settings
    QJsonObject lldbSettings;
    lldbSettings["_comment"] = "LLDB debug adapter settings";
    lldbSettings["miDebuggerPath"] = "lldb-vscode";
    adapters["cppdbg-lldb"] = lldbSettings;
    
    // Python adapter settings
    QJsonObject pythonSettings;
    pythonSettings["_comment"] = "Python debugpy settings";
    pythonSettings["justMyCode"] = true;
    pythonSettings["showReturnValue"] = true;
    pythonSettings["console"] = "integratedTerminal";
    adapters["python-debugpy"] = pythonSettings;
    
    // Node.js adapter settings
    QJsonObject nodeSettings;
    nodeSettings["_comment"] = "Node.js debug adapter settings";
    nodeSettings["console"] = "integratedTerminal";
    nodeSettings["sourceMaps"] = true;
    adapters["node-debug"] = nodeSettings;
    
    config["adapters"] = adapters;
    
    // Remote debugging settings
    QJsonObject remoteSettings;
    remoteSettings["_comment"] = "Remote debugging configuration";
    remoteSettings["defaultHost"] = "localhost";
    remoteSettings["defaultPort"] = 0;
    config["remote"] = remoteSettings;
    
    writeJsonFile(adaptersConfigPath(), config);
    LOG_INFO("Created default adapters.json");
}

void DebugSettings::createDefaultSettingsConfig()
{
    QJsonObject config;
    config["version"] = "1.0.0";
    config["_comment"] = "General debugger settings. All options are user-configurable.";
    
    // General preferences
    QJsonObject general;
    general["stopOnEntry"] = false;
    general["externalConsole"] = false;
    general["justMyCode"] = true;
    general["showReturnValue"] = true;
    general["autoReload"] = true;
    general["confirmOnExit"] = true;
    general["openDebugConsoleOnStart"] = false;
    general["focusEditorOnBreak"] = true;
    general["inlineValues"] = true;
    general["showHexValues"] = false;
    config["general"] = general;
    
    // Logging settings
    QJsonObject logging;
    logging["enabled"] = false;
    logging["logFilePath"] = "${workspaceFolder}/.lightpad/debug/debug.log";
    logging["verbosity"] = "info";
    config["logging"] = logging;
    
    // UI preferences
    QJsonObject ui;
    ui["showVariablesPanel"] = true;
    ui["showCallStackPanel"] = true;
    ui["showBreakpointsPanel"] = true;
    ui["showWatchPanel"] = true;
    ui["showDebugConsole"] = true;
    ui["variablesSortOrder"] = "alphabetical";
    ui["expandArraysLength"] = 100;
    ui["expandObjectDepth"] = 3;
    config["ui"] = ui;
    
    // Timeout settings
    QJsonObject timeouts;
    timeouts["_comment"] = "Timeout values in milliseconds";
    timeouts["launchTimeout"] = 10000;
    timeouts["attachTimeout"] = 10000;
    timeouts["evaluateTimeout"] = 5000;
    timeouts["disconnectTimeout"] = 3000;
    config["timeouts"] = timeouts;
    
    // Exception handling
    QJsonObject exceptions;
    exceptions["_comment"] = "Exception breakpoint default settings";
    exceptions["breakOnUncaught"] = true;
    exceptions["breakOnRaised"] = false;
    exceptions["breakOnUserUnhandled"] = false;
    config["exceptions"] = exceptions;
    
    // Source map settings
    QJsonObject sourceMaps;
    sourceMaps["enabled"] = true;
    sourceMaps["pathOverrides"] = QJsonObject();
    config["sourceMaps"] = sourceMaps;
    
    writeJsonFile(settingsConfigPath(), config);
    LOG_INFO("Created default settings.json");
}

void DebugSettings::loadAll()
{
    m_generalSettings = readJsonFile(settingsConfigPath());
    m_adapterSettings = readJsonFile(adaptersConfigPath());
    
    emit settingsChanged();
}

void DebugSettings::saveAll()
{
    writeJsonFile(settingsConfigPath(), m_generalSettings);
    writeJsonFile(adaptersConfigPath(), m_adapterSettings);
}

void DebugSettings::reload()
{
    loadAll();
    emit settingsChanged();
    emit launchConfigChanged();
    emit breakpointsChanged();
    emit watchesChanged();
    emit adapterSettingsChanged();
}

QJsonObject DebugSettings::generalSettings() const
{
    return m_generalSettings;
}

void DebugSettings::setGeneralSetting(const QString& key, const QJsonValue& value)
{
    QJsonObject general = m_generalSettings["general"].toObject();
    general[key] = value;
    m_generalSettings["general"] = general;
    writeJsonFile(settingsConfigPath(), m_generalSettings);
    emit settingsChanged();
}

QJsonObject DebugSettings::adapterSettings() const
{
    return m_adapterSettings;
}

void DebugSettings::setAdapterSettings(const QString& adapterId, const QJsonObject& settings)
{
    QJsonObject adapters = m_adapterSettings["adapters"].toObject();
    adapters[adapterId] = settings;
    m_adapterSettings["adapters"] = adapters;
    writeJsonFile(adaptersConfigPath(), m_adapterSettings);
    emit adapterSettingsChanged();
}

QString DebugSettings::defaultAdapterForExtension(const QString& extension) const
{
    QJsonObject defaults = m_adapterSettings["defaultAdapters"].toObject();
    return defaults[extension].toString();
}

void DebugSettings::setDefaultAdapterForExtension(const QString& extension, const QString& adapterId)
{
    QJsonObject defaults = m_adapterSettings["defaultAdapters"].toObject();
    defaults[extension] = adapterId;
    m_adapterSettings["defaultAdapters"] = defaults;
    writeJsonFile(adaptersConfigPath(), m_adapterSettings);
    emit adapterSettingsChanged();
}

bool DebugSettings::writeJsonFile(const QString& path, const QJsonObject& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Failed to write debug config: %1").arg(path));
        return false;
    }
    
    QJsonDocument doc(content);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

QJsonObject DebugSettings::readJsonFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARNING(QString("Could not read debug config: %1").arg(path));
        return QJsonObject();
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("JSON parse error in %1: %2").arg(path, error.errorString()));
        return QJsonObject();
    }
    
    return doc.object();
}
