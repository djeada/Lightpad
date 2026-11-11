#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <QCoreApplication>

#include "core/logging/logger.h"
#include "dap/breakpointmanager.h"
#include "dap/dapclient.h"
#include "dap/debugadapterregistry.h"
#include "dap/debugconfiguration.h"
#include "dap/debugsession.h"
#include "dap/debugsettings.h"
#include "dap/expressiontranslator.h"
#include "dap/watchmanager.h"
#include "ui/dialogs/debugconfigurationdialog.h"
#include "ui/dockutils.h"
#include "ui/panels/debugconcurrencypanel.h"
#include "ui/panels/debugpanel.h"
#include "ui/panels/debugvalueformatter.h"
#include "ui/panels/debugvalueinspector.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <memory>
#ifndef Q_OS_WIN
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#endif

Q_DECLARE_METATYPE(DapExceptionInfo)
Q_DECLARE_METATYPE(DapStoppedEvent)
Q_DECLARE_METATYPE(DapDataBreakpointInfo)
Q_DECLARE_METATYPE(DapMemoryChunk)
Q_DECLARE_METATYPE(QList<DapCompletionItem>)
Q_DECLARE_METATYPE(QList<DapDisassembledInstruction>)
Q_DECLARE_METATYPE(QList<DapVariable>)

class TestDap : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testDapClientInitialState();
  void testDapClientStateEnum();
  void testDapClientAdapterExitIsReported();

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
  void testPythonLaunchArgumentsUseResolvedInterpreter();
  void testAdapterLookupByFile();
  void testAdapterLookupByLanguage();
  void testAdapterLookupByConfiguration();
  void testNodeAdapterRuntimeOverride();
  void testNodeAdapterMissingCommandStatus();
  void testGdbAdapterIntegration();
  void testGdbAdapterRuntimeOverride();
  void testGdbLaunchConfigUsesConfiguredDebugger();
  void testGoAdapterIntegration();
  void testGoAdapterLaunchConfig();
  void testGoAdapterLookupByFile();
  void testRustAdapterIntegration();
  void testRustAdapterLaunchConfig();
  void testRustAdapterLookupByFile();

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
  void testWatchValuesFollowClientLifecycle_data();
  void testWatchValuesFollowClientLifecycle();

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
  void testDapSourceNameFallsBackToPath();
  void testDapStoppedEventFromJson();

  void testDapClientFramingHandlesUtf8AndChunkedDelivery();
  void testDapClientFakeAdapterFullSession();
  void testDapClientFakeAdapterExceptionInfo();
  void testDapClientCapabilityGating();
  void testDapClientSocketTransport();
  void testDapClientSocketTransportDisconnectIsReported();
  void testDapClientFramingSkipsNonProtocolOutput();
  void testDapClientFramingSkipsOversizedFrameAcrossChunks();
  void testDapClientFramingDrainsLargeBursts();
  void testDapClientStoppedWithoutThreadKeepsCurrentThread();
  void testDapClientOversizedResponseReleasesPendingRequest();
  void testDapClientDropsResponsesCancelledByResume();
  void testDapClientKeepsInteractiveEvaluationDuringFunctionCall();
  void testDapClientRestartWrapsLaunchArguments();
  void testDapClientRestartDropsOldInspectionResponses();
  void testNativeRestartClearsDebugPanel();
  void testDapClientServerTransport();
  void testDapClientCompletionsRoundTrip();
  void testDapClientDataBreakpointInfoRoundTrip();
  void testDapClientMemoryAndDisassembly();
  void testDapClientUnsupportedInspectionRequestsFailFast();
  void testDapClientRequestTimeout();
  void testDapClientAdapterCrashTerminatesSession();
  void testDapClientForwardsRunInTerminalKind();
  void testDapClientServerTransportReportsEarlyExit();
  void testBreakpointVerificationPairsByIndex();
  void testPreferredAdapterPrefersLanguageSpecific();
  void testCommandLineArgumentsRoundTrip();
  void testConfigurationRenameRefusesExistingName();
  void testDebugConfigurationDialogValidationAndCancel();
  void testDebugConfigurationDialogDebouncesStatusProbe();
  void testNestedValueFormatting();
  void testNativeScalarFormats();
  void testNativeCollections_data();
  void testNativeCollections();
  void testNativeBufferPagingAndSnapshot();
  void testNativeBufferReportsErrors();
  void testNativeVariablesRemainStructured();
  void testNativeCCollections();
  void testNativeCppNestedCollections();
  void testStringSortPointerArray_data();
  void testStringSortPointerArray();
  void testStringSortConsoleFunctionCall();
  void testStringSortInspectorFollowsSteps_data();
  void testStringSortInspectorFollowsSteps();
  void testStringSortInspectorFollowsCallerFrame();
  void testNativeInspectorLiveCollections();
  void testPointerRowsMatrix();
  void testInspectorCorrelatesRequests();
  void testInspectorMatrixAndRaggedRows();
  void testInspectorPagingAndSnapshot();
  void testInspectorPagingAndSnapshot_data();
  void testInspectorPagesNamedChildren_data();
  void testInspectorPagesNamedChildren();
  void testInspectorMixedCollections_data();
  void testInspectorMixedCollections();
  void testInspectorNestedNativeBuffers();
  void testInspectorReportsErrors();
  void testInspectorWithRealDebugpy();
  void testVtkPythonObjectsAndArrays();
  void testOpenGLCppObjectsAndMatrices();
  void testGdbLaunchEnvironmentPreservesGraphicsContext();
  void testGdbDebuggeeReadsStdinFromProvidedTerminal();
  void testGdbDebuggeeLaunchesWithoutTerminal();
  void testProgramIoButtonFollowsSelectedClient();
  void testDebugToolbarCollapsesToIconsWhenNarrow();
  void testConsoleResultOpensStructuredInspector();
  void testPartialThreadExecution();
  void testSelectedThreadExecutionArguments();
  void testPythonThreadSelection();
  void testCppThreadSelection();
  void testPythonChildProcesses_data();
  void testPythonChildProcesses();
  void testAsyncioTaskInspection();
  void testProcessScopedDataBreakpoints();
  void testContinueCancelsPendingPause();
  void testTabbedDebugDockCanShrinkBelowTwentyPercent();
  void testTabbedDebugDockCanShrinkBelowTwentyPercent_data();

private:
  QString fakeAdapterPath() const;
  static QByteArray frameMessage(const QJsonObject &message);
  bool traceFileContains(const QString &path, const QByteArray &needle,
                         int *outCount = nullptr) const;
  static bool waitForAdapterListening(QProcess *adapterProcess, int timeoutMs);

private:
  void cleanupBreakpoints();
  void cleanupWatches();
};

void TestDap::initTestCase() {
  if (qEnvironmentVariableIsSet("LIGHTPAD_DAP_TRACE"))
    Logger::instance().setLogLevel(LogLevel::Debug);
  qRegisterMetaType<DapExceptionInfo>("DapExceptionInfo");
  qRegisterMetaType<DapDataBreakpointInfo>("DapDataBreakpointInfo");
  qRegisterMetaType<DapMemoryChunk>("DapMemoryChunk");
  qRegisterMetaType<QList<DapCompletionItem>>("QList<DapCompletionItem>");
  qRegisterMetaType<QList<DapDisassembledInstruction>>(
      "QList<DapDisassembledInstruction>");
  qRegisterMetaType<QList<DapVariable>>("QList<DapVariable>");
  cleanupBreakpoints();
}

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

void TestDap::testDapClientAdapterExitIsReported() {
  DapClient client;
  QSignalSpy errorSpy(&client, &DapClient::error);
  QSignalSpy terminatedSpy(&client, &DapClient::terminated);
  QVERIFY(errorSpy.isValid());
  QVERIFY(terminatedSpy.isValid());

#ifdef Q_OS_WIN
  const QString program = "cmd";
  const QStringList args = {"/C", "exit 42"};
#else
  const QString program = "sh";
  const QStringList args = {"-c",
                            "sleep 0.1; echo adapter failed >&2; exit 42"};
#endif

  QVERIFY(client.start(program, args));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Error, 5000);
  QCOMPARE(errorSpy.count(), 1);
  QCOMPARE(terminatedSpy.count(), 1);
  QVERIFY(errorSpy.takeFirst().at(0).toString().contains("exited"));
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
  const QList<Breakpoint> source =
      bm.breakpointsForFile("/test/structured.cpp");
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

  QVERIFY(adapters.count() >= 6);

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

  auto goAdapter = reg.adapter("go-delve");
  QVERIFY(goAdapter != nullptr);
  QCOMPARE(goAdapter->config().name, QString("Go (Delve)"));

  auto rustAdapter = reg.adapter("rust-codelldb");
  QVERIFY(rustAdapter != nullptr);
  QCOMPARE(rustAdapter->config().name, QString("Rust (CodeLLDB)"));
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
      fakePythonPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
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

void TestDap::testPythonLaunchArgumentsUseResolvedInterpreter() {
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
      fakePythonPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
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
  configuration.pythonMode = PythonProjectEnvironment::customInterpreterMode();
  configuration.pythonInterpreter = fakePythonPath;

  const QJsonObject launchArguments =
      pythonAdapter->launchArguments(configuration);
  QCOMPARE(launchArguments["python"].toString(), fakePythonPath);
  QCOMPARE(launchArguments["program"].toString(), QString("/tmp/example.py"));
  QCOMPARE(launchArguments["env"].toObject()["PYTHONUNBUFFERED"].toString(),
           QString("1"));
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

  QList<std::shared_ptr<IDebugAdapter>> goAdapters =
      reg.adaptersForFile("/test/main.go");
  QVERIFY(!goAdapters.isEmpty());

  QList<std::shared_ptr<IDebugAdapter>> rsAdapters =
      reg.adaptersForFile("/test/main.rs");
  QVERIFY(!rsAdapters.isEmpty());

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

  QList<std::shared_ptr<IDebugAdapter>> goAdapters =
      reg.adaptersForLanguage("go");
  QVERIFY(!goAdapters.isEmpty());

  QList<std::shared_ptr<IDebugAdapter>> rustAdapters =
      reg.adaptersForLanguage("rust");
  QVERIFY(!rustAdapters.isEmpty());
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
      fakeAdapterPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                           QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                           QFileDevice::ExeGroup | QFileDevice::ReadOther |
                           QFileDevice::ExeOther));

  const QString fakeNodePath = tempDir.filePath("node");
  QFile fakeNode(fakeNodePath);
  QVERIFY(fakeNode.open(QIODevice::WriteOnly | QIODevice::Text));
  fakeNode.write("#!/bin/sh\necho v20.11.0\nexit 0\n");
  fakeNode.close();
  QVERIFY(QFile::setPermissions(
      fakeNodePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                        QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                        QFileDevice::ExeGroup | QFileDevice::ReadOther |
                        QFileDevice::ExeOther));

  QJsonObject nodeSettings = DebugSettings::instance()
                                 .adapterSettings()["adapters"]
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

  QJsonObject nodeSettings = DebugSettings::instance()
                                 .adapterSettings()["adapters"]
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
  QCOMPARE(attachConfig["pid"].toInt(), 12345);

  QJsonObject remoteConfig =
      gdbAdapter->createAttachConfig(0, "192.168.1.100", 1234);
  QCOMPARE(remoteConfig["type"].toString(), QString("cppdbg"));
  QCOMPARE(remoteConfig["target"].toString(), QString("192.168.1.100:1234"));

  DebugConfiguration pidAttach;
  pidAttach.adapterId = "cppdbg-gdb";
  pidAttach.request = "attach";
  pidAttach.processId = 4242;
  pidAttach.program = "/path/to/program";
  const QJsonObject pidArgs = gdbAdapter->attachArguments(pidAttach);
  QVERIFY(pidArgs["pid"].isDouble());
  QCOMPARE(pidArgs["pid"].toInt(), 4242);
  QVERIFY(!pidArgs.contains("processId"));
  QCOMPARE(pidArgs["program"].toString(), QString("/path/to/program"));

  DebugConfiguration remoteAttach;
  remoteAttach.adapterId = "cppdbg-gdb";
  remoteAttach.request = "attach";
  remoteAttach.adapterConfig["miDebuggerServerAddress"] = "10.0.0.2:2345";
  const QJsonObject remoteArgs = gdbAdapter->attachArguments(remoteAttach);
  QCOMPARE(remoteArgs["target"].toString(), QString("10.0.0.2:2345"));
  QVERIFY(!remoteArgs.contains("miDebuggerServerAddress"));
  QVERIFY(!remoteArgs.contains("pid"));
  QVERIFY(!remoteArgs.contains("program"));

  DebugConfiguration hostPortAttach;
  hostPortAttach.adapterId = "cppdbg-gdb";
  hostPortAttach.request = "attach";
  hostPortAttach.host = "127.0.0.1";
  hostPortAttach.port = 1234;
  const QJsonObject hostPortArgs = gdbAdapter->attachArguments(hostPortAttach);
  QCOMPARE(hostPortArgs["target"].toString(), QString("127.0.0.1:1234"));
  QVERIFY(!hostPortArgs.contains("host"));
  QVERIFY(!hostPortArgs.contains("port"));

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

void TestDap::testGdbLaunchConfigUsesConfiguredDebugger() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  DebugSettings::instance().initialize(tempDir.path());

  auto gdbAdapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(gdbAdapter != nullptr);

  QJsonObject gdbSettings = DebugSettings::instance()
                                .adapterSettings()["adapters"]
                                .toObject()["cppdbg-gdb"]
                                .toObject();
  gdbSettings["miDebuggerPath"] = "/custom/tools/gdb";
  DebugSettings::instance().setAdapterSettings("cppdbg-gdb", gdbSettings);

  const DebugConfiguration configuration = DebugConfiguration::fromJson(
      gdbAdapter->createLaunchConfig("/path/to/program", "/path/to"));
  QCOMPARE(configuration.adapterConfig["miDebuggerPath"].toString(),
           QString("/custom/tools/gdb"));
  QCOMPARE(gdbAdapter->configForConfiguration(configuration).program,
           QString("/custom/tools/gdb"));

  gdbSettings["miDebuggerPath"] = "gdb";
  DebugSettings::instance().setAdapterSettings("cppdbg-gdb", gdbSettings);
  const QJsonObject defaultLaunch =
      gdbAdapter->createLaunchConfig("/path/to/program", "/path/to");
  QVERIFY(defaultLaunch["miDebuggerPath"].toString() !=
          QString("/custom/tools/gdb"));
}

void TestDap::testGoAdapterIntegration() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  auto goAdapter = reg.adapter("go-delve");
  QVERIFY(goAdapter != nullptr);

  DebugAdapterConfig cfg = goAdapter->config();
  QCOMPARE(cfg.id, QString("go-delve"));
  QCOMPARE(cfg.name, QString("Go (Delve)"));
  QCOMPARE(cfg.type, QString("go"));
  QVERIFY(cfg.arguments.contains("dap"));

  QVERIFY(cfg.languages.contains("go"));
  QVERIFY(cfg.extensions.contains(".go"));

  QVERIFY(cfg.supportsFunctionBreakpoints);
  QVERIFY(cfg.supportsConditionalBreakpoints);
  QVERIFY(cfg.supportsHitConditionalBreakpoints);
  QVERIFY(cfg.supportsLogPoints);
  QVERIFY(cfg.supportsRestart);

  QVERIFY(goAdapter->canDebug("/path/to/main.go"));
  QVERIFY(!goAdapter->canDebug("/path/to/main.py"));
  QVERIFY(goAdapter->supportsLanguage("go"));
  QVERIFY(!goAdapter->supportsLanguage("cpp"));

  QCOMPARE(goAdapter->documentationUrl(),
           QString("https://github.com/go-delve/delve"));

  QString installCmd = goAdapter->installCommand();
  QVERIFY(!installCmd.isEmpty());
  QVERIFY(installCmd.contains("dlv"));
}

void TestDap::testGoAdapterLaunchConfig() {
  auto goAdapter = DebugAdapterRegistry::instance().adapter("go-delve");
  QVERIFY(goAdapter != nullptr);

  QJsonObject launchConfig =
      goAdapter->createLaunchConfig("/path/to/main.go", "/path/to");
  QCOMPARE(launchConfig["type"].toString(), QString("go"));
  QCOMPARE(launchConfig["request"].toString(), QString("launch"));
  QCOMPARE(launchConfig["mode"].toString(), QString("debug"));
  QCOMPARE(launchConfig["program"].toString(), QString("/path/to/main.go"));
  QCOMPARE(launchConfig["cwd"].toString(), QString("/path/to"));

  QJsonObject attachConfig = goAdapter->createAttachConfig(12345, "", 0);
  QCOMPARE(attachConfig["type"].toString(), QString("go"));
  QCOMPARE(attachConfig["request"].toString(), QString("attach"));
  QCOMPARE(attachConfig["mode"].toString(), QString("local"));
  QCOMPARE(attachConfig["processId"].toInt(), 12345);

  QJsonObject remoteConfig =
      goAdapter->createAttachConfig(0, "192.168.1.100", 1234);
  QCOMPARE(remoteConfig["type"].toString(), QString("go"));
  QCOMPARE(remoteConfig["mode"].toString(), QString("remote"));
  QCOMPARE(remoteConfig["host"].toString(), QString("192.168.1.100"));
  QCOMPARE(remoteConfig["port"].toInt(), 1234);
}

void TestDap::testGoAdapterLookupByFile() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  QList<std::shared_ptr<IDebugAdapter>> goAdapters =
      reg.adaptersForFile("/project/main.go");
  QVERIFY(!goAdapters.isEmpty());

  bool foundDelve = false;
  for (const auto &adapter : goAdapters) {
    if (adapter->config().id == "go-delve") {
      foundDelve = true;
      break;
    }
  }
  QVERIFY(foundDelve);
}

void TestDap::testRustAdapterIntegration() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  auto rustAdapter = reg.adapter("rust-codelldb");
  QVERIFY(rustAdapter != nullptr);

  DebugAdapterConfig cfg = rustAdapter->config();
  QCOMPARE(cfg.id, QString("rust-codelldb"));
  QCOMPARE(cfg.name, QString("Rust (CodeLLDB)"));
  QCOMPARE(cfg.type, QString("lldb"));

  QVERIFY(cfg.languages.contains("rust"));
  QVERIFY(cfg.extensions.contains(".rs"));

  QVERIFY(cfg.supportsFunctionBreakpoints);
  QVERIFY(cfg.supportsConditionalBreakpoints);
  QVERIFY(cfg.supportsHitConditionalBreakpoints);
  QVERIFY(cfg.supportsLogPoints);

  QVERIFY(rustAdapter->canDebug("/path/to/main.rs"));
  QVERIFY(!rustAdapter->canDebug("/path/to/main.py"));
  QVERIFY(rustAdapter->supportsLanguage("rust"));
  QVERIFY(!rustAdapter->supportsLanguage("cpp"));

  QCOMPARE(rustAdapter->documentationUrl(),
           QString("https://github.com/vadimcn/codelldb"));

  QString installCmd = rustAdapter->installCommand();
  QVERIFY(!installCmd.isEmpty());
  QVERIFY(installCmd.contains("CodeLLDB"));
}

void TestDap::testRustAdapterLaunchConfig() {
  auto rustAdapter = DebugAdapterRegistry::instance().adapter("rust-codelldb");
  QVERIFY(rustAdapter != nullptr);

  QJsonObject launchConfig = rustAdapter->createLaunchConfig(
      "/path/to/target/debug/myapp", "/path/to");
  QCOMPARE(launchConfig["type"].toString(), QString("lldb"));
  QCOMPARE(launchConfig["request"].toString(), QString("launch"));
  QCOMPARE(launchConfig["program"].toString(),
           QString("/path/to/target/debug/myapp"));
  QCOMPARE(launchConfig["cwd"].toString(), QString("/path/to"));

  QJsonObject attachConfig = rustAdapter->createAttachConfig(12345, "", 0);
  QCOMPARE(attachConfig["type"].toString(), QString("lldb"));
  QCOMPARE(attachConfig["request"].toString(), QString("attach"));
  QCOMPARE(attachConfig["pid"].toInt(), 12345);

  QJsonObject remoteConfig =
      rustAdapter->createAttachConfig(0, "127.0.0.1", 4567);
  QCOMPARE(remoteConfig["type"].toString(), QString("lldb"));
  QCOMPARE(remoteConfig["request"].toString(), QString("attach"));
  QVERIFY(remoteConfig.contains("connect"));
  QCOMPARE(remoteConfig["connect"].toObject()["host"].toString(),
           QString("127.0.0.1"));
  QCOMPARE(remoteConfig["connect"].toObject()["port"].toInt(), 4567);
}

void TestDap::testRustAdapterLookupByFile() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();

  QList<std::shared_ptr<IDebugAdapter>> rsAdapters =
      reg.adaptersForFile("/project/src/main.rs");
  QVERIFY(!rsAdapters.isEmpty());

  bool foundCodeLldb = false;
  for (const auto &adapter : rsAdapters) {
    if (adapter->config().id == "rust-codelldb") {
      foundCodeLldb = true;
      break;
    }
  }
  QVERIFY(foundCodeLldb);
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

