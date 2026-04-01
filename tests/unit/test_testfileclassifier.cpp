#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "test_templates/testfileclassifier.h"

class TestTestFileClassifier : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void autoDetectTestFile_prefix();
  void autoDetectTestFile_suffix();
  void autoDetectTestFile_directory();
  void autoDetectTestFile_negative();
  void autoDetectTestDirectory_true();
  void autoDetectTestDirectory_false();
  void userOverride_markAsTest();
  void userOverride_unmarkAsTest();
  void clearOverride();
  void persistenceRoundTrip();
  void overrideChangedSignal();

private:
  QTemporaryDir *m_tmpDir = nullptr;
};

void TestTestFileClassifier::init() {
  m_tmpDir = new QTemporaryDir();
  QVERIFY(m_tmpDir->isValid());
  auto &c = TestFileClassifier::instance();
  c.setWorkspaceFolder(m_tmpDir->path());
  // Clear any leftover state
  c.clearOverride("/some/test_foo.py");
  c.clearOverride("/some/foo.py");
  c.clearOverride("/app/src/main.cpp");
}

void TestTestFileClassifier::cleanup() {
  delete m_tmpDir;
  m_tmpDir = nullptr;
}

void TestTestFileClassifier::autoDetectTestFile_prefix() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(c.isTestFile("/some/test_foo.py"));
  QVERIFY(c.isTestFile("/some/test-bar.js"));
  QVERIFY(c.isTestFile("/some/tests_util.cpp"));
  QVERIFY(c.isTestFile("/some/tests-helper.ts"));
}

void TestTestFileClassifier::autoDetectTestFile_suffix() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(c.isTestFile("/src/foo_test.py"));
  QVERIFY(c.isTestFile("/src/bar-test.js"));
  QVERIFY(c.isTestFile("/src/baz_spec.rb"));
  QVERIFY(c.isTestFile("/src/qux.test.ts"));
  QVERIFY(c.isTestFile("/src/abc.spec.js"));
}

void TestTestFileClassifier::autoDetectTestFile_directory() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(c.isTestFile("/project/tests/foo.py"));
  QVERIFY(c.isTestFile("/project/__tests__/bar.js"));
  QVERIFY(c.isTestFile("/project/spec/baz.rb"));
}

void TestTestFileClassifier::autoDetectTestFile_negative() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(!c.isTestFile("/app/src/main.cpp"));
  QVERIFY(!c.isTestFile("/app/src/utils.py"));
  QVERIFY(!c.isTestFile("/app/lib/helper.js"));
}

void TestTestFileClassifier::autoDetectTestDirectory_true() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(c.isTestDirectory("/project/tests"));
  QVERIFY(c.isTestDirectory("/project/test"));
  QVERIFY(c.isTestDirectory("/project/__tests__"));
  QVERIFY(c.isTestDirectory("/project/spec"));
}

void TestTestFileClassifier::autoDetectTestDirectory_false() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(!c.isTestDirectory("/project/src"));
  QVERIFY(!c.isTestDirectory("/project/lib"));
  QVERIFY(!c.isTestDirectory("/project/app"));
}

void TestTestFileClassifier::userOverride_markAsTest() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(!c.isTestFile("/app/src/main.cpp"));
  c.setTestOverride("/app/src/main.cpp", true);
  QVERIFY(c.isTestFile("/app/src/main.cpp"));
  QVERIFY(c.hasUserOverride("/app/src/main.cpp"));
  c.clearOverride("/app/src/main.cpp");
}

void TestTestFileClassifier::userOverride_unmarkAsTest() {
  auto &c = TestFileClassifier::instance();
  QVERIFY(c.isTestFile("/some/test_foo.py"));
  c.setTestOverride("/some/test_foo.py", false);
  QVERIFY(!c.isTestFile("/some/test_foo.py"));
  c.clearOverride("/some/test_foo.py");
}

void TestTestFileClassifier::clearOverride() {
  auto &c = TestFileClassifier::instance();
  c.setTestOverride("/some/foo.py", true);
  QVERIFY(c.hasUserOverride("/some/foo.py"));
  c.clearOverride("/some/foo.py");
  QVERIFY(!c.hasUserOverride("/some/foo.py"));
}

void TestTestFileClassifier::persistenceRoundTrip() {
  auto &c = TestFileClassifier::instance();
  c.setTestOverride("/app/src/main.cpp", true);

  // Reload overrides from disk
  c.loadOverrides();
  QVERIFY(c.isTestFile("/app/src/main.cpp"));
  QVERIFY(c.hasUserOverride("/app/src/main.cpp"));
  c.clearOverride("/app/src/main.cpp");
}

void TestTestFileClassifier::overrideChangedSignal() {
  auto &c = TestFileClassifier::instance();
  QSignalSpy spy(&c,
                 &TestFileClassifier::overrideChanged);
  QVERIFY(spy.isValid());

  c.setTestOverride("/some/foo.py", true);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), QString("/some/foo.py"));
  QCOMPARE(spy.at(0).at(1).toBool(), true);
  c.clearOverride("/some/foo.py");
}

QTEST_MAIN(TestTestFileClassifier)
#include "test_testfileclassifier.moc"
