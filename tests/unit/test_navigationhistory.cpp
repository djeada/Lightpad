#include <QtTest/QtTest>
#include "core/navigationhistory.h"

class TestNavigationHistory : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testCreation();
    void testRecordLocation();
    void testGoBack();
    void testGoForward();
    void testRecordLocationIfSignificant();
    void testClear();

private:
    NavigationHistory* m_history;
};

void TestNavigationHistory::initTestCase()
{
    m_history = new NavigationHistory(nullptr);
}

void TestNavigationHistory::cleanupTestCase()
{
    delete m_history;
}

void TestNavigationHistory::testCreation()
{
    QVERIFY(m_history != nullptr);
    QVERIFY(!m_history->canGoBack());
    QVERIFY(!m_history->canGoForward());
}

void TestNavigationHistory::testRecordLocation()
{
    m_history->clear();
    
    NavigationLocation loc1;
    loc1.filePath = "/test/file1.cpp";
    loc1.line = 10;
    loc1.column = 5;
    
    m_history->recordLocation(loc1);
    
    NavigationLocation current = m_history->currentLocation();
    QCOMPARE(current.filePath, loc1.filePath);
    QCOMPARE(current.line, loc1.line);
}

void TestNavigationHistory::testGoBack()
{
    m_history->clear();
    
    NavigationLocation loc1;
    loc1.filePath = "/test/file1.cpp";
    loc1.line = 10;
    loc1.column = 5;
    
    NavigationLocation loc2;
    loc2.filePath = "/test/file2.cpp";
    loc2.line = 20;
    loc2.column = 3;
    
    m_history->recordLocation(loc1);
    m_history->recordLocation(loc2);
    
    QVERIFY(m_history->canGoBack());
    
    NavigationLocation prev = m_history->goBack();
    QCOMPARE(prev.filePath, loc1.filePath);
    QCOMPARE(prev.line, loc1.line);
}

void TestNavigationHistory::testGoForward()
{
    m_history->clear();
    
    NavigationLocation loc1;
    loc1.filePath = "/test/file1.cpp";
    loc1.line = 10;
    loc1.column = 5;
    
    NavigationLocation loc2;
    loc2.filePath = "/test/file2.cpp";
    loc2.line = 20;
    loc2.column = 3;
    
    m_history->recordLocation(loc1);
    m_history->recordLocation(loc2);
    m_history->goBack();
    
    QVERIFY(m_history->canGoForward());
    
    NavigationLocation next = m_history->goForward();
    QCOMPARE(next.filePath, loc2.filePath);
    QCOMPARE(next.line, loc2.line);
}

void TestNavigationHistory::testRecordLocationIfSignificant()
{
    m_history->clear();
    
    NavigationLocation loc1;
    loc1.filePath = "/test/file1.cpp";
    loc1.line = 10;
    loc1.column = 5;
    
    NavigationLocation loc2;
    loc2.filePath = "/test/file1.cpp";
    loc2.line = 12;  // Only 2 lines difference - not significant
    loc2.column = 5;
    
    NavigationLocation loc3;
    loc3.filePath = "/test/file1.cpp";
    loc3.line = 50;  // 40 lines difference - significant
    loc3.column = 5;
    
    m_history->recordLocation(loc1);
    m_history->recordLocationIfSignificant(loc2, 5);  // Threshold = 5
    
    // loc2 should not have been recorded (only 2 lines diff)
    QVERIFY(!m_history->canGoBack());
    
    m_history->recordLocationIfSignificant(loc3, 5);  // Threshold = 5
    
    // loc3 should have been recorded (40 lines diff)
    QVERIFY(m_history->canGoBack());
}

void TestNavigationHistory::testClear()
{
    NavigationLocation loc1;
    loc1.filePath = "/test/file1.cpp";
    loc1.line = 10;
    loc1.column = 5;
    
    m_history->recordLocation(loc1);
    m_history->clear();
    
    QVERIFY(!m_history->canGoBack());
    QVERIFY(!m_history->canGoForward());
}

QTEST_MAIN(TestNavigationHistory)
#include "test_navigationhistory.moc"
