#include "git/gitbisect.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitBisect : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testStatusNames();
  void testParseProgress();
  void testParseFoundCommit();
  void testParseLogCounts();
  void testExitCodeMeanings();
  void testGuidanceChangesWithStatus();

  void testNotBisectingByDefault();
  void testStartMovesToACandidate();
  void testManualBisectFindsTheCommit();
  void testSkipIsRecorded();
  void testResetEndsTheSearch();
  void testAutomatedRunFindsTheCommit();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;
  QStringList m_hashes;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitBisect::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));

  m_git = new GitIntegration;

  m_hashes.clear();
  for (int i = 0; i < 10; ++i) {
    writeFile("marker.txt", i >= 6 ? "broken\n" : "fine\n");
    writeFile(QStringLiteral("f%1.txt").arg(i), QString::number(i) + "\n");
    QVERIFY(git({"add", "."}));
    QVERIFY(git({"commit", "-m", QStringLiteral("commit %1").arg(i)}));
  }

  QVERIFY(m_git->setRepositoryPath(m_repoPath));
  for (const GitCommitInfo &commit :
       m_git->getCommitLogPage(QStringLiteral("HEAD"), 0, 10)) {
    m_hashes.prepend(commit.hash);
  }
}

void TestGitBisect::cleanup() {
  if (m_git && m_git->isBisecting()) {
    m_git->bisectReset();
  }
  delete m_git;
  m_git = nullptr;
}

bool TestGitBisect::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitBisect::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitBisect::testStatusNames() {
  QCOMPARE(gitBisectStatusName(GitBisectStatus::NotStarted),
           QString("Not started"));
  QCOMPARE(gitBisectStatusName(GitBisectStatus::Searching),
           QString("Searching"));
  QCOMPARE(gitBisectStatusName(GitBisectStatus::Found), QString("Found"));
}

void TestGitBisect::testParseProgress() {
  GitBisectState state;
  parseBisectProgress(
      "Bisecting: 4 revisions left to test after this (roughly 2 steps)\n"
      "[abc123] commit 5\n",
      &state);
  QCOMPARE(state.revisionsLeft, 4);
  QCOMPARE(state.estimatedSteps, 2);
  QCOMPARE(state.status, GitBisectStatus::Searching);
}

void TestGitBisect::testParseFoundCommit() {
  GitBisectState state;
  parseBisectProgress("abc1234def5678 is the first bad commit\n"
                      "commit abc1234def5678\n",
                      &state);
  QCOMPARE(state.status, GitBisectStatus::Found);
  QCOMPARE(state.suspectHash, QString("abc1234def5678"));
}

void TestGitBisect::testParseLogCounts() {
  GitBisectState state;
  parseBisectLog("git bisect start 'bad' 'good'\n"
                 "# good: [aaa] commit 1\n"
                 "git bisect good aaa\n"
                 "# bad: [bbb] commit 5\n"
                 "git bisect bad bbb\n"
                 "git bisect skip ccc\n",
                 &state);

  QCOMPARE(state.goodCount, 2);
  QCOMPARE(state.badCount, 2);
  QCOMPARE(state.skipCount, 1);
  QCOMPARE(state.log.size(), 6);
}

void TestGitBisect::testExitCodeMeanings() {
  QVERIFY(gitBisectExitCodeMeaning(0).contains("good"));
  QVERIFY(gitBisectExitCodeMeaning(1).contains("bad"));
  QVERIFY2(gitBisectExitCodeMeaning(125).contains("skipped"),
           qPrintable(gitBisectExitCodeMeaning(125)));
  QVERIFY(gitBisectExitCodeMeaning(200).contains("aborts"));
}

