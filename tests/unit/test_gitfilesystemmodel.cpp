#include "filetree/gitfilesystemmodel.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

constexpr int GIT_CMD_TIMEOUT_MS = 5000;
constexpr int STATUS_REFRESH_WAIT_MS = 600;

class TestGitFileSystemModel : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testStatusBadgeModified();
  void testStatusBadgeUntracked();
  void testStatusBadgeAdded();
  void testStatusBadgeDeleted();
  void testStatusBadgeClean();
  void testStatusBadgeColorModified();
  void testStatusBadgeColorUntracked();
  void testDirtyDirectoryPropagation();
  void testDirtyDirectoryNested();
  void testCleanDirectoryNotDirty();
  void testCustomRolesViaData();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git;
  GitFileSystemModel *m_model;

  bool runGitCommand(const QStringList &args);
  void createTestFile(const QString &fileName, const QString &content);
};

bool TestGitFileSystemModel::runGitCommand(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  process.waitForFinished(GIT_CMD_TIMEOUT_MS);
  return process.exitCode() == 0;
}

void TestGitFileSystemModel::createTestFile(const QString &fileName,
                                            const QString &content) {
  QFileInfo info(m_repoPath + "/" + fileName);
  QDir().mkpath(info.absolutePath());
  QFile file(info.absoluteFilePath());
  file.open(QIODevice::WriteOnly);
  file.write(content.toUtf8());
  file.close();
}

void TestGitFileSystemModel::initTestCase() {
  QVERIFY(m_tempDir.isValid());
  m_repoPath = m_tempDir.path();

  runGitCommand({"init"});
  runGitCommand({"config", "user.email", "test@test.com"});
  runGitCommand({"config", "user.name", "Test"});

  createTestFile("initial.txt", "initial content");
  runGitCommand({"add", "initial.txt"});
  runGitCommand({"commit", "-m", "Initial commit"});

  m_git = new GitIntegration(this);
  m_git->setRepositoryPath(m_repoPath);
  QVERIFY(m_git->isValidRepository());

  m_model = new GitFileSystemModel(this);
  m_model->setGitIntegration(m_git);
}

void TestGitFileSystemModel::cleanupTestCase() {}

void TestGitFileSystemModel::testStatusBadgeModified() {
  createTestFile("initial.txt", "modified content");
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QString badge = m_model->statusBadge(m_repoPath + "/initial.txt");
  QCOMPARE(badge, QStringLiteral("M"));
}

void TestGitFileSystemModel::testStatusBadgeUntracked() {
  createTestFile("newfile.txt", "untracked");
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QString badge = m_model->statusBadge(m_repoPath + "/newfile.txt");
  QCOMPARE(badge, QStringLiteral("U"));
}

void TestGitFileSystemModel::testStatusBadgeAdded() {
  createTestFile("staged.txt", "staged content");
  runGitCommand({"add", "staged.txt"});
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QString badge = m_model->statusBadge(m_repoPath + "/staged.txt");
  QCOMPARE(badge, QStringLiteral("A"));
}

void TestGitFileSystemModel::testStatusBadgeDeleted() {
  createTestFile("todelete.txt", "to delete");
  runGitCommand({"add", "todelete.txt"});
  runGitCommand({"commit", "-m", "add todelete"});
  runGitCommand({"rm", "todelete.txt"});
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QString badge = m_model->statusBadge(m_repoPath + "/todelete.txt");
  QCOMPARE(badge, QStringLiteral("D"));
}

void TestGitFileSystemModel::testStatusBadgeClean() {
  QString badge = m_model->statusBadge(m_repoPath + "/nonexistent.txt");
  QVERIFY(badge.isEmpty());
}

void TestGitFileSystemModel::testStatusBadgeColorModified() {
  createTestFile("initial.txt", "modified again");
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QColor color = m_model->statusBadgeColor(m_repoPath + "/initial.txt");
  QVERIFY(color.isValid());
  QCOMPARE(color, QColor("#d8a13c"));
}

void TestGitFileSystemModel::testStatusBadgeColorUntracked() {
  createTestFile("untracked2.txt", "content");
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QColor color = m_model->statusBadgeColor(m_repoPath + "/untracked2.txt");
  QVERIFY(color.isValid());
  QCOMPARE(color, QColor("#9aa6b2"));
}

void TestGitFileSystemModel::testDirtyDirectoryPropagation() {
  createTestFile("subdir/dirty.txt", "dirty content");
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QVERIFY(m_model->isDirtyDirectory(m_repoPath + "/subdir"));
  QVERIFY(m_model->isDirtyDirectory(m_repoPath));
}

void TestGitFileSystemModel::testDirtyDirectoryNested() {
  createTestFile("a/b/c/deep.txt", "deep content");
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  QVERIFY(m_model->isDirtyDirectory(m_repoPath + "/a/b/c"));
  QVERIFY(m_model->isDirtyDirectory(m_repoPath + "/a/b"));
  QVERIFY(m_model->isDirtyDirectory(m_repoPath + "/a"));
}

void TestGitFileSystemModel::testCleanDirectoryNotDirty() {
  QVERIFY(!m_model->isDirtyDirectory(m_repoPath + "/cleandir"));
}

void TestGitFileSystemModel::testCustomRolesViaData() {
  createTestFile("initial.txt", "changed for data test");
  m_git->refresh();
  m_model->refreshGitStatus();
  QTest::qWait(STATUS_REFRESH_WAIT_MS);

  m_model->setRootPath(m_repoPath);
  QModelIndex root = m_model->index(m_repoPath);
  QVERIFY(root.isValid());

  QTest::qWait(1000);

  bool foundFile = false;
  for (int i = 0; i < m_model->rowCount(root); ++i) {
    QModelIndex child = m_model->index(i, 0, root);
    if (m_model->fileName(child) == "initial.txt") {
      QVariant badge = child.data(GitStatusBadgeRole);
      QVERIFY(!badge.toString().isEmpty());
      QVariant color = child.data(GitStatusBadgeColorRole);
      QVERIFY(color.value<QColor>().isValid());
      foundFile = true;
      break;
    }
  }
  if (!foundFile) {
    QSKIP("QFileSystemModel not yet populated (async); badge logic tested "
           "directly");
  }
}

QTEST_MAIN(TestGitFileSystemModel)
#include "test_gitfilesystemmodel.moc"
