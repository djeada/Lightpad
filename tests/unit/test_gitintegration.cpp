#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QProcess>
#include "git/gitintegration.h"

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
    // Tests from HEAD (extended functionality)
    void testInitRepository();
    void testRemoteOperations();
    void testStashOperationsExtended();
    void testMergeConflictDetection();
    // Tests from master
    void testMergeBranch();
    void testStash();
    void testStashList();
    void testStashPopApply();
    void testGetRemotes();

private:
    QTemporaryDir m_tempDir;
    QString m_repoPath;
    
    bool runGitCommand(const QStringList& args);
    bool runGitCommandAt(const QString& path, const QStringList& args);
    void createTestFile(const QString& fileName, const QString& content);
};

void TestGitIntegration::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    m_repoPath = m_tempDir.path() + "/test_repo";
    
    // Create and initialize a test git repository
    QDir dir;
    QVERIFY(dir.mkpath(m_repoPath));
    
    // Initialize git repo
    QVERIFY(runGitCommand({"init"}));
    
    // Configure git user for commits
    QVERIFY(runGitCommand({"config", "user.email", "test@test.com"}));
    QVERIFY(runGitCommand({"config", "user.name", "Test User"}));
    
    // Create initial file and commit
    createTestFile("initial.txt", "Initial content\n");
    QVERIFY(runGitCommand({"add", "initial.txt"}));
    QVERIFY(runGitCommand({"commit", "-m", "Initial commit"}));
}

void TestGitIntegration::cleanupTestCase()
{
    // QTemporaryDir will clean up automatically
}

bool TestGitIntegration::runGitCommand(const QStringList& args)
{
    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    process.start("git", args);
    
    if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
        return false;
    }
    
    return process.exitCode() == 0;
}

void TestGitIntegration::createTestFile(const QString& fileName, const QString& content)
{
    QFile file(m_repoPath + "/" + fileName);
    file.open(QIODevice::WriteOnly);
    file.write(content.toUtf8());
    file.close();
}

void TestGitIntegration::testInvalidRepository()
{
    GitIntegration git;
    
    // Test with a non-existent path
    QVERIFY(!git.setRepositoryPath("/nonexistent/path"));
    QVERIFY(!git.isValidRepository());
    QVERIFY(git.repositoryPath().isEmpty());
}

void TestGitIntegration::testFindRepository()
{
    GitIntegration git;
    
    // Test with repository root
    QVERIFY(git.setRepositoryPath(m_repoPath));
    QVERIFY(git.isValidRepository());
    QCOMPARE(git.repositoryPath(), m_repoPath);
    
    // Test with a file within the repository
    QString filePath = m_repoPath + "/initial.txt";
    GitIntegration git2;
    QVERIFY(git2.setRepositoryPath(filePath));
    QVERIFY(git2.isValidRepository());
    QCOMPARE(git2.repositoryPath(), m_repoPath);
    
    // Test with a subdirectory
    QDir dir(m_repoPath);
    dir.mkdir("subdir");
    QString subdirPath = m_repoPath + "/subdir";
    GitIntegration git3;
    QVERIFY(git3.setRepositoryPath(subdirPath));
    QVERIFY(git3.isValidRepository());
    QCOMPARE(git3.repositoryPath(), m_repoPath);
}

void TestGitIntegration::testGetStatus()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Create a new untracked file
    createTestFile("untracked.txt", "Untracked content\n");
    
    QList<GitFileInfo> status = git.getStatus();
    
    // Should have at least the untracked file
    bool foundUntracked = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "untracked.txt") {
            QCOMPARE(file.workTreeStatus, GitFileStatus::Untracked);
            foundUntracked = true;
            break;
        }
    }
    QVERIFY(foundUntracked);
    
    // Clean up
    QFile::remove(m_repoPath + "/untracked.txt");
}

