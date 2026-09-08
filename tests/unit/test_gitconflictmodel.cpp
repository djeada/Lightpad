#include "git/gitconflictmodel.h"
#include "git/gitintegration.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitConflictModel : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testClassifyConflictCode();
  void testClassNamesAndExplanationsAreDistinct();
  void testParseUnmergedEntries();
  void testParseUnmergedEntriesIgnoresOtherLines();
  void testOursTheirsHintsDependOnTheOperation();

  void testMergeConflictContext();
  void testStageContentsDifferPerSide();
  void testDeleteModifyIsClassified();
  void testRebaseNamesTheSidesDifferently();
  void testResolvingMarksFileResolved();
  void testCleanRepositoryHasNoConflicts();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  void makeSameLineConflict();
};

void TestGitConflictModel::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "ada@example.com"}));
  QVERIFY(git({"config", "user.name", "Ada"}));
  writeFile("f.txt", "one\ntwo\nthree\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base commit"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitConflictModel::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitConflictModel::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitConflictModel::writeFile(const QString &name,
                                     const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitConflictModel::makeSameLineConflict() {
  QVERIFY(git({"checkout", "-b", "feature/login"}));
  writeFile("f.txt", "one\nFEATURE\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("f.txt", "one\nMAIN\nthree\n");
  QVERIFY(git({"commit", "-am", "main edit"}));

  git({"merge", "feature/login"});
}

void TestGitConflictModel::testClassifyConflictCode() {
  QCOMPARE(classifyConflictCode("UU"), GitConflictClass::SameLineEdit);
  QCOMPARE(classifyConflictCode("AA"), GitConflictClass::AddAdd);
  QCOMPARE(classifyConflictCode("DU"), GitConflictClass::DeleteModify);
  QCOMPARE(classifyConflictCode("UD"), GitConflictClass::ModifyDelete);
  QCOMPARE(classifyConflictCode("DD"), GitConflictClass::BothDeleted);
  QCOMPARE(classifyConflictCode("AU"), GitConflictClass::AddedByOneSide);
  QCOMPARE(classifyConflictCode("??"), GitConflictClass::Unknown);
}

void TestGitConflictModel::testClassNamesAndExplanationsAreDistinct() {
  const QList<GitConflictClass> classes{
      GitConflictClass::SameLineEdit, GitConflictClass::AddAdd,
      GitConflictClass::DeleteModify, GitConflictClass::ModifyDelete,
      GitConflictClass::BothDeleted,  GitConflictClass::AddedByOneSide,
      GitConflictClass::Binary,       GitConflictClass::Unknown};

  QSet<QString> names;
  QSet<QString> explanations;
  for (GitConflictClass value : classes) {
    const QString name = gitConflictClassName(value);
    const QString explanation = gitConflictClassExplanation(value);
    QVERIFY(!name.isEmpty());
    QVERIFY(!explanation.isEmpty());
    QVERIFY(!names.contains(name));
    QVERIFY(!explanations.contains(explanation));
    names.insert(name);
    explanations.insert(explanation);
  }

  QVERIFY(gitConflictClassExplanation(GitConflictClass::DeleteModify) !=
          gitConflictClassExplanation(GitConflictClass::ModifyDelete));
}

void TestGitConflictModel::testParseUnmergedEntries() {
  const QString output =
      "1 M. N... 100644 100644 100644 aaa bbb clean.txt\n"
      "u UU N... 100644 100644 100644 100644 aaa bbb ccc src/auth.cpp\n"
      "u DU N... 100644 100644 100644 100644 aaa bbb ccc gone.txt\n";

  const QList<QPair<QString, QString>> entries = parseUnmergedEntries(output);
  QCOMPARE(entries.size(), 2);
  QCOMPARE(entries.at(0).first, QString("src/auth.cpp"));
  QCOMPARE(entries.at(0).second, QString("UU"));
  QCOMPARE(entries.at(1).first, QString("gone.txt"));
  QCOMPARE(entries.at(1).second, QString("DU"));
}

void TestGitConflictModel::testParseUnmergedEntriesIgnoresOtherLines() {
  QVERIFY(parseUnmergedEntries("# branch.head main\n? untracked\n").isEmpty());
  QVERIFY(parseUnmergedEntries(QString()).isEmpty());
}

void TestGitConflictModel::testOursTheirsHintsDependOnTheOperation() {
  const QString mergeOurs = gitConflictOursHint(GitOperation::Merge);
  const QString rebaseOurs = gitConflictOursHint(GitOperation::Rebase);
  QVERIFY(mergeOurs != rebaseOurs);

  QVERIFY2(rebaseOurs.contains("replaying onto"), qPrintable(rebaseOurs));
  QVERIFY2(gitConflictTheirsHint(GitOperation::Rebase).contains("backwards"),
           qPrintable(gitConflictTheirsHint(GitOperation::Rebase)));
}

void TestGitConflictModel::testMergeConflictContext() {
  makeSameLineConflict();

  const GitConflictContext context = buildGitConflictContext(m_git);
  QVERIFY(context.valid);
  QCOMPARE(context.operation, GitOperation::Merge);
  QCOMPARE(context.files.size(), 1);
  QCOMPARE(context.files.first().path, QString("f.txt"));
  QCOMPARE(context.files.first().conflictClass, GitConflictClass::SameLineEdit);

  QVERIFY2(context.oursLabel.contains("main"), qPrintable(context.oursLabel));
  QVERIFY2(context.theirsLabel.contains("feature/login"),
           qPrintable(context.theirsLabel));
  QVERIFY(!context.mergeBase.isEmpty());
  QCOMPARE(context.mergeBaseSubject, QString("base commit"));
  QCOMPARE(context.remainingCount(), 1);
}

void TestGitConflictModel::testStageContentsDifferPerSide() {
  makeSameLineConflict();

  const GitConflictContext context = buildGitConflictContext(m_git);
  const GitConflictFile &file = context.files.first();

  QVERIFY(file.base.exists);
  QVERIFY(file.ours.exists);
  QVERIFY(file.theirs.exists);
  QVERIFY2(file.base.content.contains("two"), qPrintable(file.base.content));
  QVERIFY2(file.ours.content.contains("MAIN"), qPrintable(file.ours.content));
  QVERIFY2(file.theirs.content.contains("FEATURE"),
           qPrintable(file.theirs.content));

  QVERIFY2(file.merged.contains("<<<<<<<"), qPrintable(file.merged));
}

void TestGitConflictModel::testDeleteModifyIsClassified() {
  QVERIFY(git({"checkout", "-b", "feature/login"}));
  QVERIFY(git({"rm", "f.txt"}));
  QVERIFY(git({"commit", "-m", "remove the file"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("f.txt", "one\nCHANGED\nthree\n");
  QVERIFY(git({"commit", "-am", "keep changing it"}));
  git({"merge", "feature/login"});

  const GitConflictContext context = buildGitConflictContext(m_git);
  QCOMPARE(context.files.size(), 1);
  QCOMPARE(context.files.first().conflictClass, GitConflictClass::ModifyDelete);

  QVERIFY(context.files.first().ours.exists);
  QVERIFY(!context.files.first().theirs.exists);
}

void TestGitConflictModel::testRebaseNamesTheSidesDifferently() {
  QVERIFY(git({"checkout", "-b", "feature/login"}));
  writeFile("f.txt", "one\nFEATURE\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("f.txt", "one\nMAIN\nthree\n");
  QVERIFY(git({"commit", "-am", "main edit"}));
  QVERIFY(git({"checkout", "feature/login"}));
  git({"rebase", "main"});

  const GitConflictContext context = buildGitConflictContext(m_git);
  QCOMPARE(context.operation, GitOperation::Rebase);
  QVERIFY2(context.oursHint.contains("replaying onto"),
           qPrintable(context.oursHint));
  QVERIFY2(context.theirsLabel.contains("being replayed"),
           qPrintable(context.theirsLabel));
}

void TestGitConflictModel::testResolvingMarksFileResolved() {
  makeSameLineConflict();

  QVERIFY(m_git->resolveConflictWith("f.txt", "one\nRESOLVED\nthree\n"));

  const GitConflictContext context = buildGitConflictContext(m_git);
  QVERIFY(context.files.isEmpty());

  QFile file(m_repoPath + "/f.txt");
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(file.readAll()),
           QString("one\nRESOLVED\nthree\n"));
}

void TestGitConflictModel::testCleanRepositoryHasNoConflicts() {
  const GitConflictContext context = buildGitConflictContext(m_git);
  QVERIFY(context.valid);
  QVERIFY(context.files.isEmpty());
  QCOMPARE(context.operation, GitOperation::None);
}

QTEST_MAIN(TestGitConflictModel)
#include "test_gitconflictmodel.moc"
