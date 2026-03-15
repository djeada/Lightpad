#include "diagnostics/diagnosticsmanager.h"
#include "diagnostics/diagnosticutils.h"
#include <QSignalSpy>
#include <QTest>

class TestDiagnosticsManager : public QObject {
  Q_OBJECT

private slots:
  void testInitialState();
  void testUpsertDiagnostics();
  void testUpsertReplaceSameSource();
  void testUpsertMultipleSources();
  void testClearDiagnostics();
  void testClearAllForSource();
  void testDiagnosticsForFile();
  void testDiagnosticsForUri();
  void testCountsBySeverity();
  void testCountsChanged();
  void testDiagnosticsChangedSignal();
  void testFileCountsChangedSignal();
  void testStaleVersionDropped();
  void testVersionTracking();
  void testAllUris();
  void testFilePathToUri();
  void testUriToFilePath();
  void testClampLine();
  void testClampColumn();

private:
  LspDiagnostic makeDiag(LspDiagnosticSeverity severity, const QString &message,
                         int line = 0, int col = 0) {
    LspDiagnostic d;
    d.severity = severity;
    d.message = message;
    d.range.start.line = line;
    d.range.start.character = col;
    d.range.end.line = line;
    d.range.end.character = col + 5;
    d.source = "test";
    d.code = "E001";
    return d;
  }
};

void TestDiagnosticsManager::testInitialState() {
  DiagnosticsManager mgr;
  QCOMPARE(mgr.errorCount(), 0);
  QCOMPARE(mgr.warningCount(), 0);
  QCOMPARE(mgr.infoCount(), 0);
  QVERIFY(mgr.allUris().isEmpty());
}

void TestDiagnosticsManager::testUpsertDiagnostics() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.cpp";

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error 1"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);
  QCOMPARE(mgr.errorCount(), 1);
}

void TestDiagnosticsManager::testUpsertReplaceSameSource() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.cpp";

  QList<LspDiagnostic> diags1;
  diags1.append(makeDiag(LspDiagnosticSeverity::Error, "old error"));
  mgr.upsertDiagnostics(uri, diags1, "lsp:clangd");

  QList<LspDiagnostic> diags2;
  diags2.append(makeDiag(LspDiagnosticSeverity::Warning, "new warning"));
  mgr.upsertDiagnostics(uri, diags2, "lsp:clangd");

  QList<LspDiagnostic> result = mgr.diagnosticsForUri(uri);
  QCOMPARE(result.size(), 1);
  QCOMPARE(result[0].message, QString("new warning"));
  QCOMPARE(mgr.errorCount(), 0);
  QCOMPARE(mgr.warningCount(), 1);
}

void TestDiagnosticsManager::testUpsertMultipleSources() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.py";

  QList<LspDiagnostic> lspDiags;
  lspDiags.append(makeDiag(LspDiagnosticSeverity::Error, "lsp error"));
  mgr.upsertDiagnostics(uri, lspDiags, "lsp:pylsp");

  QList<LspDiagnostic> cliDiags;
  cliDiags.append(makeDiag(LspDiagnosticSeverity::Warning, "cli warning"));
  mgr.upsertDiagnostics(uri, cliDiags, "cli:ruff");

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 2);
  QCOMPARE(mgr.errorCount(), 1);
  QCOMPARE(mgr.warningCount(), 1);
}

void TestDiagnosticsManager::testClearDiagnostics() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.cpp";

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");

  QCOMPARE(mgr.errorCount(), 1);

  mgr.clearDiagnostics(uri);
  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 0);
  QCOMPARE(mgr.errorCount(), 0);
}

void TestDiagnosticsManager::testClearAllForSource() {
  DiagnosticsManager mgr;
  QString uri1 = "file:///test/a.cpp";
  QString uri2 = "file:///test/b.cpp";

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error"));

  mgr.upsertDiagnostics(uri1, diags, "lsp:clangd");
  mgr.upsertDiagnostics(uri2, diags, "lsp:clangd");

  QList<LspDiagnostic> otherDiags;
  otherDiags.append(makeDiag(LspDiagnosticSeverity::Warning, "warning"));
  mgr.upsertDiagnostics(uri1, otherDiags, "cli:cppcheck");

  QCOMPARE(mgr.errorCount(), 2);
  QCOMPARE(mgr.warningCount(), 1);

  mgr.clearAllForSource("lsp:clangd");

  QCOMPARE(mgr.errorCount(), 0);
  QCOMPARE(mgr.warningCount(), 1);
  QCOMPARE(mgr.diagnosticsForUri(uri1).size(), 1);
  QCOMPARE(mgr.diagnosticsForUri(uri2).size(), 0);
}

void TestDiagnosticsManager::testDiagnosticsForFile() {
  DiagnosticsManager mgr;
  QString filePath = "/test/file.cpp";
  QString uri = DiagnosticUtils::filePathToUri(filePath);

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error in file"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");

  QList<LspDiagnostic> result = mgr.diagnosticsForFile(filePath);
  QCOMPARE(result.size(), 1);
  QCOMPARE(result[0].message, QString("error in file"));
}

void TestDiagnosticsManager::testDiagnosticsForUri() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.py";

  QVERIFY(mgr.diagnosticsForUri(uri).isEmpty());

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Warning, "warn"));
  mgr.upsertDiagnostics(uri, diags, "lsp:pylsp");

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);
}