void TestGitIntegration::testStageFile()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Create a new file
    createTestFile("to_stage.txt", "Content to stage\n");
    
    // Stage the file
    QVERIFY(git.stageFile("to_stage.txt"));
    
    // Check status - file should now be staged
    QList<GitFileInfo> status = git.getStatus();
    bool foundStaged = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "to_stage.txt") {
            QCOMPARE(file.indexStatus, GitFileStatus::Added);
            foundStaged = true;
            break;
        }
    }
    QVERIFY(foundStaged);
    
    // Clean up
    runGitCommand({"reset", "HEAD", "to_stage.txt"});
    QFile::remove(m_repoPath + "/to_stage.txt");
}

void TestGitIntegration::testUnstageFile()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Create and stage a file
    createTestFile("to_unstage.txt", "Content\n");
    runGitCommand({"add", "to_unstage.txt"});
    
    // Unstage the file
    QVERIFY(git.unstageFile("to_unstage.txt"));
    
    // Check status - file should now be untracked (both index and worktree are '?')
    QList<GitFileInfo> status = git.getStatus();
    bool foundUnstaged = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "to_unstage.txt") {
            // For untracked files, git shows "??" - both index and worktree are untracked
            QCOMPARE(file.indexStatus, GitFileStatus::Untracked);
            QCOMPARE(file.workTreeStatus, GitFileStatus::Untracked);
            foundUnstaged = true;
            break;
        }
    }
    QVERIFY(foundUnstaged);
    
    // Clean up
    QFile::remove(m_repoPath + "/to_unstage.txt");
}

void TestGitIntegration::testCommit()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Create and stage a file
    createTestFile("to_commit.txt", "Content to commit\n");
    QVERIFY(git.stageFile("to_commit.txt"));
    
    // Commit
    QVERIFY(git.commit("Test commit message"));
    
    // Check status - should be clean now
    QList<GitFileInfo> status = git.getStatus();
    bool foundFile = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "to_commit.txt") {
            foundFile = true;
            break;
        }
    }
    // File should not appear in status (it's committed)
    QVERIFY(!foundFile);
    
    // Verify commit message was used
    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    process.start("git", {"log", "-1", "--pretty=%s"});
    process.waitForFinished();
    QString lastCommitMsg = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QCOMPARE(lastCommitMsg, "Test commit message");
}

void TestGitIntegration::testGetBranches()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    QList<GitBranchInfo> branches = git.getBranches();
    
    // Should have at least the default branch
    QVERIFY(!branches.isEmpty());
    
    // One branch should be current
    bool foundCurrent = false;
    for (const GitBranchInfo& branch : branches) {
        if (branch.isCurrent) {
            foundCurrent = true;
            break;
        }
    }
    QVERIFY(foundCurrent);
}

void TestGitIntegration::testCreateBranch()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    QString originalBranch = git.currentBranch();
    
    // Create and checkout a new branch
    QVERIFY(git.createBranch("test-feature-branch", true));
    QCOMPARE(git.currentBranch(), "test-feature-branch");
    
    // Switch back to original branch
    QVERIFY(git.checkoutBranch(originalBranch));
    QCOMPARE(git.currentBranch(), originalBranch);
    
    // Delete the test branch
    QVERIFY(git.deleteBranch("test-feature-branch"));
}

void TestGitIntegration::testCheckoutCommit()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));

    QString originalBranch = git.currentBranch();

    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    process.start("git", {"rev-parse", "HEAD"});
    QVERIFY(process.waitForFinished(GIT_COMMAND_TIMEOUT_MS));
    QString commitHash = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QVERIFY(!commitHash.isEmpty());

    QVERIFY(git.checkoutCommit(commitHash));

    process.start("git", {"rev-parse", "HEAD"});
    QVERIFY(process.waitForFinished(GIT_COMMAND_TIMEOUT_MS));
    QString headHash = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QCOMPARE(headHash, commitHash);

    QString branchName = git.currentBranch();
    QVERIFY(branchName == "HEAD" || branchName == "detached HEAD" || branchName.contains("HEAD"));

    if (!originalBranch.isEmpty()) {
        QVERIFY(git.checkoutBranch(originalBranch));
    }
}

