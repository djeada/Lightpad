#include <QDir>
#include <QLineEdit>
#include <QProgressBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include "test_templates/autotestrunner.h"
#include "test_templates/testconfiguration.h"
#include "test_templates/testdiscovery.h"
#include "test_templates/testrunmanager.h"
#include "ui/panels/testpanel.h"

Q_DECLARE_METATYPE(TestResult)
Q_DECLARE_METATYPE(DiscoveredTest)

class TestTestPanel : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testInitialCounts();
  void testInitialTreeEmpty();
  void testConfigurationComboRefreshesAfterTemplateLoad();
  void testCtestDiscoveryUsesBuildDirectory();

  void testDiscoveryPopulatesTree();
  void testDiscoveryWithSuites();
  void testDiscoveryEmptyList();
  void testDiscoveryReplacesPrevious();

  void testOnRunStartedClearsState();
  void testOnRunFinishedUpdatesCounts();
  void testOnTestFinishedAddsItem();
  void testCountsChangedSignal();

  void testFilterAllShowsEverything();
  void testFilterFailedHidesPassed();

  void testAutoRunToggle();
  void testAutoRunModeCombo();

  void testSetDiscoveryAdapter();

  void testSearchEditExists();
  void testProgressBarHiddenInitially();
  void testProgressBarVisibleDuringRun();
  void testEmptyStateLabelVisibleInitially();
  void testEmptyStateHiddenAfterDiscovery();
  void testSearchFilterByName();

  void testDoubleClickEmitsLocationClicked();
  void testDoubleClickPreservesDiscoveryFilePath();
  void testDoubleClickPytestPreservesFilePath();
  void testDoubleClickCtestIdMatchesFallback();

  void testSuiteItemCollapsedByDefault();
  void testTestItemFoldedByDefault();
  void testTestItemHasDetailChildrenOnFailure();
  void testDetailChildrenClearedOnRerun();

  void testStatusLabelShowsRunningDuringRun();
  void testStatusLabelShowsLatestStdoutLine();
  void testStatusLabelTruncatesLongStdoutLine();
  void testStatusLabelIgnoresStderrLines();
  void testStatusLabelClearedOnNewRun();

private:
  QTemporaryDir m_tempDir;
  QTreeWidget *findTree(TestPanel &panel);
  QLabel *findStatusLabel(TestPanel &panel);
  QComboBox *findFilterCombo(TestPanel &panel);
  QComboBox *findConfigCombo(TestPanel &panel);
  TestRunManager *findRunManager(TestPanel &panel);
  QLineEdit *findSearchEdit(TestPanel &panel);
  QProgressBar *findProgressBar(TestPanel &panel);
  QLabel *findEmptyStateLabel(TestPanel &panel);
};

void TestTestPanel::initTestCase() {
  qRegisterMetaType<TestResult>("TestResult");
  qRegisterMetaType<DiscoveredTest>("DiscoveredTest");
  qRegisterMetaType<QList<DiscoveredTest>>("QList<DiscoveredTest>");
  QVERIFY(m_tempDir.isValid());
}

void TestTestPanel::cleanupTestCase() {}

QTreeWidget *TestTestPanel::findTree(TestPanel &panel) {
  return panel.findChild<QTreeWidget *>("testTree");
}

QLabel *TestTestPanel::findStatusLabel(TestPanel &panel) {
  return panel.findChild<QLabel *>("statusLabel");
}

QComboBox *TestTestPanel::findFilterCombo(TestPanel &panel) {
  return panel.findChild<QComboBox *>("filterCombo");
}

QComboBox *TestTestPanel::findConfigCombo(TestPanel &panel) {
  return panel.findChild<QComboBox *>("configCombo");
}

TestRunManager *TestTestPanel::findRunManager(TestPanel &panel) {
  return panel.findChild<TestRunManager *>();
}

QLineEdit *TestTestPanel::findSearchEdit(TestPanel &panel) {
  return panel.findChild<QLineEdit *>("testSearchEdit");
}

QProgressBar *TestTestPanel::findProgressBar(TestPanel &panel) {
  return panel.findChild<QProgressBar *>("testProgressBar");
}

QLabel *TestTestPanel::findEmptyStateLabel(TestPanel &panel) {
  return panel.findChild<QLabel *>("testEmptyState");
}

