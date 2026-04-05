#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "test_templates/testconfiguration.h"
#include "test_templates/testoutputparser.h"
#include "test_templates/testrunmanager.h"

Q_DECLARE_METATYPE(TestResult)

class TestTestRunManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testInitialState();
  void testClearResults();
  void testIsRunningWhenIdle();
  void testFailedTestNamesEmpty();
  void testRunEmitsStartedSignal();
  void testRunEmitsFinishedSignal();
  void testResultAggregation();
  void testFailedTestNamesCollection();
  void testStopTerminatesProcess();
  void testRunAllWithTapOutput();
  void testRunAllWithPytestOutput();
  void testClearAfterRun();
  void testRunReplacesStaleResults();

private:
  QTemporaryDir m_tempDir;
  void createScript(const QString &path, const QString &content);
};

void TestTestRunManager::initTestCase() {
  qRegisterMetaType<TestResult>("TestResult");
  QVERIFY(m_tempDir.isValid());
}

void TestTestRunManager::cleanupTestCase() {}

void TestTestRunManager::createScript(const QString &path,
                                      const QString &content) {
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.write(content.toUtf8());
  f.close();
  f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                   QFileDevice::ExeOwner);
}

void TestTestRunManager::testInitialState() {
  TestRunManager mgr;
  QCOMPARE(mgr.isRunning(), false);
  QVERIFY(mgr.results().isEmpty());
  QVERIFY(mgr.failedTestNames().isEmpty());
}

void TestTestRunManager::testClearResults() {
  TestRunManager mgr;
  mgr.clearResults();
  QVERIFY(mgr.results().isEmpty());
  QVERIFY(mgr.failedTestNames().isEmpty());
}

void TestTestRunManager::testIsRunningWhenIdle() {
  TestRunManager mgr;
  QCOMPARE(mgr.isRunning(), false);
}

void TestTestRunManager::testFailedTestNamesEmpty() {
  TestRunManager mgr;
  QStringList names = mgr.failedTestNames();
  QVERIFY(names.isEmpty());
}

void TestTestRunManager::testRunEmitsStartedSignal() {
  TestRunManager mgr;
  QSignalSpy startSpy(&mgr, &TestRunManager::runStarted);

  TestConfiguration config;
  config.name = "echo-test";
  config.command = "echo";
  config.args = {"hello"};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QCOMPARE(startSpy.count(), 1);

  QTest::qWait(500);
  mgr.stop();
}

void TestTestRunManager::testRunEmitsFinishedSignal() {
  TestRunManager mgr;
  QSignalSpy finishSpy(&mgr, &TestRunManager::runFinished);

  TestConfiguration config;
  config.name = "echo-test";
  config.command = "echo";
  config.args = {"done"};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 3000);
}

void TestTestRunManager::testResultAggregation() {
  TestRunManager mgr;
  QSignalSpy finishSpy(&mgr, &TestRunManager::runFinished);
  QSignalSpy testFinishSpy(&mgr, &TestRunManager::testFinished);

  QString script = m_tempDir.filePath("tap_test.sh");
  createScript(script, "#!/bin/sh\n"
                       "echo '1..4'\n"
                       "echo 'ok 1 - passing_test'\n"
                       "echo 'not ok 2 - failing_test'\n"
                       "echo 'ok 3 - another_pass'\n"
                       "echo 'ok 4 - skipped_test # SKIP reason'\n");

  TestConfiguration config;
  config.name = "tap-test";
  config.command = "/bin/sh";
  config.args = {script};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 3000);

  QList<QVariant> finishArgs = finishSpy.at(0);
  int passed = finishArgs.at(0).toInt();
  int failed = finishArgs.at(1).toInt();
  int skipped = finishArgs.at(2).toInt();

  QCOMPARE(passed, 2);
  QCOMPARE(failed, 1);
  QCOMPARE(skipped, 1);

  QCOMPARE(mgr.results().size(), 4);
}

void TestTestRunManager::testFailedTestNamesCollection() {
  TestRunManager mgr;
  QSignalSpy finishSpy(&mgr, &TestRunManager::runFinished);

  QString script = m_tempDir.filePath("fail_test.sh");
  createScript(script, "#!/bin/sh\n"
                       "echo '1..3'\n"
                       "echo 'ok 1 - good_test'\n"
                       "echo 'not ok 2 - bad_test'\n"
                       "echo 'not ok 3 - worse_test'\n");

  TestConfiguration config;
  config.name = "fail-test";
  config.command = "/bin/sh";
  config.args = {script};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 3000);

  QStringList failed = mgr.failedTestNames();
  QCOMPARE(failed.size(), 2);
  QVERIFY(failed.contains("bad_test"));
  QVERIFY(failed.contains("worse_test"));
}

