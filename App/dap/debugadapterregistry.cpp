#include "debugadapterregistry.h"
#include "../core/logging/logger.h"

#include <QProcess>
#include <QFileInfo>
#include <QStandardPaths>

// ============================================================================
// Built-in Debug Adapters
// ============================================================================

/**
 * @brief Python debug adapter using debugpy
 */
class PythonDebugAdapter : public IDebugAdapter {
public:
    DebugAdapterConfig config() const override {
        DebugAdapterConfig cfg;
        cfg.id = "python-debugpy";
        cfg.name = "Python (debugpy)";
        cfg.type = "debugpy";
        cfg.program = "python";
        cfg.arguments = QStringList() << "-m" << "debugpy.adapter";
        cfg.languages = QStringList() << "python";
        cfg.extensions = QStringList() << ".py" << ".pyw";
        cfg.supportsRestart = true;
        cfg.supportsFunctionBreakpoints = true;
        cfg.supportsConditionalBreakpoints = true;
        cfg.supportsHitConditionalBreakpoints = true;
        cfg.supportsLogPoints = true;
        
        // Exception breakpoint filters
        QJsonArray filters;
        QJsonObject raised;
        raised["filter"] = "raised";
        raised["label"] = "Raised Exceptions";
        raised["default"] = false;
        filters.append(raised);
        
        QJsonObject uncaught;
        uncaught["filter"] = "uncaught";
        uncaught["label"] = "Uncaught Exceptions";
        uncaught["default"] = true;
        filters.append(uncaught);
        
        cfg.exceptionBreakpointFilters = filters;
        
        return cfg;
    }
    
    bool isAvailable() const override {
        // Check if debugpy is installed
        QProcess proc;
        proc.start("python", {"-c", "import debugpy; print('ok')"});
        if (!proc.waitForFinished(5000)) {
            return false;
        }
        return proc.exitCode() == 0;
    }
    
    QString statusMessage() const override {
        if (isAvailable()) {
            return "Ready";
        }
        return "debugpy not installed. Run: pip install debugpy";
    }
    
    QJsonObject createLaunchConfig(const QString& filePath,
                                   const QString& workingDir) const override {
        QJsonObject config;
        config["type"] = "debugpy";
        config["request"] = "launch";
        config["program"] = filePath;
        config["console"] = "integratedTerminal";
        
        if (!workingDir.isEmpty()) {
            config["cwd"] = workingDir;
        } else {
            QFileInfo fi(filePath);
            config["cwd"] = fi.absolutePath();
        }
        
        return config;
    }
    
    QJsonObject createAttachConfig(int processId, const QString& host,
                                   int port) const override {
        QJsonObject config;
        config["type"] = "debugpy";
        config["request"] = "attach";
        
        if (processId > 0) {
            config["processId"] = processId;
        } else if (!host.isEmpty() && port > 0) {
            QJsonObject connect;
            connect["host"] = host;
            connect["port"] = port;
            config["connect"] = connect;
        }
        
        return config;
    }
    
    QString installCommand() const override {
        return "pip install debugpy";
    }
    
    QString documentationUrl() const override {
        return "https://github.com/microsoft/debugpy";
    }
};

/**
 * @brief Node.js debug adapter (built into Node.js)
 */
class NodeDebugAdapter : public IDebugAdapter {
public:
    DebugAdapterConfig config() const override {
        DebugAdapterConfig cfg;
        cfg.id = "node-debug";
        cfg.name = "Node.js";
        cfg.type = "node";
        cfg.program = "node";
        cfg.arguments = QStringList() << "--inspect-brk";
        cfg.languages = QStringList() << "javascript" << "typescript";
        cfg.extensions = QStringList() << ".js" << ".mjs" << ".cjs" << ".ts" << ".mts";
        cfg.supportsRestart = true;
        cfg.supportsFunctionBreakpoints = false;
        cfg.supportsConditionalBreakpoints = true;
        cfg.supportsLogPoints = true;
        
        return cfg;
    }
    
    bool isAvailable() const override {
        QProcess proc;
        proc.start("node", {"--version"});
        if (!proc.waitForFinished(5000)) {
            return false;
        }
        return proc.exitCode() == 0;
    }
    
    QString statusMessage() const override {
        if (isAvailable()) {
            QProcess proc;
            proc.start("node", {"--version"});
            proc.waitForFinished(5000);
            QString version = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            return QString("Ready (%1)").arg(version);
        }
        return "Node.js not installed";
    }
    
    QJsonObject createLaunchConfig(const QString& filePath,
                                   const QString& workingDir) const override {
        QJsonObject config;
        config["type"] = "node";
        config["request"] = "launch";
        config["program"] = filePath;
        config["console"] = "integratedTerminal";
        
        if (!workingDir.isEmpty()) {
            config["cwd"] = workingDir;
        } else {
            QFileInfo fi(filePath);
            config["cwd"] = fi.absolutePath();
        }
        
        return config;
    }
    
