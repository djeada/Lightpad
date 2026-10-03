#include "git/gitintegration.h"
#include "git/gitsubmodulemodel.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitRepoTools : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  void testParseSubmoduleStatus();
  void testParseSubmoduleStatusEmptyAndMalformed();
  void testSubmoduleSummary();

  void testRenameRemote();
  void testSetRemoteUrl();
  void testPushAndDeleteRemoteTag();
  void testPruneRemote();

  void testCherryPickCommits();
  void testSequencerContinueAfterConflict();
  void testSequencerAbortCherryPick();
  void testSequencerSkipCherryPick();
  void testSequencerRevertConflictKind();
  void testSequencerNoneWhenIdle();

  void testCommitFixupRequiresStaged();
  void testFixupAndAutosquash();
  void testAutosquashWithoutFixupsFails();

  void testCleanPreviewAndClean();
  void testCleanRefusesEmptyPaths();

  void testSubmoduleLifecycle();

private:
  QTemporaryDir m_tempDir;

  QString makeRepo(const QString &name);
  bool git(const QString &path, const QStringList &args);
  QString out(const QString &path, const QStringList &args);
  void write(const QString &path, const QString &content);
  QString commitFile(const QString &repo, const QString &name,
                     const QString &content, const QString &message);
};

void TestGitRepoTools::initTestCase() {
  QVERIFY(m_tempDir.isValid());
  qputenv("GIT_CONFIG_COUNT", "1");
  qputenv("GIT_CONFIG_KEY_0", "protocol.file.allow");
  qputenv("GIT_CONFIG_VALUE_0", "always");
}

QString TestGitRepoTools::makeRepo(const QString &name) {
  const QString path = m_tempDir.path() + "/" + name;
  if (!QDir().mkpath(path) || !git(path, {"init", "-q"}) ||
      !git(path, {"config", "user.email", "t@t.com"}) ||
      !git(path, {"config", "user.name", "T"}) ||
      !git(path, {"config", "commit.gpgsign", "false"}) ||
      !git(path, {"config", "protocol.file.allow", "always"})) {
    return QString();
  }
  return path;
}

bool TestGitRepoTools::git(const QString &path, const QStringList &args) {
  QProcess p;
  p.setWorkingDirectory(path);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start("git", args);
  return p.waitForFinished(30000) && p.exitCode() == 0;
}

