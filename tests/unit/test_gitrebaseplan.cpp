#include "git/gitintegration.h"
#include "git/gitrebaseplan.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

GitRebaseEntry entryFor(const QString &hash, const QString &subject,
                        GitRebaseAction action = GitRebaseAction::Pick) {
  GitRebaseEntry entry;
  entry.commit.hash = hash;
  entry.commit.shortHash = hash.left(7);
  entry.commit.subject = subject;
  entry.action = action;
  return entry;
}

} // namespace

class TestGitRebasePlan : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testActionKeywordsRoundTrip();
  void testActionExplanationsAreDistinct();
  void testAutosquashTargetSubject();

  void testTodoKeepsOrder();
  void testDropIsOmittedFromTodo();
  void testReorder();
  void testResultingCommitsLoseTheirHashes();
  void testSquashDisappearsFromTheResult();
  void testValidationRejectsLeadingSquash();
  void testValidationRejectsDroppingEverything();
  void testValidationAcceptsAReasonablePlan();
  void testPendingMessagesForReword();
  void testPendingMessagesOncePerSquashGroup();
  void testAutosquashMovesAndSetsAction();
  void testPublishedHistoryIsFlagged();

  void testRebaseSquashesAgainstARealRepository();
  void testRebaseRewordsAgainstARealRepository();
  void testRebaseDropsAgainstARealRepository();
  void testRebaseReordersAgainstARealRepository();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  QString gitOut(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  void makeCommits(const QStringList &subjects);
  GitRebasePlan planFromLog(int count);
};

void TestGitRebasePlan::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("base.txt", "base\n");
  QVERIFY(git({"add", "base.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitRebasePlan::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitRebasePlan::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

QString TestGitRebasePlan::gitOut(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  process.waitForFinished(GIT_COMMAND_TIMEOUT_MS);
  return QString::fromUtf8(process.readAllStandardOutput());
}

void TestGitRebasePlan::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitRebasePlan::makeCommits(const QStringList &subjects) {
  int index = 0;
  for (const QString &subject : subjects) {
    writeFile(QStringLiteral("f%1.txt").arg(index++), subject + "\n");
    QVERIFY(git({"add", "."}));
    QVERIFY(git({"commit", "-m", subject}));
  }
}

GitRebasePlan TestGitRebasePlan::planFromLog(int count) {

  QList<GitCommitInfo> commits = m_git->getCommitLogPage("HEAD", 0, count);
  std::reverse(commits.begin(), commits.end());

  QList<GitRebaseEntry> entries;
  for (const GitCommitInfo &commit : commits) {
    GitRebaseEntry entry;
    entry.commit = commit;
    entries.append(entry);
  }

  GitRebasePlan plan;
  plan.setEntries(entries);
  return plan;
}

void TestGitRebasePlan::testActionKeywordsRoundTrip() {
  const QList<GitRebaseAction> actions{
      GitRebaseAction::Pick,   GitRebaseAction::Reword, GitRebaseAction::Edit,
      GitRebaseAction::Squash, GitRebaseAction::Fixup,  GitRebaseAction::Drop};
  for (GitRebaseAction action : actions) {
    QCOMPARE(gitRebaseActionFromKeyword(gitRebaseActionKeyword(action)),
             action);
  }
  QCOMPARE(gitRebaseActionFromKeyword("s"), GitRebaseAction::Squash);
  QCOMPARE(gitRebaseActionFromKeyword("nonsense"), GitRebaseAction::Pick);
}

void TestGitRebasePlan::testActionExplanationsAreDistinct() {
  QSet<QString> seen;
  for (GitRebaseAction action :
       {GitRebaseAction::Pick, GitRebaseAction::Reword, GitRebaseAction::Edit,
        GitRebaseAction::Squash, GitRebaseAction::Fixup,
        GitRebaseAction::Drop}) {
    const QString explanation = gitRebaseActionExplanation(action);
    QVERIFY(!explanation.isEmpty());
    QVERIFY(!seen.contains(explanation));
    seen.insert(explanation);
  }

  QVERIFY(
      gitRebaseActionExplanation(GitRebaseAction::Pick).contains("new hash"));
  QVERIFY(gitRebaseActionExplanation(GitRebaseAction::Drop)
              .contains("removes work"));
}

void TestGitRebasePlan::testAutosquashTargetSubject() {
  QCOMPARE(autosquashTargetSubject("fixup! Add login"), QString("Add login"));
  QCOMPARE(autosquashTargetSubject("squash! Add login"), QString("Add login"));
  QVERIFY(autosquashTargetSubject("Add login").isEmpty());
}

void TestGitRebasePlan::testTodoKeepsOrder() {
  GitRebasePlan plan;
  plan.setEntries(
      {entryFor("aaa1111", "first"), entryFor("bbb2222", "second")});

  const QStringList lines = plan.todoText().split('\n', Qt::SkipEmptyParts);
  QCOMPARE(lines.size(), 2);
  QVERIFY(lines.at(0).startsWith("pick aaa1111 first"));
  QVERIFY(lines.at(1).startsWith("pick bbb2222 second"));
}

void TestGitRebasePlan::testDropIsOmittedFromTodo() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "first"),
                   entryFor("bbb2222", "second", GitRebaseAction::Drop)});

  const QString todo = plan.todoText();
  QVERIFY(todo.contains("aaa1111"));
  QVERIFY(!todo.contains("bbb2222"));
}

