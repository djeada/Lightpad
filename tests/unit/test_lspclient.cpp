#include <QtTest/QtTest>
#include <QSignalSpy>

#include "lsp/lspclient.h"

/**
 * @brief Unit tests for LSP (Language Server Protocol) client
 * 
 * Tests the LSP client infrastructure including data structures
 * for signature help, document symbols, and rename operations.
 */
class TestLspClient : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Basic client tests
    void testLspClientInitialState();
    void testLspClientStateEnum();
    
    // Data structure tests - LspPosition
    void testLspPositionToJson();
    void testLspPositionFromJson();
    
    // Data structure tests - LspRange
    void testLspRangeToJson();
    void testLspRangeFromJson();
    
    // Data structure tests - Signature Help
    void testLspParameterInfo();
    void testLspSignatureInfo();
    void testLspSignatureHelp();
    
    // Data structure tests - Document Symbols
    void testLspSymbolKindValues();
    void testLspDocumentSymbol();
    void testLspDocumentSymbolNested();
    
    // Data structure tests - Rename/TextEdit
    void testLspTextEdit();
    void testLspWorkspaceEdit();
    void testLspWorkspaceEditMultipleFiles();
};

void TestLspClient::initTestCase()
{
    // No special initialization needed
}

void TestLspClient::cleanupTestCase()
{
    // No special cleanup needed
}

// =============================================================================
// Basic Client Tests
// =============================================================================

void TestLspClient::testLspClientInitialState()
{
    LspClient client;
    
    QCOMPARE(client.state(), LspClient::State::Disconnected);
    QVERIFY(!client.isReady());
}

void TestLspClient::testLspClientStateEnum()
{
    // Test that all states are distinct
    QVERIFY(LspClient::State::Disconnected != LspClient::State::Connecting);
    QVERIFY(LspClient::State::Connecting != LspClient::State::Initializing);
    QVERIFY(LspClient::State::Initializing != LspClient::State::Ready);
    QVERIFY(LspClient::State::Ready != LspClient::State::ShuttingDown);
    QVERIFY(LspClient::State::ShuttingDown != LspClient::State::Error);
}

// =============================================================================
// LspPosition Tests
// =============================================================================

void TestLspClient::testLspPositionToJson()
{
    LspPosition pos;
    pos.line = 10;
    pos.character = 25;
    
    QJsonObject json = pos.toJson();
    
    QCOMPARE(json["line"].toInt(), 10);
    QCOMPARE(json["character"].toInt(), 25);
}

void TestLspClient::testLspPositionFromJson()
{
    QJsonObject json;
    json["line"] = 42;
    json["character"] = 15;
    
    LspPosition pos = LspPosition::fromJson(json);
    
    QCOMPARE(pos.line, 42);
    QCOMPARE(pos.character, 15);
}

// =============================================================================
// LspRange Tests
// =============================================================================

void TestLspClient::testLspRangeToJson()
{
    LspRange range;
    range.start.line = 5;
    range.start.character = 0;
    range.end.line = 5;
    range.end.character = 20;
    
    QJsonObject json = range.toJson();
    
    QVERIFY(json.contains("start"));
    QVERIFY(json.contains("end"));
    QCOMPARE(json["start"].toObject()["line"].toInt(), 5);
    QCOMPARE(json["end"].toObject()["character"].toInt(), 20);
}

void TestLspClient::testLspRangeFromJson()
{
    QJsonObject startJson;
    startJson["line"] = 10;
    startJson["character"] = 5;
    
    QJsonObject endJson;
    endJson["line"] = 15;
    endJson["character"] = 30;
    
    QJsonObject rangeJson;
    rangeJson["start"] = startJson;
    rangeJson["end"] = endJson;
    
    LspRange range = LspRange::fromJson(rangeJson);
    
    QCOMPARE(range.start.line, 10);
    QCOMPARE(range.start.character, 5);
    QCOMPARE(range.end.line, 15);
    QCOMPARE(range.end.character, 30);
}

// =============================================================================
// Signature Help Data Structure Tests
// =============================================================================

