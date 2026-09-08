#include "git/gitdiffmodel.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitDiffModel : public QObject {
  Q_OBJECT

private slots:
  void init();

  void testParseSimpleDiff();
  void testParseLineNumbers();
  void testParseMultipleHunks();
  void testParseNewFile();
  void testParseDeletedFile();
  void testParseRename();
  void testParseBinary();
  void testParseMultipleFiles();
  void testParseEmpty();
  void testHunkCounts();

  void testBuildHunkPatchSelectsSubset();
  void testBuildHunkPatchEmptySelection();
  void testBuildLinePatchForwardDropsUnselectedAdditions();
  void testBuildLinePatchReverseKeepsUnselectedAdditions();

  void testStageSingleHunkAgainstRealRepo();
  void testStageSingleLineAgainstRealRepo();
  void testUnstageHunkAgainstRealRepo();
  void testStageHunkFromNewFile();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;

  bool git(const QStringList &args);
  QString gitOut(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  QString readFile(const QString &name);
};

void TestGitDiffModel::init() {

  m_repoPath = m_tempDir.path() + "/repo_" +
               QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
               QString::number(reinterpret_cast<quintptr>(this) % 1000);
  static int counter = 0;
  m_repoPath += QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "test@test.com"}));
  QVERIFY(git({"config", "user.name", "Test User"}));
}

bool TestGitDiffModel::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

QString TestGitDiffModel::gitOut(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  process.waitForFinished(GIT_COMMAND_TIMEOUT_MS);
  return QString::fromUtf8(process.readAllStandardOutput());
}

void TestGitDiffModel::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

