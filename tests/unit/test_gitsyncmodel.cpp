#include "git/gitintegration.h"
#include "git/gitsyncmodel.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitSyncModel : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testParsePullStrategyConfig();
  void testStrategyNames();
  void testMergePreviewMentionsMergeCommit();
  void testRebasePreviewWarnsAboutNewHashes();
  void testFastForwardPreviewRefusesWhenAhead();
  void testPreviewWithoutUpstream();
  void testPushPreviewFastForward();
  void testPushPreviewRejected();
  void testPushPreviewDistinguishesLeaseFromForce();
  void testSummaryVariants();

  void testSyncStateInSync();
  void testSyncStateAhead();
  void testSyncStateBehind();
  void testSyncStateDiverged();
  void testSyncStateWithoutUpstream();
  void testSyncStateDetachedHead();
  void testConfiguredPullStrategy();
  void testSetUpstreamBranch();
  void testPullWithStrategyRebase();
  void testPushWithLeaseSucceedsWhenRemoteUnchanged();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  QString m_remotePath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
  static GitSyncState makeState(int incoming, int outgoing,
                                bool hasUpstream = true);
};

void TestGitSyncModel::init() {
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

void TestGitSyncModel::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitSyncModel::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitSyncModel::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

GitSyncState TestGitSyncModel::makeState(int incoming, int outgoing,
                                         bool hasUpstream) {
  GitSyncState state;
  state.valid = true;
  state.branch = "feature";
  state.upstream = "origin/feature";
  state.hasUpstream = hasUpstream;
  state.mergeBase = "abc1234";
  for (int i = 0; i < incoming; ++i) {
    GitCommitInfo commit;
    commit.subject = QStringLiteral("incoming %1").arg(i);
    state.incoming.append(commit);
  }
  for (int i = 0; i < outgoing; ++i) {
    GitCommitInfo commit;
    commit.subject = QStringLiteral("outgoing %1").arg(i);
    state.outgoing.append(commit);
  }
  return state;
}

void TestGitSyncModel::testParsePullStrategyConfig() {
  QCOMPARE(parsePullStrategyConfig("true", ""), GitPullStrategy::Rebase);
  QCOMPARE(parsePullStrategyConfig("interactive", ""), GitPullStrategy::Rebase);
  QCOMPARE(parsePullStrategyConfig("merges", ""), GitPullStrategy::Rebase);
  QCOMPARE(parsePullStrategyConfig("false", "only"),
           GitPullStrategy::FastForwardOnly);
  QCOMPARE(parsePullStrategyConfig("", ""), GitPullStrategy::Merge);
  QCOMPARE(parsePullStrategyConfig("false", ""), GitPullStrategy::Merge);
}

void TestGitSyncModel::testStrategyNames() {
  QCOMPARE(gitPullStrategyName(GitPullStrategy::Merge), QString("Merge"));
  QCOMPARE(gitPullStrategyName(GitPullStrategy::Rebase), QString("Rebase"));
  QCOMPARE(gitPullStrategyName(GitPullStrategy::FastForwardOnly),
           QString("Fast-forward only"));
}

void TestGitSyncModel::testMergePreviewMentionsMergeCommit() {
  const QString preview =
      gitPullStrategyPreview(GitPullStrategy::Merge, makeState(2, 3));
  QVERIFY2(preview.contains("merge commit"), qPrintable(preview));
  QVERIFY2(preview.contains("3"), qPrintable(preview));

  const QString ff =
      gitPullStrategyPreview(GitPullStrategy::Merge, makeState(2, 0));
  QVERIFY2(ff.contains("No merge commit"), qPrintable(ff));
}

void TestGitSyncModel::testRebasePreviewWarnsAboutNewHashes() {
  const QString preview =
      gitPullStrategyPreview(GitPullStrategy::Rebase, makeState(2, 3));
  QVERIFY2(preview.contains("new hashes"), qPrintable(preview));
}

void TestGitSyncModel::testFastForwardPreviewRefusesWhenAhead() {
  const QString refused =
      gitPullStrategyPreview(GitPullStrategy::FastForwardOnly, makeState(2, 3));
  QVERIFY2(refused.contains("Refuses to run"), qPrintable(refused));

  const QString allowed =
      gitPullStrategyPreview(GitPullStrategy::FastForwardOnly, makeState(2, 0));
  QVERIFY2(allowed.contains("Moves your branch forward"), qPrintable(allowed));
}

void TestGitSyncModel::testPreviewWithoutUpstream() {
  const QString preview =
      gitPullStrategyPreview(GitPullStrategy::Merge, makeState(0, 1, false));
  QVERIFY2(preview.contains("tracks nothing"), qPrintable(preview));
}

void TestGitSyncModel::testPushPreviewFastForward() {
  const QString preview = gitPushPreview(makeState(0, 2), GitPushForce::None);
  QVERIFY2(preview.contains("nothing can be lost"), qPrintable(preview));
}

void TestGitSyncModel::testPushPreviewRejected() {
  const QString preview = gitPushPreview(makeState(1, 2), GitPushForce::None);
  QVERIFY2(preview.contains("Will be rejected"), qPrintable(preview));
  QVERIFY2(preview.contains("not a fast-forward"), qPrintable(preview));
}

void TestGitSyncModel::testPushPreviewDistinguishesLeaseFromForce() {
  const GitSyncState state = makeState(1, 2);
  const QString lease = gitPushPreview(state, GitPushForce::WithLease);
  const QString force = gitPushPreview(state, GitPushForce::Force);

  QVERIFY2(lease.contains("refused instead of discarding"), qPrintable(lease));
  QVERIFY2(force.contains("unconditionally"), qPrintable(force));
  QVERIFY(lease != force);
}

void TestGitSyncModel::testSummaryVariants() {
  QVERIFY(gitSyncSummary(makeState(0, 0)).contains("same commit"));
  QVERIFY(gitSyncSummary(makeState(2, 3)).contains("diverged"));
  QVERIFY(gitSyncSummary(makeState(2, 0)).contains("behind"));
  QVERIFY(gitSyncSummary(makeState(0, 2)).contains("ahead"));
  QVERIFY(gitSyncSummary(makeState(0, 0, false)).contains("no upstream"));

  GitSyncState detached;
  detached.valid = true;
  detached.detachedHead = true;
  QVERIFY(gitSyncSummary(detached).contains("detached"));
}

void TestGitSyncModel::testSyncStateInSync() {
  const GitSyncState state = m_git->syncState();
  QVERIFY(state.valid);
  QCOMPARE(state.branch, QString("main"));
  QCOMPARE(state.upstream, QString("origin/main"));
  QVERIFY(state.hasUpstream);
  QVERIFY(state.inSync());
  QVERIFY(!state.diverged());
  QVERIFY(!state.mergeBase.isEmpty());
}

void TestGitSyncModel::testSyncStateAhead() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "local work"}));

  const GitSyncState state = m_git->syncState();
  QCOMPARE(state.ahead(), 1);
  QCOMPARE(state.behind(), 0);
  QVERIFY(state.pushIsFastForward());
  QVERIFY(!state.pullCanFastForward());
  QCOMPARE(state.outgoing.first().subject, QString("local work"));
}