QString TestGitRepoTools::out(const QString &path, const QStringList &args) {
  QProcess p;
  p.setWorkingDirectory(path);
  p.start("git", args);
  p.waitForFinished(30000);
  return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

void TestGitRepoTools::write(const QString &path, const QString &content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(content.toUtf8());
}

QString TestGitRepoTools::commitFile(const QString &repo, const QString &name,
                                     const QString &content,
                                     const QString &message) {
  write(repo + "/" + name, content);
  if (!git(repo, {"add", name}) || !git(repo, {"commit", "-q", "-m", message}))
    return QString();
  return out(repo, {"rev-parse", "HEAD"});
}

void TestGitRepoTools::testParseSubmoduleStatus() {
  const QString text =
      " 1111111111111111111111111111111111111111 libs/a (v1.0)\n"
      "-2222222222222222222222222222222222222222 libs/b\n"
      "+3333333333333333333333333333333333333333 libs/with space (heads/main)\n"
      "U4444444444444444444444444444444444444444 libs/c\n";
  const auto mods = parseSubmoduleStatus(text);
  QCOMPARE(mods.size(), 4);
  QCOMPARE(mods[0].path, QString("libs/a"));
  QCOMPARE(mods[0].describe, QString("v1.0"));
  QCOMPARE(mods[0].state, GitSubmoduleState::Current);
  QCOMPARE(mods[1].state, GitSubmoduleState::Uninitialized);
  QVERIFY(!mods[1].isInitialized());
  QVERIFY(mods[1].describe.isEmpty());
  QCOMPARE(mods[2].path, QString("libs/with space"));
  QCOMPARE(mods[2].describe, QString("heads/main"));
  QCOMPARE(mods[2].state, GitSubmoduleState::OutOfDate);
  QCOMPARE(mods[3].state, GitSubmoduleState::Conflicted);
  QCOMPARE(mods[3].hash.size(), 40);
}

void TestGitRepoTools::testParseSubmoduleStatusEmptyAndMalformed() {
  QVERIFY(parseSubmoduleStatus(QString()).isEmpty());
  QVERIFY(parseSubmoduleStatus("\n\n  \n").isEmpty());
  QVERIFY(parseSubmoduleStatus("garbage\n").isEmpty());
}

void TestGitRepoTools::testSubmoduleSummary() {
  QCOMPARE(gitSubmoduleSummary({}), QString("No submodules"));
  const auto mods =
      parseSubmoduleStatus("-1111111111111111111111111111111111111111 a\n"
                           "+2222222222222222222222222222222222222222 b (x)\n");
  const QString s = gitSubmoduleSummary(mods);
  QVERIFY(s.contains("2 submodule"));
  QVERIFY(s.contains("1 not initialized"));
  QVERIFY(s.contains("1 out of date"));
}

void TestGitRepoTools::testRenameRemote() {
  const QString repo = makeRepo("rename-remote");
  QVERIFY(!repo.isEmpty());
  QVERIFY(git(repo, {"remote", "add", "origin", "https://example.com/a.git"}));
  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(g.renameRemote("origin", "upstream"));
  QCOMPARE(out(repo, {"remote"}), QString("upstream"));
  QVERIFY(!g.renameRemote("missing", "other"));
  QVERIFY(!g.renameRemote("upstream", ""));
}

void TestGitRepoTools::testSetRemoteUrl() {
  const QString repo = makeRepo("set-url");
  QVERIFY(git(repo, {"remote", "add", "origin", "https://example.com/a.git"}));
  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(g.setRemoteUrl("origin", "https://example.com/b.git"));
  QCOMPARE(out(repo, {"remote", "get-url", "origin"}),
           QString("https://example.com/b.git"));
  QVERIFY(g.setRemoteUrl("origin", "ssh://host/push.git", true));
  QCOMPARE(out(repo, {"remote", "get-url", "--push", "origin"}),
           QString("ssh://host/push.git"));
  QCOMPARE(out(repo, {"remote", "get-url", "origin"}),
           QString("https://example.com/b.git"));
  QVERIFY(!g.setRemoteUrl("nope", "https://x"));
}

void TestGitRepoTools::testPushAndDeleteRemoteTag() {
  const QString bare = m_tempDir.path() + "/tags-bare.git";
  QVERIFY(QDir().mkpath(bare));
  QVERIFY(git(bare, {"init", "-q", "--bare"}));
  const QString repo = makeRepo("tags-work");
  QVERIFY(!commitFile(repo, "a.txt", "a\n", "one").isEmpty());
  QVERIFY(git(repo, {"remote", "add", "origin", bare}));

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(g.createTag("v1", QString(), "release one"));
  QVERIFY(g.pushTag("origin", "v1"));
  QVERIFY(out(bare, {"tag", "-l"}).contains("v1"));
  QVERIFY(g.deleteRemoteTag("origin", "v1"));
  QVERIFY(out(bare, {"tag", "-l"}).isEmpty());
  QVERIFY(!g.pushTag("origin", "no-such-tag"));
}

void TestGitRepoTools::testPruneRemote() {
  const QString bare = m_tempDir.path() + "/prune-bare.git";
  QVERIFY(QDir().mkpath(bare));
  QVERIFY(git(bare, {"init", "-q", "--bare"}));
  const QString repo = makeRepo("prune-work");
  QVERIFY(!commitFile(repo, "a.txt", "a\n", "one").isEmpty());
  QVERIFY(git(repo, {"remote", "add", "origin", bare}));
  QVERIFY(git(repo, {"branch", "feature"}));
  QVERIFY(git(repo, {"push", "-q", "origin", "feature"}));
  QVERIFY(git(repo, {"fetch", "-q", "origin"}));
  QVERIFY(out(repo, {"branch", "-r"}).contains("origin/feature"));
  QVERIFY(git(bare, {"branch", "-D", "feature"}));

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(g.pruneRemote("origin"));
  QVERIFY(!out(repo, {"branch", "-r"}).contains("origin/feature"));
}

void TestGitRepoTools::testCherryPickCommits() {
  const QString repo = makeRepo("cp-multi");
  QVERIFY(!commitFile(repo, "base.txt", "base\n", "base").isEmpty());
  const QString mainBranch = out(repo, {"rev-parse", "--abbrev-ref", "HEAD"});
  QVERIFY(git(repo, {"checkout", "-q", "-b", "topic"}));
  const QString c1 = commitFile(repo, "x.txt", "x\n", "add x");
  const QString c2 = commitFile(repo, "y.txt", "y\n", "add y");
  QVERIFY(git(repo, {"checkout", "-q", mainBranch}));

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(g.cherryPickCommits({c1, c2}));
  QVERIFY(QFileInfo::exists(repo + "/x.txt"));
  QVERIFY(QFileInfo::exists(repo + "/y.txt"));
  QCOMPARE(out(repo, {"log", "--format=%s", "-2"}), QString("add y\nadd x"));
  QVERIFY(!g.cherryPickCommits({}));
}

void TestGitRepoTools::testSequencerContinueAfterConflict() {
  const QString repo = makeRepo("seq-continue");
  QVERIFY(!commitFile(repo, "f.txt", "base\n", "base").isEmpty());
  const QString mainBranch = out(repo, {"rev-parse", "--abbrev-ref", "HEAD"});
  QVERIFY(git(repo, {"checkout", "-q", "-b", "topic"}));
  const QString pick = commitFile(repo, "f.txt", "topic\n", "topic change");
  QVERIFY(git(repo, {"checkout", "-q", mainBranch}));
  QVERIFY(!commitFile(repo, "f.txt", "main\n", "main change").isEmpty());

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::None);
  QVERIFY(!g.cherryPickCommits({pick}));
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::CherryPick);
  QCOMPARE(g.repositoryState().operation, GitOperation::CherryPick);

  QVERIFY(!g.sequencerContinue());
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::CherryPick);

  QVERIFY(g.resolveConflictWith("f.txt", "resolved\n"));
  QVERIFY(g.sequencerContinue());
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::None);
  QCOMPARE(out(repo, {"log", "--format=%s", "-1"}), QString("topic change"));
  QCOMPARE(out(repo, {"show", "HEAD:f.txt"}), QString("resolved"));
}

