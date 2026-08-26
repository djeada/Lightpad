

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <QCoreApplication>

#include "dap/breakpointmanager.h"
#include "dap/dapclient.h"
#include "dap/debugadapterregistry.h"
#include "dap/debugconfiguration.h"
#include "dap/debugsession.h"
#include "dap/debugsettings.h"

#include "build/cmakeproject.h"

Q_DECLARE_METATYPE(DapExceptionInfo)
Q_DECLARE_METATYPE(DapStoppedEvent)
Q_DECLARE_METATYPE(QList<DapVariable>)
Q_DECLARE_METATYPE(QList<DapStackFrame>)
Q_DECLARE_METATYPE(QList<DapScope>)

namespace {

constexpr int kAdapterTimeoutMs = 30000;

QString findTool(const QString &name) {
  return QStandardPaths::findExecutable(name);
}

bool pythonCanImportDebugpy(const QString &python) {
  QProcess probe;
  probe.start(python, {"-c", "import debugpy"});
  if (!probe.waitForFinished(15000)) {
    probe.kill();
    return false;
  }
  return probe.exitCode() == 0;
}

struct CppSample {
  QString sourcePath;
  QString binaryPath;
  int breakpointLine = 0;
};

CppSample writeAndBuildCppSample(const QTemporaryDir &dir, const QString &cxx,
                                 bool crashing) {
  CppSample sample;
  const QString name = crashing ? "crashy" : "sample";
  sample.sourcePath = dir.filePath(name + ".cpp");
  sample.binaryPath = dir.filePath(name);

  QFile source(sample.sourcePath);
  if (!source.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "cannot write sample source:" << sample.sourcePath;
    return CppSample{};
  }

  if (crashing) {
    source.write("// crash sample\n"
                 "#include <cstdio>\n"
                 "static void trigger(int *p) { *p = 42; }\n"
                 "int main() {\n"
                 "    int *nullPtr = nullptr;\n"
                 "    trigger(nullPtr);\n"
                 "    printf(\"unreachable\\n\");\n"
                 "    return 0;\n"
                 "}\n");
  } else {

    source.write("// breakpoint sample\n"
                 "#include <cstdio>\n"
                 "\n"
                 "int compute(int input) {\n"
                 "    volatile int doubled = input * 2;\n"
                 "    volatile int label = input + 1;\n"
                 "    return doubled + label;\n"
                 "}\n"
                 "\n"
                 "int main() {\n"
                 "    volatile int seed = 21;\n"
                 "    int result = compute(seed);\n"
                 "    printf(\"result=%d\\n\", result);\n"
                 "    return 0;\n"
                 "}\n");
  }
  source.close();

  if (!crashing) {
    sample.breakpointLine = 7;
  }

  QProcess compiler;
  compiler.setWorkingDirectory(dir.path());
  compiler.start(cxx, {"-g", "-O0", "-std=c++17", "-o", sample.binaryPath,
                       sample.sourcePath});
  if (!compiler.waitForFinished(60000)) {
    compiler.kill();
    return CppSample{};
  }
  if (compiler.exitCode() != 0) {
    qWarning() << "compile failed:" << compiler.readAllStandardError();
    return CppSample{};
  }
  if (!QFileInfo::exists(sample.binaryPath)) {
    return CppSample{};
  }
  return sample;
}

QString writePythonScript(const QTemporaryDir &dir, bool raising) {
  const QString path = dir.filePath(raising ? "raiser.py" : "app.py");
  QFile script(path);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return QString();
  }
  if (raising) {
    script.write("def boom():\n"
                 "    dividend = 10\n"
                 "    divisor = 0\n"
                 "    return dividend // divisor\n"
                 "\n"
                 "if __name__ == \"__main__\":\n"
                 "    print(\"about to fail\")\n"
                 "    boom()\n");
  } else {
    script.write("def compute(n):\n"
                 "    doubled = n * 2\n"
                 "    label = \"h\\u00e9llo w\\u00f6rld\"\n"
                 "    return doubled, label\n"
                 "\n"
                 "if __name__ == \"__main__\":\n"
                 "    value = compute(21)\n"
                 "    print(f\"value={value}\")\n");
  }
  script.close();
  return path;
}

} // namespace

