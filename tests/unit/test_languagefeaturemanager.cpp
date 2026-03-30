#include "language/languagefeaturemanager.h"
#include "diagnostics/diagnosticutils.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
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
  void testDetectProjectRootCargo();
  void testDetectProjectRootGoMod();
  void testDetectProjectRootCMakeLists();
  void testDetectProjectRootPyproject();
  void testDetectProjectRootFallback();
  void testServerHealthInitial();
  void testServerHealthErrorOnBadCommand();
  void testConfigEnabledField();
};

void TestLanguageFeatureManager::testDefaultServerConfigs() {
  QList<DiagnosticsServerConfig> configs =
      LanguageFeatureManager::defaultServerConfigs();
  QVERIFY(configs.size() >= 4);

  bool hasCpp = false, hasPy = false, hasRust = false, hasGo = false;
  for (const DiagnosticsServerConfig &cfg : configs) {
    if (cfg.languageId == "cpp") {
      hasCpp = true;
      QCOMPARE(cfg.command, QString("clangd"));
      QVERIFY(cfg.arguments.contains("--background-index"));
    }
    if (cfg.languageId == "py") {
      hasPy = true;
      QCOMPARE(cfg.command, QString("pylsp"));
    }
    if (cfg.languageId == "rust") {
      hasRust = true;
      QCOMPARE(cfg.command, QString("rust-analyzer"));
    }
    if (cfg.languageId == "go") {
      hasGo = true;
      QCOMPARE(cfg.command, QString("gopls"));
      QVERIFY(cfg.arguments.contains("serve"));
    }
  }
  QVERIFY(hasCpp);
  QVERIFY(hasPy);
  QVERIFY(hasRust);
  QVERIFY(hasGo);
}

void TestLanguageFeatureManager::testSupportedLanguages() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QStringList langs = mgr.supportedLanguages();
  QVERIFY(langs.contains("cpp"));
  QVERIFY(langs.contains("py"));
  QVERIFY(langs.contains("rust"));
  QVERIFY(langs.contains("go"));
}

void TestLanguageFeatureManager::testIsLanguageSupported() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QVERIFY(mgr.isLanguageSupported("cpp"));
  QVERIFY(mgr.isLanguageSupported("py"));
  QVERIFY(mgr.isLanguageSupported("rust"));
  QVERIFY(mgr.isLanguageSupported("go"));
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
  QCOMPARE(mgr.resolveLanguageId("/project/main.rs"), QString("rust"));
  QCOMPARE(mgr.resolveLanguageId("/project/main.go"), QString("go"));
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

void TestLanguageFeatureManager::testDetectProjectRootCargo() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  QDir root(tmpDir.path());
  root.mkpath("src");

  QFile cargoToml(root.filePath("Cargo.toml"));
  QVERIFY(cargoToml.open(QIODevice::WriteOnly));
  cargoToml.write("[package]\nname = \"test\"\n");
  cargoToml.close();

  QString filePath = root.filePath("src/main.rs");
  QFile mainRs(filePath);
  QVERIFY(mainRs.open(QIODevice::WriteOnly));
  mainRs.write("fn main() {}\n");
  mainRs.close();

  QString detected = LanguageFeatureManager::detectProjectRoot(filePath);
  QCOMPARE(detected, root.absolutePath());
}

void TestLanguageFeatureManager::testDetectProjectRootGoMod() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  QDir root(tmpDir.path());
  root.mkpath("cmd");

  QFile goMod(root.filePath("go.mod"));
  QVERIFY(goMod.open(QIODevice::WriteOnly));
  goMod.write("module example.com/test\n");
  goMod.close();

  QString filePath = root.filePath("cmd/main.go");
  QFile mainGo(filePath);
  QVERIFY(mainGo.open(QIODevice::WriteOnly));
  mainGo.write("package main\n");
  mainGo.close();

  QString detected = LanguageFeatureManager::detectProjectRoot(filePath);
  QCOMPARE(detected, root.absolutePath());
}

void TestLanguageFeatureManager::testDetectProjectRootCMakeLists() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  QDir root(tmpDir.path());
  root.mkpath("src");

  QFile cmake(root.filePath("CMakeLists.txt"));
  QVERIFY(cmake.open(QIODevice::WriteOnly));
  cmake.write("cmake_minimum_required(VERSION 3.16)\n");
  cmake.close();

  QString filePath = root.filePath("src/main.cpp");
  QFile mainCpp(filePath);
  QVERIFY(mainCpp.open(QIODevice::WriteOnly));
  mainCpp.write("int main() {}\n");
  mainCpp.close();

  QString detected = LanguageFeatureManager::detectProjectRoot(filePath);
  QCOMPARE(detected, root.absolutePath());
}

void TestLanguageFeatureManager::testDetectProjectRootPyproject() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  QDir root(tmpDir.path());
  root.mkpath("src");

  QFile pyproject(root.filePath("pyproject.toml"));
  QVERIFY(pyproject.open(QIODevice::WriteOnly));
  pyproject.write("[project]\nname = \"test\"\n");
  pyproject.close();

  QString filePath = root.filePath("src/app.py");
  QFile appPy(filePath);
  QVERIFY(appPy.open(QIODevice::WriteOnly));
  appPy.write("print('hello')\n");
  appPy.close();

  QString detected = LanguageFeatureManager::detectProjectRoot(filePath);
  QCOMPARE(detected, root.absolutePath());
}

void TestLanguageFeatureManager::testDetectProjectRootFallback() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  QDir root(tmpDir.path());
  root.mkpath("subdir");

  QString filePath = root.filePath("subdir/file.txt");
  QFile file(filePath);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("content\n");
  file.close();

  QString detected = LanguageFeatureManager::detectProjectRoot(filePath);
  QCOMPARE(detected, root.filePath("subdir"));
}

void TestLanguageFeatureManager::testServerHealthInitial() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QCOMPARE(mgr.serverHealth("cpp"), ServerHealthStatus::Unknown);
  QCOMPARE(mgr.serverHealth("rust"), ServerHealthStatus::Unknown);
  QCOMPARE(mgr.serverHealth("go"), ServerHealthStatus::Unknown);
  QCOMPARE(mgr.serverHealth("py"), ServerHealthStatus::Unknown);
}

void TestLanguageFeatureManager::testServerHealthErrorOnBadCommand() {
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);

  QSignalSpy healthSpy(&mgr, &LanguageFeatureManager::serverHealthChanged);

  mgr.openDocument("/project/file.xyz", "unknown_lang", "content");

  QCOMPARE(mgr.serverHealth("unknown_lang"), ServerHealthStatus::Unknown);
}

void TestLanguageFeatureManager::testConfigEnabledField() {
  QList<DiagnosticsServerConfig> configs =
      LanguageFeatureManager::defaultServerConfigs();

  for (const DiagnosticsServerConfig &cfg : configs) {
    QVERIFY(cfg.enabled);
  }
}

QTEST_MAIN(TestLanguageFeatureManager)
#include "test_languagefeaturemanager.moc"