    QJsonObject createAttachConfig(int processId, const QString& host,
                                   int port) const override {
        QJsonObject config;
        config["type"] = "node";
        config["request"] = "attach";
        
        if (processId > 0) {
            config["processId"] = processId;
        } else {
            config["address"] = host.isEmpty() ? "127.0.0.1" : host;
            config["port"] = port > 0 ? port : 9229;
        }
        
        return config;
    }
    
    QString documentationUrl() const override {
        return "https://nodejs.org/en/docs/guides/debugging-getting-started/";
    }
};

/**
 * @brief GDB debug adapter for C/C++
 */
class GdbDebugAdapter : public IDebugAdapter {
public:
    DebugAdapterConfig config() const override {
        DebugAdapterConfig cfg;
        cfg.id = "cppdbg-gdb";
        cfg.name = "C/C++ (GDB)";
        cfg.type = "cppdbg";
        cfg.program = findGdbAdapter();
        cfg.languages = QStringList() << "cpp" << "c";
        cfg.extensions = QStringList() << ".cpp" << ".cxx" << ".cc" << ".c" << ".h" << ".hpp";
        cfg.supportsRestart = false;
        cfg.supportsFunctionBreakpoints = true;
        cfg.supportsConditionalBreakpoints = true;
        cfg.supportsHitConditionalBreakpoints = true;
        cfg.supportsLogPoints = false;
        
        return cfg;
    }
    
    bool isAvailable() const override {
        // Check for GDB
        QProcess proc;
        proc.start("gdb", {"--version"});
        if (!proc.waitForFinished(5000)) {
            return false;
        }
        return proc.exitCode() == 0;
    }
    
    QString statusMessage() const override {
        if (!isAvailable()) {
            return "GDB not installed";
        }
        
        QProcess proc;
        proc.start("gdb", {"--version"});
        proc.waitForFinished(5000);
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        QString firstLine = output.split('\n').first();
        return QString("Ready (%1)").arg(firstLine.simplified());
    }
    
    QJsonObject createLaunchConfig(const QString& filePath,
                                   const QString& workingDir) const override {
        QJsonObject config;
        config["type"] = "cppdbg";
        config["request"] = "launch";
        config["program"] = filePath;
        config["MIMode"] = "gdb";
        config["miDebuggerPath"] = "gdb";
        config["stopAtEntry"] = false;
        config["externalConsole"] = false;
        
        if (!workingDir.isEmpty()) {
            config["cwd"] = workingDir;
        } else {
            QFileInfo fi(filePath);
            config["cwd"] = fi.absolutePath();
        }
        
        return config;
    }
    
    QJsonObject createAttachConfig(int processId, const QString& host,
                                   int port) const override {
        Q_UNUSED(host);
        Q_UNUSED(port);
        
        QJsonObject config;
        config["type"] = "cppdbg";
        config["request"] = "attach";
        config["program"] = "";  // Will need to be filled in
        config["MIMode"] = "gdb";
        config["processId"] = processId > 0 ? QString::number(processId) : "${command:pickProcess}";
        
        return config;
    }
    
    QString installCommand() const override {
        return "sudo apt install gdb";
    }
    
    QString documentationUrl() const override {
        return "https://sourceware.org/gdb/current/onlinedocs/gdb/";
    }

private:
    QString findGdbAdapter() const {
        // Look for common DAP-compatible GDB adapters
        QStringList candidates = {
            "/usr/bin/gdb",
            "/usr/local/bin/gdb"
        };
        
        for (const QString& path : candidates) {
            if (QFileInfo::exists(path)) {
                return path;
            }
        }
        
        return "gdb";
    }
};

/**
 * @brief LLDB debug adapter for C/C++ (macOS/Linux)
 */
class LldbDebugAdapter : public IDebugAdapter {
public:
    DebugAdapterConfig config() const override {
        DebugAdapterConfig cfg;
        cfg.id = "cppdbg-lldb";
        cfg.name = "C/C++ (LLDB)";
        cfg.type = "cppdbg";
        cfg.program = "lldb-vscode";
        cfg.languages = QStringList() << "cpp" << "c";
        cfg.extensions = QStringList() << ".cpp" << ".cxx" << ".cc" << ".c" << ".h" << ".hpp";
        cfg.supportsRestart = false;
        cfg.supportsFunctionBreakpoints = true;
        cfg.supportsConditionalBreakpoints = true;
        cfg.supportsHitConditionalBreakpoints = true;
        cfg.supportsLogPoints = false;
        
        return cfg;
    }
    