void TestGitRebasePlan::testReorder() {
  GitRebasePlan plan;
  plan.setEntries(
      {entryFor("aaa1111", "first"), entryFor("bbb2222", "second")});

  QVERIFY(plan.moveDown(0));
  QCOMPARE(plan.entries().first().commit.subject, QString("second"));
  QVERIFY(plan.moveUp(1));
  QCOMPARE(plan.entries().first().commit.subject, QString("first"));

  QVERIFY(!plan.moveUp(0));
  QVERIFY(!plan.moveDown(1));
}

void TestGitRebasePlan::testResultingCommitsLoseTheirHashes() {
  GitRebasePlan plan;
  plan.setEntries(
      {entryFor("aaa1111", "first"), entryFor("bbb2222", "second")});

  const QList<GitCommitInfo> result = plan.resultingCommits();
  QCOMPARE(result.size(), 2);
  for (const GitCommitInfo &commit : result) {
    QVERIFY(commit.hash.isEmpty());
    QCOMPARE(commit.shortHash, QString("new"));
  }
}

void TestGitRebasePlan::testSquashDisappearsFromTheResult() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "first"),
                   entryFor("bbb2222", "second", GitRebaseAction::Squash),
                   entryFor("ccc3333", "third", GitRebaseAction::Drop)});

  const QList<GitCommitInfo> result = plan.resultingCommits();
  QCOMPARE(result.size(), 1);
  QCOMPARE(result.first().subject, QString("first"));
}

void TestGitRebasePlan::testValidationRejectsLeadingSquash() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "first", GitRebaseAction::Squash),
                   entryFor("bbb2222", "second")});

  const QStringList problems = plan.validationProblems();
  QCOMPARE(problems.size(), 1);
  QVERIFY2(problems.first().contains("no commit above it"),
           qPrintable(problems.first()));
}

void TestGitRebasePlan::testValidationRejectsDroppingEverything() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "first", GitRebaseAction::Drop),
                   entryFor("bbb2222", "second", GitRebaseAction::Drop)});
  QVERIFY2(plan.validationProblems().join("\n").contains("Every commit is "
                                                         "dropped"),
           qPrintable(plan.validationProblems().join("\n")));
}

void TestGitRebasePlan::testValidationAcceptsAReasonablePlan() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "first"),
                   entryFor("bbb2222", "second", GitRebaseAction::Fixup),
                   entryFor("ccc3333", "third", GitRebaseAction::Reword)});
  QVERIFY(plan.validationProblems().isEmpty());
}

void TestGitRebasePlan::testPendingMessagesForReword() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "first"),
                   entryFor("bbb2222", "second", GitRebaseAction::Reword)});
  plan.setMessage(1, "A better message");

  QCOMPARE(plan.pendingMessages(), QStringList({"A better message"}));
}