void TestDiagnosticsManager::testCountsBySeverity() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.cpp";

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "e1"));
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "e2"));
  diags.append(makeDiag(LspDiagnosticSeverity::Warning, "w1"));
  diags.append(makeDiag(LspDiagnosticSeverity::Information, "i1"));
  diags.append(makeDiag(LspDiagnosticSeverity::Hint, "h1"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");

  QCOMPARE(mgr.errorCount(), 2);
  QCOMPARE(mgr.warningCount(), 1);
  QCOMPARE(mgr.infoCount(), 2);
}

void TestDiagnosticsManager::testCountsChanged() {
  DiagnosticsManager mgr;
  QSignalSpy spy(&mgr, &DiagnosticsManager::countsChanged);

  QString uri = "file:///test/file.cpp";
  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");

  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args[0].toInt(), 1);
  QCOMPARE(args[1].toInt(), 0);
  QCOMPARE(args[2].toInt(), 0);
}

void TestDiagnosticsManager::testDiagnosticsChangedSignal() {
  DiagnosticsManager mgr;
  QSignalSpy spy(&mgr, &DiagnosticsManager::diagnosticsChanged);

  QString uri = "file:///test/file.cpp";
  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst()[0].toString(), uri);
}

void TestDiagnosticsManager::testFileCountsChangedSignal() {
  DiagnosticsManager mgr;
  QSignalSpy spy(&mgr, &DiagnosticsManager::fileCountsChanged);

  QString filePath = "/test/file.cpp";
  QString uri = DiagnosticUtils::filePathToUri(filePath);

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "e"));
  diags.append(makeDiag(LspDiagnosticSeverity::Warning, "w"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");

  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args[0].toString(), filePath);
  QCOMPARE(args[1].toInt(), 1);
  QCOMPARE(args[2].toInt(), 1);
  QCOMPARE(args[3].toInt(), 0);
}

void TestDiagnosticsManager::testStaleVersionDropped() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.cpp";

  mgr.trackDocumentVersion(uri, 5);

  QList<LspDiagnostic> staleDiags;
  staleDiags.append(makeDiag(LspDiagnosticSeverity::Error, "stale"));
  mgr.upsertDiagnostics(uri, staleDiags, "lsp:clangd", 3);

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 0);
  QCOMPARE(mgr.errorCount(), 0);

  QList<LspDiagnostic> freshDiags;
  freshDiags.append(makeDiag(LspDiagnosticSeverity::Error, "fresh"));
  mgr.upsertDiagnostics(uri, freshDiags, "lsp:clangd", 5);

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);
  QCOMPARE(mgr.errorCount(), 1);
}

void TestDiagnosticsManager::testVersionTracking() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/file.cpp";

  QCOMPARE(mgr.documentVersion(uri), 0);

  mgr.trackDocumentVersion(uri, 3);
  QCOMPARE(mgr.documentVersion(uri), 3);

  mgr.trackDocumentVersion(uri, 7);
  QCOMPARE(mgr.documentVersion(uri), 7);
}

void TestDiagnosticsManager::testAllUris() {
  DiagnosticsManager mgr;
  QString uri1 = "file:///test/a.cpp";
  QString uri2 = "file:///test/b.py";

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "e"));

  mgr.upsertDiagnostics(uri1, diags, "lsp:clangd");
  mgr.upsertDiagnostics(uri2, diags, "lsp:pylsp");

  QStringList uris = mgr.allUris();
  QCOMPARE(uris.size(), 2);
  QVERIFY(uris.contains(uri1));
  QVERIFY(uris.contains(uri2));
}

void TestDiagnosticsManager::testFilePathToUri() {
  QString path = "/home/user/project/main.cpp";
  QString uri = DiagnosticUtils::filePathToUri(path);
  QVERIFY(uri.startsWith("file:///"));
  QVERIFY(uri.contains("main.cpp"));
}

void TestDiagnosticsManager::testUriToFilePath() {
  QString uri = "file:///home/user/project/main.cpp";
  QString path = DiagnosticUtils::uriToFilePath(uri);
  QCOMPARE(path, QString("/home/user/project/main.cpp"));
}

void TestDiagnosticsManager::testClampLine() {
  QCOMPARE(DiagnosticUtils::clampLine(-1, 10), 0);
  QCOMPARE(DiagnosticUtils::clampLine(0, 10), 0);
  QCOMPARE(DiagnosticUtils::clampLine(5, 10), 5);
  QCOMPARE(DiagnosticUtils::clampLine(10, 10), 9);
  QCOMPARE(DiagnosticUtils::clampLine(100, 10), 9);
  QCOMPARE(DiagnosticUtils::clampLine(5, 0), 0);
}

void TestDiagnosticsManager::testClampColumn() {
  QCOMPARE(DiagnosticUtils::clampColumn(-1, 10), 0);
  QCOMPARE(DiagnosticUtils::clampColumn(0, 10), 0);
  QCOMPARE(DiagnosticUtils::clampColumn(5, 10), 5);
  QCOMPARE(DiagnosticUtils::clampColumn(10, 10), 10);
  QCOMPARE(DiagnosticUtils::clampColumn(11, 10), 10);
  QCOMPARE(DiagnosticUtils::clampColumn(5, 0), 0);
}

QTEST_MAIN(TestDiagnosticsManager)
#include "test_diagnosticsmanager.moc"