void TestDap::testDapSourceNameFallsBackToPath() {

  QJsonObject source;
  source["path"] = "/project/tools/analyze_mesh.py";

  QJsonObject json;
  json["id"] = 2;
  json["name"] = "main";
  json["line"] = 65;
  json["source"] = source;

  const DapStackFrame frame = DapStackFrame::fromJson(json);
  QCOMPARE(frame.source.name, QString("analyze_mesh.py"));
  QCOMPARE(frame.source.path, QString("/project/tools/analyze_mesh.py"));

  source["name"] = "custom label";
  json["source"] = source;
  QCOMPARE(DapStackFrame::fromJson(json).source.name, QString("custom label"));

  json["source"] = QJsonObject();
  QVERIFY(DapStackFrame::fromJson(json).source.name.isEmpty());
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
  config.pythonMode = "workspaceVenv";
  config.pythonVenvPath = "${workspaceFolder}/.venv";

  QJsonObject json = config.toJson();

  QCOMPARE(json["name"].toString(), QString("Test Config"));
  QCOMPARE(json["adapterId"].toString(), QString("python-debugpy"));
  QCOMPARE(json["type"].toString(), QString("debugpy"));
  QCOMPARE(json["request"].toString(), QString("launch"));
  QCOMPARE(json["program"].toString(), QString("/path/to/script.py"));
  QCOMPARE(json["args"].toArray().count(), 2);
  QCOMPARE(json["stopOnEntry"].toBool(), true);
  QCOMPARE(json["processId"].toString(), QString("${command:pickProcess}"));
  QCOMPARE(json["pythonMode"].toString(), QString("workspaceVenv"));
  QCOMPARE(json["pythonVenvPath"].toString(),
           QString("${workspaceFolder}/.venv"));
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
  json["pythonMode"] = "workspaceVenv";
  json["pythonVenvPath"] = "${workspaceFolder}/.venv";

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
  QCOMPARE(config.pythonMode, QString("workspaceVenv"));
  QCOMPARE(config.pythonVenvPath, QString("${workspaceFolder}/.venv"));
}

void TestDap::testConfigurationVariableSubstitution() {
  DebugConfigurationManager &mgr = DebugConfigurationManager::instance();
  mgr.setWorkspaceFolder("/home/user/project");

  DebugConfiguration config;
  config.name = "Test";
  config.pythonMode = "workspaceVenv";
  config.pythonVenvPath = "${workspaceFolder}/.venv";
  config.program = "${workspaceFolder}/main.py";
  config.cwd = "${workspaceFolder}";
  config.args = {"${fileBasename}", "${relativeFile}", "${venv}"};
  config.adapterConfig["connect"] =
      QJsonObject{{"host", "${workspaceFolder}"},
                  {"pathMappings",
                   QJsonArray{QJsonObject{
                       {"localRoot", "${workspaceFolder}/src"},
                       {"remoteRoot", "/app/${fileBasenameNoExtension}"}}}}};

  DebugConfiguration resolved =
      mgr.resolveVariables(config, "/home/user/project/src/app.py");

  QCOMPARE(resolved.program, QString("/home/user/project/main.py"));
  QCOMPARE(resolved.cwd, QString("/home/user/project"));
  QCOMPARE(resolved.args.at(0), QString("app.py"));
  QCOMPARE(resolved.args.at(1), QString("src/app.py"));
  QVERIFY(resolved.args.at(2).contains("/home/user/project/.venv"));
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
  QCOMPARE(compounds.first().configurations, QStringList({"Server", "Client"}));
  QVERIFY(compounds.first().stopAll);
}

void TestDap::testQuickConfigPreservesAdapterDefaults() {
  auto pythonAdapter =
      DebugAdapterRegistry::instance().adapter("python-debugpy");
  QVERIFY(pythonAdapter != nullptr);
  const DebugConfiguration pythonCfg =
      DebugConfiguration::fromJson(pythonAdapter->createLaunchConfig(
          "/home/user/project/app.py", "/home/user/project"));
  QCOMPARE(pythonAdapter->config().id, QString("python-debugpy"));
  QCOMPARE(pythonCfg.type, QString("debugpy"));
  QCOMPARE(pythonCfg.program, QString("/home/user/project/app.py"));
  QCOMPARE(pythonCfg.adapterConfig["console"].toString(),
           QString("integratedTerminal"));

  auto gdbAdapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(gdbAdapter != nullptr);
  const DebugConfiguration cppCfg =
      DebugConfiguration::fromJson(gdbAdapter->createLaunchConfig(
          "/home/user/project/main", "/home/user/project"));
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
      DebugConfigurationManager::instance().createQuickConfig("/tmp/example.py",
                                                              "py");
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

  QJsonObject nodeSettings = DebugSettings::instance()
                                 .adapterSettings()["adapters"]
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

QString TestDap::fakeAdapterPath() const {
  return QCoreApplication::applicationDirPath() + "/fakedap_adapter";
}

QByteArray TestDap::frameMessage(const QJsonObject &message) {
  const QByteArray content =
      QJsonDocument(message).toJson(QJsonDocument::Compact);
  return QString("Content-Length: %1\r\n\r\n")
      .arg(content.size())
      .toUtf8()
      .append(content);
}

bool TestDap::traceFileContains(const QString &path, const QByteArray &needle,
                                int *outCount) const {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (outCount) {
      *outCount = -1;
    }
    return false;
  }
  const QByteArray contents = file.readAll();
  int count = 0;
  for (int pos = contents.indexOf(needle); pos >= 0;
       pos = contents.indexOf(needle, pos + needle.size())) {
    ++count;
  }
  if (outCount) {
    *outCount = count;
  }
  return count > 0;
}

bool TestDap::waitForAdapterListening(QProcess *adapterProcess, int timeoutMs) {
  QElapsedTimer timer;
  timer.start();
  QByteArray stderrText;
  while (timer.elapsed() < timeoutMs) {
    stderrText += adapterProcess->readAllStandardError();
    if (stderrText.contains("listening")) {
      return true;
    }
    adapterProcess->waitForReadyRead(50);
  }
  return false;
}

void TestDap::testDapClientFramingHandlesUtf8AndChunkedDelivery() {
  DapClient client;
  QSignalSpy outputSpy(&client, &DapClient::output);
  QVERIFY(outputSpy.isValid());

  DapOutputEvent evt;
  evt.category = "console";
  evt.output = QString::fromUtf8("h\u00e9llo \u4e16\u754c \U0001F680");
  QJsonObject message;
  message["seq"] = 1;
  message["type"] = "event";
  message["event"] = "output";
  message["body"] =
      QJsonObject{{"category", evt.category}, {"output", evt.output}};

  client.feedAdapterData(frameMessage(message));

  QCOMPARE(outputSpy.count(), 1);
  const DapOutputEvent received =
      outputSpy.first().at(0).value<DapOutputEvent>();
  QCOMPARE(received.output, evt.output);

  const QByteArray framed = frameMessage(message);
  for (int i = 0; i < framed.size(); i += 3) {
    client.feedAdapterData(framed.mid(i, 3));
  }

  QCOMPARE(outputSpy.count(), 2);
}

void TestDap::testDapClientFakeAdapterFullSession() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString tracePath = dir.filePath("trace.jsonl");

  DapClient client;
  client.setAdapterMetadata("fake", "fake");

  QSignalSpy initializedSpy(&client, &DapClient::initialized);
  QSignalSpy launchedSpy(&client, &DapClient::launched);
  QSignalSpy threadsSpy(&client, &DapClient::threadsReceived);
  QSignalSpy stackSpy(&client, &DapClient::stackTraceReceived);
  QSignalSpy scopesSpy(&client, &DapClient::scopesReceived);
  QSignalSpy variablesSpy(&client, &DapClient::variablesReceived);
  QSignalSpy evaluateSpy(&client, &DapClient::evaluateResult);
  QSignalSpy variableSetSpy(&client, &DapClient::variableSet);
  QSignalSpy exceptionInfoSpy(&client, &DapClient::exceptionInfoReceived);

  QVERIFY(client.start(fakeAdapterPath(), {"--trace", tracePath}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  QCOMPARE(initializedSpy.count(), 1);
  QVERIFY(client.capabilities().contains("supportsExceptionInfoRequest"));

  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);
  QCOMPARE(launchedSpy.count(), 1);

  client.getThreads();
  QTRY_COMPARE_WITH_TIMEOUT(threadsSpy.count(), 1, 5000);
  QCOMPARE(client.currentThreadId(), 1);

  client.getStackTrace(1);
  QTRY_COMPARE_WITH_TIMEOUT(stackSpy.count(), 1, 5000);
  QCOMPARE(stackSpy.at(0).at(0).toInt(), 1);
  QCOMPARE(stackSpy.at(0).at(2).toInt(), 1);

  client.getScopes(100);
  QTRY_COMPARE_WITH_TIMEOUT(scopesSpy.count(), 1, 5000);

  client.getVariables(1000);
  QTRY_COMPARE_WITH_TIMEOUT(variablesSpy.count(), 1, 5000);
  const QList<DapVariable> variables =
      variablesSpy.at(0).at(1).value<QList<DapVariable>>();
  QCOMPARE(variables.size(), 2);
  QCOMPARE(variables.at(1).name,
           QString::fromUtf8("\u6f22\u5b57_\u00e9\u00e8"));
  QCOMPARE(variables.at(1).value, QString::fromUtf8("v\u00e0lue \U0001F680"));

  client.evaluate(QString::fromUtf8("x + \u00fc"), 100, "repl");
  QTRY_COMPARE_WITH_TIMEOUT(evaluateSpy.count(), 1, 5000);
  QCOMPARE(evaluateSpy.at(0).at(1).toString(), QString::fromUtf8("x + \u00fc"));

  client.setVariable(1000, "x", "42");
  QTRY_COMPARE_WITH_TIMEOUT(variableSetSpy.count(), 1, 5000);

  client.exceptionInfo(1);
  QTRY_COMPARE_WITH_TIMEOUT(exceptionInfoSpy.count(), 1, 5000);
  const DapExceptionInfo info =
      exceptionInfoSpy.at(0).at(1).value<DapExceptionInfo>();
  QCOMPARE(info.exceptionId, QString("ZeroDivisionError"));
  QCOMPARE(info.details.typeName, QString("ZeroDivisionError"));
  QCOMPARE(exceptionInfoSpy.at(0).at(0).toInt(), 1);

  client.stop(false);

  QTRY_VERIFY_WITH_TIMEOUT(traceFileContains(tracePath, "\"disconnect\""),
                           5000);
  int disconnectCount = 0;
  traceFileContains(tracePath, "\"disconnect\"", &disconnectCount);
  QCOMPARE(disconnectCount, 1);
  int terminateCount = 0;
  traceFileContains(tracePath, "\"terminate\"", &terminateCount);
  QCOMPARE(terminateCount, 0);
}

void TestDap::testDapClientFakeAdapterExceptionInfo() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  DapClient client;

  QSignalSpy errorSpy(&client, &DapClient::exceptionInfoError);
  QSignalSpy infoSpy(&client, &DapClient::exceptionInfoReceived);
  QVERIFY(errorSpy.isValid());
  QVERIFY(infoSpy.isValid());

  client.start(fakeAdapterPath(),
               {"--trace", dir.filePath("t.jsonl"), "--caps",
                R"({"supportsConfigurationDoneRequest":true})"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);

  client.exceptionInfo(7);
  QCOMPARE(errorSpy.count(), 1);
  QCOMPARE(infoSpy.count(), 0);

  client.stop(false);
}

void TestDap::testDapClientCapabilityGating() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString tracePath = dir.filePath("trace.jsonl");

  DapClient client;
  client.start(
      fakeAdapterPath(),
      {"--trace", tracePath, "--caps",
       R"({"supportsConfigurationDoneRequest":false,"supportsTerminateRequest":false,"supportsSetVariable":false})"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);

  QVERIFY(!client.supportsConfigurationDoneRequest());
  QVERIFY(!client.supportsTerminateRequest());
  QVERIFY(!client.supportsSetVariable());

  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);

  QSignalSpy variableSetSpy(&client, &DapClient::variableSet);
  client.configurationDone();
  client.terminate();
  client.setVariable(1000, "x", "1");

  QCOMPARE(variableSetSpy.count(), 1);

  QTRY_VERIFY_WITH_TIMEOUT(traceFileContains(tracePath, "\"disconnect\""),
                           5000);
  int disconnectCount = 0;
  traceFileContains(tracePath, "\"disconnect\"", &disconnectCount);
  QCOMPARE(disconnectCount, 1);

  int configurationDoneCount = 0;
  traceFileContains(tracePath, "\"configurationDone\"",
                    &configurationDoneCount);
  QCOMPARE(configurationDoneCount, 0);

  int terminateCount = 0;
  traceFileContains(tracePath, "\"terminate\"", &terminateCount);
  QCOMPARE(terminateCount, 0);

  int setVariableCount = 0;
  traceFileContains(tracePath, "\"setVariable\"", &setVariableCount);
  QCOMPARE(setVariableCount, 0);

  client.stop(true);
}

void TestDap::testDapClientSocketTransport() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const quint16 port = static_cast<quint16>(
      20000 + (QCoreApplication::applicationPid() % 20000));

  QProcess adapterProcess;
  adapterProcess.setProgram(fakeAdapterPath());
  adapterProcess.setArguments({"--listen", QString::number(port), "--trace",
                               dir.filePath("trace.jsonl")});
  adapterProcess.start();
  QVERIFY(adapterProcess.waitForStarted(5000));
  QVERIFY(waitForAdapterListening(&adapterProcess, 10000));

  DapClient client;
  QSignalSpy initializedSpy(&client, &DapClient::initialized);
  QVERIFY(client.startSocket("127.0.0.1", port));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 10000);
  QCOMPARE(initializedSpy.count(), 1);
  QVERIFY(client.isSocketTransport());

  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);

  QTRY_VERIFY_WITH_TIMEOUT(
      traceFileContains(dir.filePath("trace.jsonl"), "\"launch\""), 5000);

  client.stop(true);
  adapterProcess.waitForFinished(3000);
  adapterProcess.kill();
}

void TestDap::testDapClientSocketTransportDisconnectIsReported() {
  const quint16 port = static_cast<quint16>(
      20000 + ((QCoreApplication::applicationPid() + 1) % 20000));

  QProcess adapterProcess;
  adapterProcess.setProgram(fakeAdapterPath());
  adapterProcess.setArguments({"--listen", QString::number(port)});
  adapterProcess.start();
  QVERIFY(adapterProcess.waitForStarted(5000));
  QVERIFY(waitForAdapterListening(&adapterProcess, 10000));

  DapClient client;
  QSignalSpy terminatedSpy(&client, &DapClient::terminated);
  QVERIFY(client.startSocket("127.0.0.1", port));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 10000);

  adapterProcess.kill();
  adapterProcess.waitForFinished(3000);

  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Terminated, 5000);
  QVERIFY(terminatedSpy.count() >= 1);
}

namespace {
QJsonObject outputEventMessage(int seq, const QString &text) {
  QJsonObject message;
  message["seq"] = seq;
  message["type"] = "event";
  message["event"] = "output";
  message["body"] = QJsonObject{{"category", "console"}, {"output", text}};
  return message;
}

class PreferenceTestAdapter : public IDebugAdapter {
public:
  PreferenceTestAdapter(const QString &id, const QStringList &languages,
                        bool available)
      : m_available(available) {
    m_config.id = id;
    m_config.name = id;
    m_config.type = id;
    m_config.languages = languages;
    m_config.extensions = {".lpzz"};
  }
  DebugAdapterConfig config() const override { return m_config; }
  bool isAvailable() const override { return m_available; }
  QString statusMessage() const override { return {}; }
  QJsonObject createLaunchConfig(const QString &,
                                 const QString &) const override {
    return {};
  }
  QJsonObject createAttachConfig(int, const QString &, int) const override {
    return {};
  }

private:
  DebugAdapterConfig m_config;
  bool m_available;
};
} // namespace

void TestDap::testDapClientFramingSkipsNonProtocolOutput() {
  DapClient client;
  QSignalSpy outputSpy(&client, &DapClient::output);

  client.feedAdapterData("warning: stray adapter output\r\n\r\nmore noise");
  client.feedAdapterData(frameMessage(outputEventMessage(1, "first")));
  QCOMPARE(outputSpy.count(), 1);

  QByteArray mixed = "garbage between frames ";
  mixed += frameMessage(outputEventMessage(2, "second"));
  mixed += "Content-Length: nope\r\n";
  mixed += frameMessage(outputEventMessage(3, "third"));
  client.feedAdapterData(mixed);
  QCOMPARE(outputSpy.count(), 3);
  QCOMPARE(outputSpy.at(2).at(0).value<DapOutputEvent>().output,
           QString("third"));
}

void TestDap::testDapClientFramingSkipsOversizedFrameAcrossChunks() {
  DapClient client;
  QSignalSpy outputSpy(&client, &DapClient::output);

  const int hugeSize = 3 * 1024 * 1024;
  QByteArray hugeBody = "{\"type\":\"event\",\"event\":\"output\",\"body\":{";
  hugeBody += "\"output\":\"";
  hugeBody += QByteArray(hugeSize - hugeBody.size() - 3, 'x');
  hugeBody += "\"}}";
  QCOMPARE(hugeBody.size(), hugeSize);

  client.feedAdapterData("Content-Length: " + QByteArray::number(hugeSize) +
                         "\r\n\r\n");
  const int chunk = 64 * 1024;
  for (int i = 0; i < hugeBody.size(); i += chunk) {
    client.feedAdapterData(hugeBody.mid(i, chunk));
  }
  QCOMPARE(outputSpy.count(), 0);

  client.feedAdapterData(frameMessage(outputEventMessage(9, "after")));
  QCOMPARE(outputSpy.count(), 1);
  QCOMPARE(outputSpy.at(0).at(0).value<DapOutputEvent>().output,
           QString("after"));
}

void TestDap::testDapClientFramingDrainsLargeBursts() {
  DapClient client;
  QSignalSpy outputSpy(&client, &DapClient::output);

  QByteArray burst;
  for (int i = 0; i < 250; ++i) {
    burst += frameMessage(outputEventMessage(i + 1, QString::number(i)));
  }
  client.feedAdapterData(burst);
  QTRY_COMPARE_WITH_TIMEOUT(outputSpy.count(), 250, 5000);
  QCOMPARE(outputSpy.last().at(0).value<DapOutputEvent>().output,
           QString("249"));
}

void TestDap::testDapClientStoppedWithoutThreadKeepsCurrentThread() {
  DapClient client;

  QJsonObject stopped;
  stopped["seq"] = 1;
  stopped["type"] = "event";
  stopped["event"] = "stopped";
  stopped["body"] = QJsonObject{{"reason", "breakpoint"}, {"threadId", 5}};
  client.feedAdapterData(frameMessage(stopped));
  QCOMPARE(client.currentThreadId(), 5);

  stopped["seq"] = 2;
  stopped["body"] =
      QJsonObject{{"reason", "pause"}, {"allThreadsStopped", true}};
  client.feedAdapterData(frameMessage(stopped));
  QCOMPARE(client.currentThreadId(), 5);
}

