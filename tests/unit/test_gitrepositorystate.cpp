#include "git/gitintegration.h"
#include "git/gitrepositorystate.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitRepositoryState : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  void testParseCleanStatus();
  void testParseMixedStatus();
  void testParseFileStagedAndModified();
  void testParseDetachedHead();
  void testParseUnbornBranch();
  void testParseConflicts();

  void testSummaryClean();
  void testSummaryMixedChanges();
  void testSummaryNoUpstream();
  void testSummaryDetached();
  void testSummaryOperationInProgress();
  void testSummaryInvalid();
  void testOneLineForms();
  void testOperationNames();

  void testRepositoryStateOnCleanRepo();
  void testRepositoryStateTracksLayers();
  void testRepositoryStateCountsStashes();
  void testRepositoryStateDetachedHead();
  void testRepositoryStateDetectsMerge();
  void testRepositoryStateInvalidRepository();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
};

void TestGitRepositoryState::initTestCase() {
  QVERIFY(m_tempDir.isValid());
  m_repoPath = m_tempDir.path() + "/state_repo";
  QVERIFY(QDir().mkpath(m_repoPath));

  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "test@test.com"}));
  QVERIFY(git({"config", "user.name", "Test User"}));

  writeFile("initial.txt", "initial\n");
  QVERIFY(git({"add", "initial.txt"}));
  QVERIFY(git({"commit", "-m", "Initial commit"}));
}

