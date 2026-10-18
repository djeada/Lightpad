#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/dialogs/gitlogdialog.h"
#include "ui/widgets/gitgraphwidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTreeWidget>
#include <QtTest/QtTest>

class TestGitLogDialog : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testOpensOnWorkingTreeWhenDirty();
  void testOpensOnHeadWhenClean();
  void testDetailActionsFollowSelection();
  void testAllBranchesToggleChangesScope();
  void testFirstParentToggleChangesScope();
  void testFilterReportsMatchCount();
  void testCompareRequestIsForwarded();
  void testCommitDetailsListChangedFiles();
  void testMergeCommitShowsBothParents();
  void testListAndGraphShareScopeAndFilters();
  void testFilterSurvivesRefresh();
  void testInspectObjectSignal();
  void testPagedHistoryStaysInSync();
  void testSelectUnreachableCommit();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
};

void TestGitLogDialog::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Ada Lovelace"}));

  writeFile("a.txt", "a1\n");
  QVERIFY(git({"add", "a.txt"}));
  QVERIFY(git({"commit", "-m", "Add a"}));
  writeFile("b.txt", "b1\n");
  QVERIFY(git({"add", "b.txt"}));
  QVERIFY(git({"commit", "-m", "Add b"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestGitLogDialog::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitLogDialog::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitLogDialog::writeFile(const QString &name, const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

void TestGitLogDialog::testOpensOnWorkingTreeWhenDirty() {
  writeFile("a.txt", "dirty\n");

  GitLogDialog dialog(m_git, Theme());
  QTextEdit *details = dialog.findChild<QTextEdit *>("graphDetailView");
  QVERIFY(details);
  QVERIFY2(details->toPlainText().contains("Uncommitted changes"),
           qPrintable(details->toPlainText()));
}

void TestGitLogDialog::testOpensOnHeadWhenClean() {
  GitLogDialog dialog(m_git, Theme());
  QTextEdit *details = dialog.findChild<QTextEdit *>("graphDetailView");
  QVERIFY2(details->toPlainText().contains("Add b"),
           qPrintable(details->toPlainText()));
}

void TestGitLogDialog::testDetailActionsFollowSelection() {
  writeFile("a.txt", "dirty\n");

  GitLogDialog dialog(m_git, Theme());
  QPushButton *viewDiff =
      dialog.findChild<QPushButton *>("graphViewDiffButton");
  QPushButton *reset = dialog.findChild<QPushButton *>("graphResetButton");
  QVERIFY(viewDiff && reset);

  QVERIFY(!viewDiff->isEnabled());
  QVERIFY(!reset->isEnabled());

  GitGraphWidget *graph = dialog.findChild<GitGraphWidget *>();
  QVERIFY(graph);
  graph->selectCommit(m_git->getCommitDetails("HEAD").hash, true);

  QVERIFY(viewDiff->isEnabled());
  QVERIFY(reset->isEnabled());
}

void TestGitLogDialog::testAllBranchesToggleChangesScope() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("c.txt", "c1\n");
  QVERIFY(git({"add", "c.txt"}));
  QVERIFY(git({"commit", "-m", "Add c"}));
  QVERIFY(git({"checkout", "main"}));

  GitLogDialog dialog(m_git, Theme());
  GitGraphWidget *graph = dialog.findChild<GitGraphWidget *>();
  QCheckBox *allBranches =
      dialog.findChild<QCheckBox *>("graphAllBranchesCheck");
  QVERIFY(graph && allBranches);

  QVERIFY(allBranches->isChecked());
  QCOMPARE(graph->loadedCommitCount(), 3);

  allBranches->setChecked(false);
  QCOMPARE(graph->loadedCommitCount(), 2);
}

void TestGitLogDialog::testFirstParentToggleChangesScope() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("c.txt", "c1\n");
  QVERIFY(git({"add", "c.txt"}));
  QVERIFY(git({"commit", "-m", "Add c"}));
  QVERIFY(git({"checkout", "main"}));
  QVERIFY(git({"merge", "--no-ff", "side", "-m", "Merge side"}));

  GitLogDialog dialog(m_git, Theme());
  GitGraphWidget *graph = dialog.findChild<GitGraphWidget *>();
  QCheckBox *allBranches =
      dialog.findChild<QCheckBox *>("graphAllBranchesCheck");
  QCheckBox *firstParent =
      dialog.findChild<QCheckBox *>("graphFirstParentCheck");
  QVERIFY(graph && firstParent && allBranches);

  allBranches->setChecked(false);
  const int full = graph->loadedCommitCount();
  firstParent->setChecked(true);
  QVERIFY2(graph->loadedCommitCount() < full,
           qPrintable(QString("full=%1 flat=%2")
                          .arg(full)
                          .arg(graph->loadedCommitCount())));
}

void TestGitLogDialog::testFilterReportsMatchCount() {
  GitLogDialog dialog(m_git, Theme());
  QLineEdit *search = dialog.findChild<QLineEdit *>("graphSearchField");
  QVERIFY(search);

  search->setText("Add a");

  bool sawMatchCount = false;
  for (QLabel *label : dialog.findChildren<QLabel *>()) {
    if (label->text().contains("1 of 2")) {
      sawMatchCount = true;
    }
  }
  QVERIFY(sawMatchCount);
}

void TestGitLogDialog::testCompareRequestIsForwarded() {
  GitLogDialog dialog(m_git, Theme());
  QSignalSpy spy(&dialog, &GitLogDialog::compareRequested);

  GitGraphWidget *graph = dialog.findChild<GitGraphWidget *>();
  graph->selectCommit(m_git->getCommitDetails("HEAD").hash, true);

  QPushButton *compare =
      dialog.findChild<QPushButton *>("graphCompareHeadButton");
  QVERIFY(compare->isEnabled());
  compare->click();

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst().at(1).toString(), QString("HEAD"));
}

void TestGitLogDialog::testCommitDetailsListChangedFiles() {
  GitLogDialog dialog(m_git, Theme());
  GitGraphWidget *graph = dialog.findChild<GitGraphWidget *>();
  graph->selectCommit(m_git->getCommitDetails("HEAD").hash, true);

  QTreeWidget *files = dialog.findChild<QTreeWidget *>("graphDetailFiles");
  QVERIFY(files);
  QCOMPARE(files->topLevelItemCount(), 1);
  QCOMPARE(files->topLevelItem(0)->text(0), QString("b.txt"));
  QVERIFY(files->topLevelItem(0)->text(1).contains("+1"));
}

void TestGitLogDialog::testMergeCommitShowsBothParents() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("c.txt", "c1\n");
  QVERIFY(git({"add", "c.txt"}));
  QVERIFY(git({"commit", "-m", "Add c"}));
  QVERIFY(git({"checkout", "main"}));
  QVERIFY(git({"merge", "--no-ff", "side", "-m", "Merge side"}));

  GitLogDialog dialog(m_git, Theme());
  GitGraphWidget *graph = dialog.findChild<GitGraphWidget *>();
  graph->selectCommit(m_git->getCommitDetails("HEAD").hash, true);

  QTextEdit *details = dialog.findChild<QTextEdit *>("graphDetailView");
  const QString text = details->toPlainText();
  QVERIFY2(text.contains("Parents"), qPrintable(text));
  QVERIFY2(text.contains("a merge commit"), qPrintable(text));
}

