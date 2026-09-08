#include "git/gitintegration.h"
#include "git/gitoperationpreview.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitOperationPreview : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testParseMergeTreeConflicts();
  void testOperationNames();

  void testMergeFastForwardIsSafe();
  void testMergeCreatesAMergeCommit();
  void testMergePredictsConflicts();
  void testMergeWithoutConflictPredictsNone();

  void testRebaseMarksCommitsRewritten();
  void testRebaseOfNothingIsSafe();

  void testHardResetFlagsUncommittedWork();
  void testSoftResetKeepsEverything();
  void testMixedResetTouchesIndexOnly();

  void testRevertAddsACommit();
  void testCherryPickCopiesTheChange();

  void testBranchDeleteFindsOrphanedCommits();
  void testBranchDeleteOfMergedBranchIsSafe();

  void testStashApplyFlagsDirtyTree();
  void testCommandsAreReported();
  void testInvalidRepositoryFails();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitOperationPreview::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("f.txt", "one\ntwo\nthree\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitOperationPreview::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitOperationPreview::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitOperationPreview::writeFile(const QString &name,
                                        const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitOperationPreview::testParseMergeTreeConflicts() {
  const QString output = "1b203bba25f438c6da7ac2695265a07071f6d066\n"
                         "src/a.cpp\n"
                         "src/b.cpp\n"
                         "\n"
                         "Auto-merging src/a.cpp\n"
                         "CONFLICT (content): Merge conflict in src/a.cpp\n";
  const QStringList conflicts = parseMergeTreeConflicts(output);
  QCOMPARE(conflicts, QStringList({"src/a.cpp", "src/b.cpp"}));
}

void TestGitOperationPreview::testOperationNames() {
  QCOMPARE(gitOperationKindName(GitOperationKind::Rebase), QString("Rebase"));
  QCOMPARE(gitOperationRiskName(GitOperationRisk::MayDiscardUncommitted),
           QString("May discard uncommitted work"));
}

void TestGitOperationPreview::testMergeFastForwardIsSafe() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Merge;
  request.target = "side";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QVERIFY(preview.effect.valid);
  QCOMPARE(preview.effect.risk, GitOperationRisk::Safe);
  QVERIFY(preview.effect.commitsAdded.isEmpty());
  QVERIFY2(preview.effect.rationale.contains("moves the branch pointer"),
           qPrintable(preview.effect.rationale));
}

void TestGitOperationPreview::testMergeCreatesAMergeCommit() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Merge;
  request.target = "side";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.commitsAdded.size(), 1);
  QCOMPARE(preview.effect.risk, GitOperationRisk::Safe);
  QVERIFY(preview.effect.commitsRewritten.isEmpty());
  QVERIFY2(preview.effect.rationale.contains("merge commit"),
           qPrintable(preview.effect.rationale));

  QVERIFY(!preview.after.isEmpty());
  QCOMPARE(preview.after.first().state, PreviewNode::State::Added);
  QVERIFY(preview.after.first().isHead);
}

