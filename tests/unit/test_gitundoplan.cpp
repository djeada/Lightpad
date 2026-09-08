#include "git/gitintegration.h"
#include "git/gitundoplan.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitUndoPlan : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testGoalNames();
  void testAllSevenGoalsAreOffered();
  void testEveryGoalHasItsOwnLayerMatrix();
  void testDiscardIsDestructiveAndListsPaths();
  void testDiscardUnavailableOnACleanTree();
  void testUnstageIsNotDestructive();
  void testUnstageUnavailableWithNothingStaged();
  void testSoftAndMixedResetDifferOnlyInTheIndex();
  void testHardResetListsAtRiskPaths();
  void testRevertExplainsWhyItIsSafe();
  void testPublishedCommitRecommendsRevert();
  void testLocalCommitRecommendsSoftReset();
  void testStagedChangesRecommendUnstage();
  void testCommandsIncludeTheirFlags();
  void testInvalidRepository();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  QString m_remotePath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
  const GitUndoOption *optionFor(const GitUndoPlan &plan, GitUndoGoal goal);
};

void TestGitUndoPlan::init() {
  static int counter = 0;
  const QString root = m_tempDir.path() + "/case" + QString::number(counter++);
  m_repoPath = root + "/work";
  m_remotePath = root + "/remote.git";
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(QDir().mkpath(m_remotePath));

  QVERIFY(git({"init", "--bare", "--initial-branch=main"}, m_remotePath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("f.txt", "one\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));
  QVERIFY(git({"remote", "add", "origin", m_remotePath}));
  QVERIFY(git({"push", "-u", "origin", "main"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitUndoPlan::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitUndoPlan::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitUndoPlan::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

const GitUndoOption *TestGitUndoPlan::optionFor(const GitUndoPlan &plan,
                                                GitUndoGoal goal) {
  for (const GitUndoOption &option : plan.options) {
    if (option.goal == goal) {
      return &option;
    }
  }
  return nullptr;
}

void TestGitUndoPlan::testGoalNames() {
  QCOMPARE(gitUndoGoalName(GitUndoGoal::UnstageKeepEdits),
           QString("Unstage but keep edits"));
  QCOMPARE(gitUndoGoalName(GitUndoGoal::RevertPublishedCommit),
           QString("Undo with a new inverse commit"));
}

void TestGitUndoPlan::testAllSevenGoalsAreOffered() {
  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  QVERIFY(plan.valid);
  QCOMPARE(plan.options.size(), 7);
}

void TestGitUndoPlan::testEveryGoalHasItsOwnLayerMatrix() {
  const GitUndoPlan plan = buildGitUndoPlan(m_git);

  QSet<QString> signatures;
  for (const GitUndoOption &option : plan.options) {
    const QString signature = option.matrix.workingTree + "|" +
                              option.matrix.index + "|" + option.matrix.head +
                              "|" + option.matrix.branch;
    QVERIFY2(!signatures.contains(signature),
             qPrintable(gitUndoGoalName(option.goal) + ": " + signature));
    signatures.insert(signature);
    QVERIFY(!option.question.isEmpty());
    QVERIFY(!option.explanation.isEmpty());
    QVERIFY(!option.command.isEmpty());
  }
}

void TestGitUndoPlan::testDiscardIsDestructiveAndListsPaths() {
  writeFile("f.txt", "CHANGED\n");
  writeFile("g.txt", "new\n");
  QVERIFY(git({"add", "g.txt"}));
  writeFile("g.txt", "new and edited\n");

  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  const GitUndoOption *discard =
      optionFor(plan, GitUndoGoal::DiscardWorkingEdits);

  QVERIFY(discard->isAvailable());
  QVERIFY(discard->destructive);
  QVERIFY2(discard->atRiskPaths.contains("f.txt"),
           qPrintable(discard->atRiskPaths.join(",")));
  QVERIFY2(discard->explanation.contains("no safety net"),
           qPrintable(discard->explanation));
}

void TestGitUndoPlan::testDiscardUnavailableOnACleanTree() {
  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  const GitUndoOption *discard =
      optionFor(plan, GitUndoGoal::DiscardWorkingEdits);
  QVERIFY(!discard->isAvailable());
}

void TestGitUndoPlan::testUnstageIsNotDestructive() {
  writeFile("f.txt", "CHANGED\n");
  QVERIFY(git({"add", "f.txt"}));

  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  const GitUndoOption *unstage = optionFor(plan, GitUndoGoal::UnstageKeepEdits);

  QVERIFY(unstage->isAvailable());
  QVERIFY(!unstage->destructive);
  QVERIFY2(unstage->explanation.contains("keeps your work"),
           qPrintable(unstage->explanation));
  QCOMPARE(unstage->matrix.workingTree, QString("unchanged"));
}

void TestGitUndoPlan::testUnstageUnavailableWithNothingStaged() {
  writeFile("f.txt", "CHANGED\n");
  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  QVERIFY(!optionFor(plan, GitUndoGoal::UnstageKeepEdits)->isAvailable());
}

void TestGitUndoPlan::testSoftAndMixedResetDifferOnlyInTheIndex() {
  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  const GitUndoOption *soft =
      optionFor(plan, GitUndoGoal::MoveBranchKeepStaged);
  const GitUndoOption *mixed =
      optionFor(plan, GitUndoGoal::MoveBranchKeepUnstaged);

  QCOMPARE(soft->matrix.workingTree, mixed->matrix.workingTree);
  QCOMPARE(soft->matrix.head, mixed->matrix.head);
  QVERIFY(soft->matrix.index != mixed->matrix.index);
  QCOMPARE(soft->resetMode, QString("soft"));
  QCOMPARE(mixed->resetMode, QString("mixed"));
  QVERIFY(!soft->destructive);
  QVERIFY(!mixed->destructive);
}

void TestGitUndoPlan::testHardResetListsAtRiskPaths() {
  writeFile("f.txt", "CHANGED\n");

  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  const GitUndoOption *hard = optionFor(plan, GitUndoGoal::ResetEverything);
  QVERIFY(hard->destructive);
  QVERIFY(hard->atRiskPaths.contains("f.txt"));
  QCOMPARE(hard->resetMode, QString("hard"));
}

void TestGitUndoPlan::testRevertExplainsWhyItIsSafe() {
  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  const GitUndoOption *revert =
      optionFor(plan, GitUndoGoal::RevertPublishedCommit);
  QVERIFY(!revert->destructive);
  QVERIFY2(revert->explanation.contains("collaboration-safe"),
           qPrintable(revert->explanation));
  QCOMPARE(revert->previewKind, GitOperationKind::Revert);
}

void TestGitUndoPlan::testPublishedCommitRecommendsRevert() {
  const QString head = m_git->getCommitDetails("HEAD").hash;
  const GitUndoPlan plan = buildGitUndoPlan(m_git, head);

  QVERIFY(plan.targetIsPublished);
  QCOMPARE(plan.recommended, GitUndoGoal::RevertPublishedCommit);
  QVERIFY2(plan.recommendationReason.contains("already on origin/main"),
           qPrintable(plan.recommendationReason));
}

void TestGitUndoPlan::testLocalCommitRecommendsSoftReset() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "local only"}));
  const QString head = m_git->getCommitDetails("HEAD").hash;

  const GitUndoPlan plan = buildGitUndoPlan(m_git, head);
  QVERIFY(!plan.targetIsPublished);
  QCOMPARE(plan.recommended, GitUndoGoal::MoveBranchKeepStaged);
  QVERIFY2(plan.recommendationReason.contains("only here"),
           qPrintable(plan.recommendationReason));
}

void TestGitUndoPlan::testStagedChangesRecommendUnstage() {
  writeFile("f.txt", "CHANGED\n");
  QVERIFY(git({"add", "f.txt"}));

  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  QCOMPARE(plan.recommended, GitUndoGoal::UnstageKeepEdits);
}

void TestGitUndoPlan::testCommandsIncludeTheirFlags() {
  const GitUndoPlan plan = buildGitUndoPlan(m_git);
  QVERIFY(optionFor(plan, GitUndoGoal::MoveBranchKeepStaged)
              ->command.contains("--soft"));
  QVERIFY(optionFor(plan, GitUndoGoal::MoveBranchKeepUnstaged)
              ->command.contains("--mixed"));
  QVERIFY(optionFor(plan, GitUndoGoal::ResetEverything)
              ->command.contains("--hard"));
  QVERIFY(optionFor(plan, GitUndoGoal::UnstageKeepEdits)
              ->command.contains("--staged"));
  QVERIFY(optionFor(plan, GitUndoGoal::AmendLastCommit)
              ->command.contains("--amend"));
}

void TestGitUndoPlan::testInvalidRepository() {
  GitIntegration empty;
  const GitUndoPlan plan = buildGitUndoPlan(&empty);
  QVERIFY(!plan.valid);
  QVERIFY(!plan.error.isEmpty());
}

QTEST_MAIN(TestGitUndoPlan)
#include "test_gitundoplan.moc"