class TestDapIntegration : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void cppBreakpointInspectEvaluateContinue();
  void cppStepOverMovesExecution();
  void cppConditionalBreakpointVerified();
  void cppCrashStopsWithExceptionInfo();
  void sessionLevelLaunchHitsBreakpointAndExits();
  void pythonBreakpointVariablesAndEvaluate();
  void pythonUncaughtExceptionSurfacesTraceback();
  void cmakeBuildFeedsDebugSession();

private:
  QTemporaryDir m_dir;
};

void TestDapIntegration::initTestCase() {
  QVERIFY(m_dir.isValid());
  qRegisterMetaType<DapExceptionInfo>("DapExceptionInfo");
  qRegisterMetaType<DapStoppedEvent>("DapStoppedEvent");
}

void TestDapIntegration::cleanupTestCase() {
  BreakpointManager::instance().clearAll();
}

void TestDapIntegration::cppBreakpointInspectEvaluateContinue() {
  const QString cxx = findTool("g++");
  const QString gdb = findTool("gdb");
  if (cxx.isEmpty() || gdb.isEmpty()) {
    QSKIP("g++/gdb not installed");
  }

  const CppSample sample = writeAndBuildCppSample(m_dir, cxx, false);
  if (sample.binaryPath.isEmpty()) {
    QSKIP("failed to build C++ sample");
  }

  DapClient client;
  client.setAdapterMetadata("cppdbg-gdb", "cppdbg");

  QSignalSpy initializedSpy(&client, &DapClient::adapterInitialized);
  QSignalSpy readySpy(&client, &DapClient::initialized);
  QSignalSpy stoppedSpy(&client, &DapClient::stopped);

  QVERIFY(client.start(gdb, {"--interpreter=dap"}));
  QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kAdapterTimeoutMs);
  QTRY_COMPARE_WITH_TIMEOUT(initializedSpy.count(), 1, kAdapterTimeoutMs);
  QCOMPARE(client.state(), DapClient::State::Ready);

  DapSourceBreakpoint bp;
  bp.line = sample.breakpointLine;
  client.setBreakpoints(sample.sourcePath, {bp});
  client.configurationDone();

  client.launch(QJsonObject{{"program", sample.binaryPath},
                            {"cwd", m_dir.path()},
                            {"MIMode", "gdb"},
                            {"externalConsole", false}});

  QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, kAdapterTimeoutMs);
  const DapStoppedEvent stop =
      stoppedSpy.first().at(0).value<DapStoppedEvent>();
  QCOMPARE(stop.reason, DapStoppedReason::Breakpoint);
  QVERIFY(stop.threadId > 0);

  QSignalSpy stackSpy(&client, &DapClient::stackTraceReceived);
  client.getStackTrace(stop.threadId);
  stackSpy.wait(kAdapterTimeoutMs);
  QVERIFY(stackSpy.count() >= 1);
  const QList<DapStackFrame> frames =
      stackSpy.at(stackSpy.count() - 1).at(1).value<QList<DapStackFrame>>();
  QVERIFY(!frames.isEmpty());
  QCOMPARE(frames.first().line, sample.breakpointLine);

  const int frameId = frames.first().id;
  QSignalSpy scopesSpy(&client, &DapClient::scopesReceived);
  client.getScopes(frameId);
  scopesSpy.wait(kAdapterTimeoutMs);
  QVERIFY(scopesSpy.count() >= 1);
  const QList<DapScope> scopes =
      scopesSpy.at(scopesSpy.count() - 1).at(1).value<QList<DapScope>>();
  QVERIFY(!scopes.isEmpty());

  QList<DapVariable> variables;
  for (const DapScope &scope : scopes) {
    if (scope.name.contains("Register", Qt::CaseInsensitive) ||
        scope.variablesReference <= 0) {
      continue;
    }

    QSignalSpy varsSpy(&client, &DapClient::variablesReceived);
    client.getVariables(scope.variablesReference);
    varsSpy.wait(kAdapterTimeoutMs);
    QVERIFY(varsSpy.count() >= 1);
    variables.append(
        varsSpy.at(varsSpy.count() - 1).at(1).value<QList<DapVariable>>());
  }

  bool sawInput = false;
  bool sawDoubled = false;
  for (const DapVariable &var : variables) {
    qWarning() << "[gdb-var]" << var.name << "=" << var.value;
    if (var.name == "input" && var.value == "21") {
      sawInput = true;
    }
    if (var.name == "doubled" && var.value == "42") {
      sawDoubled = true;
    }
  }
  QVERIFY(sawInput);
  QVERIFY(sawDoubled);

  QSignalSpy evaluateSpy(&client, &DapClient::evaluateResult);
  client.evaluate("doubled + input", frameId, "watch");
  evaluateSpy.wait(kAdapterTimeoutMs);
  QVERIFY(evaluateSpy.count() >= 1);
  QCOMPARE(evaluateSpy.at(evaluateSpy.count() - 1).at(1).toString(),
           QString("63"));

  QSignalSpy exitedWait(&client, &DapClient::exited);
  client.continueExecution(stop.threadId);
  QTRY_COMPARE_WITH_TIMEOUT(exitedWait.count(), 1, kAdapterTimeoutMs);
  QCOMPARE(exitedWait.at(0).at(0).toInt(), 0);
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), DapClient::State::Terminated,
                            kAdapterTimeoutMs);

  client.stop(true);
}

