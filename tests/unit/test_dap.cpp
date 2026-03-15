#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "dap/breakpointmanager.h"
#include "dap/dapclient.h"
#include "dap/debugadapterregistry.h"
#include "dap/debugconfiguration.h"
#include "dap/debugsession.h"
#include "dap/debugsettings.h"
#include "dap/expressiontranslator.h"
#include "dap/watchmanager.h"

class TestDap : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testDapClientInitialState();
  void testDapClientStateEnum();

  void testBreakpointManagerSingleton();
  void testAddBreakpoint();
  void testRemoveBreakpoint();
  void testToggleBreakpoint();
  void testBreakpointCondition();
  void testBreakpointLogpoint();
  void testBreakpointPersistence();
  void testBreakpointStructuredPersistence();
  void testClearBreakpoints();
  void testDataBreakpoints();
  void testExceptionBreakpoints();

  void testRegistrySingleton();
  void testBuiltinAdapters();
  void testPythonAdapterUsesExplicitInterpreterOverride();
  void testAdapterLookupByFile();
  void testAdapterLookupByLanguage();
  void testAdapterLookupByConfiguration();
  void testNodeAdapterRuntimeOverride();
  void testNodeAdapterMissingCommandStatus();
  void testGdbAdapterIntegration();
  void testGdbAdapterRuntimeOverride();

  void testDebugConfigurationToJson();
  void testDebugConfigurationFromJson();
  void testConfigurationVariableSubstitution();
  void testCompoundConfigurationLoading();
  void testQuickConfigPreservesAdapterDefaults();
  void testQuickConfigCreatedEvenWhenAdapterUnavailable();
  void testConfigurationManagerSingleton();

  void testWatchManagerSingleton();
  void testAddWatch();
  void testRemoveWatch();
  void testWatchPersistence();
  void testWatchUpdate();
  void testWatchEvaluationWithoutClient();

  void testDapClientEvaluateErrorSignal();
  void testDapClientVariableSetSignal();
  void testDapClientSetDataBreakpoints();
  void testExpressionTranslatorGdbConsolePlan();
  void testExpressionTranslatorLocalsFallbackRequest();

  void testDebugSessionState();
  void testSessionManagerSingleton();
  void testSessionManagerReportsAdapterHints();

  void testDebugSettingsInitialization();
  void testDebugSettingsFilePaths();
  void testDebugConfigurationFileOpenPath();

  void testDapBreakpointFromJson();
  void testDapStackFrameFromJson();
  void testDapVariableFromJson();
  void testDapStoppedEventFromJson();

private:
  void cleanupBreakpoints();
  void cleanupWatches();
};

void TestDap::initTestCase() { cleanupBreakpoints(); }

void TestDap::cleanupTestCase() { cleanupBreakpoints(); }

void TestDap::cleanupBreakpoints() { BreakpointManager::instance().clearAll(); }

void TestDap::testDapClientInitialState() {
  DapClient client;

  QCOMPARE(client.state(), DapClient::State::Disconnected);
  QVERIFY(!client.isReady());
  QVERIFY(!client.isDebugging());
  QCOMPARE(client.currentThreadId(), 0);
}

void TestDap::testDapClientStateEnum() {

  QVERIFY(DapClient::State::Disconnected != DapClient::State::Connecting);
  QVERIFY(DapClient::State::Ready != DapClient::State::Running);
  QVERIFY(DapClient::State::Running != DapClient::State::Stopped);
  QVERIFY(DapClient::State::Stopped != DapClient::State::Terminated);
  QVERIFY(DapClient::State::Error != DapClient::State::Ready);
}

void TestDap::testBreakpointManagerSingleton() {
  BreakpointManager &bm1 = BreakpointManager::instance();
  BreakpointManager &bm2 = BreakpointManager::instance();

  QCOMPARE(&bm1, &bm2);
}

void TestDap::testAddBreakpoint() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();
  QSignalSpy addedSpy(&bm, &BreakpointManager::breakpointAdded);

  Breakpoint bp;
  bp.filePath = "/test/file.cpp";
  bp.line = 42;
  bp.enabled = true;

  int id = bm.addBreakpoint(bp);

  QVERIFY(id > 0);
  QCOMPARE(addedSpy.count(), 1);

  Breakpoint retrieved = bm.breakpoint(id);
  QCOMPARE(retrieved.filePath, bp.filePath);
  QCOMPARE(retrieved.line, bp.line);
  QCOMPARE(retrieved.enabled, bp.enabled);

  QVERIFY(bm.hasBreakpoint("/test/file.cpp", 42));
  QVERIFY(!bm.hasBreakpoint("/test/file.cpp", 43));
}

void TestDap::testRemoveBreakpoint() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  Breakpoint bp;
  bp.filePath = "/test/remove.cpp";
  bp.line = 10;

  int id = bm.addBreakpoint(bp);
  QVERIFY(bm.hasBreakpoint("/test/remove.cpp", 10));

  QSignalSpy removedSpy(&bm, &BreakpointManager::breakpointRemoved);
  bm.removeBreakpoint(id);

  QCOMPARE(removedSpy.count(), 1);
  QVERIFY(!bm.hasBreakpoint("/test/remove.cpp", 10));

  bm.removeBreakpoint(99999);
}

void TestDap::testToggleBreakpoint() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  QVERIFY(!bm.hasBreakpoint("/test/toggle.cpp", 5));
  bm.toggleBreakpoint("/test/toggle.cpp", 5);
  QVERIFY(bm.hasBreakpoint("/test/toggle.cpp", 5));

  bm.toggleBreakpoint("/test/toggle.cpp", 5);
  QVERIFY(!bm.hasBreakpoint("/test/toggle.cpp", 5));
}