void TestGitLogDialog::testListAndGraphShareScopeAndFilters() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("side.txt", "side\n");
  QVERIFY(git({"add", "."}));
  QVERIFY(git({"commit", "-m", "Side branch work"}));
  const QString sideHash = m_git->getCommitDetails("HEAD").hash;
  QVERIFY(git({"checkout", "main"}));
  GitLogDialog dialog(m_git, Theme());
  auto *graph = dialog.findChild<GitGraphWidget *>();
  auto *list = dialog.findChild<QTreeWidget *>("graphCommitList");
  if (!list) {
    for (auto *tree : dialog.findChildren<QTreeWidget *>())
      if (tree->columnCount() == 4)
        list = tree;
  }
  QVERIFY(list);
  QCOMPARE(list->topLevelItemCount(), graph->loadedCommitCount());
  auto *search = dialog.findChild<QLineEdit *>("graphSearchField");
  search->setText(sideHash);
  int visible = 0;
  for (int i = 0; i < list->topLevelItemCount(); ++i)
    visible += !list->topLevelItem(i)->isHidden();
  QCOMPARE(visible, 1);
  QCOMPARE(graph->matchingCommitCount(), 1);
  search->clear();
  dialog.findChild<QCheckBox *>("graphAllBranchesCheck")->setChecked(false);
  QCOMPARE(list->topLevelItemCount(), 2);
  QCOMPARE(list->topLevelItemCount(), graph->loadedCommitCount());
}

