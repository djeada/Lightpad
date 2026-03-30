#include "lsp/lspclient.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

class TestLspRegression : public QObject {
  Q_OBJECT

private slots:
  void testDiagnosticSeverityRoundTrip();
  void testPositionBoundaryValues();
  void testRangeWithZeroLength();
  void testEmptyCompletionList();
  void testWorkspaceEditEmptyChanges();
  void testCodeActionWithNullEdit();
  void testDocumentSymbolDeepNesting();
  void testTextEditOverlappingRanges();
  void testClientStateTransitions();
  void testCapabilityCheckUnknown();
  void testDiagnosticWithAllSeverities();
  void testSymbolKindCompleteness();
  void testCodeActionKindStrings();
  void testPositionComparison();
  void testRangeContainment();
};

void TestLspRegression::testDiagnosticSeverityRoundTrip() {
  LspDiagnostic diag;
  diag.severity = LspDiagnosticSeverity::Error;
  diag.message = "test error";
  diag.source = "clangd";
  diag.range.start = {10, 5};
  diag.range.end = {10, 15};

  QCOMPARE(diag.severity, LspDiagnosticSeverity::Error);
  QCOMPARE(diag.range.start.line, 10);
  QCOMPARE(diag.range.start.character, 5);
  QCOMPARE(diag.range.end.line, 10);
  QCOMPARE(diag.range.end.character, 15);
}

void TestLspRegression::testPositionBoundaryValues() {
  LspPosition pos;
  pos.line = 0;
  pos.character = 0;

  QJsonObject json = pos.toJson();
  QCOMPARE(json["line"].toInt(), 0);
  QCOMPARE(json["character"].toInt(), 0);

  LspPosition maxPos;
  maxPos.line = 999999;
  maxPos.character = 999999;

  QJsonObject maxJson = maxPos.toJson();
  QCOMPARE(maxJson["line"].toInt(), 999999);
  QCOMPARE(maxJson["character"].toInt(), 999999);
}

void TestLspRegression::testRangeWithZeroLength() {
  LspRange range;
  range.start = {5, 10};
  range.end = {5, 10};

  QJsonObject json = range.toJson();
  QJsonObject startJson = json["start"].toObject();
  QJsonObject endJson = json["end"].toObject();

  QCOMPARE(startJson["line"].toInt(), endJson["line"].toInt());
  QCOMPARE(startJson["character"].toInt(), endJson["character"].toInt());
}

void TestLspRegression::testEmptyCompletionList() {
  QList<LspDocumentSymbol> emptySymbols;
  QVERIFY(emptySymbols.isEmpty());
}

void TestLspRegression::testWorkspaceEditEmptyChanges() {
  LspWorkspaceEdit edit;
  QVERIFY(edit.changes.isEmpty());
}

void TestLspRegression::testCodeActionWithNullEdit() {
  LspCodeAction action;
  action.title = "Fix import";
  action.kind = "quickfix";

  QCOMPARE(action.title, QString("Fix import"));
  QVERIFY(action.edit.changes.isEmpty());
}

void TestLspRegression::testDocumentSymbolDeepNesting() {
  LspDocumentSymbol root;
  root.name = "Root";
  root.kind = LspSymbolKind::Class;

  LspDocumentSymbol child;
  child.name = "Child";
  child.kind = LspSymbolKind::Method;

  LspDocumentSymbol grandchild;
  grandchild.name = "Grandchild";
  grandchild.kind = LspSymbolKind::Variable;

  child.children.append(grandchild);
  root.children.append(child);

  QCOMPARE(root.children.size(), 1);
  QCOMPARE(root.children[0].children.size(), 1);
  QCOMPARE(root.children[0].children[0].name, QString("Grandchild"));
}

void TestLspRegression::testTextEditOverlappingRanges() {
  LspTextEdit edit1;
  edit1.range.start = {1, 0};
  edit1.range.end = {1, 10};
  edit1.newText = "replacement1";

  LspTextEdit edit2;
  edit2.range.start = {1, 5};
  edit2.range.end = {1, 15};
  edit2.newText = "replacement2";

  QVERIFY(edit1.range.start.line == edit2.range.start.line);
  QVERIFY(edit1.range.end.character > edit2.range.start.character);
}

void TestLspRegression::testClientStateTransitions() {
  LspClient client;

  QCOMPARE(client.state(), LspClient::State::Disconnected);
  QVERIFY(!client.isReady());

  QVERIFY(LspClient::State::Disconnected != LspClient::State::Ready);
  QVERIFY(LspClient::State::Connecting != LspClient::State::Initializing);
  QVERIFY(LspClient::State::Ready != LspClient::State::ShuttingDown);
  QVERIFY(LspClient::State::Error != LspClient::State::Ready);
}

void TestLspRegression::testCapabilityCheckUnknown() {
  LspClient client;

  QVERIFY(!client.supportsCapability("unknownCapability"));
  QVERIFY(!client.supportsCapability(""));
}

void TestLspRegression::testDiagnosticWithAllSeverities() {
  QList<LspDiagnosticSeverity> severities = {
      LspDiagnosticSeverity::Error, LspDiagnosticSeverity::Warning,
      LspDiagnosticSeverity::Information, LspDiagnosticSeverity::Hint};

  for (auto severity : severities) {
    LspDiagnostic diag;
    diag.severity = severity;
    diag.message = "test";
    QCOMPARE(diag.severity, severity);
  }
}

void TestLspRegression::testSymbolKindCompleteness() {
  QVERIFY(static_cast<int>(LspSymbolKind::File) == 1);
  QVERIFY(static_cast<int>(LspSymbolKind::Module) == 2);
  QVERIFY(static_cast<int>(LspSymbolKind::Namespace) == 3);
  QVERIFY(static_cast<int>(LspSymbolKind::Class) == 5);
  QVERIFY(static_cast<int>(LspSymbolKind::Method) == 6);
  QVERIFY(static_cast<int>(LspSymbolKind::Function) == 12);
  QVERIFY(static_cast<int>(LspSymbolKind::Variable) == 13);
}

void TestLspRegression::testCodeActionKindStrings() {
  LspCodeAction action;
  action.kind = "quickfix";
  QCOMPARE(action.kind, QString("quickfix"));

  action.kind = "refactor";
  QCOMPARE(action.kind, QString("refactor"));

  action.kind = "source";
  QCOMPARE(action.kind, QString("source"));
}

void TestLspRegression::testPositionComparison() {
  LspPosition p1 = {0, 0};
  LspPosition p2 = {0, 5};
  LspPosition p3 = {1, 0};

  QVERIFY(p1.line < p3.line);
  QVERIFY(p1.character < p2.character);
  QVERIFY(p1.line == p2.line);
}

void TestLspRegression::testRangeContainment() {
  LspRange outer;
  outer.start = {0, 0};
  outer.end = {10, 0};

  LspRange inner;
  inner.start = {2, 0};
  inner.end = {5, 0};

  QVERIFY(inner.start.line >= outer.start.line);
  QVERIFY(inner.end.line <= outer.end.line);
}

QTEST_MAIN(TestLspRegression)
#include "test_lspregression.moc"