void TestGitRepoTools::testSequencerAbortCherryPick() {
  const QString repo = makeRepo("seq-abort");
  QVERIFY(!commitFile(repo, "f.txt", "base\n", "base").isEmpty());
  const QString mainBranch = out(repo, {"rev-parse", "--abbrev-ref", "HEAD"});
  QVERIFY(git(repo, {"checkout", "-q", "-b", "topic"}));
  const QString pick = commitFile(repo, "f.txt", "topic\n", "topic change");
  QVERIFY(git(repo, {"checkout", "-q", mainBranch}));
  const QString head = commitFile(repo, "f.txt", "main\n", "main change");

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(!g.cherryPickCommits({pick}));
  QVERIFY(g.sequencerAbort());
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::None);
  QCOMPARE(out(repo, {"rev-parse", "HEAD"}), head);
  QVERIFY(out(repo, {"status", "--porcelain"}).isEmpty());
}

void TestGitRepoTools::testSequencerSkipCherryPick() {
  const QString repo = makeRepo("seq-skip");
  QVERIFY(!commitFile(repo, "f.txt", "base\n", "base").isEmpty());
  const QString mainBranch = out(repo, {"rev-parse", "--abbrev-ref", "HEAD"});
  QVERIFY(git(repo, {"checkout", "-q", "-b", "topic"}));
  const QString c1 = commitFile(repo, "f.txt", "topic\n", "conflicting");
  const QString c2 = commitFile(repo, "g.txt", "g\n", "clean one");
  QVERIFY(git(repo, {"checkout", "-q", mainBranch}));
  QVERIFY(!commitFile(repo, "f.txt", "main\n", "main change").isEmpty());

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(!g.cherryPickCommits({c1, c2}));
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::CherryPick);
  QVERIFY(g.sequencerSkip());
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::None);
  QVERIFY(QFileInfo::exists(repo + "/g.txt"));
  QCOMPARE(out(repo, {"show", "HEAD:f.txt"}), QString("main"));
}