void TestGitSyncModel::testSyncStateBehind() {

  const QString other = m_tempDir.path() + "/other" +
                        QString::number(reinterpret_cast<quintptr>(this) % 97);
  QVERIFY(git({"clone", m_remotePath, other}, m_tempDir.path()));
  QVERIFY(git({"config", "user.email", "other@example.com"}, other));
  QVERIFY(git({"config", "user.name", "Other"}, other));
  QFile file(other + "/f.txt");
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("remote work\n");
  file.close();
  QVERIFY(git({"commit", "-am", "remote work"}, other));
  QVERIFY(git({"push"}, other));

  QVERIFY(git({"fetch"}));

  const GitSyncState state = m_git->syncState();
  QCOMPARE(state.behind(), 1);
  QCOMPARE(state.ahead(), 0);
  QVERIFY(state.pullCanFastForward());
  QCOMPARE(state.incoming.first().subject, QString("remote work"));
}

void TestGitSyncModel::testSyncStateDiverged() {
  const QString other = m_tempDir.path() + "/div" +
                        QString::number(reinterpret_cast<quintptr>(this) % 89);
  QVERIFY(git({"clone", m_remotePath, other}, m_tempDir.path()));
  QVERIFY(git({"config", "user.email", "other@example.com"}, other));
  QVERIFY(git({"config", "user.name", "Other"}, other));
  QFile file(other + "/f.txt");
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("remote work\n");
  file.close();
  QVERIFY(git({"commit", "-am", "remote work"}, other));
  QVERIFY(git({"push"}, other));

  writeFile("g.txt", "local\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "local work"}));
  QVERIFY(git({"fetch"}));

  const GitSyncState state = m_git->syncState();
  QVERIFY(state.diverged());
  QCOMPARE(state.ahead(), 1);
  QCOMPARE(state.behind(), 1);
  QVERIFY(!state.pushIsFastForward());
  QVERIFY(!state.pullCanFastForward());
}

void TestGitSyncModel::testSyncStateWithoutUpstream() {
  QVERIFY(git({"checkout", "-b", "lonely"}));

  const GitSyncState state = m_git->syncState();
  QVERIFY(state.valid);
  QCOMPARE(state.branch, QString("lonely"));
  QVERIFY(!state.hasUpstream);
  QVERIFY(state.incoming.isEmpty());
  QVERIFY(state.outgoing.isEmpty());
}

void TestGitSyncModel::testSyncStateDetachedHead() {
  QVERIFY(git({"checkout", "--detach", "HEAD"}));

  const GitSyncState state = m_git->syncState();
  QVERIFY(state.valid);
  QVERIFY(state.detachedHead);
  QVERIFY(!state.hasUpstream);
}

void TestGitSyncModel::testConfiguredPullStrategy() {
  QCOMPARE(m_git->configuredPullStrategy(), GitPullStrategy::Merge);

  QVERIFY(git({"config", "pull.rebase", "true"}));
  QCOMPARE(m_git->configuredPullStrategy(), GitPullStrategy::Rebase);

  QVERIFY(git({"config", "pull.rebase", "false"}));
  QVERIFY(git({"config", "pull.ff", "only"}));
  QCOMPARE(m_git->configuredPullStrategy(), GitPullStrategy::FastForwardOnly);
}

void TestGitSyncModel::testSetUpstreamBranch() {
  QVERIFY(git({"checkout", "-b", "lonely"}));
  QVERIFY(!m_git->syncState().hasUpstream);

  QVERIFY(m_git->pushWithForce("origin", "lonely", true, GitPushForce::None));
  QVERIFY(m_git->syncState().hasUpstream);

  QVERIFY(git({"branch", "--unset-upstream"}));
  QVERIFY(!m_git->syncState().hasUpstream);

  QVERIFY(m_git->setUpstreamBranch("origin", "lonely"));
  QCOMPARE(m_git->syncState().upstream, QString("origin/lonely"));
}

void TestGitSyncModel::testPullWithStrategyRebase() {
  const QString other = m_tempDir.path() + "/reb" +
                        QString::number(reinterpret_cast<quintptr>(this) % 83);
  QVERIFY(git({"clone", m_remotePath, other}, m_tempDir.path()));
  QVERIFY(git({"config", "user.email", "other@example.com"}, other));
  QVERIFY(git({"config", "user.name", "Other"}, other));
  QFile file(other + "/remote.txt");
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("remote\n");
  file.close();
  QVERIFY(git({"add", "remote.txt"}, other));
  QVERIFY(git({"commit", "-m", "remote work"}, other));
  QVERIFY(git({"push"}, other));

  writeFile("local.txt", "local\n");
  QVERIFY(git({"add", "local.txt"}));
  QVERIFY(git({"commit", "-m", "local work"}));
  QVERIFY(git({"fetch"}));
  QVERIFY(m_git->syncState().diverged());

  QVERIFY(m_git->pullWithStrategy("origin", "main", GitPullStrategy::Rebase));

  const GitSyncState after = m_git->syncState();
  QVERIFY(!after.diverged());

  QCOMPARE(after.ahead(), 1);
  QCOMPARE(after.behind(), 0);
}

void TestGitSyncModel::testPushWithLeaseSucceedsWhenRemoteUnchanged() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "local work"}));

  QVERIFY(
      m_git->pushWithForce("origin", "main", false, GitPushForce::WithLease));
  QVERIFY(m_git->syncState().inSync());
}

QTEST_MAIN(TestGitSyncModel)
#include "test_gitsyncmodel.moc"
