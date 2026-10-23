

#include "git/gitintegration.h"
#include "ui/dialogs/bisectdialog.h"
#include "ui/dialogs/branchhygienedialog.h"
#include "ui/dialogs/branchsyncradardialog.h"
#include "ui/dialogs/commandmirrordialog.h"
#include "ui/dialogs/commitcraftingdialog.h"
#include "ui/dialogs/compareanythingdialog.h"
#include "ui/dialogs/filetimelinedialog.h"
#include "ui/dialogs/gitdiffdialog.h"
#include "ui/dialogs/gitfilehistorydialog.h"
#include "ui/dialogs/gitinitdialog.h"
#include "ui/dialogs/gitlogdialog.h"
#include "ui/dialogs/gitobjectexplorerdialog.h"
#include "ui/dialogs/gitrebasedialog.h"
#include "ui/dialogs/gitremotedialog.h"
#include "ui/dialogs/gitrepositorybrowser.h"
#include "ui/dialogs/gitrepotoolsdialog.h"
#include "ui/dialogs/gitstashdialog.h"
#include "ui/dialogs/gitworkbenchdialog.h"
#include "ui/dialogs/integrationadvisordialog.h"
#include "ui/dialogs/mergestartdialog.h"
#include "ui/dialogs/operationpreviewdialog.h"
#include "ui/dialogs/provenancelensdialog.h"
#include "ui/dialogs/rebasetimelinedialog.h"
#include "ui/dialogs/recoverycenterdialog.h"
#include "ui/dialogs/stagingcanvasdialog.h"
#include "ui/dialogs/stashshelfdialog.h"
#include "ui/dialogs/timetraveldialog.h"
#include "ui/dialogs/undochangeswizarddialog.h"
#include "ui/dialogs/whatifsandboxdialog.h"
#include "ui/dialogs/worktreemapdialog.h"
#include "ui/panels/conflictcenterpanel.h"
#include "ui/panels/conflictresolverview.h"
#include "ui/panels/sourcecontrolpanel.h"
#include "ui/widgets/flowlayout.h"
#include "ui/widgets/gitgraphwidget.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHeaderView>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>
#include <memory>

namespace {
Theme lightTheme() {
  Theme theme;
  theme.backgroundColor = QColor("#f6f7fa");
  theme.foregroundColor = QColor("#1b2430");
  theme.surfaceColor = QColor("#ffffff");
  theme.surfaceAltColor = QColor("#e8edf3");
  theme.borderColor = QColor("#c4ccd8");
  theme.accentColor = QColor("#2563eb");
  theme.accentSoftColor = QColor("#dbeafe");
  theme.hoverColor = QColor("#e1e8f2");
  theme.pressedColor = QColor("#c8d6ed");
  return theme;
}
} // namespace

class TestGitUiConsistency : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void surfaces_data();
  void surfaces();
  void remoteSelectionStates();
  void actionWrappingPreservesInteraction();
  void browserDoesNotBlockNavigation();
  void diffIncludesFilesBeyondOldPreviewLimit();
  void browserColumnsCanShrink();
  void cleanupTestCase() { QThreadPool::globalInstance()->waitForDone(); }

private:
  QByteArray git(const QStringList &args);
  void write(const QString &path, const QByteArray &contents);
  std::unique_ptr<QWidget> create(const QString &name, const Theme &theme);
  QTemporaryDir m_repo;
  QTemporaryDir m_conflictRepo;
  GitIntegration m_git;
  GitIntegration m_conflictGit;
};

QByteArray TestGitUiConsistency::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repo.path());
  process.start("git", args);
  if (!process.waitForFinished(10000) || process.exitCode() != 0)
    qFatal("Git fixture failed: %s: %s", qPrintable(args.join(' ')),
           process.readAllStandardError().constData());
  return process.readAllStandardOutput().trimmed();
}

void TestGitUiConsistency::write(const QString &path,
                                 const QByteArray &contents) {
  QFile file(m_repo.filePath(path));
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(contents), contents.size());
}

