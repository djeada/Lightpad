#include "git/gitintegration.h"
#include "git/gitstashshelf.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitStashShelf : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testApplyAndPopAreExplainedDifferently();
  void testConflictSummaryWording();

  void testEmptyShelf();
  void testStashCardCarriesItsContext();
  void testUntrackedInclusionIsRecorded();
  void testStashFilesAndStats();
  void testSeveralStashesKeepTheirOrder();
  void testPartialStashOfSelectedPaths();
  void testRestoreSelectedPathsLeavesTheStash();
  void testConflictPredictionFindsAClash();
  void testConflictPredictionIsQuietWhenClean();
  void testStashBranchAppliesOnTheBase();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  QString readFile(const QString &name);
};

void TestGitStashShelf::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("a.txt", "a base\n");
  writeFile("b.txt", "b base\n");
  QVERIFY(git({"add", "."}));
  QVERIFY(git({"commit", "-m", "base commit"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitStashShelf::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitStashShelf::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitStashShelf::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

QString TestGitStashShelf::readFile(const QString &name) {
  QFile file(m_repoPath + "/" + name);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

void TestGitStashShelf::testApplyAndPopAreExplainedDifferently() {
  const QString apply = gitStashApplyExplanation(false);
  const QString pop = gitStashApplyExplanation(true);
  QVERIFY(apply != pop);
  QVERIFY2(apply.contains("leaves the stash entry alone"), qPrintable(apply));
  QVERIFY2(pop.contains("only if applying succeeded"), qPrintable(pop));
}

void TestGitStashShelf::testConflictSummaryWording() {
  QVERIFY(gitStashConflictSummary({"a.txt"}, true)
              .contains("a conflict in "
                        "a.txt"));
  QVERIFY(gitStashConflictSummary({}, true).contains("apply cleanly"));

  QVERIFY(gitStashConflictSummary({}, false).contains("different commit"));
}

void TestGitStashShelf::testEmptyShelf() {
  QVERIFY(buildStashShelf(m_git).isEmpty());
}

void TestGitStashShelf::testStashCardCarriesItsContext() {
  const QString base = m_git->getCommitDetails("HEAD").hash;
  writeFile("a.txt", "a changed\n");
  QVERIFY(git({"stash", "push", "-m", "the login work"}));

  const QList<GitStashCard> cards = buildStashShelf(m_git);
  QCOMPARE(cards.size(), 1);

  const GitStashCard &card = cards.first();
  QCOMPARE(card.selector, QString("stash@{0}"));
  QCOMPARE(card.message, QString("the login work"));
  QCOMPARE(card.branch, QString("main"));
  QCOMPARE(card.baseHash, base);
  QCOMPARE(card.baseSubject, QString("base commit"));
  QVERIFY(!card.relativeDate.isEmpty());
}

void TestGitStashShelf::testUntrackedInclusionIsRecorded() {
  writeFile("a.txt", "a changed\n");
  QVERIFY(git({"stash", "push", "-m", "tracked only"}));
  QCOMPARE(buildStashShelf(m_git).first().includesUntracked, false);

  writeFile("a.txt", "a changed again\n");
  writeFile("brand-new.txt", "new\n");
  QVERIFY(git({"stash", "push", "-u", "-m", "with untracked"}));

  const QList<GitStashCard> cards = buildStashShelf(m_git);
  QCOMPARE(cards.size(), 2);
  QVERIFY(cards.at(0).includesUntracked);
  QVERIFY(!cards.at(1).includesUntracked);
}

void TestGitStashShelf::testStashFilesAndStats() {
  writeFile("a.txt", "a base\nextra line\n");
  QVERIFY(git({"stash", "push", "-m", "one file"}));

  const GitStashCard card = buildStashShelf(m_git).first();
  QCOMPARE(card.files.size(), 1);
  QCOMPARE(card.files.first().path, QString("a.txt"));
  QCOMPARE(card.additions(), 1);
  QCOMPARE(card.deletions(), 0);
}

void TestGitStashShelf::testSeveralStashesKeepTheirOrder() {
  writeFile("a.txt", "first stash\n");
  QVERIFY(git({"stash", "push", "-m", "first"}));
  writeFile("a.txt", "second stash\n");
  QVERIFY(git({"stash", "push", "-m", "second"}));

  const QList<GitStashCard> cards = buildStashShelf(m_git);
  QCOMPARE(cards.size(), 2);

  QCOMPARE(cards.at(0).message, QString("second"));
  QCOMPARE(cards.at(0).index, 0);
  QCOMPARE(cards.at(1).message, QString("first"));
  QCOMPARE(cards.at(1).index, 1);
}

void TestGitStashShelf::testPartialStashOfSelectedPaths() {
  writeFile("a.txt", "a changed\n");
  writeFile("b.txt", "b changed\n");

  QVERIFY(m_git->stashPaths({"a.txt"}, "only a"));

  const GitStashCard card = buildStashShelf(m_git).first();
  QCOMPARE(card.files.size(), 1);
  QCOMPARE(card.files.first().path, QString("a.txt"));

  QCOMPARE(readFile("a.txt"), QString("a base\n"));
  QCOMPARE(readFile("b.txt"), QString("b changed\n"));
}

void TestGitStashShelf::testRestoreSelectedPathsLeavesTheStash() {
  writeFile("a.txt", "a changed\n");
  writeFile("b.txt", "b changed\n");
  QVERIFY(git({"stash", "push", "-m", "both files"}));

  QVERIFY(m_git->restoreStashPaths(0, {"a.txt"}));

  QCOMPARE(readFile("a.txt"), QString("a changed\n"));
  QCOMPARE(readFile("b.txt"), QString("b base\n"));

  QCOMPARE(buildStashShelf(m_git).size(), 1);
}

void TestGitStashShelf::testConflictPredictionFindsAClash() {
  writeFile("a.txt", "stashed version\n");
  QVERIFY(git({"stash", "push", "-m", "stashed edit"}));

  writeFile("a.txt", "committed version\n");
  QVERIFY(git({"commit", "-am", "conflicting change"}));

  const GitStashCard card = buildStashShelf(m_git).first();
  const QStringList conflicts = predictStashConflicts(m_git, card);
  QVERIFY2(conflicts.contains("a.txt"), qPrintable(conflicts.join(",")));

  QCOMPARE(readFile("a.txt"), QString("committed version\n"));
  QCOMPARE(buildStashShelf(m_git).size(), 1);
}

void TestGitStashShelf::testConflictPredictionIsQuietWhenClean() {
  writeFile("a.txt", "a stashed\n");
  QVERIFY(git({"stash", "push", "-m", "clean stash"}));

  const GitStashCard card = buildStashShelf(m_git).first();
  QVERIFY(predictStashConflicts(m_git, card).isEmpty());
}

void TestGitStashShelf::testStashBranchAppliesOnTheBase() {
  const QString base = m_git->getCommitDetails("HEAD").hash;
  writeFile("a.txt", "stashed work\n");
  QVERIFY(git({"stash", "push", "-m", "work"}));

  writeFile("b.txt", "moved on\n");
  QVERIFY(git({"commit", "-am", "later commit"}));

  QVERIFY(m_git->stashBranch("stash/work", 0));

  QCOMPARE(m_git->currentBranch(), QString("stash/work"));

  QCOMPARE(m_git->getCommitDetails("HEAD").hash, base);
  QCOMPARE(readFile("a.txt"), QString("stashed work\n"));
  QVERIFY(buildStashShelf(m_git).isEmpty());
}

QTEST_MAIN(TestGitStashShelf)
#include "test_gitstashshelf.moc"
