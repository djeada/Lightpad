#include <QDir>
#include <QFile>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/panels/conflictcenterpanel.h"

class TestConflictCenterPanel : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testStaysQuietWhenNothingIsWrong();
  void testShowsTheBannerWhenAMergeHalts();
  void testListsEveryConflictedFile();
  void testCountsTheSpotsInsideEachFile();
  void testNamesWhichBranchIsWhich();
  void testExplainsWhyEachFileClashed();
  void testFinishIsBlockedUntilEveryFileIsSettled();
  void testProgressFollowsResolvedFiles();
  void testActivatingAFileAsksForItToBeOpened();
  void testAttentionIsRaisedOnce();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;
  Theme m_theme;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  void makeConflicts();
  static QString twoSpots(const QString &first, const QString &second);
};

bool TestConflictCenterPanel::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start(QStringLiteral("git"), args);
  return process.waitForFinished(15000) && process.exitCode() == 0;
}

void TestConflictCenterPanel::writeFile(const QString &name,
                                        const QString &content) {
  QFile file(QDir(m_repoPath).filePath(name));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(content.toUtf8());
  file.close();
}

QString TestConflictCenterPanel::twoSpots(const QString &first,
                                          const QString &second) {
  QStringList lines;
  lines << first;
  for (int i = 0; i < 10; ++i) {
    lines << QStringLiteral("gap %1").arg(i);
  }
  lines << second;
  return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void TestConflictCenterPanel::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "ada@example.com"}));
  QVERIFY(git({"config", "user.name", "Ada"}));
  writeFile("a.txt", twoSpots(QStringLiteral("one"), QStringLiteral("three")));
  writeFile("b.txt", "alpha\nbeta\n");
  QVERIFY(git({"add", "."}));
  QVERIFY(git({"commit", "-m", "base"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestConflictCenterPanel::cleanup() {
  delete m_git;
  m_git = nullptr;
}

void TestConflictCenterPanel::makeConflicts() {
  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("a.txt",
            twoSpots(QStringLiteral("THEIRS1"), QStringLiteral("THEIRS2")));
  writeFile("b.txt", "THEIRS\nbeta\n");
  QVERIFY(git({"commit", "-am", "feature edits"}));

  QVERIFY(git({"checkout", "main"}));
  writeFile("a.txt",
            twoSpots(QStringLiteral("MINE1"), QStringLiteral("MINE2")));
  writeFile("b.txt", "MINE\nbeta\n");
  QVERIFY(git({"commit", "-am", "main edits"}));

  git({"merge", "feature"});
  m_git->refresh();
  QVERIFY(m_git->hasMergeConflicts());
}

void TestConflictCenterPanel::testStaysQuietWhenNothingIsWrong() {
  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QVERIFY(!panel.hasConflicts());
  QCOMPARE(panel.conflictedFileCount(), 0);

  QWidget *banner =
      panel.findChild<QWidget *>(QStringLiteral("conflictCenterBanner"));
  QLabel *empty =
      panel.findChild<QLabel *>(QStringLiteral("conflictCenterEmptyLabel"));
  QVERIFY(banner);
  QVERIFY(empty);
  QVERIFY(banner->isHidden());
  QVERIFY(!empty->isHidden());
}

void TestConflictCenterPanel::testShowsTheBannerWhenAMergeHalts() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QVERIFY(panel.hasConflicts());

  QWidget *banner =
      panel.findChild<QWidget *>(QStringLiteral("conflictCenterBanner"));
  QVERIFY(banner);
  QVERIFY(!banner->isHidden());

  QLabel *title =
      panel.findChild<QLabel *>(QStringLiteral("conflictCenterTitle"));
  QVERIFY(title);
  QVERIFY(title->text().contains(QStringLiteral("paused")));
}

void TestConflictCenterPanel::testListsEveryConflictedFile() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QTreeWidget *tree =
      panel.findChild<QTreeWidget *>(QStringLiteral("conflictCenterFileTree"));
  QVERIFY(tree);
  QCOMPARE(tree->topLevelItemCount(), 2);
  QCOMPARE(panel.conflictedFileCount(), 2);

  QStringList paths;
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    paths << tree->topLevelItem(i)->text(0);
  }
  QVERIFY(paths.contains(QStringLiteral("a.txt")));
  QVERIFY(paths.contains(QStringLiteral("b.txt")));
}

