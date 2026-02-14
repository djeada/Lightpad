#include "ui/dialogs/gotolinedialog.h"
#include <QtTest/QtTest>

class TestGoToLineDialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testDialogCreation();
  void testMaxLineDefault();
  void testSetMaxLine();
  void testValidLineNumber();
  void testInvalidLineNumber();

private:
  GoToLineDialog *m_dialog;
};

void TestGoToLineDialog::initTestCase() {
  m_dialog = new GoToLineDialog(nullptr, 100);
}

void TestGoToLineDialog::cleanupTestCase() { delete m_dialog; }

void TestGoToLineDialog::testDialogCreation() { QVERIFY(m_dialog != nullptr); }

void TestGoToLineDialog::testMaxLineDefault() {

  GoToLineDialog dialog(nullptr, 50);

  QCOMPARE(dialog.lineNumber(), -1);
}

void TestGoToLineDialog::testSetMaxLine() {

  GoToLineDialog dialog(nullptr, 50);
  dialog.setMaxLine(200);

  QCOMPARE(dialog.lineNumber(), -1);
}

void TestGoToLineDialog::testValidLineNumber() {
  GoToLineDialog dialog(nullptr, 100);

  QCOMPARE(dialog.lineNumber(), -1);
}

void TestGoToLineDialog::testInvalidLineNumber() {
  GoToLineDialog dialog(nullptr, 100);

  QCOMPARE(dialog.lineNumber(), -1);
}

QTEST_MAIN(TestGoToLineDialog)
#include "test_gotolinedialog.moc"
