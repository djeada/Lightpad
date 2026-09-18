#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QtTest>

#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/dialogs/mergestartdialog.h"

class TestMergeStartDialog : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testOffersOtherBranchesOnly();
  void testNamesTheBranchThatChanges();
  void testDestinationChangesPreviewWithoutCheckout();
  void testExplainsWhatWillHappen();
  void testHidesTheCopyQuestionForASingleCopy();
  void testHidesIdenticalCopies();
  void testSelectedRefFollowsTheCopyChoice();
  void testPreferenceRadiosNameBothBranches();
  void testPreferenceMapsOntoMergeOptions();
  void testPreferenceStepHiddenWhenNothingCanConflict();
  void testUpToDateBranchCannotBeStarted();
  void testDirtyWorkingTreeIsExplainedAndBlocks();
  void testConflictingFilesAreListedUpFront();
  void testSelectBranchPreselects();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;
  Theme m_theme;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
  void makeDivergence();
};

bool TestMergeStartDialog::git(const QStringList &args, const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start(QStringLiteral("git"), args);
  return process.waitForFinished(15000) && process.exitCode() == 0;
}

void TestMergeStartDialog::writeFile(const QString &name,
                                     const QString &content) {
  QFile file(QDir(m_repoPath).filePath(name));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(content.toUtf8());
  file.close();
}