void TestConflictCenterPanel::testCountsTheSpotsInsideEachFile() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QTreeWidget *tree =
      panel.findChild<QTreeWidget *>(QStringLiteral("conflictCenterFileTree"));
  QVERIFY(tree);

  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = tree->topLevelItem(i);
    if (item->text(0) == QStringLiteral("a.txt")) {

      QCOMPARE(item->text(1), QStringLiteral("2 spots"));
    } else {
      QCOMPARE(item->text(1), QStringLiteral("1 spot"));
    }
  }
}

void TestConflictCenterPanel::testNamesWhichBranchIsWhich() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QLabel *ours =
      panel.findChild<QLabel *>(QStringLiteral("conflictCenterOursHint"));
  QLabel *theirs =
      panel.findChild<QLabel *>(QStringLiteral("conflictCenterTheirsHint"));
  QLabel *operation =
      panel.findChild<QLabel *>(QStringLiteral("conflictCenterOperation"));
  QVERIFY(ours);
  QVERIFY(theirs);
  QVERIFY(operation);

  QVERIFY(ours->text().contains(QStringLiteral("Yours")));
  QVERIFY(theirs->text().contains(QStringLiteral("Theirs")));
  QVERIFY(operation->text().contains(QStringLiteral("merge")));
  QVERIFY(operation->text().contains(QStringLiteral("main")));
}

void TestConflictCenterPanel::testExplainsWhyEachFileClashed() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QTreeWidget *tree =
      panel.findChild<QTreeWidget *>(QStringLiteral("conflictCenterFileTree"));
  QVERIFY(tree);
  QTreeWidgetItem *item = tree->topLevelItem(0);
  QVERIFY(item);
  QVERIFY(!item->text(2).isEmpty());
  QVERIFY(!item->toolTip(0).isEmpty());
}

void TestConflictCenterPanel::testFinishIsBlockedUntilEveryFileIsSettled() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QPushButton *finish = panel.findChild<QPushButton *>(
      QStringLiteral("conflictCenterFinishButton"));
  QVERIFY(finish);
  QVERIFY(!finish->isEnabled());

  QVERIFY(m_git->resolveConflictOurs(QStringLiteral("a.txt")));
  panel.refresh();
  QVERIFY(!finish->isEnabled());

  QVERIFY(m_git->resolveConflictOurs(QStringLiteral("b.txt")));
  panel.refresh();
  QVERIFY(finish->isEnabled());
}

void TestConflictCenterPanel::testProgressFollowsResolvedFiles() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QProgressBar *progress =
      panel.findChild<QProgressBar *>(QStringLiteral("conflictCenterProgress"));
  QVERIFY(progress);
  QCOMPARE(progress->maximum(), 2);
  QCOMPARE(progress->value(), 0);

  QVERIFY(m_git->resolveConflictTheirs(QStringLiteral("b.txt")));
  panel.refresh();
  QCOMPARE(progress->value(), 1);
  QCOMPARE(panel.unresolvedFileCount(), 1);
}

void TestConflictCenterPanel::testActivatingAFileAsksForItToBeOpened() {
  makeConflicts();

  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QSignalSpy spy(&panel, &ConflictCenterPanel::fileOpenRequested);
  QTreeWidget *tree =
      panel.findChild<QTreeWidget *>(QStringLiteral("conflictCenterFileTree"));
  QVERIFY(tree);
  emit tree->itemDoubleClicked(tree->topLevelItem(0), 0);

  QCOMPARE(spy.count(), 1);
  const QString path = spy.first().first().toString();
  QVERIFY(path.startsWith(m_git->repositoryPath()));
  QVERIFY(path.endsWith(QStringLiteral(".txt")));
}

void TestConflictCenterPanel::testAttentionIsRaisedOnce() {
  ConflictCenterPanel panel;
  panel.applyTheme(m_theme);
  panel.setGitIntegration(m_git);

  QSignalSpy spy(&panel, &ConflictCenterPanel::attentionNeeded);
  makeConflicts();
  panel.refresh();
  QCOMPARE(spy.count(), 1);

  panel.refresh();
  QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestConflictCenterPanel)
#include "test_conflictcenterpanel.moc"
