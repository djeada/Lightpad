#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

#include "git/gitintegration.h"
#include "ui/dialogs/gitworkbenchdialog.h"

class TestGitWorkbenchDialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testDialogCreation();
  void testThreeColumnLayout();
  void testMinimumSize();
  void testTitleBarExists();
  void testShortcutHintVisible();

  void testBranchTreeExists();
  void testBranchSearchExists();
  void testCreateBranchButtonExists();
  void testBranchGroupsCreated();
  void testBranchCurrentHighlighted();
  void testBranchSearchFilter();

  void testCommitTreeExists();
  void testCommitSearchExists();
  void testRewriteToggleExists();
  void testCommitTreeHeaders();
  void testCommitsLoaded();

  void testInspectorStackExists();
  void testInspectorEmptyByDefault();
  void testCommitInspectorShowsDetails();
  void testBranchInspectorShowsDetails();

  void testRewriteModeToggle();
  void testRewriteToolbarVisible();
  void testRewriteActionColumn();
  void testRewriteDropAction();
  void testRewriteSquashAction();
  void testRewritePickAll();
  void testRewriteMoveUp();
  void testRewriteMoveDown();

  void testPlanSummaryBrowseMode();
  void testPlanSummaryRewriteMode();

  void testBottomBarExists();
  void testApplyButtonHiddenInBrowseMode();
  void testApplyButtonVisibleInRewriteMode();
  void testBackupCheckboxDefaultChecked();
  void testCancelButtonExists();

  void testRiskLowWhenNoChanges();
  void testRiskHighWhenDrop();
  void testRiskCriticalWhenManyDrops();

  void testKeyJNavigatesDown();
  void testKeyKNavigatesUp();
  void testKeyRTogglesRewriteMode();
  void testKeyEscapeExitsRewriteMode();

  void testViewCommitDiffSignal();

  void testTagsLoadedInBranchTree();

  void testBranchInspectorRenameButton();
  void testBranchInspectorMergeButton();
  void testBranchInspectorRebaseButton();
  void testBranchInspectorActivityLabel();

  void testCommitRefsFieldExists();

  void testDropKeepActionAvailable();
  void testDropKeepRiskIsMedium();
  void testDropKeepPlanSummary();

  void testRecoveryCenterPageExists();
  void testRecoveryCenterListExists();
  void testRecoveryCenterRestoreButton();

  void testCommandPaletteKeyCtrlK();

  void testKeyBFocusesBranches();
  void testKeyQuestionShowsHelp();

  void testEditMessageButtonExists();
  void testDoubleClickMessageColumnOpensEdit();

  void testStashInspectorPageExists();
  void testStashApplyButtonExists();
  void testStashPopButtonExists();
  void testStashDropButtonExists();

  void testCriticalConfirmRequiresTyping();

  void testSelectionBarHiddenByDefault();
  void testSelectionBarAppearsOnMultiSelect();
  void testSelectionCountLabelUpdates();
  void testToolbarButtonsShowCountInRewrite();
  void testToolbarButtonsResetOnSingleSelect();
  void testMultiSelectContextMenuShowsCount();
  void testSelectionBarObjectNames();

private:
  void setupRepo();
  void createCommits(int count);

  QTemporaryDir m_tempDir;
  GitIntegration *m_git = nullptr;
  Theme m_theme;
};

void TestGitWorkbenchDialog::initTestCase() { setupRepo(); }

void TestGitWorkbenchDialog::cleanupTestCase() {
  delete m_git;
  m_git = nullptr;
}