void TestDap::testBreakpointCondition() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  Breakpoint bp;
  bp.filePath = "/test/condition.cpp";
  bp.line = 20;

  int id = bm.addBreakpoint(bp);

  QSignalSpy changedSpy(&bm, &BreakpointManager::breakpointChanged);
  bm.setCondition(id, "x > 10");

  QCOMPARE(changedSpy.count(), 1);

  Breakpoint updated = bm.breakpoint(id);
  QCOMPARE(updated.condition, QString("x > 10"));
  QVERIFY(!updated.isLogpoint);
}

void TestDap::testBreakpointLogpoint() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  Breakpoint bp;
  bp.filePath = "/test/logpoint.cpp";
  bp.line = 30;

  int id = bm.addBreakpoint(bp);
  bm.setLogMessage(id, "Value is {x}");

  Breakpoint updated = bm.breakpoint(id);
  QCOMPARE(updated.logMessage, QString("Value is {x}"));
  QVERIFY(updated.isLogpoint);

  bm.setLogMessage(id, "");
  updated = bm.breakpoint(id);
  QVERIFY(!updated.isLogpoint);
}

void TestDap::testBreakpointPersistence() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  Breakpoint bp1;
  bp1.filePath = "/test/persist1.cpp";
  bp1.line = 10;
  bp1.condition = "a == b";
  bm.addBreakpoint(bp1);

  Breakpoint bp2;
  bp2.filePath = "/test/persist2.cpp";
  bp2.line = 20;
  bp2.logMessage = "debug: {value}";
  bp2.isLogpoint = true;
  bm.addBreakpoint(bp2);

  QJsonObject json = bm.saveToJson();

  bm.clearAll();
  QCOMPARE(bm.allBreakpoints().count(), 0);

  bm.loadFromJson(json);

  QCOMPARE(bm.allBreakpoints().count(), 2);

  QList<Breakpoint> restored = bm.breakpointsForFile("/test/persist1.cpp");
  QCOMPARE(restored.count(), 1);
  QCOMPARE(restored.first().condition, QString("a == b"));
}

void TestDap::testBreakpointStructuredPersistence() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();
  QSignalSpy functionSpy(&bm, &BreakpointManager::functionBreakpointsChanged);
  QSignalSpy dataSpy(&bm, &BreakpointManager::dataBreakpointsChanged);
  QSignalSpy exceptionSpy(&bm, &BreakpointManager::exceptionBreakpointsChanged);

  QJsonObject json;
  QJsonObject sourceBreakpoints;

  QJsonArray mainBreakpoints;
  QJsonObject conditionalBreakpoint;
  conditionalBreakpoint["line"] = 12;
  conditionalBreakpoint["enabled"] = true;
  conditionalBreakpoint["condition"] = "count > 3";
  mainBreakpoints.append(conditionalBreakpoint);

  QJsonObject logpoint;
  logpoint["line"] = 18;
  logpoint["enabled"] = false;
  logpoint["logMessage"] = "count={count}";
  mainBreakpoints.append(logpoint);

  sourceBreakpoints["/test/structured.cpp"] = mainBreakpoints;
  json["sourceBreakpoints"] = sourceBreakpoints;

  QJsonArray functionBreakpoints;
  QJsonObject functionBreakpoint;
  functionBreakpoint["functionName"] = "std::vector<int>::push_back";
  functionBreakpoint["enabled"] = true;
  functionBreakpoints.append(functionBreakpoint);
  json["functionBreakpoints"] = functionBreakpoints;

  QJsonArray dataBreakpoints;
  QJsonObject dataBreakpoint;
  dataBreakpoint["dataId"] = "0xabc";
  dataBreakpoint["accessType"] = "write";
  dataBreakpoint["enabled"] = true;
  dataBreakpoints.append(dataBreakpoint);
  json["dataBreakpoints"] = dataBreakpoints;

  QJsonObject exceptionBreakpoints;
  exceptionBreakpoints["throw"] = true;
  exceptionBreakpoints["catch"] = false;
  json["exceptionBreakpoints"] = exceptionBreakpoints;

  bm.loadFromJson(json);

  QCOMPARE(bm.breakpointsForFile("/test/structured.cpp").count(), 2);
  const QList<Breakpoint> source = bm.breakpointsForFile("/test/structured.cpp");
  QCOMPARE(source.at(0).line, 12);
  QCOMPARE(source.at(0).condition, QString("count > 3"));
  QCOMPARE(source.at(1).line, 18);
  QVERIFY(!source.at(1).enabled);
  QVERIFY(source.at(1).isLogpoint);
  QCOMPARE(source.at(1).logMessage, QString("count={count}"));

  QCOMPARE(bm.allFunctionBreakpoints().count(), 1);
  QCOMPARE(bm.allFunctionBreakpoints().first().functionName,
           QString("std::vector<int>::push_back"));

  QCOMPARE(bm.allDataBreakpoints().count(), 1);
  QCOMPARE(bm.allDataBreakpoints().first().dataId, QString("0xabc"));

  QCOMPARE(bm.enabledExceptionFilters(), QStringList{"throw"});
  QCOMPARE(functionSpy.count(), 1);
  QCOMPARE(dataSpy.count(), 1);
  QCOMPARE(exceptionSpy.count(), 1);
}

