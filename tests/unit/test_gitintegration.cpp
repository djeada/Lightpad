#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitIntegration : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testInvalidRepository();
  void testFindRepository();
  void testGetStatus();
  void testStageFile();
  void testUnstageFile();
  void testCommit();
  void testGetBranches();
  void testCreateBranch();
  void testCheckoutCommit();
  void testCreateBranchFromCommit();
  void testGetDiffLines();
  void testGetFileDiffStagedAndUnstaged();
  void testWordDiffCommand();

  void testInitRepository();
  void testRemoteOperations();
  void testStashOperationsExtended();
  void testMergeConflictDetection();

  void testMergeBranch();
  void testStash();
  void testStashList();
  void testStashPopApply();
  void testGetRemotes();

  void testCommitAmend();
  void testDiscardAllChanges();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;

  bool runGitCommand(const QStringList &args);
  bool runGitCommandAt(const QString &path, const QStringList &args);
  void createTestFile(const QString &fileName, const QString &content);
};

void TestGitIntegration::initTestCase() {
  QVERIFY(m_tempDir.isValid());
  m_repoPath = m_tempDir.path() + "/test_repo";

  QDir dir;
  QVERIFY(dir.mkpath(m_repoPath));

  QVERIFY(runGitCommand({"init"}));

  QVERIFY(runGitCommand({"config", "user.email", "test@test.com"}));
  QVERIFY(runGitCommand({"config", "user.name", "Test User"}));

  createTestFile("initial.txt", "Initial content\n");
  QVERIFY(runGitCommand({"add", "initial.txt"}));
  QVERIFY(runGitCommand({"commit", "-m", "Initial commit"}));
}

void TestGitIntegration::cleanupTestCase() {}

bool TestGitIntegration::runGitCommand(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);

  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }

  return process.exitCode() == 0;
}

void TestGitIntegration::createTestFile(const QString &fileName,
                                        const QString &content) {
  QFile file(m_repoPath + "/" + fileName);
  file.open(QIODevice::WriteOnly);
  file.write(content.toUtf8());
  file.close();
}

void TestGitIntegration::testInvalidRepository() {
  GitIntegration git;

  QVERIFY(!git.setRepositoryPath("/nonexistent/path"));
  QVERIFY(!git.isValidRepository());
  QVERIFY(git.repositoryPath().isEmpty());
}

void TestGitIntegration::testFindRepository() {
  GitIntegration git;

  QVERIFY(git.setRepositoryPath(m_repoPath));
  QVERIFY(git.isValidRepository());
  QCOMPARE(git.repositoryPath(), m_repoPath);

  QString filePath = m_repoPath + "/initial.txt";
  GitIntegration git2;
  QVERIFY(git2.setRepositoryPath(filePath));
  QVERIFY(git2.isValidRepository());
  QCOMPARE(git2.repositoryPath(), m_repoPath);

  QDir dir(m_repoPath);
  dir.mkdir("subdir");
  QString subdirPath = m_repoPath + "/subdir";
  GitIntegration git3;
  QVERIFY(git3.setRepositoryPath(subdirPath));
  QVERIFY(git3.isValidRepository());
  QCOMPARE(git3.repositoryPath(), m_repoPath);
}

void TestGitIntegration::testGetStatus() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("untracked.txt", "Untracked content\n");

  QList<GitFileInfo> status = git.getStatus();

  bool foundUntracked = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "untracked.txt") {
      QCOMPARE(file.workTreeStatus, GitFileStatus::Untracked);
      foundUntracked = true;
      break;
    }
  }
  QVERIFY(foundUntracked);

  QFile::remove(m_repoPath + "/untracked.txt");
}

void TestGitIntegration::testStageFile() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("to_stage.txt", "Content to stage\n");

  QVERIFY(git.stageFile("to_stage.txt"));

  QList<GitFileInfo> status = git.getStatus();
  bool foundStaged = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "to_stage.txt") {
      QCOMPARE(file.indexStatus, GitFileStatus::Added);
      foundStaged = true;
      break;
    }
  }
  QVERIFY(foundStaged);

  runGitCommand({"reset", "HEAD", "to_stage.txt"});
  QFile::remove(m_repoPath + "/to_stage.txt");
}

