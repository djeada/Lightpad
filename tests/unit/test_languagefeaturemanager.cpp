#include "definition/languagelspdefinitionprovider.h"
#include "diagnostics/diagnosticutils.h"
#include "language/languagefeaturemanager.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestLanguageFeatureManager : public QObject {
  Q_OBJECT

private slots:
  void testDefaultServerConfigs();
  void testNodeLanguageIds_data();
  void testNodeLanguageIds();
  void testDetectAdditionalProjectRoots_data();
  void testDetectAdditionalProjectRoots();
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
  void testUriEncodingRoundTrip();
  void testQueuedDocumentUsesLatestText();
  void testReopenClosesBeforeOpening();
  void testRestartReopensAllDocuments();
  void testCrashedServerIsReplaced();
  void testStaleDiagnosticsDropped();
  void testDiagnosticsForPathWithSpaces();
  void testProjectServerContext_data();
  void testProjectServerContext();
  void testPythonExternalLibrary();
  void testPythonExternalLibrary_data();
  void testPythonServerFallback();
  void testExternalDocumentKeepsServerContext();

private:
  QString makeLogPath();
  QStringList readLog(const QString &path) const;
  int countLines(const QString &path, const QString &prefix) const;
  void useFakeServer(LanguageFeatureManager &mgr, const QString &logPath,
                     int initDelayMs = 0);

  QTemporaryDir m_tempDir;
  int m_logCounter = 0;
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
      QCOMPARE(cfg.command, QString("basedpyright-langserver"));
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
  QVERIFY(langs.contains("js"));
  QVERIFY(langs.contains("ts"));
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
  QSignalSpy errorSpy(&mgr, &LanguageFeatureManager::serverError);
  QSignalSpy unavailableSpy(&mgr, &LanguageFeatureManager::serverUnavailable);

  mgr.openDocument("/project/file.xyz", "unknown_lang", "content");

  QCOMPARE(errorSpy.count(), 0);
  QCOMPARE(unavailableSpy.count(), 1);
  const QList<QVariant> args = unavailableSpy.takeFirst();
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

QString TestLanguageFeatureManager::makeLogPath() {
  return m_tempDir.filePath(QString("lsp-%1.log").arg(++m_logCounter));
}

QStringList TestLanguageFeatureManager::readLog(const QString &path) const {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
}

int TestLanguageFeatureManager::countLines(const QString &path,
                                           const QString &prefix) const {
  int count = 0;
  for (const QString &line : readLog(path)) {
    if (line.startsWith(prefix)) {
      ++count;
    }
  }
  return count;
}

void TestLanguageFeatureManager::useFakeServer(LanguageFeatureManager &mgr,
                                               const QString &logPath,
                                               int initDelayMs) {
  DiagnosticsServerConfig config;
  config.languageId = "cpp";
  config.command = FAKE_LSP_SERVER_PATH;
  config.arguments = {"--log", logPath, "--init-delay",
                      QString::number(initDelayMs)};
  mgr.setServerConfig(config);
}

void TestLanguageFeatureManager::testUriEncodingRoundTrip() {
  const QString path = "/tmp/dir with space/\u00fcber.cpp";
  const QString uri = DiagnosticUtils::filePathToUri(path);
  QVERIFY(!uri.contains(' '));
  QVERIFY(uri.contains("%20"));
  QCOMPARE(DiagnosticUtils::uriToFilePath(uri), path);
  QCOMPARE(DiagnosticUtils::normalizeUri(
               "file:///tmp/dir%20with%20space/%C3%BCber.cpp"),
           uri);
  QCOMPARE(DiagnosticUtils::normalizeUri("file:///tmp/dir with space/"
                                         "\u00fcber.cpp"),
           uri);
}