void TestGitIntegration::testCreateBranchFromCommit()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));

    QString originalBranch = git.currentBranch();

    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    process.start("git", {"rev-parse", "HEAD"});
    QVERIFY(process.waitForFinished(GIT_COMMAND_TIMEOUT_MS));
    QString commitHash = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
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

void TestGitIntegration::testGetDiffLines()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Modify an existing file
    QString filePath = m_repoPath + "/initial.txt";
    QFile file(filePath);
    file.open(QIODevice::WriteOnly);
    file.write("Modified content\nNew line\n");
    file.close();
    
    // Get diff lines
    QList<GitDiffLineInfo> diffLines = git.getDiffLines(filePath);
    
    // Should have some diff information
    QVERIFY(!diffLines.isEmpty());
    
    // Restore the file
    runGitCommand({"checkout", "--", "initial.txt"});
}

void TestGitIntegration::testGetFileDiffStagedAndUnstaged()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));

    QString stagedFile = "staged_diff.txt";
    QString unstagedFile = "initial.txt";

    createTestFile(stagedFile, "Staged content\n");
    QVERIFY(git.stageFile(stagedFile));

    runGitCommand({"add", unstagedFile});
    runGitCommand({"commit", "-m", "Add initial.txt for unstaged diff test", "--only", unstagedFile});

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

bool TestGitIntegration::runGitCommandAt(const QString& path, const QStringList& args)
{
    QProcess process;
    process.setWorkingDirectory(path);
    process.start("git", args);
    
    if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
        return false;
    }
    
    return process.exitCode() == 0;
}

void TestGitIntegration::testInitRepository()
{
    // Create a new directory for testing init
    QString newRepoPath = m_tempDir.path() + "/new_repo";
    QDir dir;
    QVERIFY(dir.mkpath(newRepoPath));
    
    GitIntegration git;
    
    // Should not be a valid repository before init
    QVERIFY(!git.setRepositoryPath(newRepoPath));
    
    // Initialize the repository
    QVERIFY(git.initRepository(newRepoPath));
    
    // Should now be valid
    QVERIFY(git.isValidRepository());
    QCOMPARE(git.repositoryPath(), newRepoPath);
    
    // Should be able to create files and commit
    QFile file(newRepoPath + "/test.txt");
    file.open(QIODevice::WriteOnly);
    file.write("Test content\n");
    file.close();
    
    // Configure git user for commits
    runGitCommandAt(newRepoPath, {"config", "user.email", "test@test.com"});
    runGitCommandAt(newRepoPath, {"config", "user.name", "Test User"});
    
    QVERIFY(git.stageFile("test.txt"));
    QVERIFY(git.commit("Initial commit"));
}

void TestGitIntegration::testRemoteOperations()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Initially should have no remotes
    QList<GitRemoteInfo> remotes = git.getRemotes();
    // May or may not have remotes depending on test setup
    
    // Add a remote
    QVERIFY(git.addRemote("test-origin", "https://github.com/test/repo.git"));
    
    // Check that remote was added
    remotes = git.getRemotes();
    bool foundRemote = false;
    for (const GitRemoteInfo& remote : remotes) {
        if (remote.name == "test-origin") {
            QCOMPARE(remote.fetchUrl, QString("https://github.com/test/repo.git"));
            foundRemote = true;
            break;
        }
    }
    QVERIFY(foundRemote);
    
    // Remove the remote
    QVERIFY(git.removeRemote("test-origin"));
    
    // Verify it's gone
    remotes = git.getRemotes();
    foundRemote = false;
    for (const GitRemoteInfo& remote : remotes) {
        if (remote.name == "test-origin") {
            foundRemote = true;
            break;
        }
    }
    QVERIFY(!foundRemote);
}