void TestGitIntegration::testUnstageFile() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("to_unstage.txt", "Content\n");
  runGitCommand({"add", "to_unstage.txt"});

  QVERIFY(git.unstageFile("to_unstage.txt"));

  QList<GitFileInfo> status = git.getStatus();
  bool foundUnstaged = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "to_unstage.txt") {

      QCOMPARE(file.indexStatus, GitFileStatus::Untracked);
      QCOMPARE(file.workTreeStatus, GitFileStatus::Untracked);
      foundUnstaged = true;
      break;
    }
  }
  QVERIFY(foundUnstaged);

  QFile::remove(m_repoPath + "/to_unstage.txt");
}

void TestGitIntegration::testCommit() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("to_commit.txt", "Content to commit\n");
  QVERIFY(git.stageFile("to_commit.txt"));

  QVERIFY(git.commit("Test commit message"));

  QList<GitFileInfo> status = git.getStatus();
  bool foundFile = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "to_commit.txt") {
      foundFile = true;
      break;
    }
  }

  QVERIFY(!foundFile);

  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", {"log", "-1", "--pretty=%s"});
  process.waitForFinished();
  QString lastCommitMsg =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QCOMPARE(lastCommitMsg, "Test commit message");
}

void TestGitIntegration::testGetBranches() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QList<GitBranchInfo> branches = git.getBranches();

  QVERIFY(!branches.isEmpty());

  bool foundCurrent = false;
  for (const GitBranchInfo &branch : branches) {
    if (branch.isCurrent) {
      foundCurrent = true;
      break;
    }
  }
  QVERIFY(foundCurrent);
}

void TestGitIntegration::testCreateBranch() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QString originalBranch = git.currentBranch();

  QVERIFY(git.createBranch("test-feature-branch", true));
  QCOMPARE(git.currentBranch(), "test-feature-branch");

  QVERIFY(git.checkoutBranch(originalBranch));
  QCOMPARE(git.currentBranch(), originalBranch);

  QVERIFY(git.deleteBranch("test-feature-branch"));
}

void TestGitIntegration::testCheckoutCommit() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QString originalBranch = git.currentBranch();

  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", {"rev-parse", "HEAD"});
  QVERIFY(process.waitForFinished(GIT_COMMAND_TIMEOUT_MS));
  QString commitHash =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QVERIFY(!commitHash.isEmpty());

  QVERIFY(git.checkoutCommit(commitHash));

  process.start("git", {"rev-parse", "HEAD"});
  QVERIFY(process.waitForFinished(GIT_COMMAND_TIMEOUT_MS));
  QString headHash =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QCOMPARE(headHash, commitHash);

  QString branchName = git.currentBranch();
  QVERIFY(branchName == "HEAD" || branchName == "detached HEAD" ||
          branchName.contains("HEAD"));

  if (!originalBranch.isEmpty()) {
    QVERIFY(git.checkoutBranch(originalBranch));
  }
}

void TestGitIntegration::testCreateBranchFromCommit() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QString originalBranch = git.currentBranch();

  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", {"rev-parse", "HEAD"});
  QVERIFY(process.waitForFinished(GIT_COMMAND_TIMEOUT_MS));
  QString commitHash =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QVERIFY(!commitHash.isEmpty());

  const QString branchName = "commit-context-branch";
  QVERIFY(git.createBranchFromCommit(branchName, commitHash, true));
  QCOMPARE(git.currentBranch(), branchName);

  if (!originalBranch.isEmpty()) {
    QVERIFY(git.checkoutBranch(originalBranch));
  } else {
    QVERIFY(git.checkoutBranch("master") || git.checkoutBranch("main"));
  }
  QVERIFY(git.deleteBranch(branchName));
}

void TestGitIntegration::testGetDiffLines() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QString filePath = m_repoPath + "/initial.txt";
  QFile file(filePath);
  file.open(QIODevice::WriteOnly);
  file.write("Modified content\nNew line\n");
  file.close();

  QList<GitDiffLineInfo> diffLines = git.getDiffLines(filePath);

  QVERIFY(!diffLines.isEmpty());

  runGitCommand({"checkout", "--", "initial.txt"});
}

