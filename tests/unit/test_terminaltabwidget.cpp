#include "uitesthelpers.h"

#include "settings/theme.h"
#include "theme/themeengine.h"
#include "ui/panels/terminal.h"
#include "ui/panels/terminaltabwidget.h"
#include <QObject>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QtTest/QtTest>

class TestTerminalTabWidget : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testConstruction();

  void testTerminalCount();
  void testCurrentTerminal();

  void testAvailableShellProfiles();
  void testIsSplit();
  void testSplitHorizontalAddsPane();
  void testUnsplitKeepsTerminals();
  void testObjectNames();
  void testCloseButtonConfiguration();
  void testCloseButtonEmitsSignal();
  void testKillButtonExists();
  void testKillButtonInitialState();
  void testStopButtonIsVisibleAction();
  void testControlsLiveInTabCorner();
  void testNewTerminalButtonClickAddsTab();
  void testMultipleTerminalTabsRemainVisible();
  void testNewTerminalWhileProcessRunsKeepsOldTab();
  void testApplyThemeUpdatesTabStyles();
  void testSnapshotNotEmpty();
};

void TestTerminalTabWidget::initTestCase() {}

void TestTerminalTabWidget::cleanupTestCase() {}

void TestTerminalTabWidget::testConstruction() {
  TerminalTabWidget *widget = new TerminalTabWidget();
  QVERIFY(widget != nullptr);

  QVERIFY(widget->terminalCount() >= 1);

  widget->closeAllTerminals();
  delete widget;
}

