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
    void testNestedKeys();
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

void TestSettingsManager::testNestedKeys()
{
    SettingsManager& sm = SettingsManager::instance();
    
    // Test setting nested keys with dot notation
    sm.setValue("nested.level1", "value1");
    QCOMPARE(sm.getValue("nested.level1").toString(), QString("value1"));
    
    // Test deeper nesting
    sm.setValue("nested.level1.level2", "value2");
    QCOMPARE(sm.getValue("nested.level1.level2").toString(), QString("value2"));
    
    // Test that the parent key still exists and returns an object
    QVERIFY(sm.hasKey("nested.level1"));
    QVariant parentValue = sm.getValue("nested.level1");
    QVERIFY(parentValue.canConvert<QVariantMap>());
    QVariantMap parentMap = parentValue.toMap();
    QVERIFY(parentMap.contains("level2"));
    QCOMPARE(parentMap.value("level2").toString(), QString("value2"));
    
    // Test setting multiple keys in the same parent
    sm.setValue("config.option1", 123);
    sm.setValue("config.option2", "text");
    QCOMPARE(sm.getValue("config.option1").toInt(), 123);
    QCOMPARE(sm.getValue("config.option2").toString(), QString("text"));
    
    // Test deeply nested structure
    sm.setValue("deep.nested.structure.value", true);
    QCOMPARE(sm.getValue("deep.nested.structure.value").toBool(), true);
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