void TestDapIntegration::cppStepOverMovesExecution() {
  const QString cxx = findTool("g++");
  const QString gdb = findTool("gdb");
  if (cxx.isEmpty() || gdb.isEmpty()) {
    QSKIP("g++/gdb not installed");
  }

  const CppSample sample = writeAndBuildCppSample(m_dir, cxx, false);
  if (sample.binaryPath.isEmpty()) {
    QSKIP("failed to build C++ sample");
  }

  DapClient client;
  client.setAdapterMetadata("cppdbg-gdb", "cppdbg");

  QSignalSpy readySpy(&client, &DapClient::initialized);
  QSignalSpy adapterInit(&client, &DapClient::adapterInitialized);
  QSignalSpy stoppedSpy(&client, &DapClient::stopped);
  QVERIFY(client.start(gdb, {"--interpreter=dap"}));
  QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kAdapterTimeoutMs);
  QTRY_COMPARE_WITH_TIMEOUT(adapterInit.count(), 1, kAdapterTimeoutMs);

  DapSourceBreakpoint bp;
  bp.line = sample.breakpointLine;
  client.setBreakpoints(sample.sourcePath, {bp});
  client.configurationDone();
  client.launch(
      QJsonObject{{"program", sample.binaryPath}, {"cwd", m_dir.path()}});

  QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, kAdapterTimeoutMs);
  const DapStoppedEvent firstStop =
      stoppedSpy.at(0).at(0).value<DapStoppedEvent>();
  QVERIFY(firstStop.threadId > 0);

  QSignalSpy preStackSpy(&client, &DapClient::stackTraceReceived);
  client.getStackTrace(firstStop.threadId);
  preStackSpy.wait(kAdapterTimeoutMs);
  QVERIFY(preStackSpy.count() >= 1);
  const QList<DapStackFrame> preFrames = preStackSpy.at(preStackSpy.count() - 1)
                                             .at(1)
                                             .value<QList<DapStackFrame>>();
  QVERIFY(!preFrames.isEmpty());
  const int boundLine = preFrames.first().line;

  QSignalSpy stackSpy(&client, &DapClient::stackTraceReceived);
  client.stepOver(firstStop.threadId);

  QTRY_VERIFY_WITH_TIMEOUT(
      [&]() {
        for (int i = 1; i < stoppedSpy.count(); ++i) {
          if (stoppedSpy.at(i).at(0).value<DapStoppedEvent>().reason ==
              DapStoppedReason::Step) {
            return true;
          }
        }
        return false;
      }(),
      kAdapterTimeoutMs);

  int stepIndex = -1;
  for (int i = stoppedSpy.count() - 1; i >= 1; --i) {
    if (stoppedSpy.at(i).at(0).value<DapStoppedEvent>().reason ==
        DapStoppedReason::Step) {
      stepIndex = i;
      break;
    }
  }
  QVERIFY(stepIndex >= 0);
  const DapStoppedEvent stepStop =
      stoppedSpy.at(stepIndex).at(0).value<DapStoppedEvent>();
  QCOMPARE(stepStop.reason, DapStoppedReason::Step);

  client.getStackTrace(stepStop.threadId > 0 ? stepStop.threadId
                                             : firstStop.threadId);
  stackSpy.wait(kAdapterTimeoutMs);
  QVERIFY(stackSpy.count() >= 1);
  const QList<DapStackFrame> frames =
      stackSpy.at(stackSpy.count() - 1).at(1).value<QList<DapStackFrame>>();
  QVERIFY(!frames.isEmpty());
  QVERIFY(frames.first().line > boundLine);

  client.stop(true);
}

