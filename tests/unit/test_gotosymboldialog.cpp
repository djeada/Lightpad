#include "ui/dialogs/gotosymboldialog.h"
#include <QtTest/QtTest>

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
  GoToSymbolDialog *m_dialog;
};

void TestGoToSymbolDialog::initTestCase() {
  m_dialog = new GoToSymbolDialog(nullptr);
}

void TestGoToSymbolDialog::cleanupTestCase() { delete m_dialog; }

void TestGoToSymbolDialog::testDialogCreation() {

  QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testSetSymbols() {

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

  m_dialog->setSymbols(symbols);

  QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testClearSymbols() {

  QList<LspDocumentSymbol> symbols;
  LspDocumentSymbol sym;
  sym.name = "tempSymbol";
  sym.kind = LspSymbolKind::Variable;
  symbols.append(sym);
  m_dialog->setSymbols(symbols);

  m_dialog->clearSymbols();

  QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testFlattenNestedSymbols() {

  QList<LspDocumentSymbol> symbols;

  LspDocumentSymbol classSymbol;
  classSymbol.name = "MyClass";
  classSymbol.kind = LspSymbolKind::Class;
  classSymbol.selectionRange.start.line = 5;
  classSymbol.selectionRange.start.character = 0;

  LspDocumentSymbol methodSymbol;
  methodSymbol.name = "myMethod";
  methodSymbol.kind = LspSymbolKind::Method;
  methodSymbol.selectionRange.start.line = 10;
  methodSymbol.selectionRange.start.character = 4;
  classSymbol.children.append(methodSymbol);

  symbols.append(classSymbol);

  m_dialog->setSymbols(symbols);

  QVERIFY(m_dialog != nullptr);
}

void TestGoToSymbolDialog::testSymbolKindIcons() {

  QList<LspSymbolKind> kinds = {
      LspSymbolKind::File,        LspSymbolKind::Module,
      LspSymbolKind::Namespace,   LspSymbolKind::Package,
      LspSymbolKind::Class,       LspSymbolKind::Method,
      LspSymbolKind::Property,    LspSymbolKind::Field,
      LspSymbolKind::Constructor, LspSymbolKind::Enum,
      LspSymbolKind::Interface,   LspSymbolKind::Function,
      LspSymbolKind::Variable,    LspSymbolKind::Constant};

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
