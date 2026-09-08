#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "filetree/filedirtreemodel.h"

class TestFileDirTreeModel : public QObject {
  Q_OBJECT

private slots:
  void init();

  void testCreateFileWithNestedPath();
  void testCreateRejectsExistingName();
  void testCreateFolderReportsItsPath();
  void testCopyKeepsHiddenFiles();
  void testRemoveDeletesHiddenFiles();
  void testRemoveDoesNotFollowSymlinks();
  void testCopyRecreatesSymlinkRatherThanFollowingIt();
  void testIsInsideComparesWholeComponents();
  void testMoveRefusesToNestFolderInItself();
  void testMoveIntoAnotherFolder();
  void testMoveIntoSameFolderIsANoOp();
  void testCopyIntoAddsSuffixOnCollision();
  void testDuplicateWorksForDirectories();
  void testClipboardRoundTripCopy();
  void testClipboardRoundTripCut();

private:
  QString path(const QString &relative) const;
  QString makeFile(const QString &relative, const QString &contents = "x");
  QString makeDir(const QString &relative);

  QTemporaryDir m_tempDir;
};

QString TestFileDirTreeModel::path(const QString &relative) const {
  return m_tempDir.path() + "/" + relative;
}

QString TestFileDirTreeModel::makeFile(const QString &relative,
                                       const QString &contents) {
  const QString absolute = path(relative);
  QDir().mkpath(QFileInfo(absolute).absolutePath());
  QFile file(absolute);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(contents.toUtf8());
    file.close();
  }
  return absolute;
}

QString TestFileDirTreeModel::makeDir(const QString &relative) {
  const QString absolute = path(relative);
  QDir().mkpath(absolute);
  return absolute;
}

void TestFileDirTreeModel::init() { QVERIFY(m_tempDir.isValid()); }

void TestFileDirTreeModel::testCreateFileWithNestedPath() {
  FileDirTreeModel model;
  const QString root = makeDir("nested-root");

  QString created;
  QVERIFY(model.createNewFile(root, "ui/widgets/panel.cpp", &created));

  QCOMPARE(created, root + "/ui/widgets/panel.cpp");
  QVERIFY(QFileInfo::exists(created));
}

void TestFileDirTreeModel::testCreateRejectsExistingName() {
  FileDirTreeModel model;
  const QString root = makeDir("existing-root");
  makeFile("existing-root/taken.txt");

  QSignalSpy errors(&model, &FileDirTreeModel::errorOccurred);
  QVERIFY(!model.createNewFile(root, "taken.txt"));
  QCOMPARE(errors.count(), 1);
  QVERIFY(errors.first().first().toString().contains("taken.txt"));
}

void TestFileDirTreeModel::testCreateFolderReportsItsPath() {
  FileDirTreeModel model;
  const QString root = makeDir("folder-root");

  QString created;
  QVERIFY(model.createNewDirectory(root, "assets", &created));
  QCOMPARE(created, root + "/assets");
  QVERIFY(QFileInfo(created).isDir());
}

void TestFileDirTreeModel::testCopyKeepsHiddenFiles() {
  FileDirTreeModel model;
  makeDir("hidden-src");
  makeFile("hidden-src/.gitignore", "build/\n");
  makeFile("hidden-src/visible.txt");
  const QString destination = makeDir("hidden-dest");

  QString created;
  QVERIFY(model.copyInto(path("hidden-src"), destination, &created));

  QVERIFY(QFileInfo::exists(created + "/.gitignore"));
  QVERIFY(QFileInfo::exists(created + "/visible.txt"));
}

void TestFileDirTreeModel::testRemoveDeletesHiddenFiles() {
  FileDirTreeModel model;
  const QString target = makeDir("remove-me");
  makeFile("remove-me/.hidden", "secret");

  QVERIFY(model.removeFileOrDirectory(target, DeleteMode::Permanent));
  QVERIFY(!QFileInfo::exists(target));
}

void TestFileDirTreeModel::testRemoveDoesNotFollowSymlinks() {
#ifdef Q_OS_WIN
  QSKIP("Symlink creation needs elevation on Windows");
#endif
  FileDirTreeModel model;
  const QString outside = makeDir("outside-tree");
  const QString treasure = makeFile("outside-tree/treasure.txt", "keep me");
  const QString container = makeDir("link-container");
  QVERIFY(QFile::link(outside, container + "/link-to-outside"));

  QVERIFY(model.removeFileOrDirectory(container, DeleteMode::Permanent));

  QVERIFY(!QFileInfo::exists(container));

  QVERIFY(QFileInfo::exists(treasure));
}