void TestTerminalTabWidget::testTerminalCount() {
  TerminalTabWidget widget;

  int initialCount = widget.terminalCount();
  QVERIFY(initialCount >= 1);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testCurrentTerminal() {
  TerminalTabWidget widget;

  Terminal *current = widget.currentTerminal();
  QVERIFY(current != nullptr);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testAvailableShellProfiles() {
  TerminalTabWidget widget;

  QStringList profiles = widget.availableShellProfiles();
  QVERIFY(!profiles.isEmpty());

  QVERIFY(profiles.size() >= 1);

  for (const QString &profile : profiles) {
    QVERIFY(!profile.isEmpty());
  }

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testIsSplit() {
  TerminalTabWidget widget;

  QVERIFY(!widget.isSplit());

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testSplitHorizontalAddsPane() {
  TerminalTabWidget widget;
  const int initialCount = widget.terminalCount();

  widget.splitHorizontal();

  QVERIFY(widget.isSplit());
  QCOMPARE(widget.terminalCount(), initialCount + 1);
  QVERIFY(widget.findChild<QTabWidget *>("terminalSplitTabs") != nullptr);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testUnsplitKeepsTerminals() {
  TerminalTabWidget widget;

  widget.splitHorizontal();
  const int splitCount = widget.terminalCount();

  widget.unsplit();

  QVERIFY(!widget.isSplit());
  QCOMPARE(widget.terminalCount(), splitCount);
  QVERIFY(widget.findChild<QTabWidget *>("terminalSplitTabs") == nullptr);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testObjectNames() {
  TerminalTabWidget widget;

  QVERIFY(widget.findChild<QSplitter *>("terminalSplitter") != nullptr);
  QVERIFY(widget.findChild<QTabWidget *>("terminalTabs") != nullptr);
  QVERIFY(widget.findChild<QToolButton *>("newTerminalButton") != nullptr);
  QVERIFY(widget.findChild<QToolButton *>("clearTerminalButton") != nullptr);
  QVERIFY(widget.findChild<QToolButton *>("killTerminalButton") != nullptr);
  QVERIFY(widget.findChild<QToolButton *>("closeTerminalPanelButton") !=
          nullptr);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testCloseButtonConfiguration() {
  TerminalTabWidget widget;

  QToolButton *closeButton =
      widget.findChild<QToolButton *>("closeTerminalPanelButton");

  QVERIFY(closeButton != nullptr);
  QVERIFY(closeButton->autoRaise());

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testCloseButtonEmitsSignal() {
  TerminalTabWidget widget;
  QSignalSpy spy(&widget, &TerminalTabWidget::closeRequested);
  QVERIFY(spy.isValid());

  QToolButton *closeButton =
      widget.findChild<QToolButton *>("closeTerminalPanelButton");
  QVERIFY(closeButton != nullptr);

  QTest::mouseClick(closeButton, Qt::LeftButton);
  QCOMPARE(spy.count(), 1);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testKillButtonExists() {
  TerminalTabWidget widget;

  QToolButton *killButton =
      widget.findChild<QToolButton *>("killTerminalButton");

  QVERIFY(killButton != nullptr);
  QVERIFY(!killButton->autoRaise());

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testKillButtonInitialState() {
  TerminalTabWidget widget;

  QToolButton *killButton =
      widget.findChild<QToolButton *>("killTerminalButton");

  QVERIFY(killButton != nullptr);
  QVERIFY(killButton->isEnabled());

  widget.stopCurrentProcess();

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testStopButtonIsVisibleAction() {
  TerminalTabWidget widget;

  QToolButton *stopButton =
      widget.findChild<QToolButton *>("killTerminalButton");

  QVERIFY(stopButton != nullptr);
  QCOMPARE(stopButton->text(), QString("Stop"));
  QCOMPARE(stopButton->toolButtonStyle(), Qt::ToolButtonTextOnly);
  QVERIFY(!stopButton->autoRaise());
  QVERIFY(stopButton->toolTip().contains("Stop"));

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testControlsLiveInTabCorner() {
  TerminalTabWidget widget;
  UiTestHelpers::showWidget(widget, QSize(960, 640));

  QWidget *toolbar = widget.findChild<QWidget *>("terminalToolbar");
  QTabWidget *tabWidget = widget.findChild<QTabWidget *>("terminalTabs");
  QSplitter *splitter = widget.findChild<QSplitter *>("terminalSplitter");

  QVERIFY(toolbar != nullptr);
  QVERIFY(tabWidget != nullptr);
  QVERIFY(splitter != nullptr);
  QCOMPARE(tabWidget->cornerWidget(Qt::TopRightCorner), toolbar);
  QVERIFY(splitter->geometry().top() <= tabWidget->geometry().top() + 1);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testNewTerminalButtonClickAddsTab() {
  TerminalTabWidget widget;
  UiTestHelpers::showWidget(widget);
  const int initialCount = widget.terminalCount();

  QToolButton *newButton = widget.findChild<QToolButton *>("newTerminalButton");
  QVERIFY(newButton != nullptr);

  QTest::mouseClick(newButton, Qt::LeftButton);
  QCOMPARE(widget.terminalCount(), initialCount + 1);

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testMultipleTerminalTabsRemainVisible() {
  TerminalTabWidget widget;
  UiTestHelpers::showWidget(widget, QSize(960, 360));

  QTabWidget *tabWidget = widget.findChild<QTabWidget *>("terminalTabs");
  QVERIFY(tabWidget != nullptr);
  QTabBar *tabBar = tabWidget->tabBar();
  QVERIFY(tabBar != nullptr);

  widget.addNewTerminal();
  widget.addNewTerminal();
  QTest::qWait(50);

  QCOMPARE(tabWidget->count(), 3);
  QVERIFY(tabBar->isVisible());
  QVERIFY(!tabBar->expanding());
  QVERIFY(tabBar->usesScrollButtons());
  for (int i = 0; i < tabBar->count(); ++i) {
    QVERIFY2(tabBar->tabRect(i).width() > 0,
             qPrintable(QString("tab %1 has no visible width").arg(i)));
  }

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testNewTerminalWhileProcessRunsKeepsOldTab() {
  TerminalTabWidget widget;
  UiTestHelpers::showWidget(widget, QSize(960, 360));

  QTabWidget *tabWidget = widget.findChild<QTabWidget *>("terminalTabs");
  QVERIFY(tabWidget != nullptr);
  QTabBar *tabBar = tabWidget->tabBar();
  QVERIFY(tabBar != nullptr);

  Terminal *runningTerminal = widget.currentTerminal();
  QVERIFY(runningTerminal != nullptr);
  runningTerminal->executeCommand("sh", QStringList() << "-c" << "sleep 30",
                                  QDir::tempPath());
  QTRY_VERIFY_WITH_TIMEOUT(runningTerminal->hasActiveRunProcess(), 3000);

  QToolButton *newButton = widget.findChild<QToolButton *>("newTerminalButton");
  QVERIFY(newButton != nullptr);
  QTest::mouseClick(newButton, Qt::LeftButton);
  QTest::qWait(50);

  QCOMPARE(widget.terminalCount(), 2);
  QCOMPARE(tabWidget->count(), 2);
  QCOMPARE(widget.terminalAt(0), runningTerminal);
  QVERIFY(runningTerminal->hasActiveRunProcess());
  QVERIFY(tabBar->tabRect(0).width() > 0);
  QVERIFY(tabBar->tabRect(1).width() > 0);

  runningTerminal->stopProcess();
  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testApplyThemeUpdatesTabStyles() {
  TerminalTabWidget widget;
  Theme theme;
  theme.backgroundColor = QColor("#101820");
  theme.foregroundColor = QColor("#f2f5f7");
  theme.lineNumberAreaColor = QColor("#38444d");
  theme.errorColor = QColor("#ff5c57");

  widget.applyTheme(theme);

  QTabWidget *tabWidget = widget.findChild<QTabWidget *>("terminalTabs");
  QVERIFY(tabWidget != nullptr);
  const ThemeDefinition &activeTheme = ThemeEngine::instance().activeTheme();
  QVERIFY(tabWidget->styleSheet().contains(activeTheme.colors.termBg.name()));
  QVERIFY(tabWidget->styleSheet().contains(activeTheme.colors.termFg.name()));

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testSnapshotNotEmpty() {
  TerminalTabWidget widget;

  const QPixmap snapshot = UiTestHelpers::captureSnapshot(widget);

  QVERIFY(!snapshot.isNull());
  QCOMPARE(snapshot.size(), widget.size());

  widget.closeAllTerminals();
}

QTEST_MAIN(TestTerminalTabWidget)
#include "test_terminaltabwidget.moc"
