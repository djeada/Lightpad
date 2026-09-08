#include "git/gitintegration.h"
#include "git/gitintegrationadvice.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitIntegrationAdvice : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testIntentNames();
  void testAllFourOptionsAreOffered();
  void testFastForwardCaseRecommendsMerge();
  void testDivergedCaseExplainsTheTradeOff();
  void testRebaseIsMarkedAsRewriting();
  void testRebaseUnavailableWithoutOwnCommits();
  void testCherryPickNeedsACommit();
  void testCherryPickAvailableWithACommit();
  void testApplyOptionCreatesNoCommit();
  void testMergeUnavailableWhenNothingIncoming();
  void testDetachedHeadIsRefused();
  void testInvalidRepositoryIsRefused();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  const GitIntegrationOption *optionFor(const GitIntegrationAdvice &advice,
                                        GitIntegrationIntent intent);
  void makeDivergence();
};

void TestGitIntegrationAdvice::init() {
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
}

void TestGitIntegrationAdvice::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitIntegrationAdvice::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitIntegrationAdvice::writeFile(const QString &name,
                                         const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

const GitIntegrationOption *
TestGitIntegrationAdvice::optionFor(const GitIntegrationAdvice &advice,
                                    GitIntegrationIntent intent) {
  for (const GitIntegrationOption &option : advice.options) {
    if (option.intent == intent) {
      return &option;
    }
  }
  return nullptr;
}

void TestGitIntegrationAdvice::makeDivergence() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));
}

void TestGitIntegrationAdvice::testIntentNames() {
  QCOMPARE(gitIntegrationIntentName(GitIntegrationIntent::BringEverythingIn),
           QString("Merge"));
  QCOMPARE(gitIntegrationIntentName(GitIntegrationIntent::ReplayMineOnTop),
           QString("Rebase"));
  QCOMPARE(gitIntegrationIntentName(GitIntegrationIntent::BringOneCommit),
           QString("Cherry-pick"));
}

void TestGitIntegrationAdvice::testAllFourOptionsAreOffered() {
  makeDivergence();
  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side");
  QVERIFY(advice.valid);
  QCOMPARE(advice.options.size(), 4);
  QCOMPARE(advice.targetBranch, QString("main"));
}

void TestGitIntegrationAdvice::testFastForwardCaseRecommendsMerge() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));

  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side");
  QCOMPARE(advice.recommended, GitIntegrationIntent::BringEverythingIn);
  QVERIFY2(advice.recommendationReason.contains("fast-forward"),
           qPrintable(advice.recommendationReason));

  const GitIntegrationOption *merge =
      optionFor(advice, GitIntegrationIntent::BringEverythingIn);
  QVERIFY2(merge->consequence.contains("No merge commit"),
           qPrintable(merge->consequence));
}

void TestGitIntegrationAdvice::testDivergedCaseExplainsTheTradeOff() {
  makeDivergence();
  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side");

  QCOMPARE(advice.recommended, GitIntegrationIntent::BringEverythingIn);

  QVERIFY2(advice.recommendationReason.contains("straighter history"),
           qPrintable(advice.recommendationReason));
  QVERIFY2(advice.recommendationReason.contains("recreating"),
           qPrintable(advice.recommendationReason));
}

void TestGitIntegrationAdvice::testRebaseIsMarkedAsRewriting() {
  makeDivergence();
  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side");
  const GitIntegrationOption *rebase =
      optionFor(advice, GitIntegrationIntent::ReplayMineOnTop);

  QVERIFY(rebase->isAvailable());
  QVERIFY(rebase->rewritesHistory);
  QVERIFY2(rebase->consequence.contains("new hashes"),
           qPrintable(rebase->consequence));

  const GitIntegrationOption *merge =
      optionFor(advice, GitIntegrationIntent::BringEverythingIn);
  QVERIFY(!merge->rewritesHistory);
}

void TestGitIntegrationAdvice::testRebaseUnavailableWithoutOwnCommits() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));

  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side");
  const GitIntegrationOption *rebase =
      optionFor(advice, GitIntegrationIntent::ReplayMineOnTop);
  QVERIFY(!rebase->isAvailable());
  QVERIFY2(rebase->unavailableReason.contains("no commits of your own"),
           qPrintable(rebase->unavailableReason));
}

void TestGitIntegrationAdvice::testCherryPickNeedsACommit() {
  makeDivergence();
  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side");
  const GitIntegrationOption *pick =
      optionFor(advice, GitIntegrationIntent::BringOneCommit);
  QVERIFY(!pick->isAvailable());
  QVERIFY2(pick->unavailableReason.contains("which commit"),
           qPrintable(pick->unavailableReason));
}

void TestGitIntegrationAdvice::testCherryPickAvailableWithACommit() {
  makeDivergence();
  const QString hash = m_git->getCommitLogPage("side", 0, 1).first().hash;

  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side", hash);
  const GitIntegrationOption *pick =
      optionFor(advice, GitIntegrationIntent::BringOneCommit);
  QVERIFY(pick->isAvailable());
  QVERIFY2(pick->commandName.contains(hash.left(7)),
           qPrintable(pick->commandName));
  QVERIFY2(pick->consequence.contains("copying a change, not moving history"),
           qPrintable(pick->consequence));
}

void TestGitIntegrationAdvice::testApplyOptionCreatesNoCommit() {
  makeDivergence();
  const QString hash = m_git->getCommitLogPage("side", 0, 1).first().hash;
  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "side", hash);

  const GitIntegrationOption *apply =
      optionFor(advice, GitIntegrationIntent::ApplySelectedChanges);
  QVERIFY(apply->isAvailable());
  QVERIFY(!apply->createsCommit);
  QVERIFY(!apply->hasOperationPreview);
  QVERIFY2(apply->consequence.contains("without committing"),
           qPrintable(apply->consequence));
  QVERIFY2(apply->commandName.contains("-n"), qPrintable(apply->commandName));
}

void TestGitIntegrationAdvice::testMergeUnavailableWhenNothingIncoming() {
  QVERIFY(git({"branch", "copy"}));
  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "copy");
  const GitIntegrationOption *merge =
      optionFor(advice, GitIntegrationIntent::BringEverythingIn);
  QVERIFY(!merge->isAvailable());
  QVERIFY2(merge->unavailableReason.contains("nothing you do not already"),
           qPrintable(merge->unavailableReason));
}

void TestGitIntegrationAdvice::testDetachedHeadIsRefused() {
  QVERIFY(git({"checkout", "--detach", "HEAD"}));
  const GitIntegrationAdvice advice = adviseGitIntegration(m_git, "main");
  QVERIFY(!advice.valid);
  QVERIFY2(advice.error.contains("detached"), qPrintable(advice.error));
}

void TestGitIntegrationAdvice::testInvalidRepositoryIsRefused() {
  GitIntegration empty;
  const GitIntegrationAdvice advice = adviseGitIntegration(&empty, "main");
  QVERIFY(!advice.valid);
  QVERIFY(!advice.error.isEmpty());

  const GitIntegrationAdvice noSource = adviseGitIntegration(m_git, QString());
  QVERIFY(!noSource.valid);
}

QTEST_MAIN(TestGitIntegrationAdvice)
#include "test_gitintegrationadvice.moc"