void TestGitRebasePlan::testPendingMessagesOncePerSquashGroup() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "first"),
                   entryFor("bbb2222", "second", GitRebaseAction::Squash),
                   entryFor("ccc3333", "third", GitRebaseAction::Squash),
                   entryFor("ddd4444", "fourth")});
  plan.setMessage(2, "Combined");

  QCOMPARE(plan.pendingMessages(), QStringList({"Combined"}));
}

void TestGitRebasePlan::testAutosquashMovesAndSetsAction() {
  GitRebasePlan plan;
  plan.setEntries({entryFor("aaa1111", "Add login"),
                   entryFor("bbb2222", "Unrelated change"),
                   entryFor("ccc3333", "fixup! Add login")});

  QCOMPARE(plan.applyAutosquash(), 1);
  QCOMPARE(plan.entries().size(), 3);
  QCOMPARE(plan.entries().at(0).commit.subject, QString("Add login"));
  QCOMPARE(plan.entries().at(1).commit.subject, QString("fixup! Add login"));
  QCOMPARE(plan.entries().at(1).action, GitRebaseAction::Fixup);
  QCOMPARE(plan.entries().at(2).commit.subject, QString("Unrelated change"));
}

void TestGitRebasePlan::testPublishedHistoryIsFlagged() {
  GitRebasePlan plan;
  GitRebaseEntry published = entryFor("aaa1111", "first");
  published.published = true;
  plan.setEntries({published, entryFor("bbb2222", "second")});
  QVERIFY(plan.touchesPublishedHistory());

  GitRebasePlan localOnly;
  localOnly.setEntries({entryFor("aaa1111", "first")});
  QVERIFY(!localOnly.touchesPublishedHistory());
}

void TestGitRebasePlan::testRebaseSquashesAgainstARealRepository() {
  makeCommits({"first", "second", "third"});

  GitRebasePlan plan = planFromLog(3);
  QCOMPARE(plan.entries().size(), 3);
  plan.setAction(2, GitRebaseAction::Squash);
  plan.setMessage(2, "second and third together");

  QVERIFY(m_git->startInteractiveRebase("HEAD~3", plan));
  QVERIFY(!m_git->isRebaseInProgress());

  const QString log = gitOut({"log", "--format=%s", "-4"});
  QVERIFY2(log.contains("second and third together"), qPrintable(log));
  QVERIFY2(!log.contains("\nthird\n"), qPrintable(log));
}

void TestGitRebasePlan::testRebaseRewordsAgainstARealRepository() {
  makeCommits({"first", "second"});

  GitRebasePlan plan = planFromLog(2);
  plan.setAction(1, GitRebaseAction::Reword);
  plan.setMessage(1, "second, described properly");

  QVERIFY(m_git->startInteractiveRebase("HEAD~2", plan));

  const QString log = gitOut({"log", "--format=%s", "-1"});
  QVERIFY2(log.contains("second, described properly"), qPrintable(log));
}

void TestGitRebasePlan::testRebaseDropsAgainstARealRepository() {
  makeCommits({"first", "second", "third"});

  GitRebasePlan plan = planFromLog(3);
  plan.setAction(1, GitRebaseAction::Drop);

  QVERIFY(m_git->startInteractiveRebase("HEAD~3", plan));

  const QString log = gitOut({"log", "--format=%s", "-3"});
  QVERIFY2(!log.contains("second"), qPrintable(log));
  QVERIFY2(log.contains("third"), qPrintable(log));
  QVERIFY2(log.contains("first"), qPrintable(log));
}

void TestGitRebasePlan::testRebaseReordersAgainstARealRepository() {

  makeCommits({"first", "second", "third"});

  GitRebasePlan plan = planFromLog(3);
  QVERIFY(plan.moveUp(2));
  QCOMPARE(plan.entries().at(1).commit.subject, QString("third"));

  QVERIFY(m_git->startInteractiveRebase("HEAD~3", plan));

  const QStringList log =
      gitOut({"log", "--format=%s", "-3"}).split('\n', Qt::SkipEmptyParts);
  QCOMPARE(log.at(0), QString("second"));
  QCOMPARE(log.at(1), QString("third"));
  QCOMPARE(log.at(2), QString("first"));
}

QTEST_MAIN(TestGitRebasePlan)
#include "test_gitrebaseplan.moc"