void TestDapIntegration::cppConditionalBreakpointVerified() {
  const QString cxx = findTool("g++");
  const QString gdb = findTool("gdb");
  if (cxx.isEmpty() || gdb.isEmpty()) {
    QSKIP("g++/gdb not installed");
  }

  const CppSample sample = writeAndBuildCppSample(m_dir, cxx, false);
  if (sample.binaryPath.isEmpty()) {
    QSKIP("failed to build C++ sample");
  }

  DapClient client;
  client.setAdapterMetadata("cppdbg-gdb", "cppdbg");

  QSignalSpy readySpy(&client, &DapClient::initialized);
  QSignalSpy adapterInit(&client, &DapClient::adapterInitialized);
  QSignalSpy stoppedSpy(&client, &DapClient::stopped);
  QSignalSpy bpSetSpy(&client, &DapClient::breakpointsSet);
  QSignalSpy bpChangedSpy(&client, &DapClient::breakpointChanged);
  QVERIFY(client.start(gdb, {"--interpreter=dap"}));
  QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kAdapterTimeoutMs);
  QTRY_COMPARE_WITH_TIMEOUT(adapterInit.count(), 1, kAdapterTimeoutMs);

  DapSourceBreakpoint bp;
  bp.line = sample.breakpointLine;
  bp.condition = "input == 21";
  client.setBreakpoints(sample.sourcePath, {bp});
  client.configurationDone();
  client.launch(
      QJsonObject{{"program", sample.binaryPath}, {"cwd", m_dir.path()}});

  QTRY_COMPARE_WITH_TIMEOUT(bpSetSpy.count(), 1, kAdapterTimeoutMs);
  const QList<DapBreakpoint> bound =
      bpSetSpy.at(0).at(1).value<QList<DapBreakpoint>>();
  QCOMPARE(bound.size(), 1);

  if (!bound.first().verified) {
    QTRY_VERIFY_WITH_TIMEOUT(
        [&]() {
          for (int i = 0; i < bpChangedSpy.count(); ++i) {
            if (bpChangedSpy.at(i).at(0).value<DapBreakpoint>().verified) {
              return true;
            }
          }
          return false;
        }(),
        kAdapterTimeoutMs);
  }

  QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, kAdapterTimeoutMs);
  const DapStoppedEvent stop = stoppedSpy.at(0).at(0).value<DapStoppedEvent>();
  QCOMPARE(stop.reason, DapStoppedReason::Breakpoint);

  client.stop(true);
}

