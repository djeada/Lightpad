#include "git/gitfiletimeline.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

QString logRecord(const QStringList &headerFields, const QStringList &body) {
  QString record = QChar(0x01) + headerFields.join(QChar(0));
  for (const QString &line : body) {
    record += QLatin1Char('\n') + line;
  }
  return record + QLatin1Char('\n');
}

QStringList header(const QString &hash, const QString &subject) {
  return {hash,
          hash.left(7),
          QStringLiteral("Ada"),
          QStringLiteral("ada@example.com"),
          QStringLiteral("2024-01-01"),
          QStringLiteral("2 days ago"),
          subject,
          QStringLiteral("parent1")};
}

} // namespace

class TestGitFileTimeline : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testParseSimpleTimeline();
  void testParseRenameFromNameStatus();
  void testParseRenameFromNumstatBraceForm();
  void testParseBinaryCounts();
  void testParseEmpty();
  void testRevisionSummaries();

  void testTimelineNewestFirst();
  void testTimelineFollowsRenames();
  void testTimelineWithoutFollowStopsAtRename();
  void testTimelineAuthorFilter();
  void testTimelinePaging();
  void testTimelineLineRangeFilter();
  void testTimelineUnknownFile();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitFileTimeline::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "ada@example.com"}));
  QVERIFY(git({"config", "user.name", "Ada"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitFileTimeline::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitFileTimeline::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitFileTimeline::writeFile(const QString &name,
                                    const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitFileTimeline::testParseSimpleTimeline() {
  const QString nameStatus =
      logRecord(header("hash1", "Edit the file"), {"M\tsrc/a.cpp"});
  const QString numstat =
      logRecord(header("hash1", "Edit the file"), {"3\t1\tsrc/a.cpp"});

  const QList<GitFileRevision> revisions =
      parseFileTimeline(nameStatus, numstat);
  QCOMPARE(revisions.size(), 1);
  QCOMPARE(revisions.first().commit.hash, QString("hash1"));
  QCOMPARE(revisions.first().commit.subject, QString("Edit the file"));
  QCOMPARE(revisions.first().pathAtRevision, QString("src/a.cpp"));
  QCOMPARE(revisions.first().status, QChar('M'));
  QCOMPARE(revisions.first().additions, 3);
  QCOMPARE(revisions.first().deletions, 1);
}

void TestGitFileTimeline::testParseRenameFromNameStatus() {
  const QString nameStatus =
      logRecord(header("hash1", "Rename"), {"R095\told.cpp\tnew.cpp"});

  const QList<GitFileRevision> revisions =
      parseFileTimeline(nameStatus, QString());
  QCOMPARE(revisions.size(), 1);
  QVERIFY(revisions.first().isRename());
  QCOMPARE(revisions.first().previousPath, QString("old.cpp"));
  QCOMPARE(revisions.first().pathAtRevision, QString("new.cpp"));
}

void TestGitFileTimeline::testParseRenameFromNumstatBraceForm() {
  const QString nameStatus =
      logRecord(header("hash1", "Move"), {"M\tsrc/new.cpp"});

  const QString numstat =
      logRecord(header("hash1", "Move"), {"2\t2\tsrc/{old.cpp => new.cpp}"});

  const QList<GitFileRevision> revisions =
      parseFileTimeline(nameStatus, numstat);
  QCOMPARE(revisions.size(), 1);
  QCOMPARE(revisions.first().previousPath, QString("src/old.cpp"));
  QCOMPARE(revisions.first().additions, 2);
}

void TestGitFileTimeline::testParseBinaryCounts() {
  const QString nameStatus = logRecord(header("hash1", "Bin"), {"M\timg.png"});
  const QString numstat = logRecord(header("hash1", "Bin"), {"-\t-\timg.png"});

  const QList<GitFileRevision> revisions =
      parseFileTimeline(nameStatus, numstat);
  QCOMPARE(revisions.first().additions, 0);
  QCOMPARE(revisions.first().deletions, 0);
}

void TestGitFileTimeline::testParseEmpty() {
  QVERIFY(parseFileTimeline(QString(), QString()).isEmpty());
  QVERIFY(parseFileTimeline("garbage", "garbage").isEmpty());
}

void TestGitFileTimeline::testRevisionSummaries() {
  GitFileRevision working;
  working.isWorkingTree = true;
  QVERIFY(gitFileRevisionSummary(working).contains("Uncommitted"));

  GitFileRevision added;
  added.status = QChar('A');
  added.additions = 20;
  QVERIFY(gitFileRevisionSummary(added).contains("added"));
  QVERIFY(gitFileRevisionSummary(added).contains("+20"));

  GitFileRevision renamed;
  renamed.status = QChar('R');
  renamed.previousPath = "src/old.cpp";
  QVERIFY(gitFileRevisionSummary(renamed).contains("renamed from src/old.cpp"));
}

void TestGitFileTimeline::testTimelineNewestFirst() {
  writeFile("a.txt", "one\n");
  QVERIFY(git({"add", "a.txt"}));
  QVERIFY(git({"commit", "-m", "first"}));
  writeFile("a.txt", "two\n");
  QVERIFY(git({"commit", "-am", "second"}));

  const QList<GitFileRevision> revisions =
      m_git->getFileTimeline("a.txt", GitFileTimelineOptions(), 0, 50);
  QCOMPARE(revisions.size(), 2);
  QCOMPARE(revisions.at(0).commit.subject, QString("second"));
  QCOMPARE(revisions.at(1).commit.subject, QString("first"));
  QCOMPARE(revisions.at(1).status, QChar('A'));
}

void TestGitFileTimeline::testTimelineFollowsRenames() {
  writeFile("old.txt", "one\ntwo\nthree\n");
  QVERIFY(git({"add", "old.txt"}));
  QVERIFY(git({"commit", "-m", "add old"}));
  writeFile("old.txt", "one\nTWO\nthree\n");
  QVERIFY(git({"commit", "-am", "edit"}));
  QVERIFY(git({"mv", "old.txt", "new.txt"}));
  QVERIFY(git({"commit", "-am", "rename"}));

  GitFileTimelineOptions options;
  options.followRenames = true;
  const QList<GitFileRevision> revisions =
      m_git->getFileTimeline("new.txt", options, 0, 50);

  QCOMPARE(revisions.size(), 3);
  QCOMPARE(revisions.at(0).commit.subject, QString("rename"));
  QVERIFY2(revisions.at(0).isRename(),
           qPrintable(QString(revisions.at(0).status)));
  QCOMPARE(revisions.at(0).previousPath, QString("old.txt"));

  QCOMPARE(revisions.at(1).pathAtRevision, QString("old.txt"));
}

void TestGitFileTimeline::testTimelineWithoutFollowStopsAtRename() {
  writeFile("old.txt", "one\n");
  QVERIFY(git({"add", "old.txt"}));
  QVERIFY(git({"commit", "-m", "add old"}));
  QVERIFY(git({"mv", "old.txt", "new.txt"}));
  QVERIFY(git({"commit", "-am", "rename"}));

  GitFileTimelineOptions options;
  options.followRenames = false;
  const QList<GitFileRevision> revisions =
      m_git->getFileTimeline("new.txt", options, 0, 50);

  QCOMPARE(revisions.size(), 1);
}

void TestGitFileTimeline::testTimelineAuthorFilter() {
  writeFile("a.txt", "one\n");
  QVERIFY(git({"add", "a.txt"}));
  QVERIFY(git({"commit", "-m", "by ada"}));

  QVERIFY(git({"config", "user.name", "Grace"}));
  QVERIFY(git({"config", "user.email", "grace@example.com"}));
  writeFile("a.txt", "two\n");
  QVERIFY(git({"commit", "-am", "by grace"}));

  GitFileTimelineOptions options;
  options.author = "Grace";
  const QList<GitFileRevision> revisions =
      m_git->getFileTimeline("a.txt", options, 0, 50);
  QCOMPARE(revisions.size(), 1);
  QCOMPARE(revisions.first().commit.subject, QString("by grace"));
}

void TestGitFileTimeline::testTimelinePaging() {
  writeFile("a.txt", "0\n");
  QVERIFY(git({"add", "a.txt"}));
  QVERIFY(git({"commit", "-m", "commit 0"}));
  for (int i = 1; i < 5; ++i) {
    writeFile("a.txt", QString::number(i) + "\n");
    QVERIFY(git({"commit", "-am", QStringLiteral("commit %1").arg(i)}));
  }

  const QList<GitFileRevision> firstPage =
      m_git->getFileTimeline("a.txt", GitFileTimelineOptions(), 0, 2);
  QCOMPARE(firstPage.size(), 2);
  QCOMPARE(firstPage.first().commit.subject, QString("commit 4"));

  const QList<GitFileRevision> secondPage =
      m_git->getFileTimeline("a.txt", GitFileTimelineOptions(), 2, 2);
  QCOMPARE(secondPage.size(), 2);
  QCOMPARE(secondPage.first().commit.subject, QString("commit 2"));
}

void TestGitFileTimeline::testTimelineLineRangeFilter() {
  writeFile("a.txt", "one\ntwo\nthree\nfour\n");
  QVERIFY(git({"add", "a.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  writeFile("a.txt", "one\nTWO\nthree\nfour\n");
  QVERIFY(git({"commit", "-am", "change line 2"}));

  writeFile("a.txt", "one\nTWO\nthree\nFOUR\n");
  QVERIFY(git({"commit", "-am", "change line 4"}));

  GitFileTimelineOptions options;
  options.followRenames = false;
  options.lineRangeStart = 2;
  options.lineRangeEnd = 2;
  const QList<GitFileRevision> revisions =
      m_git->getFileTimeline("a.txt", options, 0, 50);

  QStringList subjects;
  for (const GitFileRevision &revision : revisions) {
    subjects << revision.commit.subject;
  }
  QVERIFY2(subjects.contains("change line 2"), qPrintable(subjects.join(",")));
  QVERIFY2(!subjects.contains("change line 4"), qPrintable(subjects.join(",")));
}

void TestGitFileTimeline::testTimelineUnknownFile() {
  writeFile("a.txt", "one\n");
  QVERIFY(git({"add", "a.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  QVERIFY(m_git->getFileTimeline("nope.txt", GitFileTimelineOptions(), 0, 50)
              .isEmpty());
  QVERIFY(m_git->getFileTimeline(QString(), GitFileTimelineOptions(), 0, 50)
              .isEmpty());
}

QTEST_MAIN(TestGitFileTimeline)
#include "test_gitfiletimeline.moc"
