#include "git/gitmergeplan.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitMergePlan : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testChoicesExcludeTheCurrentBranch();
  void testChoicesGroupLocalAndRemoteCopies();
  void testSplitCopiesAreFlaggedWhenTheyDisagree();
  void testPlanForAFastForward();
  void testPlanForDivergedBranches();
  void testPlanWhenAlreadyUpToDate();
  void testPlanForAnUnknownBranch();
  void testDirtyWorkingTreeBlocksTheMerge();
  void testPlanPredictsConflictingFiles();
  void testHeadlineNamesBothBranches();
  void testPreferenceExplanationsNameTheSides();
  void testMergeWithPreferTheirsResolvesAutomatically();
  void testMergeWithManualPreferenceStopsAtTheConflict();
  void testWriteWorkingFileDoesNotStage();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
  void makeDivergence();
};

bool TestGitMergePlan::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start(QStringLiteral("git"), args);
  if (!process.waitForFinished(15000)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitMergePlan::writeFile(const QString &name, const QString &content) {
  QFile file(QDir(m_repoPath).filePath(name));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(content.toUtf8());
  file.close();
}

void TestGitMergePlan::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "ada@example.com"}));
  QVERIFY(git({"config", "user.name", "Ada"}));
  writeFile("f.txt", "one\ntwo\nthree\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base commit"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitMergePlan::cleanup() {
  delete m_git;
  m_git = nullptr;
}

void TestGitMergePlan::makeDivergence() {
  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("f.txt", "one\nfeature\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("f.txt", "one\nmain\nthree\n");
  QVERIFY(git({"commit", "-am", "main edit"}));
  m_git->refresh();
}

void TestGitMergePlan::testChoicesExcludeTheCurrentBranch() {
  QVERIFY(git({"branch", "feature"}));
  m_git->refresh();

  const QList<GitMergeChoice> choices = gitMergeChoices(m_git);
  QStringList names;
  for (const GitMergeChoice &choice : choices) {
    names << choice.name;
  }
  QVERIFY(names.contains(QStringLiteral("feature")));
  QVERIFY(!names.contains(QStringLiteral("main")));
}

void TestGitMergePlan::testChoicesGroupLocalAndRemoteCopies() {

  const QString remotePath = m_tempDir.path() + "/remote.git";
  QVERIFY(QDir().mkpath(remotePath));
  QVERIFY(git({"init", "--bare"}, remotePath));

  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("f.txt", "one\nfeature\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"remote", "add", "origin", remotePath}));
  QVERIFY(git({"push", "origin", "feature"}));
  QVERIFY(git({"checkout", "main"}));
  m_git->refresh();

  const QList<GitMergeChoice> choices = gitMergeChoices(m_git);
  const GitMergeChoice *feature = nullptr;
  for (const GitMergeChoice &choice : choices) {
    if (choice.name == QStringLiteral("feature")) {
      feature = &choice;
    }
  }
  QVERIFY(feature);
  QVERIFY(feature->hasLocal());
  QVERIFY(feature->hasRemote());
  QVERIFY(feature->isSplit());

  QVERIFY(feature->preferred());
  QVERIFY(!feature->preferred()->isRemote);
  QCOMPARE(feature->preferred()->ref, QStringLiteral("feature"));

  QVERIFY(!feature->copiesDisagree());
}

void TestGitMergePlan::testSplitCopiesAreFlaggedWhenTheyDisagree() {
  const QString remotePath = m_tempDir.path() + "/remote2.git";
  QVERIFY(QDir().mkpath(remotePath));
  QVERIFY(git({"init", "--bare"}, remotePath));

  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("f.txt", "one\nfeature\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"remote", "add", "origin", remotePath}));
  QVERIFY(git({"push", "origin", "feature"}));

  writeFile("f.txt", "one\nfeature again\nthree\n");
  QVERIFY(git({"commit", "-am", "second feature edit"}));
  QVERIFY(git({"checkout", "main"}));
  m_git->refresh();

  for (const GitMergeChoice &choice : gitMergeChoices(m_git)) {
    if (choice.name == QStringLiteral("feature")) {
      QVERIFY(choice.copiesDisagree());
      return;
    }
  }
  QFAIL("feature branch was not offered");
}

void TestGitMergePlan::testPlanForAFastForward() {
  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("g.txt", "new file\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "add g"}));
  QVERIFY(git({"checkout", "main"}));
  m_git->refresh();

  const GitMergePlan plan = buildGitMergePlan(m_git, QStringLiteral("feature"));
  QVERIFY(plan.valid);
  QCOMPARE(plan.relation, GitMergeRelation::FastForward);
  QCOMPARE(plan.incomingCommits, 1);
  QCOMPARE(plan.localOnlyCommits, 0);
  QCOMPARE(plan.changedFiles, QStringList() << "g.txt");
  QVERIFY(plan.likelyConflicts.isEmpty());
  QVERIFY(plan.canStart());
  QVERIFY(plan.outcomeSentence().contains(QStringLiteral("main")));
}

void TestGitMergePlan::testPlanForDivergedBranches() {
  makeDivergence();

  const GitMergePlan plan = buildGitMergePlan(m_git, QStringLiteral("feature"));
  QVERIFY(plan.valid);
  QCOMPARE(plan.relation, GitMergeRelation::Diverged);
  QCOMPARE(plan.incomingCommits, 1);
  QCOMPARE(plan.localOnlyCommits, 1);
  QCOMPARE(plan.sourceRef, QStringLiteral("feature"));
  QCOMPARE(plan.targetRef, QStringLiteral("main"));
  QVERIFY(!plan.mergeBase.isEmpty());
  QCOMPARE(plan.mergeBaseSubject, QStringLiteral("base commit"));
  QCOMPARE(plan.incomingLog.size(), 1);
  QVERIFY(plan.canStart());
}

void TestGitMergePlan::testPlanWhenAlreadyUpToDate() {
  QVERIFY(git({"branch", "feature"}));
  m_git->refresh();

  const GitMergePlan plan = buildGitMergePlan(m_git, QStringLiteral("feature"));
  QVERIFY(plan.valid);
  QCOMPARE(plan.relation, GitMergeRelation::AlreadyUpToDate);
  QCOMPARE(plan.incomingCommits, 0);
  QVERIFY(!plan.canStart());
  QVERIFY(
      plan.outcomeSentence().contains(QStringLiteral("Nothing will change")));
}

void TestGitMergePlan::testPlanForAnUnknownBranch() {
  const GitMergePlan plan = buildGitMergePlan(m_git, QStringLiteral("nope"));
  QVERIFY(!plan.valid);
  QVERIFY(!plan.canStart());
  QCOMPARE(plan.blockers.size(), 1);
  QVERIFY(plan.blockers.first().contains(QStringLiteral("nope")));
}

void TestGitMergePlan::testDirtyWorkingTreeBlocksTheMerge() {
  makeDivergence();
  writeFile("f.txt", "one\nuncommitted\nthree\n");

  const GitMergePlan plan = buildGitMergePlan(m_git, QStringLiteral("feature"));
  QVERIFY(plan.valid);
  QVERIFY(plan.workingTreeDirty);
  QVERIFY(!plan.canStart());
  QCOMPARE(plan.blockers.size(), 1);
  QVERIFY(plan.blockers.first().contains(QStringLiteral("not committed yet")));
}

void TestGitMergePlan::testPlanPredictsConflictingFiles() {
  makeDivergence();

  const GitMergePlan plan = buildGitMergePlan(m_git, QStringLiteral("feature"));

  if (!plan.likelyConflicts.isEmpty()) {
    QCOMPARE(plan.likelyConflicts, QStringList() << "f.txt");
    QVERIFY(plan.conflictSentence().contains(QStringLiteral("decision")));
  }
}

void TestGitMergePlan::testHeadlineNamesBothBranches() {
  makeDivergence();

  const GitMergePlan plan = buildGitMergePlan(m_git, QStringLiteral("feature"));
  QVERIFY(plan.headline().contains(QStringLiteral("feature")));
  QVERIFY(plan.headline().contains(QStringLiteral("main")));
}

void TestGitMergePlan::testPreferenceExplanationsNameTheSides() {
  const QString ours = QStringLiteral("main");
  const QString theirs = QStringLiteral("feature");

  const QString manual = gitMergePreferenceExplanation(
      GitMergeOptions::Preference::Manual, ours, theirs);
  const QString keepOurs = gitMergePreferenceExplanation(
      GitMergeOptions::Preference::PreferOurs, ours, theirs);
  const QString keepTheirs = gitMergePreferenceExplanation(
      GitMergeOptions::Preference::PreferTheirs, ours, theirs);

  QVERIFY(!manual.isEmpty());
  QVERIFY(keepOurs.contains(ours));
  QVERIFY(keepOurs.contains(theirs));
  QVERIFY(keepTheirs.contains(ours));
  QVERIFY(keepTheirs != keepOurs);
}

void TestGitMergePlan::testMergeWithPreferTheirsResolvesAutomatically() {
  makeDivergence();

  GitMergeOptions options;
  options.preference = GitMergeOptions::Preference::PreferTheirs;
  QVERIFY(m_git->mergeBranchWithOptions(QStringLiteral("feature"), options));
  QVERIFY(!m_git->hasMergeConflicts());

  QFile file(QDir(m_repoPath).filePath("f.txt"));
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(file.readAll()),
           QStringLiteral("one\nfeature\nthree\n"));
}

void TestGitMergePlan::testMergeWithManualPreferenceStopsAtTheConflict() {
  makeDivergence();

  GitMergeOptions options;
  options.preference = GitMergeOptions::Preference::Manual;
  QVERIFY(!m_git->mergeBranchWithOptions(QStringLiteral("feature"), options));
  QVERIFY(m_git->hasMergeConflicts());
  QCOMPARE(m_git->getConflictedFiles(), QStringList() << "f.txt");
}

void TestGitMergePlan::testWriteWorkingFileDoesNotStage() {
  makeDivergence();

  GitMergeOptions options;
  m_git->mergeBranchWithOptions(QStringLiteral("feature"), options);
  QVERIFY(m_git->hasMergeConflicts());

  QVERIFY(m_git->writeWorkingFile(QStringLiteral("f.txt"),
                                  QStringLiteral("one\nhalf done\nthree\n")));
  QVERIFY(m_git->hasMergeConflicts());

  QVERIFY(m_git->resolveConflictWith(QStringLiteral("f.txt"),
                                     QStringLiteral("one\nsettled\nthree\n")));
  QVERIFY(!m_git->hasMergeConflicts());
}

QTEST_MAIN(TestGitMergePlan)
#include "test_gitmergeplan.moc"