void TestDapIntegration::cppCrashStopsWithExceptionInfo() {
  const QString cxx = findTool("g++");
  const QString gdb = findTool("gdb");
  if (cxx.isEmpty() || gdb.isEmpty()) {
    QSKIP("g++/gdb not installed");
  }

  const CppSample sample = writeAndBuildCppSample(m_dir, cxx, true);
  if (sample.binaryPath.isEmpty()) {
    QSKIP("failed to build C++ crash sample");
  }

  DapClient client;
  client.setAdapterMetadata("cppdbg-gdb", "cppdbg");

  QSignalSpy readySpy(&client, &DapClient::initialized);
  QSignalSpy adapterInit(&client, &DapClient::adapterInitialized);
  QSignalSpy stoppedSpy(&client, &DapClient::stopped);
  QVERIFY(client.start(gdb, {"--interpreter=dap"}));
  QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kAdapterTimeoutMs);
  QTRY_COMPARE_WITH_TIMEOUT(adapterInit.count(), 1, kAdapterTimeoutMs);

  client.configurationDone();
  client.launch(
      QJsonObject{{"program", sample.binaryPath}, {"cwd", m_dir.path()}});

  QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, kAdapterTimeoutMs);
  const DapStoppedEvent stop = stoppedSpy.at(0).at(0).value<DapStoppedEvent>();

  QCOMPARE(stop.reason, DapStoppedReason::Exception);
  QCOMPARE(stop.rawReason, QString("signal"));

  if (client.supportsExceptionInfoRequest()) {
    QSignalSpy infoSpy(&client, &DapClient::exceptionInfoReceived);
    QSignalSpy infoErrSpy(&client, &DapClient::exceptionInfoError);
    client.exceptionInfo(stop.threadId);
    infoSpy.wait(kAdapterTimeoutMs);
    if (infoSpy.count() > 0) {
      const DapExceptionInfo info =
          infoSpy.at(0).at(1).value<DapExceptionInfo>();
      QVERIFY(info.exceptionId.toUpper().contains("SIGSEGV"));
    } else {
      QCOMPARE(infoErrSpy.count(), 1);
    }
  }

  client.stop(true);
}

void TestDapIntegration::sessionLevelLaunchHitsBreakpointAndExits() {
  const QString cxx = findTool("g++");
  const QString gdb = findTool("gdb");
  if (cxx.isEmpty() || gdb.isEmpty()) {
    QSKIP("g++/gdb not installed");
  }

  const CppSample sample = writeAndBuildCppSample(m_dir, cxx, false);
  if (sample.binaryPath.isEmpty()) {
    QSKIP("failed to build C++ sample");
  }

  BreakpointManager &breakpoints = BreakpointManager::instance();
  breakpoints.clearAll();
  breakpoints.setWorkspaceFolder(m_dir.path());

  Breakpoint bpTemplate;
  bpTemplate.filePath = sample.sourcePath;
  bpTemplate.line = sample.breakpointLine;
  QVERIFY(breakpoints.addBreakpoint(bpTemplate) > 0);

  DebugConfiguration config;
  config.name = "integration-cpp";
  config.type = "cppdbg";
  config.request = "launch";
  config.program = sample.binaryPath;
  config.cwd = m_dir.path();

  DebugSessionManager &manager = DebugSessionManager::instance();

  QSignalSpy startedSpy(&manager, &DebugSessionManager::sessionStarted);
  QSignalSpy stoppedManagerSpy(&manager, &DebugSessionManager::sessionStopped);
  QSignalSpy errorSpy(&manager, &DebugSessionManager::sessionError);

  const QString sessionId = manager.startSession(config);
  if (sessionId.isEmpty()) {
    const QString reason = manager.lastError();
    if (reason.contains("ptrace", Qt::CaseInsensitive) ||
        reason.contains("not available", Qt::CaseInsensitive)) {
      QSKIP(qPrintable(
          QStringLiteral("environment blocks debugging: %1").arg(reason)));
    }
    QFAIL(
        qPrintable(QStringLiteral("session failed to start: %1").arg(reason)));
  }

  QTRY_COMPARE_WITH_TIMEOUT(stoppedManagerSpy.count(), 1,
                            kAdapterTimeoutMs * 2);
  const DapStoppedEvent stop =
      stoppedManagerSpy.at(0).at(1).value<DapStoppedEvent>();
  QCOMPARE(stop.reason, DapStoppedReason::Breakpoint);

  QSignalSpy terminatedSpy(&manager, &DebugSessionManager::sessionTerminated);
  DebugSession *session = manager.session(sessionId);
  QVERIFY(session);
  session->continueExecution();

  QTRY_COMPARE_WITH_TIMEOUT(terminatedSpy.count(), 1, kAdapterTimeoutMs);

  manager.stopAllSessions(false);
  breakpoints.clearAll();
}

