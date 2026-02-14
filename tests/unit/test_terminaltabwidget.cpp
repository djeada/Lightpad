#include "ui/panels/terminal.h"
#include "ui/panels/terminaltabwidget.h"
#include <QObject>
#include <QSignalSpy>
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
  void testCloseButtonConfiguration();
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

void TestTerminalTabWidget::testCloseButtonConfiguration() {
  TerminalTabWidget widget;

  QToolButton *closeButton = nullptr;
  const QList<QToolButton *> buttons = widget.findChildren<QToolButton *>();
  for (QToolButton *button : buttons) {
    if (button && button->text() == QStringLiteral("\u00D7")) {
      closeButton = button;
      break;
    }
  }

  QVERIFY(closeButton != nullptr);
  QVERIFY(closeButton->autoRaise());

  widget.closeAllTerminals();
}

QTEST_MAIN(TestTerminalTabWidget)
#include "test_terminaltabwidget.moc"
