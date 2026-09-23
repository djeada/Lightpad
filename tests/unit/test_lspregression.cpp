#include "lsp/lspclient.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
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
  void testFramingWithNonAsciiPayload();
  void testFramingSplitAcrossReads();
  void testFramingMultipleMessagesInOneRead();
  void testNullLocationResultIsEmpty();
  void testSingleLocationAndLocationLink();
  void testCompletionItemPrefersTextEdit();
  void testCompletionItemInsertTextFormat();
  void testServerRequestsAreAnswered();
  void testBurstOfMessagesIsFullyDrained();
  void testDiagnosticsCarryServerVersion();
  void testPendingRequestsFailWhenServerExits();

private:
  QString makeLogPath();
  QStringList readLog(const QString &path) const;
  bool startFakeServer(LspClient &client, const QString &logPath);

  QTemporaryDir m_tempDir;
  int m_logCounter = 0;
};

QString TestLspRegression::makeLogPath() {
  return m_tempDir.filePath(QString("lsp-%1.log").arg(++m_logCounter));
}

QStringList TestLspRegression::readLog(const QString &path) const {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
}

bool TestLspRegression::startFakeServer(LspClient &client,
                                        const QString &logPath) {
  QSignalSpy initSpy(&client, &LspClient::initialized);
  if (!client.start(FAKE_LSP_SERVER_PATH, {"--log", logPath})) {
    return false;
  }
  return initSpy.wait(5000);
}

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

namespace {
QByteArray frame(const QByteArray &payload) {
  return QByteArray("Content-Length: ") + QByteArray::number(payload.size()) +
         "\r\n\r\n" + payload;
}
} // namespace

void TestLspRegression::testFramingWithNonAsciiPayload() {

  const QByteArray first =
      QString::fromUtf8(
          "{\"m\":\"did you mean \xE2\x80\x98string\xE2\x80\x99?\"}")
          .toUtf8();
  const QByteArray second = QByteArray("{\"m\":\"plain\"}");

  QByteArray buffer = frame(first) + frame(second);
  const QList<QByteArray> messages = LspClient::extractMessages(buffer);

  QCOMPARE(messages.size(), 2);
  QCOMPARE(messages.at(0), first);
  QCOMPARE(messages.at(1), second);
  QVERIFY(buffer.isEmpty());

  QJsonParseError err;
  QJsonDocument::fromJson(messages.at(0), &err);
  QCOMPARE(err.error, QJsonParseError::NoError);
}

void TestLspRegression::testFramingSplitAcrossReads() {
  const QByteArray payload =
      QString::fromUtf8("{\"arrow\":\"\xE2\x86\x92\"}").toUtf8();
  const QByteArray full = frame(payload);

  const int cut = full.size() - 2;
  QByteArray buffer = full.left(cut);
  QList<QByteArray> messages = LspClient::extractMessages(buffer);
  QCOMPARE(messages.size(), 0);

  buffer += full.mid(cut);
  messages = LspClient::extractMessages(buffer);
  QCOMPARE(messages.size(), 1);
  QCOMPARE(messages.at(0), payload);
  QVERIFY(buffer.isEmpty());
}

void TestLspRegression::testFramingMultipleMessagesInOneRead() {
  QByteArray buffer;
  QList<QByteArray> expected;
  for (int i = 0; i < 5; ++i) {
    const QByteArray payload =
        QByteArray("{\"id\":") + QByteArray::number(i) + "}";
    expected.append(payload);
    buffer += frame(payload);
  }
  buffer += "Content-Length: 40\r\n\r\n{\"partial\":";

  const QList<QByteArray> messages = LspClient::extractMessages(buffer);
  QCOMPARE(messages, expected);
  QVERIFY(buffer.startsWith("Content-Length: 40"));
}

void TestLspRegression::testNullLocationResultIsEmpty() {
  QVERIFY(LspClient::parseLocations(QJsonValue(QJsonValue::Null)).isEmpty());
  QVERIFY(LspClient::parseLocations(QJsonValue()).isEmpty());
  QVERIFY(LspClient::parseLocations(QJsonArray{}).isEmpty());
}

