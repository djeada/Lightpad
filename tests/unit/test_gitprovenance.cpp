#include "git/gitintegration.h"
#include "git/gitprovenance.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitProvenance : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testChurnSummaryWording();
  void testCaveatAlwaysStatesWhatBlameMeans();
  void testCaveatMentionsUncertainty();

  void testLatestCommitForALine();
  void testStepsListEveryCommitTouchingTheRange();
  void testPreviousTextIsWhatItReplaced();
  void testStableLineReportsOneChange();
  void testFileLinesSlicing();
  void testFileLinesAtRevision();
  void testInvalidInput();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitProvenance::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "ada@example.com"}));
  QVERIFY(git({"config", "user.name", "Ada Lovelace"}));

  writeFile("auth.cpp", "one\ntwo\nthree\nfour\n");
  QVERIFY(git({"add", "auth.cpp"}));
  QVERIFY(git({"commit", "-m", "Introduce auth"}));

  QVERIFY(git({"config", "user.name", "Grace Hopper"}));
  QVERIFY(git({"config", "user.email", "grace@example.com"}));
  writeFile("auth.cpp", "one\nTWO CHANGED\nthree\nfour\n");
  QVERIFY(git({"commit", "-am", "Rework the second line"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitProvenance::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitProvenance::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitProvenance::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitProvenance::testChurnSummaryWording() {
  GitLineProvenance empty;
  empty.valid = true;
  QVERIFY(gitProvenanceChurnSummary(empty).contains("No commit"));

  GitLineProvenance once;
  once.valid = true;
  GitProvenanceStep step;
  step.commit.relativeDate = "3 days ago";
  once.steps.append(step);
  QVERIFY2(gitProvenanceChurnSummary(once).contains("Changed once"),
           qPrintable(gitProvenanceChurnSummary(once)));

  GitLineProvenance many;
  many.valid = true;
  GitProvenanceStep newest;
  newest.commit.relativeDate = "an hour ago";
  GitProvenanceStep oldest;
  oldest.commit.relativeDate = "2 years ago";
  many.steps << newest << oldest;
  const QString summary = gitProvenanceChurnSummary(many);
  QVERIFY2(summary.contains("Changed 2 times"), qPrintable(summary));
  QVERIFY2(summary.contains("2 years ago"), qPrintable(summary));
}

void TestGitProvenance::testCaveatAlwaysStatesWhatBlameMeans() {
  GitLineProvenance provenance;
  const QString caveat = gitProvenanceCaveat(provenance);
  QVERIFY2(caveat.contains("last touched"), qPrintable(caveat));
  QVERIFY2(caveat.contains("not the same as who wrote"), qPrintable(caveat));
}

void TestGitProvenance::testCaveatMentionsUncertainty() {
  GitLineProvenance moved;
  moved.attributionUncertain = true;
  moved.movedFromPath = "old/auth.cpp";
  const QString caveat = gitProvenanceCaveat(moved);
  QVERIFY2(caveat.contains("old/auth.cpp"), qPrintable(caveat));
  QVERIFY2(caveat.contains("heuristic"), qPrintable(caveat));

  GitLineProvenance renamed;
  renamed.followedRename = true;
  QVERIFY(gitProvenanceCaveat(renamed).contains("renamed"));
}

void TestGitProvenance::testLatestCommitForALine() {
  const GitLineProvenance provenance =
      buildLineProvenance(m_git, "auth.cpp", 2, 2);
  QVERIFY(provenance.valid);
  QCOMPARE(provenance.latest.subject, QString("Rework the second line"));
  QCOMPARE(provenance.latest.author, QString("Grace Hopper"));
}

void TestGitProvenance::testStepsListEveryCommitTouchingTheRange() {
  const GitLineProvenance provenance =
      buildLineProvenance(m_git, "auth.cpp", 2, 2);

  QStringList subjects;
  for (const GitProvenanceStep &step : provenance.steps) {
    subjects << step.commit.subject;
  }
  QVERIFY2(subjects.contains("Rework the second line"),
           qPrintable(subjects.join(",")));
  QVERIFY2(subjects.contains("Introduce auth"), qPrintable(subjects.join(",")));
}

void TestGitProvenance::testPreviousTextIsWhatItReplaced() {
  const GitLineProvenance provenance =
      buildLineProvenance(m_git, "auth.cpp", 2, 2);
  QCOMPARE(provenance.currentText, QString("TWO CHANGED"));
  QCOMPARE(provenance.previousText, QString("two"));
}

void TestGitProvenance::testStableLineReportsOneChange() {
  const GitLineProvenance provenance =
      buildLineProvenance(m_git, "auth.cpp", 3, 3);
  QCOMPARE(provenance.latest.subject, QString("Introduce auth"));
  QVERIFY2(gitProvenanceChurnSummary(provenance).contains("Changed once"),
           qPrintable(gitProvenanceChurnSummary(provenance)));
}

void TestGitProvenance::testFileLinesSlicing() {
  QCOMPARE(m_git->fileLines("auth.cpp", 1, 2), QString("one\nTWO CHANGED"));
  QCOMPARE(m_git->fileLines("auth.cpp", 4, 4), QString("four"));

  QCOMPARE(m_git->fileLines("auth.cpp", 3, 99), QString("three\nfour\n"));
}

void TestGitProvenance::testFileLinesAtRevision() {
  QCOMPARE(m_git->fileLinesAtRevision("auth.cpp", "HEAD~1", 2, 2),
           QString("two"));
  QCOMPARE(m_git->fileLinesAtRevision("auth.cpp", "HEAD", 2, 2),
           QString("TWO CHANGED"));
}

void TestGitProvenance::testInvalidInput() {
  QVERIFY(!buildLineProvenance(m_git, QString(), 1, 1).valid);
  QVERIFY(!buildLineProvenance(m_git, "auth.cpp", 0, 1).valid);
  QVERIFY(!buildLineProvenance(m_git, "auth.cpp", 5, 2).valid);

  GitIntegration empty;
  QVERIFY(!buildLineProvenance(&empty, "auth.cpp", 1, 1).valid);
}

QTEST_MAIN(TestGitProvenance)
#include "test_gitprovenance.moc"
