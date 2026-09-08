#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/widgets/gitgraphwidget.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitGraphWidget : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testLoadsCommits();
  void testAllRefsIncludesOtherBranches();
  void testFirstParentFlattensMerges();
  void testPathFilterLimitsCommits();
  void testWorkingTreeNodeAppearsWhenDirty();
  void testWorkingTreeNodeSelectableFromKeyboard();
  void testKeyboardMovesSelection();
  void testFilterMatchesBranchName();
  void testFilterMatchesAuthorAndHash();
  void testCompareAnchorFlow();
  void testCompareAnchorTogglesOff();
  void testZoomIsBounded();
  void testWorktreeAnchors();
  void testStashAnchors();
  void testCreateAndDeleteTag();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  GitGraphWidget *makeGraph(bool allRefs = true, bool firstParent = false,
                            const QString &path = QString());
};

void TestGitGraphWidget::init() {
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

void TestGitGraphWidget::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestGitGraphWidget::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestGitGraphWidget::writeFile(const QString &name,
                                   const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

GitGraphWidget *TestGitGraphWidget::makeGraph(bool allRefs, bool firstParent,
                                              const QString &path) {
  GitGraphWidget *graph = new GitGraphWidget(m_git, Theme());
  graph->resize(800, 400);
  GitLogOptions options;
  options.allRefs = allRefs;
  options.firstParentOnly = firstParent;
  options.pathFilter = path;
  graph->setLogOptions(options);
  return graph;
}

void TestGitGraphWidget::testLoadsCommits() {
  QScopedPointer<GitGraphWidget> graph(makeGraph());
  QCOMPARE(graph->loadedCommitCount(), 2);
}

void TestGitGraphWidget::testAllRefsIncludesOtherBranches() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("c.txt", "c1\n");
  QVERIFY(git({"add", "c.txt"}));
  QVERIFY(git({"commit", "-m", "Add c on side"}));
  QVERIFY(git({"checkout", "main"}));

  QScopedPointer<GitGraphWidget> focused(makeGraph(false));
  QCOMPARE(focused->loadedCommitCount(), 2);

  QScopedPointer<GitGraphWidget> all(makeGraph(true));
  QCOMPARE(all->loadedCommitCount(), 3);
}

void TestGitGraphWidget::testFirstParentFlattensMerges() {
  QVERIFY(git({"checkout", "-b", "side"}));
  writeFile("c.txt", "c1\n");
  QVERIFY(git({"add", "c.txt"}));
  QVERIFY(git({"commit", "-m", "Add c"}));
  QVERIFY(git({"checkout", "main"}));
  QVERIFY(git({"merge", "--no-ff", "side", "-m", "Merge side"}));

  QScopedPointer<GitGraphWidget> full(makeGraph(false, false));
  QScopedPointer<GitGraphWidget> flat(makeGraph(false, true));

  QVERIFY2(flat->loadedCommitCount() < full->loadedCommitCount(),
           qPrintable(QString("full=%1 flat=%2")
                          .arg(full->loadedCommitCount())
                          .arg(flat->loadedCommitCount())));
}

void TestGitGraphWidget::testPathFilterLimitsCommits() {
  QScopedPointer<GitGraphWidget> graph(makeGraph(false, false, "a.txt"));
  QCOMPARE(graph->loadedCommitCount(), 1);
}

void TestGitGraphWidget::testWorkingTreeNodeAppearsWhenDirty() {
  QScopedPointer<GitGraphWidget> graph(makeGraph());
  QVERIFY(!graph->hasWorkingTreeNode());

  writeFile("a.txt", "changed\n");
  graph->setWorkingTreeState(m_git->repositoryState());
  QVERIFY(graph->hasWorkingTreeNode());

  QVERIFY(git({"checkout", "--", "a.txt"}));
  graph->setWorkingTreeState(m_git->repositoryState());
  QVERIFY(!graph->hasWorkingTreeNode());
}

void TestGitGraphWidget::testWorkingTreeNodeSelectableFromKeyboard() {
  writeFile("a.txt", "changed\n");
  QScopedPointer<GitGraphWidget> graph(makeGraph());
  graph->setWorkingTreeState(m_git->repositoryState());
  graph->show();

  QSignalSpy wipSpy(graph.data(), &GitGraphWidget::workingTreeSelected);
  QSignalSpy commitSpy(graph.data(), &GitGraphWidget::commitSelected);

  QTest::keyClick(graph.data(), Qt::Key_Down);
  QCOMPARE(wipSpy.count(), 1);

  QTest::keyClick(graph.data(), Qt::Key_Down);
  QCOMPARE(commitSpy.count(), 1);

  QTest::keyClick(graph.data(), Qt::Key_Up);
  QCOMPARE(wipSpy.count(), 2);
}

void TestGitGraphWidget::testKeyboardMovesSelection() {
  QScopedPointer<GitGraphWidget> graph(makeGraph());
  graph->show();

  QTest::keyClick(graph.data(), Qt::Key_Down);
  const QString first = graph->selectedHash();
  QVERIFY(!first.isEmpty());

  QTest::keyClick(graph.data(), Qt::Key_Down);
  const QString second = graph->selectedHash();
  QVERIFY(!second.isEmpty());
  QVERIFY(first != second);

  QTest::keyClick(graph.data(), Qt::Key_Up);
  QCOMPARE(graph->selectedHash(), first);
}

void TestGitGraphWidget::testFilterMatchesBranchName() {
  QVERIFY(git({"branch", "feature/login"}));

  QScopedPointer<GitGraphWidget> graph(makeGraph());
  QCOMPARE(graph->matchingCommitCount(), 2);

  graph->setFilter("feature/login");
  QCOMPARE(graph->matchingCommitCount(), 1);

  graph->setFilter("no-such-ref");
  QCOMPARE(graph->matchingCommitCount(), 0);

  graph->setFilter(QString());
  QCOMPARE(graph->matchingCommitCount(), 2);
}

void TestGitGraphWidget::testFilterMatchesAuthorAndHash() {
  QScopedPointer<GitGraphWidget> graph(makeGraph());

  graph->setFilter("Ada");
  QCOMPARE(graph->matchingCommitCount(), 2);

  graph->setFilter("dev@example.com");
  QCOMPARE(graph->matchingCommitCount(), 2);

  graph->setFilter("Add a");
  QCOMPARE(graph->matchingCommitCount(), 1);
}

void TestGitGraphWidget::testCompareAnchorFlow() {
  QScopedPointer<GitGraphWidget> graph(makeGraph());
  graph->show();

  QSignalSpy compareSpy(graph.data(), &GitGraphWidget::compareRequested);
  QSignalSpy anchorSpy(graph.data(), &GitGraphWidget::compareAnchorChanged);

  QTest::keyClick(graph.data(), Qt::Key_Down);
  const QString first = graph->selectedHash();
  QTest::keyClick(graph.data(), Qt::Key_Space);
  QCOMPARE(graph->compareAnchor(), first);
  QCOMPARE(anchorSpy.count(), 1);

  QTest::keyClick(graph.data(), Qt::Key_Down);
  const QString second = graph->selectedHash();
  QTest::keyClick(graph.data(), Qt::Key_Space);

  QCOMPARE(compareSpy.count(), 1);
  const QList<QVariant> args = compareSpy.takeFirst();
  QCOMPARE(args.at(0).toString(), first);
  QCOMPARE(args.at(1).toString(), second);
  QVERIFY(graph->compareAnchor().isEmpty());
}

void TestGitGraphWidget::testCompareAnchorTogglesOff() {
  QScopedPointer<GitGraphWidget> graph(makeGraph());
  graph->show();

  QTest::keyClick(graph.data(), Qt::Key_Down);
  QTest::keyClick(graph.data(), Qt::Key_Space);
  QVERIFY(!graph->compareAnchor().isEmpty());

  QTest::keyClick(graph.data(), Qt::Key_Space);
  QVERIFY(graph->compareAnchor().isEmpty());
}

void TestGitGraphWidget::testZoomIsBounded() {
  QScopedPointer<GitGraphWidget> graph(makeGraph());
  const int initial = graph->rowHeight();

  graph->zoomIn();
  QVERIFY(graph->rowHeight() > initial);

  for (int i = 0; i < 40; ++i) {
    graph->zoomIn();
  }
  const int maxHeight = graph->rowHeight();
  graph->zoomIn();
  QCOMPARE(graph->rowHeight(), maxHeight);

  for (int i = 0; i < 40; ++i) {
    graph->zoomOut();
  }
  const int minHeight = graph->rowHeight();
  graph->zoomOut();
  QCOMPARE(graph->rowHeight(), minHeight);
  QVERIFY(minHeight < maxHeight);

  graph->resetZoom();
  QCOMPARE(graph->rowHeight(), initial);
}

void TestGitGraphWidget::testWorktreeAnchors() {
  QVERIFY(m_git->getWorktreeAnchors().isEmpty());

  const QString worktreePath =
      m_tempDir.path() + "/wt_" + QDir(m_repoPath).dirName();
  QVERIFY(git({"worktree", "add", "-b", "wt-branch", worktreePath}));

  const QMap<QString, QStringList> anchors = m_git->getWorktreeAnchors();
  QCOMPARE(anchors.size(), 1);
  QCOMPARE(anchors.first().size(), 1);
  QVERIFY(anchors.first().first().startsWith("wt_"));
}

void TestGitGraphWidget::testStashAnchors() {
  QVERIFY(m_git->getStashAnchors().isEmpty());

  writeFile("a.txt", "stash me\n");
  QVERIFY(git({"stash", "push", "-m", "wip"}));

  const QMap<QString, QStringList> anchors = m_git->getStashAnchors();
  QCOMPARE(anchors.size(), 1);
  QCOMPARE(anchors.first().first(), QString("stash@{0}"));

  const QString head = m_git->getCommitDetails(QStringLiteral("HEAD")).hash;
  QVERIFY(anchors.contains(head));
}

void TestGitGraphWidget::testCreateAndDeleteTag() {
  QVERIFY(m_git->createTag("v1.0"));
  QStringList names;
  for (const GitTagInfo &tag : m_git->getTags()) {
    names << tag.name;
  }
  QVERIFY2(names.contains("v1.0"), qPrintable(names.join(",")));

  QVERIFY(m_git->deleteTag("v1.0"));
  names.clear();
  for (const GitTagInfo &tag : m_git->getTags()) {
    names << tag.name;
  }
  QVERIFY(!names.contains("v1.0"));
}

QTEST_MAIN(TestGitGraphWidget)
#include "test_gitgraphwidget.moc"
