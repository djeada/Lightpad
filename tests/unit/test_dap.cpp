#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "dap/breakpointmanager.h"
#include "dap/dapclient.h"
#include "dap/debugadapterregistry.h"
#include "dap/debugconfiguration.h"
#include "dap/debugsession.h"
#include "dap/debugsettings.h"
#include "dap/watchmanager.h"

/**
 * @brief Unit tests for DAP (Debug Adapter Protocol) components
 *
 * Tests the DAP client infrastructure, breakpoint manager,
 * debug adapter registry, configuration manager, watch manager,
 * and session manager.
 */
class TestDap : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  // DapClient tests
  void testDapClientInitialState();
  void testDapClientStateEnum();

  // BreakpointManager tests
  void testBreakpointManagerSingleton();
  void testAddBreakpoint();
  void testRemoveBreakpoint();
  void testToggleBreakpoint();
  void testBreakpointCondition();
  void testBreakpointLogpoint();
  void testBreakpointPersistence();
  void testClearBreakpoints();
  void testDataBreakpoints();
  void testExceptionBreakpoints();

  // DebugAdapterRegistry tests
  void testRegistrySingleton();
  void testBuiltinAdapters();
  void testAdapterLookupByFile();
  void testAdapterLookupByLanguage();
  void testGdbAdapterIntegration();

  // DebugConfiguration tests
  void testDebugConfigurationToJson();
  void testDebugConfigurationFromJson();
  void testConfigurationVariableSubstitution();
  void testConfigurationManagerSingleton();

  // WatchManager tests
  void testWatchManagerSingleton();
  void testAddWatch();
  void testRemoveWatch();
  void testWatchPersistence();
  void testWatchUpdate();
  void testWatchEvaluationWithoutClient();

  // DapClient new signals tests
  void testDapClientEvaluateErrorSignal();
  void testDapClientVariableSetSignal();
  void testDapClientSetDataBreakpoints();

  // DebugSession tests
  void testDebugSessionState();
  void testSessionManagerSingleton();

  // DebugSettings tests
  void testDebugSettingsInitialization();
  void testDebugSettingsFilePaths();
  void testDebugConfigurationFileOpenPath();

  // Data structures tests
  void testDapBreakpointFromJson();
  void testDapStackFrameFromJson();
  void testDapVariableFromJson();
  void testDapStoppedEventFromJson();

private:
  void cleanupBreakpoints();
  void cleanupWatches();
};

void TestDap::initTestCase() {
  // Clean up any existing breakpoints
  cleanupBreakpoints();
}

void TestDap::cleanupTestCase() { cleanupBreakpoints(); }

void TestDap::cleanupBreakpoints() { BreakpointManager::instance().clearAll(); }

// =============================================================================
// DapClient Tests
// =============================================================================

void TestDap::testDapClientInitialState() {
  DapClient client;

  QCOMPARE(client.state(), DapClient::State::Disconnected);
  QVERIFY(!client.isReady());
  QVERIFY(!client.isDebugging());
  QCOMPARE(client.currentThreadId(), 0);
}

void TestDap::testDapClientStateEnum() {
  // Test that all states are distinct
  QVERIFY(DapClient::State::Disconnected != DapClient::State::Connecting);
  QVERIFY(DapClient::State::Ready != DapClient::State::Running);
  QVERIFY(DapClient::State::Running != DapClient::State::Stopped);
  QVERIFY(DapClient::State::Stopped != DapClient::State::Terminated);
  QVERIFY(DapClient::State::Error != DapClient::State::Ready);
}

// =============================================================================
// BreakpointManager Tests
// =============================================================================

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

  // Removing non-existent breakpoint should be safe
  bm.removeBreakpoint(99999); // Should not crash
}

void TestDap::testToggleBreakpoint() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  // First toggle - should add
  QVERIFY(!bm.hasBreakpoint("/test/toggle.cpp", 5));
  bm.toggleBreakpoint("/test/toggle.cpp", 5);
  QVERIFY(bm.hasBreakpoint("/test/toggle.cpp", 5));

  // Second toggle - should remove
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

  // Clear log message
  bm.setLogMessage(id, "");
  updated = bm.breakpoint(id);
  QVERIFY(!updated.isLogpoint);
}

