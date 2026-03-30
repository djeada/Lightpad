#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "run_templates/runconfiguration.h"

class TestRunConfiguration : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testDefaultState();
  void testIsValid();
  void testToFromJson();
  void testToFromJsonWithEnv();
  void testManagerAddConfiguration();
  void testManagerRemoveConfiguration();
  void testManagerConfigurationsOfType();
  void testManagerConfigurationByName();
  void testManagerConfigurationById();
  void testManagerSaveAndLoad();
  void testManagerSaveCreatesDir();
  void testManagerLoadMissingFile();

private:
  QTemporaryDir m_tempDir;
};

void TestRunConfiguration::initTestCase() { QVERIFY(m_tempDir.isValid()); }

void TestRunConfiguration::cleanupTestCase() {}

void TestRunConfiguration::testDefaultState() {
  RunConfiguration cfg;
  QVERIFY(cfg.id.isEmpty());
  QVERIFY(cfg.name.isEmpty());
  QCOMPARE(cfg.type, RunConfigurationType::Run);
  QVERIFY(cfg.command.isEmpty());
  QVERIFY(cfg.args.isEmpty());
  QVERIFY(cfg.workingDirectory.isEmpty());
  QVERIFY(cfg.env.isEmpty());
  QVERIFY(!cfg.isValid());
}

void TestRunConfiguration::testIsValid() {
  RunConfiguration cfg;
  QVERIFY(!cfg.isValid());

  cfg.name = "test";
  QVERIFY(!cfg.isValid());

  cfg.command = "pytest";
  QVERIFY(cfg.isValid());
}

void TestRunConfiguration::testToFromJson() {
  RunConfiguration cfg;
  cfg.id = "test-id";
  cfg.name = "My Build";
  cfg.type = RunConfigurationType::Build;
  cfg.command = "cmake";
  cfg.args = {"--build", "build"};
  cfg.workingDirectory = "/home/user/project";

  QJsonObject json = cfg.toJson();
  RunConfiguration loaded = RunConfiguration::fromJson(json);

  QCOMPARE(loaded.id, "test-id");
  QCOMPARE(loaded.name, "My Build");
  QCOMPARE(loaded.type, RunConfigurationType::Build);
  QCOMPARE(loaded.command, "cmake");
  QCOMPARE(loaded.args, (QStringList{"--build", "build"}));
  QCOMPARE(loaded.workingDirectory, "/home/user/project");
}

void TestRunConfiguration::testToFromJsonWithEnv() {
  RunConfiguration cfg;
  cfg.name = "Test";
  cfg.command = "pytest";
  cfg.type = RunConfigurationType::Test;
  cfg.env["PYTHONPATH"] = "/lib";
  cfg.env["DEBUG"] = "1";

  QJsonObject json = cfg.toJson();
  RunConfiguration loaded = RunConfiguration::fromJson(json);

  QCOMPARE(loaded.env.size(), 2);
  QCOMPARE(loaded.env["PYTHONPATH"], "/lib");
  QCOMPARE(loaded.env["DEBUG"], "1");
  QCOMPARE(loaded.type, RunConfigurationType::Test);
}

void TestRunConfiguration::testManagerAddConfiguration() {
  RunConfigurationManager &manager = RunConfigurationManager::instance();

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }

  QSignalSpy spy(&manager, &RunConfigurationManager::configurationsChanged);

  RunConfiguration cfg;
  cfg.name = "build";
  cfg.command = "make";
  cfg.type = RunConfigurationType::Build;
  manager.addConfiguration(cfg);

  QCOMPARE(spy.count(), 1);
  QCOMPARE(manager.allConfigurations().size(), 1);
  QVERIFY(!manager.allConfigurations().first().id.isEmpty());

  manager.removeConfiguration(manager.allConfigurations().first().id);
}

void TestRunConfiguration::testManagerRemoveConfiguration() {
  RunConfigurationManager &manager = RunConfigurationManager::instance();

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }

  RunConfiguration cfg;
  cfg.name = "test";
  cfg.command = "pytest";
  manager.addConfiguration(cfg);

  QString id = manager.allConfigurations().first().id;
  QCOMPARE(manager.allConfigurations().size(), 1);

  manager.removeConfiguration(id);
  QCOMPARE(manager.allConfigurations().size(), 0);
}