void TestLanguageFeatureManager::testQueuedDocumentUsesLatestText() {
  QVERIFY(m_tempDir.isValid());
  const QString logPath = makeLogPath();
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);
  useFakeServer(mgr, logPath, 400);

  const QString filePath = m_tempDir.filePath("queued.cpp");
  const QString uri = DiagnosticUtils::filePathToUri(filePath);
  mgr.openDocument(filePath, "cpp", "first");
  mgr.changeDocument(filePath, 2, "second");
  mgr.changeDocument(filePath, 3, "third");

  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "textDocument/didOpen"), 1,
                            5000);
  QVERIFY(readLog(logPath).contains(
      QString("textDocument/didOpen %1 3 third").arg(uri)));
  QCOMPARE(countLines(logPath, "textDocument/didChange"), 0);

  mgr.changeDocument(filePath, 4, "fourth");
  QTRY_VERIFY_WITH_TIMEOUT(
      readLog(logPath).contains(
          QString("textDocument/didChange %1 4 fourth").arg(uri)),
      5000);
}

void TestLanguageFeatureManager::testReopenClosesBeforeOpening() {
  QVERIFY(m_tempDir.isValid());
  const QString logPath = makeLogPath();
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);
  useFakeServer(mgr, logPath);
  QSignalSpy startedSpy(&mgr, &LanguageFeatureManager::serverStarted);

  const QString filePath = m_tempDir.filePath("reopen.cpp");
  const QString uri = DiagnosticUtils::filePathToUri(filePath);
  mgr.openDocument(filePath, "cpp", "one");
  QVERIFY(startedSpy.wait(5000));
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "textDocument/didOpen"), 1,
                            5000);

  mgr.openDocument(filePath, "cpp", "two");
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "textDocument/didOpen"), 2,
                            5000);

  const QStringList log = readLog(logPath);
  const int closeIndex =
      log.indexOf(QString("textDocument/didClose %1").arg(uri));
  const int reopenIndex =
      log.indexOf(QString("textDocument/didOpen %1 1 two").arg(uri));
  QVERIFY(closeIndex >= 0);
  QVERIFY(reopenIndex > closeIndex);
}

void TestLanguageFeatureManager::testRestartReopensAllDocuments() {
  QVERIFY(m_tempDir.isValid());
  const QString logPath = makeLogPath();
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);
  useFakeServer(mgr, logPath);

  const QString first = m_tempDir.filePath("first.cpp");
  const QString second = m_tempDir.filePath("second.cpp");
  mgr.openDocument(first, "cpp", "alpha");
  mgr.openDocument(second, "cpp", "beta");
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "textDocument/didOpen"), 2,
                            5000);

  LspClient *oldClient = mgr.clientForFile(first);
  mgr.restartServer("cpp");
  mgr.changeDocument(second, 2, "gamma");

  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "initialize"), 2, 5000);
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "textDocument/didOpen"), 4,
                            5000);
  const QStringList log = readLog(logPath);
  QVERIFY(log.contains(QString("textDocument/didOpen %1 1 alpha")
                           .arg(DiagnosticUtils::filePathToUri(first))));
  QVERIFY(log.contains(QString("textDocument/didOpen %1 2 gamma")
                           .arg(DiagnosticUtils::filePathToUri(second))));
  QVERIFY(mgr.clientForFile(first) != nullptr);
  QVERIFY(mgr.clientForFile(first) != oldClient);
  QCOMPARE(mgr.clientForFile(first), mgr.clientForFile(second));
}