void TestDap::testClearBreakpoints() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  Breakpoint bp1;
  bp1.filePath = "/test/clear1.cpp";
  bp1.line = 1;
  bm.addBreakpoint(bp1);

  Breakpoint bp2;
  bp2.filePath = "/test/clear1.cpp";
  bp2.line = 2;
  bm.addBreakpoint(bp2);

  Breakpoint bp3;
  bp3.filePath = "/test/clear2.cpp";
  bp3.line = 1;
  bm.addBreakpoint(bp3);

  QCOMPARE(bm.allBreakpoints().count(), 3);

  bm.clearFile("/test/clear1.cpp");
  QCOMPARE(bm.allBreakpoints().count(), 1);
  QCOMPARE(bm.breakpointsForFile("/test/clear1.cpp").count(), 0);
  QCOMPARE(bm.breakpointsForFile("/test/clear2.cpp").count(), 1);

  QSignalSpy clearedSpy(&bm, &BreakpointManager::allBreakpointsCleared);
  bm.clearAll();
  QCOMPARE(clearedSpy.count(), 1);
  QCOMPARE(bm.allBreakpoints().count(), 0);
}

void TestDap::testRegistrySingleton() {
  DebugAdapterRegistry &reg1 = DebugAdapterRegistry::instance();
  DebugAdapterRegistry &reg2 = DebugAdapterRegistry::instance();

  QCOMPARE(&reg1, &reg2);
}

void TestDap::testBuiltinAdapters() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  QList<std::shared_ptr<IDebugAdapter>> adapters = reg.allAdapters();

  QVERIFY(adapters.count() >= 4);

  auto pythonAdapter = reg.adapter("python-debugpy");
  QVERIFY(pythonAdapter != nullptr);
  QCOMPARE(pythonAdapter->config().name, QString("Python (debugpy)"));

  auto nodeAdapter = reg.adapter("node-debug");
  QVERIFY(nodeAdapter != nullptr);
  QVERIFY(nodeAdapter->supportsLanguage("js"));

  auto lldbAdapter = reg.adapter("cppdbg-lldb");
  QVERIFY(lldbAdapter != nullptr);
  QJsonObject lldbLaunchCfg =
      lldbAdapter->createLaunchConfig("/tmp/a.out", "/tmp");
  QCOMPARE(lldbLaunchCfg["type"].toString(), QString("cppdbg"));
}

void TestDap::testPythonAdapterUsesExplicitInterpreterOverride() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QDir temp(tempDir.path());
  QVERIFY(temp.mkpath("tools"));

  const QString fakePythonPath = temp.filePath("tools/python");
  QFile fakePython(fakePythonPath);
  QVERIFY(fakePython.open(QIODevice::WriteOnly | QIODevice::Text));
  fakePython.write("#!/bin/sh\nexit 0\n");
  fakePython.close();
  QVERIFY(QFile::setPermissions(
      fakePythonPath,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner |
          QFileDevice::ExeOwner | QFileDevice::ReadGroup |
          QFileDevice::ExeGroup | QFileDevice::ReadOther |
          QFileDevice::ExeOther));

  auto pythonAdapter =
      DebugAdapterRegistry::instance().adapter("python-debugpy");
  QVERIFY(pythonAdapter != nullptr);

  DebugConfiguration configuration;
  configuration.adapterId = "python-debugpy";
  configuration.type = "debugpy";
  configuration.request = "launch";
  configuration.program = "/tmp/example.py";
  configuration.cwd = tempDir.path();
  configuration.adapterConfig["python"] = fakePythonPath;

  QVERIFY(pythonAdapter->isAvailableForConfiguration(configuration));

  const DebugAdapterConfig cfg =
      pythonAdapter->configForConfiguration(configuration);
  QCOMPARE(cfg.program, fakePythonPath);
  QCOMPARE(pythonAdapter->runtimeOverrideKey(), QString("python"));
  QCOMPARE(pythonAdapter->statusMessageForConfiguration(configuration),
           QString("Ready (python)"));

  const QJsonObject launchConfig =
      pythonAdapter->createLaunchConfig("/tmp/example.py", tempDir.path());
  QCOMPARE(launchConfig["type"].toString(), QString("debugpy"));
}

void TestDap::testAdapterLookupByFile() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  QList<std::shared_ptr<IDebugAdapter>> pyAdapters =
      reg.adaptersForFile("/test/script.py");
  QVERIFY(!pyAdapters.isEmpty());
  QVERIFY(pyAdapters.first()->config().id.contains("python"));

  QList<std::shared_ptr<IDebugAdapter>> jsAdapters =
      reg.adaptersForFile("/test/app.js");
  QVERIFY(!jsAdapters.isEmpty());

  QList<std::shared_ptr<IDebugAdapter>> cppAdapters =
      reg.adaptersForFile("/test/main.cpp");
  QVERIFY(!cppAdapters.isEmpty());

  QList<std::shared_ptr<IDebugAdapter>> unknownAdapters =
      reg.adaptersForFile("/test/file.xyz");
  QVERIFY(unknownAdapters.isEmpty());
}

void TestDap::testAdapterLookupByLanguage() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  QList<std::shared_ptr<IDebugAdapter>> pythonAdapters =
      reg.adaptersForLanguage("py");
  QVERIFY(!pythonAdapters.isEmpty());

  QList<std::shared_ptr<IDebugAdapter>> cppAdapters =
      reg.adaptersForLanguage("cpp");
  QVERIFY(!cppAdapters.isEmpty());
}

void TestDap::testAdapterLookupByConfiguration() {
  DebugConfiguration configuration;
  configuration.adapterId = "cppdbg-gdb";
  configuration.type = "cppdbg";

  const auto adapters =
      DebugAdapterRegistry::instance().adaptersForConfiguration(configuration);
  QCOMPARE(adapters.size(), 1);
  QCOMPARE(adapters.first()->config().id, QString("cppdbg-gdb"));
}

