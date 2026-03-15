#include "language/languagefeaturemanager.h"
#include <QSignalSpy>
#include <QTest>

class TestLanguageFeatureManager : public QObject {
  Q_OBJECT

private slots:
  void testDefaultServerConfigs();
  void testSupportedLanguages();
  void testIsLanguageSupported();
  void testResolveLanguageIdByExtension();
  void testResolveLanguageIdWithOverride();
  void testResolveLanguageIdUnknown();
  void testClientForFileUnknown();
  void testOpenDocumentUnsupported();
  void testCloseDocumentWithoutOpen();
  void testServerErrorEmitted();
  void testDiagnosticsManagerIntegration();
};

void TestLanguageFeatureManager::testDefaultServerConfigs() {
  QList<DiagnosticsServerConfig> configs =
      LanguageFeatureManager::defaultServerConfigs();
  QVERIFY(configs.size() >= 2);

  bool hasCpp = false, hasPy = false;
  for (const DiagnosticsServerConfig &cfg : configs) {
    if (cfg.languageId == "cpp") {
      hasCpp = true;
      QCOMPARE(cfg.command, QString("clangd"));
    }
    if (cfg.languageId == "py") {
      hasPy = true;
      QCOMPARE(cfg.command, QString("pylsp"));
    }
  }
  QVERIFY(hasCpp);
  QVERIFY(hasPy);
}

void TestLanguageFeatureManager::testSupportedLanguages() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QStringList langs = mgr.supportedLanguages();
  QVERIFY(langs.contains("cpp"));
  QVERIFY(langs.contains("py"));
}

void TestLanguageFeatureManager::testIsLanguageSupported() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QVERIFY(mgr.isLanguageSupported("cpp"));
  QVERIFY(mgr.isLanguageSupported("py"));
  QVERIFY(!mgr.isLanguageSupported("unknown"));
  QVERIFY(!mgr.isLanguageSupported(""));
}

void TestLanguageFeatureManager::testResolveLanguageIdByExtension() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QCOMPARE(mgr.resolveLanguageId("/project/main.cpp"), QString("cpp"));
  QCOMPARE(mgr.resolveLanguageId("/project/main.cc"), QString("cpp"));
  QCOMPARE(mgr.resolveLanguageId("/project/main.h"), QString("cpp"));
  QCOMPARE(mgr.resolveLanguageId("/project/script.py"), QString("py"));
  QCOMPARE(mgr.resolveLanguageId("/project/script.pyw"), QString("py"));
}

void TestLanguageFeatureManager::testResolveLanguageIdWithOverride() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QCOMPARE(mgr.resolveLanguageId("/project/main.txt", "python"), QString("py"));
  QCOMPARE(mgr.resolveLanguageId("/project/main.txt", "cpp"), QString("cpp"));
}

void TestLanguageFeatureManager::testResolveLanguageIdUnknown() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QVERIFY(mgr.resolveLanguageId("/project/file.xyz").isEmpty());
}

void TestLanguageFeatureManager::testClientForFileUnknown() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QVERIFY(mgr.clientForFile("/project/main.cpp") == nullptr);
}

void TestLanguageFeatureManager::testOpenDocumentUnsupported() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  mgr.openDocument("/project/file.xyz", "", "content");

  QVERIFY(mgr.clientForFile("/project/file.xyz") == nullptr);
}

void TestLanguageFeatureManager::testCloseDocumentWithoutOpen() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  mgr.closeDocument("/project/main.cpp");
  QVERIFY(mgr.clientForFile("/project/main.cpp") == nullptr);
}

void TestLanguageFeatureManager::testServerErrorEmitted() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  mgr.openDocument("/project/file.xyz", "unknown_lang", "content");
  QVERIFY(mgr.clientForFile("/project/file.xyz") == nullptr);
}

void TestLanguageFeatureManager::testDiagnosticsManagerIntegration() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QCOMPARE(diagMgr.errorCount(), 0);
  QCOMPARE(diagMgr.warningCount(), 0);
  QCOMPARE(diagMgr.infoCount(), 0);
}

QTEST_MAIN(TestLanguageFeatureManager)
#include "test_languagefeaturemanager.moc"
