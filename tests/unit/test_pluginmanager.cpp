#include <QtTest/QtTest>
#include "plugins/pluginmanager.h"

class TestPluginManager : public QObject {
    Q_OBJECT

private slots:
    void testSingletonInstance();
    void testPluginDirectories();
    void testAddPluginDirectory();
    void testDiscoverPlugins();
    void testLoadNonexistentPlugin();
};

void TestPluginManager::testSingletonInstance()
{
    PluginManager& pm1 = PluginManager::instance();
    PluginManager& pm2 = PluginManager::instance();
    QCOMPARE(&pm1, &pm2);
}

void TestPluginManager::testPluginDirectories()
{
    PluginManager& pm = PluginManager::instance();
    QStringList dirs = pm.pluginDirectories();
    
    // Should have default directories
    QVERIFY(!dirs.isEmpty());
}

void TestPluginManager::testAddPluginDirectory()
{
    PluginManager& pm = PluginManager::instance();
    
    int initialCount = pm.pluginDirectories().size();
    pm.addPluginDirectory("/test/plugins/path");
    
    QCOMPARE(pm.pluginDirectories().size(), initialCount + 1);
    QVERIFY(pm.pluginDirectories().contains("/test/plugins/path"));
}

void TestPluginManager::testDiscoverPlugins()
{
    PluginManager& pm = PluginManager::instance();
    
    // Should not crash, may return empty list
    QStringList plugins = pm.discoverPlugins();
    // Just verify it returns something (even if empty)
    QVERIFY(plugins.size() >= 0);
}

void TestPluginManager::testLoadNonexistentPlugin()
{
    PluginManager& pm = PluginManager::instance();
    
    bool result = pm.loadPlugin("/nonexistent/plugin.so");
    QVERIFY(!result);
}

QTEST_MAIN(TestPluginManager)
#include "test_pluginmanager.moc"
