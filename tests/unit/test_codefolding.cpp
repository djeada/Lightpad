#include "core/editor/codefolding.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QTextDocument>
#include <QtTest/QtTest>

class TestCodeFolding : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testInitialState();
  void testIsFoldableBraceStyle();
  void testIsFoldableIndentStyle();
  void testIsFoldableRegion();
  void testNotFoldableSingleLine();
  void testFoldBlock();
  void testUnfoldBlock();
  void testToggleFoldAtLine();
  void testFoldAll();
  void testUnfoldAll();
  void testFoldToLevel();
  void testFindFoldEndBlockBrace();
  void testFindFoldEndBlockIndent();
  void testGetFoldingLevel();
  void testIsRegionStart();
  void testIsRegionEnd();
  void testFindRegionEndBlock();
  void testIsCommentBlockStart();
  void testFindCommentBlockEnd();
  void testFoldComments();
  void testUnfoldComments();
  void testSaveFoldState();
  void testRestoreFoldState();
  void testNullDocument();
  void testFoldBlockAlreadyFolded();

private:
  QTextDocument *m_document = nullptr;
  CodeFoldingManager *m_manager = nullptr;
};

void TestCodeFolding::init() {
  m_document = new QTextDocument();
  m_manager = new CodeFoldingManager(m_document);
}

void TestCodeFolding::cleanup() {
  delete m_manager;
  delete m_document;
  m_manager = nullptr;
  m_document = nullptr;
}

void TestCodeFolding::testInitialState() {
  QVERIFY(m_manager->foldedBlocks().isEmpty());
  QVERIFY(!m_manager->isFolded(0));
}

void TestCodeFolding::testIsFoldableBraceStyle() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  QVERIFY(m_manager->isFoldable(0));
  QVERIFY(!m_manager->isFoldable(1));
}

void TestCodeFolding::testIsFoldableIndentStyle() {
  m_document->setPlainText("def foo():\n    pass\n    return\n");

  QVERIFY(m_manager->isFoldable(0));
}

void TestCodeFolding::testIsFoldableRegion() {
  m_document->setPlainText("#region MyRegion\nint x = 1;\n#endregion\n");

  QVERIFY(m_manager->isFoldable(0));
}

void TestCodeFolding::testNotFoldableSingleLine() {
  m_document->setPlainText("int x = 5;\n");

  QVERIFY(!m_manager->isFoldable(0));
}

void TestCodeFolding::testFoldBlock() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  QVERIFY(m_manager->foldBlock(0));
  QVERIFY(m_manager->isFolded(0));
  QCOMPARE(m_manager->foldedBlocks().size(), 1);
}

void TestCodeFolding::testUnfoldBlock() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  m_manager->foldBlock(0);
  QVERIFY(m_manager->isFolded(0));

  QVERIFY(m_manager->unfoldBlock(0));
  QVERIFY(!m_manager->isFolded(0));
  QVERIFY(m_manager->foldedBlocks().isEmpty());
}

void TestCodeFolding::testToggleFoldAtLine() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  m_manager->toggleFoldAtLine(0);
  QVERIFY(m_manager->isFolded(0));

  m_manager->toggleFoldAtLine(0);
  QVERIFY(!m_manager->isFolded(0));
}

void TestCodeFolding::testFoldAll() {
  m_document->setPlainText(
      "void foo() {\n    int x;\n}\nvoid bar() {\n    int y;\n}\n");

  m_manager->foldAll();

  QVERIFY(m_manager->isFolded(0));
  QVERIFY(m_manager->isFolded(3));
}

void TestCodeFolding::testUnfoldAll() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  m_manager->foldBlock(0);
  QVERIFY(!m_manager->foldedBlocks().isEmpty());

  m_manager->unfoldAll();
  QVERIFY(m_manager->foldedBlocks().isEmpty());
}

void TestCodeFolding::testFoldToLevel() {
  m_document->setPlainText(
      "void foo() {\n    if (true) {\n        x;\n    }\n}\n");

  m_manager->foldToLevel(1);

  QVERIFY(m_manager->foldedBlocks().size() >= 1);
}

