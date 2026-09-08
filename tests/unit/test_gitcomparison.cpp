#include "git/gitcomparison.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitComparison : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testDiffArgsCommitToCommit();
  void testDiffArgsIndexToWorking();
  void testDiffArgsWorkingToIndexIsReversed();
  void testDiffArgsHeadToIndex();
  void testDiffArgsHeadToWorking();
  void testDiffArgsCommitToWorking();
  void testDiffArgsCommitToIndex();
  void testDiffArgsIndexToHeadIsReversed();

  void testParseComparisonFiles();
  void testParseComparisonFilesBinary();
  void testParseComparisonFilesRename();

  void testHeadToWorkingShowsAdditions();
  void testWorkingToHeadShowsDeletions();
  void testStagedVsHeadIgnoresUnstaged();
  void testBranchComparisonFindsMergeBase();
  void testBranchComparisonDetectsDivergence();
  void testAncestorComparisonIsNotDiverged();
  void testUnrelatedHistoriesHaveNoMergeBase();
  void testIdenticalEndpointsAreEmpty();
  void testAvailableEndpointsCoverRefs();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitComparison::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));

  writeFile("f.txt", "one\ntwo\nthree\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitComparison::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitComparison::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitComparison::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitComparison::testDiffArgsCommitToCommit() {
  const QStringList args = gitDiffArgsFor(GitCompareEndpoint::branch("main"),
                                          GitCompareEndpoint::branch("side"));
  QCOMPARE(args, QStringList({"diff", "main", "side"}));
}

void TestGitComparison::testDiffArgsIndexToWorking() {

  const QStringList args = gitDiffArgsFor(GitCompareEndpoint::index(),
                                          GitCompareEndpoint::workingTree());
  QCOMPARE(args, QStringList({"diff"}));
}

void TestGitComparison::testDiffArgsWorkingToIndexIsReversed() {
  const QStringList args = gitDiffArgsFor(GitCompareEndpoint::workingTree(),
                                          GitCompareEndpoint::index());
  QCOMPARE(args, QStringList({"diff", "-R"}));
}

void TestGitComparison::testDiffArgsHeadToIndex() {
  const QStringList args =
      gitDiffArgsFor(GitCompareEndpoint::head(), GitCompareEndpoint::index());
  QCOMPARE(args, QStringList({"diff", "--cached"}));
}

void TestGitComparison::testDiffArgsHeadToWorking() {
  const QStringList args = gitDiffArgsFor(GitCompareEndpoint::head(),
                                          GitCompareEndpoint::workingTree());
  QCOMPARE(args, QStringList({"diff", "HEAD"}));
}

void TestGitComparison::testDiffArgsCommitToWorking() {
  const QStringList args = gitDiffArgsFor(GitCompareEndpoint::branch("side"),
                                          GitCompareEndpoint::workingTree());
  QCOMPARE(args, QStringList({"diff", "side"}));
}

void TestGitComparison::testDiffArgsCommitToIndex() {
  const QStringList args = gitDiffArgsFor(GitCompareEndpoint::branch("side"),
                                          GitCompareEndpoint::index());
  QCOMPARE(args, QStringList({"diff", "--cached", "side"}));
}

void TestGitComparison::testDiffArgsIndexToHeadIsReversed() {
  const QStringList args =
      gitDiffArgsFor(GitCompareEndpoint::index(), GitCompareEndpoint::head());
  QCOMPARE(args, QStringList({"diff", "--cached", "-R"}));
}

void TestGitComparison::testParseComparisonFiles() {
  const QString numstat = "3\t1\tsrc/a.cpp\n0\t7\tsrc/b.cpp\n";
  const QString nameStatus = "M\tsrc/a.cpp\nD\tsrc/b.cpp\n";

  const QList<GitComparisonFile> files =
      parseComparisonFiles(numstat, nameStatus);
  QCOMPARE(files.size(), 2);
  QCOMPARE(files.at(0).path, QString("src/a.cpp"));
  QCOMPARE(files.at(0).additions, 3);
  QCOMPARE(files.at(0).deletions, 1);
  QCOMPARE(files.at(0).statusText(), QString("modified"));
  QCOMPARE(files.at(1).statusText(), QString("deleted"));
}

void TestGitComparison::testParseComparisonFilesBinary() {
  const QList<GitComparisonFile> files =
      parseComparisonFiles("-\t-\timg.png\n", "M\timg.png\n");
  QCOMPARE(files.size(), 1);
  QVERIFY(files.first().isBinary);
  QCOMPARE(files.first().additions, 0);
}

void TestGitComparison::testParseComparisonFilesRename() {
  const QList<GitComparisonFile> files = parseComparisonFiles(
      "1\t1\told.txt\tnew.txt\n", "R095\told.txt\tnew.txt\n");
  QCOMPARE(files.size(), 1);
  QCOMPARE(files.first().path, QString("new.txt"));
  QCOMPARE(files.first().oldPath, QString("old.txt"));
  QCOMPARE(files.first().statusText(), QString("renamed"));
}