void TestDap::testDapClientOversizedResponseReleasesPendingRequest() {
  DapClient client;
  QSignalSpy variablesSpy(&client, &DapClient::variablesReceived);

  QVERIFY(client.start(fakeAdapterPath(), {"--huge-variables"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);

  client.getVariables(1000);
  QTRY_COMPARE_WITH_TIMEOUT(variablesSpy.count(), 1, 10000);
  QCOMPARE(variablesSpy.at(0).at(0).toInt(), 1000);
  QVERIFY(variablesSpy.at(0).at(1).value<QList<DapVariable>>().isEmpty());

  client.getVariables(1000);
  QTRY_COMPARE_WITH_TIMEOUT(variablesSpy.count(), 2, 10000);

  client.stop(false);
}

void TestDap::testDapClientDropsResponsesCancelledByResume() {
  DapClient client;
  QSignalSpy stackSpy(&client, &DapClient::stackTraceReceived);
  QSignalSpy continuedSpy(&client, &DapClient::continued);

  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);

  client.getStackTrace(1);
  client.continueExecution(1);
  QTRY_COMPARE_WITH_TIMEOUT(continuedSpy.count(), 1, 5000);
  QTest::qWait(100);
  QCOMPARE(stackSpy.count(), 0);

  client.getStackTrace(1);
  QTRY_COMPARE_WITH_TIMEOUT(stackSpy.count(), 1, 5000);
  QCOMPARE(stackSpy.at(0).at(0).toInt(), 1);

  client.stop(false);
}

void TestDap::testDapClientKeepsInteractiveEvaluationDuringFunctionCall() {
  DapClient client;
  QVERIFY(client.start(fakeAdapterPath(),
                       {"--stop-on-launch", "--hang-on", "evaluate"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);
  const int passive = client.evaluate("oldValue", 100, "watch");
  const int interactive = client.evaluate("compare(a, b)", 100, "watch", true);
  QSignalSpy result(&client, &DapClient::evaluateResponse);
  QSignalSpy finished(&client, &DapClient::interactiveEvaluationFinished);
  auto event = [&](const QString &name, const QJsonObject &body) {
    client.feedAdapterData(frameMessage(QJsonObject{
        {"seq", 99}, {"type", "event"}, {"event", name}, {"body", body}}));
  };
  auto response = [&](int seq) {
    client.feedAdapterData(frameMessage(
        QJsonObject{{"seq", 100},
                    {"type", "response"},
                    {"command", "evaluate"},
                    {"request_seq", seq},
                    {"success", true},
                    {"body", QJsonObject{{"result", "42"}, {"type", "int"}}}}));
  };
  event("continued", {{"threadId", 1}, {"allThreadsContinued", true}});
  QVERIFY(client.hasPendingInteractiveEvaluation());
  QVERIFY(client.evaluationExecutedTarget(interactive));
  event("stopped", {{"reason", "function call"},
                    {"threadId", 1},
                    {"allThreadsStopped", true}});
  response(passive);
  QCOMPARE(result.count(), 0);
  response(interactive);
  QCOMPARE(result.count(), 1);
  QCOMPARE(result.first().first().toInt(), interactive);
  QCOMPARE(result.first().at(2).toString(), QString("42"));
  QCOMPARE(finished.count(), 1);
  QVERIFY(!client.hasPendingInteractiveEvaluation());
  const int cancelled = client.evaluate("compare(a, b)", 100, "watch", true);
  client.stepOver(1);
  response(cancelled);
  QCOMPARE(result.count(), 1);
  QVERIFY(!client.hasPendingInteractiveEvaluation());
  client.stop(false);
}

void TestDap::testDapClientRestartWrapsLaunchArguments() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString tracePath = dir.filePath("trace.jsonl");

  DapClient client;
  QVERIFY(
      client.start(fakeAdapterPath(), {"--trace", tracePath, "--caps",
                                       R"({"supportsRestartRequest":true})"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);

  client.restart();
  QTRY_VERIFY_WITH_TIMEOUT(
      traceFileContains(tracePath, "\"arguments\":{\"arguments\":{"), 5000);

  client.stop(false);
}

void TestDap::testDapClientRestartDropsOldInspectionResponses() {
  DapClient client;
  QVERIFY(
      client.start(fakeAdapterPath(), {"--stop-on-launch", "--caps",
                                       R"({"supportsRestartRequest":true})"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);

  QSignalSpy stack(&client, &DapClient::stackTraceReceived);
  QSignalSpy values(&client, &DapClient::evaluateResponse);
  QSignalSpy restarting(&client, &DapClient::restarting);
  client.getStackTrace(1);
  client.evaluate("oldValue", 100, "watch");
  client.restart();
  QCOMPARE(restarting.count(), 1);

  // The responses were already on their way when Restart was requested.
  // Wait for a new response as a barrier; only that response may be delivered.
  const int fresh = client.evaluate("freshValue", 100, "watch");
  QTRY_COMPARE_WITH_TIMEOUT(values.count(), 1, 5000);
  QCOMPARE(values.first().first().toInt(), fresh);
  QCOMPARE(stack.count(), 0);
  client.stop(false);
}

void TestDap::testWatchValuesFollowClientLifecycle_data() {
  QTest::addColumn<QString>("action");
  QTest::newRow("continue") << QString("continue");
  QTest::newRow("native-restart") << QString("restart");
  QTest::newRow("stop") << QString("stop");
  QTest::newRow("adapter-destroyed") << QString("destroy");
}

void TestDap::testWatchValuesFollowClientLifecycle() {
  QFETCH(QString, action);
  auto &watches = WatchManager::instance();
  watches.clearAll();
  auto cleanup = qScopeGuard([&]() {
    watches.setDapClient(nullptr);
    watches.clearAll();
  });
  auto client = std::make_unique<DapClient>();
  QVERIFY(
      client->start(fakeAdapterPath(), {"--stop-on-launch", "--caps",
                                        R"({"supportsRestartRequest":true})"}));
  QTRY_COMPARE_WITH_TIMEOUT(client->state(), DapClient::State::Ready, 5000);
  client->launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client->state(), DapClient::State::Stopped, 5000);
  watches.setDapClient(client.get());
  const int id = watches.addWatch("matrix");
  watches.evaluateWatch(id, 100);
  QTRY_VERIFY_WITH_TIMEOUT(!watches.watch(id).value.isEmpty() &&
                               watches.watch(id).value != "<not evaluated>",
                           5000);
  QCOMPARE(watches.watch(id).variablesReference, 2000);
  QCOMPARE(watches.watch(id).type, QString("list"));
  watches.evaluateWatch(id, 100);

  if (action == "continue")
    client->continueExecution(1);
  else if (action == "restart")
    client->restart();
  else if (action == "stop")
    client->stop(false);
  else
    client.reset();

  QTRY_COMPARE_WITH_TIMEOUT(watches.watch(id).value, QString("<not evaluated>"),
                            5000);
  QCOMPARE(watches.watch(id).expression, QString("matrix"));
  QVERIFY(watches.watch(id).type.isEmpty());
  QCOMPARE(watches.watch(id).variablesReference, 0);
  QVERIFY(!watches.watch(id).isError);
}

void TestDap::testNativeRestartClearsDebugPanel() {
  auto &watches = WatchManager::instance();
  watches.clearAll();
  auto cleanup = qScopeGuard([&]() {
    watches.setDapClient(nullptr);
    watches.clearAll();
  });
  DapClient client;
  QVERIFY(
      client.start(fakeAdapterPath(), {"--stop-on-launch", "--caps",
                                       R"({"supportsRestartRequest":true})"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  watches.setDapClient(&client);
  DebugPanel panel;
  panel.setDapClient(&client);
  const int id = watches.addWatch("oldValue");
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);
  auto *variables = panel.findChild<QTreeWidget *>("debugVariablesTree");
  auto *stack = panel.findChild<QTreeWidget *>("debugCallStackTree");
  auto *watchTree = panel.findChild<QTreeWidget *>("debugWatchTree");
  QVERIFY(variables);
  QVERIFY(stack);
  QVERIFY(watchTree);
  QTRY_VERIFY_WITH_TIMEOUT(variables->topLevelItemCount() > 0, 5000);
  QTRY_VERIFY_WITH_TIMEOUT(!watches.watch(id).value.isEmpty() &&
                               watches.watch(id).value != "<not evaluated>",
                           5000);

  client.restart();
  QCOMPARE(variables->topLevelItemCount(), 0);
  QCOMPARE(stack->topLevelItemCount(), 0);
  QCOMPARE(watchTree->topLevelItem(0)->text(1), QString("<not evaluated>"));
  QCOMPARE(watchTree->topLevelItem(0)->childCount(), 0);
  client.stop(false);
}

void TestDap::testDapClientServerTransport() {
  DapClient client;
  QSignalSpy initializedSpy(&client, &DapClient::initialized);

  QVERIFY(client.startServer(fakeAdapterPath(), {"--listen-any"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 10000);
  QCOMPARE(initializedSpy.count(), 1);
  QVERIFY(client.isSocketTransport());

  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);

  client.stop(true);
  QCOMPARE(client.state(), DapClient::State::Disconnected);
}

void TestDap::testDapClientServerTransportReportsEarlyExit() {
  DapClient client;
  QSignalSpy errorSpy(&client, &DapClient::error);
  QSignalSpy terminatedSpy(&client, &DapClient::terminated);

  QVERIFY(client.startServer(fakeAdapterPath(), {"--listen", "1"}));
  QTRY_COMPARE_WITH_TIMEOUT(terminatedSpy.count(), 1, 10000);
  QCOMPARE(client.state(), DapClient::State::Error);
  QVERIFY(!errorSpy.isEmpty());
}

void TestDap::testBreakpointVerificationPairsByIndex() {
  cleanupBreakpoints();
  BreakpointManager &bm = BreakpointManager::instance();
  DapClient client;
  bm.setDapClient(&client);

  const QString path = "/nonexistent/lightpad/bp_index_test.cpp";
  Breakpoint first;
  first.filePath = path;
  first.line = 10;
  const int firstId = bm.addBreakpoint(first);
  Breakpoint second;
  second.filePath = path;
  second.line = 20;
  const int secondId = bm.addBreakpoint(second);

  bm.syncFileBreakpoints(path);

  DapBreakpoint moved;
  moved.id = 7;
  moved.verified = true;
  moved.line = 12;
  DapBreakpoint failed;
  failed.id = 8;
  failed.verified = false;
  failed.message = "No code at this line";
  bm.updateVerification(path, {moved, failed});

  QVERIFY(bm.breakpoint(firstId).verified);
  QCOMPARE(bm.breakpoint(firstId).boundLine, 12);
  QVERIFY(!bm.breakpoint(secondId).verified);
  QCOMPARE(bm.breakpoint(secondId).verificationMessage,
           QString("No code at this line"));

  DapBreakpoint changed;
  changed.id = 8;
  changed.verified = true;
  changed.line = 21;
  bm.updateVerification(path, {changed});
  QVERIFY(bm.breakpoint(secondId).verified);
  QCOMPARE(bm.breakpoint(secondId).boundLine, 21);
  QVERIFY(bm.breakpoint(secondId).verificationMessage.isEmpty());
  QCOMPARE(bm.breakpoint(firstId).boundLine, 12);

  bm.resetVerification();
  bm.setDapClient(nullptr);
  cleanupBreakpoints();
}

void TestDap::testPreferredAdapterPrefersLanguageSpecific() {
  DebugAdapterRegistry &reg = DebugAdapterRegistry::instance();
  reg.registerAdapter(std::make_shared<PreferenceTestAdapter>(
      "aaa-generic-test", QStringList{"lpzz", "other"}, true));
  reg.registerAdapter(std::make_shared<PreferenceTestAdapter>(
      "zzz-specific-test", QStringList{"lpzz"}, true));

  auto preferred = reg.preferredAdapterForFile("/tmp/main.lpzz");
  QVERIFY(preferred != nullptr);
  QCOMPARE(preferred->config().id, QString("zzz-specific-test"));
  preferred = reg.preferredAdapterForLanguage("lpzz");
  QVERIFY(preferred != nullptr);
  QCOMPARE(preferred->config().id, QString("zzz-specific-test"));

  reg.registerAdapter(std::make_shared<PreferenceTestAdapter>(
      "zzz-specific-test", QStringList{"lpzz"}, false));
  preferred = reg.preferredAdapterForFile("/tmp/main.lpzz");
  QVERIFY(preferred != nullptr);
  QCOMPARE(preferred->config().id, QString("aaa-generic-test"));

  reg.unregisterAdapter("aaa-generic-test");
  reg.unregisterAdapter("zzz-specific-test");

  auto goAdapter = reg.adapter("go-delve");
  QVERIFY(goAdapter != nullptr);
  QVERIFY(goAdapter->config().serverTransport);
  QVERIFY(goAdapter->config().arguments.contains("--listen=127.0.0.1:0"));
}

void TestDap::testCommandLineArgumentsRoundTrip() {
  const QStringList args = {"plain", "with space", "say \"hi\"", "tab\there"};
  const QString joined =
      DebugConfigurationManager::joinCommandLineArguments(args);
  QCOMPARE(DebugConfigurationManager::splitCommandLineArguments(joined), args);
  QCOMPARE(DebugConfigurationManager::joinCommandLineArguments({"a", "b"}),
           QString("a b"));
}

void TestDap::testConfigurationRenameRefusesExistingName() {
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  const QList<DebugConfiguration> original = manager.allConfigurations();

  DebugConfiguration a;
  a.name = "Rename Test A";
  a.program = "/a";
  DebugConfiguration b;
  b.name = "Rename Test B";
  b.program = "/b";
  manager.addConfiguration(a);
  manager.addConfiguration(b);

  DebugConfiguration renamed = a;
  renamed.name = b.name;
  QVERIFY(!manager.updateConfiguration(a.name, renamed));
  QCOMPARE(manager.configuration(b.name).program, QString("/b"));
  QCOMPARE(manager.configuration(a.name).program, QString("/a"));

  manager.replaceConfigurations(original);
}

void TestDap::testDebugConfigurationDialogValidationAndCancel() {
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  const QList<DebugConfiguration> original = manager.allConfigurations();

  DebugConfiguration a;
  a.name = "Dialog Test A";
  a.type = "cppdbg";
  a.request = "launch";
  a.program = "/a";
  a.args = {"one arg", "two"};
  a.adapterConfig["MIMode"] = "gdb";
  DebugConfiguration b;
  b.name = "Dialog Test B";
  b.type = "cppdbg";
  b.request = "launch";
  b.program = "/b";
  manager.replaceConfigurations({a, b});

  {
    DebugConfigurationDialog dialog;
    QCOMPARE(dialog.m_currentConfigName, a.name);
    QCOMPARE(dialog.m_argsEdit->text(), QString("\"one arg\" two"));

    dialog.m_configList->setCurrentRow(1);
    dialog.m_configList->setCurrentRow(0);
    QCOMPARE(manager.configuration(a.name).args, a.args);

    dialog.m_adapterConfigEdit->setPlainText("{ not json");
    dialog.m_configList->setCurrentRow(1);
    QCOMPARE(dialog.m_configList->currentRow(), 0);
    QVERIFY(!dialog.m_validationLabel->isHidden());
    QVERIFY(!dialog.saveCurrentToModel());
    QCOMPARE(manager.configuration(a.name).adapterConfig.value("MIMode"),
             QJsonValue("gdb"));

    dialog.m_adapterConfigEdit->setPlainText("{\"MIMode\": \"lldb\"}");
    dialog.m_nameEdit->setText(b.name);
    QVERIFY(!dialog.saveCurrentToModel());
    QCOMPARE(manager.configuration(b.name).program, QString("/b"));

    dialog.m_nameEdit->setText("Dialog Test Renamed");
    QVERIFY(dialog.saveCurrentToModel());
    QVERIFY(dialog.m_validationLabel->isHidden());
    QCOMPARE(manager.configuration("Dialog Test Renamed").program,
             QString("/a"));

    dialog.reject();
  }

  QCOMPARE(manager.allConfigurations().size(), 2);
  QCOMPARE(manager.configuration(a.name).program, QString("/a"));
  QCOMPARE(manager.configuration(a.name).adapterConfig.value("MIMode"),
           QJsonValue("gdb"));
  QVERIFY(manager.configuration("Dialog Test Renamed").name.isEmpty());

  manager.replaceConfigurations(original);
}

void TestDap::testDebugConfigurationDialogDebouncesStatusProbe() {
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  const QList<DebugConfiguration> original = manager.allConfigurations();
  manager.replaceConfigurations({});

  {
    DebugConfigurationDialog dialog;
    dialog.m_typeCombo->setCurrentText("debugpy");
    for (const QChar ch : QString("/tmp/prog")) {
      dialog.m_programEdit->setText(dialog.m_programEdit->text() + ch);
    }
    QVERIFY(dialog.m_statusTimer->isActive());
    QVERIFY(dialog.m_adapterStatusLabel->text().contains("checking"));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.m_statusTimer->isActive(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(
        !dialog.m_adapterStatusLabel->text().contains("checking"), 30000);
    QVERIFY(!dialog.m_statusCache.isEmpty());
  }

  manager.replaceConfigurations(original);
}

void TestDap::testDapClientCompletionsRoundTrip() {
  DapClient client;
  QSignalSpy spy(&client, &DapClient::completionsReceived);
  client.start(fakeAdapterPath(),
               {"--caps", R"({"supportsCompletionsRequest":true})"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  QVERIFY(client.supportsCompletionsRequest());

  const int seq = client.completions("foo", 4, 100);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  QCOMPARE(spy.at(0).at(0).toInt(), seq);
  const auto items = spy.at(0).at(1).value<QList<DapCompletionItem>>();
  QCOMPARE(items.size(), 2);
  QCOMPARE(items.at(0).label, QString("foo"));
  QCOMPARE(items.at(1).insertText(), QString("foobar"));
  QCOMPARE(items.at(0).length, 3);
  client.stop(false);
}

void TestDap::testDapClientDataBreakpointInfoRoundTrip() {
  DapClient client;
  QSignalSpy okSpy(&client, &DapClient::dataBreakpointInfoReceived);
  client.start(fakeAdapterPath(),
               {"--caps", R"({"supportsDataBreakpoints":true})"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);

  client.dataBreakpointInfo(1000, "x", 100);
  QTRY_COMPARE_WITH_TIMEOUT(okSpy.count(), 1, 5000);
  DapDataBreakpointInfo info = okSpy.at(0).at(1).value<DapDataBreakpointInfo>();
  QVERIFY(info.isValid());
  QCOMPARE(info.dataId, QString("addr:x"));
  QCOMPARE(info.accessTypes, (QStringList{"read", "write", "readWrite"}));

  client.dataBreakpointInfo(1000, "unwatchable", 100);
  QTRY_COMPARE_WITH_TIMEOUT(okSpy.count(), 2, 5000);
  info = okSpy.at(1).at(1).value<DapDataBreakpointInfo>();
  QVERIFY(!info.isValid());
  QCOMPARE(info.description, QString("cannot watch"));
  client.stop(false);
}

void TestDap::testDapClientMemoryAndDisassembly() {
  DapClient client;
  QSignalSpy memSpy(&client, &DapClient::memoryReceived);
  QSignalSpy disSpy(&client, &DapClient::disassemblyReceived);
  client.start(
      fakeAdapterPath(),
      {"--caps",
       R"({"supportsReadMemoryRequest":true,"supportsDisassembleRequest":true})"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);

  client.readMemory("0x1000", 32);
  QTRY_COMPARE_WITH_TIMEOUT(memSpy.count(), 1, 5000);
  const auto chunk = memSpy.at(0).at(1).value<DapMemoryChunk>();
  QCOMPARE(chunk.address, QString("0x1000"));
  QCOMPARE(chunk.data.size(), 32);
  QCOMPARE(static_cast<unsigned char>(chunk.data.at(31)), 31u);

  client.disassemble("0x1000", 3);
  QTRY_COMPARE_WITH_TIMEOUT(disSpy.count(), 1, 5000);
  const auto ins =
      disSpy.at(0).at(1).value<QList<DapDisassembledInstruction>>();
  QCOMPARE(ins.size(), 3);
  QCOMPARE(ins.at(2).instruction, QString("ret"));
  QCOMPARE(ins.at(0).address, QString("0x1000"));
  client.stop(false);
}

void TestDap::testDapClientUnsupportedInspectionRequestsFailFast() {
  QTemporaryDir dir;
  const QString tracePath = dir.filePath("t.jsonl");
  DapClient client;
  QSignalSpy compSpy(&client, &DapClient::completionsFailed);
  QSignalSpy dataSpy(&client, &DapClient::dataBreakpointInfoFailed);
  QSignalSpy memSpy(&client, &DapClient::memoryFailed);
  QSignalSpy disSpy(&client, &DapClient::disassemblyFailed);
  client.start(fakeAdapterPath(),
               {"--trace", tracePath, "--caps",
                R"({"supportsConfigurationDoneRequest":true})"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);

  client.completions("a", 1);
  client.dataBreakpointInfo(1, "a");
  client.readMemory("0x1", 4);
  client.disassemble("0x1", 4);
  QTRY_COMPARE_WITH_TIMEOUT(compSpy.count(), 1, 2000);
  QTRY_COMPARE_WITH_TIMEOUT(dataSpy.count(), 1, 2000);
  QTRY_COMPARE_WITH_TIMEOUT(memSpy.count(), 1, 2000);
  QTRY_COMPARE_WITH_TIMEOUT(disSpy.count(), 1, 2000);

  int count = 0;
  traceFileContains(tracePath, "\"completions\"", &count);
  QCOMPARE(count, 0);
  traceFileContains(tracePath, "\"readMemory\"", &count);
  QCOMPARE(count, 0);
  client.stop(false);
}

void TestDap::testDapClientRequestTimeout() {
  DapClient client;
  client.setRequestTimeoutMs(300);
  QSignalSpy timeoutSpy(&client, &DapClient::requestTimedOut);
  QSignalSpy evalErrorSpy(&client, &DapClient::evaluateError);
  QSignalSpy evalOkSpy(&client, &DapClient::evaluateResult);
  QSignalSpy varSpy(&client, &DapClient::variablesReceived);
  client.start(fakeAdapterPath(), {"--hang-on", "evaluate"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);

  client.evaluate("1+1", 100);
  QTRY_COMPARE_WITH_TIMEOUT(evalErrorSpy.count(), 1, 3000);
  QCOMPARE(timeoutSpy.count(), 1);
  QCOMPARE(timeoutSpy.at(0).at(0).toString(), QString("evaluate"));
  QVERIFY(evalErrorSpy.at(0).at(1).toString().contains("timed out"));
  QCOMPARE(evalOkSpy.count(), 0);

  client.getVariables(1000);
  QTRY_COMPARE_WITH_TIMEOUT(varSpy.count(), 1, 3000);
  QCOMPARE(timeoutSpy.count(), 1);
  client.stop(false);
}

void TestDap::testDapClientAdapterCrashTerminatesSession() {
  DapClient client;
  QSignalSpy terminatedSpy(&client, &DapClient::terminated);
  client.start(fakeAdapterPath(), {"--crash-on", "threads"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);

  client.getThreads();
  QTRY_VERIFY_WITH_TIMEOUT(terminatedSpy.count() >= 1, 5000);
  QVERIFY(client.state() == DapClient::State::Terminated ||
          client.state() == DapClient::State::Error ||
          client.state() == DapClient::State::Disconnected);

  client.getThreads();
  client.evaluate("x", 1);
  client.stop(false);
}

void TestDap::testDapClientForwardsRunInTerminalKind() {
  QTemporaryDir dir;
  const QString tracePath = dir.filePath("t.jsonl");
  DapClient client;
  QSignalSpy ritSpy(&client, &DapClient::runInTerminalRequested);
  client.start(fakeAdapterPath(),
               {"--trace", tracePath, "--run-in-terminal", "external"});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(ritSpy.count(), 1, 5000);
  QCOMPARE(ritSpy.at(0).at(1).toString(), QString("external"));
  QCOMPARE(ritSpy.at(0).at(2).toString(), QString("Fake Debuggee"));
  QCOMPARE(ritSpy.at(0).at(3).toStringList(), (QStringList{"echo", "hello"}));

  client.respondToRunInTerminal(ritSpy.at(0).at(0).toInt(), true, 4242);
  QTRY_VERIFY_WITH_TIMEOUT(traceFileContains(tracePath, "4242"), 5000);
  client.stop(false);
}

void TestDap::testNestedValueFormatting() {
  QCOMPARE(DebugValueFormatter::format("[[1, 2], [3, 4]]"),
           QString("[\n    [1, 2],\n    [3, 4]\n]"));
  QCOMPARE(
      DebugValueFormatter::format("{'matrix': [[1], [2]], 'text': 'a,b:[c]'}"),
      QString("{\n    'matrix': [\n        [1],\n        [2]\n    ],\n    "
              "'text': 'a,b:[c]'\n}"));
  const QString escaped = QStringLiteral("['it\\'s [fine], really', [1, 2]]");
  QVERIFY(
      DebugValueFormatter::format(escaped).contains("'it\\'s [fine], really'"));
  QCOMPARE(DebugValueFormatter::format("[1, 2, ..."), QString("[1, 2, ..."));
  QCOMPARE(DebugValueFormatter::format("'a,b:[c]'"), QString("'a,b:[c]'"));
  QCOMPARE(DebugValueFormatter::format("[[1], [2])"), QString("[[1], [2])"));
  QCOMPARE(DebugValueFormatter::format("[1, 2, 3]"), QString("[1, 2, 3]"));
  QVERIFY(DebugValueFormatter::format(QString(70000, 'x')).contains("limited"));
  QCOMPARE(DebugValueFormatter::tableCell("np.float32(1.75)"), QString("1.75"));
  QCOMPARE(DebugValueFormatter::tableCell("np.float64(1.2345678901234567)"),
           QString("1.2345678901234567"));
  QCOMPARE(DebugValueFormatter::tableCell("np.int64(-123)"), QString("-123"));
  QCOMPARE(DebugValueFormatter::tableCell("'np.float32(1.75)'"),
           QString("'np.float32(1.75)'"));
}

void TestDap::testNativeScalarFormats() {
  using namespace DebugValueFormatter;
  QCOMPARE(scalar("65 'A'", "char", DisplayFormat::Decimal), QString("65"));
  QCOMPARE(scalar("65 'A'", "char", DisplayFormat::Hexadecimal),
           QString("0x41"));
  QCOMPARE(scalar("65", "int", DisplayFormat::Binary), QString("0b1000001"));
  QCOMPARE(scalar("65", "int", DisplayFormat::Character), QString("'A'"));
  QCOMPARE(scalar("10", "int", DisplayFormat::Character), QString("'\\n'"));
  QCOMPARE(scalar("0", "int", DisplayFormat::Character), QString("'\\0'"));
  QCOMPARE(scalar("-1", "signed char", DisplayFormat::Character),
           QString("'\\xff'"));
  QCOMPARE(scalar("'\\n'", "char", DisplayFormat::Decimal), QString("10"));
  QCOMPARE(scalar("18446744073709551615", "unsigned long long",
                  DisplayFormat::Hexadecimal),
           QString("0xffffffffffffffff"));
  QCOMPARE(scalar("-9223372036854775808", "long long", DisplayFormat::Decimal),
           QString("-9223372036854775808"));
  QCOMPARE(scalar("1", "double", DisplayFormat::Character), QString("1"));
  QCOMPARE(scalar("0x1234", "int *", DisplayFormat::Decimal),
           QString("0x1234"));
  QCOMPARE(scalar("0x1234 \"hello\"", "const char *", DisplayFormat::Natural),
           QString("\"hello\""));
  QCOMPARE(childExpression("records", "Record [2]", "[1]"),
           QString("(records)[1]"));
  QVERIFY(childExpression("map", "std::map<int, int>", "[1]").isEmpty());
  QCOMPARE(childExpression("node", "Node *", "value"), QString("(*(node))"));
  QCOMPARE(childExpression("node", "Node *", "next"), QString("(node)->next"));
  QCOMPARE(childExpression("record", "Record", "id"), QString("(record).id"));
  QCOMPARE(arrayLength("int [2][3]"), 2);
  QCOMPARE(arrayLength("char (&)[4]"), 4);
  QCOMPARE(arrayLength("int (*[2])[3]"), 2);
  QCOMPARE(arrayLength("int (*[2])(int)"), 2);
  QCOMPARE(arrayLength("int (*)[3]"), 0);
  QCOMPARE(arrayLength("int [][3]"), 0);
  QCOMPARE(arrayLength("std::vector<int [3]>"), 0);
  QCOMPARE(pointeeType("int (* const)[3]"), QString("int [3]"));
  QCOMPARE(pointerDepth("char (**)[3]"), 2);
  QCOMPARE(pointerDepth("char **"), 2);
  QCOMPARE(pointerDepth("char * const * volatile"), 2);
  QCOMPARE(pointerDepth("std::vector<char *>"), 0);
  QCOMPARE(pointeeType("char **"), QString("char *"));
  QVERIFY(isCharacterPointer("const unsigned char *"));
  QVERIFY(!isCharacterPointer("char (*)[3]"));
  QVERIFY(!isCharacterPointer("std::vector<char> *"));
  QVERIFY(isCharacterArray("const char [3]"));
  QVERIFY(!isCharacterArray("char [3][2]"));
  QCOMPARE(childExpression("row", "int (&)[3]", "value"), QString("(row)"));
  QCOMPARE(childExpression("record", "Record<char *>", "id"),
           QString("(record).id"));
  QCOMPARE(
      scalar("0x1234 -> 0x4567 \"wkue\"", "char **", DisplayFormat::Natural),
      QString("0x1234 -> 0x4567 \"wkue\""));
  QByteArray bytes;
  QVERIFY(stringBytes("\"A\\000B\\n\"", &bytes));
  QCOMPARE(bytes, QByteArray("A\0B\n", 4));
  QVERIFY(stringBytes("0x1234 \"\\\\\\\"\\x41\"", &bytes));
  QCOMPARE(bytes, QByteArray("\\\"A"));
  QVERIFY(!stringBytes("\"truncated\"...", &bytes));
}

void TestDap::testInspectorCorrelatesRequests() {
  DapClient client;
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);
  QSignalSpy scopeSpy(&client, &DapClient::variablesReceived);
  QSignalSpy inspectionSpy(&client, &DapClient::inspectionVariablesReceived);
  client.getVariables(3000);
  const int pageOne = client.inspectVariables(3000, "indexed", 0, 100);
  const int pageTwo = client.inspectVariables(3000, "indexed", 100, 100);
  QVERIFY(pageOne > 0 && pageTwo > pageOne);
  QTRY_COMPARE_WITH_TIMEOUT(inspectionSpy.count(), 2, 5000);
  QCOMPARE(scopeSpy.count(), 1);
  QCOMPARE(inspectionSpy.at(0).at(0).toInt(), pageOne);
  QCOMPARE(inspectionSpy.at(1).at(0).toInt(), pageTwo);
  QCOMPARE(inspectionSpy.at(1).at(1).value<QList<DapVariable>>().at(1).name,
           QString("100"));
  client.stop(false);
}

void TestDap::testInspectorMatrixAndRaggedRows() {
  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);
  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  DapVariable matrix;
  matrix.name = "matrix";
  matrix.evaluateName = "matrix";
  matrix.type = "list";
  matrix.value = "[[0.25, 1.25, 2.25], [10.25, 11.25], [20.25, 21.25, 22.25]]";
  matrix.variablesReference = 2000;
  matrix.evaluateName = "matrix";
  inspector.setValue(matrix);
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  auto *tabs = inspector.findChild<QTabWidget *>("debugValueTabs");
  QVERIFY(table && tabs);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(2, 2) && table->item(2, 2)->text() == "22.25", 5000);
  QCOMPARE(table->rowCount(), 3);
  QCOMPARE(table->columnCount(), 3);
  QCOMPARE(table->item(1, 2)->text(), QString());
  QCOMPARE(tabs->currentIndex(), 1);
  table->setRangeSelected(QTableWidgetSelectionRange(0, 0, 1, 1), true);
  for (auto *button : inspector.findChildren<QPushButton *>()) {
    if (button->text() == "Copy")
      button->click();
  }
  QCOMPARE(QApplication::clipboard()->text(),
           QString("0.25\t1.25\n10.25\t11.25"));
  auto *filter = inspector.findChild<QLineEdit *>("debugValueFilter");
  filter->setText("22.25");
  QVERIFY(table->isRowHidden(0));
  QVERIFY(!table->isRowHidden(2));
  filter->clear();
  QVERIFY(!table->isRowHidden(0));
  QSignalSpy watchSpy(&inspector, &DebugValueInspector::addWatchRequested);
  for (auto *button : inspector.findChildren<QPushButton *>()) {
    if (button->text() == "Add to Watches")
      button->click();
  }
  QCOMPARE(watchSpy.count(), 1);
  QCOMPARE(watchSpy.first().first().toString(), QString("matrix"));
  client.stop(false);
}

void TestDap::testInspectorPagingAndSnapshot_data() {
  QTest::addColumn<int>("reference");
  QTest::newRow("adapter-pages") << 3000;
  QTest::newRow("adapter-ignores-paging") << 4000;
  QTest::newRow("adapter-ignores-start") << 5000;
  QTest::newRow("adapter-more-reference") << 6000;
}

void TestDap::testInspectorPagingAndSnapshot() {
  QFETCH(int, reference);
  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);
  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  DapVariable list;
  list.name = "large";
  list.type = "list";
  list.variablesReference = reference;
  list.indexedVariables = 250;
  inspector.setValue(list);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  auto *root = tree->topLevelItem(0);
  QTRY_COMPARE_WITH_TIMEOUT(root->childCount(), 101, 5000);
  auto *next = root->child(100);
  tree->itemActivated(next, 0);
  QTRY_COMPARE_WITH_TIMEOUT(root->childCount(), 201, 5000);
  QCOMPARE(root->child(100)->text(0), QString("100"));
  next = root->child(200);
  tree->itemActivated(next, 0);
  QTRY_COMPARE_WITH_TIMEOUT(root->childCount(), 250, 5000);
  QCOMPARE(root->child(249)->text(1), QString("249"));
  QSignalSpy responseSpy(&client, &DapClient::inspectionVariablesReceived);
  client.continueExecution(1);
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);
  QVERIFY(inspector.findChild<QLabel *>("debugValueStatus")
              ->text()
              .contains("Snapshot"));
  root->setExpanded(false);
  root->setExpanded(true);
  QCOMPARE(responseSpy.count(), 0);
  QCOMPARE(root->childCount(), 250);
  QCOMPARE(client.inspectVariables(3000), 0);
  client.stop(false);
}

namespace {
QTreeWidgetItem *childNamed(QTreeWidgetItem *parent, const QString &name) {
  if (parent)
    for (int i = 0; i < parent->childCount(); ++i)
      if (parent->child(i)->text(0) == name)
        return parent->child(i);
  return nullptr;
}
} // namespace

void TestDap::testInspectorPagesNamedChildren_data() {
  QTest::addColumn<QString>("adapterId");
  QTest::addColumn<QString>("adapterType");
  QTest::addColumn<QString>("type");
  QTest::addColumn<int>("reference");
  QTest::newRow("gdb-map") << "cppdbg-gdb" << "cppdbg"
                           << "std::map<std::string, int>" << 7000;
  QTest::newRow("debugpy-dict-returns-all")
      << "python-debugpy" << "debugpy" << "dict" << 7100;
  QTest::newRow("rust-record-ignores-start")
      << "rust-codelldb" << "lldb" << "Record<[i32; 4]>" << 7200;
}

void TestDap::testInspectorPagesNamedChildren() {
  QFETCH(QString, adapterId);
  QFETCH(QString, adapterType);
  QFETCH(QString, type);
  QFETCH(int, reference);
  DapClient client;
  client.setAdapterMetadata(adapterId, adapterType);
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE(client.state(), DapClient::State::Ready);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE(client.state(), DapClient::State::Stopped);

  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  DapVariable value;
  value.name = value.evaluateName = "collection";
  value.type = type;
  value.variablesReference = reference;
  if (reference == 7000) {
    value.hasNamedVariables = true;
    value.namedVariables = 205;
  } else if (reference == 7200) {
    value.memoryReference = "0x1234";
  }
  inspector.setValue(value);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  auto *root = tree->topLevelItem(0);
  QVERIFY(inspector.findChild<QWidget *>("debugArrayControls")->isHidden());
  QTRY_COMPARE(root->childCount(), 101);
  QCOMPARE(root->child(99)->text(0), QString("key-99"));
  tree->itemActivated(root->child(100), 0);
  QTRY_COMPARE(root->childCount(), 201);
  QCOMPARE(root->child(100)->text(0), QString("key-100"));
  tree->itemActivated(root->child(200), 0);
  QTRY_COMPARE(root->childCount(), 205);
  QCOMPARE(root->child(204)->text(0), QString("key-204"));
  client.stop(false);
}

void TestDap::testInspectorMixedCollections_data() {
  QTest::addColumn<int>("reference");
  QTest::newRow("sparse-javascript-array") << 7300;
  QTest::newRow("javascript-map-with-repeated-properties") << 7301;
  QTest::newRow("go-map-with-expandable-keys-and-values") << 7400;
}

void TestDap::testInspectorMixedCollections() {
  QFETCH(int, reference);
  const bool goMap = reference == 7400;
  const bool jsMap = reference == 7301;
  DapClient client;
  client.setAdapterMetadata(goMap ? "go-delve" : "node-debug",
                            goMap ? "go" : "node");
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE(client.state(), DapClient::State::Ready);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE(client.state(), DapClient::State::Stopped);

  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  DapVariable value;
  value.name = value.evaluateName = "mixed";
  value.type = goMap ? "map[Key]Record" : jsMap ? "object" : "Array";
  value.variablesReference = reference;
  value.hasIndexedVariables = true;
  value.hasNamedVariables = !jsMap;
  value.indexedVariables = goMap ? 101 : jsMap ? 105 : 205;
  value.namedVariables = jsMap ? 0 : 1;
  inspector.setValue(value);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  auto *root = tree->topLevelItem(0);
  if (jsMap) {
    QTRY_COMPARE(root->childCount(), 102);
    QCOMPARE(root->child(100)->text(0), QString("size"));
    tree->itemActivated(root->child(101), 0);
    QTRY_COMPARE(root->childCount(), 106);
    QCOMPARE(root->child(105)->text(0), QString("104"));
    client.stop(false);
    return;
  }
  QTRY_COMPARE(root->childCount(), goMap ? 202 : 3);

  auto *properties = childNamed(root, "Named children");
  QVERIFY(properties);
  properties->setExpanded(true);
  QTRY_COMPARE(properties->childCount(), goMap ? 1 : 3);
  QTreeWidgetItem *nested = nullptr;
  if (goMap) {
    QCOMPARE(properties->child(0)->text(0), QString("len()"));
    nested = childNamed(root, "[key 0]");
    QVERIFY(childNamed(root, "[val 0]"));
    tree->itemActivated(root->child(201), 0);
    QTRY_COMPARE(root->childCount(), 203);
    QCOMPARE(root->child(201)->text(0), QString("[key 100]"));
    QCOMPARE(root->child(202)->text(0), QString("[val 100]"));
  } else {
    QCOMPARE(root->child(1)->text(0), QString("2"));
    QCOMPARE(properties->child(1)->text(0), QString("special variables"));
    nested = childNamed(properties, "label");
    QSignalSpy pages(&client, &DapClient::inspectionVariablesReceived);
    tree->itemActivated(root->child(2), 0);
    QTRY_COMPARE(pages.count(), 1);
    QCOMPARE(root->childCount(), 3);
    QVERIFY(root->child(2)->text(0).startsWith("Load next"));
    tree->itemActivated(root->child(2), 0);
    QTRY_COMPARE(root->child(2)->text(0), QString("204"));
  }
  QVERIFY(nested);
  nested->setExpanded(true);
  QTRY_VERIFY(childNamed(nested, "tuple"));
  auto *tuple = childNamed(nested, "tuple");
  tuple->setExpanded(true);
  QTRY_COMPARE(tuple->childCount(), 1);
  QCOMPARE(tuple->child(0)->text(0), QString("Some(λ)"));
  QCOMPARE(tuple->child(0)->text(1), QString("42"));
  auto *cycle = childNamed(nested, "self");
  QVERIFY(cycle);
  QVERIFY(cycle->text(1).contains("already in this branch"));
  QCOMPARE(cycle->childIndicatorPolicy(), QTreeWidgetItem::DontShowIndicator);
  client.stop(false);
}

void TestDap::testInspectorNestedNativeBuffers() {
  DapClient client;
  client.setAdapterMetadata("cppdbg-gdb", "cppdbg");
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE(client.state(), DapClient::State::Ready);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE(client.state(), DapClient::State::Stopped);

  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  DapVariable value;
  value.name = value.evaluateName = "record";
  value.type = "struct Record";
  value.variablesReference = 7600;
  QSignalSpy evaluated(&client, &DapClient::evaluateValueReceived);
  inspector.setValue(value, 100);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  auto *root = tree->topLevelItem(0);
  QTRY_COMPARE(root->childCount(), 4);
  QCOMPARE(evaluated.count(), 0);

  auto *data = childNamed(root, "data");
  auto *other = childNamed(root, "other");
  QVERIFY(data && other);
  data->setExpanded(true);
  other->setExpanded(true);
  QTRY_COMPARE(evaluated.count(), 6);
  QCOMPARE(data->childCount(), 3);
  QCOMPARE(data->child(1)->text(1), QString("0"));
  QCOMPARE(data->child(2)->text(1), QString("66"));
  QCOMPARE(other->child(2)->text(1), QString("68"));

  auto *row = childNamed(root, "row");
  QVERIFY(row);
  row->setExpanded(true);
  QTRY_COMPARE(row->childCount(), 1);
  auto *array = row->child(0);
  array->setExpanded(true);
  QTRY_COMPARE(evaluated.count(), 9);
  QCOMPARE(array->childCount(), 3);
  QCOMPARE(array->child(2)->text(1), QString("30"));

  auto *tuple = childNamed(root, "tuple");
  QVERIFY(tuple);
  tuple->setExpanded(true);
  QTRY_COMPARE(tuple->childCount(), 1);
  tuple->child(0)->setExpanded(true);
  QTRY_COMPARE(tuple->child(0)->childCount(), 3);
  QCOMPARE(tuple->child(0)->child(1)->text(1), QString("0 '\\0'"));
  QCOMPARE(evaluated.count(), 9);
  client.stop(false);
}

void TestDap::testInspectorReportsErrors() {
  DapClient client;
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);
  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  DapVariable missing;
  missing.name = "missing";
  missing.variablesReference = 9999;
  inspector.setValue(missing);
  auto *status = inspector.findChild<QLabel *>("debugValueStatus");
  QTRY_VERIFY_WITH_TIMEOUT(
      status->text().contains("Object is no longer available"), 5000);
  client.stop(false);
}

void TestDap::testInspectorWithRealDebugpy() {
  const QString python = QStandardPaths::findExecutable("python3");
  QProcess probe;
  probe.start(python, {"-c", "import debugpy"});
  if (!probe.waitForFinished(10000) || probe.exitCode() != 0)
    QSKIP("debugpy is not installed");
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QFile script(dir.filePath("matrix.py"));
  QVERIFY(script.open(QIODevice::WriteOnly));
  script.write("matrix = [[7.0, 4.88, 2.67, 933.52], [4.88, 4.3432], [2.67, "
               "1.4981, 1.0791, 423.24]]\n"
               "records = {f'key-{i:03}': {'leaf': (i, {'tags': {2, 3}})} "
               "for i in range(205)}\n"
               "records['key-204']['leaf'] = (999, {'tags': {2, 3}})\n"
               "print(matrix, len(records))\n");
  script.close();
  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");
  QSignalSpy adapterReady(&client, &DapClient::adapterInitialized);
  QSignalSpy stopped(&client, &DapClient::stopped);
  QSignalSpy stack(&client, &DapClient::stackTraceReceived);
  QSignalSpy evaluated(&client, &DapClient::evaluateValueReceived);
  QVERIFY(client.start(python, {"-m", "debugpy.adapter"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 15000);
  client.launch(QJsonObject{{"program", script.fileName()},
                            {"cwd", dir.path()},
                            {"console", "internalConsole"}});
  QTRY_COMPARE_WITH_TIMEOUT(adapterReady.count(), 1, 15000);
  DapSourceBreakpoint breakpoint;
  breakpoint.line = 3;
  client.setBreakpoints(script.fileName(), {breakpoint});
  client.configurationDone();
  QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 15000);
  client.getStackTrace(client.currentThreadId());
  QTRY_COMPARE_WITH_TIMEOUT(stack.count(), 1, 5000);
  const auto frames = stack.first().at(1).value<QList<DapStackFrame>>();
  QVERIFY(!frames.isEmpty());
  client.evaluate("matrix", frames.first().id, "repl");
  QTRY_COMPARE_WITH_TIMEOUT(evaluated.count(), 1, 5000);
  auto matrix = evaluated.first().at(1).value<DapVariable>();
  matrix.name = "matrix";
  QVERIFY2(matrix.variablesReference > 0, qPrintable(matrix.value));
  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  inspector.setValue(matrix);
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItem(0)->childCount() > 0, 5000);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(2, 3) && table->item(2, 3)->text() == "423.24", 10000);
  QCOMPARE(table->item(0, 3)->text(), QString("933.52"));
  QCOMPARE(table->item(1, 3)->text(), QString());

  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_INSPECTOR_SCREENSHOT")) {
    inspector.applyTheme(Theme());
    inspector.resize(1000, 600);
    inspector.show();
    QTest::qWait(100);
    inspector.grab().save(
        qEnvironmentVariable("LIGHTPAD_INSPECTOR_SCREENSHOT"));
  }
  client.evaluate("records", frames.first().id, "watch");
  QTRY_COMPARE_WITH_TIMEOUT(evaluated.count(), 2, 5000);
  auto records = evaluated.last().at(1).value<DapVariable>();
  records.name = records.evaluateName = "records";
  inspector.setValue(records, frames.first().id);
  auto *root = tree->topLevelItem(0);
  QTRY_COMPARE(root->childCount(), 101);
  tree->itemActivated(root->child(100), 0);
  QTRY_COMPARE(root->childCount(), 201);
  tree->itemActivated(root->child(200), 0);
  const auto lastKey = [&]() {
    return childNamed(tree->topLevelItem(0), "'key-204'");
  };
  const auto leaf = [&]() { return childNamed(lastKey(), "'leaf'"); };
  QTRY_VERIFY(lastKey());
  lastKey()->setExpanded(true);
  QTRY_VERIFY(leaf());
  leaf()->setExpanded(true);
  QTRY_VERIFY(leaf()->childCount() >= 2);
  QCOMPARE(leaf()->child(0)->text(1), QString("204"));

  auto *dict = leaf()->child(1);
  dict->setExpanded(true);
  QTRY_VERIFY(childNamed(dict, "'tags'"));
  auto *tags = childNamed(dict, "'tags'");
  tags->setExpanded(true);
  QTRY_VERIFY(tags->childCount() >= 2);
  QSet<QString> tagValues;
  for (int i = 0; i < tags->childCount(); ++i)
    tagValues.insert(tags->child(i)->text(1));
  QVERIFY(tagValues.contains("2") && tagValues.contains("3"));

  client.stepOver(client.currentThreadId());
  QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 2, 10000);
  QTRY_VERIFY_WITH_TIMEOUT(leaf() && leaf()->childCount() >= 2 &&
                               leaf()->child(0)->text(1) == "999",
                           10000);
  QVERIFY(lastKey()->isExpanded() && leaf()->isExpanded());
  client.stop(true);
}

namespace {
bool waitForDebugSignal(QSignalSpy &spy, int count = 1, int timeout = 15000) {
  QElapsedTimer timer;
  timer.start();
  while (spy.count() < count && timer.elapsed() < timeout) {
    spy.wait(qMax(1, timeout - static_cast<int>(timer.elapsed())));
  }
  return spy.count() >= count;
}

// Fixture breakpoints are located by code, not comments: the repository's
// comment stripper would otherwise erase any marker.
int firstLineContaining(const QString &path, const QByteArrayList &needles) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return 0;
  const auto lines = file.readAll().split('\n');
  for (int i = 0; i < lines.size(); ++i)
    for (const QByteArray &needle : needles)
      if (lines.at(i).contains(needle))
        return i + 1;
  return 0;
}

int graphicsInspectionLine(const QString &path) {
  return firstLineContaining(
      path, {"scene.model[0][0] = 2.0f;", "coordinates[0, 0] = 99.0"});
}

bool currentDebugFrame(DapClient &client, DapStackFrame *frame) {
  QSignalSpy stack(&client, &DapClient::stackTraceReceived);
  client.getStackTrace(client.currentThreadId());
  if (!waitForDebugSignal(stack))
    return false;
  const auto frames = stack.last().at(1).value<QList<DapStackFrame>>();
  if (frames.isEmpty())
    return false;
  *frame = frames.first();
  return true;
}

bool evaluateDebugObject(DapClient &client, int frame,
                         const QString &expression, DapVariable *value) {
  QSignalSpy evaluated(&client, &DapClient::evaluateValueReceived);
  QSignalSpy failed(&client, &DapClient::evaluateResponseError);
  const int request = client.evaluate(expression, frame, "watch");
  QElapsedTimer timer;
  timer.start();
  auto result = [&]() -> int {
    for (int i = 0; i < evaluated.size(); ++i)
      if (evaluated.at(i).at(0).toInt() == request)
        return i;
    return -1;
  };
  auto requestFailed = [&]() {
    return std::any_of(
        failed.cbegin(), failed.cend(),
        [request](const auto &args) { return args.at(0).toInt() == request; });
  };
  while (result() < 0 && timer.elapsed() < 15000 && !requestFailed())
    evaluated.wait(qMax(1, 15000 - static_cast<int>(timer.elapsed())));
  if (result() < 0) {
    if (!failed.isEmpty())
      qWarning() << failed.last();
    return false;
  }
  *value = evaluated.at(result()).at(1).value<DapVariable>();
  value->name = expression;
  value->evaluateName = expression;
  return true;
}

QString nativeCollectionSkipReason() {
  const auto adapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  if (QStandardPaths::findExecutable("c++").isEmpty() || !adapter ||
      !adapter->isAvailable())
    return "C++ collections need a C++ compiler and usable GDB DAP";
  return {};
}

struct NativeCollectionRun {
  QTemporaryDir dir;
  DapClient client;
  DapStackFrame frame;
  QString source;
  QString error;

  bool start(bool c = false) {
    source = c ? QFINDTESTDATA("../fixtures/debugger/native_collections.c")
               : QFINDTESTDATA("../fixtures/debugger/native_collections.cpp");
    const QString binary = dir.filePath("native_collections");
    QProcess compiler;
    compiler.start(
        c ? "cc" : "c++",
        {"-g", "-O0", c ? "-std=c11" : "-std=c++17", source, "-o", binary});
    if (!compiler.waitForFinished(30000) || compiler.exitCode() != 0) {
      error = QString::fromUtf8(compiler.readAllStandardError());
      return false;
    }
    client.setAdapterMetadata("cppdbg-gdb", "cppdbg");
    QSignalSpy ready(&client, &DapClient::initialized);
    QSignalSpy initialized(&client, &DapClient::adapterInitialized);
    QSignalSpy stopped(&client, &DapClient::stopped);
    if (!client.start(QStandardPaths::findExecutable("gdb"),
                      {"--interpreter=dap"}) ||
        !waitForDebugSignal(ready) || !waitForDebugSignal(initialized)) {
      error = "GDB failed to initialize";
      return false;
    }
    DapSourceBreakpoint breakpoint;
    breakpoint.line =
        firstLineContaining(source, {"int checkpoint = fixed[0];"});
    client.setBreakpoints(source, {breakpoint});
    client.configurationDone();
    client.launch(QJsonObject{{"program", binary}, {"cwd", dir.path()}});
    if (!waitForDebugSignal(stopped) || !currentDebugFrame(client, &frame)) {
      error = "GDB did not stop in the collection fixture";
      return false;
    }
    return true;
  }
};
} // namespace

void TestDap::testNativeCollections_data() {
  QTest::addColumn<QString>("expression");
  QTest::addColumn<int>("rowCount");
  QTest::addColumn<int>("columnCount");
  QTest::addColumn<QString>("first");
  QTest::addColumn<QString>("last");
  QTest::newRow("c-array") << "fixed" << 6 << 1 << "65" << "-1";
  QTest::newRow("c-matrix") << "matrix" << 2 << 3 << "1" << "6";
  QTest::newRow("vector") << "values" << 4 << 1 << "12" << "48";
  QTest::newRow("nested-vector") << "rows" << 2 << 3 << "7" << "11";
  QTest::newRow("string-vector")
      << "names" << 3 << 1 << "\"alpha\"" << "\"beta\"";
  QTest::newRow("char-pointer-array")
      << "words" << 3 << 1 << "\"alpha\"" << "\"\"";
  QTest::newRow("struct-array")
      << "records" << 2 << 2 << "1" << "\"second\\000\"";
  QTest::newRow("dynamic-char-pointer-array")
      << "wordPointers" << 3 << 1 << "\"alpha\"" << "\"\"";
  QTest::newRow("character-buffer") << "characters" << 6 << 1 << "65" << "39";
  QTest::newRow("std-string-with-nul") << "text" << 4 << 1 << "65" << "10";
  QTest::newRow("char-pointer-string") << "message" << 5 << 1 << "104" << "111";
}

void TestDap::testNativeCollections() {
  const QString skip = nativeCollectionSkipReason();
  if (!skip.isEmpty())
    QSKIP(qPrintable(skip));
  QFETCH(QString, expression);
  QFETCH(int, rowCount);
  QFETCH(int, columnCount);
  QFETCH(QString, first);
  QFETCH(QString, last);
  NativeCollectionRun run;
  QVERIFY2(run.start(), qPrintable(run.error));
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, expression, &value));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  inspector.setValue(value, run.frame.id);
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  auto *tabs = inspector.findChild<QTabWidget *>("debugValueTabs");
  auto *format = inspector.findChild<QComboBox *>("debugValueDisplayFormat");
  if (expression == "wordPointers") {
    inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(3);
    inspector.findChild<QPushButton *>("debugArrayApply")->click();
  }
  if (expression == "characters" || expression == "text" ||
      expression == "message")
    format->setCurrentIndex(1);
  tabs->setCurrentIndex(1);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(rowCount - 1, columnCount - 1) &&
          table->item(rowCount - 1, columnCount - 1)->text() == last,
      10000);
  QCOMPARE(table->rowCount(), rowCount);
  QCOMPARE(table->columnCount(), columnCount);
  QCOMPARE(table->item(0, 0)->text(), first);
  if (expression == "text")
    QCOMPARE(table->item(1, 0)->text(), QString("0"));
  if (expression == "fixed") {
    format->setCurrentIndex(2);
    QCOMPARE(table->item(0, 0)->text(), QString("0x41"));
    format->setCurrentIndex(3);
    QCOMPARE(table->item(0, 0)->text(), QString("0b1000001"));
    format->setCurrentIndex(4);
    QCOMPARE(table->item(0, 0)->text(), QString("'A'"));
  }
  if (expression == "names")
    QCOMPARE(table->item(1, 0)->text(), QString("\"\""));
  if (expression == "rows")
    QCOMPARE(table->item(0, 2)->text(), QString());
  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_NATIVE_SCREENSHOT")) {
    inspector.applyTheme(Theme());
    inspector.resize(1000, 620);
    inspector.show();
    QTest::qWait(50);
    inspector.grab().save(qEnvironmentVariable("LIGHTPAD_NATIVE_SCREENSHOT") +
                          "-" + QTest::currentDataTag() + ".png");
  }
  run.client.stop(true);
}

void TestDap::testNativeBufferPagingAndSnapshot() {
  const QString skip = nativeCollectionSkipReason();
  if (!skip.isEmpty())
    QSKIP(qPrintable(skip));
  NativeCollectionRun run;
  QVERIFY2(run.start(), qPrintable(run.error));
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "heap", &value));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  inspector.setValue(value, run.frame.id);
  inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(240);
  inspector.findChild<QSpinBox *>("debugArrayColumns")->setValue(5);
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  auto *root = tree->topLevelItem(0);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(19, 4) && table->item(19, 4)->text() == "297", 10000);
  QCOMPARE(root->childCount(), 101);
  tree->itemActivated(root->child(100), 0);
  QTRY_VERIFY_WITH_TIMEOUT(
      root->childCount() == 201 && root->child(199)->text(1) == "597", 10000);
  tree->itemActivated(root->child(200), 0);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(47, 4) && table->item(47, 4)->text() == "717", 10000);
  QCOMPARE(root->childCount(), 240);

  QSignalSpy watch(&inspector, &DebugValueInspector::addWatchRequested);
  for (auto *button : inspector.findChildren<QPushButton *>())
    if (button->text() == "Add to Watches")
      button->click();
  QCOMPARE(watch.count(), 1);
  QVERIFY(evaluateDebugObject(run.client, run.frame.id,
                              watch.first().first().toString(), &value));
  QCOMPARE(value.indexedVariables, 240);

  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "untyped", &value));
  inspector.setValue(value, run.frame.id);
  inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(4);
  inspector.findChild<QComboBox *>("debugArrayElementType")
      ->setCurrentText("int");
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(3, 0) && table->item(3, 0)->text() == "9", 10000);

  // A new selection must not receive the previous buffer's late replies.
  inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(100);
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "fixed", &value));
  inspector.setValue(value, run.frame.id);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(5, 0) && table->item(5, 0)->text() == "-1", 10000);
  QCOMPARE(table->rowCount(), 6);
  QSignalSpy stopped(&run.client, &DapClient::stopped);
  run.client.stepOver(run.client.currentThreadId());
  QVERIFY(waitForDebugSignal(stopped));
  QTRY_VERIFY_WITH_TIMEOUT(
      inspector.findChild<QPushButton *>("debugArrayApply")->isEnabled(),
      10000);
  inspector.findChild<QComboBox *>("debugValueDisplayFormat")
      ->setCurrentIndex(2);
  QCOMPARE(table->item(0, 0)->text(), QString("0x41"));
  run.client.stop(true);
}