void TestDap::testBreakpointPersistence() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  // Add some breakpoints
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

  // Save to JSON
  QJsonObject json = bm.saveToJson();

  // Clear and reload
  bm.clearAll();
  QCOMPARE(bm.allBreakpoints().count(), 0);

  bm.loadFromJson(json);

  // Verify restored
  QCOMPARE(bm.allBreakpoints().count(), 2);

  QList<Breakpoint> restored = bm.breakpointsForFile("/test/persist1.cpp");
  QCOMPARE(restored.count(), 1);
  QCOMPARE(restored.first().condition, QString("a == b"));
}

void TestDap::testClearBreakpoints() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  // Add breakpoints to multiple files
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

  // Clear single file
  bm.clearFile("/test/clear1.cpp");
  QCOMPARE(bm.allBreakpoints().count(), 1);
  QCOMPARE(bm.breakpointsForFile("/test/clear1.cpp").count(), 0);
  QCOMPARE(bm.breakpointsForFile("/test/clear2.cpp").count(), 1);

  // Clear all
  QSignalSpy clearedSpy(&bm, &BreakpointManager::allBreakpointsCleared);
  bm.clearAll();
  QCOMPARE(clearedSpy.count(), 1);
  QCOMPARE(bm.allBreakpoints().count(), 0);
}

// =============================================================================
// DebugAdapterRegistry Tests
// =============================================================================

void TestDap::testRegistrySingleton() {
  DebugAdapterRegistry &reg1 = DebugAdapterRegistry::instance();
  DebugAdapterRegistry &reg2 = DebugAdapterRegistry::instance();

  QCOMPARE(&reg1, &reg2);
}

void TestDap::testBuiltinAdapters() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  QList<std::shared_ptr<IDebugAdapter>> adapters = reg.allAdapters();

  // Should have at least the built-in adapters
  QVERIFY(adapters.count() >= 4); // Python, Node.js, GDB, LLDB

  // Check for Python adapter
  auto pythonAdapter = reg.adapter("python-debugpy");
  QVERIFY(pythonAdapter != nullptr);
  QCOMPARE(pythonAdapter->config().name, QString("Python (debugpy)"));

  // Check for Node.js adapter
  auto nodeAdapter = reg.adapter("node-debug");
  QVERIFY(nodeAdapter != nullptr);
  QVERIFY(nodeAdapter->supportsLanguage("js"));

  // LLDB adapter should produce launch config routable by adapter type lookup.
  auto lldbAdapter = reg.adapter("cppdbg-lldb");
  QVERIFY(lldbAdapter != nullptr);
  QJsonObject lldbLaunchCfg =
      lldbAdapter->createLaunchConfig("/tmp/a.out", "/tmp");
  QCOMPARE(lldbLaunchCfg["type"].toString(), QString("cppdbg"));
}

void TestDap::testAdapterLookupByFile() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  // Python files
  QList<std::shared_ptr<IDebugAdapter>> pyAdapters =
      reg.adaptersForFile("/test/script.py");
  QVERIFY(!pyAdapters.isEmpty());
  QVERIFY(pyAdapters.first()->config().id.contains("python"));

  // JavaScript files
  QList<std::shared_ptr<IDebugAdapter>> jsAdapters =
      reg.adaptersForFile("/test/app.js");
  QVERIFY(!jsAdapters.isEmpty());

  // C++ files
  QList<std::shared_ptr<IDebugAdapter>> cppAdapters =
      reg.adaptersForFile("/test/main.cpp");
  QVERIFY(!cppAdapters.isEmpty());

  // Unknown extension
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