void TestGitBisect::testGuidanceChangesWithStatus() {
  GitBisectState notStarted;
  QVERIFY2(gitBisectGuidance(notStarted).contains("binary search"),
           qPrintable(gitBisectGuidance(notStarted)));

  GitBisectState searching;
  searching.status = GitBisectStatus::Searching;
  searching.currentHash = "abcdef1234";
  searching.revisionsLeft = 3;
  searching.estimatedSteps = 2;
  const QString text = gitBisectGuidance(searching);
  QVERIFY2(text.contains("abcdef1"), qPrintable(text));
  QVERIFY2(text.contains("3 commits left"), qPrintable(text));

  GitBisectState found;
  found.status = GitBisectStatus::Found;
  found.suspectHash = "abcdef1234";
  QVERIFY(gitBisectGuidance(found).contains("first commit where"));
}

void TestGitBisect::testNotBisectingByDefault() {
  QVERIFY(!m_git->isBisecting());
  const GitBisectState state = buildBisectState(m_git);
  QVERIFY(state.valid);
  QCOMPARE(state.status, GitBisectStatus::NotStarted);
}

void TestGitBisect::testStartMovesToACandidate() {
  const QString output = m_git->bisectStart(m_hashes.at(9), m_hashes.at(0));
  QVERIFY(m_git->isBisecting());

  GitBisectState state;
  parseBisectProgress(output, &state);
  QCOMPARE(state.status, GitBisectStatus::Searching);
  QVERIFY2(state.revisionsLeft > 0, qPrintable(output));

  const QString head = m_git->getCommitDetails("HEAD").hash;
  QVERIFY(head != m_hashes.at(9));
  QVERIFY(head != m_hashes.at(0));
}

void TestGitBisect::testManualBisectFindsTheCommit() {
  m_git->bisectStart(m_hashes.at(9), m_hashes.at(0));

  QString output;
  for (int step = 0; step < 12 && m_git->isBisecting(); ++step) {
    QFile marker(m_repoPath + "/marker.txt");
    QVERIFY(marker.open(QIODevice::ReadOnly));
    const bool broken = QString::fromUtf8(marker.readAll()).contains("broken");
    marker.close();

    output = m_git->bisectMark(broken ? QStringLiteral("bad")
                                      : QStringLiteral("good"));
    GitBisectState state;
    parseBisectProgress(output, &state);
    if (state.status == GitBisectStatus::Found) {
      QCOMPARE(state.suspectHash, m_hashes.at(6));
      return;
    }
  }
  QFAIL(qPrintable(QStringLiteral("bisect did not converge: %1").arg(output)));
}

void TestGitBisect::testSkipIsRecorded() {
  m_git->bisectStart(m_hashes.at(9), m_hashes.at(0));
  m_git->bisectMark(QStringLiteral("skip"));

  GitBisectState state;
  parseBisectLog(m_git->bisectLog(), &state);
  QCOMPARE(state.skipCount, 1);
  QVERIFY2(gitBisectGuidance(buildBisectState(m_git)).contains("skipped"),
           qPrintable(gitBisectGuidance(buildBisectState(m_git))));
}

void TestGitBisect::testResetEndsTheSearch() {
  m_git->bisectStart(m_hashes.at(9), m_hashes.at(0));
  QVERIFY(m_git->isBisecting());

  QVERIFY(m_git->bisectReset());
  QVERIFY(!m_git->isBisecting());
  QCOMPARE(m_git->getCommitDetails("HEAD").hash, m_hashes.at(9));
}

void TestGitBisect::testAutomatedRunFindsTheCommit() {
  m_git->bisectStart(m_hashes.at(9), m_hashes.at(0));

  int exitCode = 0;

  const QString output = m_git->bisectRun(
      QStringLiteral("! grep -q broken marker.txt"), &exitCode);

  GitBisectState state;
  parseBisectProgress(output, &state);
  QCOMPARE(state.status, GitBisectStatus::Found);
  QCOMPARE(state.suspectHash.left(m_hashes.at(6).size()), m_hashes.at(6));
}

QTEST_MAIN(TestGitBisect)
#include "test_gitbisect.moc"