void TestDap::testNativeBufferReportsErrors() {
  const QString skip = nativeCollectionSkipReason();
  if (!skip.isEmpty())
    QSKIP(qPrintable(skip));
  NativeCollectionRun run;
  QVERIFY2(run.start(), qPrintable(run.error));
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "missing", &value));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  inspector.setValue(value, run.frame.id);
  inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(2);
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(table->item(1, 0) &&
                               table->item(1, 0)->text().contains("unreadable"),
                           10000);
  QVERIFY(inspector.findChild<QLabel *>("debugValueStatus")
              ->text()
              .contains("unreadable"));
  QCOMPARE(run.client.state(), DapClient::State::Stopped);
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "empty", &value));
  inspector.setValue(value, run.frame.id);
  QVERIFY(inspector.findChild<QLabel *>("debugValueStatus")
              ->text()
              .contains("Empty"));
  QCOMPARE(table->rowCount(), 0);
  run.client.stop(true);
}

void TestDap::testNativeVariablesRemainStructured() {
  const QString skip = nativeCollectionSkipReason();
  if (!skip.isEmpty())
    QSKIP(qPrintable(skip));
  NativeCollectionRun run;
  QVERIFY2(run.start(), qPrintable(run.error));
  DebugPanel panel;
  panel.setDapClient(&run.client);
  auto *variables = panel.findChild<QTreeWidget *>("debugVariablesTree");
  QVERIFY(variables);
  auto find = [variables](const QString &name) -> QTreeWidgetItem * {
    const auto items =
        variables->findItems(name, Qt::MatchExactly | Qt::MatchRecursive);
    return items.isEmpty() ? nullptr : items.first();
  };
  QTRY_VERIFY_WITH_TIMEOUT(find("fixed") && find("values") && find("node"),
                           10000);
  QVERIFY(find("fixed")->text(2).contains("int"));
  QVERIFY(find("fixed")->data(0, Qt::UserRole).toInt() > 0);
  QCOMPARE(find("values")->data(0, Qt::UserRole + 5).toInt(), 4);
  find("fixed")->setExpanded(true);
  QTRY_COMPARE_WITH_TIMEOUT(find("fixed")->childCount(), 6, 10000);
  QCOMPARE(find("fixed")->child(1)->data(0, Qt::UserRole + 1).toString(),
           QString("(fixed)[1]"));
  find("node")->setExpanded(true);
  QTRY_VERIFY_WITH_TIMEOUT(find("node")->childCount() > 0, 10000);
  auto *pointee = find("node")->child(0);
  QCOMPARE(pointee->data(0, Qt::UserRole + 1).toString(), QString("(*(node))"));
  pointee->setExpanded(true);
  QTRY_VERIFY_WITH_TIMEOUT(pointee->childCount() > 0, 10000);
  QCOMPARE(pointee->child(0)->data(0, Qt::UserRole + 1).toString(),
           QString("((*(node))).key"));

  variables->itemDoubleClicked(find("heap"), 1);
  const auto dialogs = panel.findChildren<QDialog *>();
  QVERIFY(!dialogs.isEmpty());
  auto *inspector = dialogs.last()->findChild<DebugValueInspector *>();
  QVERIFY(inspector);
  inspector->findChild<QSpinBox *>("debugArrayCount")->setValue(4);
  inspector->findChild<QPushButton *>("debugArrayApply")->click();
  auto *table = inspector->findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(3, 0) && table->item(3, 0)->text() == "9", 10000);
  dialogs.last()->close();

  auto &watches = WatchManager::instance();
  watches.clearAll();
  auto cleanup = qScopeGuard([&watches]() {
    watches.clearAll();
    watches.setDapClient(nullptr);
  });
  const int id = watches.addWatch("values");
  watches.evaluateWatch(id, run.frame.id);
  QTRY_COMPARE_WITH_TIMEOUT(watches.watch(id).indexedVariables, 4, 10000);
  QVERIFY(watches.watch(id).hasIndexedVariables);
  run.client.stop(true);
}

