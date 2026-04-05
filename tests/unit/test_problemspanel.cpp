#include <QSignalSpy>
#include <QTreeWidget>
#include <QtTest>

#include "ui/panels/problemspanel.h"

class TestProblemsPanel : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  void testInitialCounts();
  void testInitialTreeEmpty();
  void testEmptyStateLabelVisibleInitially();

  void testSetDiagnosticsPopulatesTree();
  void testSetDiagnosticsCountsErrors();
  void testSetDiagnosticsCountsWarnings();
  void testSetDiagnosticsCountsMixed();
  void testSetDiagnosticsEmptyListRemovesFile();
  void testSetDiagnosticsEmptyStateHidden();

  void testClearAllResetsToEmpty();

  void testClearFileRemovesSingleUri();
  void testClearFilePreservesOtherUris();

  void testCountsChangedSignal();
  void testProblemClickedSignal();
  void testFileCountsChangedSignal();

  void testFilterErrors();
  void testFilterWarnings();
  void testFilterAll();

  void testSetCurrentFilePath();

  void testAutoRefreshToggle();
  void testOnFileSavedEmitsRefreshWhenAutoRefreshEnabled();
  void testOnFileSavedDoesNotEmitWhenAutoRefreshDisabled();

private:
  LspDiagnostic makeDiag(LspDiagnosticSeverity severity, const QString &message,
                         int line = 0, int col = 0);
  QTreeWidget *findTree(ProblemsPanel &panel);
  QLabel *findEmptyStateLabel(ProblemsPanel &panel);
  QLabel *findStatusLabel(ProblemsPanel &panel);
};

void TestProblemsPanel::initTestCase() {}

LspDiagnostic TestProblemsPanel::makeDiag(LspDiagnosticSeverity severity,
                                          const QString &message, int line,
                                          int col) {
  LspDiagnostic d;
  d.severity = severity;
  d.message = message;
  d.range.start.line = line;
  d.range.start.character = col;
  d.range.end.line = line;
  d.range.end.character = col + message.length();
  d.source = "test";
  d.code = "E001";
  return d;
}

QTreeWidget *TestProblemsPanel::findTree(ProblemsPanel &panel) {
  return panel.findChild<QTreeWidget *>("problemsTree");
}

QLabel *TestProblemsPanel::findEmptyStateLabel(ProblemsPanel &panel) {
  return panel.findChild<QLabel *>("problemsEmptyState");
}

QLabel *TestProblemsPanel::findStatusLabel(ProblemsPanel &panel) {
  return panel.findChild<QLabel *>("problemsStatusLabel");
}

void TestProblemsPanel::testInitialCounts() {
  ProblemsPanel panel;
  QCOMPARE(panel.errorCount(), 0);
  QCOMPARE(panel.warningCount(), 0);
  QCOMPARE(panel.infoCount(), 0);
  QCOMPARE(panel.totalCount(), 0);
}

void TestProblemsPanel::testInitialTreeEmpty() {
  ProblemsPanel panel;
  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree);
  QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestProblemsPanel::testEmptyStateLabelVisibleInitially() {
  ProblemsPanel panel;
  panel.show();
  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState);
  QVERIFY(emptyState->isVisible());
}

void TestProblemsPanel::testSetDiagnosticsPopulatesTree() {
  ProblemsPanel panel;
  panel.setCurrentFilePath("/test.cpp");
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "undefined variable", 5, 0);
  diags << makeDiag(LspDiagnosticSeverity::Warning, "unused import", 1, 0);

  panel.setDiagnostics("file:///test.cpp", diags);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree);
  QCOMPARE(tree->topLevelItemCount(), 2);
}

void TestProblemsPanel::testSetDiagnosticsCountsErrors() {
  ProblemsPanel panel;
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error 1");
  diags << makeDiag(LspDiagnosticSeverity::Error, "error 2");

  panel.setDiagnostics("file:///test.cpp", diags);

  QCOMPARE(panel.errorCount(), 2);
  QCOMPARE(panel.warningCount(), 0);
  QCOMPARE(panel.totalCount(), 2);
}