void TestLanguageFeatureManager::testCrashedServerIsReplaced() {
  QVERIFY(m_tempDir.isValid());
  const QString logPath = makeLogPath();
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);
  useFakeServer(mgr, logPath);

  const QString first = m_tempDir.filePath("crash.cpp");
  mgr.openDocument(first, "cpp", "fine");
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "textDocument/didOpen"), 1,
                            5000);

  mgr.changeDocument(first, 2, "CRASH now");
  QTRY_COMPARE_WITH_TIMEOUT(mgr.serverHealth("cpp"), ServerHealthStatus::Error,
                            5000);
  QTRY_VERIFY_WITH_TIMEOUT(mgr.clientForFile(first) == nullptr, 5000);

  mgr.changeDocument(first, 3, "recovered");
  const QString second = m_tempDir.filePath("after.cpp");
  mgr.openDocument(second, "cpp", "next");

  QTRY_COMPARE_WITH_TIMEOUT(mgr.serverHealth("cpp"),
                            ServerHealthStatus::Running, 5000);
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "textDocument/didOpen"), 3,
                            5000);
  QVERIFY(readLog(logPath).contains(
      QString("textDocument/didOpen %1 3 recovered")
          .arg(DiagnosticUtils::filePathToUri(first))));
}

void TestLanguageFeatureManager::testStaleDiagnosticsDropped() {
  QVERIFY(m_tempDir.isValid());
  const QString logPath = makeLogPath();
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);
  useFakeServer(mgr, logPath);

  const QString filePath = m_tempDir.filePath("stale.cpp");
  mgr.openDocument(filePath, "cpp", "current");
  QTRY_COMPARE_WITH_TIMEOUT(diagMgr.diagnosticsForFile(filePath).size(), 1,
                            5000);
  QCOMPARE(diagMgr.diagnosticsForFile(filePath).first().message,
           QString("current"));

  mgr.changeDocument(filePath, 2, "STALE");
  QTRY_VERIFY_WITH_TIMEOUT(
      !diagMgr.diagnosticsForUri("file:///marker").isEmpty(), 5000);
  QCOMPARE(diagMgr.diagnosticsForFile(filePath).first().message,
           QString("current"));
}

void TestLanguageFeatureManager::testDiagnosticsForPathWithSpaces() {
  QVERIFY(m_tempDir.isValid());
  QVERIFY(QDir(m_tempDir.path()).mkpath("with space"));
  const QString logPath = makeLogPath();
  DiagnosticsManager diagMgr;
  LanguageFeatureManager mgr(&diagMgr);
  useFakeServer(mgr, logPath);

  const QString filePath = m_tempDir.filePath("with space/\u00e9t\u00e9.cpp");
  mgr.openDocument(filePath, "cpp", "spaced");
  QTRY_COMPARE_WITH_TIMEOUT(diagMgr.diagnosticsForFile(filePath).size(), 1,
                            5000);
  QCOMPARE(diagMgr.diagnosticsForFile(filePath).first().message,
           QString("spaced"));
}

void TestLanguageFeatureManager::testNodeLanguageIds_data() {
  QTest::addColumn<QString>("extension");
  QTest::addColumn<QString>("language");
  QTest::addColumn<QString>("protocolId");
  QTest::newRow("javascript")
      << QString("js") << QString("js") << QString("javascript");
  QTest::newRow("commonjs")
      << QString("cjs") << QString("node.js") << QString("javascript");
  QTest::newRow("esm") << QString("mjs") << QString("JavaScript")
                       << QString("javascript");
  QTest::newRow("jsx") << QString("jsx") << QString("js")
                       << QString("javascriptreact");
  QTest::newRow("typescript")
      << QString("ts") << QString("ts") << QString("typescript");
  QTest::newRow("tsx") << QString("tsx") << QString("ts")
                       << QString("typescriptreact");
  QTest::newRow("typescript esm")
      << QString("mts") << QString("ts") << QString("typescript");
  QTest::newRow("typescript commonjs")
      << QString("cts") << QString("ts") << QString("typescript");
}