void TestGitOperationPreview::testMergePredictsConflicts() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("f.txt", "one\nSIDE\nthree\n");
  QVERIFY(git({"commit", "-am", "side edit"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("f.txt", "one\nMAIN\nthree\n");
  QVERIFY(git({"commit", "-am", "main edit"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Merge;
  request.target = "side";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QVERIFY(preview.effect.conflictsKnown);
  QCOMPARE(preview.effect.conflictPaths, QStringList({"f.txt"}));
  QVERIFY2(preview.effect.rationale.contains("predicts a conflict"),
           qPrintable(preview.effect.rationale));

  QVERIFY(!m_git->isMergeInProgress());
  QVERIFY(m_git->repositoryState().isClean());
}

void TestGitOperationPreview::testMergeWithoutConflictPredictsNone() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Merge;
  request.target = "side";
  const GitOperationPreview preview = previewGitOperation(m_git, request);
  QVERIFY(preview.effect.conflictPaths.isEmpty());
}

void TestGitOperationPreview::testRebaseMarksCommitsRewritten() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s1.txt", "s1\n");
  QVERIFY(git({"add", "s1.txt"}));
  QVERIFY(git({"commit", "-m", "side one"}));
  writeFile("s2.txt", "s2\n");
  QVERIFY(git({"add", "s2.txt"}));
  QVERIFY(git({"commit", "-m", "side two"}));

  QVERIFY(git({"checkout", "main"}));
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));
  QVERIFY(git({"checkout", "side"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Rebase;
  request.target = "main";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.commitsRewritten.size(), 2);
  QCOMPARE(preview.effect.risk, GitOperationRisk::RewritesLocalHistory);
  QVERIFY2(preview.effect.rationale.contains("new hash"),
           qPrintable(preview.effect.rationale));

  QVERIFY(!preview.effect.conflictsKnown);

  int rewritten = 0;
  for (const PreviewNode &node : preview.after) {
    if (node.state == PreviewNode::State::Rewritten) {
      ++rewritten;
    }
  }
  QCOMPARE(rewritten, 2);
}

void TestGitOperationPreview::testRebaseOfNothingIsSafe() {
  QVERIFY(git({"checkout", "-b", "side"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Rebase;
  request.target = "main";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.risk, GitOperationRisk::Safe);
  QVERIFY(preview.effect.commitsRewritten.isEmpty());
}

void TestGitOperationPreview::testHardResetFlagsUncommittedWork() {
  writeFile("f.txt", "one\nCHANGED\nthree\n");
  writeFile("extra.txt", "extra\n");

  GitOperationRequest request;
  request.kind = GitOperationKind::Reset;
  request.target = "HEAD";
  request.resetMode = "hard";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.risk, GitOperationRisk::MayDiscardUncommitted);
  QVERIFY(preview.effect.changesWorkingTree);
  QVERIFY(preview.effect.changesIndex);

  QVERIFY2(preview.effect.atRiskPaths.contains("f.txt"),
           qPrintable(preview.effect.atRiskPaths.join(",")));
  QVERIFY2(preview.effect.atRiskPaths.contains("extra.txt"),
           qPrintable(preview.effect.atRiskPaths.join(",")));
}

void TestGitOperationPreview::testSoftResetKeepsEverything() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");
  QVERIFY(git({"commit", "-am", "second"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Reset;
  request.target = "HEAD~1";
  request.resetMode = "soft";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QVERIFY(!preview.effect.changesWorkingTree);
  QVERIFY(!preview.effect.changesIndex);
  QCOMPARE(preview.effect.commitsUnreachable.size(), 1);
  QVERIFY(preview.effect.atRiskPaths.isEmpty());
  QVERIFY2(preview.effect.rationale.contains("stays staged"),
           qPrintable(preview.effect.rationale));
}

void TestGitOperationPreview::testMixedResetTouchesIndexOnly() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");
  QVERIFY(git({"commit", "-am", "second"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Reset;
  request.target = "HEAD~1";
  request.resetMode = "mixed";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QVERIFY(preview.effect.changesIndex);
  QVERIFY(!preview.effect.changesWorkingTree);
  QVERIFY(preview.effect.atRiskPaths.isEmpty());
  QVERIFY2(preview.effect.rationale.contains("Nothing on disk is lost"),
           qPrintable(preview.effect.rationale));
}

void TestGitOperationPreview::testRevertAddsACommit() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");
  QVERIFY(git({"commit", "-am", "add four"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::Revert;
  request.target = m_git->getCommitDetails("HEAD").hash;
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.commitsAdded.size(), 1);
  QVERIFY(preview.effect.commitsUnreachable.isEmpty());
  QCOMPARE(preview.effect.risk, GitOperationRisk::Safe);
  QVERIFY2(preview.effect.rationale.contains("original commit stays"),
           qPrintable(preview.effect.rationale));
}

void TestGitOperationPreview::testCherryPickCopiesTheChange() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  const QString hash = m_git->getCommitDetails("HEAD").hash;
  QVERIFY(git({"checkout", "main"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::CherryPick;
  request.target = hash;
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.commitsAdded.size(), 1);
  QVERIFY2(preview.effect.rationale.contains("different hash"),
           qPrintable(preview.effect.rationale));
}

void TestGitOperationPreview::testBranchDeleteFindsOrphanedCommits() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "only on side"}));
  QVERIFY(git({"checkout", "main"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::BranchDelete;
  request.target = "side";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.commitsUnreachable.size(), 1);
  QCOMPARE(preview.effect.commitsUnreachable.first().subject,
           QString("only on side"));
  QCOMPARE(preview.effect.risk, GitOperationRisk::RewritesLocalHistory);
}

void TestGitOperationPreview::testBranchDeleteOfMergedBranchIsSafe() {
  QVERIFY(git({"branch", "copy"}));

  GitOperationRequest request;
  request.kind = GitOperationKind::BranchDelete;
  request.target = "copy";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QVERIFY(preview.effect.commitsUnreachable.isEmpty());
  QCOMPARE(preview.effect.risk, GitOperationRisk::Safe);
  QVERIFY2(preview.effect.rationale.contains("only removes a name"),
           qPrintable(preview.effect.rationale));
}

void TestGitOperationPreview::testStashApplyFlagsDirtyTree() {
  writeFile("f.txt", "one\nDIRTY\nthree\n");

  GitOperationRequest request;
  request.kind = GitOperationKind::StashPop;
  request.stashIndex = 0;
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.risk, GitOperationRisk::MayDiscardUncommitted);
  QVERIFY(preview.effect.atRiskPaths.contains("f.txt"));
  QVERIFY2(preview.effect.rationale.contains("drops the stash"),
           qPrintable(preview.effect.rationale));
}

void TestGitOperationPreview::testCommandsAreReported() {
  GitOperationRequest request;
  request.kind = GitOperationKind::Reset;
  request.target = "HEAD~0";
  request.resetMode = "hard";
  const GitOperationPreview preview = previewGitOperation(m_git, request);

  QCOMPARE(preview.effect.commands.size(), 1);
  QCOMPARE(preview.effect.commands.first(), QString("git reset --hard HEAD~0"));
}

void TestGitOperationPreview::testInvalidRepositoryFails() {
  GitIntegration empty;
  GitOperationRequest request;
  request.kind = GitOperationKind::Merge;
  request.target = "side";
  const GitOperationPreview preview = previewGitOperation(&empty, request);
  QVERIFY(!preview.effect.valid);
  QVERIFY(!preview.effect.error.isEmpty());
}

QTEST_MAIN(TestGitOperationPreview)
#include "test_gitoperationpreview.moc"