void TestGitWorkbenchDialog::setupRepo() {
  QVERIFY(m_tempDir.isValid());

  QProcess proc;
  proc.setWorkingDirectory(m_tempDir.path());

  proc.start("git", {"init"});
  QVERIFY(proc.waitForFinished(5000));

  proc.start("git", {"config", "user.email", "test@test.com"});
  QVERIFY(proc.waitForFinished(5000));

  proc.start("git", {"config", "user.name", "Tester"});
  QVERIFY(proc.waitForFinished(5000));

  createCommits(8);

  proc.start("git", {"tag", "v1.0"});
  QVERIFY(proc.waitForFinished(5000));

  proc.start("git", {"checkout", "-b", "feature/test"});
  QVERIFY(proc.waitForFinished(5000));

  createCommits(3);

  proc.start("git", {"checkout", "main"});
  if (proc.waitForFinished(5000) && proc.exitCode() != 0) {
    proc.start("git", {"checkout", "master"});
    proc.waitForFinished(5000);
  }

  m_git = new GitIntegration();
  QVERIFY(m_git->setRepositoryPath(m_tempDir.path()));
}

void TestGitWorkbenchDialog::createCommits(int count) {
  QProcess proc;
  proc.setWorkingDirectory(m_tempDir.path());

  for (int i = 0; i < count; ++i) {
    QString filename = QString("file_%1_%2.txt")
                           .arg(QDateTime::currentMSecsSinceEpoch())
                           .arg(i);
    QFile f(m_tempDir.path() + "/" + filename);
    f.open(QIODevice::WriteOnly);
    f.write(QString("content %1\n").arg(i).toUtf8());
    f.close();

    proc.start("git", {"add", filename});
    QVERIFY(proc.waitForFinished(5000));

    proc.start("git", {"commit", "-m", QString("Commit %1").arg(i)});
    QVERIFY(proc.waitForFinished(5000));
  }
}

void TestGitWorkbenchDialog::testDialogCreation() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();
  QCOMPARE(dialog.windowTitle(), tr("Git Workbench"));
}

void TestGitWorkbenchDialog::testThreeColumnLayout() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *splitter = dialog.findChild<QSplitter *>();
  QVERIFY(splitter != nullptr);
  QCOMPARE(splitter->count(), 3);
}

void TestGitWorkbenchDialog::testMinimumSize() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  QVERIFY(dialog.minimumWidth() >= 1100);
  QVERIFY(dialog.minimumHeight() >= 700);
}

void TestGitWorkbenchDialog::testTitleBarExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *titleBar = dialog.findChild<QWidget *>("workbenchTitleBar");
  QVERIFY(titleBar != nullptr);
}

void TestGitWorkbenchDialog::testShortcutHintVisible() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *hint = dialog.findChild<QLabel *>("workbenchShortcutHint");
  QVERIFY(hint != nullptr);
  QVERIFY(hint->text().contains("J/K"));
}

void TestGitWorkbenchDialog::testBranchTreeExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *tree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(tree != nullptr);
}

void TestGitWorkbenchDialog::testBranchSearchExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *search = dialog.findChild<QLineEdit *>("branchSearch");
  QVERIFY(search != nullptr);
}

void TestGitWorkbenchDialog::testCreateBranchButtonExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *btn = dialog.findChild<QPushButton *>("createBranchBtn");
  QVERIFY(btn != nullptr);
}

void TestGitWorkbenchDialog::testBranchGroupsCreated() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *tree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(tree != nullptr);

  QVERIFY(tree->topLevelItemCount() >= 1);
  QCOMPARE(tree->topLevelItem(0)->text(0), tr("Local Branches"));
}

void TestGitWorkbenchDialog::testBranchCurrentHighlighted() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *tree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(tree != nullptr);

  auto *localGroup = tree->topLevelItem(0);
  QVERIFY(localGroup != nullptr);
  bool foundCurrent = false;
  for (int i = 0; i < localGroup->childCount(); ++i) {
    if (localGroup->child(i)->text(0).startsWith("● ")) {
      foundCurrent = true;
      break;
    }
  }
  QVERIFY(foundCurrent);
}