void TestDap::testNodeAdapterRuntimeOverride() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  DebugSettings::instance().initialize(tempDir.path());

  const QString fakeAdapterPath = tempDir.filePath("js-debug-adapter");
  QFile fakeAdapter(fakeAdapterPath);
  QVERIFY(fakeAdapter.open(QIODevice::WriteOnly | QIODevice::Text));
  fakeAdapter.write("#!/bin/sh\necho js-debug-adapter help\nexit 0\n");
  fakeAdapter.close();
  QVERIFY(QFile::setPermissions(
      fakeAdapterPath,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner |
          QFileDevice::ExeOwner | QFileDevice::ReadGroup |
          QFileDevice::ExeGroup | QFileDevice::ReadOther |
          QFileDevice::ExeOther));

  const QString fakeNodePath = tempDir.filePath("node");
  QFile fakeNode(fakeNodePath);
  QVERIFY(fakeNode.open(QIODevice::WriteOnly | QIODevice::Text));
  fakeNode.write("#!/bin/sh\necho v20.11.0\nexit 0\n");
  fakeNode.close();
  QVERIFY(QFile::setPermissions(
      fakeNodePath,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner |
          QFileDevice::ExeOwner | QFileDevice::ReadGroup |
          QFileDevice::ExeGroup | QFileDevice::ReadOther |
          QFileDevice::ExeOther));

  QJsonObject nodeSettings =
      DebugSettings::instance().adapterSettings()["adapters"]
          .toObject()["node-debug"]
          .toObject();
  nodeSettings["adapterCommand"] = fakeAdapterPath;
  nodeSettings["runtimeExecutable"] = fakeNodePath;
  DebugSettings::instance().setAdapterSettings("node-debug", nodeSettings);

  auto nodeAdapter = DebugAdapterRegistry::instance().adapter("node-debug");
  QVERIFY(nodeAdapter != nullptr);

  DebugConfiguration configuration;
  configuration.adapterId = "node-debug";
  configuration.type = "node";
  configuration.request = "launch";
  configuration.program = "/workspace/app.js";
  configuration.adapterConfig["runtimeExecutable"] = fakeNodePath;
  configuration.adapterConfig["adapterCommand"] = fakeAdapterPath;

  const DebugAdapterConfig cfg =
      nodeAdapter->configForConfiguration(configuration);
  QCOMPARE(cfg.program, fakeAdapterPath);

  const QJsonObject launchArguments =
      nodeAdapter->launchArguments(configuration);
  QCOMPARE(launchArguments["program"].toString(), QString("/workspace/app.js"));
  QCOMPARE(launchArguments["runtimeExecutable"].toString(), fakeNodePath);
  QCOMPARE(launchArguments["console"].toString(),
           QString("integratedTerminal"));
  QVERIFY(launchArguments["sourceMaps"].toBool());

  QVERIFY(nodeAdapter->isAvailableForConfiguration(configuration));
  QVERIFY(nodeAdapter->statusMessageForConfiguration(configuration)
              .contains("Ready"));
}

void TestDap::testNodeAdapterMissingCommandStatus() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  DebugSettings::instance().initialize(tempDir.path());

  auto nodeAdapter = DebugAdapterRegistry::instance().adapter("node-debug");
  QVERIFY(nodeAdapter != nullptr);

  QJsonObject nodeSettings =
      DebugSettings::instance().adapterSettings()["adapters"]
          .toObject()["node-debug"]
          .toObject();
  nodeSettings["adapterCommand"] = "/definitely/missing/js-debug-adapter";
  nodeSettings["runtimeExecutable"] = "node";
  DebugSettings::instance().setAdapterSettings("node-debug", nodeSettings);

  DebugConfiguration configuration;
  configuration.adapterId = "node-debug";
  configuration.type = "node";
  configuration.request = "launch";
  configuration.program = "/workspace/app.js";

  QVERIFY(!nodeAdapter->isAvailableForConfiguration(configuration));
  QVERIFY(nodeAdapter->statusMessageForConfiguration(configuration)
              .contains("adapter", Qt::CaseInsensitive));
}

void TestDap::testGdbAdapterIntegration() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  auto gdbAdapter = reg.adapter("cppdbg-gdb");
  QVERIFY(gdbAdapter != nullptr);

  DebugAdapterConfig cfg = gdbAdapter->config();
  QCOMPARE(cfg.id, QString("cppdbg-gdb"));
  QCOMPARE(cfg.name, QString("C/C++ (GDB)"));
  QCOMPARE(cfg.type, QString("cppdbg"));
  QVERIFY(cfg.arguments.contains("--interpreter=dap"));

  QVERIFY(cfg.languages.contains("cpp"));
  QVERIFY(cfg.languages.contains("c"));

  QVERIFY(cfg.extensions.contains(".cpp"));
  QVERIFY(cfg.extensions.contains(".c"));
  QVERIFY(cfg.extensions.contains(".h"));

  QVERIFY(cfg.supportsFunctionBreakpoints);
  QVERIFY(cfg.supportsConditionalBreakpoints);
  QVERIFY(cfg.supportsHitConditionalBreakpoints);

  QJsonObject launchConfig =
      gdbAdapter->createLaunchConfig("/path/to/program", "/path/to");
  QCOMPARE(launchConfig["type"].toString(), QString("cppdbg"));
  QCOMPARE(launchConfig["request"].toString(), QString("launch"));
  QCOMPARE(launchConfig["program"].toString(), QString("/path/to/program"));
  QCOMPARE(launchConfig["MIMode"].toString(), QString("gdb"));
  QVERIFY(launchConfig.contains("miDebuggerPath"));
  QVERIFY(launchConfig.contains("setupCommands"));

  QJsonObject attachConfig = gdbAdapter->createAttachConfig(12345, "", 0);
  QCOMPARE(attachConfig["type"].toString(), QString("cppdbg"));
  QCOMPARE(attachConfig["request"].toString(), QString("attach"));
  QCOMPARE(attachConfig["processId"].toString(), QString("12345"));

  QJsonObject remoteConfig =
      gdbAdapter->createAttachConfig(0, "192.168.1.100", 1234);
  QCOMPARE(remoteConfig["type"].toString(), QString("cppdbg"));
  QVERIFY(remoteConfig.contains("miDebuggerServerAddress") ||
          remoteConfig.contains("setupCommands"));

  QString status = gdbAdapter->statusMessage();
  QVERIFY(!status.isEmpty());

  QCOMPARE(gdbAdapter->documentationUrl(),
           QString("https://sourceware.org/gdb/current/onlinedocs/gdb/"));

  QString installCmd = gdbAdapter->installCommand();
  QVERIFY(!installCmd.isEmpty());
}