void TestLanguageFeatureManager::testNodeLanguageIds() {
  QFETCH(QString, extension);
  QFETCH(QString, language);
  QFETCH(QString, protocolId);
  DiagnosticsManager diagnostics;
  LanguageFeatureManager manager(&diagnostics);
  const QString canonical = LanguageCatalog::normalize(language);
  const auto defaultConfig = manager.serverConfig(canonical);
  QCOMPARE(defaultConfig.command, QString("typescript-language-server"));
  QCOMPARE(defaultConfig.arguments, QStringList{"--stdio"});

  const QString logPath = makeLogPath();
  DiagnosticsServerConfig config;
  config.languageId = canonical;
  config.command = FAKE_LSP_SERVER_PATH;
  config.arguments = {"--log", logPath};
  manager.setServerConfig(config);
  const QString first = m_tempDir.filePath("first." + extension);
  QCOMPARE(manager.resolveLanguageId(first), canonical);
  manager.openDocument(first, language, "export const value = 1;");
  QTRY_VERIFY_WITH_TIMEOUT(
      readLog(logPath).contains("languageId " + protocolId), 5000);
  QVERIFY(readLog(logPath).contains(
      "rootUri " + DiagnosticUtils::filePathToUri(m_tempDir.path())));
  // Also exercise the immediate didOpen path after initialization.
  const QString second = m_tempDir.filePath("second." + extension);
  manager.openDocument(second, language, "export const other = 2;");
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "languageId " + protocolId), 2,
                            5000);
  QCOMPARE(manager.clientForFile(first), manager.clientForFile(second));
}

void TestLanguageFeatureManager::testDetectAdditionalProjectRoots_data() {
  QTest::addColumn<QString>("marker");
  for (const char *marker :
       {"go.work", "package.json", "tsconfig.json", "jsconfig.json"}) {
    QTest::newRow(marker) << QString::fromLatin1(marker);
  }
}

void TestLanguageFeatureManager::testDetectAdditionalProjectRoots() {
  QFETCH(QString, marker);
  QTemporaryDir project;
  QVERIFY(project.isValid());
  QVERIFY(QDir(project.path()).mkpath("src/nested"));
  QFile file(project.filePath(marker));
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("{}");
  file.close();
  QCOMPARE(LanguageFeatureManager::detectProjectRoot(
               project.filePath("src/nested/main.js")),
           project.path());
}

void TestLanguageFeatureManager::testProjectServerContext_data() {
  QTest::addColumn<QString>("language");
  QTest::addColumn<QString>("extension");
  QTest::addColumn<QString>("marker");
  QTest::addColumn<QString>("protocolId");
  QTest::newRow("C++") << QString("cpp") << QString("cpp")
                       << QString("CMakeLists.txt") << QString("cpp");
  QTest::newRow("C") << QString("c") << QString("c")
                     << QString("CMakeLists.txt") << QString("c");
  QTest::newRow("C with shared clangd configuration")
      << QString("cpp") << QString("c") << QString("CMakeLists.txt")
      << QString("c");
  QTest::newRow("Python") << QString("py") << QString("py")
                          << QString("pyproject.toml") << QString("python");
  QTest::newRow("Rust") << QString("rust") << QString("rs")
                        << QString("Cargo.toml") << QString("rust");
  QTest::newRow("Go") << QString("go") << QString("go") << QString("go.mod")
                      << QString("go");
  QTest::newRow("JavaScript")
      << QString("js") << QString("js") << QString("package.json")
      << QString("javascript");
  QTest::newRow("TypeScript")
      << QString("ts") << QString("ts") << QString("tsconfig.json")
      << QString("typescript");
  QTest::newRow("Java") << QString("java") << QString("java")
                        << QString("pom.xml") << QString("java");
  QTest::newRow("Dockerfile")
      << QString("dockerfile") << QString("dockerfile")
      << QString("package.json") << QString("dockerfile");
}