void TestDap::testNativeCCollections() {
  const QString skip = nativeCollectionSkipReason();
  if (!skip.isEmpty() || QStandardPaths::findExecutable("cc").isEmpty())
    QSKIP("C collections need cc and usable GDB DAP");
  NativeCollectionRun run;
  QVERIFY2(run.start(true), qPrintable(run.error));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  auto *tabs = inspector.findChild<QTabWidget *>("debugValueTabs");
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "fixed", &value));
  inspector.setValue(value, run.frame.id);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(2, 0) && table->item(2, 0)->text() == "30", 10000);
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "words", &value));
  inspector.setValue(value, run.frame.id);
  tabs->setCurrentIndex(1);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(1, 0) && table->item(1, 0)->text() == "\"beta\"", 10000);
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "heap", &value));
  inspector.setValue(value, run.frame.id);
  inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(4);
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(3, 0) && table->item(3, 0)->text() == "43", 10000);
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "buffer", &value));
  inspector.setValue(value, run.frame.id);
  inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(4);
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  inspector.findChild<QComboBox *>("debugValueDisplayFormat")
      ->setCurrentIndex(1);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(3, 0) && table->item(3, 0)->text() == "10", 10000);
  QCOMPARE(table->item(1, 0)->text(), QString("0"));
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "packet", &value));
  inspector.setValue(value, run.frame.id);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  auto *root = tree->topLevelItem(0);
  QTRY_VERIFY(childNamed(root, "sample"));
  auto *sample = childNamed(root, "sample");
  sample->setExpanded(true);
  QTRY_VERIFY(childNamed(sample, "bytes"));
  auto *bytes = childNamed(sample, "bytes");
  bytes->setExpanded(true);
  QTRY_VERIFY(bytes->childCount() == 4 && bytes->child(2)->text(1) == "66");
  QCOMPARE(bytes->child(1)->text(1), QString("0"));

  auto *row = childNamed(root, "row");
  QVERIFY(row);
  row->setExpanded(true);
  QTRY_VERIFY(row->childCount() > 0);
  auto *array = row->child(0);
  array->setExpanded(true);
  QTRY_VERIFY(array->childCount() == 3 && array->child(2)->text(1) == "3");

  run.client.stop(true);
}

#ifndef Q_OS_WIN
void TestDap::testNativeCppNestedCollections() {
  const QString skip = nativeCollectionSkipReason();
  if (!skip.isEmpty())
    QSKIP(qPrintable(skip));
  NativeCollectionRun run;
  QVERIFY2(run.start(), qPrintable(run.error));
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "nested", &value));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  inspector.setValue(value, run.frame.id);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  auto *root = tree->topLevelItem(0);
  QTRY_COMPARE(root->childCount(), 2);
  QCOMPARE(root->child(0)->text(1), QString("7"));
  auto *tuple = root->child(1);
  tuple->setExpanded(true);
  QTRY_COMPARE(tuple->childCount(), 3);

  auto *set = tuple->child(0);
  set->setExpanded(true);
  QTRY_COMPARE(set->childCount(), 2);
  QCOMPARE(set->child(1)->text(1), QString("23"));

  auto *array = tuple->child(1);
  array->setExpanded(true);
  QTRY_VERIFY(array->childCount() > 0);
  if (array->childCount() == 1) {
    array = array->child(0);
    array->setExpanded(true);
  }
  QTRY_COMPARE(array->childCount(), 2);
  QCOMPARE(array->child(1)->text(1), QString("37"));

  auto *record = tuple->child(2);
  record->setExpanded(true);
  QTRY_VERIFY(childNamed(record, "id"));
  QCOMPARE(childNamed(record, "id")->text(1), QString("41"));
  run.client.stop(true);
}

namespace {
struct StringSortRun {
  QTemporaryDir dir;
  DapClient client;
  DapStackFrame frame;
  QString source;
  QString error;
  int master = -1;
  int slave = -1;

  ~StringSortRun() {
    client.stop(true);
    if (slave >= 0)
      ::close(slave);
    if (master >= 0)
      ::close(master);
  }

  QByteArray output() const {
    QByteArray result;
    char buffer[4096];
    for (ssize_t n = ::read(master, buffer, sizeof(buffer)); n > 0;
         n = ::read(master, buffer, sizeof(buffer)))
      result.append(buffer, static_cast<int>(n));
    return result.replace("\r\n", "\n");
  }

  bool start(const QByteArray &line) {
    source = QFINDTESTDATA("../fixtures/debugger/string_sort.c");
    const QString binary = dir.filePath("string_sort");
    QProcess compiler;
    compiler.start("cc", {"-g", "-O0", "-std=c11", source, "-o", binary});
    if (!compiler.waitForFinished(30000) || compiler.exitCode() != 0) {
      error = QString::fromUtf8(compiler.readAllStandardError());
      return false;
    }
    master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
      error = "Could not create the input terminal";
      return false;
    }
    const QString tty = QString::fromLocal8Bit(ptsname(master));
    slave = ::open(tty.toLocal8Bit().constData(), O_RDWR | O_NOCTTY);
    client.setAdapterMetadata("cppdbg-gdb", "cppdbg");
    QSignalSpy ready(&client, &DapClient::initialized);
    QSignalSpy initialized(&client, &DapClient::adapterInitialized);
    QSignalSpy stopped(&client, &DapClient::stopped);
    if (!client.start(QStandardPaths::findExecutable("gdb"),
                      {"--interpreter=dap"}) ||
        !waitForDebugSignal(ready) || !waitForDebugSignal(initialized)) {
      error = "Could not initialize GDB";
      return false;
    }
    client.evaluate("set inferior-tty " + tty);
    DapSourceBreakpoint breakpoint;
    breakpoint.line = firstLineContaining(source, {line});
    client.setBreakpoints(source, {breakpoint});
    client.configurationDone();
    client.launch(QJsonObject{{"program", binary}, {"cwd", dir.path()}});
    const QByteArray input("4\nwkue\nqoi\nsbv\nfekls\n");
    if (::write(master, input.constData(), static_cast<size_t>(input.size())) !=
            input.size() ||
        !waitForDebugSignal(stopped) || !currentDebugFrame(client, &frame)) {
      error = "The input program did not reach the inspection breakpoint";
      return false;
    }
    output(); // Discard the terminal's input echo.
    return true;
  }
};
} // namespace
#endif

void TestDap::testStringSortPointerArray_data() {
  QTest::addColumn<QByteArray>("line");
  QTest::addColumn<bool>("watch");
  QTest::addColumn<bool>("sorted");
  QTest::newRow("main-before-sort")
      << QByteArray("int (*const comparators[])") << false << false;
  QTest::newRow("function-argument")
      << QByteArray("for (int i = 1; i < count; ++i)") << false << false;
  QTest::newRow("function-argument-watch")
      << QByteArray("for (int i = 1; i < count; ++i)") << true << false;
  QTest::newRow("main-after-sort")
      << QByteArray("for (int j = 0; j < count; ++j)") << false << true;
}

