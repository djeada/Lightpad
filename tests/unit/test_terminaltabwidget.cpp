#include <QObject>
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTabWidget>
#include <QToolButton>
#include "ui/panels/terminaltabwidget.h"
#include "ui/panels/terminal.h"

class TestTerminalTabWidget : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Test construction and destruction
    void testConstruction();
    
    // Test terminal management
    void testTerminalCount();
    void testCurrentTerminal();
    
    // Test new features (quick tests without shell interaction)
    void testAvailableShellProfiles();
    void testIsSplit();
    void testCloseButtonConfiguration();
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
    
    // closeAllTerminals handles stopping all shells
    widget->closeAllTerminals();
    delete widget;
}

void TestTerminalTabWidget::testTerminalCount()
{
    TerminalTabWidget widget;
    
    int initialCount = widget.terminalCount();
    QVERIFY(initialCount >= 1);
    
    // closeAllTerminals handles cleanup
    widget.closeAllTerminals();
}

void TestTerminalTabWidget::testCurrentTerminal()
{
    TerminalTabWidget widget;
    
    Terminal* current = widget.currentTerminal();
    QVERIFY(current != nullptr);
    
    // closeAllTerminals handles cleanup
    widget.closeAllTerminals();
}

void TestTerminalTabWidget::testAvailableShellProfiles()
{
    TerminalTabWidget widget;
    
    QStringList profiles = widget.availableShellProfiles();
    QVERIFY(!profiles.isEmpty());
    
    // Should have at least one shell profile
    QVERIFY(profiles.size() >= 1);
    
    // Each profile name should be non-empty
    for (const QString& profile : profiles) {
        QVERIFY(!profile.isEmpty());
    }
    
    // closeAllTerminals handles cleanup
    widget.closeAllTerminals();
}

void TestTerminalTabWidget::testIsSplit()
{
    TerminalTabWidget widget;
    
    // Initially not split
    QVERIFY(!widget.isSplit());
    
    // closeAllTerminals handles cleanup
    widget.closeAllTerminals();
}

void TestTerminalTabWidget::testCloseButtonConfiguration()
{
    TerminalTabWidget widget;

    QToolButton* closeButton = nullptr;
    const QList<QToolButton*> buttons = widget.findChildren<QToolButton*>();
    for (QToolButton* button : buttons) {
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