void TestProblemsPanel::testSetDiagnosticsCountsWarnings() {
  ProblemsPanel panel;
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Warning, "warning 1");

  panel.setDiagnostics("file:///test.cpp", diags);

  QCOMPARE(panel.errorCount(), 0);
  QCOMPARE(panel.warningCount(), 1);
  QCOMPARE(panel.totalCount(), 1);
}

void TestProblemsPanel::testSetDiagnosticsCountsMixed() {
  ProblemsPanel panel;
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  diags << makeDiag(LspDiagnosticSeverity::Warning, "warning");
  diags << makeDiag(LspDiagnosticSeverity::Information, "info");
  diags << makeDiag(LspDiagnosticSeverity::Hint, "hint");

  panel.setDiagnostics("file:///test.cpp", diags);

  QCOMPARE(panel.errorCount(), 1);
  QCOMPARE(panel.warningCount(), 1);
  QCOMPARE(panel.infoCount(), 1);
  QCOMPARE(panel.totalCount(), 4);
}

void TestProblemsPanel::testSetDiagnosticsEmptyListRemovesFile() {
  ProblemsPanel panel;
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");

  panel.setDiagnostics("file:///test.cpp", diags);
  QCOMPARE(panel.totalCount(), 1);

  panel.setDiagnostics("file:///test.cpp", {});
  QCOMPARE(panel.totalCount(), 0);
}

void TestProblemsPanel::testSetDiagnosticsEmptyStateHidden() {
  ProblemsPanel panel;
  panel.show();
  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState);
  QVERIFY(emptyState->isVisible());

  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  panel.setDiagnostics("file:///test.cpp", diags);

  QVERIFY(!emptyState->isVisible());
}

void TestProblemsPanel::testClearAllResetsToEmpty() {
  ProblemsPanel panel;
  panel.show();
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  panel.setDiagnostics("file:///a.cpp", diags);
  panel.setDiagnostics("file:///b.cpp", diags);
  QCOMPARE(panel.totalCount(), 2);

  panel.clearAll();
  QCOMPARE(panel.totalCount(), 0);
  QCOMPARE(panel.errorCount(), 0);

  QLabel *emptyState = findEmptyStateLabel(panel);
  QVERIFY(emptyState->isVisible());
}

void TestProblemsPanel::testClearFileRemovesSingleUri() {
  ProblemsPanel panel;
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");

  panel.setDiagnostics("file:///a.cpp", diags);
  panel.setDiagnostics("file:///b.cpp", diags);
  QCOMPARE(panel.totalCount(), 2);

  panel.clearFile("file:///a.cpp");
  QCOMPARE(panel.totalCount(), 1);
}

void TestProblemsPanel::testClearFilePreservesOtherUris() {
  ProblemsPanel panel;
  QList<LspDiagnostic> diagsA;
  diagsA << makeDiag(LspDiagnosticSeverity::Error, "error A");
  QList<LspDiagnostic> diagsB;
  diagsB << makeDiag(LspDiagnosticSeverity::Warning, "warning B");

  panel.setDiagnostics("file:///a.cpp", diagsA);
  panel.setDiagnostics("file:///b.cpp", diagsB);

  panel.clearFile("file:///a.cpp");
  QCOMPARE(panel.errorCount(), 0);
  QCOMPARE(panel.warningCount(), 1);
}

void TestProblemsPanel::testCountsChangedSignal() {
  ProblemsPanel panel;
  QSignalSpy spy(&panel, &ProblemsPanel::countsChanged);
  QVERIFY(spy.isValid());

  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  diags << makeDiag(LspDiagnosticSeverity::Warning, "warning");

  panel.setDiagnostics("file:///test.cpp", diags);

  QVERIFY(spy.count() >= 1);
  QList<QVariant> args = spy.last();
  QCOMPARE(args.at(0).toInt(), 1);
  QCOMPARE(args.at(1).toInt(), 1);
  QCOMPARE(args.at(2).toInt(), 0);
}