void TestDap::testStringSortPointerArray() {
#ifdef Q_OS_WIN
  QSKIP("The scanf regression uses a POSIX terminal");
#else
  if (!nativeCollectionSkipReason().isEmpty() ||
      QStandardPaths::findExecutable("cc").isEmpty())
    QSKIP("The scanf regression needs cc and GDB DAP");
  QFETCH(QByteArray, line);
  QFETCH(bool, watch);
  QFETCH(bool, sorted);
  StringSortRun run;
  QVERIFY2(run.start(line), qPrintable(run.error));
  DebugPanel panel;
  panel.applyTheme(Theme());
  panel.setDapClient(&run.client);
  auto *variables = panel.findChild<QTreeWidget *>("debugVariablesTree");
  auto variable = [variables](const QString &name) -> QTreeWidgetItem * {
    const auto items =
        variables->findItems(name, Qt::MatchExactly | Qt::MatchRecursive);
    return items.isEmpty() ? nullptr : items.first();
  };
  QTRY_VERIFY_WITH_TIMEOUT(variable("strings") && variable("count"), 10000);
  QVERIFY(variable("strings")->text(2).contains("**"));
  QTreeWidgetItem *target = variable("strings");
  QTreeWidget *tree = variables;
  auto &watches = WatchManager::instance();
  auto cleanup = qScopeGuard([&watches]() {
    watches.clearAll();
    watches.setDapClient(nullptr);
  });
  if (watch) {
    watches.clearAll();
    const int id = watches.addWatch("strings");
    watches.evaluateWatch(id, run.frame.id);
    tree = panel.findChild<QTreeWidget *>("debugWatchTree");
    QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItemCount() &&
                                 !tree->topLevelItem(0)->text(2).isEmpty(),
                             10000);
    target = tree->topLevelItem(0);
  }
  const QStringList words = sorted ? QStringList{"fekls", "qoi", "sbv", "wkue"}
                                   : QStringList{"wkue", "qoi", "sbv", "fekls"};
  QTRY_COMPARE_WITH_TIMEOUT(
      target->data(0, Qt::UserRole + (watch ? 7 : 5)).toInt(), 4, 10000);
  target->setExpanded(true);
  QTRY_COMPARE_WITH_TIMEOUT(target->childCount(), 4, 10000);
  for (int row = 0; row < 4; ++row)
    QVERIFY(target->child(row)->text(1).contains('"' + words.at(row) + '"'));
  tree->itemDoubleClicked(target, 1);
  const auto dialogs = panel.findChildren<QDialog *>();
  QVERIFY(!dialogs.isEmpty());
  auto *inspector = dialogs.last()->findChild<DebugValueInspector *>();
  QVERIFY(inspector);
  auto *table = inspector->findChild<QTableWidget *>("debugValueTable");
  auto *length =
      inspector->findChild<QLineEdit *>("debugArrayLengthExpression");
  QCOMPARE(length->text(), QString("count"));
  QTRY_VERIFY_WITH_TIMEOUT(table->item(3, 0) && table->item(3, 0)->text() ==
                                                    '"' + words.at(3) + '"',
                           10000);
  QCOMPARE(inspector->findChild<QSpinBox *>("debugArrayCount")->value(), 4);
  QCOMPARE(table->rowCount(), 4);
  QCOMPARE(table->columnCount(), 1);
  for (int row = 0; row < 4; ++row)
    QCOMPARE(table->item(row, 0)->text(), '"' + words.at(row) + '"');
  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_STRING_SORT_SCREENSHOT")) {
    QTest::qWait(50);
    dialogs.last()->grab().save(
        qEnvironmentVariable("LIGHTPAD_STRING_SORT_SCREENSHOT") + "-" +
        QTest::currentDataTag() + "-strings.png");
  }

  inspector->findChild<QSpinBox *>("debugArrayColumns")->setValue(2);
  inspector->findChild<QPushButton *>("debugArrayApply")->click();
  QTRY_VERIFY_WITH_TIMEOUT(table->columnCount() == 2 && table->item(3, 1) &&
                               table->item(3, 1)->text() ==
                                   '\'' + words.at(3).mid(1, 1) + '\'',
                           10000);
  QCOMPARE(table->rowCount(), 4);
  inspector->findChild<QSpinBox *>("debugArrayColumns")->setValue(1);
  inspector->findChild<QComboBox *>("debugArrayLayout")->setCurrentIndex(3);
  QVERIFY(!inspector->findChild<QSpinBox *>("debugArrayColumns")->isEnabled());
  inspector->findChild<QPushButton *>("debugArrayApply")->click();
  QTRY_VERIFY_WITH_TIMEOUT(table->columnCount() == 5 && table->item(0, 0) &&
                               table->item(0, 0)->text() ==
                                   '\'' + words.at(0).left(1) + '\'',
                           10000);
  QCOMPARE(table->rowCount(), 4);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 5; ++column) {
      QCOMPARE(table->item(row, column)->text(),
               column < words.at(row).size()
                   ? '\'' + words.at(row).mid(column, 1) + '\''
                   : QString());
    }
  }
  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_STRING_SORT_SCREENSHOT")) {
    dialogs.last()->show();
    QTest::qWait(50);
    dialogs.last()->grab().save(
        qEnvironmentVariable("LIGHTPAD_STRING_SORT_SCREENSHOT") + "-" +
        QTest::currentDataTag() + ".png");
  }
  inspector->findChild<QComboBox *>("debugValueDisplayFormat")
      ->setCurrentIndex(1);
  QCOMPARE(table->item(0, 0)->text(),
           QString::number(words.at(0).at(0).unicode()));
  dialogs.last()->close();

  QSignalSpy exited(&run.client, &DapClient::exited);
  run.client.setBreakpoints(run.source, {});
  run.client.continueExecution();
  QVERIFY(waitForDebugSignal(exited));
  QCOMPARE(exited.last().first().toInt(), 0);
  const QByteArray expected("fekls\nqoi\nsbv\nwkue\n\nwkue\nsbv\nqoi\nfekls\n\n"
                            "qoi\nsbv\nwkue\nfekls\n\nqoi\nsbv\nwkue\nfekls\n");
  QByteArray output;
  QTRY_VERIFY_WITH_TIMEOUT((output += run.output(), output.contains(expected)),
                           5000);
#endif
}

void TestDap::testStringSortConsoleFunctionCall() {
#ifdef Q_OS_WIN
  QSKIP("The function-call regression uses a POSIX input terminal");
#else
  if (!nativeCollectionSkipReason().isEmpty())
    QSKIP("Function evaluation needs GDB DAP");
  StringSortRun run;
  QVERIFY2(run.start("while (j > 0 && compare("), qPrintable(run.error));
  DebugPanel panel;
  panel.setDapClient(&run.client);
  auto *variables = panel.findChild<QTreeWidget *>("debugVariablesTree");
  QTRY_VERIFY_WITH_TIMEOUT(
      !variables->findItems("value", Qt::MatchExactly | Qt::MatchRecursive)
           .isEmpty(),
      10000);
  auto *input = panel.findChild<QLineEdit *>("debugConsoleInput");
  auto *output = panel.findChild<QPlainTextEdit *>("debugConsoleOutput");
  const QString expression = "compare(strings[j - 1], value)";
  DapVariable strings;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "strings", &strings));
  DebugValueInspector liveStrings;
  liveStrings.setDapClient(&run.client);
  liveStrings.setValue(strings, run.frame.id, "count");
  auto *liveTable = liveStrings.findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(liveTable->item(1, 0) &&
                               liveTable->item(1, 0)->text() == "\"qoi\"",
                           10000);
  QSignalSpy evaluated(&run.client, &DapClient::evaluateResponse);
  input->setText(expression);
  input->returnPressed();
  auto returnedNumber = [&]() {
    for (const auto &response : evaluated) {
      bool numeric = false;
      const int value = response.at(2).toString().toInt(&numeric);
      if (response.at(1).toString() == expression && numeric && value > 0)
        return true;
    }
    return false;
  };
  QTRY_VERIFY2_WITH_TIMEOUT(returnedNumber(), qPrintable(output->toPlainText()),
                            5000);
  QVERIFY(output->toPlainText().contains(expression + " (int)"));
  QCOMPARE(run.client.state(), DapClient::State::Stopped);
  QVERIFY(!output->toPlainText().split('\n').contains("Stopped"));
  auto *inspector =
      panel.findChild<DebugValueInspector *>("debugConsoleValueInspector");
  QVERIFY(
      !inspector->findChild<QCheckBox *>("debugValueLiveUpdates")->isChecked());
  // The same explicit call can be made again after its temporary stop.
  evaluated.clear();
  input->setText(expression);
  input->returnPressed();
  QTRY_VERIFY2_WITH_TIMEOUT(returnedNumber(), qPrintable(output->toPlainText()),
                            5000);
  QTRY_VERIFY_WITH_TIMEOUT(
      liveStrings.findChild<QPushButton *>("debugArrayApply")->isEnabled(),
      10000);
  QTRY_VERIFY_WITH_TIMEOUT(liveTable->item(1, 0) &&
                               liveTable->item(1, 0)->text() == "\"qoi\"",
                           10000);
  // A breakpoint inside the call is an actual interruption. Report its error
  // once; retrying via the REPL could execute the expression a second time.
  run.client.setFunctionBreakpoints({"lexicographic_sort"});
  QSignalSpy errors(&run.client, &DapClient::evaluateResponseError);
  input->setText(expression);
  input->returnPressed();
  auto callErrors = [&]() {
    return std::count_if(
        errors.cbegin(), errors.cend(), [&](const QList<QVariant> &response) {
          return response.at(1).toString().contains(expression);
        });
  };
  QTRY_COMPARE_WITH_TIMEOUT(callErrors(), 1, 10000);
  QTRY_VERIFY_WITH_TIMEOUT(output->toPlainText().contains(expression + ":"),
                           10000);
  QVERIFY(!run.client.hasPendingInteractiveEvaluation());
#endif
}

void TestDap::testStringSortInspectorFollowsSteps_data() {
  QTest::addColumn<bool>("characters");
  QTest::newRow("strings") << false;
  QTest::newRow("character-grid") << true;
}

void TestDap::testStringSortInspectorFollowsSteps() {
#ifdef Q_OS_WIN
  QSKIP("The sorting regression uses a POSIX input terminal");
#else
  if (!nativeCollectionSkipReason().isEmpty())
    QSKIP("Live native inspection needs GDB DAP");
  QFETCH(bool, characters);
  StringSortRun run;
  QVERIFY2(run.start("strings[j] = strings[j - 1]"), qPrintable(run.error));
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "strings", &value));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  inspector.setValue(value, run.frame.id, "count");
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  auto *layout = inspector.findChild<QComboBox *>("debugArrayLayout");
  auto *format = inspector.findChild<QComboBox *>("debugValueDisplayFormat");
  auto *apply = inspector.findChild<QPushButton *>("debugArrayApply");
  auto *filter = inspector.findChild<QLineEdit *>("debugValueFilter");
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(1, 0) && table->item(1, 0)->text() == "\"qoi\"", 10000);
  if (characters) {
    layout->setCurrentIndex(3);
    apply->click();
    QTRY_VERIFY_WITH_TIMEOUT(
        table->item(3, 4) && table->item(3, 4)->text() == "'s'", 10000);
    format->setCurrentIndex(1);
  }
  filter->setText("strings");
  auto cell = [&](const QString &word) {
    return characters ? QString::number(word.front().unicode())
                      : '"' + word + '"';
  };
  QSignalSpy stopped(&run.client, &DapClient::stopped);
  auto step = [&]() {
    stopped.clear();
    run.client.stepOver(run.client.currentThreadId());
    return waitForDebugSignal(stopped);
  };
  QVERIFY(step()); // Shift wkue into slot 1.
  QTRY_VERIFY_WITH_TIMEOUT(table->item(1, 0) &&
                               table->item(1, 0)->text() == cell("wkue") &&
                               apply->isEnabled(),
                           10000);
  QCOMPARE(layout->currentIndex(), characters ? 3 : 0);
  QCOMPARE(format->currentIndex(), characters ? 1 : 0);
  QCOMPARE(filter->text(), QString("strings"));
  QCOMPARE(
      inspector.findChild<QLineEdit *>("debugArrayLengthExpression")->text(),
      QString("count"));
  QCOMPARE(inspector.findChild<QSpinBox *>("debugArrayCount")->value(), 4);
  auto *tabs = inspector.findChild<QTabWidget *>("debugValueTabs");
  tabs->setCurrentIndex(0);
  QVERIFY(step()); // --j
  QVERIFY(step()); // while condition
  QVERIFY(step()); // Insert qoi at slot 0.
  QTRY_VERIFY_WITH_TIMEOUT(apply->isEnabled(), 10000);
  QCOMPARE(tabs->currentIndex(), 0);
  tabs->setCurrentIndex(1);
  QTRY_VERIFY_WITH_TIMEOUT(table->item(0, 0) &&
                               table->item(0, 0)->text() == cell("qoi") &&
                               apply->isEnabled(),
                           10000);
  QCOMPARE(table->item(1, 0)->text(), cell("wkue"));
  QCOMPARE(table->rowCount(), 4);
  QCOMPARE(table->columnCount(), characters ? 5 : 1);
  QVERIFY(!inspector.findChild<QLabel *>("debugValueStatus")
               ->text()
               .contains("Snapshot"));
  // Live can be disabled for a snapshot, then Refresh reads the latest pause.
  inspector.findChild<QCheckBox *>("debugValueLiveUpdates")->setChecked(false);
  QVERIFY(step());
  QTRY_VERIFY_WITH_TIMEOUT(inspector.findChild<QLabel *>("debugValueStatus")
                               ->text()
                               .contains("Snapshot"),
                           10000);
  tabs->setCurrentIndex(2);
  inspector.findChild<QPushButton *>("debugValueRefresh")->click();
  QTRY_VERIFY_WITH_TIMEOUT(apply->isEnabled(), 10000);
  QCOMPARE(tabs->currentIndex(), 2);
  tabs->setCurrentIndex(1);
  QCOMPARE(table->item(0, 0)->text(), cell("qoi"));
  QCOMPARE(format->currentIndex(), characters ? 1 : 0);
  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_LIVE_INSPECTOR_SCREENSHOT")) {
    filter->clear();
    inspector.findChild<QCheckBox *>("debugValueLiveUpdates")->setChecked(true);
    inspector.resize(1000, 620);
    inspector.applyTheme(Theme());
    inspector.show();
    QTest::qWait(50);
    inspector.grab().save(
        qEnvironmentVariable("LIGHTPAD_LIVE_INSPECTOR_SCREENSHOT") + "-" +
        QTest::currentDataTag() + ".png");
  }
#endif
}

void TestDap::testStringSortInspectorFollowsCallerFrame() {
#ifdef Q_OS_WIN
  QSKIP("The sorting regression uses a POSIX input terminal");
#else
  if (!nativeCollectionSkipReason().isEmpty())
    QSKIP("Live native inspection needs GDB DAP");
  StringSortRun run;
  QVERIFY2(run.start("while (j > 0 && compare("), qPrintable(run.error));
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "strings", &value));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  inspector.setValue(value, run.frame.id, "count");
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  auto *apply = inspector.findChild<QPushButton *>("debugArrayApply");
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(3, 0) && table->item(3, 0)->text() == "\"fekls\"", 10000);
  QSignalSpy stopped(&run.client, &DapClient::stopped);
  QSignalSpy refreshed(&run.client, &DapClient::evaluateResponse);
  run.client.stepInto(run.client.currentThreadId());
  QVERIFY(waitForDebugSignal(stopped));
  QTRY_VERIFY_WITH_TIMEOUT(apply->isEnabled(), 10000);
  QVERIFY(run.client.cachedStackFrames(run.client.currentThreadId())
              .first()
              .name.startsWith("lexicographic_sort"));
  QVERIFY(std::any_of(refreshed.cbegin(), refreshed.cend(),
                      [](const QList<QVariant> &response) {
                        return response.at(1).toString() == "strings";
                      }));
  QCOMPARE(table->item(1, 0)->text(), QString("\"qoi\""));
  stopped.clear();
  run.client.stepOut(run.client.currentThreadId());
  QVERIFY(waitForDebugSignal(stopped));
  QTRY_VERIFY_WITH_TIMEOUT(apply->isEnabled(), 10000);
  // A returned function's locals must never be read from the caller instead.
  run.client.setBreakpoints(
      run.source, {DapSourceBreakpoint{
                      firstLineContaining(run.source, {"puts(strings[j]);"})}});
  stopped.clear();
  run.client.continueExecution();
  QVERIFY(waitForDebugSignal(stopped));
  QTRY_VERIFY_WITH_TIMEOUT(inspector.findChild<QLabel *>("debugValueStatus")
                               ->text()
                               .contains("out of scope"),
                           10000);
  QVERIFY(!apply->isEnabled());
#endif
}

void TestDap::testNativeInspectorLiveCollections() {
  if (!nativeCollectionSkipReason().isEmpty())
    QSKIP("Live native inspection needs GDB DAP");
  NativeCollectionRun run;
  QVERIFY2(run.start(), qPrintable(run.error));
  DebugValueInspector heap, vector, text;
  for (auto *inspector : {&heap, &vector, &text})
    inspector->setDapClient(&run.client);
  DapVariable value;
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "heap", &value));
  heap.setValue(value, run.frame.id);
  heap.findChild<QSpinBox *>("debugArrayCount")->setValue(240);
  heap.findChild<QSpinBox *>("debugArrayColumns")->setValue(5);
  heap.findChild<QComboBox *>("debugArrayElementType")->setCurrentText("int");
  heap.findChild<QPushButton *>("debugArrayApply")->click();
  auto *heapTree = heap.findChild<QTreeWidget *>("debugValueTree");
  auto *heapTable = heap.findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(
      heapTable->item(19, 4) && heapTable->item(19, 4)->text() == "297", 10000);
  heapTree->itemActivated(heapTree->topLevelItem(0)->child(100), 0);
  QTRY_COMPARE_WITH_TIMEOUT(heapTree->topLevelItem(0)->childCount(), 201,
                            10000);
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "values", &value));
  vector.setValue(value, run.frame.id);
  auto *vectorTable = vector.findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(vectorTable->item(0, 0) &&
                               vectorTable->item(0, 0)->text() == "12",
                           10000);
  QVERIFY(evaluateDebugObject(run.client, run.frame.id, "text", &value));
  text.setValue(value, run.frame.id);
  text.findChild<QComboBox *>("debugValueDisplayFormat")->setCurrentIndex(2);
  auto *textTable = text.findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(
      textTable->item(0, 0) && textTable->item(0, 0)->text() == "0x41", 10000);
  const int line = firstLineContaining(run.source, {"std::free(heap);"});
  run.client.setBreakpoints(run.source, {DapSourceBreakpoint{line}});
  QSignalSpy stopped(&run.client, &DapClient::stopped);
  run.client.continueExecution();
  QVERIFY(waitForDebugSignal(stopped));
  QTRY_VERIFY_WITH_TIMEOUT(
      heapTable->item(0, 1) && heapTable->item(0, 1)->text() == "999", 10000);
  QTRY_VERIFY_WITH_TIMEOUT(vectorTable->item(0, 0) &&
                               vectorTable->item(0, 0)->text() == "77",
                           10000);
  QTRY_VERIFY_WITH_TIMEOUT(
      textTable->item(0, 0) && textTable->item(0, 0)->text() == "0x5a", 10000);
  QTRY_VERIFY_WITH_TIMEOUT(heapTree->topLevelItem(0)->childCount() == 201 &&
                               heapTree->topLevelItem(0)->child(199)->text(1) ==
                                   "597",
                           10000);
  QCOMPARE(heap.findChild<QSpinBox *>("debugArrayColumns")->value(), 5);
  QCOMPARE(heap.findChild<QComboBox *>("debugArrayElementType")->currentText(),
           QString("int"));
  QCOMPARE(
      text.findChild<QComboBox *>("debugValueDisplayFormat")->currentIndex(),
      2);
}

void TestDap::testPointerRowsMatrix() {
  if (!nativeCollectionSkipReason().isEmpty())
    QSKIP("Pointer matrices need GDB DAP");
  NativeCollectionRun run;
  QVERIFY2(run.start(true), qPrintable(run.error));
  DapVariable value;
  QVERIFY(
      evaluateDebugObject(run.client, run.frame.id, "matrixPointers", &value));
  DebugValueInspector inspector;
  inspector.setDapClient(&run.client);
  inspector.setValue(value, run.frame.id);
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  inspector.findChild<QSpinBox *>("debugArrayCount")->setValue(2);
  inspector.findChild<QSpinBox *>("debugArrayColumns")->setValue(3);
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(1, 2) && table->item(1, 2)->text() == "30", 10000);
  QCOMPARE(table->rowCount(), 2);
  QCOMPARE(table->columnCount(), 3);
  QCOMPARE(table->item(0, 2)->text(), QString("3"));
  QCOMPARE(table->item(1, 0)->text(), QString("10"));

  auto *length = inspector.findChild<QLineEdit *>("debugArrayLengthExpression");
  length->setText("-1");
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  QTRY_VERIFY_WITH_TIMEOUT(inspector.findChild<QLabel *>("debugValueStatus")
                               ->text()
                               .contains("must be an integer"),
                           5000);
  QCOMPARE(table->rowCount(), 2);
  length->setText("0");
  inspector.findChild<QPushButton *>("debugArrayApply")->click();
  QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 0, 5000);
  QCOMPARE(inspector.findChild<QSpinBox *>("debugArrayCount")->value(), 0);

  value = {};
  value.name = value.evaluateName = "strings";
  value.type = "char **";
  value.value = "\"wkue\"";
  inspector.setValue(value, run.frame.id);
  QCOMPARE(inspector.findChild<QTreeWidget *>("debugValueTree")
               ->topLevelItem(0)
               ->childCount(),
           0);
  QCOMPARE(table->rowCount(), 0);
  QVERIFY(inspector.findChild<QLabel *>("debugValueStatus")
              ->text()
              .contains("outer"));
  run.client.stop(true);
}