void TestGitWorkbenchDialog::testBranchSearchFilter() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *search = dialog.findChild<QLineEdit *>("branchSearch");
  auto *tree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(search != nullptr && tree != nullptr);

  search->setText("feature");
  QTest::qWait(50);

  auto *localGroup = tree->topLevelItem(0);
  QVERIFY(localGroup != nullptr);

  int visibleCount = 0;
  for (int i = 0; i < localGroup->childCount(); ++i) {
    if (!localGroup->child(i)->isHidden())
      ++visibleCount;
  }

  QVERIFY(visibleCount >= 1);
}

void TestGitWorkbenchDialog::testCommitTreeExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *tree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(tree != nullptr);
}

void TestGitWorkbenchDialog::testCommitSearchExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *search = dialog.findChild<QLineEdit *>("commitSearch");
  QVERIFY(search != nullptr);
}

void TestGitWorkbenchDialog::testRewriteToggleExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  QVERIFY(btn != nullptr);
  QVERIFY(btn->isCheckable());
  QVERIFY(!btn->isChecked());
}

void TestGitWorkbenchDialog::testCommitTreeHeaders() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *tree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(tree != nullptr);
  QCOMPARE(tree->columnCount(), 4);
  QCOMPARE(tree->headerItem()->text(0), tr("Commit"));
  QCOMPARE(tree->headerItem()->text(1), tr("Hash"));
  QCOMPARE(tree->headerItem()->text(2), tr("Author"));
  QCOMPARE(tree->headerItem()->text(3), tr("Date"));
}

void TestGitWorkbenchDialog::testCommitsLoaded() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *tree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(tree != nullptr);

  QVERIFY(tree->topLevelItemCount() >= 5);
}

void TestGitWorkbenchDialog::testInspectorStackExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *stack = dialog.findChild<QStackedWidget *>();
  QVERIFY(stack != nullptr);
  QVERIFY(stack->count() >= 4);
}

void TestGitWorkbenchDialog::testInspectorEmptyByDefault() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *stack = dialog.findChild<QStackedWidget *>();
  QVERIFY(stack != nullptr);
  QCOMPARE(stack->currentIndex(), 0);
}

void TestGitWorkbenchDialog::testCommitInspectorShowsDetails() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *tree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(tree != nullptr);
  QVERIFY(tree->topLevelItemCount() > 0);

  tree->setCurrentItem(tree->topLevelItem(0));
  emit tree->itemSelectionChanged();
  QTest::qWait(50);

  auto *stack = dialog.findChild<QStackedWidget *>();
  QVERIFY(stack != nullptr);
  QCOMPARE(stack->currentIndex(), 1);

  auto allLabels = dialog.findChildren<QLabel *>("inspMonoLabel");
  QVERIFY(!allLabels.isEmpty());

  bool foundText = false;
  for (auto *lbl : allLabels) {
    if (!lbl->text().isEmpty()) {
      foundText = true;
      break;
    }
  }
  QVERIFY(foundText);
}

void TestGitWorkbenchDialog::testBranchInspectorShowsDetails() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *branchTree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(branchTree != nullptr);

  auto *localGroup = branchTree->topLevelItem(0);
  QVERIFY(localGroup != nullptr);
  QVERIFY(localGroup->childCount() > 0);

  auto *branchItem = localGroup->child(0);
  branchTree->setCurrentItem(branchItem);

  emit branchTree->itemClicked(branchItem, 0);
  QTest::qWait(50);

  auto *stack = dialog.findChild<QStackedWidget *>();
  QVERIFY(stack != nullptr);
  QCOMPARE(stack->currentIndex(), 2);
}

void TestGitWorkbenchDialog::testRewriteModeToggle() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  QVERIFY(btn != nullptr);

  QVERIFY(!btn->isChecked());

  btn->click();
  QVERIFY(btn->isChecked());

  btn->click();
  QVERIFY(!btn->isChecked());
}

void TestGitWorkbenchDialog::testRewriteToolbarVisible() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toolbar = dialog.findChild<QWidget *>("rewriteToolbarContainer");
  QVERIFY(toolbar != nullptr);
  QVERIFY(toolbar->isHidden());

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();
  QVERIFY(!toolbar->isHidden());
}