void TestDap::testGdbAdapterIntegration() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  // Get GDB adapter
  auto gdbAdapter = reg.adapter("cppdbg-gdb");
  QVERIFY(gdbAdapter != nullptr);

  // Verify configuration
  DebugAdapterConfig cfg = gdbAdapter->config();
  QCOMPARE(cfg.id, QString("cppdbg-gdb"));
  QCOMPARE(cfg.name, QString("C/C++ (GDB)"));
  QCOMPARE(cfg.type, QString("cppdbg"));
  QVERIFY(cfg.arguments.contains("--interpreter=dap"));

  // Should support C, C++ and other languages
  QVERIFY(cfg.languages.contains("cpp"));
  QVERIFY(cfg.languages.contains("c"));

  // Should support common file extensions
  QVERIFY(cfg.extensions.contains(".cpp"));
  QVERIFY(cfg.extensions.contains(".c"));
  QVERIFY(cfg.extensions.contains(".h"));

  // Should have capabilities
  QVERIFY(cfg.supportsFunctionBreakpoints);
  QVERIFY(cfg.supportsConditionalBreakpoints);
  QVERIFY(cfg.supportsHitConditionalBreakpoints);

  // Test launch config generation
  QJsonObject launchConfig =
      gdbAdapter->createLaunchConfig("/path/to/program", "/path/to");
  QCOMPARE(launchConfig["type"].toString(), QString("cppdbg"));
  QCOMPARE(launchConfig["request"].toString(), QString("launch"));
  QCOMPARE(launchConfig["program"].toString(), QString("/path/to/program"));
  QCOMPARE(launchConfig["MIMode"].toString(), QString("gdb"));
  QVERIFY(launchConfig.contains("miDebuggerPath"));
  QVERIFY(launchConfig.contains("setupCommands"));

  // Test attach config generation
  QJsonObject attachConfig = gdbAdapter->createAttachConfig(12345, "", 0);
  QCOMPARE(attachConfig["type"].toString(), QString("cppdbg"));
  QCOMPARE(attachConfig["request"].toString(), QString("attach"));
  QCOMPARE(attachConfig["processId"].toString(), QString("12345"));

  // Test remote attach config
  QJsonObject remoteConfig =
      gdbAdapter->createAttachConfig(0, "192.168.1.100", 1234);
  QCOMPARE(remoteConfig["type"].toString(), QString("cppdbg"));
  QVERIFY(remoteConfig.contains("miDebuggerServerAddress") ||
          remoteConfig.contains("setupCommands"));

  // Status message should provide useful information
  QString status = gdbAdapter->statusMessage();
  QVERIFY(!status.isEmpty());

  // Documentation URL should be valid
  QCOMPARE(gdbAdapter->documentationUrl(),
           QString("https://sourceware.org/gdb/current/onlinedocs/gdb/"));

  // Install command should be provided
  QString installCmd = gdbAdapter->installCommand();
  QVERIFY(!installCmd.isEmpty());
}

// =============================================================================
// Data Structure Tests
// =============================================================================

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

// =============================================================================
// Data Breakpoint and Exception Breakpoint Tests
// =============================================================================

void TestDap::testDataBreakpoints() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  // Add data breakpoint
  int id = bm.addDataBreakpoint("myVariable", "write");
  QVERIFY(id > 0);

  QList<DataBreakpoint> dataBps = bm.allDataBreakpoints();
  QCOMPARE(dataBps.count(), 1);
  QCOMPARE(dataBps.first().dataId, QString("myVariable"));
  QCOMPARE(dataBps.first().accessType, QString("write"));

  // Remove data breakpoint
  bm.removeDataBreakpoint(id);
  QCOMPARE(bm.allDataBreakpoints().count(), 0);
}

void TestDap::testExceptionBreakpoints() {
  cleanupBreakpoints();

  BreakpointManager &bm = BreakpointManager::instance();

  // Set exception breakpoints
  QStringList filters;
  filters << "uncaught" << "raised";
  bm.setExceptionBreakpoints(filters);

  QStringList enabled = bm.enabledExceptionFilters();
  QCOMPARE(enabled.count(), 2);
  QVERIFY(enabled.contains("uncaught"));
  QVERIFY(enabled.contains("raised"));
}

// =============================================================================
// DebugConfiguration Tests
// =============================================================================

void TestDap::testDebugConfigurationToJson() {
  DebugConfiguration config;
  config.name = "Test Config";
  config.type = "debugpy";
  config.request = "launch";
  config.program = "/path/to/script.py";
  config.args << "--verbose" << "--debug";
  config.cwd = "/path/to";
  config.stopOnEntry = true;

  QJsonObject json = config.toJson();

  QCOMPARE(json["name"].toString(), QString("Test Config"));
  QCOMPARE(json["type"].toString(), QString("debugpy"));
  QCOMPARE(json["request"].toString(), QString("launch"));
  QCOMPARE(json["program"].toString(), QString("/path/to/script.py"));
  QCOMPARE(json["args"].toArray().count(), 2);
  QCOMPARE(json["stopOnEntry"].toBool(), true);
}

void TestDap::testDebugConfigurationFromJson() {
  QJsonObject json;
  json["name"] = "Python Debug";
  json["type"] = "debugpy";
  json["request"] = "launch";
  json["program"] = "${file}";
  json["cwd"] = "${workspaceFolder}";
  json["stopOnEntry"] = false;

  QJsonArray args;
  args.append("--arg1");
  args.append("--arg2");
  json["args"] = args;

  DebugConfiguration config = DebugConfiguration::fromJson(json);

  QCOMPARE(config.name, QString("Python Debug"));
  QCOMPARE(config.type, QString("debugpy"));
  QCOMPARE(config.request, QString("launch"));
  QCOMPARE(config.program, QString("${file}"));
  QCOMPARE(config.args.count(), 2);
  QVERIFY(!config.stopOnEntry);
}

