#include "git/gitintegration.h"
#include "git/gitrecovery.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

QString reflogLine(const QString &selector, const QString &action,
                   const QString &hash, const QString &subject,
                   const QString &when) {

  return QStringList{selector, action, hash, subject, when}.join(QChar(0));
}

} // namespace

class TestGitRecovery : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testClassifyReflogAction();
  void testKindNamesAreDistinct();
  void testParseReflogEntries();
  void testParseLinksFromAndToHashes();
  void testGroupingFoldsARebaseRun();
  void testGroupingKeepsUnrelatedEvents();
  void testDescriptionsReadLikeEvents();
  void testGuaranteeDependsOnReachability();

  void testTimelineAfterCommits();
  void testResetIsRecoverable();
  void testUnreachableCommitIsFlagged();
  void testRecoveryBranchMakesItReachable();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitRecovery::init() {
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

void TestGitRecovery::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitRecovery::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitRecovery::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitRecovery::testClassifyReflogAction() {
  QCOMPARE(classifyReflogAction("rebase (pick): foo"),
           GitRecoveryEventKind::RebaseRewrote);
  QCOMPARE(classifyReflogAction("reset: moving to HEAD~1"),
           GitRecoveryEventKind::ResetMoved);
  QCOMPARE(classifyReflogAction("checkout: moving from main to side"),
           GitRecoveryEventKind::HeadSwitched);
  QCOMPARE(classifyReflogAction("commit: add a thing"),
           GitRecoveryEventKind::Commit);
  QCOMPARE(classifyReflogAction("commit (amend): fix it"),
           GitRecoveryEventKind::AmendedCommit);
  QCOMPARE(classifyReflogAction("merge side: Fast-forward"),
           GitRecoveryEventKind::Merged);
  QCOMPARE(classifyReflogAction("something else"), GitRecoveryEventKind::Other);
}

void TestGitRecovery::testKindNamesAreDistinct() {
  QSet<QString> names;
  for (GitRecoveryEventKind kind :
       {GitRecoveryEventKind::Commit, GitRecoveryEventKind::ResetMoved,
        GitRecoveryEventKind::RebaseRewrote, GitRecoveryEventKind::HeadSwitched,
        GitRecoveryEventKind::Merged, GitRecoveryEventKind::CherryPicked,
        GitRecoveryEventKind::Pulled, GitRecoveryEventKind::BranchCreated,
        GitRecoveryEventKind::AmendedCommit, GitRecoveryEventKind::Other}) {
    const QString name = gitRecoveryEventKindName(kind);
    QVERIFY(!name.isEmpty());
    QVERIFY(!names.contains(name));
    names.insert(name);
  }
}

void TestGitRecovery::testParseReflogEntries() {
  const QString output = reflogLine("HEAD@{0}", "commit: second", "aaaa111",
                                    "second", "2 minutes ago") +
                         "\n" +
                         reflogLine("HEAD@{1}", "commit: first", "bbbb222",
                                    "first", "5 minutes ago") +
                         "\n";

  const QList<GitRecoveryEvent> events = parseReflogEntries(output);
  QCOMPARE(events.size(), 2);
  QCOMPARE(events.at(0).selector, QString("HEAD@{0}"));
  QCOMPARE(events.at(0).toHash, QString("aaaa111"));
  QCOMPARE(events.at(0).subject, QString("second"));
  QCOMPARE(events.at(0).relativeDate, QString("2 minutes ago"));
  QCOMPARE(events.at(0).kind, GitRecoveryEventKind::Commit);
}

void TestGitRecovery::testParseLinksFromAndToHashes() {
  const QString output =
      reflogLine("HEAD@{0}", "reset: moving to HEAD~1", "aaaa111", "first",
                 "now") +
      "\n" +
      reflogLine("HEAD@{1}", "commit: second", "bbbb222", "second", "now") +
      "\n";

  const QList<GitRecoveryEvent> events = parseReflogEntries(output);

  QCOMPARE(events.at(0).fromHash, QString("bbbb222"));
  QCOMPARE(events.at(0).toHash, QString("aaaa111"));
  QVERIFY(events.at(1).fromHash.isEmpty());
}

void TestGitRecovery::testGroupingFoldsARebaseRun() {
  QString output;
  for (int i = 0; i < 5; ++i) {
    output += reflogLine(QStringLiteral("HEAD@{%1}").arg(i),
                         QStringLiteral("rebase (pick): commit %1").arg(i),
                         QStringLiteral("hash%1").arg(i),
                         QStringLiteral("commit %1").arg(i), "now") +
              "\n";
  }
  output += reflogLine("HEAD@{5}", "commit: before the rebase", "start000",
                       "before", "now") +
            "\n";

  const QList<GitRecoveryEvent> grouped =
      groupRecoveryEvents(parseReflogEntries(output));

  QCOMPARE(grouped.size(), 2);
  QCOMPARE(grouped.at(0).kind, GitRecoveryEventKind::RebaseRewrote);
  QCOMPARE(grouped.at(0).groupedCount, 5);
  QCOMPARE(grouped.at(0).rawEntries.size(), 5);

  QCOMPARE(grouped.at(0).fromHash, QString("start000"));
  QVERIFY(gitRecoveryEventDescription(grouped.at(0)).contains("5 commits"));
}

void TestGitRecovery::testGroupingKeepsUnrelatedEvents() {
  const QString output =
      reflogLine("HEAD@{0}", "commit: b", "b", "b", "now") + "\n" +
      reflogLine("HEAD@{1}", "commit: a", "a", "a", "now") + "\n";
  QCOMPARE(groupRecoveryEvents(parseReflogEntries(output)).size(), 2);
}

void TestGitRecovery::testDescriptionsReadLikeEvents() {
  GitRecoveryEvent reset;
  reset.kind = GitRecoveryEventKind::ResetMoved;
  reset.fromHash = "bbbbbbbbbb";
  reset.toHash = "aaaaaaaaaa";
  QVERIFY2(gitRecoveryEventDescription(reset).contains("A reset moved the "
                                                       "branch"),
           qPrintable(gitRecoveryEventDescription(reset)));

  GitRecoveryEvent commit;
  commit.kind = GitRecoveryEventKind::Commit;
  commit.toHash = "aaaaaaaaaa";
  commit.subject = "Add the thing";
  QVERIFY(gitRecoveryEventDescription(commit).contains("Add the thing"));
}

void TestGitRecovery::testGuaranteeDependsOnReachability() {
  GitRecoveryEvent reachable;
  reachable.reachable = true;
  QVERIFY2(gitRecoveryGuarantee(reachable).contains("not going anywhere"),
           qPrintable(gitRecoveryGuarantee(reachable)));

  GitRecoveryEvent unreachable;
  unreachable.reachable = false;
  const QString warning = gitRecoveryGuarantee(unreachable);
  QVERIFY2(warning.contains("garbage collection"), qPrintable(warning));
  QVERIFY2(warning.contains("Give it a name"), qPrintable(warning));
}

void TestGitRecovery::testTimelineAfterCommits() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "second"}));

  const QList<GitRecoveryEvent> events = buildRecoveryTimeline(m_git);
  QVERIFY(events.size() >= 2);
  QCOMPARE(events.first().kind, GitRecoveryEventKind::Commit);
  QCOMPARE(events.first().subject, QString("second"));
  QVERIFY(events.first().reachable);
}