void TestGitUiConsistency::initTestCase() {
  QVERIFY(m_repo.isValid());
  git({"init", "-b", "main"});
  git({"config", "user.name", "UI Tester"});
  git({"config", "user.email", "ui@example.invalid"});
  write("sample.txt", "first line\nsecond line\n");
  git({"add", "."});
  git({"commit", "-m", "Create a readable history fixture"});
  git({"branch", "feature/example"});
  write("sample.txt", "first line\nmain branch change\n");
  git({"commit", "-am", "Improve the main branch"});
  git({"tag", "v1.0"});
  git({"checkout", "feature/example"});
  write("feature.txt", "feature branch content\n");
  git({"add", "."});
  git({"commit", "-m", "Add a feature on a separate branch"});
  git({"checkout", "main"});
  write("sample.txt", "first line\nsaved work\n");
  git({"stash", "push", "-m", "Work saved for later"});
  git({"gc", "--prune=now"});
  git({"remote", "add", "origin", m_repo.filePath("upstream.git")});
  write("sample.txt", "first line\nstaged change\n");
  git({"add", "sample.txt"});
  write("sample.txt", "first line\nstaged change\nworking tree change\n");
  write("untracked.txt", "new file\n");
  QVERIFY(m_git.setRepositoryPath(m_repo.path()));
  m_git.setAutoRefreshInterval(0);
  QVERIFY(m_conflictRepo.isValid());
  QProcess conflict;
  conflict.setWorkingDirectory(m_conflictRepo.path());
  const auto run = [&](const QStringList &args, bool succeeds = true) {
    conflict.start("git", args);
    QVERIFY(conflict.waitForFinished(10000));
    QCOMPARE(conflict.exitCode() == 0, succeeds);
  };
  const auto content = [&](const QByteArray &bytes) {
    QFile file(m_conflictRepo.filePath("conflicted.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
  };
  run({"init", "-b", "main"});
  run({"config", "user.name", "UI Tester"});
  run({"config", "user.email", "ui@example.invalid"});
  content("context before\noriginal line\ncontext after\n");
  run({"add", "."});
  run({"commit", "-m", "Common base"});
  run({"checkout", "-b", "incoming"});
  content("context before\nincoming change\ncontext after\n");
  run({"commit", "-am", "Incoming change"});
  run({"checkout", "main"});
  content("context before\nmy local change\ncontext after\n");
  run({"commit", "-am", "Local change"});
  run({"merge", "incoming"}, false);
  QVERIFY(m_conflictGit.setRepositoryPath(m_conflictRepo.path()));
  m_conflictGit.setAutoRefreshInterval(0);
}

void TestGitUiConsistency::surfaces_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("light");
  const QStringList names = {"init",
                             "push",
                             "pull",
                             "fetch",
                             "remotes",
                             "stash",
                             "tools",
                             "log",
                             "diff",
                             "filehistory",
                             "objects",
                             "objects-dag",
                             "objects-disk",
                             "browser",
                             "workbench",
                             "workbench-inspect",
                             "workbench-rewrite",
                             "log-selected",
                             "rebase",
                             "hygiene",
                             "sync",
                             "bisect",
                             "commands",
                             "crafting",
                             "compare",
                             "timeline",
                             "advisor",
                             "merge",
                             "preview",
                             "provenance",
                             "rebase-timeline",
                             "recovery",
                             "staging",
                             "shelf",
                             "time-travel",
                             "undo",
                             "sandbox",
                             "worktrees",
                             "source-control",
                             "source-control-empty",
                             "source-control-conflict",
                             "conflicts",
                             "conflicts-active",
                             "resolver",
                             "resolver-active"};
  for (const auto &name : names)
    for (bool light : {false, true})
      QTest::newRow(qPrintable(name + (light ? "-light" : "-dark")))
          << name << light;
}

std::unique_ptr<QWidget> TestGitUiConsistency::create(const QString &name,
                                                      const Theme &theme) {
  QWidget *surface = nullptr;
  if (name == "browser") {
    surface = new GitRepositoryBrowser(m_repo.path(), theme);
    surface->setAttribute(Qt::WA_DeleteOnClose, false);
  } else if (name == "init")
    surface = new GitInitDialog(m_repo.path());
  else if (name == "push")
    surface = new GitRemoteDialog(&m_git, GitRemoteDialog::Mode::Push);
  else if (name == "pull")
    surface = new GitRemoteDialog(&m_git, GitRemoteDialog::Mode::Pull);
  else if (name == "fetch")
    surface = new GitRemoteDialog(&m_git, GitRemoteDialog::Mode::Fetch);
  else if (name == "remotes")
    surface = new GitRemoteDialog(&m_git, GitRemoteDialog::Mode::ManageRemotes);
  else if (name == "stash")
    surface = new GitStashDialog(&m_git);
  else if (name == "tools")
    surface = new GitRepoToolsDialog(&m_git);
  else if (name.startsWith("log")) {
    auto *log = new GitLogDialog(&m_git, theme);
    if (name == "log-selected")
      log->selectCommit(m_git.getCommitDetails("HEAD").hash);
    surface = log;
  } else if (name == "diff") {
    auto *diff = new GitDiffDialog(
        &m_git, "sample.txt", GitDiffDialog::DiffTarget::File, false, theme);
    diff->setDiffText(m_git.getFileDiff("sample.txt", false));
    surface = diff;
  } else if (name == "filehistory")
    surface = new GitFileHistoryDialog(&m_git, "sample.txt");
  else if (name.startsWith("objects")) {
    auto *objects = new GitObjectExplorerDialog(m_repo.path(), theme);
    objects->searchHash(m_git.getCommitDetails("HEAD").hash);
    if (name == "objects-dag")
      objects->findChild<QTabWidget *>("objectDetailTabs")->setCurrentIndex(2);
    if (name == "objects-disk")
      objects->findChild<QTabWidget *>("objectDetailTabs")
          ->setCurrentWidget(objects->findChild<QWidget *>("objectDiskPage"));
    surface = objects;
  } else if (name.startsWith("workbench")) {
    auto *workbench = new GitWorkbenchDialog(&m_git, theme);
    if (name == "workbench-inspect") {
      auto *tree = workbench->findChild<QTreeWidget *>("commitTree");
      tree->setCurrentItem(tree->topLevelItem(0));
    } else if (name == "workbench-rewrite") {
      workbench->findChild<QPushButton *>("rewriteToggleBtn")->click();
    }
    surface = workbench;
  } else if (name == "rebase") {
    auto *rebase = new GitRebaseDialog(&m_git, theme);
    rebase->loadCommits("HEAD~1");
    surface = rebase;
  } else if (name == "hygiene")
    surface = new BranchHygieneDialog(&m_git, theme);
  else if (name == "sync")
    surface = new BranchSyncRadarDialog(&m_git, theme);
  else if (name == "bisect")
    surface = new BisectDialog(&m_git, theme);
  else if (name == "commands")
    surface = new CommandMirrorDialog(&m_git, theme);
  else if (name == "crafting")
    surface = new CommitCraftingDialog(&m_git, theme);
  else if (name == "compare")
    surface = new CompareAnythingDialog(&m_git, theme);
  else if (name == "timeline")
    surface = new FileTimelineDialog(&m_git, "sample.txt", theme);
  else if (name == "advisor")
    surface = new IntegrationAdvisorDialog(&m_git, theme);
  else if (name == "merge")
    surface = new MergeStartDialog(&m_git, theme);
  else if (name == "preview") {
    GitOperationRequest request;
    request.target = "feature/example";
    surface = new OperationPreviewDialog(&m_git, request, theme);
  } else if (name == "provenance")
    surface = new ProvenanceLensDialog(&m_git, "sample.txt", 1, 2, theme);
  else if (name == "rebase-timeline")
    surface = new RebaseTimelineDialog(&m_git, theme);
  else if (name == "recovery")
    surface = new RecoveryCenterDialog(&m_git, theme);
  else if (name == "staging")
    surface = new StagingCanvasDialog(&m_git, theme);
  else if (name == "shelf")
    surface = new StashShelfDialog(&m_git, theme);
  else if (name == "time-travel")
    surface = new TimeTravelDialog(&m_git, "HEAD~1", theme);
  else if (name == "undo")
    surface = new UndoChangesWizardDialog(&m_git, theme);
  else if (name == "sandbox")
    surface = new WhatIfSandboxDialog(&m_git, theme);
  else if (name == "worktrees")
    surface = new WorktreeMapDialog(&m_git, theme);
  else if (name.startsWith("source-control")) {
    auto *panel = new SourceControlPanel;
    if (name != "source-control-empty")
      panel->setGitIntegration(name.endsWith("conflict") ? &m_conflictGit
                                                         : &m_git);
    panel->applyTheme(theme);
    surface = panel;
  } else if (name.startsWith("conflicts")) {
    auto *panel = new ConflictCenterPanel;
    panel->setGitIntegration(name.endsWith("active") ? &m_conflictGit : &m_git);
    panel->applyTheme(theme);
    surface = panel;
  } else if (name.startsWith("resolver")) {
    auto *panel = new ConflictResolverView(
        name.endsWith("active") ? &m_conflictGit : &m_git,
        name.endsWith("active") ? "conflicted.txt" : "sample.txt");
    panel->applyTheme(theme);
    surface = panel;
  }
  if (auto *dialog = dynamic_cast<StyledDialog *>(surface))
    dialog->applyTheme(theme);
  return std::unique_ptr<QWidget>(surface);
}

void TestGitUiConsistency::browserDoesNotBlockNavigation() {
  auto browser = std::make_unique<GitRepositoryBrowser>(m_repo.path(), Theme());
  browser->setAttribute(Qt::WA_DeleteOnClose, false);
  browser->resize(1800, 960);
  browser->show();
  const QString head = QString::fromLatin1(git({"rev-parse", "HEAD"}));
  const QString tree = QString::fromLatin1(git({"rev-parse", "HEAD^{tree}"}));
  browser->showObject(head);
  auto *objects = browser->findChild<GitObjectExplorerDialog *>();
  auto *history = browser->findChild<GitLogDialog *>();
  auto *summary = objects->findChild<QLabel *>("objectDetailSummary");
  QTRY_VERIFY(summary->text().contains(head));
  QVERIFY(!QApplication::activeModalWidget());

  // Single-click a real card in the embedded view. Its contents must appear
  // without closing a window or switching out of the graph.
  auto *dag = objects->findChild<QGraphicsView *>("objectDagView");
  QGraphicsItem *card = nullptr;
  for (auto *item : dag->scene()->items())
    if (item->data(0).toString() == tree) {
      card = item;
      break;
    }
  QVERIFY(card);
  dag->centerOn(card);
  QTest::mouseClick(dag->viewport(), Qt::LeftButton, Qt::NoModifier,
                    dag->mapFromScene(card->sceneBoundingRect().center()));
  QTRY_VERIFY(summary->text().contains(tree));
  auto *preview = objects->findChild<QPlainTextEdit *>("objectDagPreview");
  QVERIFY(preview->isVisible());
  QTRY_VERIFY(preview->toPlainText().contains("sample.txt"));

  // Split view leaves both panes interactive. Selecting a history row locates
  // that commit across storage even if a pack was previously selected.
  QTest::mouseClick(browser->findChild<QPushButton *>("gitBrowserMode2"),
                    Qt::LeftButton);
  QVERIFY(objects->isVisible());
  QVERIFY(history->isVisible());
  QVERIFY(!QApplication::activeModalWidget());
  auto *graph = history->findChild<GitGraphWidget *>();
  QTRY_VERIFY(graph->height() >= 200);
  QCoreApplication::processEvents();
  const auto commits = graph->loadedCommits();
  QVERIFY(commits.size() > 1);
  graph->findChild<QScrollBar *>()->setValue(0);
  const int row = 1 + (graph->hasWorkingTreeNode() ? 1 : 0);
  QTest::mouseClick(graph, Qt::LeftButton, Qt::NoModifier,
                    QPoint(graph->width() / 2,
                           graph->rowHeight() * row + graph->rowHeight() / 2));
  QCOMPARE(graph->selectedHash(), commits.at(1).hash);
  QTRY_VERIFY(summary->text().contains(commits.at(1).hash));
  auto *packs = objects->findChild<QTreeWidget *>("objectPackTable");
  QVERIFY(packs->isEnabled());
  QTest::mouseClick(packs->viewport(), Qt::LeftButton, Qt::NoModifier,
                    packs->visualItemRect(packs->topLevelItem(1)).center());
  QVERIFY(objects->findChild<QPushButton *>("objectVerifyButton")->isEnabled());
  QTRY_VERIFY(
      objects->findChild<QTreeWidget *>("objectResultsTable")->isVisible());
  QVERIFY(
      objects->findChild<QLineEdit *>("objectHashSearch")->text().isEmpty());

  QTest::mouseClick(history->findChild<QPushButton *>("graphViewDiffButton"),
                    Qt::LeftButton);
  auto *diff = browser->findChild<GitDiffDialog *>();
  QVERIFY(diff);
  QVERIFY(diff->isVisible());
  QVERIFY(!QApplication::activeModalWidget());
  QTest::mouseClick(history->findChild<QPushButton *>("graphViewDiffButton"),
                    Qt::LeftButton);
  QCOMPARE(browser->findChildren<GitDiffDialog *>().size(), 1);
  diff->close();

  QPushButton unrelated("Editor remains usable");
  unrelated.show();
  QSignalSpy clicked(&unrelated, &QPushButton::clicked);
  QTest::mouseClick(&unrelated, Qt::LeftButton);
  QCOMPARE(clicked.count(), 1);
  const int dialogs = browser->findChildren<GitObjectExplorerDialog *>().size();
  history->inspectObjectRequested(head);
  QTRY_VERIFY(summary->text().contains(head));
  QCOMPARE(browser->findChildren<GitObjectExplorerDialog *>().size(), dialogs);
  QCOMPARE(browser->repositoryPath(), m_repo.path());
}

void TestGitUiConsistency::browserColumnsCanShrink() {
  auto browser = std::make_unique<GitRepositoryBrowser>(m_repo.path(), Theme());
  browser->setAttribute(Qt::WA_DeleteOnClose, false);
  browser->resize(1400, 900);
  browser->show();
  browser->showObject(QString::fromLatin1(git({"rev-parse", "HEAD"})));
  auto *mode = browser->findChild<QPushButton *>("gitBrowserMode2");
  mode->click();
  QCoreApplication::processEvents();
  QVERIFY(!browser->isMaximized());
  auto *split = browser->findChild<QSplitter *>("gitBrowserSplitter");
  QVERIFY(split);
  split->setSizes({400, 960});
  QCoreApplication::processEvents();
  QVERIFY2(split->sizes().first() <= 420,
           qPrintable(QString("Objects pane is pinned to %1px")
                          .arg(split->sizes().first())));
  split->setSizes({960, 400});
  QCoreApplication::processEvents();
  QVERIFY2(split->sizes().last() <= 420,
           qPrintable(QString("History pane is pinned to %1px")
                          .arg(split->sizes().last())));
  auto *storage = browser->findChild<QSplitter *>("objectStorageSplitter");
  storage->setSizes({140, 780});
  QCoreApplication::processEvents();
  QVERIFY2(storage->sizes().first() <= 160,
           qPrintable(QString("Storage pane is pinned to %1px")
                          .arg(storage->sizes().first())));

  for (const auto &name :
       {"objectPackTable", "objectResultsTable", "objectRelationsTable",
        "objectExplorationTrail", "graphDetailFiles", "graphCommitList"}) {
    auto *tree = browser->findChild<QTreeWidget *>(name);
    QVERIFY(tree);
    for (int column = 0; column < tree->columnCount(); ++column) {
      QCOMPARE(tree->header()->sectionResizeMode(column),
               QHeaderView::Interactive);
      tree->header()->resizeSection(column, 40);
      QCOMPARE(tree->header()->sectionSize(column), 40);
    }
  }
  // An explicit split remains selected when moving between view modes.
  const QList<int> chosen = split->sizes();
  browser->findChild<QPushButton *>("gitBrowserMode0")->click();
  mode->click();
  QCoreApplication::processEvents();
  QVERIFY(qAbs(split->sizes().first() - chosen.first()) < 25);
}

void TestGitUiConsistency::diffIncludesFilesBeyondOldPreviewLimit() {
  QString patch = "diff --git a/large.txt b/large.txt\n--- a/large.txt\n+++ "
                  "b/large.txt\n@@ -0,0 +1,1000 @@\n";
  for (int i = 0; i < 1000; ++i)
    patch += "+" + QString(110, 'x') + "\n";
  patch += "diff --git a/later.txt b/later.txt\n--- a/later.txt\n+++ "
           "b/later.txt\n@@ -0,0 +1 @@\n+later-file-content\n";
  GitDiffDialog diff(&m_git, "HEAD", GitDiffDialog::DiffTarget::Commit, false,
                     Theme());
  diff.setDiffText(patch);
  auto *files = diff.findChild<QListWidget *>("fileList");
  QVERIFY(files);
  QCOMPARE(files->count(), 2);
  diff.selectFile("later.txt");
  QCOMPARE(files->currentRow(), 1);
}

void TestGitUiConsistency::surfaces() {
  QFETCH(QString, name);
  QFETCH(bool, light);
  const Theme theme = light ? lightTheme() : Theme();
  auto surface = create(name, theme);
  QVERIFY(surface);
  const bool panel =
      name.startsWith("source-control") || name.startsWith("conflicts");
  const QSize requested(qMax(panel ? 320 : 780, surface->minimumWidth()),
                        qMax(640, surface->minimumHeight()));
  surface->resize(requested);
  surface->show();
  QTest::qWait(name.startsWith("objects") ? 200 : 25);
  QCOMPARE(surface->width(), requested.width());
  QStringList issues;
  for (auto *button : surface->findChildren<QPushButton *>()) {
    if (!button->isVisibleTo(surface.get()) || button->isWindow())
      continue;
    if (!button->parentWidget()->rect().contains(button->geometry()))
      issues << QString("%1: %2 outside parent %3 (%4,%5 %6x%7 in %8x%9)")
                    .arg(button->objectName(), button->text(),
                         button->parentWidget()->objectName())
                    .arg(button->x())
                    .arg(button->y())
                    .arg(button->width())
                    .arg(button->height())
                    .arg(button->parentWidget()->width())
                    .arg(button->parentWidget()->height());
    if (button->text().size() > 2 &&
        button->width() < button->fontMetrics().horizontalAdvance(
                              button->text().remove('&')) +
                              12)
      issues << QString("Clipped button text: %1 (%2px)")
                    .arg(button->text())
                    .arg(button->width());

    if (button->isDefault() && name != "init" && name != "preview" &&
        name != "compare" && !button->property("gitDismiss").toBool())
      issues << QString("Unexpected default action: %1").arg(button->text());
  }
  for (auto *label : surface->findChildren<QLabel *>()) {
    if (label->isVisibleTo(surface.get()) && label->wordWrap() &&
        label->objectName() != "gitEmptyState" && !label->text().isEmpty() &&
        label->height() + 1 < label->heightForWidth(label->width()))
      issues << QString("Clipped label %1: %2 (%3px, needs %4px)")
                    .arg(label->objectName(), label->text().left(70))
                    .arg(label->height())
                    .arg(label->heightForWidth(label->width()));
  }
  for (auto *scroll : surface->findChildren<QScrollArea *>())
    if (scroll->widget() && scroll->isVisibleTo(surface.get()) &&
        scroll->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff &&
        scroll->widget()->width() > scroll->viewport()->width())
      issues << QString("Content exceeds viewport in %1: %2 > %3")
                    .arg(scroll->objectName())
                    .arg(scroll->widget()->width())
                    .arg(scroll->viewport()->width());
  if (name.startsWith("workbench")) {
    QVERIFY(
        surface->findChild<QTreeWidget *>("commitTree")->topLevelItemCount() >
        0);
    auto *scroll = surface->findChild<QScrollArea *>("inspectorScroll");
    QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
    if (name == "workbench")
      QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
  }
  if (auto *dialog = dynamic_cast<GitDialog *>(surface.get())) {
    int dismiss = 0;
    for (auto *button : dialog->findChildren<QPushButton *>())
      if (button->property("gitDismiss").toBool())
        ++dismiss;
    QCOMPARE(dismiss, 1);
    const auto headerCount =
        dialog->findChildren<QLabel *>("gitEmptyState").size();
    dialog->applyTheme(theme);
    QCOMPARE(dialog->findChildren<QLabel *>("gitEmptyState").size(),
             headerCount);
  }
  const QString captureDir =
      qEnvironmentVariable("LIGHTPAD_GIT_UI_CAPTURE_DIR");
  if (!captureDir.isEmpty()) {
    QDir().mkpath(captureDir);
    const QString prefix =
        captureDir + "/" + name + (light ? "-light" : "-dark");
    QVERIFY(surface->grab().save(prefix + ".png"));
    int tabNumber = 0;
    for (auto *tabs : surface->findChildren<QTabWidget *>()) {
      for (int i = 1; i < tabs->count(); ++i) {
        tabs->setCurrentIndex(i);
        QTest::qWait(5);
        QVERIFY(surface->grab().save(
            prefix + QString("-tab%1-%2.png").arg(tabNumber).arg(i)));
      }
      tabs->setCurrentIndex(0);
      ++tabNumber;
    }
  }
  QVERIFY2(issues.isEmpty(), qPrintable(issues.join('\n')));
}

void TestGitUiConsistency::remoteSelectionStates() {
  GitRemoteDialog fetch(&m_git, GitRemoteDialog::Mode::Fetch);
  fetch.applyTheme(Theme());
  QCOMPARE(fetch.findChildren<QComboBox *>().size(), 1);
  QCOMPARE(fetch.findChildren<QGroupBox *>().size(), 1);
  GitRemoteDialog remotes(&m_git, GitRemoteDialog::Mode::ManageRemotes);
  remotes.applyTheme(Theme());
  auto *list = remotes.findChild<QListWidget *>();
  QVERIFY(list);
  QPushButton *remove = nullptr;
  for (auto *button : remotes.findChildren<QPushButton *>())
    if (button->text() == "Remove")
      remove = button;
  QVERIFY(remove);
  QVERIFY(!remove->isEnabled());
  list->setCurrentRow(0);
  QVERIFY(remove->isEnabled());
  list->clear();
  QVERIFY(!remove->isEnabled());
}

void TestGitUiConsistency::actionWrappingPreservesInteraction() {
  GitDialog dialog;
  auto *outer = new QVBoxLayout(&dialog);
  auto *row = new QHBoxLayout;
  outer->addLayout(row);
  QList<QPushButton *> buttons;
  for (const auto &label :
       {"Inspect selected object", "Compare selected commits",
        "Copy full object hash", "Refresh repository"}) {
    auto *button = new QPushButton(label, &dialog);
    row->addWidget(button);
    buttons << button;
  }
  QSignalSpy click(buttons[1], &QPushButton::clicked);
  dialog.applyTheme(Theme());
  dialog.resize(360, 300);
  dialog.show();
  QTest::qWait(20);
  QVERIFY(buttons.last()->y() > buttons.first()->y());
  QTest::mouseClick(buttons[1], Qt::LeftButton);
  QCOMPARE(click.size(), 1);
  dialog.applyTheme(lightTheme());
  QCOMPARE(dialog.findChildren<QWidget *>("gitActionGroup").size(), 1);
  for (auto *button : buttons)
    QVERIFY(button->parentWidget()->rect().contains(button->geometry()));
}

QTEST_MAIN(TestGitUiConsistency)
#include "test_gituiconsistency.moc"