void TestDap::testGdbAdapterRuntimeOverride() {
  auto gdbAdapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(gdbAdapter != nullptr);

  DebugConfiguration configuration;
  configuration.adapterId = "cppdbg-gdb";
  configuration.type = "cppdbg";
  configuration.adapterConfig["miDebuggerPath"] = "/custom/tools/gdb";

  const DebugAdapterConfig cfg =
      gdbAdapter->configForConfiguration(configuration);
  QCOMPARE(cfg.program, QString("/custom/tools/gdb"));
}

void TestDap::testDapBreakpointFromJson() {
  QJsonObject json;
  json["id"] = 42;
  json["verified"] = true;
  json["message"] = "";
  json["line"] = 100;
  json["column"] = 5;

  QJsonObject source;
  source["name"] = "test.py";
  source["path"] = "/home/user/test.py";
  json["source"] = source;

  DapBreakpoint bp = DapBreakpoint::fromJson(json);

  QCOMPARE(bp.id, 42);
  QVERIFY(bp.verified);
  QCOMPARE(bp.line, 100);
  QCOMPARE(bp.column, 5);
  QCOMPARE(bp.source.name, QString("test.py"));
  QCOMPARE(bp.source.path, QString("/home/user/test.py"));
}

void TestDap::testDapStackFrameFromJson() {
  QJsonObject json;
  json["id"] = 1;
  json["name"] = "main";
  json["line"] = 50;
  json["column"] = 0;
  json["presentationHint"] = "normal";

  QJsonObject source;
  source["name"] = "main.cpp";
  source["path"] = "/project/main.cpp";
  json["source"] = source;

  DapStackFrame frame = DapStackFrame::fromJson(json);

  QCOMPARE(frame.id, 1);
  QCOMPARE(frame.name, QString("main"));
  QCOMPARE(frame.line, 50);
  QCOMPARE(frame.source.name, QString("main.cpp"));
  QCOMPARE(frame.presentationHint, QString("normal"));
}

void TestDap::testDapVariableFromJson() {
  QJsonObject json;
  json["name"] = "counter";
  json["value"] = "42";
  json["type"] = "int";
  json["variablesReference"] = 0;
  json["evaluateName"] = "counter";

  DapVariable var = DapVariable::fromJson(json);

  QCOMPARE(var.name, QString("counter"));
  QCOMPARE(var.value, QString("42"));
  QCOMPARE(var.type, QString("int"));
  QCOMPARE(var.variablesReference, 0);
}

void TestDap::testDapStoppedEventFromJson() {
  QJsonObject json;
  json["reason"] = "breakpoint";
  json["threadId"] = 1;
  json["allThreadsStopped"] = true;
  json["description"] = "Paused on breakpoint";

  QJsonArray hitBps;
  hitBps.append(5);
  hitBps.append(10);
  json["hitBreakpointIds"] = hitBps;

  DapStoppedEvent evt = DapStoppedEvent::fromJson(json);

  QCOMPARE(evt.reason, DapStoppedReason::Breakpoint);
  QCOMPARE(evt.threadId, 1);
  QVERIFY(evt.allThreadsStopped);
  QCOMPARE(evt.hitBreakpointIds.count(), 2);
  QCOMPARE(evt.hitBreakpointIds.at(0), 5);
}

void TestDap::testDataBreakpoints() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  int id = bm.addDataBreakpoint("myVariable", "write");
  QVERIFY(id > 0);

  QList<DataBreakpoint> dataBps = bm.allDataBreakpoints();
  QCOMPARE(dataBps.count(), 1);
  QCOMPARE(dataBps.first().dataId, QString("myVariable"));
  QCOMPARE(dataBps.first().accessType, QString("write"));

  bm.removeDataBreakpoint(id);
  QCOMPARE(bm.allDataBreakpoints().count(), 0);
}

void TestDap::testExceptionBreakpoints() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  QStringList filters;
  filters << "uncaught" << "raised";
  bm.setExceptionBreakpoints(filters);

  QStringList enabled = bm.enabledExceptionFilters();
  QCOMPARE(enabled.count(), 2);
  QVERIFY(enabled.contains("uncaught"));
  QVERIFY(enabled.contains("raised"));
}

