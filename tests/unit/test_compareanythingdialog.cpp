#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/dialogs/compareanythingdialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest/QtTest>

class TestCompareAnythingDialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  void testDefaultsToUncommittedChanges();
  void testFileTreeListsChanges();
  void testSwapReversesDirection();
  void testPresetsCoverTheCommonQuestions();
  void testPresetSelectsEndpoints();
  void testSelectingAFileFiltersTheDiff();
  void testBranchComparisonReportsMergeBase();
  void testDivergenceIsSpelledOut();
  void testIndexEndpointExplainsNoMergeBase();
  void testCommitListsShowUniqueCommits();
  void testRecentComparisonsAreRemembered();
  void testFreeTextRevisionIsAccepted();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  QString labelText(CompareAnythingDialog &dialog, const char *name);
};

void TestCompareAnythingDialog::initTestCase() {

  QStandardPaths::setTestModeEnabled(true);
}

void TestCompareAnythingDialog::init() {
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

void TestCompareAnythingDialog::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestCompareAnythingDialog::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestCompareAnythingDialog::writeFile(const QString &name,
                                          const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

QString TestCompareAnythingDialog::labelText(CompareAnythingDialog &dialog,
                                             const char *name) {
  QLabel *label = dialog.findChild<QLabel *>(QString::fromLatin1(name));
  return label ? label->text() : QString();
}

void TestCompareAnythingDialog::testDefaultsToUncommittedChanges() {
  CompareAnythingDialog dialog(m_git, Theme());
  QCOMPARE(dialog.result().base.label, QString("HEAD"));
  QCOMPARE(dialog.result().compare.kind, GitCompareEndpoint::Kind::WorkingTree);
}

void TestCompareAnythingDialog::testFileTreeListsChanges() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");
  writeFile("g.txt", "new\n");
  QVERIFY(git({"add", "g.txt"}));

  CompareAnythingDialog dialog(m_git, Theme());
  QTreeWidget *files = dialog.findChild<QTreeWidget *>("compareFileTree");
  QVERIFY(files);
  QCOMPARE(files->topLevelItemCount(), 2);

  QStringList statuses;
  for (int i = 0; i < files->topLevelItemCount(); ++i) {
    statuses << files->topLevelItem(i)->text(1);
  }
  QVERIFY2(statuses.contains("added"), qPrintable(statuses.join(",")));
  QVERIFY2(statuses.contains("modified"), qPrintable(statuses.join(",")));
}

void TestCompareAnythingDialog::testSwapReversesDirection() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");

  CompareAnythingDialog dialog(m_git, Theme());
  QCOMPARE(dialog.result().additions(), 1);
  QCOMPARE(dialog.result().deletions(), 0);

  dialog.findChild<QPushButton *>("compareSwapButton")->click();

  QCOMPARE(dialog.result().base.kind, GitCompareEndpoint::Kind::WorkingTree);
  QCOMPARE(dialog.result().additions(), 0);
  QCOMPARE(dialog.result().deletions(), 1);
}

void TestCompareAnythingDialog::testPresetsCoverTheCommonQuestions() {
  QVERIFY(git({"checkout", "-b", "feature"}));

  CompareAnythingDialog dialog(m_git, Theme());
  QComboBox *presets = dialog.findChild<QComboBox *>("comparePresetCombo");
  QVERIFY(presets);

  QStringList labels;
  for (int i = 0; i < presets->count(); ++i) {
    labels << presets->itemText(i);
  }
  const QString joined = labels.join("\n");
  QVERIFY2(joined.contains("uncommitted changes"), qPrintable(joined));
  QVERIFY2(joined.contains("Staged vs HEAD"), qPrintable(joined));
  QVERIFY2(joined.contains("Unstaged vs staged"), qPrintable(joined));
  QVERIFY2(joined.contains("last commit"), qPrintable(joined));
  QVERIFY2(joined.contains("Current branch vs main"), qPrintable(joined));
}

void TestCompareAnythingDialog::testPresetSelectsEndpoints() {
  writeFile("f.txt", "one\ntwo\nthree\nstaged\n");
  QVERIFY(git({"add", "f.txt"}));
  writeFile("f.txt", "one\ntwo\nthree\nstaged\nunstaged\n");

  CompareAnythingDialog dialog(m_git, Theme());
  QComboBox *presets = dialog.findChild<QComboBox *>("comparePresetCombo");

  int stagedPreset = -1;
  for (int i = 0; i < presets->count(); ++i) {
    if (presets->itemText(i).contains("Staged vs HEAD")) {
      stagedPreset = i;
      break;
    }
  }
  QVERIFY(stagedPreset > 0);

  presets->setCurrentIndex(stagedPreset);
  emit presets->activated(stagedPreset);

  QCOMPARE(dialog.result().compare.kind, GitCompareEndpoint::Kind::Index);
  QVERIFY(dialog.result().diffText.contains("+staged"));
  QVERIFY(!dialog.result().diffText.contains("+unstaged"));
}