void TestDap::testVtkPythonObjectsAndArrays() {
  const QString python = qEnvironmentVariable(
      "LIGHTPAD_VTK_PYTHON", QStandardPaths::findExecutable("python3"));
  QProcess probe;
  probe.start(python, {"-c", "import debugpy, vtkmodules, numpy"});
  if (!probe.waitForFinished(10000) || probe.exitCode() != 0) {
    QSKIP("Set LIGHTPAD_VTK_PYTHON to a Python interpreter with VTK, NumPy and "
          "debugpy");
  }
  const QString script = QFINDTESTDATA("../fixtures/debugger/vtk_scene.py");
  QVERIFY(!script.isEmpty());
  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");
  QSignalSpy ready(&client, &DapClient::initialized);
  QSignalSpy adapterReady(&client, &DapClient::adapterInitialized);
  QSignalSpy stopped(&client, &DapClient::stopped);
  QSignalSpy exited(&client, &DapClient::exited);
  QVERIFY(client.start(python, {"-m", "debugpy.adapter"}));
  QVERIFY(waitForDebugSignal(ready));
  client.launch(QJsonObject{{"program", script},
                            {"cwd", QFileInfo(script).absolutePath()},
                            {"console", "internalConsole"}});
  QVERIFY(waitForDebugSignal(adapterReady));
  DapSourceBreakpoint breakpoint;
  breakpoint.line = graphicsInspectionLine(script);
  QVERIFY(breakpoint.line > 0);
  client.setBreakpoints(script, {breakpoint});
  client.configurationDone();
  QVERIFY(waitForDebugSignal(stopped));
  DapStackFrame frame;
  QVERIFY(currentDebugFrame(client, &frame));
  QCOMPARE(frame.line, breakpoint.line);

  DapVariable value;
  QVERIFY(evaluateDebugObject(client, frame.id, "mesh", &value));
  QVERIFY(value.variablesReference > 0);
  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  inspector.setValue(value);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItem(0)->childCount() > 0, 10000);
  QVERIFY(evaluateDebugObject(client, frame.id, "mesh.GetNumberOfPoints()",
                              &value));
  QCOMPARE(value.value, QString("226"));
  QVERIFY(evaluateDebugObject(
      client, frame.id, "actor.GetMapper().GetInput().GetNumberOfPoints()",
      &value));
  QCOMPARE(value.value, QString("226"));
  QVERIFY(evaluateDebugObject(client, frame.id, "point_rows", &value));
  inspector.setValue(value);
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(7, 2) && !table->item(7, 2)->text().isEmpty(), 10000);
  QCOMPARE(table->columnCount(), 3);

  QVERIFY(evaluateDebugObject(client, frame.id, "velocity", &value));
  inspector.setValue(value);
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(1, 2) && table->item(1, 2)->text() == "1.75", 10000);
  QCOMPARE(table->columnCount(), 3);
  QVERIFY(evaluateDebugObject(client, frame.id, "coordinates", &value));
  inspector.setValue(value);
  auto *root = tree->topLevelItem(0);
  QTRY_COMPARE_WITH_TIMEOUT(root->childCount(), 101, 10000);
  tree->itemActivated(root->child(100), 0);
  QTRY_COMPARE_WITH_TIMEOUT(root->childCount(), 201, 10000);
  tree->itemActivated(root->child(200), 0);
  QTRY_COMPARE_WITH_TIMEOUT(root->childCount(), 226, 10000);
  QCOMPARE(root->child(225)->text(0).toInt(), 225);
  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_VTK_SCREENSHOT")) {
    inspector.applyTheme(Theme());
    inspector.resize(1000, 600);
    inspector.show();
    QTest::qWait(100);
    inspector.grab().save(qEnvironmentVariable("LIGHTPAD_VTK_SCREENSHOT"));
  }
  client.stepOver(client.currentThreadId());
  QVERIFY(waitForDebugSignal(stopped, 2));
  QVERIFY(inspector.findChild<QLabel *>("debugValueStatus")
              ->text()
              .contains("Snapshot"));
  QVERIFY(currentDebugFrame(client, &frame));
  QVERIFY(evaluateDebugObject(client, frame.id, "mesh.GetPoint(0)[0]", &value));
  QCOMPARE(value.value, QString("99.0"));
  client.continueExecution(client.currentThreadId());
  QVERIFY(waitForDebugSignal(exited));
  QCOMPARE(exited.last().first().toInt(), 0);
  client.stop(true);
  QTest::qWait(100);
}

void TestDap::testOpenGLCppObjectsAndMatrices() {
  const QString compiler = QStandardPaths::findExecutable("c++");
  const QString gdb = QStandardPaths::findExecutable("gdb");
  if (compiler.isEmpty() || gdb.isEmpty() ||
      qEnvironmentVariableIsEmpty("DISPLAY")) {
    QSKIP("OpenGL debugging needs c++, GDB DAP, and DISPLAY (or xvfb-run)");
  }
  QProcess flags;
  flags.start("pkg-config", {"--cflags", "--libs", "Qt6Gui", "Qt6OpenGL"});
  if (!flags.waitForFinished(10000) || flags.exitCode() != 0)
    QSKIP("Qt6 OpenGL development libraries are not installed");
  const QString source = QFINDTESTDATA("../fixtures/debugger/opengl_scene.cpp");
  QVERIFY(!source.isEmpty());
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString binary = dir.filePath("opengl_scene");
  QStringList args = {"-g", "-O0", "-std=c++17", "-fPIC", source, "-o", binary};
  args +=
      QProcess::splitCommand(QString::fromUtf8(flags.readAllStandardOutput()));
  QProcess build;
  build.start(compiler, args);
  QVERIFY(build.waitForFinished(60000));
  QVERIFY2(build.exitCode() == 0, build.readAllStandardError().constData());
  DapClient client;
  client.setAdapterMetadata("cppdbg-gdb", "cppdbg");
  QSignalSpy ready(&client, &DapClient::initialized);
  QSignalSpy adapterReady(&client, &DapClient::adapterInitialized);
  QSignalSpy stopped(&client, &DapClient::stopped);
  QSignalSpy exited(&client, &DapClient::exited);
  QVERIFY(client.start(gdb, {"--interpreter=dap"}));
  QVERIFY(waitForDebugSignal(ready));
  QVERIFY(waitForDebugSignal(adapterReady));
  DapSourceBreakpoint breakpoint;
  breakpoint.line = graphicsInspectionLine(source);
  QVERIFY(breakpoint.line > 0);
  client.setBreakpoints(source, {breakpoint});
  client.configurationDone();
  DebugConfiguration configuration;
  configuration.program = binary;
  configuration.cwd = dir.path();
  configuration.env["QT_QPA_PLATFORM"] = "xcb";
  const auto adapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(adapter);
  client.launch(adapter->launchArguments(configuration));
  QVERIFY(waitForDebugSignal(stopped));
  DapStackFrame frame;
  QVERIFY(currentDebugFrame(client, &frame));
  QCOMPARE(frame.line, breakpoint.line);
  DapVariable value;
  QVERIFY(evaluateDebugObject(client, frame.id, "graphicsError", &value));
  QCOMPARE(value.value, QString("0"));
  QVERIFY(evaluateDebugObject(client, frame.id, "scene.vertexBuffer", &value));
  QVERIFY(value.value.toUInt() > 0);
  QVERIFY(evaluateDebugObject(client, frame.id, "scene.texture", &value));
  QVERIFY(value.value.toUInt() > 0);
  QVERIFY(evaluateDebugObject(client, frame.id, "scene.model", &value));
  QVERIFY(value.variablesReference > 0);
  DebugValueInspector inspector;
  inspector.setDapClient(&client);
  inspector.setValue(value);
  auto *table = inspector.findChild<QTableWidget *>("debugValueTable");
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(3, 3) && table->item(3, 3)->text() == "1", 10000);
  QCOMPARE(table->rowCount(), 4);
  QCOMPARE(table->columnCount(), 4);
  QCOMPARE(table->item(0, 1)->text(), QString("0"));
  QVERIFY(evaluateDebugObject(client, frame.id, "scene.vertices", &value));
  inspector.setValue(value);
  auto *tree = inspector.findChild<QTreeWidget *>("debugValueTree");
  QTRY_COMPARE_WITH_TIMEOUT(tree->topLevelItem(0)->childCount(), 3, 10000);
  auto *vertex = tree->topLevelItem(0)->child(0);
  vertex->setExpanded(true);
  QTRY_COMPARE_WITH_TIMEOUT(vertex->childCount(), 2, 10000);
  QCOMPARE(vertex->child(0)->text(0), QString("position"));
  vertex->child(0)->setExpanded(true);
  QTRY_COMPARE_WITH_TIMEOUT(vertex->child(0)->childCount(), 3, 10000);
  QCOMPARE(vertex->child(0)->child(0)->text(1), QString("-0.5"));
  client.stepOver(client.currentThreadId());
  QVERIFY(waitForDebugSignal(stopped, 2));
  QVERIFY(currentDebugFrame(client, &frame));
  QVERIFY(evaluateDebugObject(client, frame.id, "scene.model[0][0]", &value));
  QCOMPARE(value.value, QString("2"));
  client.continueExecution(client.currentThreadId());
  QVERIFY(waitForDebugSignal(exited));
  QCOMPARE(exited.last().first().toInt(), 0);
  client.stop(true);
}

void TestDap::testGdbLaunchEnvironmentPreservesGraphicsContext() {
  const auto adapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(adapter);
  DebugConfiguration configuration;
  configuration.program = "/tmp/graphics-test";
  configuration.env["QT_QPA_PLATFORM"] = "xcb";
  configuration.adapterConfig["environment"] = QJsonArray{QJsonObject{
      {"name", "LIGHTPAD_GRAPHICS_DRIVER"}, {"value", "test-driver"}}};
  const QJsonObject arguments = adapter->launchArguments(configuration);
  const QJsonObject environment = arguments["env"].toObject();
  QCOMPARE(environment["PATH"].toString(), qEnvironmentVariable("PATH"));
  if (qEnvironmentVariableIsSet("DISPLAY"))
    QCOMPARE(environment["DISPLAY"].toString(),
             qEnvironmentVariable("DISPLAY"));
  QCOMPARE(environment["QT_QPA_PLATFORM"].toString(), QString("xcb"));
  QCOMPARE(environment["LIGHTPAD_GRAPHICS_DRIVER"].toString(),
           QString("test-driver"));
  QVERIFY(!arguments.contains("environment"));
}

#ifndef Q_OS_WIN
namespace {
// Builds a tiny interactive C program and starts it under the real GDB adapter.
// `terminal` receives the pty master (or -1) once the session has asked for a
// debuggee terminal; `provide` decides which tty path the session is given.
struct InteractiveGdbRun {
  QTemporaryDir dir;
  QString binary;
  QString skipReason;
};

bool buildInteractiveProgram(InteractiveGdbRun &run) {
  const QString compiler = QStandardPaths::findExecutable("cc");
  const QString gdb = QStandardPaths::findExecutable("gdb");
  if (compiler.isEmpty() || gdb.isEmpty()) {
    run.skipReason = "needs a C compiler and GDB with DAP support";
    return false;
  }
  const auto adapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  if (!adapter || !adapter->isAvailable()) {
    run.skipReason = "GDB DAP is not usable on this system";
    return false;
  }
  if (!run.dir.isValid()) {
    run.skipReason = "temporary directory unavailable";
    return false;
  }
  const QString source = run.dir.filePath("interactive.c");
  QFile file(source);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  file.write("#include <stdio.h>\n"
             "int main(void) {\n"
             "  int n = 0;\n"
             "  printf(\"enter: \");\n"
             "  if (scanf(\"%d\", &n) != 1) { puts(\"no-input\"); return 1; }\n"
             "  printf(\"got %d\\n\", n * 2);\n"
             "  return 0;\n"
             "}\n");
  file.close();
  run.binary = run.dir.filePath("interactive");
  QProcess build;
  build.start(compiler, {"-g", "-O0", source, "-o", run.binary});
  if (!build.waitForFinished(60000) || build.exitCode() != 0)
    return false;
  return true;
}

DebugConfiguration interactiveConfiguration(const InteractiveGdbRun &run) {
  DebugConfiguration configuration;
  configuration.name = "interactive";
  configuration.type = "cppdbg";
  configuration.adapterId = "cppdbg-gdb";
  configuration.request = "launch";
  configuration.program = run.binary;
  configuration.cwd = run.dir.path();
  return configuration;
}
} // namespace

void TestDap::testGdbDebuggeeReadsStdinFromProvidedTerminal() {
  InteractiveGdbRun run;
  if (!buildInteractiveProgram(run)) {
    if (!run.skipReason.isEmpty())
      QSKIP(qPrintable(run.skipReason));
    QFAIL("Failed to build the interactive fixture");
  }

  const int master = posix_openpt(O_RDWR | O_NOCTTY);
  QVERIFY(master >= 0);
  auto closeMaster = qScopeGuard([master]() { ::close(master); });
  QVERIFY(grantpt(master) == 0 && unlockpt(master) == 0);
  const QString slave = QString::fromLocal8Bit(ptsname(master));
  QVERIFY(!slave.isEmpty());

  DebugSession session("stdin-terminal-test");
  QSignalSpy requested(&session, &DebugSession::inferiorTerminalRequested);
  QSignalSpy terminated(&session, &DebugSession::terminated);
  QSignalSpy stopped(&session, &DebugSession::stopped);
  QString consoleOutput;
  connect(&session, &DebugSession::outputReceived, &session,
          [&consoleOutput](const DapOutputEvent &event) {
            consoleOutput += event.output;
          });
  connect(&session, &DebugSession::inferiorTerminalRequested, &session,
          [&]() { session.provideInferiorTerminal(slave); });

  const auto adapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  BreakpointManager::instance().toggleBreakpoint(
      run.dir.filePath("interactive.c"), 6);
  auto clearBreakpoints = qScopeGuard([this]() { cleanupBreakpoints(); });
  QVERIFY(session.start(interactiveConfiguration(run), adapter));

  // Type the answer the way a user would; the line discipline holds it until
  // the debuggee reads it.
  QVERIFY(::write(master, "21\n", 3) == 3);

  QByteArray terminalOutput;
  auto drainTerminal = [&terminalOutput, master]() {
    char buffer[512];
    pollfd fd{master, POLLIN, 0};
    while (::poll(&fd, 1, 0) > 0) {
      const ssize_t n = ::read(master, buffer, sizeof(buffer));
      if (n <= 0)
        break;
      terminalOutput.append(buffer, int(n));
    }
    return terminalOutput.contains("enter: ");
  };
  QTRY_VERIFY_WITH_TIMEOUT(drainTerminal(), 20000);
  QCOMPARE(requested.count(), 1);
  QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 10000);
  QCOMPARE(session.state(), DebugSession::State::Stopped);
  QCOMPARE(qvariant_cast<DapStoppedEvent>(stopped.first().first()).reason,
           DapStoppedReason::Breakpoint);
  session.continueExecution();
  QTRY_VERIFY_WITH_TIMEOUT((drainTerminal(), terminalOutput.contains("got 42")),
                           10000);
  QVERIFY(!terminalOutput.contains("no-input"));
  QVERIFY2(consoleOutput.contains("Debuggee I/O"), qPrintable(consoleOutput));
  QTRY_COMPARE_WITH_TIMEOUT(terminated.count(), 1, 10000);
}

void TestDap::testGdbDebuggeeLaunchesWithoutTerminal() {
  InteractiveGdbRun run;
  if (!buildInteractiveProgram(run)) {
    if (!run.skipReason.isEmpty())
      QSKIP(qPrintable(run.skipReason));
    QFAIL("Failed to build the interactive fixture");
  }

  DebugSession session("no-terminal-test");
  QSignalSpy terminated(&session, &DebugSession::terminated);
  QString output;
  connect(&session, &DebugSession::outputReceived, &session,
          [&output](const DapOutputEvent &event) { output += event.output; });
  // A UI that cannot supply a terminal must not block the launch.
  connect(&session, &DebugSession::inferiorTerminalRequested, &session,
          [&session]() { session.provideInferiorTerminal({}); });

  const auto adapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(session.start(interactiveConfiguration(run), adapter));
  QTRY_COMPARE_WITH_TIMEOUT(terminated.count(), 1, 20000);
  QVERIFY2(output.contains("no-input"), qPrintable(output));
}
#else
void TestDap::testGdbDebuggeeReadsStdinFromProvidedTerminal() {
  QSKIP("GDB debuggee terminals use a POSIX pty");
}

void TestDap::testGdbDebuggeeLaunchesWithoutTerminal() {
  QSKIP("GDB debuggee terminals use a POSIX pty");
}
#endif

void TestDap::testProgramIoButtonFollowsSelectedClient() {
  DebugPanel panel;
  auto *button = panel.findChild<QToolButton *>("debugProgramIoButton");
  QVERIFY(button);
  QVERIFY(!button->isEnabled());
  QSignalSpy requested(&panel, &DebugPanel::showProgramIoRequested);
  button->click();
  QCOMPARE(requested.count(), 0);

  DapClient first;
  DapClient second;
  panel.setDapClient(&first);
  panel.setProgramIoAvailable(true);
  QVERIFY(button->isEnabled());
  panel.resize(340, 400);
  panel.show();
  QVERIFY(QTest::qWaitForWindowExposed(&panel));
  QVERIFY(!button->visibleRegion().isEmpty());
  button->click();
  QCOMPARE(requested.count(), 1);

  panel.setDapClient(&second);
  QVERIFY(!button->isEnabled());
  panel.setProgramIoAvailable(true);
  panel.setDapClient(nullptr);
  QVERIFY(!button->isEnabled());
}

void TestDap::testDebugToolbarCollapsesToIconsWhenNarrow() {
  DebugPanel panel;
  panel.resize(1200, 400);
  panel.show();
  QVERIFY(QTest::qWaitForWindowExposed(&panel));
  const auto buttons = panel.findChildren<QToolButton *>("debugToolbarButton");
  QVERIFY(buttons.size() >= 7);
  for (const QToolButton *button : buttons)
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);

  panel.resize(340, 400);
  QTRY_COMPARE(buttons.first()->toolButtonStyle(), Qt::ToolButtonIconOnly);
  for (const QToolButton *button : buttons) {
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonIconOnly);
    QVERIFY(!button->icon().isNull());
    QVERIFY(!button->toolTip().isEmpty());
  }

  panel.resize(1200, 400);
  QTRY_COMPARE(buttons.first()->toolButtonStyle(),
               Qt::ToolButtonTextBesideIcon);
}

