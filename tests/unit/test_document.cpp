#include "core/document.h"
#include "core/io/filemanager.h"
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestDocument : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testNewDocument();
  void testDocumentWithContent();
  void testLoadDocument();
  void testSaveDocument();
  void testSaveAsDocument();
  void testModificationState();
  void testLanguageHint();
  void testSignals();

private:
  QTemporaryDir m_tempDir;
  QString m_testFilePath;
};

void TestDocument::initTestCase() {
  QVERIFY(m_tempDir.isValid());

  // Create a test file
  m_testFilePath = m_tempDir.path() + "/test.cpp";
  QFile file(m_testFilePath);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  QTextStream stream(&file);
  stream << "#include <iostream>\nint main() { return 0; }\n";
  file.close();
}

void TestDocument::cleanupTestCase() {}

void TestDocument::testNewDocument() {
  Document doc;

  QVERIFY(doc.isNew());
  QVERIFY(!doc.isModified());
  QCOMPARE(doc.fileName(), QString("Untitled"));
  QVERIFY(doc.filePath().isEmpty());
  QCOMPARE(doc.state(), Document::State::New);
}

void TestDocument::testDocumentWithContent() {
  Document doc;
  doc.setContent("Hello, World!");

  QCOMPARE(doc.content(), QString("Hello, World!"));
  QVERIFY(doc.isModified());
}

void TestDocument::testLoadDocument() {
  Document doc(m_testFilePath);

  QVERIFY(!doc.isNew());
  QCOMPARE(doc.filePath(), m_testFilePath);
  QVERIFY(doc.content().contains("#include"));
  QCOMPARE(doc.state(), Document::State::Saved);
}

void TestDocument::testSaveDocument() {
  QString newFilePath = m_tempDir.path() + "/newdoc.txt";
  Document doc;
  doc.setFilePath(newFilePath);
  doc.setContent("Test save content");

  QVERIFY(doc.save());
  QVERIFY(QFile::exists(newFilePath));

  // Verify saved state
  QCOMPARE(doc.state(), Document::State::Saved);
  QVERIFY(!doc.isModified());
}

void TestDocument::testSaveAsDocument() {
  Document doc;
  doc.setContent("Content to save");

  QString newFilePath = m_tempDir.path() + "/saveas.txt";
  QVERIFY(doc.saveAs(newFilePath));

  QCOMPARE(doc.filePath(), newFilePath);
  QVERIFY(QFile::exists(newFilePath));
}

void TestDocument::testModificationState() {
  Document doc;

  // New document is not modified initially
  QVERIFY(!doc.isModified());

  // Setting content marks as modified
  doc.setContent("Modified");
  QVERIFY(doc.isModified());

  // Marking as saved clears modified state
  doc.markAsSaved();
  QVERIFY(!doc.isModified());
}

void TestDocument::testLanguageHint() {
  Document cppDoc;
  cppDoc.setFilePath("/path/to/file.cpp");
  QCOMPARE(cppDoc.languageHint(), QString("cpp"));

  Document pyDoc;
  pyDoc.setFilePath("/path/to/script.py");
  QCOMPARE(pyDoc.languageHint(), QString("py"));

  Document jsDoc;
  jsDoc.setFilePath("/path/to/app.js");
  QCOMPARE(jsDoc.languageHint(), QString("js"));

  Document unknownDoc;
  unknownDoc.setFilePath("/path/to/file.xyz");
  QCOMPARE(unknownDoc.languageHint(), QString("text"));
}

void TestDocument::testSignals() {
  Document doc;

  QSignalSpy contentSpy(&doc, &Document::contentChanged);
  QSignalSpy modifiedSpy(&doc, &Document::modificationStateChanged);
  QSignalSpy pathSpy(&doc, &Document::filePathChanged);

  doc.setContent("Test");
  QCOMPARE(contentSpy.count(), 1);
  QCOMPARE(modifiedSpy.count(), 1);

  doc.setFilePath("/some/path.txt");
  QCOMPARE(pathSpy.count(), 1);
}

QTEST_MAIN(TestDocument)
#include "test_document.moc"
