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

QTEST_MAIN(TestLanguageFeatureManager)
#include "test_languagefeaturemanager.moc"