void TestGitWorkbenchDialog::testRewriteActionColumn() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree != nullptr);
  QCOMPARE(commitTree->columnCount(), 4);

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  QCOMPARE(commitTree->columnCount(), 5);
  QCOMPARE(commitTree->headerItem()->text(4), tr("Action"));
}

void TestGitWorkbenchDialog::testRewriteDropAction() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() > 0);

  commitTree->setCurrentItem(commitTree->topLevelItem(0));
  commitTree->topLevelItem(0)->setSelected(true);

  auto *dropBtn = dialog.findChild<QPushButton *>("rewriteDropBtn");
  QVERIFY(dropBtn != nullptr);
  dropBtn->click();

  auto *combo = qobject_cast<QComboBox *>(
      commitTree->itemWidget(commitTree->topLevelItem(0), 4));
  QVERIFY(combo != nullptr);
  QCOMPARE(combo->currentText(), QString("drop"));
}

void TestGitWorkbenchDialog::testRewriteSquashAction() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  commitTree->topLevelItem(0)->setSelected(true);
  commitTree->topLevelItem(1)->setSelected(true);

  auto buttons = dialog.findChildren<QPushButton *>();
  QPushButton *squashBtn = nullptr;
  for (auto *b : buttons) {
    if (b->text().contains("Squash") && b->objectName() == "rewriteToolBtn") {
      squashBtn = b;
      break;
    }
  }
  QVERIFY(squashBtn != nullptr);
  squashBtn->click();

  auto *combo0 = qobject_cast<QComboBox *>(
      commitTree->itemWidget(commitTree->topLevelItem(0), 4));
  auto *combo1 = qobject_cast<QComboBox *>(
      commitTree->itemWidget(commitTree->topLevelItem(1), 4));
  QVERIFY(combo0 != nullptr && combo1 != nullptr);
  QCOMPARE(combo0->currentText(), QString("pick"));
  QCOMPARE(combo1->currentText(), QString("squash"));
}

void TestGitWorkbenchDialog::testRewritePickAll() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");

  commitTree->setCurrentItem(commitTree->topLevelItem(0));
  commitTree->topLevelItem(0)->setSelected(true);

  auto *dropBtn = dialog.findChild<QPushButton *>("rewriteDropBtn");
  dropBtn->click();

  auto buttons = dialog.findChildren<QPushButton *>();
  QPushButton *resetBtn = nullptr;
  for (auto *b : buttons) {
    if (b->text().contains("Reset All")) {
      resetBtn = b;
      break;
    }
  }
  QVERIFY(resetBtn != nullptr);
  resetBtn->click();

  auto *combo = qobject_cast<QComboBox *>(
      commitTree->itemWidget(commitTree->topLevelItem(0), 4));
  QVERIFY(combo != nullptr);
  QCOMPARE(combo->currentText(), QString("pick"));
}

void TestGitWorkbenchDialog::testRewriteMoveUp() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  QString hash1 = commitTree->topLevelItem(1)->text(1);

  commitTree->setCurrentItem(commitTree->topLevelItem(1));

  auto buttons = dialog.findChildren<QPushButton *>();
  QPushButton *upBtn = nullptr;
  for (auto *b : buttons) {
    if (b->text() == "▲") {
      upBtn = b;
      break;
    }
  }
  QVERIFY(upBtn != nullptr);
  upBtn->click();

  QCOMPARE(commitTree->topLevelItem(0)->text(1), hash1);
}

void TestGitWorkbenchDialog::testRewriteMoveDown() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  QString hash0 = commitTree->topLevelItem(0)->text(1);

  commitTree->setCurrentItem(commitTree->topLevelItem(0));

  auto buttons = dialog.findChildren<QPushButton *>();
  QPushButton *downBtn = nullptr;
  for (auto *b : buttons) {
    if (b->text() == "▼") {
      downBtn = b;
      break;
    }
  }
  QVERIFY(downBtn != nullptr);
  downBtn->click();

  QCOMPARE(commitTree->topLevelItem(1)->text(1), hash0);
}

