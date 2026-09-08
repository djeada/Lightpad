#include "git/gitbranchhygiene.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

QString refLine(const QString &refname, const QString &hash,
                const QString &date, const QString &upstream,
                const QString &track, const QString &head) {
  return QStringList{refname, hash, date, upstream, track, head}.join(QChar(0));
}

} // namespace

class TestGitBranchHygiene : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testStateNamesAndExplanations();
  void testParseUpstreamTrack();
  void testParseBranchRefs();
  void testParseBranchRefsSkipsRemoteHead();
  void testSafeToDeleteRules();
  void testDeleteWarnings();

  void testMergedBranchIsACandidate();
  void testUnmergedBranchIsNotACandidate();
  void testCurrentBranchIsNeverACandidate();
  void testWorktreeBranchIsNeverACandidate();
  void testPinnedBranchIsNeverACandidate();
  void testBaseChoiceChangesTheAnswer();
  void testUpstreamGoneIsDetected();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  QString m_remotePath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
  const GitBranchHealth *branchNamed(const GitBranchHygieneReport &report,
                                     const QString &name);
};

void TestGitBranchHygiene::init() {
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

void TestGitBranchHygiene::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitBranchHygiene::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitBranchHygiene::writeFile(const QString &name,
                                     const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

const GitBranchHealth *
TestGitBranchHygiene::branchNamed(const GitBranchHygieneReport &report,
                                  const QString &name) {
  for (const GitBranchHealth &branch : report.branches) {
    if (branch.name == name) {
      return &branch;
    }
  }
  return nullptr;
}

void TestGitBranchHygiene::testStateNamesAndExplanations() {
  QSet<QString> names;
  for (GitBranchState state :
       {GitBranchState::Current, GitBranchState::MergedIntoBase,
        GitBranchState::HasUniqueCommits, GitBranchState::UpstreamGone,
        GitBranchState::LocalOnly, GitBranchState::RemoteOnly,
        GitBranchState::CheckedOutInWorktree, GitBranchState::Pinned}) {
    const QString name = gitBranchStateName(state);
    QVERIFY(!name.isEmpty());
    QVERIFY(!names.contains(name));
    names.insert(name);
    QVERIFY(!gitBranchStateExplanation(state).isEmpty());
  }

  QVERIFY(gitBranchStateExplanation(GitBranchState::RemoteOnly)
              .contains("not a branch you can commit to"));
}

void TestGitBranchHygiene::testParseUpstreamTrack() {
  int ahead = 0;
  int behind = 0;
  bool gone = false;

  parseUpstreamTrack("[ahead 2, behind 1]", &ahead, &behind, &gone);
  QCOMPARE(ahead, 2);
  QCOMPARE(behind, 1);
  QVERIFY(!gone);

  parseUpstreamTrack("[ahead 3]", &ahead, &behind, &gone);
  QCOMPARE(ahead, 3);
  QCOMPARE(behind, 0);

  parseUpstreamTrack("[gone]", &ahead, &behind, &gone);
  QVERIFY(gone);

  parseUpstreamTrack(QString(), &ahead, &behind, &gone);
  QCOMPARE(ahead, 0);
  QCOMPARE(behind, 0);
  QVERIFY(!gone);
}

void TestGitBranchHygiene::testParseBranchRefs() {
  const QString output = refLine("refs/heads/main", "aaa111", "2 hours ago",
                                 "origin/main", "[ahead 1]", "*") +
                         "\n" +
                         refLine("refs/remotes/origin/main", "bbb222",
                                 "3 hours ago", "", "", " ") +
                         "\n";

  const QList<GitBranchHealth> branches = parseBranchRefs(output);
  QCOMPARE(branches.size(), 2);
  QCOMPARE(branches.at(0).name, QString("main"));
  QVERIFY(!branches.at(0).isRemote);
  QVERIFY(branches.at(0).isCurrent);
  QCOMPARE(branches.at(0).upstream, QString("origin/main"));
  QCOMPARE(branches.at(0).ahead, 1);
  QCOMPARE(branches.at(1).name, QString("origin/main"));
  QVERIFY(branches.at(1).isRemote);
}

void TestGitBranchHygiene::testParseBranchRefsSkipsRemoteHead() {
  const QString output =
      refLine("refs/remotes/origin/HEAD", "aaa", "now", "", "", " ") + "\n";
  QVERIFY(parseBranchRefs(output).isEmpty());
}

void TestGitBranchHygiene::testSafeToDeleteRules() {
  GitBranchHealth branch;
  branch.mergedIntoBase = true;
  QVERIFY(branch.safeToDelete());

  branch.isCurrent = true;
  QVERIFY(!branch.safeToDelete());
  branch.isCurrent = false;

  branch.worktreePath = "/tmp/wt";
  QVERIFY(!branch.safeToDelete());
  branch.worktreePath.clear();

  branch.pinned = true;
  QVERIFY(!branch.safeToDelete());
  branch.pinned = false;

  branch.mergedIntoBase = false;
  QVERIFY(!branch.safeToDelete());
}

void TestGitBranchHygiene::testDeleteWarnings() {
  GitBranchHealth unmerged;
  unmerged.uniqueCommits = 3;
  QVERIFY2(unmerged.deleteWarning().contains("only on this branch"),
           qPrintable(unmerged.deleteWarning()));

  GitBranchHealth inWorktree;
  inWorktree.mergedIntoBase = true;
  inWorktree.worktreePath = "/tmp/hotfix";
  QVERIFY2(inWorktree.deleteWarning().contains("/tmp/hotfix"),
           qPrintable(inWorktree.deleteWarning()));

  GitBranchHealth clean;
  clean.mergedIntoBase = true;
  QVERIFY(clean.deleteWarning().isEmpty());
}

void TestGitBranchHygiene::testMergedBranchIsACandidate() {
  QVERIFY(git({"checkout", "-b", "done"}));
  writeFile("g.txt", "g\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "done work"}));
  QVERIFY(git({"checkout", "main"}));
  QVERIFY(git({"merge", "done"}));

  const GitBranchHygieneReport report =
      buildBranchHygieneReport(m_git, "main", {});
  const GitBranchHealth *branch = branchNamed(report, "done");
  QVERIFY(branch);
  QVERIFY(branch->mergedIntoBase);
  QVERIFY(branch->safeToDelete());

  QStringList candidates;
  for (const GitBranchHealth &candidate : report.cleanupCandidates()) {
    candidates << candidate.name;
  }
  QVERIFY2(candidates.contains("done"), qPrintable(candidates.join(",")));
}

void TestGitBranchHygiene::testUnmergedBranchIsNotACandidate() {
  QVERIFY(git({"checkout", "-b", "wip"}));
  writeFile("g.txt", "g\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "unfinished"}));
  QVERIFY(git({"checkout", "main"}));

  const GitBranchHygieneReport report =
      buildBranchHygieneReport(m_git, "main", {});
  const GitBranchHealth *branch = branchNamed(report, "wip");
  QVERIFY(!branch->mergedIntoBase);
  QCOMPARE(branch->uniqueCommits, 1);
  QVERIFY(branch->hasState(GitBranchState::HasUniqueCommits));

  for (const GitBranchHealth &candidate : report.cleanupCandidates()) {
    QVERIFY(candidate.name != "wip");
  }
}

void TestGitBranchHygiene::testCurrentBranchIsNeverACandidate() {
  const GitBranchHygieneReport report =
      buildBranchHygieneReport(m_git, "main", {});
  const GitBranchHealth *branch = branchNamed(report, "main");
  QVERIFY(branch->isCurrent);
  QVERIFY(!branch->safeToDelete());
  for (const GitBranchHealth &candidate : report.cleanupCandidates()) {
    QVERIFY(candidate.name != "main");
  }
}

void TestGitBranchHygiene::testWorktreeBranchIsNeverACandidate() {
  QVERIFY(git({"branch", "hotfix"}));
  QVERIFY(git({"merge", "hotfix"}));
  const QString worktree =
      m_tempDir.path() + "/wt" +
      QString::number(reinterpret_cast<quintptr>(this) % 991);
  QVERIFY(git({"worktree", "add", worktree, "hotfix"}));

  const GitBranchHygieneReport report =
      buildBranchHygieneReport(m_git, "main", {});
  const GitBranchHealth *branch = branchNamed(report, "hotfix");
  QVERIFY(branch);
  QVERIFY(branch->mergedIntoBase);

  QVERIFY(!branch->worktreePath.isEmpty());
  QVERIFY(!branch->safeToDelete());
  QVERIFY(branch->hasState(GitBranchState::CheckedOutInWorktree));
}

void TestGitBranchHygiene::testPinnedBranchIsNeverACandidate() {
  QVERIFY(git({"branch", "keep-me"}));

  const GitBranchHygieneReport report =
      buildBranchHygieneReport(m_git, "main", {QStringLiteral("keep-me")});
  const GitBranchHealth *branch = branchNamed(report, "keep-me");
  QVERIFY(branch->pinned);
  QVERIFY(branch->hasState(GitBranchState::Pinned));
  QVERIFY(!branch->safeToDelete());
}

void TestGitBranchHygiene::testBaseChoiceChangesTheAnswer() {
  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("g.txt", "g\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "feature work"}));
  QVERIFY(git({"checkout", "-b", "release"}));
  QVERIFY(git({"checkout", "main"}));

  QVERIFY(!branchNamed(buildBranchHygieneReport(m_git, "main", {}), "feature")
               ->mergedIntoBase);
  QVERIFY(branchNamed(buildBranchHygieneReport(m_git, "release", {}), "feature")
              ->mergedIntoBase);
}

void TestGitBranchHygiene::testUpstreamGoneIsDetected() {
  QVERIFY(git({"checkout", "-b", "short-lived"}));
  QVERIFY(git({"push", "-u", "origin", "short-lived"}));
  QVERIFY(git({"checkout", "main"}));
  QVERIFY(git({"push", "origin", "--delete", "short-lived"}));
  QVERIFY(git({"fetch", "--prune"}));

  const GitBranchHygieneReport report =
      buildBranchHygieneReport(m_git, "main", {});
  const GitBranchHealth *branch = branchNamed(report, "short-lived");
  QVERIFY(branch);
  QVERIFY(branch->upstreamGone);
  QVERIFY(branch->hasState(GitBranchState::UpstreamGone));
}

QTEST_MAIN(TestGitBranchHygiene)
#include "test_gitbranchhygiene.moc"
