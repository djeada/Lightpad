#include "settings/settingsmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

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
  void testCorruptSettingsAreBackedUp();
  void testFontWeightMigratesFromQt5Scale();

private:
  QTemporaryDir m_tempDir;
  QByteArray m_previousXdgConfigHome;
  bool m_hadXdgConfigHome = false;
};

void TestSettingsManager::initTestCase() {
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

void TestSettingsManager::cleanupTestCase() {
  if (m_hadXdgConfigHome) {
    qputenv("XDG_CONFIG_HOME", m_previousXdgConfigHome);
  } else {
    qunsetenv("XDG_CONFIG_HOME");
  }
}

void TestSettingsManager::testSingletonInstance() {
  SettingsManager &sm1 = SettingsManager::instance();
  SettingsManager &sm2 = SettingsManager::instance();
  QCOMPARE(&sm1, &sm2);
}

void TestSettingsManager::testGetSettingsDirectory() {
  SettingsManager &sm = SettingsManager::instance();
  QString dir = sm.getSettingsDirectory();

  QVERIFY(!dir.isEmpty());

  QVERIFY(dir.contains("config") ||
          dir.contains("lightpad", Qt::CaseInsensitive) ||
          dir.contains("Lightpad", Qt::CaseInsensitive));
}

void TestSettingsManager::testDefaultValues() {
  SettingsManager &sm = SettingsManager::instance();
  sm.resetToDefaults();

  QCOMPARE(sm.getValue("tabWidth", 0).toInt(), 4);
  QCOMPARE(sm.getValue("autoIndent", false).toBool(), true);
  QCOMPARE(sm.getValue("showLineNumberArea", false).toBool(), true);
  QCOMPARE(sm.getValue("showSourceControlDock", false).toBool(), true);
  QCOMPARE(sm.getValue("showDebugDock", true).toBool(), false);
  QCOMPARE(sm.getValue("showTerminalDock", true).toBool(), false);
  QCOMPARE(sm.getValue("showFindReplacePanel", true).toBool(), false);
}

void TestSettingsManager::testSetGetValue() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("testKey", "testValue");
  QCOMPARE(sm.getValue("testKey").toString(), QString("testValue"));

  sm.setValue("testInt", 42);
  QCOMPARE(sm.getValue("testInt").toInt(), 42);

  sm.setValue("testBool", true);
  QCOMPARE(sm.getValue("testBool").toBool(), true);
}

void TestSettingsManager::testNestedKeys() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("nested.level1", "value1");
  QCOMPARE(sm.getValue("nested.level1").toString(), QString("value1"));

  sm.setValue("nested.level1.level2", "value2");
  QCOMPARE(sm.getValue("nested.level1.level2").toString(), QString("value2"));

  QVERIFY(sm.hasKey("nested.level1"));
  QVariant parentValue = sm.getValue("nested.level1");
  QVERIFY(parentValue.canConvert<QVariantMap>());
  QVariantMap parentMap = parentValue.toMap();
  QVERIFY(parentMap.contains("level2"));
  QCOMPARE(parentMap.value("level2").toString(), QString("value2"));

  sm.setValue("config.option1", 123);
  sm.setValue("config.option2", "text");
  QCOMPARE(sm.getValue("config.option1").toInt(), 123);
  QCOMPARE(sm.getValue("config.option2").toString(), QString("text"));

  sm.setValue("deep.nested.structure.value", true);
  QCOMPARE(sm.getValue("deep.nested.structure.value").toBool(), true);
}

void TestSettingsManager::testHasKey() {
  SettingsManager &sm = SettingsManager::instance();
  sm.resetToDefaults();

  QVERIFY(sm.hasKey("tabWidth"));
  QVERIFY(sm.hasKey("autoIndent"));
  QVERIFY(!sm.hasKey("nonExistentKey"));
}

void TestSettingsManager::testResetToDefaults() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("tabWidth", 8);
  QCOMPARE(sm.getValue("tabWidth").toInt(), 8);

  sm.resetToDefaults();
  QCOMPARE(sm.getValue("tabWidth").toInt(), 4);
}

void TestSettingsManager::testLoadSaveSettings() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("testSaveLoad", "unique_value_123");
  sm.setValue("currentFilePath", "/tmp/example.cpp");
  sm.setValue("showTerminalDock", true);

  QVERIFY(sm.saveSettings());

  sm.resetToDefaults();
  QVERIFY(sm.loadSettings());

  QCOMPARE(sm.getValue("testSaveLoad").toString(), QString("unique_value_123"));
  QCOMPARE(sm.getValue("currentFilePath").toString(),
           QString("/tmp/example.cpp"));
  QCOMPARE(sm.getValue("showTerminalDock").toBool(), true);

  sm.resetToDefaults();
  sm.saveSettings();
}

void TestSettingsManager::testCorruptSettingsAreBackedUp() {
  SettingsManager &sm = SettingsManager::instance();
  QVERIFY(QDir().mkpath(sm.getSettingsDirectory()));
  const QString path = sm.getSettingsFilePath();
  const QByteArray corrupt = "{ \"fontFamily\": \"Precious\", ";

  QFile::remove(path + ".bak");
  {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(corrupt);
  }

  QVERIFY(!sm.loadSettings());
  QVERIFY(QFile::exists(path + ".bak"));
  QFile backup(path + ".bak");
  QVERIFY(backup.open(QIODevice::ReadOnly));
  QCOMPARE(backup.readAll(), corrupt);

  QVERIFY(sm.saveSettings());
  QVERIFY(sm.loadSettings());

  QFile::remove(path + ".bak");
  sm.resetToDefaults();
  sm.saveSettings();
}

void TestSettingsManager::testFontWeightMigratesFromQt5Scale() {
  QCOMPARE(SettingsManager::normalizeFontWeight(50),
           static_cast<int>(QFont::Normal));
  QCOMPARE(SettingsManager::normalizeFontWeight(75),
           static_cast<int>(QFont::Bold));
  QCOMPARE(SettingsManager::normalizeFontWeight(25),
           static_cast<int>(QFont::Light));
  QCOMPARE(SettingsManager::normalizeFontWeight(0),
           static_cast<int>(QFont::Normal));
  QCOMPARE(SettingsManager::normalizeFontWeight(600), 600);

  SettingsManager &sm = SettingsManager::instance();
  sm.resetToDefaults();
  QCOMPARE(sm.getValue("fontWeight").toInt(), static_cast<int>(QFont::Normal));

  sm.setValue("fontWeight", 75);
  QVERIFY(sm.saveSettings());
  QVERIFY(sm.loadSettings());
  QCOMPARE(sm.getValue("fontWeight").toInt(), static_cast<int>(QFont::Bold));

  sm.resetToDefaults();
  sm.saveSettings();
}

QTEST_MAIN(TestSettingsManager)
#include "test_settingsmanager.moc"