bool TestGitRepositoryState::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitRepositoryState::writeFile(const QString &name,
                                       const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitRepositoryState::testParseCleanStatus() {
  const QString output = "# branch.oid 1a2b3c4d5e6f7a8b\n"
                         "# branch.head main\n"
                         "# branch.upstream origin/main\n"
                         "# branch.ab +0 -0\n";

  GitRepositoryState state;
  parseGitStatusPorcelainV2(output, state);

  QCOMPARE(state.branch, QString("main"));
  QVERIFY(!state.detachedHead);
  QVERIFY(!state.unbornBranch);
  QCOMPARE(state.headShortHash, QString("1a2b3c4"));
  QCOMPARE(state.upstream, QString("origin/main"));
  QVERIFY(state.hasUpstream);
  QCOMPARE(state.ahead, 0);
  QCOMPARE(state.behind, 0);
  QCOMPARE(state.stagedCount, 0);
  QCOMPARE(state.modifiedCount, 0);
  QCOMPARE(state.untrackedCount, 0);
}

void TestGitRepositoryState::testParseMixedStatus() {
  const QString output =
      "# branch.oid 1a2b3c4d5e6f7a8b\n"
      "# branch.head feature/login\n"
      "# branch.upstream origin/feature/login\n"
      "# branch.ab +2 -1\n"
      "1 M. N... 100644 100644 100644 aaa bbb staged.txt\n"
      "1 .M N... 100644 100644 100644 aaa bbb dirty.txt\n"
      "2 R. N... 100644 100644 100644 R100 new.txt\told.txt\n"
      "? untracked.txt\n"
      "! ignored.txt\n";

  GitRepositoryState state;
  parseGitStatusPorcelainV2(output, state);

  QCOMPARE(state.branch, QString("feature/login"));
  QCOMPARE(state.upstream, QString("origin/feature/login"));
  QCOMPARE(state.ahead, 2);
  QCOMPARE(state.behind, 1);

  QCOMPARE(state.stagedCount, 2);
  QCOMPARE(state.modifiedCount, 1);
  QCOMPARE(state.untrackedCount, 1);
  QCOMPARE(state.conflictedCount, 0);
  QCOMPARE(state.workingTreeCount(), 2);
}

void TestGitRepositoryState::testParseFileStagedAndModified() {

  const QString output = "# branch.head main\n"
                         "1 MM N... 100644 100644 100644 aaa bbb both.txt\n";

  GitRepositoryState state;
  parseGitStatusPorcelainV2(output, state);

  QCOMPARE(state.stagedCount, 1);
  QCOMPARE(state.modifiedCount, 1);
}

void TestGitRepositoryState::testParseDetachedHead() {
  const QString output = "# branch.oid 9f8e7d6c5b4a3928\n"
                         "# branch.head (detached)\n";

  GitRepositoryState state;
  parseGitStatusPorcelainV2(output, state);

  QVERIFY(state.detachedHead);
  QVERIFY(state.branch.isEmpty());
  QCOMPARE(state.headShortHash, QString("9f8e7d6"));
  QVERIFY(!state.hasUpstream);
}

void TestGitRepositoryState::testParseUnbornBranch() {
  const QString output = "# branch.oid (initial)\n"
                         "# branch.head main\n"
                         "? first.txt\n";

  GitRepositoryState state;
  parseGitStatusPorcelainV2(output, state);

  QVERIFY(state.unbornBranch);
  QVERIFY(state.headShortHash.isEmpty());
  QCOMPARE(state.untrackedCount, 1);
}

void TestGitRepositoryState::testParseConflicts() {
  const QString output =
      "# branch.head main\n"
      "u UU N... 100644 100644 100644 100644 aaa bbb ccc conflict.txt\n";

  GitRepositoryState state;
  parseGitStatusPorcelainV2(output, state);

  QCOMPARE(state.conflictedCount, 1);
  QCOMPARE(state.stagedCount, 0);
  QCOMPARE(state.modifiedCount, 0);
}

void TestGitRepositoryState::testSummaryClean() {
  GitRepositoryState state;
  state.valid = true;
  state.branch = "main";
  state.upstream = "origin/main";
  state.hasUpstream = true;

  const QString summary = gitRepositoryStateSummary(state);
  QVERIFY2(summary.contains("Working tree clean"), qPrintable(summary));
  QVERIFY2(summary.contains("main is up to date with origin/main"),
           qPrintable(summary));
  QVERIFY(summary.endsWith('.'));
}

void TestGitRepositoryState::testSummaryMixedChanges() {
  GitRepositoryState state;
  state.valid = true;
  state.branch = "feature/login";
  state.upstream = "origin/feature/login";
  state.hasUpstream = true;
  state.ahead = 2;
  state.behind = 1;
  state.modifiedCount = 3;
  state.stagedCount = 2;

  const QString summary = gitRepositoryStateSummary(state);
  QVERIFY2(summary.contains("3 unstaged changes"), qPrintable(summary));
  QVERIFY2(summary.contains("2 staged changes"), qPrintable(summary));
  QVERIFY2(summary.contains("feature/login is 2 commits ahead and 1 behind "
                            "origin/feature/login"),
           qPrintable(summary));
}

void TestGitRepositoryState::testSummaryNoUpstream() {
  GitRepositoryState state;
  state.valid = true;
  state.branch = "wip";
  state.untrackedCount = 1;

  const QString summary = gitRepositoryStateSummary(state);
  QVERIFY2(summary.contains("1 untracked file"), qPrintable(summary));
  QVERIFY2(summary.contains("wip has no upstream branch"), qPrintable(summary));
}

void TestGitRepositoryState::testSummaryDetached() {
  GitRepositoryState state;
  state.valid = true;
  state.detachedHead = true;
  state.headShortHash = "a1b2c3d";

  const QString summary = gitRepositoryStateSummary(state);
  QVERIFY2(summary.contains("HEAD is detached at a1b2c3d"),
           qPrintable(summary));
}

void TestGitRepositoryState::testSummaryOperationInProgress() {
  GitRepositoryState state;
  state.valid = true;
  state.branch = "main";
  state.operation = GitOperation::Rebase;
  state.operationDetail = "3 of 7";
  state.conflictedCount = 2;
  state.stashCount = 1;

  const QString summary = gitRepositoryStateSummary(state);
  QVERIFY2(summary.startsWith("Rebase in progress (3 of 7)"),
           qPrintable(summary));
  QVERIFY2(summary.contains("2 conflicted files"), qPrintable(summary));
  QVERIFY2(summary.contains("1 stash saved"), qPrintable(summary));
}

void TestGitRepositoryState::testSummaryInvalid() {
  GitRepositoryState state;
  QCOMPARE(gitRepositoryStateSummary(state), QString("No Git repository."));
  QCOMPARE(gitRepositoryStateOneLine(state), QString("No Git repository"));
}

void TestGitRepositoryState::testOneLineForms() {
  GitRepositoryState clean;
  clean.valid = true;
  clean.branch = "main";
  QCOMPARE(gitRepositoryStateOneLine(clean), QString("main · clean"));

  GitRepositoryState busy;
  busy.valid = true;
  busy.branch = "main";
  busy.hasUpstream = true;
  busy.upstream = "origin/main";
  busy.ahead = 2;
  busy.behind = 1;
  busy.stagedCount = 2;
  busy.modifiedCount = 3;
  busy.stashCount = 1;

  const QString line = gitRepositoryStateOneLine(busy);
  QVERIFY2(line.contains(QString::fromUtf8("main ↑2 ↓1")), qPrintable(line));
  QVERIFY2(line.contains("2 staged"), qPrintable(line));
  QVERIFY2(line.contains("3 changed"), qPrintable(line));
  QVERIFY2(line.contains("1 stashed"), qPrintable(line));

  GitRepositoryState detached;
  detached.valid = true;
  detached.detachedHead = true;
  detached.headShortHash = "a1b2c3d";
  QVERIFY(gitRepositoryStateOneLine(detached).contains("detached a1b2c3d"));
}

void TestGitRepositoryState::testOperationNames() {
  QCOMPARE(gitOperationName(GitOperation::None), QString());
  QCOMPARE(gitOperationName(GitOperation::Rebase), QString("Rebase"));
  QCOMPARE(gitOperationName(GitOperation::CherryPick), QString("Cherry-pick"));
  QVERIFY(gitOperationExitHint(GitOperation::None).isEmpty());
  QVERIFY(gitOperationExitHint(GitOperation::Merge).contains("git merge"));
}

void TestGitRepositoryState::testRepositoryStateOnCleanRepo() {
  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  const GitRepositoryState state = integration.repositoryState();
  QVERIFY(state.valid);
  QCOMPARE(state.repositoryRoot, m_repoPath);
  QCOMPARE(state.branch, QString("main"));
  QVERIFY(!state.detachedHead);
  QVERIFY(!state.unbornBranch);
  QCOMPARE(state.headShortHash.size(), 7);
  QCOMPARE(state.headSubject, QString("Initial commit"));
  QVERIFY(state.isClean());
  QCOMPARE(state.operation, GitOperation::None);
}

void TestGitRepositoryState::testRepositoryStateTracksLayers() {
  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  writeFile("staged.txt", "staged\n");
  writeFile("untracked.txt", "untracked\n");
  writeFile("initial.txt", "changed\n");
  QVERIFY(git({"add", "staged.txt"}));

  const GitRepositoryState state = integration.repositoryState();
  QCOMPARE(state.stagedCount, 1);
  QCOMPARE(state.modifiedCount, 1);
  QCOMPARE(state.untrackedCount, 1);
  QCOMPARE(state.workingTreeCount(), 2);
  QVERIFY(!state.isClean());

  QVERIFY(git({"reset", "--hard", "HEAD"}));
  QVERIFY(QFile::remove(m_repoPath + "/untracked.txt"));
  QVERIFY(integration.repositoryState().isClean());
}

void TestGitRepositoryState::testRepositoryStateCountsStashes() {
  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));
  QCOMPARE(integration.repositoryState().stashCount, 0);

  writeFile("initial.txt", "stash me\n");
  QVERIFY(git({"stash", "push", "-m", "wip"}));
  QCOMPARE(integration.repositoryState().stashCount, 1);

  QVERIFY(git({"stash", "drop"}));
  QCOMPARE(integration.repositoryState().stashCount, 0);
}

