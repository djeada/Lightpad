#ifndef IDEBUGADAPTER_H
#define IDEBUGADAPTER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include "debugconfiguration.h"

struct DebugAdapterConfig {
  QString id;
  QString name;
  QString type;
  QString program;
  QStringList arguments;
  QStringList languages;

  QStringList extensions;

  QJsonObject defaultLaunchConfig;
  QJsonObject defaultAttachConfig;

  QJsonArray exceptionBreakpointFilters;

  bool supportsRestart = false;
  bool supportsTerminate = true;
  bool supportsFunctionBreakpoints = false;
  bool supportsConditionalBreakpoints = true;
  bool supportsHitConditionalBreakpoints = true;
  bool supportsLogPoints = true;
};

struct DebugAdapterOption {
  QString key;
  QString label;
  QString placeholder;
  bool browseable = true;
};

class IDebugAdapter {
public:
  virtual ~IDebugAdapter() = default;

  virtual DebugAdapterConfig config() const = 0;

  virtual DebugAdapterConfig
  configForContext(const QString &filePath = {},
                   const QString &workingDir = {}) const {
    (void)filePath;
    (void)workingDir;
    return config();
  }

  virtual DebugAdapterConfig
  configForConfiguration(const DebugConfiguration &configuration) const {
    return configForContext(configuration.program, configuration.cwd);
  }

  virtual bool isAvailable() const = 0;

  virtual bool isAvailableForContext(const QString &filePath = {},
                                     const QString &workingDir = {}) const {
    (void)filePath;
    (void)workingDir;
    return isAvailable();
  }

  virtual bool
  isAvailableForConfiguration(const DebugConfiguration &configuration) const {
    return isAvailableForContext(configuration.program, configuration.cwd);
  }

  virtual QString statusMessage() const = 0;

  virtual QString
  statusMessageForContext(const QString &filePath = {},
                          const QString &workingDir = {}) const {
    (void)filePath;
    (void)workingDir;
    return statusMessage();
  }

  virtual QString
  statusMessageForConfiguration(const DebugConfiguration &configuration) const {
    return statusMessageForContext(configuration.program, configuration.cwd);
  }

  virtual bool
  matchesConfiguration(const DebugConfiguration &configuration) const {
    if (!configuration.adapterId.trimmed().isEmpty()) {
      return config().id.compare(configuration.adapterId.trimmed(),
                                 Qt::CaseInsensitive) == 0;
    }
    return config().type.compare(configuration.type.trimmed(),
                                 Qt::CaseInsensitive) == 0;
  }

  virtual QJsonObject
  createLaunchConfig(const QString &filePath,
                     const QString &workingDir = {}) const = 0;

  virtual QJsonObject
  launchArguments(const DebugConfiguration &configuration) const {
    QJsonObject arguments =
        createLaunchConfig(configuration.program, configuration.cwd);

    for (auto it = configuration.adapterConfig.begin();
         it != configuration.adapterConfig.end(); ++it) {
      arguments[it.key()] = it.value();
    }

    if (!configuration.program.isEmpty()) {
      arguments["program"] = configuration.program;
    }

    if (!configuration.args.isEmpty()) {
      QJsonArray argsArray;
      for (const QString &arg : configuration.args) {
        argsArray.append(arg);
      }
      arguments["args"] = argsArray;
    }

    if (!configuration.cwd.isEmpty()) {
      arguments["cwd"] = configuration.cwd;
    }

    if (!configuration.env.isEmpty()) {
      QJsonObject envObj;
      for (auto it = configuration.env.begin(); it != configuration.env.end();
           ++it) {
        envObj[it.key()] = it.value();
      }
      arguments["env"] = envObj;
    }

    arguments["stopOnEntry"] = configuration.stopOnEntry;
    arguments["stopAtBeginningOfMainSubprogram"] = configuration.stopOnEntry;
    return arguments;
  }

  virtual QJsonObject createAttachConfig(int processId = 0,
                                         const QString &host = {},
                                         int port = 0) const = 0;

  virtual QJsonObject
  attachArguments(const DebugConfiguration &configuration) const {
    QJsonObject arguments = createAttachConfig(
        configuration.processId, configuration.host, configuration.port);

    for (auto it = configuration.adapterConfig.begin();
         it != configuration.adapterConfig.end(); ++it) {
      arguments[it.key()] = it.value();
    }

    if (configuration.processId > 0) {
      arguments["processId"] = configuration.processId;
    } else if (!configuration.processIdExpression.isEmpty() &&
               !arguments.contains("processId")) {
      arguments["processId"] = configuration.processIdExpression;
    } else if (!configuration.host.isEmpty() &&
               !arguments.contains("connect") &&
               !arguments.contains("miDebuggerServerAddress") &&
               !arguments.contains("address")) {
      arguments["host"] = configuration.host;
      if (configuration.port > 0) {
        arguments["port"] = configuration.port;
      }
    } else if (configuration.port > 0 && !arguments.contains("port") &&
               !arguments.contains("connect") &&
               !arguments.contains("miDebuggerServerAddress")) {
      arguments["port"] = configuration.port;
    }

    if (!configuration.program.isEmpty() && !arguments.contains("program")) {
      arguments["program"] = configuration.program;
    }

    if (!configuration.cwd.isEmpty() && !arguments.contains("cwd")) {
      arguments["cwd"] = configuration.cwd;
    }

    if (!configuration.env.isEmpty() && !arguments.contains("env")) {
      QJsonObject envObj;
      for (auto it = configuration.env.begin(); it != configuration.env.end();
           ++it) {
        envObj[it.key()] = it.value();
      }
      arguments["env"] = envObj;
    }

    return arguments;
  }

  virtual bool canDebug(const QString &filePath) const {
    const DebugAdapterConfig &cfg = config();
    for (const QString &ext : cfg.extensions) {
      if (filePath.endsWith(ext, Qt::CaseInsensitive)) {
        return true;
      }
    }
    return false;
  }

  virtual bool supportsLanguage(const QString &languageId) const {
    return config().languages.contains(languageId, Qt::CaseInsensitive);
  }

  virtual QList<DebugAdapterOption> configurationOptions() const { return {}; }

  virtual QString installCommand() const { return {}; }

  virtual QString documentationUrl() const { return {}; }

  virtual QString runtimeOverrideKey() const {
    const QList<DebugAdapterOption> options = configurationOptions();
    return options.size() == 1 ? options.first().key : QString();
  }

  virtual QString runtimeOverrideLabel() const {
    const QList<DebugAdapterOption> options = configurationOptions();
    return options.size() == 1 ? options.first().label
                               : QStringLiteral("Runtime Override");
  }

  virtual QString runtimeOverridePlaceholder() const {
    const QList<DebugAdapterOption> options = configurationOptions();
    return options.size() == 1 ? options.first().placeholder : QString();
  }
};

#define IDebugAdapter_iid "org.lightpad.IDebugAdapter/1.0"
Q_DECLARE_INTERFACE(IDebugAdapter, IDebugAdapter_iid)

#endif