    bool isAvailable() const override {
        // Check for lldb-vscode (LLDB's DAP adapter)
        QStringList candidates;
        candidates << "lldb-vscode" << "lldb-dap";
        for (const QString& name : candidates) {
            QProcess proc;
            proc.start(name, {"--help"});
            if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
                return true;
            }
        }
        return false;
    }
    
    QString statusMessage() const override {
        if (isAvailable()) {
            return "Ready";
        }
        return "lldb-vscode not found. Install LLDB with DAP support.";
    }
    
    QJsonObject createLaunchConfig(const QString& filePath,
                                   const QString& workingDir) const override {
        QJsonObject config;
        config["type"] = "lldb";
        config["request"] = "launch";
        config["program"] = filePath;
        config["stopOnEntry"] = false;
        
        if (!workingDir.isEmpty()) {
            config["cwd"] = workingDir;
        } else {
            QFileInfo fi(filePath);
            config["cwd"] = fi.absolutePath();
        }
        
        return config;
    }
    
    QJsonObject createAttachConfig(int processId, const QString& host,
                                   int port) const override {
        Q_UNUSED(host);
        Q_UNUSED(port);
        
        QJsonObject config;
        config["type"] = "lldb";
        config["request"] = "attach";
        config["pid"] = processId;
        
        return config;
    }
    
    QString documentationUrl() const override {
        return "https://lldb.llvm.org/";
    }
};

// ============================================================================
// DebugAdapterRegistry Implementation
// ============================================================================

DebugAdapterRegistry& DebugAdapterRegistry::instance()
{
    static DebugAdapterRegistry instance;
    return instance;
}

DebugAdapterRegistry::DebugAdapterRegistry()
    : QObject(nullptr)
{
    registerBuiltinAdapters();
}

void DebugAdapterRegistry::registerBuiltinAdapters()
{
    // Register built-in debug adapters
    registerAdapter(std::make_shared<PythonDebugAdapter>());
    registerAdapter(std::make_shared<NodeDebugAdapter>());
    registerAdapter(std::make_shared<GdbDebugAdapter>());
    registerAdapter(std::make_shared<LldbDebugAdapter>());
    
    LOG_INFO("Registered built-in debug adapters");
}

void DebugAdapterRegistry::registerAdapter(std::shared_ptr<IDebugAdapter> adapter)
{
    if (!adapter) {
        return;
    }
    
    QString id = adapter->config().id;
    m_adapters[id] = adapter;
    
    LOG_INFO(QString("Registered debug adapter: %1").arg(id));
    emit adapterRegistered(id);
}

void DebugAdapterRegistry::unregisterAdapter(const QString& adapterId)
{
    if (m_adapters.remove(adapterId) > 0) {
        LOG_INFO(QString("Unregistered debug adapter: %1").arg(adapterId));
        emit adapterUnregistered(adapterId);
    }
}

QList<std::shared_ptr<IDebugAdapter>> DebugAdapterRegistry::allAdapters() const
{
    return m_adapters.values();
}

QList<std::shared_ptr<IDebugAdapter>> DebugAdapterRegistry::availableAdapters() const
{
    QList<std::shared_ptr<IDebugAdapter>> result;
    for (const auto& adapter : m_adapters) {
        if (adapter->isAvailable()) {
            result.append(adapter);
        }
    }
    return result;
}

std::shared_ptr<IDebugAdapter> DebugAdapterRegistry::adapter(const QString& adapterId) const
{
    return m_adapters.value(adapterId);
}

QList<std::shared_ptr<IDebugAdapter>> DebugAdapterRegistry::adaptersForFile(
    const QString& filePath) const
{
    QList<std::shared_ptr<IDebugAdapter>> result;
    for (const auto& adapter : m_adapters) {
        if (adapter->canDebug(filePath)) {
            result.append(adapter);
        }
    }
    return result;
}

QList<std::shared_ptr<IDebugAdapter>> DebugAdapterRegistry::adaptersForLanguage(
    const QString& languageId) const
{
    QList<std::shared_ptr<IDebugAdapter>> result;
    for (const auto& adapter : m_adapters) {
        if (adapter->supportsLanguage(languageId)) {
            result.append(adapter);
        }
    }
    return result;
}

std::shared_ptr<IDebugAdapter> DebugAdapterRegistry::preferredAdapterForFile(
    const QString& filePath) const
{
    for (const auto& adapter : m_adapters) {
        if (adapter->canDebug(filePath) && adapter->isAvailable()) {
            return adapter;
        }
    }
    return nullptr;
}

void DebugAdapterRegistry::refreshAvailability()
{
    // Just emit the signal - adapters check availability on-demand
    emit availabilityChanged();
}