void TestCompareAnythingDialog::testSelectingAFileFiltersTheDiff() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");
  writeFile("g.txt", "brand new\n");
  QVERIFY(git({"add", "g.txt"}));

  CompareAnythingDialog dialog(m_git, Theme());
  QTreeWidget *files = dialog.findChild<QTreeWidget *>("compareFileTree");
  QListWidget *diff = dialog.findChild<QListWidget *>("compareDiffView");
  QVERIFY(files && diff);

  for (int i = 0; i < files->topLevelItemCount(); ++i) {
    if (files->topLevelItem(i)->text(0) == "g.txt") {
      files->setCurrentItem(files->topLevelItem(i));
      break;
    }
  }

  QStringList lines;
  for (int i = 0; i < diff->count(); ++i) {
    lines << diff->item(i)->text();
  }
  const QString joined = lines.join("\n");
  QVERIFY2(joined.contains("+brand new"), qPrintable(joined));
  QVERIFY2(!joined.contains("+four"), qPrintable(joined));
}

void TestCompareAnythingDialog::testBranchComparisonReportsMergeBase() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));

  CompareAnythingDialog dialog(m_git, Theme());
  dialog.setEndpoints(GitCompareEndpoint::branch("main"),
                      GitCompareEndpoint::branch("side"));

  QVERIFY(dialog.result().mergeBaseMeaningful);
  const QString merge = labelText(dialog, "compareMergeBaseLabel");
  QVERIFY2(merge.contains("ahead"), qPrintable(merge));
  QVERIFY2(merge.contains("fast-forward"), qPrintable(merge));
}

void TestCompareAnythingDialog::testDivergenceIsSpelledOut() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));
  writeFile("m.txt", "m\n");
  QVERIFY(git({"add", "m.txt"}));
  QVERIFY(git({"commit", "-m", "main work"}));

  CompareAnythingDialog dialog(m_git, Theme());
  dialog.setEndpoints(GitCompareEndpoint::branch("main"),
                      GitCompareEndpoint::branch("side"));

  const QString merge = labelText(dialog, "compareMergeBaseLabel");
  QVERIFY2(merge.contains("Diverged since"), qPrintable(merge));
}

void TestCompareAnythingDialog::testIndexEndpointExplainsNoMergeBase() {
  CompareAnythingDialog dialog(m_git, Theme());
  dialog.setEndpoints(GitCompareEndpoint::head(), GitCompareEndpoint::index());

  const QString merge = labelText(dialog, "compareMergeBaseLabel");
  QVERIFY2(merge.contains("not commits"), qPrintable(merge));
}

void TestCompareAnythingDialog::testCommitListsShowUniqueCommits() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("s.txt", "s\n");
  QVERIFY(git({"add", "s.txt"}));
  QVERIFY(git({"commit", "-m", "side work"}));
  QVERIFY(git({"checkout", "main"}));

  CompareAnythingDialog dialog(m_git, Theme());
  dialog.setEndpoints(GitCompareEndpoint::branch("main"),
                      GitCompareEndpoint::branch("side"));

  QTreeWidget *onlyBase =
      dialog.findChild<QTreeWidget *>("compareOnlyBaseTree");
  QTreeWidget *onlyCompare =
      dialog.findChild<QTreeWidget *>("compareOnlyCompareTree");
  QVERIFY(onlyBase && onlyCompare);
  QCOMPARE(onlyBase->topLevelItemCount(), 0);
  QCOMPARE(onlyCompare->topLevelItemCount(), 1);
  QCOMPARE(onlyCompare->topLevelItem(0)->text(1), QString("side work"));
}

void TestCompareAnythingDialog::testRecentComparisonsAreRemembered() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");

  {
    CompareAnythingDialog dialog(m_git, Theme());
    dialog.setEndpoints(GitCompareEndpoint::head(),
                        GitCompareEndpoint::workingTree());
  }

  CompareAnythingDialog second(m_git, Theme());
  QComboBox *recents = second.findChild<QComboBox *>("compareRecentCombo");
  QVERIFY(recents);
  QVERIFY2(recents->count() > 1, "the previous comparison should be listed");
  QVERIFY(recents->isEnabled());

  QStringList entries;
  for (int i = 1; i < recents->count(); ++i) {
    entries << recents->itemText(i);
  }
  QVERIFY2(entries.filter("HEAD → Working Tree").size() >= 1,
           qPrintable(entries.join(",")));
}

void TestCompareAnythingDialog::testFreeTextRevisionIsAccepted() {
  writeFile("f.txt", "one\ntwo\nthree\nfour\n");
  QVERIFY(git({"commit", "-am", "extend"}));

  CompareAnythingDialog dialog(m_git, Theme());
  QComboBox *base = dialog.findChild<QComboBox *>("compareBaseCombo");
  QComboBox *compare = dialog.findChild<QComboBox *>("compareCompareCombo");
  QVERIFY(base->isEditable() && compare->isEditable());

  base->setEditText("HEAD~1");
  compare->setEditText("HEAD");
  dialog.findChild<QPushButton *>("compareRunButton")->click();

  QCOMPARE(dialog.result().base.ref, QString("HEAD~1"));
  QVERIFY2(dialog.result().diffText.contains("+four"),
           qPrintable(dialog.result().diffText));
}

QTEST_MAIN(TestCompareAnythingDialog)
#include "test_compareanythingdialog.moc"