void TestLanguageFeatureManager::testProjectServerContext() {
  QFETCH(QString, language);
  QFETCH(QString, extension);
  QFETCH(QString, marker);
  QFETCH(QString, protocolId);
  QTemporaryDir first;
  QTemporaryDir second;
  QVERIFY(first.isValid() && second.isValid());
  for (const QString &root : {first.path(), second.path()}) {
    QFile file(QDir(root).filePath(marker));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{}");
  }
  const QString logPath = makeLogPath();
  const QString externalUri = DiagnosticUtils::filePathToUri(
      m_tempDir.filePath("external library." + extension));
  DiagnosticsManager diagnostics;
  LanguageFeatureManager manager(&diagnostics);
  QVERIFY(manager.isLanguageSupported(language));
  auto config = manager.serverConfig(language);
  config.command = FAKE_LSP_SERVER_PATH;
  config.arguments = {
      "--log",     logPath,        "--trace-context",        "--definition-uri",
      externalUri, "--hover-text", "External library member"};
  config.environmentVariables = {
      "LIGHTPAD_LSP_TEST_ENV=${workspaceFolder}=value"};
  config.settings = QJsonObject{{"example", QJsonObject{{"imports", true}}}};
  config.initializationOptions = QJsonObject{{"librarySupport", true}};
  manager.setServerConfig(config);
  const QString a = first.filePath("main." + extension);
  const QString b = second.filePath("main." + extension);
  manager.openDocument(a, language, "library.member");
  manager.openDocument(b, language, "library.member");
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "languageId " + protocolId), 2,
                            5000);
  QVERIFY(manager.clientForFile(a) != manager.clientForFile(b));
  for (const QString &root : {first.path(), second.path()}) {
    QVERIFY(readLog(logPath).contains("rootUri " +
                                      DiagnosticUtils::filePathToUri(root)));
    QVERIFY(readLog(logPath).contains("cwd " + root));
    QVERIFY(readLog(logPath).contains("environment " + root + "=value"));
  }
  QVERIFY(readLog(logPath).contains(
      "initializationOptions {\"librarySupport\":true}"));
  QTRY_VERIFY_WITH_TIMEOUT(countLines(logPath, "settings ") == 2, 5000);
  auto *client = manager.clientForFile(a);
  QSignalSpy hover(client, &LspClient::hoverReceived);
  QSignalSpy definition(client, &LspClient::definitionReceived);
  client->requestHover(DiagnosticUtils::filePathToUri(a), {0, 10});
  client->requestDefinition(DiagnosticUtils::filePathToUri(a), {0, 10});
  QTRY_COMPARE_WITH_TIMEOUT(hover.count(), 1, 5000);
  QTRY_COMPARE_WITH_TIMEOUT(definition.count(), 1, 5000);
  QCOMPARE(hover.first().at(1).toString(), QString("External library member"));
  const auto locations =
      qvariant_cast<QList<LspLocation>>(definition.first().at(1));
  QCOMPARE(locations.size(), 1);
  QCOMPARE(locations.first().uri, externalUri);
  manager.restartServer(language);
  QTRY_COMPARE_WITH_TIMEOUT(countLines(logPath, "languageId " + protocolId), 4,
                            5000);
  QVERIFY(manager.clientForFile(a) != manager.clientForFile(b));
}