void TestTestPanel::testInitialCounts() {
  TestPanel panel;
  QCOMPARE(panel.passedCount(), 0);
  QCOMPARE(panel.failedCount(), 0);
  QCOMPARE(panel.skippedCount(), 0);
  QCOMPARE(panel.erroredCount(), 0);
}

void TestTestPanel::testInitialTreeEmpty() {
  TestPanel panel;
  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestTestPanel::testConfigurationComboRefreshesAfterTemplateLoad() {
  TestPanel panel;
  QComboBox *configCombo = findConfigCombo(panel);
  QVERIFY(configCombo != nullptr);
  QCOMPARE(configCombo->count(), 0);

  QVERIFY(TestConfigurationManager::instance().loadTemplates());
  QVERIFY(configCombo->count() > 0);
}

void TestTestPanel::testCtestDiscoveryUsesBuildDirectory() {
  TestPanel panel;
  QVERIFY(TestConfigurationManager::instance().loadTemplates());

  QComboBox *configCombo = findConfigCombo(panel);
  QVERIFY(configCombo != nullptr);
  const int gtestIndex = configCombo->findData("gtest_cmake");
  QVERIFY(gtestIndex >= 0);
  configCombo->setCurrentIndex(gtestIndex);

  class CapturingDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString lastPath;

    QString adapterId() const override { return "ctest"; }
    void discover(const QString &path) override {
      lastPath = path;
      emit discoveryFinished({});
    }
    void cancel() override {}
  };

  CapturingDiscoveryAdapter adapter;
  panel.setDiscoveryAdapter(&adapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  panel.discoverTests();

  QCOMPARE(adapter.lastPath, QDir(m_tempDir.path()).absoluteFilePath("build"));
}

void TestTestPanel::testDiscoveryPopulatesTree() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest t1;
      t1.name = "test_alpha";
      t1.id = "test_alpha";
      t1.suite = "";
      tests.append(t1);

      DiscoveredTest t2;
      t2.name = "test_beta";
      t2.id = "test_beta";
      t2.suite = "";
      tests.append(t2);

      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 2);
  QCOMPARE(tree->topLevelItem(0)->text(0), "test_alpha");
  QCOMPARE(tree->topLevelItem(1)->text(0), "test_beta");
}

void TestTestPanel::testDiscoveryWithSuites() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest t1;
      t1.name = "test_one";
      t1.id = "suite::test_one";
      t1.suite = "MySuite";
      t1.filePath = "/path/test.py";
      t1.line = 10;
      tests.append(t1);

      DiscoveredTest t2;
      t2.name = "test_two";
      t2.id = "suite::test_two";
      t2.suite = "MySuite";
      tests.append(t2);

      DiscoveredTest t3;
      t3.name = "standalone";
      t3.id = "standalone";
      t3.suite = "";
      tests.append(t3);

      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  QCOMPARE(tree->topLevelItemCount(), 2);

  QTreeWidgetItem *suiteItem = nullptr;
  QTreeWidgetItem *standaloneItem = nullptr;
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    if (tree->topLevelItem(i)->text(0) == "MySuite")
      suiteItem = tree->topLevelItem(i);
    else if (tree->topLevelItem(i)->text(0) == "standalone")
      standaloneItem = tree->topLevelItem(i);
  }

  QVERIFY(suiteItem != nullptr);
  QVERIFY(standaloneItem != nullptr);
  QCOMPARE(suiteItem->childCount(), 2);
  QCOMPARE(suiteItem->child(0)->text(0), "test_one");
  QCOMPARE(suiteItem->child(1)->text(0), "test_two");
}

void TestTestPanel::testDiscoveryEmptyList() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      emit discoveryFinished(QList<DiscoveredTest>());
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestTestPanel::testDiscoveryReplacesPrevious() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QList<DiscoveredTest> batch;
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override { emit discoveryFinished(batch); }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  DiscoveredTest t1;
  t1.name = "old_test";
  t1.id = "old_test";
  mockAdapter.batch = {t1};
  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QCOMPARE(tree->topLevelItemCount(), 1);

  DiscoveredTest t2;
  t2.name = "new_test_a";
  t2.id = "new_test_a";
  DiscoveredTest t3;
  t3.name = "new_test_b";
  t3.id = "new_test_b";
  mockAdapter.batch = {t2, t3};
  mockAdapter.discover(m_tempDir.path());

  QCOMPARE(tree->topLevelItemCount(), 2);
  QCOMPARE(tree->topLevelItem(0)->text(0), "new_test_a");
}

