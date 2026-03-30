#include "core/document.h"
#include "core/io/filemanager.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestDocumentRegression : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testReopenAfterSave();
  void testSaveRenameReopen();
  void testModifyAndDiscardByReload();
  void testSaveEmptyContent();
  void testSaveToNewDirectory();
  void testMultipleSaveAs();
  void testLanguageHintUnknownExtension();
  void testLanguageHintNoExtension();
  void testLanguageHintCaseSensitivity();
  void testContentChangeSignalMultipleTimes();
  void testModificationStateAfterMultipleEdits();
  void testFilePathWithSpecialCharacters();

private:
  QTemporaryDir m_tempDir;
};

void TestDocumentRegression::initTestCase() {
  QVERIFY(m_tempDir.isValid());
}

void TestDocumentRegression::cleanupTestCase() {}

void TestDocumentRegression::testReopenAfterSave() {
  QString filePath = m_tempDir.path() + "/reopen_test.txt";

  Document doc1;
  doc1.setFilePath(filePath);
  doc1.setContent("original content");
  QVERIFY(doc1.save());

  Document doc2(filePath);
  QCOMPARE(doc2.content(), QString("original content"));
  QCOMPARE(doc2.state(), Document::State::Saved);
  QVERIFY(!doc2.isModified());
}

void TestDocumentRegression::testSaveRenameReopen() {
  QString originalPath = m_tempDir.path() + "/original.txt";
  QString renamedPath = m_tempDir.path() + "/renamed.txt";

  Document doc;
  doc.setFilePath(originalPath);
  doc.setContent("test content for rename");
  QVERIFY(doc.save());

  QVERIFY(QFile::rename(originalPath, renamedPath));

  Document renamedDoc(renamedPath);
  QCOMPARE(renamedDoc.content(), QString("test content for rename"));
  QCOMPARE(renamedDoc.filePath(), renamedPath);
}

void TestDocumentRegression::testModifyAndDiscardByReload() {
  QString filePath = m_tempDir.path() + "/reload_test.txt";

  {
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "saved content";
    file.close();
  }

  Document doc(filePath);
  QCOMPARE(doc.content(), QString("saved content"));

  doc.setContent("modified content");
  QVERIFY(doc.isModified());

  doc.setFilePath(filePath);
  doc.load();
  QCOMPARE(doc.content(), QString("saved content"));
  QVERIFY(!doc.isModified());
}

void TestDocumentRegression::testSaveEmptyContent() {
  QString filePath = m_tempDir.path() + "/empty.txt";

  Document doc;
  doc.setFilePath(filePath);
  doc.setContent("");
  QVERIFY(doc.save());

  Document reloaded(filePath);
  QCOMPARE(reloaded.content(), QString(""));
}

void TestDocumentRegression::testSaveToNewDirectory() {
  QString subDir = m_tempDir.path() + "/subdir/nested";
  QDir dir;
  dir.mkpath(subDir);

  QString filePath = subDir + "/nested_file.txt";

  Document doc;
  doc.setFilePath(filePath);
  doc.setContent("nested content");
  QVERIFY(doc.save());
  QVERIFY(QFile::exists(filePath));
}

void TestDocumentRegression::testMultipleSaveAs() {
  Document doc;
  doc.setContent("movable content");

  QString path1 = m_tempDir.path() + "/saveas1.txt";
  QVERIFY(doc.saveAs(path1));
  QCOMPARE(doc.filePath(), path1);

  QString path2 = m_tempDir.path() + "/saveas2.txt";
  QVERIFY(doc.saveAs(path2));
  QCOMPARE(doc.filePath(), path2);

  QVERIFY(QFile::exists(path1));
  QVERIFY(QFile::exists(path2));
}

void TestDocumentRegression::testLanguageHintUnknownExtension() {
  Document doc;
  doc.setFilePath("/path/to/file.xyz");
  QCOMPARE(doc.languageHint(), QString("text"));
}

void TestDocumentRegression::testLanguageHintNoExtension() {
  Document doc;
  doc.setFilePath("/path/to/Makefile");
  QString hint = doc.languageHint();
  QVERIFY(!hint.isEmpty());
}

void TestDocumentRegression::testLanguageHintCaseSensitivity() {
  Document cppDoc;
  cppDoc.setFilePath("/path/to/file.CPP");
  QString cppHint = cppDoc.languageHint();

  Document pyDoc;
  pyDoc.setFilePath("/path/to/file.PY");
  QString pyHint = pyDoc.languageHint();

  QVERIFY(!cppHint.isEmpty());
  QVERIFY(!pyHint.isEmpty());
}

void TestDocumentRegression::testContentChangeSignalMultipleTimes() {
  Document doc;
  QSignalSpy contentSpy(&doc, &Document::contentChanged);

  doc.setContent("first");
  doc.setContent("second");
  doc.setContent("third");

  QCOMPARE(contentSpy.count(), 3);
}

void TestDocumentRegression::testModificationStateAfterMultipleEdits() {
  Document doc;

  QVERIFY(!doc.isModified());
  doc.setContent("edit1");
  QVERIFY(doc.isModified());

  doc.markAsSaved();
  QVERIFY(!doc.isModified());

  doc.setContent("edit2");
  QVERIFY(doc.isModified());

  doc.markAsSaved();
  QVERIFY(!doc.isModified());
}

void TestDocumentRegression::testFilePathWithSpecialCharacters() {
  QString filePath = m_tempDir.path() + "/file with spaces.txt";

  Document doc;
  doc.setFilePath(filePath);
  doc.setContent("content with spaces in path");
  QVERIFY(doc.save());
  QVERIFY(QFile::exists(filePath));

  Document reloaded(filePath);
  QCOMPARE(reloaded.content(), QString("content with spaces in path"));
}

QTEST_MAIN(TestDocumentRegression)
#include "test_documentregression.moc"