void TestGitIntegration::testGetFileDiffStagedAndUnstaged() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QString stagedFile = "staged_diff.txt";
  QString unstagedFile = "initial.txt";

  createTestFile(stagedFile, "Staged content\n");
  QVERIFY(git.stageFile(stagedFile));

  runGitCommand({"add", unstagedFile});
  runGitCommand({"commit", "-m", "Add initial.txt for unstaged diff test",
                 "--only", unstagedFile});

  createTestFile(unstagedFile, "Modified content\n");

  QString stagedDiff = git.getFileDiff(stagedFile, true);
  QVERIFY(!stagedDiff.trimmed().isEmpty());
  QVERIFY(stagedDiff.contains("Staged content"));

  QString unstagedDiff = git.getFileDiff(unstagedFile, false);
  QVERIFY(!unstagedDiff.trimmed().isEmpty());
  QVERIFY(unstagedDiff.contains("Modified content"));

  runGitCommand({"rm", "--cached", stagedFile});
  QFile::remove(m_repoPath + "/" + stagedFile);
  runGitCommand({"checkout", "--", unstagedFile});
}

void TestGitIntegration::testWordDiffCommand() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QString filePath = m_repoPath + "/initial.txt";
  QFile file(filePath);
  file.open(QIODevice::WriteOnly);
  file.write("Word diff base\n");
  file.close();
  QVERIFY(runGitCommand({"add", "initial.txt"}));
  QVERIFY(runGitCommand({"commit", "-m", "Base for word diff"}));

  file.open(QIODevice::WriteOnly);
  file.write("Word diff updated\n");
  file.close();

  QString diff = git.executeWordDiff(
      {"diff", "--word-diff", "--color=never", "--", "initial.txt"});
  QVERIFY(!diff.trimmed().isEmpty());
  QVERIFY(diff.contains("{+updated+}"));

  runGitCommand({"checkout", "--", "initial.txt"});
}

bool TestGitIntegration::runGitCommandAt(const QString &path,
                                         const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(path);
  process.start("git", args);

  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }

  return process.exitCode() == 0;
}

void TestGitIntegration::testInitRepository() {

  QString newRepoPath = m_tempDir.path() + "/new_repo";
  QDir dir;
  QVERIFY(dir.mkpath(newRepoPath));

  GitIntegration git;

  QVERIFY(!git.setRepositoryPath(newRepoPath));

  QVERIFY(git.initRepository(newRepoPath));

  QVERIFY(git.isValidRepository());
  QCOMPARE(git.repositoryPath(), newRepoPath);

  QFile file(newRepoPath + "/test.txt");
  file.open(QIODevice::WriteOnly);
  file.write("Test content\n");
  file.close();

  runGitCommandAt(newRepoPath, {"config", "user.email", "test@test.com"});
  runGitCommandAt(newRepoPath, {"config", "user.name", "Test User"});

  QVERIFY(git.stageFile("test.txt"));
  QVERIFY(git.commit("Initial commit"));
}

void TestGitIntegration::testRemoteOperations() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QList<GitRemoteInfo> remotes = git.getRemotes();

  QVERIFY(git.addRemote("test-origin", "https://github.com/test/repo.git"));

  remotes = git.getRemotes();
  bool foundRemote = false;
  for (const GitRemoteInfo &remote : remotes) {
    if (remote.name == "test-origin") {
      QCOMPARE(remote.fetchUrl, QString("https://github.com/test/repo.git"));
      foundRemote = true;
      break;
    }
  }
  QVERIFY(foundRemote);

  QVERIFY(git.removeRemote("test-origin"));

  remotes = git.getRemotes();
  foundRemote = false;
  for (const GitRemoteInfo &remote : remotes) {
    if (remote.name == "test-origin") {
      foundRemote = true;
      break;
    }
  }
  QVERIFY(!foundRemote);
}

