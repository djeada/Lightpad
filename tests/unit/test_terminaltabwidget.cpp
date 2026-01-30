#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTabWidget>
#include "ui/panels/terminaltabwidget.h"
#include "ui/panels/terminal.h"

class TestTerminalTabWidget : public QObject {
    Q_OBJECT

private:
    void stopAllShells(TerminalTabWidget& widget) {
        for (int i = 0; i < widget.terminalCount(); ++i) {
            Terminal* t = widget.terminalAt(i);
            if (t) t->stopShell();
        }
        QTest::qWait(100);
    }

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Test construction and destruction
    void testConstruction();
    
    // Test terminal management
    void testTerminalCount();
    void testCurrentTerminal();
};

void TestTerminalTabWidget::initTestCase()
{
    // Any one-time setup needed
}

void TestTerminalTabWidget::cleanupTestCase()
{
    // Any one-time cleanup needed
}

void TestTerminalTabWidget::testConstruction()
{
    TerminalTabWidget* widget = new TerminalTabWidget();
    QVERIFY(widget != nullptr);
    
    // Should have one terminal by default
    QVERIFY(widget->terminalCount() >= 1);
    
    // Stop all shells before cleanup to prevent hanging
    stopAllShells(*widget);
    widget->closeAllTerminals();
    delete widget;
}

void TestTerminalTabWidget::testTerminalCount()
{
    TerminalTabWidget widget;
    
    int initialCount = widget.terminalCount();
    QVERIFY(initialCount >= 1);
    
    // Stop shells before cleanup
    stopAllShells(widget);
    widget.closeAllTerminals();
}

void TestTerminalTabWidget::testCurrentTerminal()
{
    TerminalTabWidget widget;
    
    Terminal* current = widget.currentTerminal();
    QVERIFY(current != nullptr);
    
    // Stop shells before cleanup
    stopAllShells(widget);
    widget.closeAllTerminals();
}

QTEST_MAIN(TestTerminalTabWidget)
#include "test_terminaltabwidget.moc"