void TestDap::testDebugConfigurationToJson() {
  DebugConfiguration config;
  config.name = "Test Config";
  config.adapterId = "python-debugpy";
  config.type = "debugpy";
  config.request = "launch";
  config.program = "/path/to/script.py";
  config.args << "--verbose" << "--debug";
  config.cwd = "/path/to";
  config.stopOnEntry = true;
  config.processIdExpression = "${command:pickProcess}";

  QJsonObject json = config.toJson();

  QCOMPARE(json["name"].toString(), QString("Test Config"));
  QCOMPARE(json["adapterId"].toString(), QString("python-debugpy"));
  QCOMPARE(json["type"].toString(), QString("debugpy"));
  QCOMPARE(json["request"].toString(), QString("launch"));
  QCOMPARE(json["program"].toString(), QString("/path/to/script.py"));
  QCOMPARE(json["args"].toArray().count(), 2);
  QCOMPARE(json["stopOnEntry"].toBool(), true);
  QCOMPARE(json["processId"].toString(), QString("${command:pickProcess}"));
}

void TestDap::testDebugConfigurationFromJson() {
  QJsonObject json;
  json["name"] = "Python Debug";
  json["adapterId"] = "python-debugpy";
  json["type"] = "debugpy";
  json["request"] = "launch";
  json["program"] = "${file}";
  json["cwd"] = "${workspaceFolder}";
  json["stopOnEntry"] = false;
  json["processId"] = "${command:pickProcess}";

  QJsonArray args;
  args.append("--arg1");
  args.append("--arg2");
  json["args"] = args;

  DebugConfiguration config = DebugConfiguration::fromJson(json);

  QCOMPARE(config.name, QString("Python Debug"));
  QCOMPARE(config.adapterId, QString("python-debugpy"));
  QCOMPARE(config.type, QString("debugpy"));
  QCOMPARE(config.request, QString("launch"));
  QCOMPARE(config.program, QString("${file}"));
  QCOMPARE(config.args.count(), 2);
  QVERIFY(!config.stopOnEntry);
  QCOMPARE(config.processIdExpression, QString("${command:pickProcess}"));
}

void TestDap::testConfigurationVariableSubstitution() {
  DebugConfigurationManager &mgr = DebugConfigurationManager::instance();
  mgr.setWorkspaceFolder("/home/user/project");

  DebugConfiguration config;
  config.name = "Test";
  config.program = "${workspaceFolder}/main.py";
  config.cwd = "${workspaceFolder}";
  config.args = {"${fileBasename}", "${relativeFile}"};
  config.adapterConfig["connect"] = QJsonObject{
      {"host", "${workspaceFolder}"},
      {"pathMappings",
       QJsonArray{QJsonObject{{"localRoot", "${workspaceFolder}/src"},
                              {"remoteRoot", "/app/${fileBasenameNoExtension}"}}}}};

  DebugConfiguration resolved =
      mgr.resolveVariables(config, "/home/user/project/src/app.py");

  QCOMPARE(resolved.program, QString("/home/user/project/main.py"));
  QCOMPARE(resolved.cwd, QString("/home/user/project"));
  QCOMPARE(resolved.args,
           QStringList({"app.py", QString("src/app.py")}));
  const QJsonObject connect = resolved.adapterConfig["connect"].toObject();
  QCOMPARE(connect["host"].toString(), QString("/home/user/project"));
  const QJsonArray pathMappings = connect["pathMappings"].toArray();
  QCOMPARE(pathMappings.size(), 1);
  QCOMPARE(pathMappings.first().toObject()["localRoot"].toString(),
           QString("/home/user/project/src"));
  QCOMPARE(pathMappings.first().toObject()["remoteRoot"].toString(),
           QString("/app/app"));
}

void TestDap::testCompoundConfigurationLoading() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  const QString launchPath = tempDir.path() + "/launch.json";
  QFile file(launchPath);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

  const QByteArray json = R"({
    "version": "1.0.0",
    "configurations": [
      {
        "name": "Server",
        "type": "debugpy",
        "request": "launch",
        "program": "${workspaceFolder}/server.py"
      },
      {
        "name": "Client",
        "type": "debugpy",
        "request": "launch",
        "program": "${workspaceFolder}/client.py"
      }
    ],
    "compounds": [
      {
        "name": "Server + Client",
        "configurations": ["Server", "Client"],
        "stopAll": true
      }
    ]
  })";
  file.write(json);
  file.close();

  DebugConfigurationManager &mgr = DebugConfigurationManager::instance();
  QVERIFY(mgr.loadFromFile(launchPath));

  const QList<CompoundDebugConfiguration> compounds =
      mgr.allCompoundConfigurations();
  QCOMPARE(compounds.size(), 1);
  QCOMPARE(compounds.first().name, QString("Server + Client"));
  QCOMPARE(compounds.first().configurations,
           QStringList({"Server", "Client"}));
  QVERIFY(compounds.first().stopAll);
}

void TestDap::testQuickConfigPreservesAdapterDefaults() {
  auto pythonAdapter =
      DebugAdapterRegistry::instance().adapter("python-debugpy");
  QVERIFY(pythonAdapter != nullptr);
  const DebugConfiguration pythonCfg = DebugConfiguration::fromJson(
      pythonAdapter->createLaunchConfig("/home/user/project/app.py",
                                        "/home/user/project"));
  QCOMPARE(pythonAdapter->config().id, QString("python-debugpy"));
  QCOMPARE(pythonCfg.type, QString("debugpy"));
  QCOMPARE(pythonCfg.program, QString("/home/user/project/app.py"));
  QCOMPARE(pythonCfg.adapterConfig["console"].toString(),
           QString("integratedTerminal"));

  auto gdbAdapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(gdbAdapter != nullptr);
  const DebugConfiguration cppCfg = DebugConfiguration::fromJson(
      gdbAdapter->createLaunchConfig("/home/user/project/main",
                                     "/home/user/project"));
  QCOMPARE(gdbAdapter->runtimeOverrideKey(), QString("miDebuggerPath"));
  QCOMPARE(cppCfg.type, QString("cppdbg"));
  QCOMPARE(cppCfg.program, QString("/home/user/project/main"));
  QCOMPARE(cppCfg.adapterConfig["MIMode"].toString(), QString("gdb"));
  QVERIFY(cppCfg.adapterConfig.contains("miDebuggerPath"));
  QVERIFY(cppCfg.adapterConfig.contains("setupCommands"));
}