void TestCodeFolding::testFindFoldEndBlockBrace() {
  m_document->setPlainText("int main() {\n    int x;\n    int y;\n}\n");

  int endBlock = m_manager->findFoldEndBlock(0);
  QCOMPARE(endBlock, 3);
}

void TestCodeFolding::testFindFoldEndBlockIndent() {
  m_document->setPlainText("def foo():\n    x = 1\n    y = 2\nz = 3\n");

  int endBlock = m_manager->findFoldEndBlock(0);
  QVERIFY(endBlock >= 1);
  QVERIFY(endBlock <= 2);
}

void TestCodeFolding::testGetFoldingLevel() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  int level0 = m_manager->getFoldingLevel(0);
  int level1 = m_manager->getFoldingLevel(1);

  QVERIFY(level1 >= level0);
}

void TestCodeFolding::testIsRegionStart() {
  m_document->setPlainText("#region TestRegion\ncode;\n#endregion\n");

  QVERIFY(m_manager->isRegionStart(0));
  QVERIFY(!m_manager->isRegionStart(1));
}

void TestCodeFolding::testIsRegionEnd() {
  m_document->setPlainText("#region TestRegion\ncode;\n#endregion\n");

  QVERIFY(!m_manager->isRegionEnd(0));
  QVERIFY(m_manager->isRegionEnd(2));
}

void TestCodeFolding::testFindRegionEndBlock() {
  m_document->setPlainText(
      "#region Outer\ncode;\n#region Inner\ncode;\n#endregion\n#endregion\n");

  int endBlock = m_manager->findRegionEndBlock(0);
  QCOMPARE(endBlock, 5);
}

void TestCodeFolding::testIsCommentBlockStart() {
  m_document->setPlainText("/* Multi-line\n * comment\n */\ncode;\n");

  QVERIFY(m_manager->isCommentBlockStart(0));
  QVERIFY(!m_manager->isCommentBlockStart(3));
}

void TestCodeFolding::testFindCommentBlockEnd() {
  m_document->setPlainText("/* start\n * middle\n */\ncode;\n");

  int endBlock = m_manager->findCommentBlockEnd(0);
  QCOMPARE(endBlock, 2);
}

void TestCodeFolding::testFoldComments() {
  m_document->setPlainText("/* Multi-line\n * comment\n */\ncode;\n");

  m_manager->foldComments();

  QVERIFY(m_manager->isFolded(0));
  QVERIFY(!m_manager->isFolded(3));
}

void TestCodeFolding::testUnfoldComments() {
  m_document->setPlainText("/* Multi-line\n * comment\n */\ncode;\n");

  m_manager->foldComments();
  QVERIFY(m_manager->isFolded(0));

  m_manager->unfoldComments();
  QVERIFY(!m_manager->isFolded(0));
}

void TestCodeFolding::testSaveFoldState() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  m_manager->foldBlock(0);

  QJsonObject state = m_manager->saveFoldState();
  QVERIFY(state.contains("foldedBlocks"));

  QJsonArray foldedArray = state["foldedBlocks"].toArray();
  QCOMPARE(foldedArray.size(), 1);
  QCOMPARE(foldedArray[0].toInt(), 0);
}

void TestCodeFolding::testRestoreFoldState() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  m_manager->foldBlock(0);
  QJsonObject state = m_manager->saveFoldState();

  m_manager->unfoldAll();
  QVERIFY(!m_manager->isFolded(0));

  m_manager->restoreFoldState(state);
  QVERIFY(m_manager->isFolded(0));
}

void TestCodeFolding::testNullDocument() {
  CodeFoldingManager nullMgr(nullptr);
  QVERIFY(!nullMgr.isFoldable(0));
  QVERIFY(!nullMgr.foldBlock(0));
  QVERIFY(!nullMgr.unfoldBlock(0));
  QCOMPARE(nullMgr.getFoldingLevel(0), 0);
  QCOMPARE(nullMgr.findFoldEndBlock(0), 0);
}

void TestCodeFolding::testFoldBlockAlreadyFolded() {
  m_document->setPlainText("int main() {\n    return 0;\n}\n");

  QVERIFY(m_manager->foldBlock(0));
  QVERIFY(!m_manager->foldBlock(0));
}

QTEST_MAIN(TestCodeFolding)
#include "test_codefolding.moc"