void TestLanguageFeatureManager::testPythonServerFallback() {
  QTemporaryDir project;
  QVERIFY(project.isValid());
  const QString environment = project.filePath(".venv");
  const QString bin = PythonProjectEnvironment::venvBinPath(environment);
  QVERIFY(QDir().mkpath(bin));
#ifdef Q_OS_WIN
  const QString pythonName = "python.exe";
  const QString serverName = "pylsp.exe";
#else
  const QString pythonName = "python";
  const QString serverName = "pylsp";
#endif
  const QString python = PythonProjectEnvironment::globalPythonInterpreter();
  if (python.isEmpty())
    QSKIP("Python interpreter not installed");
  QVERIFY(QFile::copy(python, QDir(bin).filePath(pythonName)));
  QVERIFY(QFile::copy(FAKE_LSP_SERVER_PATH, QDir(bin).filePath(serverName)));
  QFile marker(QDir(environment).filePath("pyvenv.cfg"));
  QVERIFY(marker.open(QIODevice::WriteOnly));
  marker.write("home = .\n");
  marker.close();
  LanguageFeatureManager manager(nullptr);
  auto config = manager.serverConfig("py");
  config.command = "basedpyright-langserver";
  config.arguments = {"--stdio"};
  config.environmentVariables = {"PATH=" + bin};
  manager.setServerConfig(config);
  const QString file = project.filePath("example.py");
  manager.openDocument(file, "py", "print('hello')");
  QTRY_VERIFY_WITH_TIMEOUT(manager.clientForFile(file) &&
                               manager.clientForFile(file)->isReady(),
                           5000);
  QCOMPARE(manager.clientForFile(file)->program(),
           QDir(bin).filePath(serverName));
  auto *process = manager.clientForFile(file)->findChild<QProcess *>();
  QVERIFY(process && process->arguments().isEmpty());
}

void TestLanguageFeatureManager::testExternalDocumentKeepsServerContext() {
  QTemporaryDir project;
  QTemporaryDir cache;
  QVERIFY(project.isValid() && cache.isValid());
  QFile marker(project.filePath("pom.xml"));
  QVERIFY(marker.open(QIODevice::WriteOnly));
  marker.write("<project/>");
  marker.close();
  const QString source = project.filePath("Main.java");
  const QString external = cache.filePath("Library.java");
  const QString uri = "jdt://contents/library/Library.class?jar=example";
  const QString log = makeLogPath();
  LanguageFeatureManager manager(nullptr);
  auto config = manager.serverConfig("java");
  config.command = FAKE_LSP_SERVER_PATH;
  config.arguments = {"--log", log, "--trace-context"};
  manager.setServerConfig(config);
  manager.openDocument(source, "java", "class Main {}");
  QTRY_COMPARE_WITH_TIMEOUT(countLines(log, "languageId java"), 1, 5000);
  auto *original = manager.clientForFile(source);
  manager.openExternalDocument(external, uri, source, "class Library {}");
  QCOMPARE(manager.clientForFile(external), original);
  QCOMPARE(manager.documentUriForFile(external), uri);
  QVERIFY(manager.isExternalDocument(external));
  QTRY_VERIFY_WITH_TIMEOUT(readLog(log).contains("textDocument/didOpen " + uri +
                                                 " 1 class Library {}"),
                           5000);
  manager.closeDocument(source);
  manager.restartServer("java");
  QTRY_COMPARE_WITH_TIMEOUT(countLines(log, "initialize"), 2, 5000);
  QTRY_VERIFY_WITH_TIMEOUT(manager.clientForFile(external) &&
                               manager.clientForFile(external)->isReady(),
                           5000);
  QVERIFY(manager.clientForFile(external) != original);
  manager.saveDocument(external);
  QTRY_VERIFY_WITH_TIMEOUT(readLog(log).contains("textDocument/didSave " + uri),
                           5000);
  manager.closeDocument(external);
  QVERIFY(!manager.isExternalDocument(external));
}

void TestLanguageFeatureManager::testPythonExternalLibrary_data() {
  QTest::addColumn<QString>("server");
  QTest::addColumn<QStringList>("arguments");
  QTest::addColumn<bool>("compiledDefinitions");
  const QString pylsp = qEnvironmentVariable("LIGHTPAD_TEST_PYLSP");
  const QString basedpyright =
      qEnvironmentVariable("LIGHTPAD_TEST_BASEDPYRIGHT");
  if (!pylsp.isEmpty()) {
    QTest::newRow("pylsp") << pylsp << QStringList{} << false;
  }
  if (!basedpyright.isEmpty()) {
    QTest::newRow("basedpyright")
        << basedpyright << QStringList{"--stdio"} << true;
  }
  if (pylsp.isEmpty() && basedpyright.isEmpty()) {
    QTest::newRow("optional") << QString{} << QStringList{} << false;
  }
}