void TestLspClient::testLspParameterInfo()
{
    LspParameterInfo param;
    param.label = "int x";
    param.documentation = "The x coordinate";
    
    QCOMPARE(param.label, QString("int x"));
    QCOMPARE(param.documentation, QString("The x coordinate"));
}

void TestLspClient::testLspSignatureInfo()
{
    LspSignatureInfo sig;
    sig.label = "void foo(int x, int y)";
    sig.documentation = "Function that does something";
    sig.activeParameter = 1;
    
    LspParameterInfo param1;
    param1.label = "int x";
    param1.documentation = "First parameter";
    sig.parameters.append(param1);
    
    LspParameterInfo param2;
    param2.label = "int y";
    param2.documentation = "Second parameter";
    sig.parameters.append(param2);
    
    QCOMPARE(sig.label, QString("void foo(int x, int y)"));
    QCOMPARE(sig.parameters.count(), 2);
    QCOMPARE(sig.activeParameter, 1);
    QCOMPARE(sig.parameters.at(0).label, QString("int x"));
    QCOMPARE(sig.parameters.at(1).label, QString("int y"));
}

void TestLspClient::testLspSignatureHelp()
{
    LspSignatureHelp help;
    help.activeSignature = 0;
    help.activeParameter = 2;
    
    LspSignatureInfo sig1;
    sig1.label = "void func(int a, int b, int c)";
    help.signatures.append(sig1);
    
    LspSignatureInfo sig2;
    sig2.label = "void func(double a)";
    help.signatures.append(sig2);
    
    QCOMPARE(help.signatures.count(), 2);
    QCOMPARE(help.activeSignature, 0);
    QCOMPARE(help.activeParameter, 2);
}

// =============================================================================
// Document Symbol Data Structure Tests
// =============================================================================

void TestLspClient::testLspSymbolKindValues()
{
    // Test that symbol kinds have expected values (from LSP spec)
    QCOMPARE(static_cast<int>(LspSymbolKind::File), 1);
    QCOMPARE(static_cast<int>(LspSymbolKind::Class), 5);
    QCOMPARE(static_cast<int>(LspSymbolKind::Method), 6);
    QCOMPARE(static_cast<int>(LspSymbolKind::Function), 12);
    QCOMPARE(static_cast<int>(LspSymbolKind::Variable), 13);
    QCOMPARE(static_cast<int>(LspSymbolKind::Struct), 23);
}

void TestLspClient::testLspDocumentSymbol()
{
    LspDocumentSymbol symbol;
    symbol.name = "MyClass";
    symbol.detail = "class MyClass";
    symbol.kind = LspSymbolKind::Class;
    symbol.range.start.line = 10;
    symbol.range.start.character = 0;
    symbol.range.end.line = 50;
    symbol.range.end.character = 1;
    symbol.selectionRange.start.line = 10;
    symbol.selectionRange.start.character = 6;
    symbol.selectionRange.end.line = 10;
    symbol.selectionRange.end.character = 13;
    
    QCOMPARE(symbol.name, QString("MyClass"));
    QCOMPARE(symbol.kind, LspSymbolKind::Class);
    QCOMPARE(symbol.range.start.line, 10);
    QCOMPARE(symbol.range.end.line, 50);
}

void TestLspClient::testLspDocumentSymbolNested()
{
    // Create a class with nested methods
    LspDocumentSymbol classSymbol;
    classSymbol.name = "Calculator";
    classSymbol.kind = LspSymbolKind::Class;
    classSymbol.range.start.line = 0;
    classSymbol.range.end.line = 100;
    
    LspDocumentSymbol method1;
    method1.name = "add";
    method1.kind = LspSymbolKind::Method;
    method1.range.start.line = 5;
    method1.range.end.line = 10;
    
    LspDocumentSymbol method2;
    method2.name = "subtract";
    method2.kind = LspSymbolKind::Method;
    method2.range.start.line = 12;
    method2.range.end.line = 17;
    
    classSymbol.children.append(method1);
    classSymbol.children.append(method2);
    
    QCOMPARE(classSymbol.children.count(), 2);
    QCOMPARE(classSymbol.children.at(0).name, QString("add"));
    QCOMPARE(classSymbol.children.at(1).name, QString("subtract"));
    QCOMPARE(classSymbol.children.at(0).kind, LspSymbolKind::Method);
}

