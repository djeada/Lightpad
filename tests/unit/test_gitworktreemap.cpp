#include "git/gitintegration.h"
#include "git/gitworktreemap.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitWorktreeMap : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testParseWorktreeList();
  void testParseDetachedAndPrunable();
  void testRemovalWarningForMain();
  void testRemovalWarningForDirtyWorktree();
  void testRemovalWarningForCleanWorktree();
  void testSummaryWording();

  void testMapHasTheMainWorktree();
  void testAddedWorktreeAppears();
  void testWorktreeStateIsItsOwn();
  void testCurrentWorktreeIsMarked();
  void testRemovingAWorktree();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
  const GitWorktreeCard *cardNamed(const QList<GitWorktreeCard> &cards,
                                   const QString &branch);
};

void TestGitWorktreeMap::init() {
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

void TestGitWorktreeMap::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitWorktreeMap::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitWorktreeMap::writeFile(const QString &name,
                                   const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

const GitWorktreeCard *
TestGitWorktreeMap::cardNamed(const QList<GitWorktreeCard> &cards,
                              const QString &branch) {
  for (const GitWorktreeCard &card : cards) {
    if (card.branch == branch) {
      return &card;
    }
  }
  return nullptr;
}

void TestGitWorktreeMap::testParseWorktreeList() {
  const QString output = "worktree /home/dev/project\n"
                         "HEAD aaaa1111\n"
                         "branch refs/heads/main\n"
                         "\n"
                         "worktree /home/dev/project-hotfix\n"
                         "HEAD bbbb2222\n"
                         "branch refs/heads/hotfix\n"
                         "\n";

  const QList<GitWorktreeCard> cards = parseWorktreeList(output);
  QCOMPARE(cards.size(), 2);
  QCOMPARE(cards.at(0).path, QString("/home/dev/project"));
  QCOMPARE(cards.at(0).branch, QString("main"));
  QCOMPARE(cards.at(0).name, QString("project"));
  QVERIFY(cards.at(0).isMain);
  QVERIFY(!cards.at(1).isMain);
  QCOMPARE(cards.at(1).branch, QString("hotfix"));
}

void TestGitWorktreeMap::testParseDetachedAndPrunable() {
  const QString output =
      "worktree /home/dev/project\n"
      "HEAD aaaa1111\n"
      "branch refs/heads/main\n"
      "\n"
      "worktree /home/dev/gone\n"
      "HEAD bbbb2222\n"
      "detached\n"
      "prunable gitdir file points to non-existent location\n"
      "\n";

  const QList<GitWorktreeCard> cards = parseWorktreeList(output);
  QCOMPARE(cards.size(), 2);
  QVERIFY(cards.at(1).detached);
  QVERIFY(cards.at(1).branch.isEmpty());
  QVERIFY(cards.at(1).prunable);
  QVERIFY(cards.at(1).prunableReason.contains("non-existent"));
}

void TestGitWorktreeMap::testRemovalWarningForMain() {
  GitWorktreeCard card;
  card.isMain = true;
  QVERIFY2(gitWorktreeRemovalWarning(card).contains("main working directory"),
           qPrintable(gitWorktreeRemovalWarning(card)));
}

void TestGitWorktreeMap::testRemovalWarningForDirtyWorktree() {
  GitWorktreeCard card;
  card.modified = 2;
  card.untracked = 1;
  const QString warning = gitWorktreeRemovalWarning(card);
  QVERIFY2(warning.contains("2 modified"), qPrintable(warning));
  QVERIFY2(warning.contains("1 untracked"), qPrintable(warning));
  QVERIFY2(warning.contains("exist nowhere else"), qPrintable(warning));
}

void TestGitWorktreeMap::testRemovalWarningForCleanWorktree() {
  GitWorktreeCard card;
  QVERIFY(gitWorktreeRemovalWarning(card).isEmpty());
}

void TestGitWorktreeMap::testSummaryWording() {
  GitWorktreeCard onBranch;
  onBranch.branch = "hotfix";
  QVERIFY(gitWorktreeSummary(onBranch).contains("on hotfix"));
  QVERIFY(gitWorktreeSummary(onBranch).contains("clean"));

  GitWorktreeCard detached;
  detached.detached = true;
  detached.headHash = "aaaa11112222";
  QVERIFY2(gitWorktreeSummary(detached).contains("detached at aaaa111"),
           qPrintable(gitWorktreeSummary(detached)));
}

void TestGitWorktreeMap::testMapHasTheMainWorktree() {
  const QList<GitWorktreeCard> cards = buildWorktreeMap(m_git);
  QCOMPARE(cards.size(), 1);
  QVERIFY(cards.first().isMain);
  QCOMPARE(cards.first().branch, QString("main"));
  QVERIFY(cards.first().isCurrent);
}

void TestGitWorktreeMap::testAddedWorktreeAppears() {
  const QString path = m_tempDir.path() + "/hotfix-wt" +
                       QString::number(reinterpret_cast<quintptr>(this) % 977);
  QVERIFY(git({"worktree", "add", "-b", "hotfix", path}));

  const QList<GitWorktreeCard> cards = buildWorktreeMap(m_git);
  QCOMPARE(cards.size(), 2);
  const GitWorktreeCard *hotfix = cardNamed(cards, "hotfix");
  QVERIFY(hotfix);
  QVERIFY(!hotfix->isMain);
  QVERIFY(!hotfix->isCurrent);
  QVERIFY(!hotfix->hasUncommittedWork());
}

void TestGitWorktreeMap::testWorktreeStateIsItsOwn() {
  const QString path = m_tempDir.path() + "/state-wt" +
                       QString::number(reinterpret_cast<quintptr>(this) % 971);
  QVERIFY(git({"worktree", "add", "-b", "sidework", path}));

  QFile file(path + "/f.txt");
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("changed over there\n");
  file.close();

  const QList<GitWorktreeCard> cards = buildWorktreeMap(m_git);
  const GitWorktreeCard *side = cardNamed(cards, "sidework");
  const GitWorktreeCard *main = cardNamed(cards, "main");
  QVERIFY(side && main);

  QVERIFY(side->hasUncommittedWork());
  QCOMPARE(side->modified, 1);
  QVERIFY(!main->hasUncommittedWork());
  QVERIFY(!gitWorktreeRemovalWarning(*side).isEmpty());
}

void TestGitWorktreeMap::testCurrentWorktreeIsMarked() {
  const QString path = m_tempDir.path() + "/other-wt" +
                       QString::number(reinterpret_cast<quintptr>(this) % 967);
  QVERIFY(git({"worktree", "add", "-b", "other", path}));

  const QList<GitWorktreeCard> cards = buildWorktreeMap(m_git);
  int currentCount = 0;
  for (const GitWorktreeCard &card : cards) {
    if (card.isCurrent) {
      ++currentCount;
      QCOMPARE(card.branch, QString("main"));
    }
  }
  QCOMPARE(currentCount, 1);
}

void TestGitWorktreeMap::testRemovingAWorktree() {
  const QString path = m_tempDir.path() + "/temp-wt" +
                       QString::number(reinterpret_cast<quintptr>(this) % 953);
  QVERIFY(git({"worktree", "add", "-b", "temp", path}));
  QCOMPARE(buildWorktreeMap(m_git).size(), 2);

  QVERIFY(m_git->removeWorktree(path));
  QCOMPARE(buildWorktreeMap(m_git).size(), 1);
}

QTEST_MAIN(TestGitWorktreeMap)
#include "test_gitworktreemap.moc"