void TestProblemsPanel::testProblemClickedSignal() {
  ProblemsPanel panel;
  panel.setCurrentFilePath("/test.cpp");
  QSignalSpy spy(&panel, &ProblemsPanel::problemClicked);
  QVERIFY(spy.isValid());

  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error", 10, 5);
  panel.setDiagnostics("file:///test.cpp", diags);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree);
  QVERIFY(tree->topLevelItemCount() > 0);
  QTreeWidgetItem *diagItem = tree->topLevelItem(0);

  emit tree->itemClicked(diagItem, 0);

  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.first();
  QCOMPARE(args.at(1).toInt(), 10);
  QCOMPARE(args.at(2).toInt(), 5);
}

void TestProblemsPanel::testFileCountsChangedSignal() {
  ProblemsPanel panel;
  panel.setCurrentFilePath("/test.cpp");
  QSignalSpy spy(&panel, &ProblemsPanel::countsChanged);
  QVERIFY(spy.isValid());

  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  diags << makeDiag(LspDiagnosticSeverity::Warning, "warning");
  panel.setDiagnostics("file:///test.cpp", diags);

  QVERIFY(spy.count() >= 1);
}

void TestProblemsPanel::testFilterErrors() {
  ProblemsPanel panel;
  panel.setCurrentFilePath("/test.cpp");
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  diags << makeDiag(LspDiagnosticSeverity::Warning, "warning");
  panel.setDiagnostics("file:///test.cpp", diags);

  QComboBox *filterCombo = panel.findChild<QComboBox *>();
  QVERIFY(filterCombo);
  filterCombo->setCurrentIndex(1);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree);
  QCOMPARE(tree->topLevelItemCount(), 1);
}

void TestProblemsPanel::testFilterWarnings() {
  ProblemsPanel panel;
  panel.setCurrentFilePath("/test.cpp");
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  diags << makeDiag(LspDiagnosticSeverity::Warning, "warning");
  panel.setDiagnostics("file:///test.cpp", diags);

  QComboBox *filterCombo = panel.findChild<QComboBox *>();
  QVERIFY(filterCombo);
  filterCombo->setCurrentIndex(2);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree);
  QCOMPARE(tree->topLevelItemCount(), 1);
}

void TestProblemsPanel::testFilterAll() {
  ProblemsPanel panel;
  panel.setCurrentFilePath("/test.cpp");
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  diags << makeDiag(LspDiagnosticSeverity::Warning, "warning");
  panel.setDiagnostics("file:///test.cpp", diags);

  QComboBox *filterCombo = panel.findChild<QComboBox *>();
  QVERIFY(filterCombo);
  filterCombo->setCurrentIndex(0);

  QTreeWidget *tree = findTree(panel);
  QVERIFY(tree);
  QCOMPARE(tree->topLevelItemCount(), 2);
}

void TestProblemsPanel::testSetCurrentFilePath() {
  ProblemsPanel panel;
  QList<LspDiagnostic> diags;
  diags << makeDiag(LspDiagnosticSeverity::Error, "error");
  panel.setDiagnostics("file:///test.cpp", diags);

  panel.setCurrentFilePath("/test.cpp");
  panel.clearCurrentFile();

  QCOMPARE(panel.totalCount(), 0);
}

void TestProblemsPanel::testAutoRefreshToggle() {
  ProblemsPanel panel;
  QVERIFY(panel.isAutoRefreshEnabled());

  panel.setAutoRefreshEnabled(false);
  QVERIFY(!panel.isAutoRefreshEnabled());

  panel.setAutoRefreshEnabled(true);
  QVERIFY(panel.isAutoRefreshEnabled());
}

void TestProblemsPanel::testOnFileSavedEmitsRefreshWhenAutoRefreshEnabled() {
  ProblemsPanel panel;
  panel.setAutoRefreshEnabled(true);
  QSignalSpy spy(&panel, &ProblemsPanel::refreshRequested);
  QVERIFY(spy.isValid());

  panel.onFileSaved("/test.cpp");
  QCOMPARE(spy.count(), 1);
}

void TestProblemsPanel::testOnFileSavedDoesNotEmitWhenAutoRefreshDisabled() {
  ProblemsPanel panel;
  panel.setAutoRefreshEnabled(false);
  QSignalSpy spy(&panel, &ProblemsPanel::refreshRequested);
  QVERIFY(spy.isValid());

  panel.onFileSaved("/test.cpp");
  QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestProblemsPanel)
#include "test_problemspanel.moc"
