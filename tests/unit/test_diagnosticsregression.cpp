#include "diagnostics/diagnosticsmanager.h"
#include "lsp/lspclient.h"
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QtTest>

class TestDiagnosticsRegression : public QObject {
  Q_OBJECT

private slots:
  void testStaleDiagnosticsDropped();
  void testVersionMismatchDoesNotOverwrite();
  void testConcurrentSourceUpdates();
  void testClearAndReinsert();
  void testUpsertEmptyListClears();
  void testMultipleFilesIndependent();
  void testRapidUpsertSequence();
  void testDiagnosticsAfterClearAll();
  void testSameSourceMultipleFiles();
  void testSignalEmissionOnVersionDrop();

private:
  LspDiagnostic makeDiag(LspDiagnosticSeverity severity, const QString &message,
                         int line = 0) {
    LspDiagnostic d;
    d.severity = severity;
    d.message = message;
    d.range.start.line = line;
    d.range.start.character = 0;
    d.range.end.line = line;
    d.range.end.character = 10;
    return d;
  }
};

void TestDiagnosticsRegression::testStaleDiagnosticsDropped() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/stale.cpp";

  mgr.trackDocumentVersion(uri, 5);

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "stale error"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd", 3);

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 0);

  QList<LspDiagnostic> freshDiags;
  freshDiags.append(makeDiag(LspDiagnosticSeverity::Error, "fresh error"));
  mgr.upsertDiagnostics(uri, freshDiags, "lsp:clangd", 5);

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);
  QCOMPARE(mgr.diagnosticsForUri(uri).first().message, QString("fresh error"));
}

void TestDiagnosticsRegression::testVersionMismatchDoesNotOverwrite() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/version.cpp";

  mgr.trackDocumentVersion(uri, 10);

  QList<LspDiagnostic> currentDiags;
  currentDiags.append(makeDiag(LspDiagnosticSeverity::Warning, "current"));
  mgr.upsertDiagnostics(uri, currentDiags, "lsp:clangd", 10);

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);

  mgr.trackDocumentVersion(uri, 11);

  QList<LspDiagnostic> oldDiags;
  oldDiags.append(makeDiag(LspDiagnosticSeverity::Error, "old version error"));
  oldDiags.append(
      makeDiag(LspDiagnosticSeverity::Error, "old version error 2"));
  mgr.upsertDiagnostics(uri, oldDiags, "lsp:clangd", 9);

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);
  QCOMPARE(mgr.diagnosticsForUri(uri).first().message, QString("current"));
}

void TestDiagnosticsRegression::testConcurrentSourceUpdates() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/concurrent.cpp";

  QList<LspDiagnostic> lspDiags;
  lspDiags.append(makeDiag(LspDiagnosticSeverity::Error, "lsp error"));
  mgr.upsertDiagnostics(uri, lspDiags, "lsp:clangd");

  QList<LspDiagnostic> cliDiags;
  cliDiags.append(makeDiag(LspDiagnosticSeverity::Warning, "cli warning"));
  mgr.upsertDiagnostics(uri, cliDiags, "cli:cppcheck");

  QList<LspDiagnostic> allDiags = mgr.diagnosticsForUri(uri);
  QCOMPARE(allDiags.size(), 2);

  mgr.clearAllForSource("lsp:clangd");

  allDiags = mgr.diagnosticsForUri(uri);
  QCOMPARE(allDiags.size(), 1);
  QCOMPARE(allDiags.first().message, QString("cli warning"));
}

void TestDiagnosticsRegression::testClearAndReinsert() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/clearreinsert.cpp";

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error 1"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");
  QCOMPARE(mgr.errorCount(), 1);

  mgr.clearDiagnostics(uri);
  QCOMPARE(mgr.errorCount(), 0);
  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 0);

  QList<LspDiagnostic> newDiags;
  newDiags.append(makeDiag(LspDiagnosticSeverity::Warning, "warning after"));
  mgr.upsertDiagnostics(uri, newDiags, "lsp:clangd");
  QCOMPARE(mgr.warningCount(), 1);
  QCOMPARE(mgr.errorCount(), 0);
}