void TestLspRegression::testSingleLocationAndLocationLink() {
  QJsonObject range{{"start", QJsonObject{{"line", 3}, {"character", 4}}},
                    {"end", QJsonObject{{"line", 3}, {"character", 9}}}};
  QJsonObject location{{"uri", "file:///a.cpp"}, {"range", range}};

  QList<LspLocation> single = LspClient::parseLocations(location);
  QCOMPARE(single.size(), 1);
  QCOMPARE(single.first().uri, QString("file:///a.cpp"));
  QCOMPARE(single.first().range.start.line, 3);

  QJsonObject link{{"targetUri", "file:///b.cpp"},
                   {"targetRange", range},
                   {"targetSelectionRange", range}};
  QList<LspLocation> links = LspClient::parseLocations(QJsonArray{link});
  QCOMPARE(links.size(), 1);
  QCOMPARE(links.first().uri, QString("file:///b.cpp"));
  QCOMPARE(links.first().range.start.character, 4);
}

void TestLspRegression::testCompletionItemPrefersTextEdit() {
  QJsonObject obj;
  obj["label"] = "push_back";
  obj["insertText"] = "stale";
  obj["textEdit"] = QJsonObject{
      {"range",
       QJsonObject{{"start", QJsonObject{{"line", 0}, {"character", 0}}},
                   {"end", QJsonObject{{"line", 0}, {"character", 2}}}}},
      {"newText", "push_back(${1:value})"}};
  obj["insertTextFormat"] = 2;

  LspCompletionItem item = LspClient::parseCompletionItem(obj);
  QCOMPARE(item.insertText, QString("push_back(${1:value})"));
  QCOMPARE(item.insertTextFormat, 2);
}

void TestLspRegression::testCompletionItemInsertTextFormat() {
  QJsonObject obj;
  obj["label"] = "price$";
  LspCompletionItem item = LspClient::parseCompletionItem(obj);
  QCOMPARE(item.insertText, QString("price$"));
  QCOMPARE(item.insertTextFormat, 1);
}

void TestLspRegression::testServerRequestsAreAnswered() {
  QVERIFY(m_tempDir.isValid());
  const QString logPath = makeLogPath();
  LspClient client;
  QVERIFY(startFakeServer(client, logPath));

  QTRY_VERIFY_WITH_TIMEOUT(readLog(logPath).size() >= 4, 5000);
  const QStringList log = readLog(logPath);
  QVERIFY(log.contains("response cfg [null,null]"));
  QVERIFY(log.contains("response 77 null"));
  bool unknownRejected = false;
  for (const QString &line : log) {
    if (line.startsWith("response 78 ") && line.contains("-32601")) {
      unknownRejected = true;
    }
  }
  QVERIFY(unknownRejected);
  client.stop();
}

void TestLspRegression::testBurstOfMessagesIsFullyDrained() {
  QVERIFY(m_tempDir.isValid());
  LspClient client;
  QVERIFY(startFakeServer(client, makeLogPath()));

  QSignalSpy diagSpy(&client, &LspClient::diagnosticsReceived);
  client.didOpen("file:///burst.cpp", "cpp", 1, "BURST");
  QTRY_COMPARE_WITH_TIMEOUT(diagSpy.count(), 150, 5000);
  client.stop();
}

void TestLspRegression::testDiagnosticsCarryServerVersion() {
  QVERIFY(m_tempDir.isValid());
  LspClient client;
  QVERIFY(startFakeServer(client, makeLogPath()));

  QSignalSpy diagSpy(&client, &LspClient::diagnosticsReceived);
  client.didOpen("file:///v.cpp", "cpp", 7, "hello");
  QTRY_COMPARE_WITH_TIMEOUT(diagSpy.count(), 1, 5000);
  QCOMPARE(diagSpy.takeFirst().at(2).toInt(), 7);

  client.didChange("file:///v.cpp", 8, "NOVERSION");
  QTRY_COMPARE_WITH_TIMEOUT(diagSpy.count(), 1, 5000);
  QCOMPARE(diagSpy.takeFirst().at(2).toInt(), -1);
  client.stop();
}

void TestLspRegression::testPendingRequestsFailWhenServerExits() {
  QVERIFY(m_tempDir.isValid());
  LspClient client;
  QVERIFY(startFakeServer(client, makeLogPath()));

  QSignalSpy failedSpy(&client, &LspClient::requestFailed);
  client.didOpen("file:///c.cpp", "cpp", 1, "ok");
  client.requestHover("file:///c.cpp", {0, 0});
  client.didChange("file:///c.cpp", 2, "CRASH");
  QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() >= 1, 5000);
  QCOMPARE(failedSpy.first().at(1).toString(), QString("textDocument/hover"));
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), LspClient::State::Disconnected,
                            5000);
}

QTEST_MAIN(TestLspRegression)
#include "test_lspregression.moc"