// =============================================================================
// Rename/TextEdit Data Structure Tests
// =============================================================================

void TestLspClient::testLspTextEdit()
{
    LspTextEdit edit;
    edit.range.start.line = 5;
    edit.range.start.character = 10;
    edit.range.end.line = 5;
    edit.range.end.character = 20;
    edit.newText = "newVariableName";
    
    QCOMPARE(edit.newText, QString("newVariableName"));
    QCOMPARE(edit.range.start.line, 5);
    QCOMPARE(edit.range.start.character, 10);
}

void TestLspClient::testLspWorkspaceEdit()
{
    LspWorkspaceEdit wsEdit;
    
    // Add edits for one file
    LspTextEdit edit1;
    edit1.range.start.line = 10;
    edit1.range.start.character = 5;
    edit1.range.end.line = 10;
    edit1.range.end.character = 15;
    edit1.newText = "renamedVar";
    
    LspTextEdit edit2;
    edit2.range.start.line = 20;
    edit2.range.start.character = 5;
    edit2.range.end.line = 20;
    edit2.range.end.character = 15;
    edit2.newText = "renamedVar";
    
    QList<LspTextEdit> edits;
    edits.append(edit1);
    edits.append(edit2);
    
    wsEdit.changes["file:///path/to/file.cpp"] = edits;
    
    QVERIFY(wsEdit.changes.contains("file:///path/to/file.cpp"));
    QCOMPARE(wsEdit.changes["file:///path/to/file.cpp"].count(), 2);
}

void TestLspClient::testLspWorkspaceEditMultipleFiles()
{
    LspWorkspaceEdit wsEdit;
    
    // Edits for first file - simulate renaming in two places
    LspTextEdit edit1a;
    edit1a.newText = "newName";
    edit1a.range.start.line = 10;
    edit1a.range.start.character = 5;
    edit1a.range.end.line = 10;
    edit1a.range.end.character = 12;
    
    LspTextEdit edit1b;
    edit1b.newText = "newName";
    edit1b.range.start.line = 25;
    edit1b.range.start.character = 5;
    edit1b.range.end.line = 25;
    edit1b.range.end.character = 12;
    
    wsEdit.changes["file:///path/to/file1.cpp"].append(edit1a);
    wsEdit.changes["file:///path/to/file1.cpp"].append(edit1b);
    
    // Edits for second file
    LspTextEdit edit2;
    edit2.newText = "newName";
    edit2.range.start.line = 5;
    edit2.range.start.character = 10;
    edit2.range.end.line = 5;
    edit2.range.end.character = 17;
    wsEdit.changes["file:///path/to/file2.cpp"].append(edit2);
    
    // Edits for header file - simulate renaming in three places
    LspTextEdit edit3a;
    edit3a.newText = "newName";
    edit3a.range.start.line = 3;
    edit3a.range.start.character = 0;
    edit3a.range.end.line = 3;
    edit3a.range.end.character = 7;
    
    LspTextEdit edit3b;
    edit3b.newText = "newName";
    edit3b.range.start.line = 15;
    edit3b.range.start.character = 4;
    edit3b.range.end.line = 15;
    edit3b.range.end.character = 11;
    
    LspTextEdit edit3c;
    edit3c.newText = "newName";
    edit3c.range.start.line = 22;
    edit3c.range.start.character = 8;
    edit3c.range.end.line = 22;
    edit3c.range.end.character = 15;
    
    wsEdit.changes["file:///path/to/file1.h"].append(edit3a);
    wsEdit.changes["file:///path/to/file1.h"].append(edit3b);
    wsEdit.changes["file:///path/to/file1.h"].append(edit3c);
    
    QCOMPARE(wsEdit.changes.count(), 3);
    QCOMPARE(wsEdit.changes["file:///path/to/file1.cpp"].count(), 2);
    QCOMPARE(wsEdit.changes["file:///path/to/file2.cpp"].count(), 1);
    QCOMPARE(wsEdit.changes["file:///path/to/file1.h"].count(), 3);
}

QTEST_MAIN(TestLspClient)
#include "test_lspclient.moc"
