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
    void testGetDiffLines();

private:
    QTemporaryDir m_tempDir;
    QString m_repoPath;
    
    bool runGitCommand(const QStringList& args);
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
    
    if (!process.waitForFinished(5000)) {
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

QTEST_MAIN(TestGitIntegration)
#include "test_gitintegration.moc"