void TestDap::testConfigurationVariableSubstitution() {
  DebugConfigurationManager &mgr = DebugConfigurationManager::instance();
  mgr.setWorkspaceFolder("/home/user/project");

  DebugConfiguration config;
  config.name = "Test";
  config.program = "${workspaceFolder}/main.py";
  config.cwd = "${workspaceFolder}";

  DebugConfiguration resolved =
      mgr.resolveVariables(config, "/home/user/project/src/app.py");

  QCOMPARE(resolved.program, QString("/home/user/project/main.py"));
  QCOMPARE(resolved.cwd, QString("/home/user/project"));
}

void TestDap::testConfigurationManagerSingleton() {
  DebugConfigurationManager &mgr1 = DebugConfigurationManager::instance();
  DebugConfigurationManager &mgr2 = DebugConfigurationManager::instance();

  QCOMPARE(&mgr1, &mgr2);
}

// =============================================================================
// WatchManager Tests
// =============================================================================

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

  // Adding empty expression should fail
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

  // Save to JSON
  QJsonObject json = wm.saveToJson();

  // Clear and reload
  wm.clearAll();
  QCOMPARE(wm.allWatches().count(), 0);

  wm.loadFromJson(json);
  QCOMPARE(wm.allWatches().count(), 3);
}

// =============================================================================
// DebugSession Tests
// =============================================================================

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

  // Initially no sessions
  QVERIFY(!mgr1.hasActiveSessions());
}

// =============================================================================
// DebugSettings Tests
// =============================================================================

void TestDap::testDebugSettingsInitialization() {
  DebugSettings &settings = DebugSettings::instance();

  // Create a temporary directory for testing
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString workspaceFolder = tempDir.path();
  settings.initialize(workspaceFolder);

  QCOMPARE(settings.workspaceFolder(), workspaceFolder);
  QCOMPARE(settings.debugSettingsDir(), workspaceFolder + "/.lightpad/debug");

  // Check that the debug settings directory was created
  QDir dir(settings.debugSettingsDir());
  QVERIFY(dir.exists());

  // Check that default config files were created
  QVERIFY(QFile::exists(settings.launchConfigPath()));
  QVERIFY(QFile::exists(settings.breakpointsConfigPath()));
  QVERIFY(QFile::exists(settings.watchesConfigPath()));
  QVERIFY(QFile::exists(settings.adaptersConfigPath()));
  QVERIFY(QFile::exists(settings.settingsConfigPath()));
}

void TestDap::testDebugSettingsFilePaths() {
  DebugSettings &settings = DebugSettings::instance();

  // Create a temporary directory for testing
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString workspaceFolder = tempDir.path();
  settings.initialize(workspaceFolder);

  // Verify file paths
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

  // Verify that config files have valid JSON
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

// =============================================================================
// Watch Manager Extended Tests
// =============================================================================

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

  // Ensure no DAP client is connected
  wm.setDapClient(nullptr);

  int id = wm.addWatch("testExpr");

  // Evaluating without a client should be a no-op
  wm.evaluateWatch(id, 1);

  // Watch should retain its default state
  WatchExpression w = wm.watch(id);
  QVERIFY(w.value.isEmpty());
  QVERIFY(!w.isError);
}

// =============================================================================
// DapClient Extended Tests
// =============================================================================

void TestDap::testDapClientEvaluateErrorSignal() {
  DapClient client;

  // Verify the evaluateError signal is declared and connectable
  QSignalSpy errorSpy(&client, &DapClient::evaluateError);
  QVERIFY(errorSpy.isValid());
}

void TestDap::testDapClientVariableSetSignal() {
  DapClient client;

  // Verify the variableSet signal is declared and connectable
  QSignalSpy setSpy(&client, &DapClient::variableSet);
  QVERIFY(setSpy.isValid());
}

void TestDap::testDapClientSetDataBreakpoints() {
  DapClient client;

  // Verify the method exists and doesn't crash when called without a process
  // (it should log a warning and return gracefully)
  QList<QJsonObject> dataBreakpoints;
  QJsonObject bp1;
  bp1["dataId"] = "myVar";
  bp1["accessType"] = "write";
  dataBreakpoints.append(bp1);

  // This should not crash even without a running process
  client.setDataBreakpoints(dataBreakpoints);
}

QTEST_MAIN(TestDap)
#include "test_dap.moc"
