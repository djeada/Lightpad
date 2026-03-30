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

  // Initial state
  void testInitialCounts();
  void testInitialTreeEmpty();

  // Discovery populates tree
  void testDiscoveryPopulatesTree();
  void testDiscoveryWithSuites();
  void testDiscoveryEmptyList();
  void testDiscoveryReplacesPrevious();

  // Run state transitions
  void testOnRunStartedClearsState();
  void testOnRunFinishedUpdatesCounts();
  void testOnTestFinishedAddsItem();
  void testCountsChangedSignal();

  // Filter behavior
  void testFilterAllShowsEverything();
  void testFilterFailedHidesPassed();

  // Auto-run toggle
  void testAutoRunToggle();
  void testAutoRunModeCombo();

  // Discovery adapter wiring
  void testSetDiscoveryAdapter();

private:
  QTemporaryDir m_tempDir;
  QTreeWidget *findTree(TestPanel &panel);
  QLabel *findStatusLabel(TestPanel &panel);
  QComboBox *findFilterCombo(TestPanel &panel);
  TestRunManager *findRunManager(TestPanel &panel);
};

void TestTestPanel::initTestCase() {
  qRegisterMetaType<TestResult>("TestResult");
  qRegisterMetaType<DiscoveredTest>("DiscoveredTest");
  qRegisterMetaType<QList<DiscoveredTest>>("QList<DiscoveredTest>");
  QVERIFY(m_tempDir.isValid());
}

void TestTestPanel::cleanupTestCase() {}

QTreeWidget *TestTestPanel::findTree(TestPanel &panel) {
  return panel.findChild<QTreeWidget *>();
}

QLabel *TestTestPanel::findStatusLabel(TestPanel &panel) {
  QList<QLabel *> labels = panel.findChildren<QLabel *>();
  for (QLabel *l : labels) {
    if (l->text().contains("Passed") || l->text().contains("Total") ||
        l->text().isEmpty() || l->text().contains("0"))
      return l;
  }
  return labels.isEmpty() ? nullptr : labels.last();
}

QComboBox *TestTestPanel::findFilterCombo(TestPanel &panel) {
  QList<QComboBox *> combos = panel.findChildren<QComboBox *>();
  for (QComboBox *cb : combos) {
    if (cb->count() > 0 && cb->itemText(0) == "All" &&
        cb->itemText(1) == "Failed")
      return cb;
  }
  return nullptr;
}

TestRunManager *TestTestPanel::findRunManager(TestPanel &panel) {
  return panel.findChild<TestRunManager *>();
}

// --- Initial state tests ---

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

// --- Discovery tests ---

void TestTestPanel::testDiscoveryPopulatesTree() {
  TestPanel panel;

  // Use a concrete subclass to emit discoveryFinished
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

  // Trigger discovery
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

  // One suite item + one standalone item
  QCOMPARE(tree->topLevelItemCount(), 2);

  // Find the suite item
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
    void discover(const QString &) override {
      emit discoveryFinished(batch);
    }
    void cancel() override {}
  };

  MockDiscoveryAdapter mockAdapter;
  panel.setDiscoveryAdapter(&mockAdapter);
  panel.setWorkspaceFolder(m_tempDir.path());

  // First discovery
  DiscoveredTest t1;
  t1.name = "old_test";
  t1.id = "old_test";
  mockAdapter.batch = {t1};
  mockAdapter.discover(m_tempDir.path());

  QTreeWidget *tree = findTree(panel);
  QCOMPARE(tree->topLevelItemCount(), 1);

  // Second discovery replaces
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

// --- Run state transition tests ---

void TestTestPanel::testOnRunStartedClearsState() {
  TestPanel panel;

  // Simulate run started via internal run manager
  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  // Emit runStarted signal to trigger panel state change
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

  // Emit runFinished with specific counts
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

  // Clear via runStarted
  emit runMgr->runStarted();

  // Emit testFinished for a passing test
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

// --- Filter tests ---

void TestTestPanel::testFilterAllShowsEverything() {
  TestPanel panel;
  TestRunManager *runMgr = findRunManager(panel);
  QVERIFY(runMgr != nullptr);

  emit runMgr->runStarted();

  // Add a passing and failing test
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

  // Filter to "All" (index 0) - both visible
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
  filter->setCurrentIndex(1); // "Failed"

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree != nullptr);

  // The passed test should be hidden
  QCOMPARE(tree->topLevelItem(0)->isHidden(), true);  // pass_test
  QCOMPARE(tree->topLevelItem(1)->isHidden(), false);  // fail_test
}

// --- Auto-run tests ---

void TestTestPanel::testAutoRunToggle() {
  TestPanel panel;
  AutoTestRunner *autoRunner = panel.autoTestRunner();
  QVERIFY(autoRunner != nullptr);

  QCOMPARE(autoRunner->isEnabled(), false);

  // Find the auto-run action via child QAction
  QList<QAction *> actions = panel.findChildren<QAction *>();
  QAction *autoRunAction = nullptr;
  for (QAction *a : actions) {
    if (a->isCheckable() && a->text() == "Auto") {
      autoRunAction = a;
      break;
    }
  }
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

  // Find the auto-run mode combo
  QList<QComboBox *> combos = panel.findChildren<QComboBox *>();
  QComboBox *modeCombo = nullptr;
  for (QComboBox *cb : combos) {
    if (cb->count() == 3 && cb->itemText(0) == "All on Save") {
      modeCombo = cb;
      break;
    }
  }
  QVERIFY(modeCombo != nullptr);

  // Enable auto-run first
  QAction *autoRunAction = nullptr;
  for (QAction *a : panel.findChildren<QAction *>()) {
    if (a->isCheckable() && a->text() == "Auto") {
      autoRunAction = a;
      break;
    }
  }
  QVERIFY(autoRunAction != nullptr);
  autoRunAction->setChecked(true);

  modeCombo->setCurrentIndex(1); // "File on Save"
  QCOMPARE(autoRunner->mode(), AutoRunMode::CurrentFileOnSave);

  modeCombo->setCurrentIndex(2); // "Last Selection"
  QCOMPARE(autoRunner->mode(), AutoRunMode::LastSelection);
}

// --- Discovery adapter wiring ---

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

  // Calling discover through the panel's discover method
  panel.discoverTests();
  QVERIFY(mockAdapter.discoverCalled);
}

QTEST_MAIN(TestTestPanel)
#include "test_testpanel.moc"