void TestDap::testQuickConfigCreatedEvenWhenAdapterUnavailable() {
  DebugSettings &settings = DebugSettings::instance();

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  settings.initialize(tempDir.path());

  const QJsonObject originalPythonSettings =
      settings.adapterSettings()["adapters"]
          .toObject()["python-debugpy"]
          .toObject();

  QJsonObject brokenPythonSettings = originalPythonSettings;
  brokenPythonSettings["python"] = "/definitely/missing/python3";
  settings.setAdapterSettings("python-debugpy", brokenPythonSettings);

  DebugConfiguration cfg =
      DebugConfigurationManager::instance().createQuickConfig(
          "/tmp/example.py", "py");
  QCOMPARE(cfg.type, QString("debugpy"));
  QCOMPARE(cfg.adapterId, QString("python-debugpy"));
  QCOMPARE(cfg.program, QString("/tmp/example.py"));

  settings.setAdapterSettings("python-debugpy", originalPythonSettings);
}

void TestDap::testConfigurationManagerSingleton() {
  DebugConfigurationManager &mgr1 = DebugConfigurationManager::instance();
  DebugConfigurationManager &mgr2 = DebugConfigurationManager::instance();

  QCOMPARE(&mgr1, &mgr2);
}

void TestDap::cleanupWatches() { WatchManager::instance().clearAll(); }

void TestDap::testWatchManagerSingleton() {
  WatchManager &wm1 = WatchManager::instance();
  WatchManager &wm2 = WatchManager::instance();

  QCOMPARE(&wm1, &wm2);
}

void TestDap::testAddWatch() {
  cleanupWatches();

  WatchManager &wm = WatchManager::instance();

  int id = wm.addWatch("myVariable");
  QVERIFY(id > 0);

  WatchExpression watch = wm.watch(id);
  QCOMPARE(watch.expression, QString("myVariable"));

  int emptyId = wm.addWatch("");
  QCOMPARE(emptyId, 0);
}

void TestDap::testRemoveWatch() {
  cleanupWatches();

  WatchManager &wm = WatchManager::instance();

  int id = wm.addWatch("testExpr");
  QCOMPARE(wm.allWatches().count(), 1);

  wm.removeWatch(id);
  QCOMPARE(wm.allWatches().count(), 0);
}

void TestDap::testWatchPersistence() {
  cleanupWatches();

  WatchManager &wm = WatchManager::instance();

  wm.addWatch("expr1");
  wm.addWatch("expr2");
  wm.addWatch("expr3");

  QJsonObject json = wm.saveToJson();

  wm.clearAll();
  QCOMPARE(wm.allWatches().count(), 0);

  wm.loadFromJson(json);
  QCOMPARE(wm.allWatches().count(), 3);
}

void TestDap::testDebugSessionState() {
  DebugSession session("test-session");

  QCOMPARE(session.id(), QString("test-session"));
  QCOMPARE(session.state(), DebugSession::State::Idle);
  QVERIFY(session.client() != nullptr);
}

void TestDap::testSessionManagerSingleton() {
  DebugSessionManager &mgr1 = DebugSessionManager::instance();
  DebugSessionManager &mgr2 = DebugSessionManager::instance();

  QCOMPARE(&mgr1, &mgr2);

  QVERIFY(!mgr1.hasActiveSessions());
}

void TestDap::testSessionManagerReportsAdapterHints() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  DebugSettings::instance().initialize(tempDir.path());

  QJsonObject nodeSettings =
      DebugSettings::instance().adapterSettings()["adapters"]
          .toObject()["node-debug"]
          .toObject();
  nodeSettings["adapterCommand"] = "/definitely/missing/js-debug-adapter";
  nodeSettings["runtimeExecutable"] = "node";
  DebugSettings::instance().setAdapterSettings("node-debug", nodeSettings);

  DebugConfiguration configuration;
  configuration.name = "Node Smoke";
  configuration.adapterId = "node-debug";
  configuration.type = "node";
  configuration.request = "launch";
  configuration.program = "/workspace/app.js";

  const QString sessionId =
      DebugSessionManager::instance().startSession(configuration);
  QVERIFY(sessionId.isEmpty());

  const QString lastError = DebugSessionManager::instance().lastError();
  QVERIFY(lastError.contains("Node.js"));
  QVERIFY(lastError.contains("adapter", Qt::CaseInsensitive));
  QVERIFY(lastError.contains("Adapter Command"));
}

void TestDap::testDebugSettingsInitialization() {
  DebugSettings &settings = DebugSettings::instance();

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString workspaceFolder = tempDir.path();
  settings.initialize(workspaceFolder);

  QCOMPARE(settings.workspaceFolder(), workspaceFolder);
  QCOMPARE(settings.debugSettingsDir(), workspaceFolder + "/.lightpad/debug");

  QDir dir(settings.debugSettingsDir());
  QVERIFY(dir.exists());

  QVERIFY(QFile::exists(settings.launchConfigPath()));
  QVERIFY(QFile::exists(settings.breakpointsConfigPath()));
  QVERIFY(QFile::exists(settings.watchesConfigPath()));
  QVERIFY(QFile::exists(settings.adaptersConfigPath()));
  QVERIFY(QFile::exists(settings.settingsConfigPath()));
}

