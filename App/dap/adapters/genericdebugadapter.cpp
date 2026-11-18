#include "genericdebugadapter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace {

QStringList stringList(const QJsonValue &value) {
  if (value.isString()) {
    return {value.toString()};
  }
  QStringList out;
  for (const QJsonValue &item : value.toArray()) {
    if (!item.toString().isEmpty()) {
      out << item.toString();
    }
  }
  return out;
}

QJsonObject readJsonObject(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  return doc.isObject() ? doc.object() : QJsonObject();
}

// Replaces ${file}, ${fileDirname}/${fileDir}, ${fileBasenameNoExtension}
// and ${cwd} in the string values of a launch template; ${workspaceFolder}
// and anything else stay for the launch-time resolver.
QJsonValue substitute(const QJsonValue &value,
                      const QMap<QString, QString> &variables) {
  if (value.isString()) {
    QString text = value.toString();
    for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
      text.replace(QStringLiteral("${%1}").arg(it.key()), it.value());
    }
    return text;
  }
  if (value.isArray()) {
    QJsonArray out;
    for (const QJsonValue &item : value.toArray()) {
      out.append(substitute(item, variables));
    }
    return out;
  }
  if (value.isObject()) {
    QJsonObject out;
    const QJsonObject obj = value.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      out.insert(it.key(), substitute(it.value(), variables));
    }
    return out;
  }
  return value;
}

} // namespace

DebugAdapterDefinition DebugAdapterDefinition::fromJson(const QJsonObject &o) {
  DebugAdapterDefinition d;
  d.id = o.value("id").toString().trimmed();
  d.name = o.value("name").toString(d.id);
  d.type = o.value("type").toString().trimmed();
  d.languages = stringList(o.value("languages"));
  for (const QString &extension : stringList(o.value("extensions"))) {
    d.extensions << (extension.startsWith('.') ? extension
                                               : QLatin1Char('.') + extension);
  }
  d.commands = stringList(o.value("command"));
  d.arguments = stringList(o.value("arguments"));
  d.probeArguments = stringList(o.value("probe"));
  d.serverTransport =
      o.value("transport").toString() == QLatin1String("server");
  d.install = o.value("install").toString();
  d.docs = o.value("docs").toString();
  d.launchDefaults = o.value("launch").toObject();
  d.attachDefaults = o.value("attach").toObject();
  d.processIdKey = o.value("processIdKey").toString(d.processIdKey);
  d.exceptionBreakpointFilters = o.value("exceptionFilters").toArray();
  const QJsonObject supports = o.value("supports").toObject();
  d.supportsFunctionBreakpoints =
      supports.value("functionBreakpoints").toBool(true);
  d.supportsConditionalBreakpoints =
      supports.value("conditionalBreakpoints").toBool(true);
  d.supportsHitConditionalBreakpoints =
      supports.value("hitConditionalBreakpoints").toBool(true);
  d.supportsLogPoints = supports.value("logPoints").toBool(true);
  d.supportsRestart = supports.value("restart").toBool(false);
  const QJsonObject run = o.value("run").toObject();
  d.runProgram = run.value("program").toString();
  d.runArguments = stringList(run.value("args"));
  return d;
}

GenericDebugAdapter::GenericDebugAdapter(DebugAdapterDefinition definition)
    : ProcessBackedDebugAdapter(
          baseConfigFor(definition), QStringLiteral("adapterPath"),
          QStringLiteral("Adapter Executable"),
          definition.commands.value(0) + QStringLiteral(" or /path/to/it")),
      m_definition(std::move(definition)) {}

DebugAdapterConfig
GenericDebugAdapter::baseConfigFor(const DebugAdapterDefinition &d) {
  DebugAdapterConfig cfg;
  cfg.id = d.id;
  cfg.name = d.name;
  cfg.type = d.type;
  cfg.program = d.commands.value(0);
  cfg.arguments = d.arguments;
  cfg.serverTransport = d.serverTransport;
  cfg.languages = d.languages;
  cfg.extensions = d.extensions;
  cfg.defaultLaunchConfig = d.launchDefaults;
  cfg.defaultAttachConfig = d.attachDefaults;
  cfg.exceptionBreakpointFilters = d.exceptionBreakpointFilters;
  cfg.supportsRestart = d.supportsRestart;
  cfg.supportsFunctionBreakpoints = d.supportsFunctionBreakpoints;
  cfg.supportsConditionalBreakpoints = d.supportsConditionalBreakpoints;
  cfg.supportsHitConditionalBreakpoints = d.supportsHitConditionalBreakpoints;
  cfg.supportsLogPoints = d.supportsLogPoints;
  return cfg;
}

DebugAdapterConfig GenericDebugAdapter::configForConfiguration(
    const DebugConfiguration &configuration) const {
  DebugAdapterConfig cfg =
      ProcessBackedDebugAdapter::configForConfiguration(configuration);
  cfg.arguments = m_definition.arguments;
  cfg.serverTransport = m_definition.serverTransport;
  return cfg;
}