void TestDapIntegration::pythonBreakpointVariablesAndEvaluate() {
  const QString python = findTool("python3");
  if (python.isEmpty()) {
    QSKIP("python3 not installed");
  }
  if (!pythonCanImportDebugpy(python)) {
    QSKIP("debugpy module not importable (set PYTHONPATH for debugpy)");
  }

  const QString script = writePythonScript(m_dir, false);

  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");

  QSignalSpy readySpy(&client, &DapClient::initialized);
  QSignalSpy adapterInit(&client, &DapClient::adapterInitialized);
  QSignalSpy stoppedSpy(&client, &DapClient::stopped);
  QSignalSpy outputSpy(&client, &DapClient::output);

  QVERIFY(client.start(python, {"-m", "debugpy.adapter"}));
  QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kAdapterTimeoutMs);

  QJsonObject launchArgs{{"program", script},
                         {"cwd", m_dir.path()},
                         {"console", "internalConsole"},
                         {"justMyCode", false}};
  client.launch(launchArgs);

  QTRY_COMPARE_WITH_TIMEOUT(adapterInit.count(), 1, kAdapterTimeoutMs);

  DapSourceBreakpoint bp;
  bp.line = 4;
  client.setBreakpoints(script, {bp});
  client.configurationDone();

  QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, kAdapterTimeoutMs);
  const DapStoppedEvent stop = stoppedSpy.at(0).at(0).value<DapStoppedEvent>();
  QCOMPARE(stop.reason, DapStoppedReason::Breakpoint);

  QSignalSpy stackSpy(&client, &DapClient::stackTraceReceived);
  client.getStackTrace(stop.threadId);
  stackSpy.wait(kAdapterTimeoutMs);
  QVERIFY(stackSpy.count() >= 1);
  const QList<DapStackFrame> frames =
      stackSpy.at(stackSpy.count() - 1).at(1).value<QList<DapStackFrame>>();
  QVERIFY(!frames.isEmpty());
  QVERIFY(frames.first().source.path.endsWith("app.py"));
  QCOMPARE(frames.first().line, 4);

  const int frameId = frames.first().id;
  QSignalSpy scopesSpy(&client, &DapClient::scopesReceived);
  client.getScopes(frameId);
  scopesSpy.wait(kAdapterTimeoutMs);
  QVERIFY(scopesSpy.count() >= 1);
  const QList<DapScope> scopes =
      scopesSpy.at(scopesSpy.count() - 1).at(1).value<QList<DapScope>>();
  int localsRef = 0;
  for (const DapScope &scope : scopes) {
    if (localsRef == 0 && scope.variablesReference > 0 &&
        scope.name.compare("Locals", Qt::CaseInsensitive) == 0) {
      localsRef = scope.variablesReference;
    }
  }
  QVERIFY(localsRef > 0);

  QSignalSpy varsSpy(&client, &DapClient::variablesReceived);
  client.getVariables(localsRef);
  varsSpy.wait(kAdapterTimeoutMs);
  QVERIFY(varsSpy.count() >= 1);
  const QList<DapVariable> variables =
      varsSpy.at(varsSpy.count() - 1).at(1).value<QList<DapVariable>>();

  bool sawN = false;
  bool sawUnicodeLabel = false;
  for (const DapVariable &var : variables) {
    qWarning() << "[py-var]" << var.name << "=" << var.value;
    if (var.name == "n" && var.value == "21") {
      sawN = true;
    }
    if (var.name == "label" &&
        var.value.contains(QString::fromUtf8("h\u00e9llo"))) {
      sawUnicodeLabel = true;
    }
  }
  QVERIFY(sawN);
  QVERIFY(sawUnicodeLabel);

  QSignalSpy evaluateSpy(&client, &DapClient::evaluateResult);
  client.evaluate("doubled * 2", frameId, "repl");
  evaluateSpy.wait(kAdapterTimeoutMs);
  QVERIFY(evaluateSpy.count() >= 1);
  QCOMPARE(evaluateSpy.at(evaluateSpy.count() - 1).at(1).toString(),
           QString("84"));

  QSignalSpy exitedSpy(&client, &DapClient::exited);
  client.continueExecution(stop.threadId);
  QTRY_COMPARE_WITH_TIMEOUT(exitedSpy.count(), 1, kAdapterTimeoutMs);
  QCOMPARE(exitedSpy.at(0).at(0).toInt(), 0);

  bool sawProgramOutput = false;
  for (int i = 0; i < outputSpy.count(); ++i) {
    const DapOutputEvent evt = outputSpy.at(i).at(0).value<DapOutputEvent>();
    if (evt.output.contains("value=")) {
      sawProgramOutput = true;
      break;
    }
  }
  QVERIFY(sawProgramOutput);

  client.stop(true);
}