void TestGitRepoTools::testSequencerRevertConflictKind() {
  const QString repo = makeRepo("seq-revert");
  QVERIFY(!commitFile(repo, "f.txt", "one\n", "one").isEmpty());
  const QString two = commitFile(repo, "f.txt", "two\n", "two");
  QVERIFY(!commitFile(repo, "f.txt", "three\n", "three").isEmpty());

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(!g.revertCommit(two));
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::Revert);
  QVERIFY(g.sequencerAbort());
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::None);
  QCOMPARE(out(repo, {"show", "HEAD:f.txt"}), QString("three"));
}

void TestGitRepoTools::testSequencerNoneWhenIdle() {
  const QString repo = makeRepo("seq-idle");
  QVERIFY(!commitFile(repo, "f.txt", "a\n", "a").isEmpty());
  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QCOMPARE(g.sequencerKind(), GitIntegration::SequencerKind::None);
  QVERIFY(!g.sequencerContinue());
  QVERIFY(!g.sequencerAbort());
  QVERIFY(!g.sequencerSkip());
  GitIntegration invalid;
  QCOMPARE(invalid.sequencerKind(), GitIntegration::SequencerKind::None);
}

void TestGitRepoTools::testCommitFixupRequiresStaged() {
  const QString repo = makeRepo("fixup-nostage");
  const QString base = commitFile(repo, "a.txt", "a\n", "base");
  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(!g.commitFixup(base));
  QVERIFY(!g.commitFixup(QString()));
  QCOMPARE(out(repo, {"rev-list", "--count", "HEAD"}), QString("1"));
}

void TestGitRepoTools::testFixupAndAutosquash() {
  const QString repo = makeRepo("fixup-squash");
  const QString root = commitFile(repo, "root.txt", "r\n", "root");
  const QString first = commitFile(repo, "a.txt", "a1\n", "feature a");
  QVERIFY(!commitFile(repo, "b.txt", "b\n", "feature b").isEmpty());

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  write(repo + "/a.txt", "a2\n");
  QVERIFY(g.stageFile("a.txt"));
  QVERIFY(g.commitFixup(first));
  QCOMPARE(out(repo, {"log", "--format=%s", "-1"}),
           QString("fixup! feature a"));
  QCOMPARE(g.pendingFixupSubjects(root).size(), 1);

  write(repo + "/untracked-change.txt", "u\n");
  write(repo + "/root.txt", "dirty\n");

  QVERIFY(g.autosquashRebase(root));
  QCOMPARE(out(repo, {"rev-list", "--count", "HEAD"}), QString("3"));
  QCOMPARE(out(repo, {"log", "--format=%s", "--reverse"}),
           QString("root\nfeature a\nfeature b"));
  QCOMPARE(out(repo, {"show", "HEAD~1:a.txt"}), QString("a2"));
  QCOMPARE(out(repo, {"show", "HEAD:b.txt"}), QString("b"));
  QVERIFY(g.pendingFixupSubjects(root).isEmpty());
  QVERIFY(!g.isRebaseInProgress());
  QVERIFY(out(repo, {"status", "--porcelain"}).contains("root.txt"));
}

void TestGitRepoTools::testAutosquashWithoutFixupsFails() {
  const QString repo = makeRepo("autosquash-none");
  const QString root = commitFile(repo, "root.txt", "r\n", "root");
  QVERIFY(!commitFile(repo, "a.txt", "a\n", "a").isEmpty());
  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(!g.autosquashRebase(root));
  QVERIFY(!g.autosquashRebase(QString()));
  QVERIFY(!g.isRebaseInProgress());
}