QString GenericDebugAdapter::defaultAdapterExecutableCandidate(
    const DebugConfiguration &configuration) const {
  Q_UNUSED(configuration);
  for (const QString &command : m_definition.commands) {
    if (!resolveExecutablePath(command).isEmpty()) {
      return command;
    }
  }
  return m_definition.commands.value(0);
}

QStringList GenericDebugAdapter::executableProbeArguments(
    const DebugConfiguration &configuration) const {
  Q_UNUSED(configuration);
  return m_definition.probeArguments;
}

QString GenericDebugAdapter::missingExecutableMessage(
    const DebugConfiguration &configuration, const QString &candidate) const {
  Q_UNUSED(configuration);
  QString message =
      QStringLiteral("%1 was not found (looked for %2).")
          .arg(m_definition.name,
               m_definition.commands.isEmpty()
                   ? candidate
                   : m_definition.commands.join(QStringLiteral(", ")));
  if (!m_definition.install.isEmpty()) {
    message += QLatin1Char(' ') + m_definition.install;
  }
  return message;
}

QJsonObject
GenericDebugAdapter::createLaunchConfig(const QString &filePath,
                                        const QString &workingDir) const {
  const QFileInfo info(filePath);
  QMap<QString, QString> variables;
  if (!filePath.isEmpty()) {
    variables.insert(QStringLiteral("file"), filePath);
    variables.insert(QStringLiteral("fileDirname"), info.absolutePath());
    variables.insert(QStringLiteral("fileDir"), info.absolutePath());
    variables.insert(QStringLiteral("fileBasenameNoExtension"),
                     info.completeBaseName());
  }
  const QString cwd = workingDir.isEmpty() && !filePath.isEmpty()
                          ? info.absolutePath()
                          : workingDir;
  if (!cwd.isEmpty()) {
    variables.insert(QStringLiteral("cwd"), cwd);
  }
  QJsonObject config =
      substitute(m_definition.launchDefaults, variables).toObject();
  config["type"] = m_definition.type;
  config["request"] = QStringLiteral("launch");
  if (!config.contains("program") && !filePath.isEmpty()) {
    config["program"] = filePath;
  }
  if (!config.contains("cwd") && !cwd.isEmpty()) {
    config["cwd"] = cwd;
  }
  return config;
}

QJsonObject GenericDebugAdapter::createAttachConfig(int processId,
                                                    const QString &host,
                                                    int port) const {
  QJsonObject config = m_definition.attachDefaults;
  config["type"] = m_definition.type;
  config["request"] = QStringLiteral("attach");
  if (processId > 0) {
    config[m_definition.processIdKey] = processId;
  } else if (port > 0) {
    config["host"] = host.isEmpty() ? QStringLiteral("localhost") : host;
    config["port"] = port;
  }
  return config;
}

QJsonObject GenericDebugAdapter::attachArguments(
    const DebugConfiguration &configuration) const {
  QJsonObject arguments =
      ProcessBackedDebugAdapter::attachArguments(configuration);
  const QString key = m_definition.processIdKey;
  if (key != QLatin1String("processId") && arguments.contains("processId")) {
    const QJsonValue processId = arguments.take("processId");
    if (!arguments.contains(key)) {
      arguments.insert(key, processId);
    }
  }
  return arguments;
}

RunCommand
GenericDebugAdapter::runCommand(const DebugConfiguration &configuration) const {
  RunCommand command = AbstractDebugAdapter::runCommand(configuration);
  if (m_definition.runProgram.isEmpty() || !command.error.isEmpty() ||
      configuration.request.compare(QLatin1String("attach"),
                                    Qt::CaseInsensitive) == 0) {
    return command;
  }
  auto expand = [&](const QString &text) {
    QString out = text;
    out.replace(QLatin1String("${program}"), configuration.program);
    out.replace(QLatin1String("${cwd}"), configuration.cwd);
    return out;
  };
  command.program = expand(m_definition.runProgram);
  command.arguments.clear();
  for (const QString &argument : m_definition.runArguments) {
    if (argument == QLatin1String("${args}")) {
      command.arguments << configuration.args;
    } else {
      command.arguments << expand(argument);
    }
  }
  return command;
}

QList<DebugAdapterDefinition>
GenericDebugAdapter::parse(const QJsonObject &root) {
  QList<DebugAdapterDefinition> out;
  for (const QJsonValue &value : root.value("adapters").toArray()) {
    const DebugAdapterDefinition definition =
        DebugAdapterDefinition::fromJson(value.toObject());
    if (definition.isValid()) {
      out.append(definition);
    }
  }
  return out;
}

QString GenericDebugAdapter::userDefinitionsPath() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::AppConfigLocation))
      .filePath(QStringLiteral("debug_adapters.json"));
}

QList<std::shared_ptr<IDebugAdapter>> GenericDebugAdapter::loadAll() {
  QList<std::shared_ptr<IDebugAdapter>> adapters;
  for (const QString &path :
       {QStringLiteral(":/dap/debug_adapters.json"), userDefinitionsPath()}) {
    for (const DebugAdapterDefinition &definition :
         parse(readJsonObject(path))) {
      adapters.append(std::make_shared<GenericDebugAdapter>(definition));
    }
  }
  return adapters;
}