void TestTestPanel::testOnRunStartedClearsState() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  QCOMPARE(panel.passedCount(), 0);
  QCOMPARE(panel.failedCount(), 0);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestTestPanel::testOnRunFinishedUpdatesCounts() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runFinished(5, 2, 1, 0);

  QCOMPARE(panel.passedCount(), 5);
  QCOMPARE(panel.failedCount(), 2);
  QCOMPARE(panel.skippedCount(), 1);
  QCOMPARE(panel.erroredCount(), 0);
}

void TestTestPanel::testOnTestFinishedAddsItem() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test1";
  result.name = "test_example";
  result.suite = "";
  result.status = TestStatus::Passed;
  result.durationMs = 42;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 1);
  QCOMPARE(tree->topLevelItem(0)->text(0), "test_example");
  QCOMPARE(tree->topLevelItem(0)->text(1), "Passed");
}

void TestTestPanel::testCountsChangedSignal() {
  TestPanel panel;
  QSignalSpy countsSpy(&panel, &TestPanel::countsChanged);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runFinished(3, 1, 0, 1);

  QCOMPARE(countsSpy.count(), 1);
  QList<QVariant> args = countsSpy.at(0);
  QCOMPARE(args.at(0).toInt(), 3);
  QCOMPARE(args.at(1).toInt(), 1);
  QCOMPARE(args.at(2).toInt(), 0);
  QCOMPARE(args.at(3).toInt(), 1);
}

void TestTestPanel::testFilterAllShowsEverything() {
  TestPanel panel;
  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult r1;
  r1.id = "pass1";
  r1.name = "pass_test";
  r1.suite = "";
  r1.status = TestStatus::Passed;
  emit runMgr->testFinished(r1);

  TestResult r2;
  r2.id = "fail1";
  r2.name = "fail_test";
  r2.suite = "";
  r2.status = TestStatus::Failed;
  emit runMgr->testFinished(r2);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  QComboBox *filter = findFilterCombo(panel);
  QVERIFY(filter != nullptr);
  filter->setCurrentIndex(0);

  QCOMPARE(tree->topLevelItem(0)->isHidden(), false);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), false);
}

void TestTestPanel::testFilterFailedHidesPassed() {
  TestPanel panel;
  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult r1;
  r1.id = "pass1";
  r1.name = "pass_test";
  r1.suite = "";
  r1.status = TestStatus::Passed;
  emit runMgr->testFinished(r1);

  TestResult r2;
  r2.id = "fail1";
  r2.name = "fail_test";
  r2.suite = "";
  r2.status = TestStatus::Failed;
  emit runMgr->testFinished(r2);

  QComboBox *filter = findFilterCombo(panel);
  QVERIFY(filter != nullptr);
  filter->setCurrentIndex(1);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  QCOMPARE(tree->topLevelItem(0)->isHidden(), true);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), false);
}

void TestTestPanel::testAutoRunToggle() {
  TestPanel panel;
  AutoTestRunner *autoRunner = panel.autoTestRunner();
  QVERIFY(autoRunner != nullptr);

  QCOMPARE(autoRunner->isEnabled(), false);

  QAction *autoRunAction = panel.findChild<QAction *>("autoRunAction");
  QVERIFY(autoRunAction != nullptr);

  autoRunAction->setChecked(true);
  QCOMPARE(autoRunner->isEnabled(), true);

  autoRunAction->setChecked(false);
  QCOMPARE(autoRunner->isEnabled(), false);
}

void TestTestPanel::testAutoRunModeCombo() {
  TestPanel panel;
  AutoTestRunner *autoRunner = panel.autoTestRunner();
  QVERIFY(autoRunner != nullptr);

  QComboBox *modeCombo = panel.findChild<QComboBox *>("autoRunModeCombo");
  QVERIFY(modeCombo != nullptr);

  QAction *autoRunAction = panel.findChild<QAction *>("autoRunAction");
  QVERIFY(autoRunAction != nullptr);
  autoRunAction->setChecked(true);

  modeCombo->setCurrentIndex(1);
  QCOMPARE(autoRunner->mode(), AutoRunMode::CurrentFileOnSave);

  modeCombo->setCurrentIndex(2);
  QCOMPARE(autoRunner->mode(), AutoRunMode::LastSelection);
}