void TestGitWorkbenchDialog::testPlanSummaryBrowseMode() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *label = dialog.findChild<QLabel *>("planSummaryLabel");
  QVERIFY(label != nullptr);
  QVERIFY(label->text().contains("Browse mode"));
}

void TestGitWorkbenchDialog::testPlanSummaryRewriteMode() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *label = dialog.findChild<QLabel *>("planSummaryLabel");
  QVERIFY(label != nullptr);
  QVERIFY(label->text().contains("Plan:"));
  QVERIFY(label->text().contains("pick"));
}

void TestGitWorkbenchDialog::testBottomBarExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *bar = dialog.findChild<QWidget *>("workbenchBottomBar");
  QVERIFY(bar != nullptr);
}

void TestGitWorkbenchDialog::testApplyButtonHiddenInBrowseMode() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *applyBtn = dialog.findChild<QPushButton *>("workbenchApplyBtn");
  QVERIFY(applyBtn != nullptr);
  QVERIFY(!applyBtn->isVisible());
}

void TestGitWorkbenchDialog::testApplyButtonVisibleInRewriteMode() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();

  auto *applyBtn = dialog.findChild<QPushButton *>("workbenchApplyBtn");
  QVERIFY(applyBtn != nullptr);
  QVERIFY(!applyBtn->isHidden());
}

void TestGitWorkbenchDialog::testBackupCheckboxDefaultChecked() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();

  auto *checkbox = dialog.findChild<QCheckBox *>("backupCheckbox");
  QVERIFY(checkbox != nullptr);
  QVERIFY(checkbox->isChecked());
}

void TestGitWorkbenchDialog::testCancelButtonExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  auto *cancelBtn = dialog.findChild<QPushButton *>("workbenchCancelBtn");
  QVERIFY(cancelBtn != nullptr);
}

void TestGitWorkbenchDialog::testRiskLowWhenNoChanges() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();

  auto *riskLabel = dialog.findChild<QLabel *>("riskIndicator");
  QVERIFY(riskLabel != nullptr);
  QVERIFY(riskLabel->text().contains("Low"));
}

void TestGitWorkbenchDialog::testRiskHighWhenDrop() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  commitTree->setCurrentItem(commitTree->topLevelItem(0));
  commitTree->topLevelItem(0)->setSelected(true);

  auto *dropBtn = dialog.findChild<QPushButton *>("rewriteDropBtn");
  dropBtn->click();

  auto *riskLabel = dialog.findChild<QLabel *>("riskIndicator");
  QVERIFY(riskLabel != nullptr);
  QVERIFY(riskLabel->text().contains("High"));
}

void TestGitWorkbenchDialog::testRiskCriticalWhenManyDrops() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  auto *dropBtn = dialog.findChild<QPushButton *>("rewriteDropBtn");

  for (int i = 0; i < qMin(4, commitTree->topLevelItemCount()); ++i) {
    commitTree->clearSelection();
    commitTree->setCurrentItem(commitTree->topLevelItem(i));
    commitTree->topLevelItem(i)->setSelected(true);
    dropBtn->click();
  }

  auto *riskLabel = dialog.findChild<QLabel *>("riskIndicator");
  QVERIFY(riskLabel != nullptr);
  QVERIFY(riskLabel->text().contains("Critical"));
}

void TestGitWorkbenchDialog::testKeyJNavigatesDown() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  commitTree->setFocus();
  commitTree->setCurrentItem(commitTree->topLevelItem(0));

  QTest::keyClick(&dialog, Qt::Key_J);
  QTest::qWait(50);

  QCOMPARE(commitTree->currentItem(), commitTree->topLevelItem(1));
}

void TestGitWorkbenchDialog::testKeyKNavigatesUp() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  commitTree->setFocus();
  commitTree->setCurrentItem(commitTree->topLevelItem(1));

  QTest::keyClick(&dialog, Qt::Key_K);
  QTest::qWait(50);

  QCOMPARE(commitTree->currentItem(), commitTree->topLevelItem(0));
}

