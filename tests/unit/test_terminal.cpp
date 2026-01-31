#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QPlainTextEdit>
#include "ui/panels/terminal.h"
#include "ui/panels/shellprofile.h"

class TestTerminal : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Test construction and destruction
    void testConstruction();
    void testDestructor();

    // Test shell management
    void testStartAndStopShell();
    void testIsRunning();

    // Test working directory
    void testSetWorkingDirectory();

    // Test clear
    void testClear();

    // Test signals
    void testShellStartedSignal();
    
    // Test error handling
    void testMultipleStopCalls();
    void testRestartAfterStop();
    
    // Test new features
    void testShellProfiles();
    void testScrollbackLines();
    void testLinkDetection();
    void testSendText();
};

void TestTerminal::initTestCase()
{
    // Any one-time setup needed
}

void TestTerminal::cleanupTestCase()
{
    // Any one-time cleanup needed
}

void TestTerminal::testConstruction()
{
    Terminal* t = new Terminal();
    QVERIFY(t != nullptr);
    // Terminal auto-starts shell in setupTerminal()
    t->stopShell();
    QTest::qWait(200);
    delete t;
}

void TestTerminal::testDestructor()
{
    Terminal* t = new Terminal();
    t->stopShell(); // Make sure shell is stopped before deleting
    QTest::qWait(200);
    delete t;
    // If we get here without crash, destructor properly cleaned up
    QVERIFY(true);
}

void TestTerminal::testStartAndStopShell()
{
    Terminal terminal;
    // Terminal auto-starts, so stop it first
    terminal.stopShell();
    QTest::qWait(200);
    QVERIFY(!terminal.isRunning());
    
    bool started = terminal.startShell();
    QVERIFY(started);
    QVERIFY(terminal.isRunning());
    
    terminal.stopShell();
    QTest::qWait(200);
    QVERIFY(!terminal.isRunning());
}

void TestTerminal::testIsRunning()
{
    Terminal terminal;
    terminal.stopShell();
    QTest::qWait(200);
    
    QVERIFY(!terminal.isRunning());
    terminal.startShell();
    QVERIFY(terminal.isRunning());
    terminal.stopShell();
    QTest::qWait(200);
    QVERIFY(!terminal.isRunning());
}

void TestTerminal::testSetWorkingDirectory()
{
    Terminal terminal;
    terminal.stopShell();
    QTest::qWait(200);
    
    terminal.setWorkingDirectory("/tmp");
    bool started = terminal.startShell();
    
    QVERIFY(started);
    QVERIFY(terminal.isRunning());
    
    terminal.stopShell();
    QTest::qWait(200);
}

void TestTerminal::testClear()
{
    Terminal terminal;
    terminal.stopShell();
    QTest::qWait(200);
    
    terminal.startShell();
    QTest::qWait(200);
    
    // Clear should not crash
    terminal.clear();
    
    terminal.stopShell();
    QTest::qWait(200);
    QVERIFY(true);
}

void TestTerminal::testShellStartedSignal()
{
    Terminal terminal;
    terminal.stopShell();
    QTest::qWait(200);
    
    QSignalSpy spy(&terminal, &Terminal::shellStarted);
    
    terminal.startShell();
    
    // The signal should have been emitted
    QVERIFY(spy.count() >= 1);
    
    terminal.stopShell();
    QTest::qWait(200);
}

void TestTerminal::testMultipleStopCalls()
{
    Terminal terminal;
    // Terminal auto-starts shell
    QVERIFY(terminal.isRunning());
    
    // Multiple stop calls should not crash
    terminal.stopShell();
    terminal.stopShell();
    terminal.stopShell();
    
    QVERIFY(!terminal.isRunning());
}

void TestTerminal::testRestartAfterStop()
{
    Terminal terminal;
    // Terminal auto-starts shell
    QVERIFY(terminal.isRunning());
    
    // Stop the shell
    terminal.stopShell();
    QVERIFY(!terminal.isRunning());
    
    // Restart should work cleanly
    bool started = terminal.startShell();
    QVERIFY(started);
    QVERIFY(terminal.isRunning());
    
    // Stop and restart again to ensure no resource leaks
    terminal.stopShell();
    QVERIFY(!terminal.isRunning());
    
    started = terminal.startShell();
    QVERIFY(started);
    QVERIFY(terminal.isRunning());
    
    terminal.stopShell();
}

void TestTerminal::testShellProfiles()
{
    Terminal terminal;
    terminal.stopShell();
    QTest::qWait(200);
    
    // Test that available shell profiles returns something
    QStringList profiles = terminal.availableShellProfiles();
    QVERIFY(!profiles.isEmpty());
    
    // Test getting current shell profile
    ShellProfile profile = terminal.shellProfile();
    QVERIFY(profile.isValid());
    QVERIFY(!profile.name.isEmpty());
    QVERIFY(!profile.command.isEmpty());
    
    // Test setting shell profile by name
    if (!profiles.isEmpty()) {
        QString firstName = profiles.first();
        bool result = terminal.setShellProfileByName(firstName);
        QVERIFY(result);
        QCOMPARE(terminal.shellProfile().name, firstName);
    }
    
    // Test that an invalid profile name returns false
    bool result = terminal.setShellProfileByName("NonExistentShell12345");
    QVERIFY(!result);
}

void TestTerminal::testScrollbackLines()
{
    Terminal terminal;
    terminal.stopShell();
    QTest::qWait(200);
    
    // Test default scrollback lines
    int defaultLines = terminal.scrollbackLines();
    QVERIFY(defaultLines > 0);
    
    // Test setting scrollback lines
    terminal.setScrollbackLines(5000);
    QCOMPARE(terminal.scrollbackLines(), 5000);
    
    terminal.setScrollbackLines(0);  // Unlimited
    QCOMPARE(terminal.scrollbackLines(), 0);
    
    terminal.setScrollbackLines(1000);
    QCOMPARE(terminal.scrollbackLines(), 1000);
}

void TestTerminal::testLinkDetection()
{
    Terminal terminal;
    terminal.stopShell();
    QTest::qWait(200);
    
    // Test default link detection state
    QVERIFY(terminal.isLinkDetectionEnabled());
    
    // Test toggling link detection
    terminal.setLinkDetectionEnabled(false);
    QVERIFY(!terminal.isLinkDetectionEnabled());
    
    terminal.setLinkDetectionEnabled(true);
    QVERIFY(terminal.isLinkDetectionEnabled());
}

void TestTerminal::testSendText()
{
    Terminal terminal;
    // Terminal auto-starts shell
    QVERIFY(terminal.isRunning());
    
    // sendText should not crash when shell is running
    terminal.sendText("echo test", false);
    terminal.sendText("ls", true);
    
    terminal.stopShell();
    QTest::qWait(200);
    
    // sendText on stopped shell should not crash
    terminal.sendText("test", true);
    
    QVERIFY(true);  // If we get here without crash, test passed
}

QTEST_MAIN(TestTerminal)
#include "test_terminal.moc"