void TestGitIntegration::testStashOperationsExtended() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("stash_test.txt", "Content to stash\n");
  QVERIFY(git.stageFile("stash_test.txt"));

  QVERIFY(git.stash("Test stash message"));

  QList<GitFileInfo> status = git.getStatus();
  bool foundFile = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "stash_test.txt") {
      foundFile = true;
      break;
    }
  }
  QVERIFY(!foundFile);

  QList<GitStashEntry> stashes = git.getStashList();
  QVERIFY(!stashes.isEmpty());

  QVERIFY(stashes.size() >= 1);

  QVERIFY(git.stashPop(0));

  status = git.getStatus();
  foundFile = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "stash_test.txt") {
      foundFile = true;
      break;
    }
  }
  QVERIFY(foundFile);

  runGitCommand({"reset", "HEAD", "stash_test.txt"});
  QFile::remove(m_repoPath + "/stash_test.txt");
}

void TestGitIntegration::testMergeConflictDetection() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QVERIFY(!git.hasMergeConflicts());
  QVERIFY(git.getConflictedFiles().isEmpty());

  QString originalBranch = git.currentBranch();

  QVERIFY(git.createBranch("conflict-test-branch", true));

  QFile file(m_repoPath + "/initial.txt");
  file.open(QIODevice::WriteOnly);
  file.write("Feature branch content\n");
  file.close();

  QVERIFY(git.stageFile("initial.txt"));
  QVERIFY(git.commit("Feature branch change"));

  QVERIFY(git.checkoutBranch(originalBranch));

  file.open(QIODevice::WriteOnly);
  file.write("Original branch content\n");
  file.close();

  QVERIFY(git.stageFile("initial.txt"));
  QVERIFY(git.commit("Original branch change"));

  bool mergeSuccess = git.mergeBranch("conflict-test-branch");

  if (!mergeSuccess) {

    QVERIFY(git.hasMergeConflicts() || git.isMergeInProgress());

    if (git.isMergeInProgress()) {
      git.abortMerge();
    }
  }

  git.deleteBranch("conflict-test-branch", true);

  QProcess checkProcess;
  checkProcess.setWorkingDirectory(m_repoPath);
  checkProcess.start("git", {"rev-list", "--count", "HEAD"});
  checkProcess.waitForFinished(GIT_COMMAND_TIMEOUT_MS);
  int commitCount =
      QString::fromUtf8(checkProcess.readAllStandardOutput()).trimmed().toInt();

  if (commitCount > 1) {
    runGitCommand({"checkout", "HEAD~1", "--", "initial.txt"});
    runGitCommand({"reset", "--hard", "HEAD~1"});
  } else {

    runGitCommand({"checkout", "HEAD", "--", "initial.txt"});
  }
}

void TestGitIntegration::testMergeBranch() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QString originalBranch = git.currentBranch();

  QVERIFY(git.createBranch("merge-test-branch", true));
  createTestFile("merge_test.txt", "Merge test content\n");
  QVERIFY(git.stageFile("merge_test.txt"));
  QVERIFY(git.commit("Add merge test file"));

  QVERIFY(git.checkoutBranch(originalBranch));

  QVERIFY(git.mergeBranch("merge-test-branch"));

  QFileInfo mergedFile(m_repoPath + "/merge_test.txt");
  QVERIFY(mergedFile.exists());

  QVERIFY(git.deleteBranch("merge-test-branch"));
}

void TestGitIntegration::testStash() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("stash_test.txt", "Stash test content\n");
  QVERIFY(git.stageFile("stash_test.txt"));

  QVERIFY(git.stash("Test stash message", false));

  QList<GitFileInfo> status = git.getStatus();
  bool foundStashFile = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "stash_test.txt") {
      foundStashFile = true;
      break;
    }
  }
  QVERIFY(!foundStashFile);

  QList<GitStashEntry> stashes = git.stashList();
  QVERIFY(!stashes.isEmpty());

  QVERIFY(git.stashPop(0));

  runGitCommand({"reset", "HEAD", "stash_test.txt"});
  QFile::remove(m_repoPath + "/stash_test.txt");
}

void TestGitIntegration::testStashList() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("stash_list_test.txt", "Stash list test\n");
  QVERIFY(git.stageFile("stash_list_test.txt"));
  QVERIFY(git.stash("First stash"));

  createTestFile("stash_list_test2.txt", "Stash list test 2\n");
  QVERIFY(git.stageFile("stash_list_test2.txt"));
  QVERIFY(git.stash("Second stash"));

  QList<GitStashEntry> stashes = git.stashList();
  QVERIFY(stashes.size() >= 2);

  bool foundFirst = false;
  bool foundSecond = false;
  for (const GitStashEntry &entry : stashes) {
    if (entry.message.contains("First stash"))
      foundFirst = true;
    if (entry.message.contains("Second stash"))
      foundSecond = true;
  }
  QVERIFY(foundFirst);
  QVERIFY(foundSecond);

  QVERIFY(git.stashClear());
  stashes = git.stashList();
  QVERIFY(stashes.isEmpty());
}