void TestGitLogDialog::testFilterSurvivesRefresh() {
  GitLogDialog dialog(m_git, Theme());
  auto *search = dialog.findChild<QLineEdit *>("graphSearchField");
  search->setText("Add a");
  dialog.refresh();
  QCOMPARE(dialog.findChild<GitGraphWidget *>()->matchingCommitCount(), 1);
  for (auto *tree : dialog.findChildren<QTreeWidget *>()) {
    if (tree->columnCount() == 4) {
      int visible = 0;
      for (int i = 0; i < tree->topLevelItemCount(); ++i)
        visible += !tree->topLevelItem(i)->isHidden();
      QCOMPARE(visible, 1);
    }
  }
}

void TestGitLogDialog::testInspectObjectSignal() {
  GitLogDialog dialog(m_git, Theme());
  const QString hash = m_git->getCommitDetails("HEAD").hash;
  dialog.selectCommit(hash);
  QSignalSpy spy(&dialog, &GitLogDialog::inspectObjectRequested);
  dialog.findChild<QPushButton *>("graphObjectsButton")->click();
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.first().first().toString(), hash);
}

void TestGitLogDialog::testPagedHistoryStaysInSync() {
  QByteArray input;
  for (int i = 0; i < 205; ++i) {
    input += "commit refs/heads/main\ncommitter Ada <dev@example.com> " +
             QByteArray::number(1700000000 + i) + " +0000\n";
    const QByteArray message = "History " + QByteArray::number(i);
    input +=
        "data " + QByteArray::number(message.size()) + '\n' + message + '\n';
    if (i == 0)
      input += "from refs/heads/main^0\n";
    input += '\n';
  }
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", {"fast-import", "--quiet"});
  QVERIFY(process.waitForStarted());
  process.write(input);
  process.closeWriteChannel();
  QVERIFY(process.waitForFinished());
  QCOMPARE(process.exitCode(), 0);
  QVERIFY(git({"reset", "--hard", "HEAD"}));
  GitLogDialog dialog(m_git, Theme());
  auto *graph = dialog.findChild<GitGraphWidget *>();
  QCOMPARE(graph->loadedCommitCount(), 200);
  auto *more = dialog.findChild<QPushButton *>("graphLoadMoreButton");
  QVERIFY(more->isEnabled());
  more->click();
  QTRY_COMPARE(graph->loadedCommitCount(), 207);
  for (auto *tree : dialog.findChildren<QTreeWidget *>())
    if (tree->columnCount() == 4)
      QCOMPARE(tree->topLevelItemCount(), 207);
  QVERIFY(!more->isEnabled());
}

void TestGitLogDialog::testSelectUnreachableCommit() {
  QVERIFY(git({"checkout", "--detach", "HEAD"}));
  writeFile("lost.txt", "recoverable\n");
  QVERIFY(git({"add", "."}));
  QVERIFY(git({"commit", "-m", "Unreferenced work"}));
  const QString hash = m_git->getCommitDetails("HEAD").hash;
  QVERIFY(git({"checkout", "main"}));
  GitLogDialog dialog(m_git, Theme());
  QVERIFY(!dialog.findChild<GitGraphWidget *>()->containsCommit(hash));
  dialog.selectCommit(hash);
  QCOMPARE(dialog.findChild<GitGraphWidget *>()->selectedHash(), hash);
  QVERIFY(dialog.findChild<QTextEdit *>("graphDetailView")
              ->toPlainText()
              .contains("Unreferenced work"));
}

QTEST_MAIN(TestGitLogDialog)
#include "test_gitlogdialog.moc"