void TestGitRecovery::testResetIsRecoverable() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "the commit that will be reset away"}));
  const QString lost = m_git->getCommitDetails("HEAD").hash;

  QVERIFY(git({"reset", "--hard", "HEAD~1"}));

  const QList<GitRecoveryEvent> events = buildRecoveryTimeline(m_git);
  QCOMPARE(events.first().kind, GitRecoveryEventKind::ResetMoved);

  QCOMPARE(events.first().fromHash, lost);
}

void TestGitRecovery::testUnreachableCommitIsFlagged() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "soon to be orphaned"}));
  const QString orphan = m_git->getCommitDetails("HEAD").hash;
  QVERIFY(m_git->isCommitReachable(orphan));

  QVERIFY(git({"reset", "--hard", "HEAD~1"}));
  QVERIFY(!m_git->isCommitReachable(orphan));
}

void TestGitRecovery::testRecoveryBranchMakesItReachable() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "work to recover"}));
  const QString lost = m_git->getCommitDetails("HEAD").hash;
  QVERIFY(git({"reset", "--hard", "HEAD~1"}));
  QVERIFY(!m_git->isCommitReachable(lost));

  const QString before = m_git->getCommitDetails("HEAD").hash;
  QVERIFY(m_git->createBranchFromCommit("recovered/work", lost, false));

  QVERIFY(m_git->isCommitReachable(lost));
  QCOMPARE(m_git->getCommitDetails("HEAD").hash, before);
}

QTEST_MAIN(TestGitRecovery)
#include "test_gitrecovery.moc"