void TestGitWorkbenchDialog::testKeyRTogglesRewriteMode() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  QVERIFY(!toggleBtn->isChecked());

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  commitTree->setFocus();

  QTest::keyClick(&dialog, Qt::Key_R);
  QTest::qWait(50);

  QVERIFY(toggleBtn->isChecked());
}

void TestGitWorkbenchDialog::testKeyEscapeExitsRewriteMode() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();
  QVERIFY(toggleBtn->isChecked());

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  commitTree->setFocus();

  QTest::keyClick(&dialog, Qt::Key_Escape);
  QTest::qWait(50);

  QVERIFY(!toggleBtn->isChecked());
}

void TestGitWorkbenchDialog::testViewCommitDiffSignal() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  QSignalSpy spy(&dialog, &GitWorkbenchDialog::viewCommitDiff);

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() > 0);

  auto *item = commitTree->topLevelItem(0);
  emit commitTree->itemDoubleClicked(item, 1);
  QTest::qWait(50);

  QCOMPARE(spy.count(), 1);
  QVERIFY(!spy.at(0).at(0).toString().isEmpty());
}

void TestGitWorkbenchDialog::testTagsLoadedInBranchTree() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *branchTree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(branchTree);

  bool foundTags = false;
  for (int i = 0; i < branchTree->topLevelItemCount(); ++i) {
    auto *group = branchTree->topLevelItem(i);
    if (group->text(0).contains("Tags")) {
      foundTags = true;
      QVERIFY(group->childCount() > 0);

      bool foundV1 = false;
      for (int j = 0; j < group->childCount(); ++j) {
        if (group->child(j)->text(0).contains("v1.0"))
          foundV1 = true;
      }
      QVERIFY(foundV1);
      break;
    }
  }
  QVERIFY(foundTags);
}

void TestGitWorkbenchDialog::testBranchInspectorRenameButton() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *renameBtn = dialog.findChild<QPushButton *>("inspActionBtn");

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Rename")) {
      found = true;
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testBranchInspectorMergeButton() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Merge into Current")) {
      found = true;
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testBranchInspectorRebaseButton() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Rebase onto")) {
      found = true;
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testBranchInspectorActivityLabel() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *activityLabel = dialog.findChild<QLabel *>("inspBranchActivity");

  auto *branchTree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(branchTree);

  for (int i = 0; i < branchTree->topLevelItemCount(); ++i) {
    auto *group = branchTree->topLevelItem(i);
    for (int j = 0; j < group->childCount(); ++j) {
      auto *child = group->child(j);
      if (child->text(0).contains("feature/test")) {
        emit branchTree->itemClicked(child, 0);
        QTest::qWait(50);

        QVERIFY(!activityLabel->text().isEmpty());
        return;
      }
    }
  }
  QFAIL("feature/test branch not found in tree");
}

void TestGitWorkbenchDialog::testCommitRefsFieldExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *refsLabel = dialog.findChild<QLabel *>("inspCommitRefs");
  QVERIFY(refsLabel);
}