void TestTestRunManager::testStopTerminatesProcess() {
  TestRunManager mgr;
  QSignalSpy startSpy(&mgr, &TestRunManager::runStarted);

  TestConfiguration config;
  config.name = "sleep-test";
  config.command = "sleep";
  config.args = {"30"};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QCOMPARE(startSpy.count(), 1);
  QVERIFY(mgr.isRunning());

  mgr.stop();
  QCOMPARE(mgr.isRunning(), false);
}

void TestTestRunManager::testRunAllWithTapOutput() {
  TestRunManager mgr;
  QSignalSpy finishSpy(&mgr, &TestRunManager::runFinished);
  QSignalSpy testStartSpy(&mgr, &TestRunManager::testStarted);

  QString script = m_tempDir.filePath("alltap.sh");
  createScript(script, "#!/bin/sh\n"
                       "echo '1..2'\n"
                       "echo 'ok 1 - first'\n"
                       "echo 'ok 2 - second'\n");

  TestConfiguration config;
  config.name = "tap-all";
  config.command = "/bin/sh";
  config.args = {script};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 3000);

  QList<QVariant> finishArgs = finishSpy.at(0);
  QCOMPARE(finishArgs.at(0).toInt(), 2);
  QCOMPARE(finishArgs.at(1).toInt(), 0);
}

void TestTestRunManager::testRunAllWithPytestOutput() {
  TestRunManager mgr;
  QSignalSpy finishSpy(&mgr, &TestRunManager::runFinished);

  QString script = m_tempDir.filePath("pytest_out.sh");
  createScript(script, "#!/bin/sh\n"
                       "echo 'test_foo.py::test_bar PASSED'\n"
                       "echo 'test_foo.py::test_baz FAILED'\n"
                       "echo 'test_foo.py::test_skip SKIPPED'\n");

  TestConfiguration config;
  config.name = "pytest-mock";
  config.command = "/bin/sh";
  config.args = {script};
  config.outputFormat = "pytest";

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 3000);

  QList<QVariant> finishArgs = finishSpy.at(0);
  int passed = finishArgs.at(0).toInt();
  int failed = finishArgs.at(1).toInt();
  int skipped = finishArgs.at(2).toInt();

  QCOMPARE(passed, 1);
  QCOMPARE(failed, 1);
  QCOMPARE(skipped, 1);
}

void TestTestRunManager::testClearAfterRun() {
  TestRunManager mgr;
  QSignalSpy finishSpy(&mgr, &TestRunManager::runFinished);

  QString script = m_tempDir.filePath("clear_test.sh");
  createScript(script, "#!/bin/sh\n"
                       "echo '1..1'\n"
                       "echo 'ok 1 - pass'\n");

  TestConfiguration config;
  config.name = "clear-test";
  config.command = "/bin/sh";
  config.args = {script};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 3000);

  QCOMPARE(mgr.results().size(), 1);

  mgr.clearResults();
  QVERIFY(mgr.results().isEmpty());
  QVERIFY(mgr.failedTestNames().isEmpty());
}

void TestTestRunManager::testRunReplacesStaleResults() {
  TestRunManager mgr;
  QSignalSpy finishSpy(&mgr, &TestRunManager::runFinished);

  QString script = m_tempDir.filePath("stale_test.sh");
  createScript(script, "#!/bin/sh\n"
                       "echo '1..2'\n"
                       "echo 'ok 1 - first'\n"
                       "echo 'not ok 2 - second'\n");

  TestConfiguration config;
  config.name = "stale-test";
  config.command = "/bin/sh";
  config.args = {script};
  config.outputFormat = "tap";

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 3000);

  QCOMPARE(mgr.results().size(), 2);
  QCOMPARE(mgr.failedTestNames().size(), 1);

  mgr.runAll(config, m_tempDir.path());
  QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 2, 3000);

  QCOMPARE(mgr.results().size(), 2);
}

QTEST_MAIN(TestTestRunManager)
#include "test_testrunmanager.moc"