void TestGitIntegration::testStashPopApply() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("stash_pop_test.txt", "Stash pop test\n");
  QVERIFY(git.stageFile("stash_pop_test.txt"));
  QVERIFY(git.stash("Pop test stash"));

  QVERIFY(git.stashApply(0));

  QList<GitFileInfo> status = git.getStatus();
  bool foundFile = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "stash_pop_test.txt") {
      QCOMPARE(file.indexStatus, GitFileStatus::Added);
      foundFile = true;
      break;
    }
  }
  QVERIFY(foundFile);

  QList<GitStashEntry> stashes = git.stashList();
  QVERIFY(!stashes.isEmpty());

  QVERIFY(git.stashDrop(0));

  runGitCommand({"reset", "HEAD", "stash_pop_test.txt"});
  QFile::remove(m_repoPath + "/stash_pop_test.txt");
}

void TestGitIntegration::testGetRemotes() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  QList<GitRemoteInfo> remotes = git.getRemotes();
  QVERIFY(remotes.isEmpty());

  runGitCommand({"remote", "add", "origin", "https://example.com/repo.git"});

  remotes = git.getRemotes();
  QVERIFY(!remotes.isEmpty());
  bool foundOrigin = false;
  for (const GitRemoteInfo &remote : remotes) {
    if (remote.name == "origin") {
      foundOrigin = true;
      break;
    }
  }
  QVERIFY(foundOrigin);

  runGitCommand({"remote", "remove", "origin"});
}

void TestGitIntegration::testCommitAmend() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("amend_test.txt", "Original content\n");
  QVERIFY(git.stageFile("amend_test.txt"));
  QVERIFY(git.commit("Original commit message"));

  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", {"log", "-1", "--pretty=%s"});
  process.waitForFinished();
  QString lastMsg =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QCOMPARE(lastMsg, "Original commit message");

  QVERIFY(git.commitAmend("Amended commit message"));

  process.start("git", {"log", "-1", "--pretty=%s"});
  process.waitForFinished();
  lastMsg = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QCOMPARE(lastMsg, "Amended commit message");

  createTestFile("amend_test.txt", "Updated content\n");
  QVERIFY(git.stageFile("amend_test.txt"));
  QVERIFY(git.commitAmend());

  process.start("git", {"log", "-1", "--pretty=%s"});
  process.waitForFinished();
  lastMsg = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QCOMPARE(lastMsg, "Amended commit message");

  process.start("git", {"show", "HEAD:amend_test.txt"});
  process.waitForFinished();
  QString fileContent =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QCOMPARE(fileContent, "Updated content");
}

void TestGitIntegration::testDiscardAllChanges() {
  GitIntegration git;
  QVERIFY(git.setRepositoryPath(m_repoPath));

  createTestFile("discard_all_test.txt", "Original content\n");
  QVERIFY(git.stageFile("discard_all_test.txt"));
  QVERIFY(git.commit("Add discard all test file"));

  createTestFile("discard_all_test.txt", "Modified content for discard test\n");

  QList<GitFileInfo> status = git.getStatus();
  bool foundModified = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "discard_all_test.txt") {
      QCOMPARE(file.workTreeStatus, GitFileStatus::Modified);
      foundModified = true;
      break;
    }
  }
  QVERIFY(foundModified);

  QVERIFY(git.discardAllChanges());

  status = git.getStatus();
  foundModified = false;
  for (const GitFileInfo &file : status) {
    if (file.filePath == "discard_all_test.txt" &&
        file.workTreeStatus == GitFileStatus::Modified) {
      foundModified = true;
      break;
    }
  }
  QVERIFY(!foundModified);
}

QTEST_MAIN(TestGitIntegration)
#include "test_gitintegration.moc"
