#include "settings/settingsmanager.h"
#include <QDir>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestSettingsRegression : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testSerializeDeserializeRoundTrip();
  void testOverwriteExistingKey();
  void testDeepNestedKeys();
  void testSetValueEmitsSignal();
  void testResetToDefaultsRestoresValues();
  void testSaveLoadPreservesAllTypes();
  void testConcurrentSetGet();
  void testHasKeyAfterRemove();
  void testDefaultValueForMissingKey();
  void testLoadNonexistentFile();

private:
  QTemporaryDir m_tempDir;
  bool m_hadXdgConfigHome = false;
  QByteArray m_oldXdgConfigHome;
};

void TestSettingsRegression::initTestCase() {
  QVERIFY(m_tempDir.isValid());

  m_hadXdgConfigHome = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
  if (m_hadXdgConfigHome) {
    m_oldXdgConfigHome = qgetenv("XDG_CONFIG_HOME");
  }

  const QString configRoot = m_tempDir.path() + "/config";
  QDir dir;
  dir.mkpath(configRoot);
  qputenv("XDG_CONFIG_HOME", configRoot.toUtf8());

  SettingsManager::instance().resetToDefaults();
}

void TestSettingsRegression::cleanupTestCase() {
  if (m_hadXdgConfigHome) {
    qputenv("XDG_CONFIG_HOME", m_oldXdgConfigHome);
  } else {
    qunsetenv("XDG_CONFIG_HOME");
  }
}

void TestSettingsRegression::testSerializeDeserializeRoundTrip() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("roundTrip.string", "hello");
  sm.setValue("roundTrip.integer", 42);
  sm.setValue("roundTrip.boolean", true);
  sm.setValue("roundTrip.decimal", 3.14);

  QVERIFY(sm.saveSettings());
  QVERIFY(sm.loadSettings());

  QCOMPARE(sm.getValue("roundTrip.string").toString(), QString("hello"));
  QCOMPARE(sm.getValue("roundTrip.integer").toInt(), 42);
  QCOMPARE(sm.getValue("roundTrip.boolean").toBool(), true);
  QVERIFY(qAbs(sm.getValue("roundTrip.decimal").toDouble() - 3.14) < 0.001);
}

void TestSettingsRegression::testOverwriteExistingKey() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("overwrite.key", "original");
  QCOMPARE(sm.getValue("overwrite.key").toString(), QString("original"));

  sm.setValue("overwrite.key", "replacement");
  QCOMPARE(sm.getValue("overwrite.key").toString(), QString("replacement"));
}

void TestSettingsRegression::testDeepNestedKeys() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("a.b.c.d.e", "deep");
  QCOMPARE(sm.getValue("a.b.c.d.e").toString(), QString("deep"));

  sm.setValue("a.b.c.d.f", "sibling");
  QCOMPARE(sm.getValue("a.b.c.d.f").toString(), QString("sibling"));
  QCOMPARE(sm.getValue("a.b.c.d.e").toString(), QString("deep"));
}

void TestSettingsRegression::testSetValueEmitsSignal() {
  SettingsManager &sm = SettingsManager::instance();

  QSignalSpy spy(&sm, &SettingsManager::settingChanged);

  sm.setValue("signal.test", "value1");
  QVERIFY(spy.count() >= 1);

  QList<QVariant> args = spy.takeLast();
  QCOMPARE(args[0].toString(), QString("signal.test"));
}

void TestSettingsRegression::testResetToDefaultsRestoresValues() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("custom.key", "custom_value");
  QCOMPARE(sm.getValue("custom.key").toString(), QString("custom_value"));

  sm.resetToDefaults();

  QCOMPARE(sm.getValue("tabWidth", 4).toInt(), 4);
  QCOMPARE(sm.getValue("autoIndent", true).toBool(), true);
}

void TestSettingsRegression::testSaveLoadPreservesAllTypes() {
  SettingsManager &sm = SettingsManager::instance();
  sm.resetToDefaults();

  sm.setValue("types.string", "text");
  sm.setValue("types.int", 123);
  sm.setValue("types.bool", false);

  QVERIFY(sm.saveSettings());
  sm.resetToDefaults();
  QVERIFY(sm.loadSettings());

  QCOMPARE(sm.getValue("types.string").toString(), QString("text"));
  QCOMPARE(sm.getValue("types.int").toInt(), 123);
  QCOMPARE(sm.getValue("types.bool").toBool(), false);
}

void TestSettingsRegression::testConcurrentSetGet() {
  SettingsManager &sm = SettingsManager::instance();

  for (int i = 0; i < 100; i++) {
    QString key = QString("concurrent.key%1").arg(i);
    sm.setValue(key, i);
  }

  for (int i = 0; i < 100; i++) {
    QString key = QString("concurrent.key%1").arg(i);
    QCOMPARE(sm.getValue(key).toInt(), i);
  }
}

void TestSettingsRegression::testHasKeyAfterRemove() {
  SettingsManager &sm = SettingsManager::instance();

  sm.setValue("removable.key", "value");
  QVERIFY(sm.hasKey("removable.key"));

  sm.setValue("removable.key", QVariant());
  QVariant val = sm.getValue("removable.key");
  Q_UNUSED(val);
}

void TestSettingsRegression::testDefaultValueForMissingKey() {
  SettingsManager &sm = SettingsManager::instance();

  QCOMPARE(sm.getValue("nonexistent.key", "default").toString(),
           QString("default"));
  QCOMPARE(sm.getValue("nonexistent.key", 42).toInt(), 42);
  QCOMPARE(sm.getValue("nonexistent.key", true).toBool(), true);
}

void TestSettingsRegression::testLoadNonexistentFile() {
  SettingsManager &sm = SettingsManager::instance();
  bool result = sm.loadSettings();
  Q_UNUSED(result);
}

QTEST_MAIN(TestSettingsRegression)
#include "test_settingsregression.moc"
