#ifndef GENERICDEBUGADAPTER_H
#define GENERICDEBUGADAPTER_H

#include "processbackeddebugadapter.h"

#include <QJsonObject>
#include <memory>

// A debug adapter described by data instead of code: any program that speaks
// the Debug Adapter Protocol over stdio (or announces "listening at
// host:port") can be added in debug_adapters.json. The built-in file ships
// definitions for LLVM's lldb-dap, netcoredbg (C#/.NET), Dart, Flutter and
// OCaml's earlybird; users add or override entries (by id) in
// <AppConfigLocation>/debug_adapters.json.
//
//   {
//     "id": "netcoredbg", "name": "C# (netcoredbg)", "type": "coreclr",
//     "languages": ["csharp"], "extensions": [".cs"],
//     "command": ["netcoredbg"], "arguments": ["--interpreter=vscode"],
//     "probe": ["--version"],
//     "launch": {"program": "${workspaceFolder}/bin/Debug/app.dll"},
//     "run": {"program": "dotnet", "args": ["${program}", "${args}"]},
//     "install": "...", "docs": "https://..."
//   }
struct DebugAdapterDefinition {
  QString id;
  QString name;
  QString type;
  QStringList languages;
  QStringList extensions;
  // Executables to look for, in order; the first one on PATH is used.
  QStringList commands;
  QStringList arguments;
  QStringList probeArguments;
  bool serverTransport = false;
  QString install;
  QString docs;
  QJsonObject launchDefaults;
  QJsonObject attachDefaults;
  // Name of the process id in attach requests ("pid" for lldb-dap).
  QString processIdKey = QStringLiteral("processId");
  QJsonArray exceptionBreakpointFilters;
  bool supportsFunctionBreakpoints = true;
  bool supportsConditionalBreakpoints = true;
  bool supportsHitConditionalBreakpoints = true;
  bool supportsLogPoints = true;
  bool supportsRestart = false;
  // How Run starts a configuration of this type without the debugger.
  // ${program}, ${cwd} and ${args} (spread) are replaced.
  QString runProgram;
  QStringList runArguments;

  bool isValid() const {
    return !id.isEmpty() && !type.isEmpty() && !commands.isEmpty();
  }
  static DebugAdapterDefinition fromJson(const QJsonObject &obj);
};

class GenericDebugAdapter : public ProcessBackedDebugAdapter {
public:
  explicit GenericDebugAdapter(DebugAdapterDefinition definition);

  const DebugAdapterDefinition &definition() const { return m_definition; }

  DebugAdapterConfig configForConfiguration(
      const DebugConfiguration &configuration) const override;
  QJsonObject createLaunchConfig(const QString &filePath,
                                 const QString &workingDir) const override;
  QJsonObject createAttachConfig(int processId, const QString &host,
                                 int port) const override;
  QJsonObject
  attachArguments(const DebugConfiguration &configuration) const override;
  RunCommand runCommand(const DebugConfiguration &configuration) const override;
  QString installCommand() const override { return m_definition.install; }
  QString documentationUrl() const override { return m_definition.docs; }

  // Definitions from a debug_adapters.json document; invalid entries are
  // skipped.
  static QList<DebugAdapterDefinition> parse(const QJsonObject &root);
  // Built-in definitions followed by the user's, as adapters.
  static QList<std::shared_ptr<IDebugAdapter>> loadAll();
  static QString userDefinitionsPath();

protected:
  QString defaultAdapterExecutableCandidate(
      const DebugConfiguration &configuration) const override;
  QStringList executableProbeArguments(
      const DebugConfiguration &configuration) const override;
  QString missingExecutableMessage(const DebugConfiguration &configuration,
                                   const QString &candidate) const override;

private:
  static DebugAdapterConfig baseConfigFor(const DebugAdapterDefinition &d);
  DebugAdapterDefinition m_definition;
};

#endif