void TestTestPanel::testSetDiscoveryAdapter() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    bool discoverCalled = false;
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      discoverCalled = true;
      emit discoveryFinished(QList<DiscoveredTest>());
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  panel.discoverTests();
  QVERIFY(mockAdapter.discoverCalled);
}

void TestTestPanel::testSearchEditExists() {
  TestPanel panel;
  QLineEdit *searchEdit = findSearchEdit(panel);
  QVERIFY(searchEdit != nullptr);
  QVERIFY(searchEdit->placeholderText().contains("Filter"));
}

void TestTestPanel::testProgressBarHiddenInitially() {
  TestPanel panel;
  panel.show();
  QProgressBar *progressBar = findProgressBar(panel);
  QVERIFY(progressBar != nullptr);
  QCOMPARE(progressBar->isVisible(), false);
}

void TestTestPanel::testProgressBarVisibleDuringRun() {
  TestPanel panel;
  panel.show();
  QProgressBar *progressBar = findProgressBar(panel);
  QVERIFY(progressBar != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();
  QCOMPARE(progressBar->isVisible(), true);

  emit runMgr->runFinished(1, 0, 0, 0);
  QCOMPARE(progressBar->isVisible(), false);
}

void TestTestPanel::testEmptyStateLabelVisibleInitially() {
  TestPanel panel;
  panel.show();
  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState != nullptr);
  QCOMPARE(emptyState->isVisible(), true);
}

void TestTestPanel::testEmptyStateHiddenAfterDiscovery() {
  TestPanel panel;

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest t1;
      t1.name = "test_one";
      t1.id = "test_one";
      t1.suite = "";
      tests.append(t1);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState != nullptr);
  QCOMPARE(emptyState->isVisible(), false);
}

void TestTestPanel::testSearchFilterByName() {
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult r1;
  r1.id = "alpha";
  r1.name = "test_alpha";
  r1.suite = "";
  r1.status = TestStatus::Passed;
  emit runMgr->testFinished(r1);

  TestResult r2;
  r2.id = "beta";
  r2.name = "test_beta";
  r2.suite = "";
  r2.status = TestStatus::Passed;
  emit runMgr->testFinished(r2);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 2);

  QLineEdit *searchEdit = findSearchEdit(panel);
  QVERIFY(searchEdit != nullptr);
  searchEdit->setText("alpha");

  QCOMPARE(tree->topLevelItem(0)->isHidden(), false);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), true);

  searchEdit->setText("");
  QCOMPARE(tree->topLevelItem(0)->isHidden(), false);
  QCOMPARE(tree->topLevelItem(1)->isHidden(), false);
}

void TestTestPanel::testDoubleClickEmitsLocationClicked() {
  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test1";
  result.name = "test_example";
  result.suite = "";
  result.status = TestStatus::Passed;
  result.filePath = "/path/to/test.cpp";
  result.line = 42;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), QString("/path/to/test.cpp"));
  QCOMPARE(spy.at(0).at(1).toInt(), 42);
}

void TestTestPanel::testDoubleClickPreservesDiscoveryFilePath() {
  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  class MockDiscoveryAdapter : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "test_example";
      dt.id = "test1";
      dt.suite = "";
      dt.filePath = "/discovered/path/test.cpp";
      dt.line = 10;
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  // Run the test without file path info (runner doesn't report source location)
  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test1";
  result.name = "test_example";
  result.suite = "";
  result.status = TestStatus::Passed;
  // filePath intentionally left empty — runner does not report source location
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  // The discovery file path must have been preserved
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), QString("/discovered/path/test.cpp"));
  QCOMPARE(spy.at(0).at(1).toInt(), 10);
}