void TestGitRepoTools::testCleanPreviewAndClean() {
  const QString repo = makeRepo("clean");
  QVERIFY(!commitFile(repo, "tracked.txt", "t\n", "base").isEmpty());
  write(repo + "/.gitignore", "*.log\n");
  QVERIFY(git(repo, {"add", ".gitignore"}));
  QVERIFY(git(repo, {"commit", "-q", "-m", "ignore"}));
  write(repo + "/junk.txt", "j\n");
  write(repo + "/dir/inner.txt", "i\n");
  write(repo + "/build.log", "l\n");

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QStringList preview = g.cleanPreview();
  preview.sort();
  QCOMPARE(preview, QStringList({"dir/", "junk.txt"}));
  QVERIFY(g.cleanPreview(true).contains("build.log"));

  QVERIFY(g.cleanPaths({"junk.txt"}));
  QVERIFY(!QFileInfo::exists(repo + "/junk.txt"));
  QVERIFY(QFileInfo::exists(repo + "/dir/inner.txt"));
  QVERIFY(QFileInfo::exists(repo + "/build.log"));
  QVERIFY(QFileInfo::exists(repo + "/tracked.txt"));

  QVERIFY(g.cleanPaths({"dir/"}));
  QVERIFY(!QFileInfo::exists(repo + "/dir"));
  QVERIFY(g.cleanPaths({"build.log"}, true));
  QVERIFY(!QFileInfo::exists(repo + "/build.log"));
  QVERIFY(g.cleanPreview(true).isEmpty());
}

void TestGitRepoTools::testCleanRefusesEmptyPaths() {
  const QString repo = makeRepo("clean-empty");
  QVERIFY(!commitFile(repo, "t.txt", "t\n", "base").isEmpty());
  write(repo + "/keep.txt", "k\n");
  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(!g.cleanPaths({}));
  QVERIFY(QFileInfo::exists(repo + "/keep.txt"));
}

void TestGitRepoTools::testSubmoduleLifecycle() {
  const QString sub = makeRepo("sub-origin");
  QVERIFY(!commitFile(sub, "lib.txt", "lib\n", "lib one").isEmpty());
  const QString repo = makeRepo("sub-super");
  QVERIFY(!commitFile(repo, "main.txt", "m\n", "super").isEmpty());

  GitIntegration g;
  QVERIFY(g.setRepositoryPath(repo));
  QVERIFY(g.submodules().isEmpty());
  QVERIFY(g.addSubmodule(sub, "vendor/lib"));
  QVERIFY(git(repo, {"commit", "-q", "-m", "add submodule"}));

  auto mods = g.submodules();
  QCOMPARE(mods.size(), 1);
  QCOMPARE(mods[0].path, QString("vendor/lib"));
  QCOMPARE(mods[0].state, GitSubmoduleState::Current);

  const QString clone = m_tempDir.path() + "/sub-clone";
  QVERIFY(git(m_tempDir.path(), {"clone", "-q", repo, clone}));
  QVERIFY(git(clone, {"config", "protocol.file.allow", "always"}));
  GitIntegration c;
  QVERIFY(c.setRepositoryPath(clone));
  mods = c.submodules();
  QCOMPARE(mods.size(), 1);
  QCOMPARE(mods[0].state, GitSubmoduleState::Uninitialized);
  QVERIFY(c.syncSubmodules());
  QVERIFY(c.initUpdateSubmodules());
  mods = c.submodules();
  QCOMPARE(mods[0].state, GitSubmoduleState::Current);
  QVERIFY(QFileInfo::exists(clone + "/vendor/lib/lib.txt"));

  const QString subWork = clone + "/vendor/lib";
  QVERIFY(git(subWork, {"config", "user.email", "t@t.com"}));
  QVERIFY(git(subWork, {"config", "user.name", "T"}));
  QVERIFY(!commitFile(subWork, "lib2.txt", "2\n", "lib two").isEmpty());
  QCOMPARE(c.submodules()[0].state, GitSubmoduleState::OutOfDate);

  QVERIFY(c.removeSubmodule("vendor/lib"));
  QVERIFY(c.submodules().isEmpty());
  QVERIFY(!c.removeSubmodule(QString()));
  QVERIFY(!c.addSubmodule(QString(), "x"));
}

QTEST_MAIN(TestGitRepoTools)
#include "test_gitrepotools.moc"