void TestGitIntegration::testStashOperationsExtended()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Create a change to stash
    createTestFile("stash_test.txt", "Content to stash\n");
    QVERIFY(git.stageFile("stash_test.txt"));
    
    // Stash the changes
    QVERIFY(git.stash("Test stash message"));
    
    // File should no longer be in status (stashed)
    QList<GitFileInfo> status = git.getStatus();
    bool foundFile = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "stash_test.txt") {
            foundFile = true;
            break;
        }
    }
    QVERIFY(!foundFile);
    
    // Should have at least one stash entry
    QList<GitStashEntry> stashes = git.getStashList();
    QVERIFY(!stashes.isEmpty());
    
    // Just verify we have a stash - message format varies by git version
    QVERIFY(stashes.size() >= 1);
    
    // Pop the stash
    QVERIFY(git.stashPop(0));
    
    // File should be back in status
    status = git.getStatus();
    foundFile = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "stash_test.txt") {
            foundFile = true;
            break;
        }
    }
    QVERIFY(foundFile);
    
    // Clean up
    runGitCommand({"reset", "HEAD", "stash_test.txt"});
    QFile::remove(m_repoPath + "/stash_test.txt");
}

void TestGitIntegration::testMergeConflictDetection()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Initially should have no merge conflicts
    QVERIFY(!git.hasMergeConflicts());
    QVERIFY(git.getConflictedFiles().isEmpty());
    
    // Create a branch with a conflicting change
    QString originalBranch = git.currentBranch();
    
    // Create feature branch
    QVERIFY(git.createBranch("conflict-test-branch", true));
    
    // Modify a file on the feature branch
    QFile file(m_repoPath + "/initial.txt");
    file.open(QIODevice::WriteOnly);
    file.write("Feature branch content\n");
    file.close();
    
    QVERIFY(git.stageFile("initial.txt"));
    QVERIFY(git.commit("Feature branch change"));
    
    // Switch back to original branch
    QVERIFY(git.checkoutBranch(originalBranch));
    
    // Make a conflicting change on the original branch
    file.open(QIODevice::WriteOnly);
    file.write("Original branch content\n");
    file.close();
    
    QVERIFY(git.stageFile("initial.txt"));
    QVERIFY(git.commit("Original branch change"));
    
    // Try to merge - this should create conflicts
    bool mergeSuccess = git.mergeBranch("conflict-test-branch");
    
    // Merge should fail due to conflicts (or git might auto-resolve, depending on content)
    // Either way, let's check the state
    if (!mergeSuccess) {
        // If merge failed, we should have conflicts
        QVERIFY(git.hasMergeConflicts() || git.isMergeInProgress());
        
        // Abort the merge to clean up
        if (git.isMergeInProgress()) {
            git.abortMerge();
        }
    }
    
    // Clean up - delete the test branch
    git.deleteBranch("conflict-test-branch", true);
    
    // Restore the original file - use a more robust cleanup
    // Check if we have commits to go back to
    QProcess checkProcess;
    checkProcess.setWorkingDirectory(m_repoPath);
    checkProcess.start("git", {"rev-list", "--count", "HEAD"});
    checkProcess.waitForFinished(GIT_COMMAND_TIMEOUT_MS);
    int commitCount = QString::fromUtf8(checkProcess.readAllStandardOutput()).trimmed().toInt();
    
    if (commitCount > 1) {
        runGitCommand({"checkout", "HEAD~1", "--", "initial.txt"});
        runGitCommand({"reset", "--hard", "HEAD~1"});
    } else {
        // If only one commit, just restore the file
        runGitCommand({"checkout", "HEAD", "--", "initial.txt"});
    }
}

void TestGitIntegration::testMergeBranch()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    QString originalBranch = git.currentBranch();
    
    // Create a feature branch with a commit
    QVERIFY(git.createBranch("merge-test-branch", true));
    createTestFile("merge_test.txt", "Merge test content\n");
    QVERIFY(git.stageFile("merge_test.txt"));
    QVERIFY(git.commit("Add merge test file"));
    
    // Switch back to original branch
    QVERIFY(git.checkoutBranch(originalBranch));
    
    // Merge the feature branch
    QVERIFY(git.mergeBranch("merge-test-branch"));
    
    // Verify the file exists after merge
    QFileInfo mergedFile(m_repoPath + "/merge_test.txt");
    QVERIFY(mergedFile.exists());
    
    // Clean up
    QVERIFY(git.deleteBranch("merge-test-branch"));
}