void TestTestPanel::testDoubleClickPytestPreservesFilePath() {
  // pytest results carry the file path in TestResult::filePath, so the panel
  // must use result.filePath directly and emit locationClicked with it.
  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  // Simulate pytest discovery: ID = "tests/test_math.py::TestArithmetic::test_add"
  class MockPytestDiscovery : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock_pytest"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "test_add";
      dt.suite = "TestArithmetic";
      dt.filePath = "tests/test_math.py";
      dt.id = "tests/test_math.py::TestArithmetic::test_add";
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockPytestDiscovery mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  emit runMgr->runStarted();

  // pytest result carries filePath directly (as PytestParser emits it)
  TestResult result;
  result.id = "tests/test_math.py::TestArithmetic::test_add";
  result.name = "TestArithmetic::test_add";
  result.suite = "tests/test_math.py";
  result.filePath = "tests/test_math.py";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  // The result has suite="tests/test_math.py", so the tree has a suite item at
  // the top level and the actual test item is its child.
  QVERIFY(tree->topLevelItemCount() > 0);
  QTreeWidgetItem *suiteItem = tree->topLevelItem(0);
  QVERIFY(suiteItem != nullptr);
  QVERIFY(suiteItem->childCount() > 0);
  QTreeWidgetItem *item = suiteItem->child(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), QString("tests/test_math.py"));
}

void TestTestPanel::testDoubleClickCtestIdMatchesFallback() {
  // ctest results carry no filePath. The discovery stores WORKING_DIRECTORY
  // under filePath. The panel must fall back to the discovery entry whose ID
  // (test-number string) matches the result ID so double-click can navigate.
  TestPanel panel;
  QSignalSpy spy(&panel, &TestPanel::locationClicked);

  class MockCtestDiscovery : public ITestDiscoveryAdapter {
  public:
    QString adapterId() const override { return "mock_ctest"; }
    void discover(const QString &) override {
      QList<DiscoveredTest> tests;
      DiscoveredTest dt;
      dt.name = "LoggerTests";
      dt.id = "1"; // ctest uses test-number as ID
      dt.filePath = "/project/build"; // WORKING_DIRECTORY from ctest JSON
      tests.append(dt);
      emit discoveryFinished(tests);
    }
    void cancel() override {}
  };

  MockCtestDiscovery mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());
  mockAdapter.discover(m_tempDir.path());

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);
  emit runMgr->runStarted();

  // ctest result: ID is the test number, no filePath
  TestResult result;
  result.id = "1";
  result.name = "LoggerTests";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  emit tree->itemDoubleClicked(item, 0);

  // The fallback must have used the discovery filePath
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), QString("/project/build"));
}

void TestTestPanel::testSuiteItemCollapsedByDefault() {
  // Suite items remain expanded so that individual test rows are immediately
  // visible. It is the individual test rows that are collapsed (folded); the
  // user expands a test row to reveal its execution details.
  TestPanel panel;
  panel.show();

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "MySuite::test_one";
  result.name = "test_one";
  result.suite = "MySuite";
  result.status = TestStatus::Passed;
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->topLevelItemCount(), 1);

  QTreeWidgetItem *suiteItem = tree->topLevelItem(0);
  QVERIFY(suiteItem != nullptr);
  QCOMPARE(suiteItem->text(0), QString("MySuite"));
  // Suite must remain expanded so tests inside it are visible without an extra click
  QCOMPARE(suiteItem->isExpanded(), true);
}

void TestTestPanel::testTestItemFoldedByDefault() {
  // A test item with execution details must start collapsed (folded).
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test_fail";
  result.name = "test_fail";
  result.suite = "";
  result.status = TestStatus::Failed;
  result.message = "Expected 1 but got 2";
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  // Item has detail children but must not be expanded by default
  QVERIFY(item->childCount() > 0);
  QCOMPARE(item->isExpanded(), false);
}