void TestMergeStartDialog::init() {
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

void TestMergeStartDialog::cleanup() {
  delete m_git;
  m_git = nullptr;
}

void TestMergeStartDialog::makeDivergence() {
  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("f.txt", "one\nfeature\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("f.txt", "one\nmain\nthree\n");
  QVERIFY(git({"commit", "-am", "main edit"}));
  m_git->refresh();
}

void TestMergeStartDialog::testOffersOtherBranchesOnly() {
  QVERIFY(git({"branch", "feature"}));
  QVERIFY(git({"branch", "spike"}));
  m_git->refresh();

  MergeStartDialog dialog(m_git, m_theme);
  QComboBox *combo =
      dialog.findChild<QComboBox *>(QStringLiteral("mergeSourceCombo"));
  QVERIFY(combo);
  QCOMPARE(combo->count(), 2);

  QStringList offered;
  for (int i = 0; i < combo->count(); ++i) {
    offered << combo->itemData(i).toString();
  }
  QVERIFY(offered.contains(QStringLiteral("feature")));
  QVERIFY(offered.contains(QStringLiteral("spike")));
  QVERIFY(!offered.contains(QStringLiteral("main")));
}

void TestMergeStartDialog::testNamesTheBranchThatChanges() {
  makeDivergence();

  MergeStartDialog dialog(m_git, m_theme);
  QLabel *target =
      dialog.findChild<QLabel *>(QStringLiteral("mergeTargetLabel"));
  QVERIFY(target);
  QCOMPARE(target->text(), QStringLiteral("main"));
}

void TestMergeStartDialog::testExplainsWhatWillHappen() {
  makeDivergence();

  MergeStartDialog dialog(m_git, m_theme);
  QLabel *headline =
      dialog.findChild<QLabel *>(QStringLiteral("mergeHeadlineLabel"));
  QLabel *outcome =
      dialog.findChild<QLabel *>(QStringLiteral("mergeOutcomeLabel"));
  QVERIFY(headline);
  QVERIFY(outcome);
  QVERIFY(headline->text().contains(QStringLiteral("feature")));
  QVERIFY(headline->text().contains(QStringLiteral("main")));
  QVERIFY(!outcome->text().isEmpty());
}

void TestMergeStartDialog::testHidesTheCopyQuestionForASingleCopy() {
  QVERIFY(git({"branch", "feature"}));
  m_git->refresh();

  MergeStartDialog dialog(m_git, m_theme);
  QGroupBox *copyGroup =
      dialog.findChild<QGroupBox *>(QStringLiteral("mergeCopyGroup"));
  QVERIFY(copyGroup);
  QVERIFY(copyGroup->isHidden());
  QCOMPARE(dialog.selectedRef(), QStringLiteral("feature"));
}

void TestMergeStartDialog::testHidesIdenticalCopies() {
  const QString remotePath = m_tempDir.path() + "/remote.git";
  QVERIFY(QDir().mkpath(remotePath));
  QVERIFY(git({"init", "--bare"}, remotePath));

  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("f.txt", "one\nfeature\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"remote", "add", "origin", remotePath}));
  QVERIFY(git({"push", "origin", "feature"}));
  QVERIFY(git({"checkout", "main"}));
  m_git->refresh();

  MergeStartDialog dialog(m_git, m_theme);
  dialog.selectBranch(QStringLiteral("feature"));

  QGroupBox *copyGroup =
      dialog.findChild<QGroupBox *>(QStringLiteral("mergeCopyGroup"));
  QVERIFY(copyGroup);
  QVERIFY(copyGroup->isHidden());

  const QList<QRadioButton *> options =
      dialog.findChildren<QRadioButton *>(QStringLiteral("mergeCopyOption"));
  QCOMPARE(options.size(), 2);

  QVERIFY(options.first()->isChecked());
  QCOMPARE(dialog.selectedRef(), QStringLiteral("feature"));
}

void TestMergeStartDialog::testSelectedRefFollowsTheCopyChoice() {
  const QString remotePath = m_tempDir.path() + "/remote2.git";
  QVERIFY(QDir().mkpath(remotePath));
  QVERIFY(git({"init", "--bare"}, remotePath));

  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("f.txt", "one\nfeature\nthree\n");
  QVERIFY(git({"commit", "-am", "feature edit"}));
  QVERIFY(git({"remote", "add", "origin", remotePath}));
  QVERIFY(git({"push", "origin", "feature"}));
  QVERIFY(git({"checkout", "main"}));
  m_git->refresh();

  MergeStartDialog dialog(m_git, m_theme);
  dialog.selectBranch(QStringLiteral("feature"));

  const QList<QRadioButton *> options =
      dialog.findChildren<QRadioButton *>(QStringLiteral("mergeCopyOption"));
  QCOMPARE(options.size(), 2);

  options.at(1)->setChecked(true);
  QCOMPARE(dialog.selectedRef(), QStringLiteral("origin/feature"));
}

void TestMergeStartDialog::testPreferenceRadiosNameBothBranches() {
  makeDivergence();

  MergeStartDialog dialog(m_git, m_theme);
  QRadioButton *keepOurs = dialog.findChild<QRadioButton *>(
      QStringLiteral("mergePreferenceKeepOurs"));
  QRadioButton *keepTheirs = dialog.findChild<QRadioButton *>(
      QStringLiteral("mergePreferenceKeepTheirs"));
  QVERIFY(keepOurs);
  QVERIFY(keepTheirs);
  QVERIFY(keepOurs->text().contains(QStringLiteral("main")));
  QVERIFY(keepTheirs->text().contains(QStringLiteral("feature")));

  QLabel *explanation =
      dialog.findChild<QLabel *>(QStringLiteral("mergePreferenceExplanation"));
  QVERIFY(explanation);
  QVERIFY(!explanation->text().isEmpty());
}

void TestMergeStartDialog::testPreferenceMapsOntoMergeOptions() {
  makeDivergence();

  MergeStartDialog dialog(m_git, m_theme);
  QCOMPARE(dialog.mergeOptions().preference,
           GitMergeOptions::Preference::Manual);

  dialog.findChild<QRadioButton *>(QStringLiteral("mergePreferenceKeepTheirs"))
      ->setChecked(true);
  QCOMPARE(dialog.mergeOptions().preference,
           GitMergeOptions::Preference::PreferTheirs);

  dialog.findChild<QRadioButton *>(QStringLiteral("mergePreferenceKeepOurs"))
      ->setChecked(true);
  QCOMPARE(dialog.mergeOptions().preference,
           GitMergeOptions::Preference::PreferOurs);
}

void TestMergeStartDialog::testPreferenceStepHiddenWhenNothingCanConflict() {

  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("g.txt", "new\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "add g"}));
  QVERIFY(git({"checkout", "main"}));
  m_git->refresh();

  MergeStartDialog dialog(m_git, m_theme);
  QGroupBox *preferences =
      dialog.findChild<QGroupBox *>(QStringLiteral("mergePreferenceGroup"));
  QVERIFY(preferences);
  QVERIFY(preferences->isHidden());
}

void TestMergeStartDialog::testUpToDateBranchCannotBeStarted() {
  QVERIFY(git({"branch", "feature"}));
  m_git->refresh();

  MergeStartDialog dialog(m_git, m_theme);
  QPushButton *start =
      dialog.findChild<QPushButton *>(QStringLiteral("mergeStartButton"));
  QVERIFY(start);
  QVERIFY(!start->isEnabled());
  QCOMPARE(start->text(), QStringLiteral("Nothing to merge"));
}

void TestMergeStartDialog::testDirtyWorkingTreeIsExplainedAndBlocks() {
  makeDivergence();
  writeFile("f.txt", "one\nuncommitted\nthree\n");

  MergeStartDialog dialog(m_git, m_theme);
  QPushButton *start =
      dialog.findChild<QPushButton *>(QStringLiteral("mergeStartButton"));
  QLabel *blocker =
      dialog.findChild<QLabel *>(QStringLiteral("mergeBlockerLabel"));
  QVERIFY(start);
  QVERIFY(blocker);
  QVERIFY(!start->isEnabled());
  QVERIFY(!blocker->isHidden());
  QVERIFY(blocker->text().contains(QStringLiteral("not committed yet")));
}

void TestMergeStartDialog::testConflictingFilesAreListedUpFront() {
  makeDivergence();

  MergeStartDialog dialog(m_git, m_theme);
  QListWidget *list =
      dialog.findChild<QListWidget *>(QStringLiteral("mergeConflictList"));
  QVERIFY(list);

  if (!dialog.plan().likelyConflicts.isEmpty()) {
    QVERIFY(!list->isHidden());
    QCOMPARE(list->count(), 1);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("f.txt")));
  }
}

void TestMergeStartDialog::testSelectBranchPreselects() {
  QVERIFY(git({"branch", "alpha"}));
  QVERIFY(git({"branch", "beta"}));
  m_git->refresh();

  MergeStartDialog dialog(m_git, m_theme);
  dialog.selectBranch(QStringLiteral("beta"));
  QCOMPARE(dialog.selectedRef(), QStringLiteral("beta"));
}

void TestMergeStartDialog::testDestinationChangesPreviewWithoutCheckout() {
  makeDivergence();
  MergeStartDialog dialog(m_git, m_theme);
  auto *target =
      dialog.findChild<QComboBox *>(QStringLiteral("mergeTargetCombo"));
  QVERIFY(target);
  target->setCurrentIndex(target->findData(QStringLiteral("feature")));
  QCOMPARE(dialog.selectedTarget(), QStringLiteral("feature"));
  QCOMPARE(dialog.selectedRef(), QStringLiteral("main"));
  QCOMPARE(dialog.plan().targetRef, QStringLiteral("feature"));
  QCOMPARE(dialog.plan().sourceRef, QStringLiteral("main"));
  QCOMPARE(m_git->currentBranch(), QStringLiteral("main"));
  QVERIFY(dialog.plan().canStart());
}

QTEST_MAIN(TestMergeStartDialog)
#include "test_mergestartdialog.moc"