void TestGitComparison::testHeadToWorkingShowsAdditions() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");

  const GitComparisonResult result = runGitComparison(
      m_git, GitCompareEndpoint::head(), GitCompareEndpoint::workingTree());
  QVERIFY(result.valid);
  QCOMPARE(result.files.size(), 1);
  QCOMPARE(result.additions(), 1);
  QCOMPARE(result.deletions(), 0);
  QVERIFY2(result.diffText.contains("+four"), qPrintable(result.diffText));
}

void TestGitComparison::testWorkingToHeadShowsDeletions() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");

  const GitComparisonResult result = runGitComparison(
      m_git, GitCompareEndpoint::workingTree(), GitCompareEndpoint::head());
  QVERIFY(result.valid);
  QCOMPARE(result.additions(), 0);
  QCOMPARE(result.deletions(), 1);
  QVERIFY2(result.diffText.contains("-four"), qPrintable(result.diffText));
}

void TestGitComparison::testStagedVsHeadIgnoresUnstaged() {
  writeFile("f.txt", "one\ntwo\nthree\nstaged\n");
  QVERIFY(git({"add", "f.txt"}));
  writeFile("f.txt", "one\ntwo\nthree\nstaged\nunstaged\n");

  const GitComparisonResult result = runGitComparison(
      m_git, GitCompareEndpoint::head(), GitCompareEndpoint::index());
  QVERIFY2(result.diffText.contains("+staged"), qPrintable(result.diffText));
  QVERIFY2(!result.diffText.contains("+unstaged"), qPrintable(result.diffText));
}

void TestGitComparison::testBranchComparisonFindsMergeBase() {
  const QString baseCommit = m_git->getCommitDetails("HEAD").hash;

  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));

  const GitComparisonResult result =
      runGitComparison(m_git, GitCompareEndpoint::branch("main"),
                       GitCompareEndpoint::branch("side"));

  QVERIFY(result.mergeBaseMeaningful);
  QCOMPARE(result.mergeBase, baseCommit);
  QCOMPARE(result.onlyInCompare.size(), 1);
  QCOMPARE(result.onlyInCompare.first().subject, QString("side work"));
  QVERIFY(result.onlyInBase.isEmpty());
}

void TestGitComparison::testBranchComparisonDetectsDivergence() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));

  QVERIFY(git({"checkout", "main"}));
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));

  const GitComparisonResult result =
      runGitComparison(m_git, GitCompareEndpoint::branch("main"),
                       GitCompareEndpoint::branch("side"));

  QVERIFY(result.diverged);
  QCOMPARE(result.onlyInBase.size(), 1);
  QCOMPARE(result.onlyInCompare.size(), 1);
}

void TestGitComparison::testAncestorComparisonIsNotDiverged() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");
  QVERIFY(git({"commit", "-am", "extend"}));

  const GitComparisonResult result = runGitComparison(
      m_git, GitCompareEndpoint::commit("HEAD~1"), GitCompareEndpoint::head());

  QVERIFY(!result.diverged);
  QCOMPARE(result.onlyInCompare.size(), 1);
  QVERIFY(result.onlyInBase.isEmpty());
}

void TestGitComparison::testUnrelatedHistoriesHaveNoMergeBase() {
  QVERIFY(git({"checkout", "--orphan", "lonely"}));
  QVERIFY(git({"rm", "-rf", "--cached", "."}));
  writeFile("only.txt", "only\n");
  QVERIFY(git({"add", "only.txt"}));
  QVERIFY(git({"commit", "-m", "orphan root"}));

  const GitComparisonResult result =
      runGitComparison(m_git, GitCompareEndpoint::branch("main"),
                       GitCompareEndpoint::branch("lonely"));

  QVERIFY(result.valid);
  QVERIFY(!result.mergeBaseMeaningful);
}

void TestGitComparison::testIdenticalEndpointsAreEmpty() {
  const GitComparisonResult result = runGitComparison(
      m_git, GitCompareEndpoint::head(), GitCompareEndpoint::head());
  QVERIFY(result.valid);
  QVERIFY(result.isEmpty());
}

void TestGitComparison::testAvailableEndpointsCoverRefs() {
  QVERIFY(git({"branch", "feature"}));
  QVERIFY(git({"tag", "v1"}));
  writeFile("f.txt", "stash me\n");
  QVERIFY(git({"stash", "push", "-m", "wip"}));

  QStringList labels;
  for (const GitCompareEndpoint &endpoint : availableCompareEndpoints(m_git)) {
    labels << endpoint.label;
  }

  QVERIFY2(labels.contains("Working Tree"), qPrintable(labels.join(",")));
  QVERIFY2(labels.contains("Index (staged)"), qPrintable(labels.join(",")));
  QVERIFY2(labels.contains("HEAD"), qPrintable(labels.join(",")));
  QVERIFY2(labels.contains("feature"), qPrintable(labels.join(",")));
  QVERIFY2(labels.contains("v1"), qPrintable(labels.join(",")));
  QVERIFY2(labels.filter("stash@{0}").size() == 1,
           qPrintable(labels.join(",")));
}

QTEST_MAIN(TestGitComparison)
#include "test_gitcomparison.moc"
