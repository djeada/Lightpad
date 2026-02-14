#include "core/io/filemanager.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestFileManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testSingletonInstance();
  void testReadFile();
  void testWriteFile();
  void testFileExists();
  void testGetFileExtension();
  void testGetFileName();
  void testGetDirectory();
  void testReadNonExistentFile();
  void testWriteToEmptyPath();

private:
  QTemporaryDir m_tempDir;
  QString m_testFilePath;
};

void TestFileManager::initTestCase() {
  QVERIFY(m_tempDir.isValid());

  m_testFilePath = m_tempDir.path() + "/test.txt";
  QFile file(m_testFilePath);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  QTextStream stream(&file);
  stream << "Test content\nLine 2\n";
  file.close();
}

void TestFileManager::cleanupTestCase() {}

void TestFileManager::testSingletonInstance() {
  FileManager &fm1 = FileManager::instance();
  FileManager &fm2 = FileManager::instance();
  QCOMPARE(&fm1, &fm2);
}

void TestFileManager::testReadFile() {
  FileManager &fm = FileManager::instance();
  auto result = fm.readFile(m_testFilePath);

  QVERIFY(result.success);
  QVERIFY(result.content.contains("Test content"));
  QVERIFY(result.errorMessage.isEmpty());
}

void TestFileManager::testWriteFile() {
  FileManager &fm = FileManager::instance();
  QString newFilePath = m_tempDir.path() + "/newfile.txt";
  QString content = "New file content";

  auto result = fm.writeFile(newFilePath, content);

  QVERIFY(result.success);
  QVERIFY(QFile::exists(newFilePath));

  auto readResult = fm.readFile(newFilePath);
  QCOMPARE(readResult.content, content);
}

void TestFileManager::testFileExists() {
  FileManager &fm = FileManager::instance();

  QVERIFY(fm.fileExists(m_testFilePath));
  QVERIFY(!fm.fileExists(m_tempDir.path() + "/nonexistent.txt"));
}

void TestFileManager::testGetFileExtension() {
  FileManager &fm = FileManager::instance();

  QCOMPARE(fm.getFileExtension("/path/to/file.txt"), QString("txt"));
  QCOMPARE(fm.getFileExtension("/path/to/file.cpp"), QString("cpp"));
  QCOMPARE(fm.getFileExtension("/path/to/file.tar.gz"), QString("tar.gz"));
}

void TestFileManager::testGetFileName() {
  FileManager &fm = FileManager::instance();

  QCOMPARE(fm.getFileName("/path/to/file.txt"), QString("file.txt"));
  QCOMPARE(fm.getFileName("/path/to/directory/"), QString(""));
}

void TestFileManager::testGetDirectory() {
  FileManager &fm = FileManager::instance();

  QString dir = fm.getDirectory("/path/to/file.txt");
  QCOMPARE(dir, QString("/path/to"));
}

void TestFileManager::testReadNonExistentFile() {
  FileManager &fm = FileManager::instance();
  auto result = fm.readFile(m_tempDir.path() + "/nonexistent.txt");

  QVERIFY(!result.success);
  QVERIFY(!result.errorMessage.isEmpty());
}

void TestFileManager::testWriteToEmptyPath() {
  FileManager &fm = FileManager::instance();
  auto result = fm.writeFile("", "content");

  QVERIFY(!result.success);
  QVERIFY(!result.errorMessage.isEmpty());
}

QTEST_MAIN(TestFileManager)
#include "test_filemanager.moc"
