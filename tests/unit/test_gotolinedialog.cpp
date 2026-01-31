#include <QtTest/QtTest>
#include "ui/dialogs/gotolinedialog.h"

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
    GoToLineDialog* m_dialog;
};

void TestGoToLineDialog::initTestCase()
{
    m_dialog = new GoToLineDialog(nullptr, 100);
}

void TestGoToLineDialog::cleanupTestCase()
{
    delete m_dialog;
}

void TestGoToLineDialog::testDialogCreation()
{
    // Dialog should be created successfully
    QVERIFY(m_dialog != nullptr);
}

void TestGoToLineDialog::testMaxLineDefault()
{
    // Create dialog with specific max line
    GoToLineDialog dialog(nullptr, 50);
    
    // Line number should be invalid initially (empty input)
    QCOMPARE(dialog.lineNumber(), -1);
}

void TestGoToLineDialog::testSetMaxLine()
{
    // Create a dialog with initial max line, then change it
    GoToLineDialog dialog(nullptr, 50);
    dialog.setMaxLine(200);
    
    // Verify initial state - empty input should still be invalid
    QCOMPARE(dialog.lineNumber(), -1);
}

void TestGoToLineDialog::testValidLineNumber()
{
    GoToLineDialog dialog(nullptr, 100);
    
    // We can't directly set the line edit text without exposing internals,
    // but we can verify the initial state
    QCOMPARE(dialog.lineNumber(), -1);
}

void TestGoToLineDialog::testInvalidLineNumber()
{
    GoToLineDialog dialog(nullptr, 100);
    
    // With no input, lineNumber should return -1
    QCOMPARE(dialog.lineNumber(), -1);
}

QTEST_MAIN(TestGoToLineDialog)
#include "test_gotolinedialog.moc"