void TestDapIntegration::pythonUncaughtExceptionSurfacesTraceback() {
  const QString python = findTool("python3");
  if (python.isEmpty()) {
    QSKIP("python3 not installed");
  }
  if (!pythonCanImportDebugpy(python)) {
    QSKIP("debugpy module not importable (set PYTHONPATH for debugpy)");
  }

  const QString script = writePythonScript(m_dir, true);

  DapClient client;
  client.setAdapterMetadata("python-debugpy", "debugpy");

  QSignalSpy readySpy(&client, &DapClient::initialized);
  QSignalSpy adapterInit(&client, &DapClient::adapterInitialized);
  QSignalSpy stoppedSpy(&client, &DapClient::stopped);

  QVERIFY(client.start(python, {"-m", "debugpy.adapter"}));
  QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kAdapterTimeoutMs);

  client.launch(QJsonObject{{"program", script},
                            {"cwd", m_dir.path()},
                            {"console", "internalConsole"},
                            {"justMyCode", false}});
  QTRY_COMPARE_WITH_TIMEOUT(adapterInit.count(), 1, kAdapterTimeoutMs);

  client.setExceptionBreakpoints({"uncaught"});
  client.configurationDone();

  QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, kAdapterTimeoutMs);
  const DapStoppedEvent stop = stoppedSpy.at(0).at(0).value<DapStoppedEvent>();
  QCOMPARE(stop.reason, DapStoppedReason::Exception);

  QVERIFY(client.supportsExceptionInfoRequest());
  QSignalSpy infoSpy(&client, &DapClient::exceptionInfoReceived);
  QSignalSpy infoErrSpy(&client, &DapClient::exceptionInfoError);
  client.exceptionInfo(stop.threadId);
  infoSpy.wait(kAdapterTimeoutMs);
  QCOMPARE(infoErrSpy.count(), 0);
  QCOMPARE(infoSpy.count(), 1);

  const DapExceptionInfo info = infoSpy.at(0).at(1).value<DapExceptionInfo>();
  qWarning() << "[py-exception] id=" << info.exceptionId
             << "msg=" << info.details.message
             << "type=" << info.details.typeName
             << "trace=" << info.details.stackTrace.left(200);
  QVERIFY(info.exceptionId.contains("ZeroDivisionError"));
  QVERIFY(info.details.message.contains("division"));
  QVERIFY(info.details.stackTrace.contains("boom"));

  client.stop(true);
}

