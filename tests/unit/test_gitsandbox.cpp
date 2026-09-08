#include "git/gitintegration.h"
#include "git/gitsandbox.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitSandbox : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testLoadsTheGraph();
  void testMergeFastForwardMovesTheRef();
  void testMergeCreatesASyntheticCommit();
  void testRebaseCreatesNewCommitsAndOrphansTheOld();
  void testResetOrphansWhatItPassed();
  void testCherryPickCopiesRatherThanMoves();
  void testDeleteRefReportsOrphans();
  void testDeleteMergedRefOrphansNothing();
  void testResetRestoresTheLoadedGraph();
  void testPlanMirrorsTheSimulation();
  void testLimitationsAreStated();
  void testRepositoryIsNeverTouched();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  QString gitOut(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitSandbox::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("f.txt", "base\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("g.txt", "g\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "feature one"}));
  writeFile("h.txt", "h\n");
  QVERIFY(git({"add", "h.txt"}));
  QVERIFY(git({"commit", "-m", "feature two"}));
  QVERIFY(git({"checkout", "main"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitSandbox::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitSandbox::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

QString TestGitSandbox::gitOut(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  process.waitForFinished(GIT_COMMAND_TIMEOUT_MS);
  return QString::fromUtf8(process.readAllStandardOutput());
}

void TestGitSandbox::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitSandbox::testLoadsTheGraph() {
  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  QVERIFY(sandbox.isLoaded());
  QCOMPARE(sandbox.commits().size(), 3);
  QVERIFY(sandbox.refByName("main"));
  QVERIFY(sandbox.refByName("feature"));
  QVERIFY(sandbox.refByName("main")->isHead);
  QVERIFY(!sandbox.isModified());
}

void TestGitSandbox::testMergeFastForwardMovesTheRef() {
  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  const QString featureTip = sandbox.refByName("feature")->commitId;

  const QString message = sandbox.simulateMerge("main", "feature");
  QVERIFY2(message.contains("fast-forwards"), qPrintable(message));
  QCOMPARE(sandbox.refByName("main")->commitId, featureTip);

  QCOMPARE(sandbox.commits().size(), 3);
}

void TestGitSandbox::testMergeCreatesASyntheticCommit() {
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));

  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  const QString message = sandbox.simulateMerge("main", "feature");

  QVERIFY2(message.contains("merge commit"), qPrintable(message));
  QCOMPARE(sandbox.commits().size(), 5);
  QVERIFY(sandbox.commits().first().synthetic);
  QCOMPARE(sandbox.commits().first().parentIds.size(), 2);
  QVERIFY(sandbox.unreachableCommitIds().isEmpty());
}

void TestGitSandbox::testRebaseCreatesNewCommitsAndOrphansTheOld() {
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));

  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  const QString message = sandbox.simulateRebase("feature", "main");

  QVERIFY2(message.contains("recreated"), qPrintable(message));
  QVERIFY2(message.contains("copies, it does not move"), qPrintable(message));

  int synthetic = 0;
  for (const SandboxCommit &commit : sandbox.commits()) {
    if (commit.synthetic) {
      ++synthetic;
    }
  }
  QCOMPARE(synthetic, 2);

  QCOMPARE(sandbox.unreachableCommitIds().size(), 2);
}

void TestGitSandbox::testResetOrphansWhatItPassed() {
  GitSandbox sandbox;
  sandbox.loadFrom(m_git);

  const QString mainTip = sandbox.refByName("main")->commitId;
  const QString message = sandbox.simulateReset("feature", mainTip);

  QVERIFY2(message.contains("moves back past 2 commits"), qPrintable(message));
  QCOMPARE(sandbox.unreachableCommitIds().size(), 2);
  QVERIFY2(message.contains("reset mode"), qPrintable(message));
}

void TestGitSandbox::testCherryPickCopiesRatherThanMoves() {
  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  const QString featureTip = sandbox.refByName("feature")->commitId;

  const QString message = sandbox.simulateCherryPick("main", featureTip);
  QVERIFY2(message.contains("copies a change, not a commit"),
           qPrintable(message));
  QCOMPARE(sandbox.commits().size(), 4);

  QVERIFY(sandbox.unreachableCommitIds().isEmpty());
}

void TestGitSandbox::testDeleteRefReportsOrphans() {
  GitSandbox sandbox;
  sandbox.loadFrom(m_git);

  const QString message = sandbox.simulateDeleteRef("feature");
  QVERIFY2(message.contains("no ref pointing"), qPrintable(message));
  QVERIFY(!sandbox.refByName("feature"));
  QCOMPARE(sandbox.unreachableCommitIds().size(), 2);
}

void TestGitSandbox::testDeleteMergedRefOrphansNothing() {
  QVERIFY(git({"merge", "feature"}));
  QVERIFY(git({"branch", "spare"}));

  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  const QString message = sandbox.simulateDeleteRef("spare");
  QVERIFY2(message.contains("only a name"), qPrintable(message));
  QVERIFY(sandbox.unreachableCommitIds().isEmpty());
}

void TestGitSandbox::testResetRestoresTheLoadedGraph() {
  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  const int originalCount = sandbox.commits().size();

  sandbox.simulateCherryPick("main", sandbox.refByName("feature")->commitId);
  QVERIFY(sandbox.isModified());
  QVERIFY(sandbox.commits().size() > originalCount);

  sandbox.reset();
  QVERIFY(!sandbox.isModified());
  QCOMPARE(sandbox.commits().size(), originalCount);
  QVERIFY(sandbox.plan().isEmpty());
}

void TestGitSandbox::testPlanMirrorsTheSimulation() {
  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  sandbox.simulateMerge("main", "feature");
  sandbox.simulateDeleteRef("feature");

  const QList<GitOperationRequest> plan = sandbox.plan();
  QCOMPARE(plan.size(), 2);
  QCOMPARE(plan.at(0).kind, GitOperationKind::Merge);
  QCOMPARE(plan.at(0).target, QString("feature"));
  QCOMPARE(plan.at(1).kind, GitOperationKind::BranchDelete);
}

void TestGitSandbox::testLimitationsAreStated() {
  GitSandbox sandbox;
  const QStringList limitations = sandbox.limitations();
  QCOMPARE(limitations.size(), 3);
  QVERIFY(limitations.join("\n").contains("Conflicts are not simulated"));
  QVERIFY(limitations.join("\n").contains("working tree and the index"));
}

void TestGitSandbox::testRepositoryIsNeverTouched() {
  const QString before = gitOut({"log", "--all", "--format=%H %d %s"});
  const QString headBefore = m_git->getCommitDetails("HEAD").hash;

  GitSandbox sandbox;
  sandbox.loadFrom(m_git);
  sandbox.simulateMerge("main", "feature");
  sandbox.simulateRebase("feature", "main");
  sandbox.simulateDeleteRef("feature");
  sandbox.simulateCherryPick("main", sandbox.commits().first().id);

  QCOMPARE(gitOut({"log", "--all", "--format=%H %d %s"}), before);
  QCOMPARE(m_git->getCommitDetails("HEAD").hash, headBefore);
  QVERIFY(m_git->repositoryState().isClean());
}

QTEST_MAIN(TestGitSandbox)
#include "test_gitsandbox.moc"
