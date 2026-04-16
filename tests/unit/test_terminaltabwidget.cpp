#include "uitesthelpers.h"

#include "ui/panels/terminal.h"
#include "ui/panels/terminaltabwidget.h"
#include "settings/theme.h"
#include <QObject>
#include <QSignalSpy>
#include <QSplitter>
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
  void testObjectNames();
  void testCloseButtonConfiguration();
  void testCloseButtonEmitsSignal();
  void testKillButtonExists();
  void testKillButtonInitialState();
  void testNewTerminalButtonClickAddsTab();
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

  QToolButton *killButton = widget.findChild<QToolButton *>("killTerminalButton");

  QVERIFY(killButton != nullptr);
  QVERIFY(killButton->autoRaise());

  widget.closeAllTerminals();
}

void TestTerminalTabWidget::testKillButtonInitialState() {
  TerminalTabWidget widget;

  QToolButton *killButton = widget.findChild<QToolButton *>("killTerminalButton");

  QVERIFY(killButton != nullptr);
  QVERIFY(killButton->isEnabled());

  widget.stopCurrentProcess();

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
  QVERIFY(tabWidget->styleSheet().contains("#101820"));
  QVERIFY(tabWidget->styleSheet().contains("#f2f5f7"));

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
