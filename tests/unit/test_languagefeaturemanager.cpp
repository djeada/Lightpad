#include "language/languagefeaturemanager.h"
#include "diagnostics/diagnosticutils.h"
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
  QSignalSpy spy(&mgr, &LanguageFeatureManager::serverError);

  mgr.openDocument("/project/file.xyz", "unknown_lang", "content");

  QCOMPARE(spy.count(), 1);
  const QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), QString("unknown_lang"));
  QVERIFY(args.at(1).toString().contains("No language server configured"));
  QVERIFY(mgr.clientForFile("/project/file.xyz") == nullptr);
}

void TestLanguageFeatureManager::testDiagnosticsManagerIntegration() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);
  const QString filePath = "/project/main.cpp";
  const QString uri = DiagnosticUtils::filePathToUri(filePath);

  QCOMPARE(diagMgr.errorCount(), 0);
  QCOMPARE(diagMgr.warningCount(), 0);
  QCOMPARE(diagMgr.infoCount(), 0);

  mgr.openDocument(filePath, "cpp", "int main() { return 0; }\n");
  QCOMPARE(diagMgr.documentVersion(uri), 1);
  QVERIFY(mgr.clientForFile(filePath) != nullptr);

  LspDiagnostic diagnostic;
  diagnostic.range.start = {0, 0};
  diagnostic.range.end = {0, 3};
  diagnostic.severity = LspDiagnosticSeverity::Error;
  diagnostic.message = "integration test error";

  diagMgr.upsertDiagnostics(uri, {diagnostic}, "lsp:cpp", 1);

  QCOMPARE(diagMgr.errorCount(), 1);
  QCOMPARE(diagMgr.diagnosticsForFile(filePath).size(), 1);

  mgr.closeDocument(filePath);

  QCOMPARE(diagMgr.errorCount(), 0);
  QVERIFY(diagMgr.diagnosticsForFile(filePath).isEmpty());
}

QTEST_MAIN(TestLanguageFeatureManager)
#include "test_languagefeaturemanager.moc"
