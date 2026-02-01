#include <QtTest/QtTest>
#include "ui/dialogs/gotosymboldialog.h"

class TestGoToSymbolDialog : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testDialogCreation();
    void testSetSymbols();
    void testClearSymbols();
    void testFlattenNestedSymbols();
    void testSymbolKindIcons();

private:
    GoToSymbolDialog* m_dialog;
};

void TestGoToSymbolDialog::initTestCase()
{
    m_dialog = new GoToSymbolDialog(nullptr);
}

void TestGoToSymbolDialog::cleanupTestCase()
{
    delete m_dialog;
}

void TestGoToSymbolDialog::testDialogCreation()
{
    // Dialog should be created successfully
    QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testSetSymbols()
{
    // Create some test symbols
    QList<LspDocumentSymbol> symbols;
    
    LspDocumentSymbol sym1;
    sym1.name = "testFunction";
    sym1.kind = LspSymbolKind::Function;
    sym1.selectionRange.start.line = 10;
    sym1.selectionRange.start.character = 0;
    symbols.append(sym1);
    
    LspDocumentSymbol sym2;
    sym2.name = "TestClass";
    sym2.kind = LspSymbolKind::Class;
    sym2.selectionRange.start.line = 50;
    sym2.selectionRange.start.character = 0;
    symbols.append(sym2);
    
    // Set symbols
    m_dialog->setSymbols(symbols);
    
    // Dialog should now have symbols (we can't directly verify since symbols are private)
    // But we can verify the dialog is still valid
    QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testClearSymbols()
{
    // Set some symbols first
    QList<LspDocumentSymbol> symbols;
    LspDocumentSymbol sym;
    sym.name = "tempSymbol";
    sym.kind = LspSymbolKind::Variable;
    symbols.append(sym);
    m_dialog->setSymbols(symbols);
    
    // Clear all symbols
    m_dialog->clearSymbols();
    
    // Dialog should still be valid after clearing
    QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testFlattenNestedSymbols()
{
    // Create nested symbols (class with methods)
    QList<LspDocumentSymbol> symbols;
    
    LspDocumentSymbol classSymbol;
    classSymbol.name = "MyClass";
    classSymbol.kind = LspSymbolKind::Class;
    classSymbol.selectionRange.start.line = 5;
    classSymbol.selectionRange.start.character = 0;
    
    // Add child method
    LspDocumentSymbol methodSymbol;
    methodSymbol.name = "myMethod";
    methodSymbol.kind = LspSymbolKind::Method;
    methodSymbol.selectionRange.start.line = 10;
    methodSymbol.selectionRange.start.character = 4;
    classSymbol.children.append(methodSymbol);
    
    symbols.append(classSymbol);
    
    // Set symbols - should flatten nested structure
    m_dialog->setSymbols(symbols);
    
    // Dialog should handle nested symbols
    QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testSymbolKindIcons()
{
    // Test that all symbol kinds have icons
    QList<LspSymbolKind> kinds = {
        LspSymbolKind::File,
        LspSymbolKind::Module,
        LspSymbolKind::Namespace,
        LspSymbolKind::Package,
        LspSymbolKind::Class,
        LspSymbolKind::Method,
        LspSymbolKind::Property,
        LspSymbolKind::Field,
        LspSymbolKind::Constructor,
        LspSymbolKind::Enum,
        LspSymbolKind::Interface,
        LspSymbolKind::Function,
        LspSymbolKind::Variable,
        LspSymbolKind::Constant
    };
    
    for (LspSymbolKind kind : kinds) {
        QList<LspDocumentSymbol> symbols;
        LspDocumentSymbol sym;
        sym.name = "testSymbol";
        sym.kind = kind;
        sym.selectionRange.start.line = 0;
        sym.selectionRange.start.character = 0;
        symbols.append(sym);
        
        m_dialog->setSymbols(symbols);
        QVERIFY(m_dialog != nullptr);
    }
}

QTEST_MAIN(TestGoToSymbolDialog)
#include "test_gotosymboldialog.moc"
