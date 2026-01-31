#include <QtTest/QtTest>
#include "core/recentfilesmanager.h"
#include <QTemporaryFile>

class TestRecentFilesManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testManagerCreation();
    void testAddFile();
    void testRemoveFile();
    void testClearAll();
    void testMaxFiles();
    void testContains();

private:
    RecentFilesManager* m_manager;
    QStringList m_tempFiles;
};

void TestRecentFilesManager::initTestCase()
{
    m_manager = new RecentFilesManager(nullptr);
    m_manager->clearAll();
    
    // Create some temporary test files
    for (int i = 0; i < 5; ++i) {
        QTemporaryFile* tempFile = new QTemporaryFile();
        tempFile->setAutoRemove(false);
        if (tempFile->open()) {
            m_tempFiles.append(tempFile->fileName());
        }
        delete tempFile;
    }
}

void TestRecentFilesManager::cleanupTestCase()
{
    m_manager->clearAll();
    delete m_manager;
    
    // Clean up temp files
    for (const QString& file : m_tempFiles) {
        QFile::remove(file);
    }
}

void TestRecentFilesManager::testManagerCreation()
{
    QVERIFY(m_manager != nullptr);
}

void TestRecentFilesManager::testAddFile()
{
    m_manager->clearAll();
    
    if (!m_tempFiles.isEmpty()) {
        m_manager->addFile(m_tempFiles[0]);
        QVERIFY(m_manager->recentFiles().contains(m_tempFiles[0]));
    }
}

void TestRecentFilesManager::testRemoveFile()
{
    m_manager->clearAll();
    
    if (!m_tempFiles.isEmpty()) {
        m_manager->addFile(m_tempFiles[0]);
        m_manager->removeFile(m_tempFiles[0]);
        QVERIFY(!m_manager->contains(m_tempFiles[0]));
    }
}

void TestRecentFilesManager::testClearAll()
{
    if (m_tempFiles.size() >= 2) {
        m_manager->addFile(m_tempFiles[0]);
        m_manager->addFile(m_tempFiles[1]);
        m_manager->clearAll();
        QCOMPARE(m_manager->recentFiles().size(), 0);
    }
}

void TestRecentFilesManager::testMaxFiles()
{
    m_manager->clearAll();
    m_manager->setMaxFiles(3);
    QCOMPARE(m_manager->maxFiles(), 3);
    
    // Add more files than max
    for (int i = 0; i < qMin(5, m_tempFiles.size()); ++i) {
        m_manager->addFile(m_tempFiles[i]);
    }
    
    // Should only keep 3
    QVERIFY(m_manager->recentFiles().size() <= 3);
}

void TestRecentFilesManager::testContains()
{
    m_manager->clearAll();
    
    if (!m_tempFiles.isEmpty()) {
        m_manager->addFile(m_tempFiles[0]);
        QVERIFY(m_manager->contains(m_tempFiles[0]));
        QVERIFY(!m_manager->contains("/nonexistent/file.txt"));
    }
}

QTEST_MAIN(TestRecentFilesManager)
#include "test_recentfilesmanager.moc"