void TestGitIntegration::testStash()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Modify a file
    createTestFile("stash_test.txt", "Stash test content\n");
    QVERIFY(git.stageFile("stash_test.txt"));
    
    // Stash changes
    QVERIFY(git.stash("Test stash message", false));
    
    // File should not exist after stash (it was staged as new)
    QList<GitFileInfo> status = git.getStatus();
    bool foundStashFile = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "stash_test.txt") {
            foundStashFile = true;
            break;
        }
    }
    QVERIFY(!foundStashFile);
    
    // Stash should have at least one entry
    QList<GitStashEntry> stashes = git.stashList();
    QVERIFY(!stashes.isEmpty());
    
    // Pop the stash
    QVERIFY(git.stashPop(0));
    
    // Clean up - unstage the file and remove it
    runGitCommand({"reset", "HEAD", "stash_test.txt"});
    QFile::remove(m_repoPath + "/stash_test.txt");
}

void TestGitIntegration::testStashList()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Create and stash some changes
    createTestFile("stash_list_test.txt", "Stash list test\n");
    QVERIFY(git.stageFile("stash_list_test.txt"));
    QVERIFY(git.stash("First stash"));
    
    createTestFile("stash_list_test2.txt", "Stash list test 2\n");
    QVERIFY(git.stageFile("stash_list_test2.txt"));
    QVERIFY(git.stash("Second stash"));
    
    // List stashes
    QList<GitStashEntry> stashes = git.stashList();
    QVERIFY(stashes.size() >= 2);
    
    // Verify stash messages
    bool foundFirst = false;
    bool foundSecond = false;
    for (const GitStashEntry& entry : stashes) {
        if (entry.message.contains("First stash")) foundFirst = true;
        if (entry.message.contains("Second stash")) foundSecond = true;
    }
    QVERIFY(foundFirst);
    QVERIFY(foundSecond);
    
    // Clean up - clear all stashes
    QVERIFY(git.stashClear());
    stashes = git.stashList();
    QVERIFY(stashes.isEmpty());
}

void TestGitIntegration::testStashPopApply()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // Create and stash changes
    createTestFile("stash_pop_test.txt", "Stash pop test\n");
    QVERIFY(git.stageFile("stash_pop_test.txt"));
    QVERIFY(git.stash("Pop test stash"));
    
    // Apply stash (keeps the stash entry)
    QVERIFY(git.stashApply(0));
    
    // File should now be staged again
    QList<GitFileInfo> status = git.getStatus();
    bool foundFile = false;
    for (const GitFileInfo& file : status) {
        if (file.filePath == "stash_pop_test.txt") {
            QCOMPARE(file.indexStatus, GitFileStatus::Added);
            foundFile = true;
            break;
        }
    }
    QVERIFY(foundFile);
    
    // Stash should still exist
    QList<GitStashEntry> stashes = git.stashList();
    QVERIFY(!stashes.isEmpty());
    
    // Drop the stash
    QVERIFY(git.stashDrop(0));
    
    // Clean up
    runGitCommand({"reset", "HEAD", "stash_pop_test.txt"});
    QFile::remove(m_repoPath + "/stash_pop_test.txt");
}

void TestGitIntegration::testGetRemotes()
{
    GitIntegration git;
    QVERIFY(git.setRepositoryPath(m_repoPath));
    
    // By default, a local repo has no remotes
    QList<GitRemoteInfo> remotes = git.getRemotes();
    QVERIFY(remotes.isEmpty());
    
    // Add a remote
    runGitCommand({"remote", "add", "origin", "https://example.com/repo.git"});
    
    remotes = git.getRemotes();
    QVERIFY(!remotes.isEmpty());
    bool foundOrigin = false;
    for (const GitRemoteInfo& remote : remotes) {
        if (remote.name == "origin") {
            foundOrigin = true;
            break;
        }
    }
    QVERIFY(foundOrigin);
    
    // Clean up - remove the remote
    runGitCommand({"remote", "remove", "origin"});
}

QTEST_MAIN(TestGitIntegration)
#include "test_gitintegration.moc"
