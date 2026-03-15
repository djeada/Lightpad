#ifndef ABSTRACTDEBUGADAPTER_H
#define ABSTRACTDEBUGADAPTER_H

#include "../idebugadapter.h"
#include "adapterutils.h"

class AbstractDebugAdapter : public IDebugAdapter {
public:
  explicit AbstractDebugAdapter(DebugAdapterConfig baseConfig,
                                QList<DebugAdapterOption> options = {})
      : m_baseConfig(std::move(baseConfig)),
        m_configurationOptions(std::move(options)) {}

  AbstractDebugAdapter(
      DebugAdapterConfig baseConfig, QString runtimeOverrideKey,
      QString runtimeOverrideLabel = QStringLiteral("Runtime Override"),
      QString runtimeOverridePlaceholder = {})
      : m_baseConfig(std::move(baseConfig)),
        m_configurationOptions(buildSingleOption(
            std::move(runtimeOverrideKey), std::move(runtimeOverrideLabel),
            std::move(runtimeOverridePlaceholder))) {}

  DebugAdapterConfig config() const override { return m_baseConfig; }

  DebugAdapterConfig configForConfiguration(
      const DebugConfiguration &configuration) const override {
    DebugAdapterConfig cfg = m_baseConfig;
    const QString overrideKey = runtimeOverrideKey();
    if (!overrideKey.isEmpty()) {
      const QString overrideValue =
          mergedAdapterConfig(configuration)[overrideKey].toString().trimmed();
      if (!overrideValue.isEmpty()) {
        cfg.program = overrideValue;
      }
    }
    return cfg;
  }

  QList<DebugAdapterOption> configurationOptions() const override {
    return m_configurationOptions;
  }

  QJsonObject
  launchArguments(const DebugConfiguration &configuration) const override {
    QJsonObject arguments =
        createLaunchConfig(configuration.program, configuration.cwd);
    const QJsonObject adapterConfig = mergedAdapterConfig(configuration);

    for (auto it = adapterConfig.begin(); it != adapterConfig.end(); ++it) {
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

  QJsonObject
  attachArguments(const DebugConfiguration &configuration) const override {
    QJsonObject arguments = createAttachConfig(
        configuration.processId, configuration.host, configuration.port);
    const QJsonObject adapterConfig = mergedAdapterConfig(configuration);

    for (auto it = adapterConfig.begin(); it != adapterConfig.end(); ++it) {
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

protected:
  DebugAdapterConfig &baseConfig() { return m_baseConfig; }
  const DebugAdapterConfig &baseConfig() const { return m_baseConfig; }
  QJsonObject
  mergedAdapterConfig(const DebugConfiguration &configuration) const {
    QJsonObject merged = debugAdapterSettingsObject(m_baseConfig.id);
    for (auto it = configuration.adapterConfig.begin();
         it != configuration.adapterConfig.end(); ++it) {
      merged[it.key()] = it.value();
    }
    return merged;
  }

private:
  static QList<DebugAdapterOption> buildSingleOption(QString key, QString label,
                                                     QString placeholder) {
    if (key.isEmpty()) {
      return {};
    }
    DebugAdapterOption option;
    option.key = std::move(key);
    option.label = std::move(label);
    option.placeholder = std::move(placeholder);
    return {option};
  }

  DebugAdapterConfig m_baseConfig;
  QList<DebugAdapterOption> m_configurationOptions;
};

#endif