void TestFileDirTreeModel::testCopyRecreatesSymlinkRatherThanFollowingIt() {
#ifdef Q_OS_WIN
  QSKIP("Symlink creation needs elevation on Windows");
#endif
  FileDirTreeModel model;
  const QString outside = makeDir("copy-outside");
  makeFile("copy-outside/big.bin", "lots of data");
  const QString source = makeDir("copy-src");
  QVERIFY(QFile::link(outside, source + "/link"));
  const QString destination = makeDir("copy-dest");

  QString created;
  QVERIFY(model.copyInto(source, destination, &created));

  const QFileInfo copiedLink(created + "/link");
  QVERIFY(copiedLink.isSymLink());
  QVERIFY(!QFileInfo::exists(created + "/link/big.bin") ||
          copiedLink.isSymLink());
}

void TestFileDirTreeModel::testIsInsideComparesWholeComponents() {
  QVERIFY(FileDirTreeModel::isInside("/a/b", "/a/b"));
  QVERIFY(FileDirTreeModel::isInside("/a/b", "/a/b/c"));

  QVERIFY(!FileDirTreeModel::isInside("/a/b", "/a/bc"));
  QVERIFY(!FileDirTreeModel::isInside("/a/b", "/a"));
}

void TestFileDirTreeModel::testMoveRefusesToNestFolderInItself() {
  FileDirTreeModel model;
  const QString outer = makeDir("outer");
  const QString inner = makeDir("outer/inner");

  QSignalSpy errors(&model, &FileDirTreeModel::errorOccurred);
  QVERIFY(!model.moveInto(outer, inner));
  QCOMPARE(errors.count(), 1);
  QVERIFY(QFileInfo(outer).isDir());
  QVERIFY(QFileInfo(inner).isDir());
}

void TestFileDirTreeModel::testMoveIntoAnotherFolder() {
  FileDirTreeModel model;
  const QString source = makeFile("move-src/note.txt", "content");
  const QString destination = makeDir("move-dest");

  QString created;
  QVERIFY(model.moveInto(source, destination, &created));

  QCOMPARE(created, destination + "/note.txt");
  QVERIFY(QFileInfo::exists(created));
  QVERIFY(!QFileInfo::exists(source));
}

void TestFileDirTreeModel::testMoveIntoSameFolderIsANoOp() {
  FileDirTreeModel model;
  const QString source = makeFile("same-folder/note.txt");

  QSignalSpy errors(&model, &FileDirTreeModel::errorOccurred);
  QString created;
  QVERIFY(model.moveInto(source, path("same-folder"), &created));

  QCOMPARE(created, source);
  QCOMPARE(errors.count(), 0);
  QVERIFY(!QFileInfo::exists(path("same-folder/note (1).txt")));
}

void TestFileDirTreeModel::testCopyIntoAddsSuffixOnCollision() {
  FileDirTreeModel model;
  const QString source = makeFile("collide-src/note.txt", "a");
  const QString destination = makeDir("collide-dest");
  makeFile("collide-dest/note.txt", "b");

  QString created;
  QVERIFY(model.copyInto(source, destination, &created));

  QCOMPARE(created, destination + "/note (1).txt");
  QVERIFY(QFileInfo::exists(destination + "/note.txt"));
}

void TestFileDirTreeModel::testDuplicateWorksForDirectories() {
  FileDirTreeModel model;
  const QString source = makeDir("dup-src");
  makeFile("dup-src/inner.txt");

  QString created;
  QVERIFY(model.duplicateEntry(source, &created));

  QVERIFY(QFileInfo(created).isDir());
  QVERIFY(QFileInfo::exists(created + "/inner.txt"));
}

void TestFileDirTreeModel::testClipboardRoundTripCopy() {
  FileDirTreeModel model;
  const QString source = makeFile("clip-copy/note.txt", "payload");
  const QString destination = makeDir("clip-copy-dest");

  QVERIFY(model.copyToClipboard(QStringList{source}));
  QVERIFY(model.canPaste());

  QStringList created;
  QVERIFY(model.pasteFromClipboard(destination, &created));

  QCOMPARE(created.size(), 1);
  QVERIFY(QFileInfo::exists(destination + "/note.txt"));

  QVERIFY(QFileInfo::exists(source));
}

void TestFileDirTreeModel::testClipboardRoundTripCut() {
  FileDirTreeModel model;
  const QString source = makeFile("clip-cut/note.txt", "payload");
  const QString destination = makeDir("clip-cut-dest");

  QVERIFY(model.cutToClipboard(QStringList{source}));

  QStringList created;
  QVERIFY(model.pasteFromClipboard(destination, &created));

  QVERIFY(QFileInfo::exists(destination + "/note.txt"));
  QVERIFY(!QFileInfo::exists(source));

  QVERIFY(!model.canPaste());
}

QTEST_MAIN(TestFileDirTreeModel)
#include "test_filedirtreemodel.moc"