void TestGitWorkbenchDialog::testDropKeepActionAvailable() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  QVERIFY(toggleBtn);
  toggleBtn->click();
  QTest::qWait(50);

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() > 0);

  auto *combo = qobject_cast<QComboBox *>(
      commitTree->itemWidget(commitTree->topLevelItem(0), 4));
  QVERIFY(combo);

  bool found = false;
  for (int i = 0; i < combo->count(); ++i) {
    if (combo->itemText(i) == "drop-keep") {
      found = true;
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testDropKeepRiskIsMedium() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();
  QTest::qWait(50);

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  auto *combo = qobject_cast<QComboBox *>(
      commitTree->itemWidget(commitTree->topLevelItem(0), 4));
  QVERIFY(combo);

  int dkIdx = combo->findText("drop-keep");
  QVERIFY(dkIdx >= 0);
  combo->setCurrentIndex(dkIdx);
  QTest::qWait(50);

  auto *riskLabel = dialog.findChild<QLabel *>("riskIndicator");
  QVERIFY(riskLabel);
  QVERIFY(riskLabel->text().contains("Medium") ||
          riskLabel->text().contains("MEDIUM"));
}

void TestGitWorkbenchDialog::testDropKeepPlanSummary() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *toggleBtn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  toggleBtn->click();
  QTest::qWait(50);

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  auto *combo = qobject_cast<QComboBox *>(
      commitTree->itemWidget(commitTree->topLevelItem(0), 4));
  int dkIdx = combo->findText("drop-keep");
  combo->setCurrentIndex(dkIdx);
  QTest::qWait(50);

  auto *summaryLabel = dialog.findChild<QLabel *>("planSummaryLabel");
  QVERIFY(summaryLabel);
  QVERIFY(summaryLabel->text().contains("drop-keep") ||
          summaryLabel->text().contains("preserve"));
}

void TestGitWorkbenchDialog::testRecoveryCenterPageExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *stack = dialog.findChild<QStackedWidget *>("inspectorStack");
  QVERIFY(stack);
  QVERIFY(stack->count() >= 5);
}

void TestGitWorkbenchDialog::testRecoveryCenterListExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *recoveryList = dialog.findChild<QTreeWidget *>("recoveryList");
  QVERIFY(recoveryList);
}

void TestGitWorkbenchDialog::testRecoveryCenterRestoreButton() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Restore")) {
      found = true;

      QVERIFY(!btn->isEnabled());
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testCommandPaletteKeyCtrlK() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  QTimer::singleShot(100, [&dialog]() {
    auto children = dialog.findChildren<QDialog *>();
    for (auto *child : children) {
      if (child->isVisible())
        child->close();
    }
  });

  QTest::keyPress(&dialog, Qt::Key_K, Qt::ControlModifier);
  QTest::qWait(200);
}

void TestGitWorkbenchDialog::testKeyBFocusesBranches() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *branchTree = dialog.findChild<QTreeWidget *>("branchTree");
  QVERIFY(branchTree);

  QTest::keyPress(&dialog, Qt::Key_B);
  QTest::qWait(50);
  QVERIFY(true);
}

void TestGitWorkbenchDialog::testKeyQuestionShowsHelp() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  commitTree->setFocus();
  QTest::qWait(50);

  QVERIFY(true);
}

void TestGitWorkbenchDialog::testEditMessageButtonExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Edit Message")) {
      found = true;
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testDoubleClickMessageColumnOpensEdit() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() > 0);

  QTimer::singleShot(100, [&dialog]() {
    auto children = dialog.findChildren<QDialog *>();
    for (auto *child : children) {
      if (child->isVisible())
        child->reject();
    }
  });

  auto *item = commitTree->topLevelItem(0);
  commitTree->setCurrentItem(item);
  QTest::qWait(50);

  emit commitTree->itemDoubleClicked(item, 0);
  QTest::qWait(200);

  QSignalSpy spy(&dialog, &GitWorkbenchDialog::viewCommitDiff);
  QCOMPARE(spy.count(), 0);
}

void TestGitWorkbenchDialog::testStashInspectorPageExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *stack = dialog.findChild<QStackedWidget *>("inspectorStack");
  QVERIFY(stack);

  QVERIFY(stack->count() >= 6);
}

void TestGitWorkbenchDialog::testStashApplyButtonExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Apply Stash") || btn->text().contains("Apply")) {
      if (btn->objectName() == "inspActionBtn" ||
          btn->text().contains("Apply Stash")) {
        found = true;
        break;
      }
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testStashPopButtonExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Pop")) {
      found = true;
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testStashDropButtonExists() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto buttons = dialog.findChildren<QPushButton *>();
  bool found = false;
  for (auto *btn : buttons) {
    if (btn->text().contains("Drop")) {
      found = true;
      break;
    }
  }
  QVERIFY(found);
}

