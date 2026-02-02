#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QStandardPaths>
#include "settings/settingsmanager.h"

class TestSettingsManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testSingletonInstance();
    void testGetSettingsDirectory();
    void testDefaultValues();
    void testSetGetValue();
    void testHasKey();
    void testResetToDefaults();
    void testLoadSaveSettings();
private:
    QTemporaryDir m_tempDir;
    QByteArray m_previousXdgConfigHome;
    bool m_hadXdgConfigHome = false;
};

void TestSettingsManager::initTestCase()
{
    QVERIFY(m_tempDir.isValid());

    m_hadXdgConfigHome = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
    if (m_hadXdgConfigHome) {
        m_previousXdgConfigHome = qgetenv("XDG_CONFIG_HOME");
    }

    const QString configRoot = m_tempDir.path() + "/config";
    QDir dir;
    QVERIFY(dir.mkpath(configRoot));
    qputenv("XDG_CONFIG_HOME", configRoot.toUtf8());

    QCoreApplication::setOrganizationName("lightpad");
    QCoreApplication::setApplicationName("lightpad");
}

void TestSettingsManager::cleanupTestCase()
{
    if (m_hadXdgConfigHome) {
        qputenv("XDG_CONFIG_HOME", m_previousXdgConfigHome);
    } else {
        qunsetenv("XDG_CONFIG_HOME");
    }
}

void TestSettingsManager::testSingletonInstance()
{
    SettingsManager& sm1 = SettingsManager::instance();
    SettingsManager& sm2 = SettingsManager::instance();
    QCOMPARE(&sm1, &sm2);
}

void TestSettingsManager::testGetSettingsDirectory()
{
    SettingsManager& sm = SettingsManager::instance();
    QString dir = sm.getSettingsDirectory();
    
    // Should not be empty
    QVERIFY(!dir.isEmpty());
    
    // Should contain app name or config path
    QVERIFY(dir.contains("config") || dir.contains("lightpad", Qt::CaseInsensitive) 
            || dir.contains("Lightpad", Qt::CaseInsensitive));
}

void TestSettingsManager::testDefaultValues()
{
    SettingsManager& sm = SettingsManager::instance();
    sm.resetToDefaults();
    
    // Check some default values
    QCOMPARE(sm.getValue("tabWidth", 0).toInt(), 4);
    QCOMPARE(sm.getValue("autoIndent", false).toBool(), true);
    QCOMPARE(sm.getValue("showLineNumberArea", false).toBool(), true);
}

void TestSettingsManager::testSetGetValue()
{
    SettingsManager& sm = SettingsManager::instance();
    
    sm.setValue("testKey", "testValue");
    QCOMPARE(sm.getValue("testKey").toString(), QString("testValue"));
    
    sm.setValue("testInt", 42);
    QCOMPARE(sm.getValue("testInt").toInt(), 42);
    
    sm.setValue("testBool", true);
    QCOMPARE(sm.getValue("testBool").toBool(), true);
}

void TestSettingsManager::testHasKey()
{
    SettingsManager& sm = SettingsManager::instance();
    sm.resetToDefaults();
    
    QVERIFY(sm.hasKey("tabWidth"));
    QVERIFY(sm.hasKey("autoIndent"));
    QVERIFY(!sm.hasKey("nonExistentKey"));
}

void TestSettingsManager::testResetToDefaults()
{
    SettingsManager& sm = SettingsManager::instance();
    
    // Change a value
    sm.setValue("tabWidth", 8);
    QCOMPARE(sm.getValue("tabWidth").toInt(), 8);
    
    // Reset to defaults
    sm.resetToDefaults();
    QCOMPARE(sm.getValue("tabWidth").toInt(), 4);
}

void TestSettingsManager::testLoadSaveSettings()
{
    SettingsManager& sm = SettingsManager::instance();
    
    // Set a unique value
    sm.setValue("testSaveLoad", "unique_value_123");
    
    // Save
    QVERIFY(sm.saveSettings());
    
    // Reset and reload
    sm.resetToDefaults();
    QVERIFY(sm.loadSettings());
    
    // Value should be restored
    QCOMPARE(sm.getValue("testSaveLoad").toString(), QString("unique_value_123"));
    
    // Clean up: reset to defaults and save
    sm.resetToDefaults();
    sm.saveSettings();
}

QTEST_MAIN(TestSettingsManager)
#include "test_settingsmanager.moc"