void TestRunConfiguration::testManagerConfigurationsOfType() {
  RunConfigurationManager &manager = RunConfigurationManager::instance();

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }

  RunConfiguration build;
  build.name = "build";
  build.command = "make";
  build.type = RunConfigurationType::Build;
  manager.addConfiguration(build);

  RunConfiguration test;
  test.name = "test";
  test.command = "pytest";
  test.type = RunConfigurationType::Test;
  manager.addConfiguration(test);

  RunConfiguration run;
  run.name = "run";
  run.command = "./app";
  run.type = RunConfigurationType::Run;
  manager.addConfiguration(run);

  QCOMPARE(manager.configurationsOfType(RunConfigurationType::Build).size(), 1);
  QCOMPARE(manager.configurationsOfType(RunConfigurationType::Test).size(), 1);
  QCOMPARE(manager.configurationsOfType(RunConfigurationType::Run).size(), 1);

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }
}

void TestRunConfiguration::testManagerConfigurationByName() {
  RunConfigurationManager &manager = RunConfigurationManager::instance();

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }

  RunConfiguration cfg;
  cfg.name = "my-test";
  cfg.command = "pytest";
  manager.addConfiguration(cfg);

  RunConfiguration found = manager.configurationByName("my-test");
  QVERIFY(found.isValid());
  QCOMPARE(found.command, "pytest");

  RunConfiguration notFound = manager.configurationByName("nonexistent");
  QVERIFY(!notFound.isValid());

  manager.removeConfiguration(found.id);
}

void TestRunConfiguration::testManagerConfigurationById() {
  RunConfigurationManager &manager = RunConfigurationManager::instance();

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }

  RunConfiguration cfg;
  cfg.name = "find-by-id";
  cfg.command = "cargo test";
  manager.addConfiguration(cfg);

  QString id = manager.allConfigurations().first().id;
  RunConfiguration found = manager.configurationById(id);
  QVERIFY(found.isValid());
  QCOMPARE(found.name, "find-by-id");

  manager.removeConfiguration(id);
}

void TestRunConfiguration::testManagerSaveAndLoad() {
  RunConfigurationManager &manager = RunConfigurationManager::instance();

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }

  RunConfiguration cfg1;
  cfg1.name = "build";
  cfg1.command = "make";
  cfg1.type = RunConfigurationType::Build;
  manager.addConfiguration(cfg1);

  RunConfiguration cfg2;
  cfg2.name = "test";
  cfg2.command = "pytest";
  cfg2.type = RunConfigurationType::Test;
  cfg2.env["PYTHONPATH"] = "/lib";
  manager.addConfiguration(cfg2);

  QVERIFY(manager.saveConfigurations(m_tempDir.path()));

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }
  QCOMPARE(manager.allConfigurations().size(), 0);

  QVERIFY(manager.loadConfigurations(m_tempDir.path()));
  QCOMPARE(manager.allConfigurations().size(), 2);

  RunConfiguration loaded = manager.configurationByName("test");
  QVERIFY(loaded.isValid());
  QCOMPARE(loaded.command, "pytest");
  QCOMPARE(loaded.env["PYTHONPATH"], "/lib");

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }
}

void TestRunConfiguration::testManagerSaveCreatesDir() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  RunConfigurationManager &manager = RunConfigurationManager::instance();

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }

  RunConfiguration cfg;
  cfg.name = "test";
  cfg.command = "go test";
  manager.addConfiguration(cfg);

  QDir dir(tempDir.path());
  QVERIFY(!dir.exists(".lightpad"));

  QVERIFY(manager.saveConfigurations(tempDir.path()));
  QVERIFY(dir.exists(".lightpad"));

  while (!manager.allConfigurations().isEmpty()) {
    manager.removeConfiguration(manager.allConfigurations().first().id);
  }
}

void TestRunConfiguration::testManagerLoadMissingFile() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  RunConfigurationManager &manager = RunConfigurationManager::instance();
  QVERIFY(!manager.loadConfigurations(tempDir.path()));
}

QTEST_MAIN(TestRunConfiguration)
#include "test_runconfiguration.moc"