void TestDap::testDebugSettingsFilePaths() {
  DebugSettings &settings = DebugSettings::instance();

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString workspaceFolder = tempDir.path();
  settings.initialize(workspaceFolder);

  QCOMPARE(settings.launchConfigPath(),
           workspaceFolder + "/.lightpad/debug/launch.json");
  QCOMPARE(settings.breakpointsConfigPath(),
           workspaceFolder + "/.lightpad/debug/breakpoints.json");
  QCOMPARE(settings.watchesConfigPath(),
           workspaceFolder + "/.lightpad/debug/watches.json");
  QCOMPARE(settings.adaptersConfigPath(),
           workspaceFolder + "/.lightpad/debug/adapters.json");
  QCOMPARE(settings.settingsConfigPath(),
           workspaceFolder + "/.lightpad/debug/settings.json");

  QFile launchFile(settings.launchConfigPath());
  QVERIFY(launchFile.open(QIODevice::ReadOnly));
  QJsonDocument launchDoc = QJsonDocument::fromJson(launchFile.readAll());
  QVERIFY(!launchDoc.isNull());
  QVERIFY(launchDoc.object().contains("configurations"));

  QFile adaptersFile(settings.adaptersConfigPath());
  QVERIFY(adaptersFile.open(QIODevice::ReadOnly));
  QJsonDocument adaptersDoc = QJsonDocument::fromJson(adaptersFile.readAll());
  QVERIFY(!adaptersDoc.isNull());
  QVERIFY(adaptersDoc.object().contains("adapters"));
  QVERIFY(adaptersDoc.object().contains("defaultAdapters"));
}

void TestDap::testDebugConfigurationFileOpenPath() {
  DebugSettings &settings = DebugSettings::instance();
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString workspaceFolder = tempDir.path();
  settings.initialize(workspaceFolder);
  manager.setWorkspaceFolder(workspaceFolder);
  manager.loadFromLightpadDir();

  QString launchPath = manager.lightpadLaunchConfigPath();
  QCOMPARE(launchPath, settings.launchConfigPath());
  QVERIFY(QFile::exists(launchPath));
}

void TestDap::testWatchUpdate() {
  cleanupWatches();

  WatchManager &wm = WatchManager::instance();

  int id = wm.addWatch("oldExpr");
  QCOMPARE(wm.watch(id).expression, QString("oldExpr"));

  QSignalSpy updatedSpy(&wm, &WatchManager::watchUpdated);
  wm.updateWatch(id, "newExpr");

  QCOMPARE(updatedSpy.count(), 1);
  QCOMPARE(wm.watch(id).expression, QString("newExpr"));
  QVERIFY(wm.watch(id).value.isEmpty());
  QVERIFY(wm.watch(id).type.isEmpty());
}

void TestDap::testWatchEvaluationWithoutClient() {
  cleanupWatches();

  WatchManager &wm = WatchManager::instance();

  wm.setDapClient(nullptr);

  int id = wm.addWatch("testExpr");

  wm.evaluateWatch(id, 1);

  WatchExpression w = wm.watch(id);
  QVERIFY(w.value.isEmpty());
  QVERIFY(!w.isError);
}

void TestDap::testDapClientEvaluateErrorSignal() {
  DapClient client;

  QSignalSpy errorSpy(&client, &DapClient::evaluateError);
  QVERIFY(errorSpy.isValid());
}

void TestDap::testDapClientVariableSetSignal() {
  DapClient client;

  QSignalSpy setSpy(&client, &DapClient::variableSet);
  QVERIFY(setSpy.isValid());
}

void TestDap::testDapClientSetDataBreakpoints() {
  DapClient client;

  QList<QJsonObject> dataBreakpoints;
  QJsonObject bp1;
  bp1["dataId"] = "myVar";
  bp1["accessType"] = "write";
  dataBreakpoints.append(bp1);

  client.setDataBreakpoints(dataBreakpoints);
}

void TestDap::testExpressionTranslatorGdbConsolePlan() {
  const QList<DebugEvaluateRequest> expressionPlan =
      DebugExpressionTranslator::buildConsoleEvaluationPlan(
          "a + b", "cppdbg-gdb", "cppdbg");
  QCOMPARE(expressionPlan.size(), 2);
  QCOMPARE(expressionPlan.at(0).expression, QString("a + b"));
  QCOMPARE(expressionPlan.at(0).context, QString("watch"));
  QCOMPARE(expressionPlan.at(1).expression, QString("print a + b"));
  QCOMPARE(expressionPlan.at(1).context, QString("repl"));

  const QList<DebugEvaluateRequest> commandPlan =
      DebugExpressionTranslator::buildConsoleEvaluationPlan(
          "info locals", "cppdbg-gdb", "cppdbg");
  QCOMPARE(commandPlan.size(), 1);
  QCOMPARE(commandPlan.at(0).expression, QString("info locals"));
  QCOMPARE(commandPlan.at(0).context, QString("repl"));
}

void TestDap::testExpressionTranslatorLocalsFallbackRequest() {
  const DebugEvaluateRequest gdbRequest =
      DebugExpressionTranslator::localsFallbackRequest("cppdbg-gdb", "cppdbg");
  QCOMPARE(gdbRequest.expression,
           QString("interpreter-exec console \"info locals\""));
  QCOMPARE(gdbRequest.context, QString("repl"));

  const DebugEvaluateRequest lldbRequest =
      DebugExpressionTranslator::localsFallbackRequest("cppdbg-lldb", "cppdbg");
  QVERIFY(lldbRequest.expression.isEmpty());
}

QTEST_MAIN(TestDap)
#include "test_dap.moc"