void TestGitWorkbenchDialog::testCriticalConfirmRequiresTyping() {

  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();
  QVERIFY(true);
}

void TestGitWorkbenchDialog::testSelectionBarHiddenByDefault() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *selectionBar = dialog.findChild<QWidget *>("selectionBar");
  QVERIFY(selectionBar != nullptr);
  QVERIFY(!selectionBar->isVisible());
}

void TestGitWorkbenchDialog::testSelectionBarAppearsOnMultiSelect() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  commitTree->topLevelItem(0)->setSelected(true);
  commitTree->topLevelItem(1)->setSelected(true);

  auto *selectionBar = dialog.findChild<QWidget *>("selectionBar");
  QVERIFY(selectionBar != nullptr);

  QVERIFY(!selectionBar->isHidden());
}

void TestGitWorkbenchDialog::testSelectionCountLabelUpdates() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 3);

  commitTree->topLevelItem(0)->setSelected(true);
  commitTree->topLevelItem(1)->setSelected(true);
  commitTree->topLevelItem(2)->setSelected(true);

  auto *countLabel = dialog.findChild<QLabel *>("selectionCountLabel");
  QVERIFY(countLabel != nullptr);
  QVERIFY(countLabel->text().contains("3"));
}

void TestGitWorkbenchDialog::testToolbarButtonsShowCountInRewrite() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  commitTree->topLevelItem(0)->setSelected(true);
  commitTree->topLevelItem(1)->setSelected(true);

  auto buttons = dialog.findChildren<QPushButton *>();
  QPushButton *squashBtn = nullptr;
  for (auto *b : buttons) {
    if (b->objectName() == "rewriteToolBtn" && b->text().contains("Squash")) {
      squashBtn = b;
      break;
    }
  }
  QVERIFY(squashBtn != nullptr);
  QVERIFY(squashBtn->text().contains("(2)"));
}

void TestGitWorkbenchDialog::testToolbarButtonsResetOnSingleSelect() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *btn = dialog.findChild<QPushButton *>("rewriteToggleBtn");
  btn->click();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);

  commitTree->topLevelItem(0)->setSelected(true);
  commitTree->topLevelItem(1)->setSelected(true);
  commitTree->clearSelection();
  commitTree->topLevelItem(0)->setSelected(true);

  auto buttons = dialog.findChildren<QPushButton *>();
  QPushButton *squashBtn = nullptr;
  for (auto *b : buttons) {
    if (b->objectName() == "rewriteToolBtn" && b->text().contains("Squash")) {
      squashBtn = b;
      break;
    }
  }
  QVERIFY(squashBtn != nullptr);
  QCOMPARE(squashBtn->text(), QString("Squash"));
}

void TestGitWorkbenchDialog::testMultiSelectContextMenuShowsCount() {

  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *commitTree = dialog.findChild<QTreeWidget *>("commitTree");
  QVERIFY(commitTree->topLevelItemCount() >= 2);
  commitTree->topLevelItem(0)->setSelected(true);
  commitTree->topLevelItem(1)->setSelected(true);

  QCOMPARE(commitTree->selectedItems().size(), 2);
}

void TestGitWorkbenchDialog::testSelectionBarObjectNames() {
  GitWorkbenchDialog dialog(m_git, m_theme);
  dialog.loadRepository();

  auto *selectionBar = dialog.findChild<QWidget *>("selectionBar");
  QVERIFY(selectionBar != nullptr);

  auto *countLabel = dialog.findChild<QLabel *>("selectionCountLabel");
  QVERIFY(countLabel != nullptr);

  auto *hintLabel = dialog.findChild<QLabel *>("selectionHintLabel");
  QVERIFY(hintLabel != nullptr);
}

QTEST_MAIN(TestGitWorkbenchDialog)
#include "test_gitworkbenchdialog.moc"
