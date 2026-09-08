#include "git/gitintegration.h"
#include "git/gittimetravel.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitTimeTravel : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testModeNames();
  void testThreeOptionsWithDistinctConsequences();
  void testOnlyStartBranchMovesTheCheckout();
  void testCommandsNameTheCommit();

  void testAttachedHeadHasNoExplanation();
  void testDetachedHeadIsDetected();
  void testDetachedHeadWithoutOwnCommitsHasNoWarning();
  void testDetachedHeadWithOwnCommitsWarnsAndListsThem();
  void testCommitsWithoutRefIgnoresBranchedWork();
  void testWorktreeDirtySummary();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
};

void TestGitTimeTravel::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("f.txt", "one\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "second"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitTimeTravel::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitTimeTravel::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitTimeTravel::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitTimeTravel::testModeNames() {
  QCOMPARE(gitTimeTravelModeName(GitTimeTravelMode::InspectSnapshot),
           QString("Inspect snapshot"));
  QCOMPARE(gitTimeTravelModeName(GitTimeTravelMode::RunnableWorktree),
           QString("Open runnable snapshot"));
  QCOMPARE(gitTimeTravelModeName(GitTimeTravelMode::StartBranch),
           QString("Start work from here"));
}

void TestGitTimeTravel::testThreeOptionsWithDistinctConsequences() {
  const QList<GitTimeTravelOption> options =
      gitTimeTravelOptions("abc1234def", "abc1234");
  QCOMPARE(options.size(), 3);

  QSet<QString> explanations;
  for (const GitTimeTravelOption &option : options) {
    QVERIFY(!option.title.isEmpty());
    QVERIFY(!option.explanation.isEmpty());
    QVERIFY(!option.command.isEmpty());
    explanations.insert(option.explanation);
  }
  QCOMPARE(explanations.size(), 3);
}

void TestGitTimeTravel::testOnlyStartBranchMovesTheCheckout() {
  const QList<GitTimeTravelOption> options =
      gitTimeTravelOptions("abc1234def", "abc1234");

  int moving = 0;
  for (const GitTimeTravelOption &option : options) {
    if (option.changesCheckout) {
      ++moving;
      QCOMPARE(option.mode, GitTimeTravelMode::StartBranch);
    }
  }

  QCOMPARE(moving, 1);
}

void TestGitTimeTravel::testCommandsNameTheCommit() {
  const QList<GitTimeTravelOption> options =
      gitTimeTravelOptions("abc1234def", "abc1234");
  for (const GitTimeTravelOption &option : options) {
    QVERIFY2(option.command.contains("abc1234"), qPrintable(option.command));
  }
  QVERIFY(options.at(1).command.contains("worktree add"));
  QVERIFY(options.at(2).command.contains("switch -c"));
}

void TestGitTimeTravel::testAttachedHeadHasNoExplanation() {
  const GitDetachedHeadState state = gitDetachedHeadState(m_git);
  QVERIFY(state.valid);
  QVERIFY(!state.detached);
  QVERIFY(gitDetachedHeadExplanation(state).isEmpty());
  QVERIFY(gitDetachedHeadLeaveWarning(state).isEmpty());
}

void TestGitTimeTravel::testDetachedHeadIsDetected() {
  QVERIFY(git({"checkout", "--detach", "HEAD~1"}));

  const GitDetachedHeadState state = gitDetachedHeadState(m_git);
  QVERIFY(state.detached);
  QVERIFY(!state.headShortHash.isEmpty());
  const QString explanation = gitDetachedHeadExplanation(state);
  QVERIFY2(explanation.contains("not on a branch"), qPrintable(explanation));
  QVERIFY2(explanation.contains("nothing names them"), qPrintable(explanation));
}

void TestGitTimeTravel::testDetachedHeadWithoutOwnCommitsHasNoWarning() {
  QVERIFY(git({"checkout", "--detach", "HEAD~1"}));

  const GitDetachedHeadState state = gitDetachedHeadState(m_git);
  QVERIFY(state.detached);

  QVERIFY(!state.hasUnreferencedWork());
  QVERIFY(gitDetachedHeadLeaveWarning(state).isEmpty());
}

void TestGitTimeTravel::testDetachedHeadWithOwnCommitsWarnsAndListsThem() {
  QVERIFY(git({"checkout", "--detach", "HEAD~1"}));
  writeFile("detached.txt", "work\n");
  QVERIFY(git({"add", "detached.txt"}));
  QVERIFY(git({"commit", "-m", "work done while detached"}));

  const GitDetachedHeadState state = gitDetachedHeadState(m_git);
  QVERIFY(state.hasUnreferencedWork());
  QCOMPARE(state.unreferencedCommits.size(), 1);
  QCOMPARE(state.unreferencedCommits.first().subject,
           QString("work done while detached"));

  const QString warning = gitDetachedHeadLeaveWarning(state);
  QVERIFY2(warning.contains("work done while detached"), qPrintable(warning));
  QVERIFY2(warning.contains("give them a name"), qPrintable(warning));
  QVERIFY(gitDetachedHeadExplanation(state).contains("1 such commit"));
}

void TestGitTimeTravel::testCommitsWithoutRefIgnoresBranchedWork() {
  QVERIFY(git({"checkout", "-b", "named"}));
  writeFile("named.txt", "work\n");
  QVERIFY(git({"add", "named.txt"}));
  QVERIFY(git({"commit", "-m", "named work"}));

  QVERIFY(m_git->getCommitsWithoutRef().isEmpty());
}

void TestGitTimeTravel::testWorktreeDirtySummary() {
  const QString worktree =
      m_tempDir.path() + "/wt" +
      QString::number(reinterpret_cast<quintptr>(this) % 1000);
  QVERIFY(git({"worktree", "add", "-b", "wt-branch", worktree}));

  QVERIFY(m_git->worktreeDirtySummary(worktree).isEmpty());

  QFile file(worktree + "/f.txt");
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("dirty\n");
  file.close();

  const QString summary = m_git->worktreeDirtySummary(worktree);
  QVERIFY2(summary.contains("f.txt"), qPrintable(summary));
}

QTEST_MAIN(TestGitTimeTravel)
#include "test_gittimetravel.moc"