void TestLanguageFeatureManager::testPythonExternalLibrary() {
  QFETCH(QString, server);
  QFETCH(QStringList, arguments);
  QFETCH(bool, compiledDefinitions);
  const QString interpreter = qEnvironmentVariable("LIGHTPAD_TEST_PYTHON");
  if (server.isEmpty() || interpreter.isEmpty()) {
    QSKIP("Set LIGHTPAD_TEST_PYTHON and LIGHTPAD_TEST_PYLSP or "
          "LIGHTPAD_TEST_BASEDPYRIGHT to test NumPy in a "
          "separate interpreter");
  }
  QTemporaryDir project;
  QVERIFY(project.isValid());
  QFile marker(project.filePath("pyproject.toml"));
  QVERIFY(marker.open(QIODevice::WriteOnly));
  marker.write(
      "[project]\nname = 'external-library-test'\nversion = '0.1.0'\n");
  marker.close();
  const QString path = project.filePath("example.py");
  const QString source = "import numpy as np\nvalues = np.array([1, 2, "
                         "3])\nlength = np.linalg.norm(values)\n";
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(source.toUtf8());
  file.close();
  LanguageFeatureManager manager(nullptr);
  manager.setWorkspaceFolder(project.path());
  manager.setPythonEnvironmentResolver([interpreter](const QString &) {
    return PythonEnvironmentPreference{
        "customInterpreter", interpreter, {}, {}};
  });
  auto config = manager.serverConfig("py");
  config.command = server;
  config.arguments = arguments;
  manager.setServerConfig(config);
  manager.openDocument(path, "python", source);
  QTRY_VERIFY_WITH_TIMEOUT(manager.clientForFile(path) &&
                               manager.clientForFile(path)->isReady(),
                           15000);
  auto *client = manager.clientForFile(path);
  QSignalSpy hover(client, &LspClient::hoverReceived);
  LanguageLspDefinitionProvider provider(
      {"python-test", "Python", {"py"}, server, arguments});
  provider.setClientResolver([client](const QString &) { return client; });
  QSignalSpy definition(&provider, &IDefinitionProvider::definitionReady);
  QList<QPair<int, QString>> symbols{{1, "np"}, {2, "norm"}};
  if (compiledDefinitions)
    symbols.append({1, "array"});
  for (const auto &symbol : symbols) {
    hover.clear();
    definition.clear();
    const int column =
        source.split('\n').at(symbol.first).indexOf(symbol.second) + 1;
    client->requestHover(DiagnosticUtils::filePathToUri(path),
                         {symbol.first, column});
    DefinitionRequest request;
    request.filePath = path;
    request.languageId = "py";
    request.line = symbol.first + 1;
    request.column = column;
    provider.requestDefinition(request);
    QTRY_COMPARE_WITH_TIMEOUT(hover.count(), 1, 20000);
    QVERIFY2(!hover.first().at(1).toString().trimmed().isEmpty(),
             qPrintable(symbol.second));
    QTRY_COMPARE_WITH_TIMEOUT(definition.count(), 1, 20000);
    const auto locations =
        qvariant_cast<QList<DefinitionTarget>>(definition.first().at(1));
    QVERIFY2(!locations.isEmpty(), qPrintable(symbol.second));
    const QString target = locations.first().filePath;
    QDir environmentRoot(QFileInfo(interpreter).absolutePath());
    QVERIFY(environmentRoot.cdUp());
    QVERIFY2(target.startsWith(environmentRoot.absolutePath() + '/'),
             qPrintable(target));
    QVERIFY2(target.contains("numpy"), qPrintable(target));
    QVERIFY2(QFileInfo::exists(target), qPrintable(target));
  }
}

QTEST_MAIN(TestLanguageFeatureManager)
#include "test_languagefeaturemanager.moc"