void TestTestPanel::testTestItemHasDetailChildrenOnFailure() {
  // A failed test with message/stdout/stderr must expose them as collapsible
  // child items so the user can expand to see all execution info.
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult result;
  result.id = "test_fail";
  result.name = "test_fail";
  result.suite = "";
  result.status = TestStatus::Failed;
  result.message = "Expected 1 but got 2";
  result.stackTrace = "at test_fail() line 10";
  result.stdoutOutput = "some output line";
  result.stderrOutput = "some error line";
  emit runMgr->testFinished(result);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);

  // Expect 4 detail children: message, stack trace, stdout, stderr
  QCOMPARE(item->childCount(), 4);

  // Verify children carry execution info in their text
  QStringList childTexts;
  for (int i = 0; i < item->childCount(); ++i)
    childTexts << item->child(i)->text(0);

  QVERIFY(childTexts.at(0).contains("Message:"));
  QVERIFY(childTexts.at(1).contains("Stack Trace:"));
  QVERIFY(childTexts.at(2).contains("stdout:"));
  QVERIFY(childTexts.at(3).contains("stderr:"));

  // A passed test with no output must have no detail children
  emit runMgr->runStarted();
  // runStarted() triggers onRunStarted() which clears the tree
  QCOMPARE(tree->topLevelItemCount(), 0);

  TestResult passedResult;
  passedResult.id = "test_pass";
  passedResult.name = "test_pass";
  passedResult.suite = "";
  passedResult.status = TestStatus::Passed;
  emit runMgr->testFinished(passedResult);

  QCOMPARE(tree->topLevelItemCount(), 1);
  QTreeWidgetItem *passedItem = tree->topLevelItem(0);
  QVERIFY(passedItem != nullptr);
  QCOMPARE(passedItem->childCount(), 0);
}

void TestTestPanel::testDetailChildrenClearedOnRerun() {
  // When the run is restarted, old detail children are replaced with fresh
  // ones so stale output from a previous run is not shown.
  TestPanel panel;

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  TestResult firstRun;
  firstRun.id = "test1";
  firstRun.name = "test_example";
  firstRun.suite = "";
  firstRun.status = TestStatus::Failed;
  firstRun.message = "first failure message";
  emit runMgr->testFinished(firstRun);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);
  QCOMPARE(item->childCount(), 1);
  QVERIFY(item->child(0)->text(0).contains("first failure message"));

  // Second run: tree is cleared by onRunStarted, item is re-created
  emit runMgr->runStarted();
  // Verify the tree is cleared before the second run populates it
  QCOMPARE(tree->topLevelItemCount(), 0);

  TestResult secondRun;
  secondRun.id = "test1";
  secondRun.name = "test_example";
  secondRun.suite = "";
  secondRun.status = TestStatus::Failed;
  secondRun.message = "second failure message";
  emit runMgr->testFinished(secondRun);

  item = tree->topLevelItem(0);
  QVERIFY(item != nullptr);
  // Must have exactly one child with the new message, not two
  QCOMPARE(item->childCount(), 1);
  QVERIFY(item->child(0)->text(0).contains("second failure message"));
}

void TestTestPanel::testStatusLabelShowsRunningDuringRun() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  QVERIFY(statusLabel->text().contains("Running tests"));
}

void TestTestPanel::testStatusLabelShowsLatestStdoutLine() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();
  emit runMgr->outputLine("Building project...", false);

  QVERIFY(statusLabel->text().contains("Building project..."));
  QVERIFY(statusLabel->text().contains("Running tests"));
}

void TestTestPanel::testStatusLabelTruncatesLongStdoutLine() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  // Emit a line longer than the 120-char truncation limit
  const QString longLine = QString("x").repeated(200);
  emit runMgr->outputLine(longLine, false);

  const QString labelText = statusLabel->text();
  // The label must not contain the full 200-char line
  QVERIFY(!labelText.contains(longLine));
  // The label must contain the ellipsis truncation indicator
  QVERIFY(labelText.contains(QChar(0x2026)));
}

void TestTestPanel::testStatusLabelIgnoresStderrLines() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();
  // Emit a stderr line (isError=true); it should not appear in status label
  emit runMgr->outputLine("some error text", true);

  const QString labelText = statusLabel->text();
  QVERIFY(!labelText.contains("some error text"));
}

void TestTestPanel::testStatusLabelClearedOnNewRun() {
  TestPanel panel;
  QLabel *statusLabel = findStatusLabel(panel);
  QVERIFY(statusLabel != nullptr);

  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  // First run: emit a stdout line
  emit runMgr->runStarted();
  emit runMgr->outputLine("first run output", false);
  QVERIFY(statusLabel->text().contains("first run output"));

  // Second run: onRunStarted must clear the previous stdout line
  emit runMgr->runStarted();
  QVERIFY(!statusLabel->text().contains("first run output"));
  QVERIFY(statusLabel->text().contains("Running tests"));
}

QTEST_MAIN(TestTestPanel)
#include "test_testpanel.moc"
