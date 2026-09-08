#include "git/gitcommandlog.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitCommandLog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  void testFormatQuotesWhatNeedsIt();
  void testRedactUrlCredentials();
  void testRedactNamedSecrets();
  void testRedactTokens();
  void testRedactLeavesOrdinaryTextAlone();
  void testExplanationsDistinguishResetModes();
  void testExplanationsDistinguishRestoreTargets();
  void testRiskClassification();
  void testReadOnlyDetection();
  void testModeNames();

  void testCommandsAreRecorded();
  void testFailedCommandsRecordTheirExitCode();
  void testRecordedOutputIsRedacted();
  void testSignalFiresPerCommand();
  void testHistoryCanBeCleared();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitCommandLog::initTestCase() {
  QStandardPaths::setTestModeEnabled(true);
}

void TestGitCommandLog::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("f.txt", "one\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
  m_git->clearCommandHistory();
}

void TestGitCommandLog::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitCommandLog::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitCommandLog::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitCommandLog::testFormatQuotesWhatNeedsIt() {
  QCOMPARE(formatGitCommand({"status", "--porcelain"}),
           QString("git status --porcelain"));
  QCOMPARE(formatGitCommand({"commit", "-m", "a message with spaces"}),
           QString("git commit -m 'a message with spaces'"));
  QCOMPARE(formatGitCommand({"log", "HEAD~3..HEAD"}),
           QString("git log HEAD~3..HEAD"));
}

void TestGitCommandLog::testRedactUrlCredentials() {
  QCOMPARE(redactSecrets("https://ada:hunter2@example.com/repo.git"),
           QString("https://***@example.com/repo.git"));
  QCOMPARE(redactSecrets("http://token@host/x"), QString("http://***@host/x"));
}

void TestGitCommandLog::testRedactNamedSecrets() {
  QCOMPARE(redactSecrets("--password=hunter2"), QString("--password=***"));
  QVERIFY(redactSecrets("--token abcdef123456").endsWith("***"));
  QVERIFY(!redactSecrets("--api-key=zzz").contains("zzz"));
}

void TestGitCommandLog::testRedactTokens() {
  QVERIFY(!redactSecrets("Authorization: Bearer abcdefghijklmnop")
               .contains("abcdefghijklmnop"));
  QVERIFY(!redactSecrets("ghp_0123456789abcdefghij").contains("0123456789"));
}

void TestGitCommandLog::testRedactLeavesOrdinaryTextAlone() {
  const QString plain = "Merge branch 'feature/login' into main";
  QCOMPARE(redactSecrets(plain), plain);
  QCOMPARE(redactSecrets("https://example.com/repo.git"),
           QString("https://example.com/repo.git"));
}

void TestGitCommandLog::testExplanationsDistinguishResetModes() {
  const QString hard = gitCommandExplanation({"reset", "--hard", "HEAD~1"});
  const QString soft = gitCommandExplanation({"reset", "--soft", "HEAD~1"});
  const QString mixed = gitCommandExplanation({"reset", "HEAD~1"});

  QVERIFY(hard != soft);
  QVERIFY(soft != mixed);
  QVERIFY2(hard.contains("Uncommitted work is lost"), qPrintable(hard));
  QVERIFY2(soft.contains("stay exactly as they are"), qPrintable(soft));
  QVERIFY2(mixed.contains("clears the index"), qPrintable(mixed));
}

void TestGitCommandLog::testExplanationsDistinguishRestoreTargets() {
  const QString staged =
      gitCommandExplanation({"restore", "--staged", "f.txt"});
  const QString worktree = gitCommandExplanation({"restore", "f.txt"});
  QVERIFY(staged != worktree);
  QVERIFY2(worktree.contains("Uncommitted edits are lost"),
           qPrintable(worktree));
}

