#include "core/io/filemanager.h"
#include <QFile>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QtTest>
#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/resource.h>
#endif

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
  void testPythonWriteExpandsTabsToSpaces();
  void testShebangPythonWriteExpandsTabsToSpaces();
  void testNonPythonWritePreservesTabs();
  void testFailedWritePreservesExistingContents();
  void testReadDirectoryReportsAnError();

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

void TestFileManager::testPythonWriteExpandsTabsToSpaces() {
  FileManager &fm = FileManager::instance();
  QString path = m_tempDir.path() + "/script.py";

  auto result = fm.writeFile(path, "if True:\n\tprint('ok')\n");

  QVERIFY(result.success);
  auto readResult = fm.readFile(path);
  QCOMPARE(readResult.content, QString("if True:\n    print('ok')\n"));
  QVERIFY(!readResult.content.contains('\t'));
}

void TestFileManager::testShebangPythonWriteExpandsTabsToSpaces() {
  FileManager &fm = FileManager::instance();
  QString path = m_tempDir.path() + "/script";

  auto result =
      fm.writeFile(path, "#!/usr/bin/env python3\nif True:\n\tprint('ok')\n");

  QVERIFY(result.success);
  auto readResult = fm.readFile(path);
  QCOMPARE(readResult.content,
           QString("#!/usr/bin/env python3\nif True:\n    print('ok')\n"));
  QVERIFY(!readResult.content.contains('\t'));
}

void TestFileManager::testNonPythonWritePreservesTabs() {
  FileManager &fm = FileManager::instance();
  QString path = m_tempDir.path() + "/notes.txt";

  auto result = fm.writeFile(path, "a\tb\n");

  QVERIFY(result.success);
  auto readResult = fm.readFile(path);
  QCOMPARE(readResult.content, QString("a\tb\n"));
}

void TestFileManager::testFailedWritePreservesExistingContents() {
#ifndef Q_OS_UNIX
  QSKIP("Write failure injection uses the Unix file size limit");
#else
  const QString path = m_tempDir.filePath("failed-write.txt");
  auto &manager = FileManager::instance();
  QVERIFY(manager.writeFile(path, "original contents").success);
  QSignalSpy saved(&manager, &FileManager::fileSaved);
  QSignalSpy errors(&manager, &FileManager::fileError);
  struct rlimit previous;
  QVERIFY(getrlimit(RLIMIT_FSIZE, &previous) == 0);
  const auto previousHandler = std::signal(SIGXFSZ, SIG_IGN);
  QVERIFY(previousHandler != SIG_ERR);
  auto restore = qScopeGuard([&]() {
    setrlimit(RLIMIT_FSIZE, &previous);
    std::signal(SIGXFSZ, previousHandler);
  });
  struct rlimit limited = previous;
  limited.rlim_cur = qMin<rlim_t>(1024, previous.rlim_max);
  QVERIFY(setrlimit(RLIMIT_FSIZE, &limited) == 0);

  const auto result = manager.writeFile(path, QString(8192, 'x'));
  QVERIFY(!result.success);
  QVERIFY(!result.errorMessage.isEmpty());
  QCOMPARE(saved.count(), 0);
  QCOMPARE(errors.count(), 1);
  QCOMPARE(manager.readFile(path).content, QString("original contents"));
#endif
}

void TestFileManager::testReadDirectoryReportsAnError() {
  auto &manager = FileManager::instance();
  QSignalSpy opened(&manager, &FileManager::fileOpened);
  QSignalSpy errors(&manager, &FileManager::fileError);
  const auto result = manager.readFile(m_tempDir.path());
  QVERIFY(!result.success);
  QVERIFY(!result.errorMessage.isEmpty());
  QCOMPARE(opened.count(), 0);
  QCOMPARE(errors.count(), 1);
}

QTEST_MAIN(TestFileManager)
#include "test_filemanager.moc"