void TestGitRepositoryState::testRepositoryStateDetachedHead() {
  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  QVERIFY(git({"checkout", "--detach", "HEAD"}));
  const GitRepositoryState state = integration.repositoryState();
  QVERIFY(state.detachedHead);
  QVERIFY(state.branch.isEmpty());
  QVERIFY(!state.headShortHash.isEmpty());
  QVERIFY(gitRepositoryStateSummary(state).contains("HEAD is detached"));

  QVERIFY(git({"checkout", "main"}));
  QVERIFY(!integration.repositoryState().detachedHead);
}

void TestGitRepositoryState::testRepositoryStateDetectsMerge() {
  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("initial.txt", "side change\n");
  QVERIFY(git({"commit", "-am", "Side change"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("initial.txt", "main change\n");
  QVERIFY(git({"commit", "-am", "Main change"}));

  git({"merge", "side"});

  const GitRepositoryState state = integration.repositoryState();
  QCOMPARE(state.operation, GitOperation::Merge);
  QVERIFY(state.conflictedCount > 0);
  QVERIFY(gitRepositoryStateSummary(state).contains("Merge in progress"));

  QVERIFY(git({"merge", "--abort"}));
  QCOMPARE(integration.repositoryState().operation, GitOperation::None);
}

void TestGitRepositoryState::testRepositoryStateInvalidRepository() {
  GitIntegration integration;
  const GitRepositoryState state = integration.repositoryState();
  QVERIFY(!state.valid);
  QVERIFY(!state.isClean());
}

QTEST_MAIN(TestGitRepositoryState)
#include "test_gitrepositorystate.moc"