void TestGitCommandLog::testRiskClassification() {
  QCOMPARE(gitCommandRisk({"status"}), GitOperationRisk::Safe);
  QCOMPARE(gitCommandRisk({"reset", "--hard", "HEAD"}),
           GitOperationRisk::MayDiscardUncommitted);
  QCOMPARE(gitCommandRisk({"restore", "f.txt"}),
           GitOperationRisk::MayDiscardUncommitted);
  QCOMPARE(gitCommandRisk({"restore", "--staged", "f.txt"}),
           GitOperationRisk::Safe);
  QCOMPARE(gitCommandRisk({"push", "--force", "origin", "main"}),
           GitOperationRisk::AffectsSharedHistory);
  QCOMPARE(gitCommandRisk({"push", "origin", "main"}), GitOperationRisk::Safe);
  QCOMPARE(gitCommandRisk({"rebase", "main"}),
           GitOperationRisk::RewritesLocalHistory);
  QCOMPARE(gitCommandRisk({"commit", "--amend"}),
           GitOperationRisk::RewritesLocalHistory);
}

void TestGitCommandLog::testReadOnlyDetection() {
  QVERIFY(gitCommandIsReadOnly({"status", "--porcelain"}));
  QVERIFY(gitCommandIsReadOnly({"log", "-5"}));
  QVERIFY(gitCommandIsReadOnly({"merge-tree", "--write-tree", "a", "b"}));
  QVERIFY(gitCommandIsReadOnly({"branch", "--all", "--contains", "abc"}));
  QVERIFY(gitCommandIsReadOnly({"config", "--get", "pull.rebase"}));

  QVERIFY(!gitCommandIsReadOnly({"commit", "-m", "x"}));
  QVERIFY(!gitCommandIsReadOnly({"reset", "--hard"}));
  QVERIFY(!gitCommandIsReadOnly({"branch", "-d", "feature"}));
  QVERIFY(!gitCommandIsReadOnly({"config", "pull.rebase", "true"}));
  QVERIFY(!gitCommandIsReadOnly({"worktree", "add", "path"}));
}

void TestGitCommandLog::testModeNames() {
  QSet<QString> names;
  for (GitCommandMirrorMode mode :
       {GitCommandMirrorMode::Hidden, GitCommandMirrorMode::Learn,
        GitCommandMirrorMode::Preview, GitCommandMirrorMode::Expert}) {
    const QString name = gitCommandMirrorModeName(mode);
    QVERIFY(!name.isEmpty());
    QVERIFY(!names.contains(name));
    names.insert(name);
  }
}

void TestGitCommandLog::testCommandsAreRecorded() {
  m_git->getStatus();

  const QList<GitCommandRecord> history = m_git->commandHistory();
  QVERIFY(!history.isEmpty());
  const GitCommandRecord &record = history.last();
  QCOMPARE(record.args.first(), QString("status"));
  QVERIFY(record.succeeded);
  QCOMPARE(record.workingDirectory, m_repoPath);
  QVERIFY(record.commandLine().startsWith("git status"));
  QVERIFY(record.when.isValid());
}

void TestGitCommandLog::testFailedCommandsRecordTheirExitCode() {
  m_git->getCommitDetails("definitely-not-a-ref");

  bool sawFailure = false;
  for (const GitCommandRecord &record : m_git->commandHistory()) {
    if (!record.succeeded) {
      sawFailure = true;
      QVERIFY(record.exitCode != 0);
    }
  }
  QVERIFY(sawFailure);
}

void TestGitCommandLog::testRecordedOutputIsRedacted() {

  QVERIFY(git(
      {"remote", "add", "secret", "https://ada:hunter2@example.com/repo.git"}));
  m_git->clearCommandHistory();
  m_git->getRemotes();

  for (const GitCommandRecord &record : m_git->commandHistory()) {
    QVERIFY2(!record.output.contains("hunter2"), qPrintable(record.output));
    QVERIFY(!record.error.contains("hunter2"));
  }
}

void TestGitCommandLog::testSignalFiresPerCommand() {
  QSignalSpy spy(m_git, &GitIntegration::commandExecuted);
  m_git->getStatus();
  QVERIFY(spy.count() >= 1);
}

void TestGitCommandLog::testHistoryCanBeCleared() {
  m_git->getStatus();
  QVERIFY(!m_git->commandHistory().isEmpty());
  m_git->clearCommandHistory();
  QVERIFY(m_git->commandHistory().isEmpty());
}

QTEST_MAIN(TestGitCommandLog)
#include "test_gitcommandlog.moc"