void TestDap::testConsoleResultOpensStructuredInspector() {
  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");
  QVERIFY(client.start(fakeAdapterPath(), {"--stop-on-launch"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/app"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Stopped, 5000);
  DebugPanel panel;
  panel.setDapClient(&client);
  panel.setCurrentFrame(100);
  auto *input = panel.findChild<QLineEdit *>("debugConsoleInput");
  auto *table =
      panel.findChild<DebugValueInspector *>("debugConsoleValueInspector")
          ->findChild<QTableWidget *>("debugValueTable");
  QVERIFY(input && table);
  input->setText("matrix");
  input->returnPressed();
  QTRY_VERIFY_WITH_TIMEOUT(
      table->item(2, 2) && table->item(2, 2)->text() == "22.25", 5000);
  const auto *console = panel.findChild<QPlainTextEdit *>("debugConsoleOutput");
  QVERIFY(console->toPlainText().contains("[\n"));
  QCOMPARE(table->item(1, 2)->text(), QString());
  client.continueExecution(1);
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);
  QVERIFY(panel.findChild<DebugValueInspector *>("debugConsoleValueInspector")
              ->findChild<QLabel *>("debugValueStatus")
              ->text()
              .contains("Snapshot"));
  client.stop(false);
}

namespace {
QString concurrencyPython() {
  static const QString python = []() {
    const QString candidate = QStandardPaths::findExecutable("python3");
    QProcess probe;
    probe.start(candidate, {"-c", "import debugpy"});
    return probe.waitForFinished(10000) && probe.exitCode() == 0 ? candidate
                                                                 : QString();
  }();
  return python;
}

int concurrencyMarkerLine(const QString &path) {
  return firstLineContaining(path,
                             {"inspection_marker =", "inspectionMarker ="});
}

bool workerFrame(DapClient &client, int thread, const QString &source,
                 DapStackFrame *frame) {
  if (!client.selectThread(thread))
    return false;
  QSignalSpy stack(&client, &DapClient::stackTraceReceived);
  client.getStackTrace(thread);
  if (!waitForDebugSignal(stack))
    return false;
  for (const auto &entry : stack.last().at(1).value<QList<DapStackFrame>>()) {
    if (QFileInfo(entry.source.path).canonicalFilePath() ==
            QFileInfo(source).canonicalFilePath() &&
        entry.name.contains("worker")) {
      *frame = entry;
      return true;
    }
  }
  return false;
}
} // namespace

void TestDap::testPartialThreadExecution() {
  DapClient client;
  auto event = [&client](const QString &name, const QJsonObject &body) {
    client.feedAdapterData(frameMessage(
        QJsonObject{{"type", "event"}, {"event", name}, {"body", body}}));
  };
  for (int id : {1, 2})
    event("thread", {{"threadId", id}, {"reason", "started"}});
  event(
      "stopped",
      {{"threadId", 1}, {"reason", "breakpoint"}, {"allThreadsStopped", true}});
  QVERIFY(client.selectThread(2));
  event("continued", {{"threadId", 2}, {"allThreadsContinued", false}});
  QCOMPARE(client.state(), DapClient::State::Stopped);
  QVERIFY(client.isThreadStopped(1));
  QVERIFY(!client.isThreadStopped(2));
  QCOMPARE(client.currentThreadId(), 1);
  QVERIFY(!client.selectThread(2));
  event("stopped",
        {{"threadId", 2}, {"reason", "step"}, {"allThreadsStopped", false}});
  event("continued", {{"threadId", 1}, {"allThreadsContinued", false}});
  QCOMPARE(client.currentThreadId(), 2);
  QVERIFY(client.isThreadStopped(2));
  event("thread", {{"threadId", 2}, {"reason", "exited"}});
  QCOMPARE(client.state(), DapClient::State::Running);
  QCOMPARE(client.currentThreadId(), 0);
}

void TestDap::testSelectedThreadExecutionArguments() {
  for (bool supported : {false, true}) {
    QTemporaryDir dir;
    const QString trace = dir.filePath("requests.jsonl");
    DapClient client;
    QVERIFY(
        client.start(fakeAdapterPath(),
                     {"--trace", trace, "--caps",
                      QString("{\"supportsSingleThreadExecutionRequests\":%1}")
                          .arg(supported ? "true" : "false")}));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
    client.feedAdapterData(frameMessage(QJsonObject{
        {"type", "event"},
        {"event", "stopped"},
        {"body", QJsonObject{{"threadId", 1}, {"allThreadsStopped", true}}}}));
    QVERIFY(client.selectThread(2));
    client.continueExecution(0, true);
    QTRY_VERIFY_WITH_TIMEOUT(traceFileContains(trace, "\"continue\""), 5000);
    QFile file(trace);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonObject arguments;
    for (const auto &line : file.readAll().split('\n')) {
      auto request = QJsonDocument::fromJson(line).object();
      if (request["command"] == "continue")
        arguments = request["arguments"].toObject();
    }
    QCOMPARE(arguments["threadId"].toInt(), 2);
    QCOMPARE(arguments["singleThread"].toBool(), supported);
    client.stop(false);
  }
}

void TestDap::testPythonThreadSelection() {
  const QString python = concurrencyPython();
  if (python.isEmpty())
    QSKIP("Python concurrency integration tests require python3 with debugpy");
  const QString source =
      QFINDTESTDATA("../fixtures/debugger/concurrency_threads.py");
  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");
  QSignalSpy ready(&client, &DapClient::initialized);
  QSignalSpy configured(&client, &DapClient::adapterInitialized);
  QSignalSpy stopped(&client, &DapClient::stopped);
  QSignalSpy exited(&client, &DapClient::exited);
  QVERIFY(client.start(python, {"-m", "debugpy.adapter"}));
  QVERIFY(waitForDebugSignal(ready));
  client.launch({{"program", source}, {"console", "internalConsole"}});
  QVERIFY(waitForDebugSignal(configured));
  DapSourceBreakpoint breakpoint;
  breakpoint.line = concurrencyMarkerLine(source);
  QVERIFY(breakpoint.line > 0);
  client.setBreakpoints(source, {breakpoint});
  client.configurationDone();
  QVERIFY(waitForDebugSignal(stopped));
  const int alpha = client.currentThreadId();
  QSignalSpy threads(&client, &DapClient::threadsReceived);
  client.getThreads();
  QVERIFY(waitForDebugSignal(threads));
  int beta = 0;
  for (const auto &thread : threads.last().at(0).value<QList<DapThread>>())
    if (thread.name == "beta")
      beta = thread.id;
  QVERIFY(beta > 0 && beta != alpha);
  DebugPanel panel;
  panel.setDapClient(&client);
  client.getThreads();
  auto *selector = panel.findChild<QComboBox *>("debugThreadSelector");
  QTRY_VERIFY_WITH_TIMEOUT(selector->findData(beta) >= 0, 5000);
  selector->setCurrentIndex(selector->findData(beta));
  QCOMPARE(client.currentThreadId(), beta);
  DapStackFrame frame;
  QVERIFY(workerFrame(client, beta, source, &frame));
  DapVariable value;
  QVERIFY(evaluateDebugObject(client, frame.id, "payload['worker']", &value));
  QCOMPARE(value.value, QString("'beta'"));
  selector->setCurrentIndex(selector->findData(alpha));
  QCOMPARE(client.currentThreadId(), alpha);
  QVERIFY(workerFrame(client, alpha, source, &frame));
  QVERIFY(evaluateDebugObject(client, frame.id, "payload['worker']", &value));
  QCOMPARE(value.value, QString("'alpha'"));
  client.continueExecution();
  QVERIFY(waitForDebugSignal(exited));
  QCOMPARE(exited.last().first().toInt(), 0);
  client.stop(true);
}

void TestDap::testCppThreadSelection() {
  if (QStandardPaths::findExecutable("gdb").isEmpty() ||
      QStandardPaths::findExecutable("c++").isEmpty())
    QSKIP("C++ concurrency debugging requires a compiler and GDB with DAP "
          "support");
#ifndef Q_OS_LINUX
  QSKIP("The named native thread fixture currently uses Linux pthread names");
#endif
  const QString source =
      QFINDTESTDATA("../fixtures/debugger/concurrency_threads.cpp");
  QTemporaryDir dir;
  const QString binary = dir.filePath("threads");
  QProcess compiler;
  compiler.start("c++",
                 {"-g", "-O0", "-std=c++17", "-pthread", source, "-o", binary});
  QVERIFY(compiler.waitForFinished(30000));
  QVERIFY2(compiler.exitCode() == 0,
           compiler.readAllStandardError().constData());
  DapClient client;
  client.setAdapterMetadata("cppdbg-gdb", "cppdbg");
  QSignalSpy ready(&client, &DapClient::initialized);
  QSignalSpy configured(&client, &DapClient::adapterInitialized);
  QSignalSpy stopped(&client, &DapClient::stopped);
  QSignalSpy exited(&client, &DapClient::exited);
  QVERIFY(client.start("gdb", {"--interpreter=dap"}));
  QVERIFY(waitForDebugSignal(ready));
  QVERIFY(waitForDebugSignal(configured));
  DapSourceBreakpoint breakpoint;
  breakpoint.line = concurrencyMarkerLine(source);
  QVERIFY(breakpoint.line > 0);
  client.setBreakpoints(source, {breakpoint});
  client.configurationDone();
  DebugConfiguration config;
  config.program = binary;
  client.launch(DebugAdapterRegistry::instance()
                    .adapter("cppdbg-gdb")
                    ->launchArguments(config));
  QVERIFY(waitForDebugSignal(stopped));
  const int alpha = client.currentThreadId();
  QSignalSpy threads(&client, &DapClient::threadsReceived);
  client.getThreads();
  QVERIFY(waitForDebugSignal(threads));
  int beta = 0;
  for (const auto &thread : threads.last().at(0).value<QList<DapThread>>())
    if (thread.name == "beta")
      beta = thread.id;
  QVERIFY(beta > 0 && beta != alpha);
  DapStackFrame frame;
  QVERIFY(workerFrame(client, beta, source, &frame));
  DapVariable value;
  QVERIFY(evaluateDebugObject(client, frame.id, "payload.id", &value));
  QCOMPARE(value.value, QString("2"));
  QVERIFY(workerFrame(client, alpha, source, &frame));
  QVERIFY(evaluateDebugObject(client, frame.id, "payload.id", &value));
  QCOMPARE(value.value, QString("1"));
  client.continueExecution();
  QVERIFY(waitForDebugSignal(exited));
  QCOMPARE(exited.last().first().toInt(), 0);
  client.stop(true);
}

void TestDap::testPythonChildProcesses_data() {
  QTest::addColumn<QString>("method");
  QTest::newRow("spawn") << QString("spawn");
#ifdef Q_OS_UNIX
  QTest::newRow("fork") << QString("fork");
#endif
}

void TestDap::testPythonChildProcesses() {
  cleanupWatches();
  const QString python = concurrencyPython();
  if (python.isEmpty())
    QSKIP("Python concurrency integration tests require python3 with debugpy");
  QFETCH(QString, method);
  const QString source =
      QFINDTESTDATA("../fixtures/debugger/concurrency_processes.py");
  auto &manager = DebugSessionManager::instance();
  auto &breakpoints = BreakpointManager::instance();
  auto cleanup = qScopeGuard([&]() {
    manager.stopAllSessions(true);
    breakpoints.clearAll();
  });
  breakpoints.clearAll();
  Breakpoint breakpoint;
  breakpoint.filePath = source;
  breakpoint.line = concurrencyMarkerLine(source);
  QVERIFY(breakpoint.line > 0);
  breakpoints.addBreakpoint(breakpoint);
  DebugConfiguration config;
  config.name = "Multiprocessing test";
  config.adapterId = "python-debugpy";
  config.type = "debugpy";
  config.request = "launch";
  config.program = source;
  config.cwd = QFileInfo(source).absolutePath();
  config.pythonMode = PythonProjectEnvironment::customInterpreterMode();
  config.pythonInterpreter = python;
  config.env["LIGHTPAD_MP_METHOD"] = method;
  config.adapterConfig = {{"console", "internalConsole"}, {"subProcess", true}};
  QSignalSpy stops(&manager, &DebugSessionManager::sessionStopped);
  QSignalSpy errors(&manager, &DebugSessionManager::sessionError);
  DebugPanel debugPanel;
  for (auto *button :
       debugPanel.findChildren<QToolButton *>("debugInspectorTab"))
    if (button->text() == "Concurrency")
      button->click();
  debugPanel.resize(1350, 700);
  debugPanel.applyTheme(Theme());
  debugPanel.show();
  const QString root = manager.startSession(config);
  QVERIFY2(!root.isEmpty(), qPrintable(manager.lastError()));
  QVERIFY2(waitForDebugSignal(stops, 2, 30000),
           qPrintable(manager.lastError()));
  QCOMPARE(errors.count(), 0);
  QSet<int> workers;
  QSet<int> pids;
  QStringList childIds;
  QMap<QString, int> childWorkers;
  for (auto *session : manager.allSessions()) {
    if (session->parentSessionId() != root ||
        session->state() != DebugSession::State::Stopped)
      continue;
    childIds << session->id();
    pids.insert(session->processId());
    QVERIFY(!session->configuration()
                 .adapterConfig["connect"]
                 .toObject()
                 .isEmpty());
    DapStackFrame frame;
    QVERIFY(currentDebugFrame(*session->client(), &frame));
    DapVariable value;
    QVERIFY(evaluateDebugObject(*session->client(), frame.id,
                                "payload['worker']", &value));
    workers.insert(value.value.toInt());
    childWorkers[session->id()] = value.value.toInt();
    QVERIFY(evaluateDebugObject(*session->client(), frame.id, "phase", &value));
    QCOMPARE(value.value, QString("0"));
    manager.setFocusedSession(session->id());
    QCOMPARE(breakpoints.dapClient(), session->client());
  }
  QCOMPARE(workers, (QSet<int>{1, 2}));
  QCOMPARE(pids.size(), 2);
  QVERIFY(!pids.contains(0));
  auto *panel = debugPanel.findChild<DebugConcurrencyPanel *>();
  panel->refresh();
  auto *tree = panel->findChild<QTreeWidget *>("debugProcessesTree");
  QVERIFY(tree);
  bool foundParent = false;
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    auto *item = tree->topLevelItem(i);
    if (item->data(0, Qt::UserRole).toString() != root)
      continue;
    int children = 0;
    for (int j = 0; j < item->childCount(); ++j)
      if (childIds.contains(item->child(j)->data(0, Qt::UserRole).toString()))
        ++children;
    QCOMPARE(children, 2);
    foundParent = true;
  }
  QVERIFY(foundParent);
  const QString selectedChild = childIds.first();
  const auto items =
      tree->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
  for (auto *item : items) {
    if (item->data(0, Qt::UserRole).toString() == selectedChild &&
        item->data(0, Qt::UserRole + 1).toInt() == 0) {
      tree->setCurrentItem(item);
      tree->itemActivated(item, 0);
      break;
    }
  }
  QCOMPARE(manager.focusedSession()->id(), selectedChild);
  manager.pauseAllSessions();
  QVERIFY(waitForDebugSignal(stops, 3));
  QCOMPARE(manager.session(root)->state(), DebugSession::State::Stopped);
  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_PROCESS_SCREENSHOT")) {
    QTest::qWait(100);
    debugPanel.grab().save(qEnvironmentVariable("LIGHTPAD_PROCESS_SCREENSHOT"));
  }

  breakpoints.removeBreakpoint(source, breakpoint.line);
  QTest::qWait(250);
  manager.continueAllSessions();
  QTRY_COMPARE_WITH_TIMEOUT(manager.sessionCount(), 0, 30000);
  QVERIFY2(errors.isEmpty(),
           errors.isEmpty() ? "" : qPrintable(errors.last().at(1).toString()));
  QCOMPARE(stops.count(), 3);
  QString output;
  QSet<QString> retained;
  for (const auto &session : manager.recentSessions()) {
    if (childIds.contains(session.id) || session.id == root) {
      retained.insert(session.id);
      output += session.output;
      if (childWorkers.contains(session.id))
        QVERIFY2(
            session.output.contains(
                QString("process-debug-ok %1 1").arg(childWorkers[session.id])),
            qPrintable(session.name + ": " + session.output));
    }
  }
  QVERIFY(retained.contains(root));
  for (const auto &id : childIds)
    QVERIFY(retained.contains(id));
  QVERIFY2(output.contains("process-debug-ok 1 1"), qPrintable(output));
  QVERIFY2(output.contains("process-debug-ok 2 1"), qPrintable(output));
  QVERIFY2(output.contains("parent-debug-ok"), qPrintable(output));
}

void TestDap::testAsyncioTaskInspection() {
  const QString python = concurrencyPython();
  if (python.isEmpty())
    QSKIP("Python concurrency integration tests require python3 with debugpy");
  const QString source =
      QFINDTESTDATA("../fixtures/debugger/concurrency_async.py");
  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");
  QSignalSpy ready(&client, &DapClient::initialized);
  QSignalSpy configured(&client, &DapClient::adapterInitialized);
  QSignalSpy stopped(&client, &DapClient::stopped);
  QSignalSpy exited(&client, &DapClient::exited);
  QVERIFY(client.start(python, {"-m", "debugpy.adapter"}));
  QVERIFY(waitForDebugSignal(ready));
  client.launch({{"program", source}, {"console", "internalConsole"}});
  QVERIFY(waitForDebugSignal(configured));
  DapSourceBreakpoint breakpoint;
  breakpoint.line = concurrencyMarkerLine(source);
  QVERIFY(breakpoint.line > 0);
  client.setBreakpoints(source, {breakpoint});
  client.configurationDone();
  QVERIFY(waitForDebugSignal(stopped));
  DapStackFrame frame;
  QVERIFY(currentDebugFrame(client, &frame));
  DebugConcurrencyPanel panel;
  panel.setFrameContext(&client, frame.id);
  panel.inspectAsyncTasks();
  auto *table = panel.findChild<QTableWidget *>("debugValueTable");
  QVERIFY(table);
  QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 3, 15000);
  QTRY_VERIFY_WITH_TIMEOUT(table->item(2, 0), 15000);
  QTRY_VERIFY_WITH_TIMEOUT(table->columnCount() > 1 && table->item(2, 1) &&
                               !table->item(2, 1)->text().isEmpty(),
                           15000);
  QString cells;
  for (int row = 0; row < table->rowCount(); ++row)
    for (int column = 0; column < table->columnCount(); ++column)
      if (table->item(row, column))
        cells += table->item(row, column)->text() + "\n";
  QVERIFY2(cells.contains("io-reader") && cells.contains("io-writer") &&
               cells.contains("driver"),
           qPrintable(cells));
  QVERIFY(cells.contains("current") && cells.contains("waiting"));
  int framesColumn = -1;
  for (int i = 0; i < table->columnCount(); ++i)
    if (table->horizontalHeaderItem(i)->text().contains("Frames"))
      framesColumn = i;
  QVERIFY(framesColumn >= 0);
  table->cellActivated(1, framesColumn);
  auto *tree = panel.findChild<QTreeWidget *>("debugValueTree");
  auto *frames = tree->currentItem();
  QVERIFY(frames && frames->text(0).contains("Frames"));
  QTRY_VERIFY_WITH_TIMEOUT(frames->childCount() > 0, 5000);
  auto *taskFrame = frames->child(0);
  taskFrame->setExpanded(true);
  auto namedChild = [](QTreeWidgetItem *item, const QString &name) {
    for (int i = 0; i < item->childCount(); ++i)
      if (item->child(i)->text(0) == name ||
          item->child(i)->text(0) == "'" + name + "'")
        return item->child(i);
    return static_cast<QTreeWidgetItem *>(nullptr);
  };
  QTRY_VERIFY_WITH_TIMEOUT(namedChild(taskFrame, "Locals"), 5000);
  auto *locals = namedChild(taskFrame, "Locals");
  locals->setExpanded(true);
  QTRY_VERIFY_WITH_TIMEOUT(namedChild(locals, "payload"), 5000);
  auto *payload = namedChild(locals, "payload");
  payload->setExpanded(true);
  QTRY_VERIFY_WITH_TIMEOUT(namedChild(payload, "worker"), 5000);
  QCOMPARE(namedChild(payload, "worker")->text(1), QString("'reader'"));
  DapVariable value;
  QVERIFY(evaluateDebugObject(
      client, frame.id,
      "next(t for t in __import__('asyncio').all_tasks() if t.get_name() == "
      "'io-reader').get_coro().cr_frame.f_locals['payload']['worker']",
      &value));
  QCOMPARE(value.value, QString("'reader'"));
  if (!qEnvironmentVariableIsEmpty("LIGHTPAD_CONCURRENCY_SCREENSHOT")) {
    panel.applyTheme(Theme());
    panel.resize(1150, 650);
    panel.findChild<QTabWidget *>()->setCurrentIndex(1);
    panel.findChild<QTabWidget *>("debugValueTabs")->setCurrentIndex(1);
    panel.show();
    QTest::qWait(100);
    panel.grab().save(qEnvironmentVariable("LIGHTPAD_CONCURRENCY_SCREENSHOT"));
  }

  client.stepOver(client.currentThreadId());
  QVERIFY(waitForDebugSignal(stopped, 2));
  client.stepOver(client.currentThreadId());
  QVERIFY(waitForDebugSignal(stopped, 3));
  QVERIFY(currentDebugFrame(client, &frame));
  panel.setFrameContext(&client, frame.id);
  panel.inspectAsyncTasks();
  auto hasCancellingTask = [table]() {
    for (int row = 0; row < table->rowCount(); ++row)
      for (int column = 0; column < table->columnCount(); ++column)
        if (table->item(row, column) &&
            table->item(row, column)->text() == "'cancelling'")
          return true;
    return false;
  };
  QTRY_VERIFY_WITH_TIMEOUT(hasCancellingTask(), 10000);
  client.continueExecution();
  QVERIFY(waitForDebugSignal(exited));
  QCOMPARE(exited.last().first().toInt(), 0);
  QVERIFY(
      !panel.findChild<QPushButton *>("debugInspectAsyncTasks")->isEnabled());
  QVERIFY(panel.findChild<QLabel *>("debugValueStatus")
              ->text()
              .contains("Snapshot"));
  client.stop(true);
}

void TestDap::testProcessScopedDataBreakpoints() {
  auto &manager = BreakpointManager::instance();
  manager.clearAll();
  DapClient first;
  DapClient second;
  auto cleanup = qScopeGuard([&]() {
    manager.setDapClient(nullptr);
    manager.clearAll();
  });
  manager.setDapClient(&first);
  manager.addDataBreakpoint("process-owned-id", "write", "first worker");
  manager.setDapClient(&second);
  QVERIFY(manager.allDataBreakpoints().isEmpty());
  manager.addDataBreakpoint("process-owned-id", "write", "second worker");
  manager.setDapClient(&first);
  QCOMPARE(manager.allDataBreakpoints().size(), 1);
  QCOMPARE(manager.allDataBreakpoints().first().description,
           QString("first worker"));
  manager.setDapClient(&second);
  QCOMPARE(manager.allDataBreakpoints().first().description,
           QString("second worker"));
  manager.clearAll();
  manager.setDapClient(&first);
  QVERIFY(manager.allDataBreakpoints().isEmpty());
}

void TestDap::testContinueCancelsPendingPause() {
  QTemporaryDir dir;
  const QString trace = dir.filePath("requests.jsonl");
  DapClient client;
  QVERIFY(client.start(fakeAdapterPath(),
                       {"--trace", trace, "--hang-on", "pause"}));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Ready, 5000);
  client.launch(QJsonObject{{"program", "/tmp/test-program"}});
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Running, 5000);
  client.pauseAllThreads();
  QTRY_VERIFY_WITH_TIMEOUT(traceFileContains(trace, "\"pause\""), 5000);
  QVERIFY(client.hasPendingPause());
  client.continueExecution();
  QTRY_VERIFY_WITH_TIMEOUT(!client.hasPendingPause(), 5000);
  QCOMPARE(client.state(), DapClient::State::Running);
  client.stop(false);
}

void TestDap::testTabbedDebugDockCanShrinkBelowTwentyPercent_data() {
  QTest::addColumn<int>("windowHeight");
  QTest::newRow("small-window") << 600;
  QTest::newRow("laptop") << 768;
  QTest::newRow("desktop") << 1050;
  QTest::newRow("large-desktop") << 1440;
}

void TestDap::testTabbedDebugDockCanShrinkBelowTwentyPercent() {
  QFETCH(int, windowHeight);
  QMainWindow window;
  window.setDockOptions(DockUtils::mainWindowDockOptions());
  window.resize(1400, windowHeight);
  auto *editor = new QWidget(&window);
  window.setCentralWidget(editor);
  auto *debug = new QDockWidget("Debug", &window);
  DockUtils::configureToolPanelDock(debug);
  auto *panel = new DebugPanel(debug);
  panel->applyTheme(Theme());
  debug->setWidget(panel);
  window.addDockWidget(Qt::BottomDockWidgetArea, debug);
  auto *terminal = new QDockWidget("Terminal", &window);
  DockUtils::configureToolPanelDock(terminal);
  auto *terminalContent = new QPlainTextEdit(terminal);
  // Measure the debug panel's constraints, not QPlainTextEdit's unrelated
  // minimum height in the sibling tab.
  terminalContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
  terminal->setWidget(terminalContent);
  window.addDockWidget(Qt::BottomDockWidgetArea, terminal);
  window.tabifyDockWidget(debug, terminal);
  window.show();
  terminal->raise();
  QTest::qWait(50);
  window.resizeDocks({terminal}, {1}, Qt::Vertical);
  QTest::qWait(50);
  QVERIFY2(window.height() - editor->height() <= window.height() / 5,
           "A tabbed debug panel must allow the bottom area to shrink below "
           "20% of the window");
  debug->raise();
  for (auto *button : panel->findChildren<QToolButton *>("debugInspectorTab")) {
    button->click();
    QCoreApplication::processEvents();
    QVERIFY2(
        window.height() - editor->height() <= window.height() / 5,
        qPrintable("Opening " + button->text() + " expanded the bottom dock"));
  }

  window.resizeDocks({debug}, {window.height() / 2}, Qt::Vertical);
  QCoreApplication::processEvents();
  QVERIFY(debug->height() > window.height() / 5);
}

QTEST_MAIN(TestDap)
#include "test_dap.moc"