void TestDapIntegration::cmakeBuildFeedsDebugSession() {
  const QString gdb = findTool("gdb");
  const QString cmake = findTool("cmake");
  if (gdb.isEmpty() || cmake.isEmpty()) {
    QSKIP("gdb/cmake not installed");
  }

  const QString root = m_dir.path() + "/cmake_demo";
  QVERIFY(QDir().mkpath(root));
  QVERIFY(QDir().mkpath(root + "/src"));

  const auto writeFile = [](const QString &path, const QString &contents) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(contents.toUtf8());
    f.close();
  };

  writeFile(root + "/CMakeLists.txt", "cmake_minimum_required(VERSION 3.16)\n"
                                      "project(cmake_demo CXX)\n"
                                      "set(CMAKE_BUILD_TYPE Debug)\n"
                                      "add_subdirectory(src)\n");
  writeFile(root + "/src/CMakeLists.txt",
            "add_library(mathlib STATIC math.cpp)\n"
            "add_executable(cmake_demo_app main.cpp)\n"
            "target_link_libraries(cmake_demo_app PRIVATE mathlib)\n"
            "target_compile_options(cmake_demo_app PRIVATE -g -O0)\n");
  writeFile(root + "/src/math.cpp", "#include \"math.h\"\n"
                                    "int twice(int v) { return v * 2; }\n");
  writeFile(root + "/src/math.h", "int twice(int v);\n");
  writeFile(root + "/src/main.cpp", "#include \"math.h\"\n"
                                    "#include <cstdio>\n"
                                    "\n"
                                    "int main() {\n"
                                    "    int base = 21;\n"
                                    "    int doubled = twice(base);\n"
                                    "    printf(\"doubled=%d\\n\", doubled);\n"
                                    "    return 0;\n"
                                    "}\n");

  BreakpointManager &breakpoints = BreakpointManager::instance();
  breakpoints.clearAll();
  breakpoints.setWorkspaceFolder(root);
  Breakpoint bpTemplate;
  bpTemplate.filePath = root + "/src/main.cpp";
  bpTemplate.line = 6;
  QVERIFY(breakpoints.addBreakpoint(bpTemplate) > 0);

  CMakeProject project;
  QVERIFY(CMakeProject::isCMakeProject(root));

  const QList<CMakeTargetInfo> executables =
      project.parseExecutableTargets(root);
  QCOMPARE(executables.size(), 1);
  QCOMPARE(executables.first().name, QString("cmake_demo_app"));

  const QString binaryDir = CMakeProject::defaultBinaryDir(root);
  QString error;
  QVERIFY(project.configure(root, binaryDir, nullptr, &error));
  QVERIFY(project.build(binaryDir, 2, nullptr, &error));

  const QString exePath =
      CMakeProject::executablePathFor(executables.first(), binaryDir);
  QVERIFY(QFileInfo::exists(exePath));

  DebugConfiguration config;
  config.name = "cmake-demo";
  config.type = "cppdbg";
  config.request = "launch";
  config.program = exePath;
  config.cwd = root;

  DebugSessionManager &manager = DebugSessionManager::instance();
  QSignalSpy stoppedSpy(&manager, &DebugSessionManager::sessionStopped);
  QSignalSpy errorSpy(&manager, &DebugSessionManager::sessionError);

  const QString sessionId = manager.startSession(config);
  if (sessionId.isEmpty()) {
    const QString reason = manager.lastError();
    if (reason.contains("ptrace", Qt::CaseInsensitive)) {
      QSKIP(qPrintable(
          QStringLiteral("environment blocks debugging: %1").arg(reason)));
    }
    QFAIL(
        qPrintable(QStringLiteral("session failed to start: %1").arg(reason)));
  }

  QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, kAdapterTimeoutMs * 2);
  QCOMPARE(errorSpy.count(), 0);
  const DapStoppedEvent stop = stoppedSpy.at(0).at(1).value<DapStoppedEvent>();
  QCOMPARE(stop.reason, DapStoppedReason::Breakpoint);

  QSignalSpy terminatedSpy(&manager, &DebugSessionManager::sessionTerminated);
  if (DebugSession *session = manager.session(sessionId)) {
    session->continueExecution();
  }
  QTRY_COMPARE_WITH_TIMEOUT(terminatedSpy.count(), 1, kAdapterTimeoutMs);

  manager.stopAllSessions(false);
  breakpoints.clearAll();
}

QTEST_MAIN(TestDapIntegration)
#include "test_dap_integration.moc"