QString TestGitDiffModel::readFile(const QString &name) {
  QFile file(m_repoPath + "/" + name);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

void TestGitDiffModel::testParseSimpleDiff() {
  const QString diff = "diff --git a/foo.txt b/foo.txt\n"
                       "index 1234567..89abcde 100644\n"
                       "--- a/foo.txt\n"
                       "+++ b/foo.txt\n"
                       "@@ -1,3 +1,3 @@ some section\n"
                       " one\n"
                       "-two\n"
                       "+TWO\n"
                       " three\n";

  const GitDiffFile file = parseUnifiedDiff(diff);
  QVERIFY(file.valid);
  QCOMPARE(file.path, QString("foo.txt"));
  QCOMPARE(file.oldPath, QString("foo.txt"));
  QCOMPARE(file.hunks.size(), 1);

  const GitHunk &hunk = file.hunks.first();
  QCOMPARE(hunk.oldStart, 1);
  QCOMPARE(hunk.oldCount, 3);
  QCOMPARE(hunk.newStart, 1);
  QCOMPARE(hunk.newCount, 3);
  QCOMPARE(hunk.section, QString(" some section"));
  QCOMPARE(hunk.lines.size(), 4);
  QCOMPARE(hunk.lines.at(0).type, GitDiffLineType::Context);
  QCOMPARE(hunk.lines.at(1).type, GitDiffLineType::Removed);
  QCOMPARE(hunk.lines.at(1).text, QString("two"));
  QCOMPARE(hunk.lines.at(2).type, GitDiffLineType::Added);
  QCOMPARE(hunk.lines.at(2).text, QString("TWO"));
  QCOMPARE(file.additions(), 1);
  QCOMPARE(file.deletions(), 1);
}

void TestGitDiffModel::testParseLineNumbers() {
  const QString diff = "diff --git a/f b/f\n"
                       "--- a/f\n"
                       "+++ b/f\n"
                       "@@ -10,4 +10,4 @@\n"
                       " keep\n"
                       "-gone\n"
                       "+added\n"
                       " tail\n";

  const GitHunk &hunk = parseUnifiedDiff(diff).hunks.first();
  QCOMPARE(hunk.lines.at(0).oldLine, 10);
  QCOMPARE(hunk.lines.at(0).newLine, 10);
  QCOMPARE(hunk.lines.at(1).oldLine, 11);
  QCOMPARE(hunk.lines.at(1).newLine, -1);
  QCOMPARE(hunk.lines.at(2).oldLine, -1);
  QCOMPARE(hunk.lines.at(2).newLine, 11);
  QCOMPARE(hunk.lines.at(3).oldLine, 12);
  QCOMPARE(hunk.lines.at(3).newLine, 12);
}

void TestGitDiffModel::testParseMultipleHunks() {
  const QString diff = "diff --git a/f b/f\n"
                       "--- a/f\n"
                       "+++ b/f\n"
                       "@@ -1,3 +1,3 @@\n"
                       " a\n"
                       "-b\n"
                       "+B\n"
                       "@@ -10,3 +10,3 @@\n"
                       " x\n"
                       "-y\n"
                       "+Y\n";

  const GitDiffFile file = parseUnifiedDiff(diff);
  QCOMPARE(file.hunks.size(), 2);
  QCOMPARE(file.hunks.at(1).oldStart, 10);
}

void TestGitDiffModel::testParseNewFile() {
  const QString diff = "diff --git a/new.txt b/new.txt\n"
                       "new file mode 100644\n"
                       "index 0000000..1234567\n"
                       "--- /dev/null\n"
                       "+++ b/new.txt\n"
                       "@@ -0,0 +1,2 @@\n"
                       "+one\n"
                       "+two\n";

  const GitDiffFile file = parseUnifiedDiff(diff);
  QVERIFY(file.isNew);
  QCOMPARE(file.path, QString("new.txt"));
  QCOMPARE(file.hunks.first().oldCount, 0);
  QCOMPARE(file.additions(), 2);
}

void TestGitDiffModel::testParseDeletedFile() {
  const QString diff = "diff --git a/gone.txt b/gone.txt\n"
                       "deleted file mode 100644\n"
                       "--- a/gone.txt\n"
                       "+++ /dev/null\n"
                       "@@ -1,2 +0,0 @@\n"
                       "-one\n"
                       "-two\n";

  const GitDiffFile file = parseUnifiedDiff(diff);
  QVERIFY(file.isDeleted);
  QCOMPARE(file.deletions(), 2);
}

void TestGitDiffModel::testParseRename() {
  const QString diff = "diff --git a/old.txt b/new.txt\n"
                       "similarity index 95%\n"
                       "rename from old.txt\n"
                       "rename to new.txt\n";

  const GitDiffFile file = parseUnifiedDiff(diff);
  QVERIFY(file.isRenamed);
  QCOMPARE(file.oldPath, QString("old.txt"));
  QCOMPARE(file.path, QString("new.txt"));
  QVERIFY(file.isEmpty());
}

void TestGitDiffModel::testParseBinary() {
  const QString diff = "diff --git a/img.png b/img.png\n"
                       "index 1..2 100644\n"
                       "Binary files a/img.png and b/img.png differ\n";

  const GitDiffFile file = parseUnifiedDiff(diff);
  QVERIFY(file.isBinary);
  QVERIFY(file.isEmpty());
}

void TestGitDiffModel::testParseMultipleFiles() {
  const QString diff = "diff --git a/a.txt b/a.txt\n"
                       "--- a/a.txt\n"
                       "+++ b/a.txt\n"
                       "@@ -1 +1 @@\n"
                       "-a\n"
                       "+A\n"
                       "diff --git a/b.txt b/b.txt\n"
                       "--- a/b.txt\n"
                       "+++ b/b.txt\n"
                       "@@ -1 +1 @@\n"
                       "-b\n"
                       "+B\n";

  const QList<GitDiffFile> files = parseUnifiedDiffFiles(diff);
  QCOMPARE(files.size(), 2);
  QCOMPARE(files.at(0).path, QString("a.txt"));
  QCOMPARE(files.at(1).path, QString("b.txt"));

  QCOMPARE(parseUnifiedDiff(diff).path, QString("a.txt"));
}

void TestGitDiffModel::testParseEmpty() {
  QVERIFY(!parseUnifiedDiff(QString()).valid);
  QVERIFY(!parseUnifiedDiff("not a diff at all").valid);
  QVERIFY(parseUnifiedDiffFiles(QString()).isEmpty());
}

void TestGitDiffModel::testHunkCounts() {
  const QString diff = "diff --git a/f b/f\n"
                       "--- a/f\n"
                       "+++ b/f\n"
                       "@@ -1,4 +1,5 @@\n"
                       " a\n"
                       "-b\n"
                       "-c\n"
                       "+B\n"
                       "+C\n"
                       "+D\n"
                       " e\n";

  const GitHunk &hunk = parseUnifiedDiff(diff).hunks.first();
  QCOMPARE(hunk.additions(), 3);
  QCOMPARE(hunk.deletions(), 2);
  QCOMPARE(hunk.changedLineCount(), 5);
}

void TestGitDiffModel::testBuildHunkPatchSelectsSubset() {
  const QString diff = "diff --git a/f b/f\n"
                       "--- a/f\n"
                       "+++ b/f\n"
                       "@@ -1,3 +1,3 @@\n"
                       " a\n"
                       "-b\n"
                       "+B\n"
                       "@@ -10,3 +10,3 @@\n"
                       " x\n"
                       "-y\n"
                       "+Y\n";

  const GitDiffFile file = parseUnifiedDiff(diff);
  const QString patch = buildHunkPatch(file, {1});

  QVERIFY(patch.contains("@@ -10,3 +10,3 @@"));
  QVERIFY(!patch.contains("@@ -1,3"));
  QVERIFY(patch.contains("+Y"));
  QVERIFY(!patch.contains("+B"));
  QVERIFY(patch.startsWith("diff --git a/f b/f"));
  QVERIFY(patch.endsWith("\n"));
}

void TestGitDiffModel::testBuildHunkPatchEmptySelection() {
  const QString diff = "diff --git a/f b/f\n"
                       "--- a/f\n"
                       "+++ b/f\n"
                       "@@ -1 +1 @@\n"
                       "-a\n"
                       "+A\n";
  QVERIFY(buildHunkPatch(parseUnifiedDiff(diff), {}).isEmpty());
}

void TestGitDiffModel::testBuildLinePatchForwardDropsUnselectedAdditions() {
  const QString diff = "diff --git a/f b/f\n"
                       "--- a/f\n"
                       "+++ b/f\n"
                       "@@ -1,2 +1,4 @@\n"
                       " a\n"
                       "-b\n"
                       "+B1\n"
                       "+B2\n"
                       "+B3\n";

  const GitDiffFile file = parseUnifiedDiff(diff);

  const QString patch = buildLinePatch(file, 0, {1, 2}, false);

  QVERIFY(patch.contains("-b"));
  QVERIFY(patch.contains("+B1"));

  QVERIFY(!patch.contains("+B2"));
  QVERIFY(!patch.contains(" B2"));
  QVERIFY(!patch.contains("B3"));
}

void TestGitDiffModel::testBuildLinePatchReverseKeepsUnselectedAdditions() {
  const QString diff = "diff --git a/f b/f\n"
                       "--- a/f\n"
                       "+++ b/f\n"
                       "@@ -1,2 +1,4 @@\n"
                       " a\n"
                       "-b\n"
                       "+B1\n"
                       "+B2\n"
                       "+B3\n";

  const GitDiffFile file = parseUnifiedDiff(diff);

  const QString patch = buildLinePatch(file, 0, {2}, true);

  QVERIFY(patch.contains("+B1"));
  QVERIFY(patch.contains(" B2"));
  QVERIFY(patch.contains(" B3"));
  QVERIFY(!patch.contains("-b"));
}

void TestGitDiffModel::testStageSingleHunkAgainstRealRepo() {
  writeFile("f.txt", "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  writeFile("f.txt",
            "l1\nCHANGED2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
            "l11\nl12\nl13\nl14\nl15\nl16\nl17\nCHANGED18\nl19\nl20\n");

  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  const GitDiffFile file =
      parseUnifiedDiff(integration.getFileDiff("f.txt", false));
  QCOMPARE(file.hunks.size(), 2);

  const QString patch = buildHunkPatch(file, {0});
  QVERIFY(!patch.isEmpty());
  QVERIFY(integration.applyPatch(patch, true, false));

  const QString staged = gitOut({"diff", "--cached"});
  QVERIFY2(staged.contains("CHANGED2"), qPrintable(staged));
  QVERIFY2(!staged.contains("CHANGED18"), qPrintable(staged));

  QVERIFY(readFile("f.txt").contains("CHANGED2"));
  QVERIFY(readFile("f.txt").contains("CHANGED18"));
}

void TestGitDiffModel::testStageSingleLineAgainstRealRepo() {
  writeFile("f.txt", "a\nb\nc\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  writeFile("f.txt", "a\nb\nc\nnew1\nnew2\nnew3\n");

  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  const GitDiffFile file =
      parseUnifiedDiff(integration.getFileDiff("f.txt", false));
  QCOMPARE(file.hunks.size(), 1);

  const GitHunk &hunk = file.hunks.first();
  int firstAddition = -1;
  for (int i = 0; i < hunk.lines.size(); ++i) {
    if (hunk.lines.at(i).type == GitDiffLineType::Added) {
      firstAddition = i;
      break;
    }
  }
  QVERIFY(firstAddition >= 0);

  const QString patch = buildLinePatch(file, 0, {firstAddition}, false);
  QVERIFY(integration.applyPatch(patch, true, false));

  const QString staged = gitOut({"diff", "--cached"});
  QVERIFY2(staged.contains("+new1"), qPrintable(staged));
  QVERIFY2(!staged.contains("+new2"), qPrintable(staged));
  QVERIFY2(!staged.contains("+new3"), qPrintable(staged));
}

void TestGitDiffModel::testUnstageHunkAgainstRealRepo() {
  writeFile("f.txt", "a\nb\nc\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  writeFile("f.txt", "a\nB\nc\n");
  QVERIFY(git({"add", "f.txt"}));

  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  const GitDiffFile staged =
      parseUnifiedDiff(integration.getFileDiff("f.txt", true));
  QCOMPARE(staged.hunks.size(), 1);

  const QString patch = buildHunkPatch(staged, {0});
  QVERIFY(integration.applyPatch(patch, true, true));

  QVERIFY2(gitOut({"diff", "--cached"}).trimmed().isEmpty(),
           qPrintable(gitOut({"diff", "--cached"})));

  QCOMPARE(readFile("f.txt"), QString("a\nB\nc\n"));
}

void TestGitDiffModel::testStageHunkFromNewFile() {
  writeFile("seed.txt", "seed\n");
  QVERIFY(git({"add", "seed.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  writeFile("fresh.txt", "one\ntwo\n");

  QVERIFY(git({"add", "-N", "fresh.txt"}));

  GitIntegration integration;
  QVERIFY(integration.setRepositoryPath(m_repoPath));

  const GitDiffFile file =
      parseUnifiedDiff(integration.getFileDiff("fresh.txt", false));
  QCOMPARE(file.hunks.size(), 1);

  const QString patch = buildLinePatch(file, 0, {0}, false);
  QVERIFY(integration.applyPatch(patch, true, false));

  const QString staged = gitOut({"diff", "--cached"});
  QVERIFY2(staged.contains("+one"), qPrintable(staged));
  QVERIFY2(!staged.contains("+two"), qPrintable(staged));
}

QTEST_MAIN(TestGitDiffModel)
#include "test_gitdiffmodel.moc"