void TestDiagnosticsRegression::testUpsertEmptyListClears() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/empty.cpp";

  QList<LspDiagnostic> diags;
  diags.append(makeDiag(LspDiagnosticSeverity::Error, "error"));
  mgr.upsertDiagnostics(uri, diags, "lsp:clangd");
  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);

  mgr.upsertDiagnostics(uri, {}, "lsp:clangd");
  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 0);
}

void TestDiagnosticsRegression::testMultipleFilesIndependent() {
  DiagnosticsManager mgr;
  QString uri1 = "file:///test/file1.cpp";
  QString uri2 = "file:///test/file2.cpp";

  QList<LspDiagnostic> diags1;
  diags1.append(makeDiag(LspDiagnosticSeverity::Error, "error in file1"));
  mgr.upsertDiagnostics(uri1, diags1, "lsp:clangd");

  QList<LspDiagnostic> diags2;
  diags2.append(makeDiag(LspDiagnosticSeverity::Warning, "warning in file2"));
  mgr.upsertDiagnostics(uri2, diags2, "lsp:clangd");

  mgr.clearDiagnostics(uri1);

  QCOMPARE(mgr.diagnosticsForUri(uri1).size(), 0);
  QCOMPARE(mgr.diagnosticsForUri(uri2).size(), 1);
}

void TestDiagnosticsRegression::testRapidUpsertSequence() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/rapid.cpp";

  for (int i = 0; i < 100; i++) {
    QList<LspDiagnostic> diags;
    diags.append(
        makeDiag(LspDiagnosticSeverity::Error, QString("error %1").arg(i)));
    mgr.upsertDiagnostics(uri, diags, "lsp:clangd");
  }

  QCOMPARE(mgr.diagnosticsForUri(uri).size(), 1);
  QCOMPARE(mgr.diagnosticsForUri(uri).first().message, QString("error 99"));
}

void TestDiagnosticsRegression::testDiagnosticsAfterClearAll() {
  DiagnosticsManager mgr;
  QString uri1 = "file:///test/a.cpp";
  QString uri2 = "file:///test/b.cpp";

  mgr.upsertDiagnostics(uri1, {makeDiag(LspDiagnosticSeverity::Error, "err a")},
                        "lsp:clangd");
  mgr.upsertDiagnostics(uri2, {makeDiag(LspDiagnosticSeverity::Error, "err b")},
                        "lsp:clangd");

  mgr.clearAllForSource("lsp:clangd");

  QCOMPARE(mgr.diagnosticsForUri(uri1).size(), 0);
  QCOMPARE(mgr.diagnosticsForUri(uri2).size(), 0);
  QCOMPARE(mgr.errorCount(), 0);

  mgr.upsertDiagnostics(uri1,
                        {makeDiag(LspDiagnosticSeverity::Warning, "new warn")},
                        "lsp:clangd");
  QCOMPARE(mgr.diagnosticsForUri(uri1).size(), 1);
  QCOMPARE(mgr.warningCount(), 1);
}

void TestDiagnosticsRegression::testSameSourceMultipleFiles() {
  DiagnosticsManager mgr;

  for (int i = 0; i < 10; i++) {
    QString uri = QString("file:///test/file%1.cpp").arg(i);
    mgr.upsertDiagnostics(
        uri, {makeDiag(LspDiagnosticSeverity::Error, QString("err %1").arg(i))},
        "lsp:clangd");
  }

  QCOMPARE(mgr.errorCount(), 10);
  QCOMPARE(mgr.allUris().size(), 10);

  mgr.clearAllForSource("lsp:clangd");
  QCOMPARE(mgr.errorCount(), 0);
}

void TestDiagnosticsRegression::testSignalEmissionOnVersionDrop() {
  DiagnosticsManager mgr;
  QString uri = "file:///test/signal.cpp";

  mgr.trackDocumentVersion(uri, 5);

  QSignalSpy changeSpy(&mgr, &DiagnosticsManager::diagnosticsChanged);

  QList<LspDiagnostic> staleDiags;
  staleDiags.append(makeDiag(LspDiagnosticSeverity::Error, "stale"));
  mgr.upsertDiagnostics(uri, staleDiags, "lsp:clangd", 2);

  QCOMPARE(changeSpy.count(), 0);
}

QTEST_MAIN(TestDiagnosticsRegression)
#include "test_diagnosticsregression.moc"
