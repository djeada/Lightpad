#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/dialogs/branchsyncradardialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest/QtTest>

class TestBranchSyncRadarDialog : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testInSyncShowsEmptyLanes();
  void testOutgoingLanePopulated();
  void testIncomingLanePopulated();
  void testDivergenceIsCalledOut();
  void testStrategyFollowsRepositoryConfig();
  void testStrategyPreviewChangesWithSelection();
  void testPushModeChangesPreviewAndButton();
  void testPullDisabledWhenNothingIncoming();
  void testNoUpstreamDisablesStrategies();
  void testFetchDoesNotIntegrate();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  QString m_remotePath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args, const QString &path = QString());
  void writeFile(const QString &name, const QString &content);
  QString pushRemoteCommit(const QString &subject);
  QString labelText(BranchSyncRadarDialog &dialog, const char *name);
};

void TestBranchSyncRadarDialog::init() {
  static int counter = 0;
  const QString root = m_tempDir.path() + "/case" + QString::number(counter++);
  m_repoPath = root + "/work";
  m_remotePath = root + "/remote.git";
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(QDir().mkpath(m_remotePath));

  QVERIFY(git({"init", "--bare", "--initial-branch=main"}, m_remotePath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));
  writeFile("f.txt", "one\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));
  QVERIFY(git({"remote", "add", "origin", m_remotePath}));
  QVERIFY(git({"push", "-u", "origin", "main"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestBranchSyncRadarDialog::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestBranchSyncRadarDialog::git(const QStringList &args,
                                    const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repoPath : path);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

void TestBranchSyncRadarDialog::writeFile(const QString &name,
                                          const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

QString TestBranchSyncRadarDialog::pushRemoteCommit(const QString &subject) {
  static int counter = 0;
  const QString other = m_tempDir.path() + "/peer" + QString::number(counter++);
  if (!git({"clone", m_remotePath, other}, m_tempDir.path())) {
    return QString();
  }
  git({"config", "user.email", "peer@example.com"}, other);
  git({"config", "user.name", "Peer"}, other);
  QFile file(other + "/f.txt");
  file.open(QIODevice::WriteOnly);
  file.write(subject.toUtf8() + "\n");
  file.close();
  git({"commit", "-am", subject}, other);
  git({"push"}, other);
  return other;
}

QString TestBranchSyncRadarDialog::labelText(BranchSyncRadarDialog &dialog,
                                             const char *name) {
  QLabel *label = dialog.findChild<QLabel *>(QString::fromLatin1(name));
  return label ? label->text() : QString();
}

void TestBranchSyncRadarDialog::testInSyncShowsEmptyLanes() {
  BranchSyncRadarDialog dialog(m_git, Theme());

  QCOMPARE(
      dialog.findChild<QTreeWidget *>("radarIncomingTree")->topLevelItemCount(),
      0);
  QCOMPARE(
      dialog.findChild<QTreeWidget *>("radarOutgoingTree")->topLevelItemCount(),
      0);
  QVERIFY(labelText(dialog, "radarDivergenceLabel").contains("One lane"));
  QVERIFY(labelText(dialog, "radarSummaryLabel").contains("same commit"));
}

void TestBranchSyncRadarDialog::testOutgoingLanePopulated() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "local work"}));

  BranchSyncRadarDialog dialog(m_git, Theme());
  QTreeWidget *outgoing = dialog.findChild<QTreeWidget *>("radarOutgoingTree");
  QCOMPARE(outgoing->topLevelItemCount(), 1);
  QCOMPARE(outgoing->topLevelItem(0)->text(1), QString("local work"));
  QVERIFY(labelText(dialog, "radarOutgoingLabel").contains("Outgoing"));
}

void TestBranchSyncRadarDialog::testIncomingLanePopulated() {
  QVERIFY(!pushRemoteCommit("remote work").isEmpty());
  QVERIFY(git({"fetch"}));

  BranchSyncRadarDialog dialog(m_git, Theme());
  QTreeWidget *incoming = dialog.findChild<QTreeWidget *>("radarIncomingTree");
  QCOMPARE(incoming->topLevelItemCount(), 1);
  QCOMPARE(incoming->topLevelItem(0)->text(1), QString("remote work"));
  QVERIFY(dialog.findChild<QPushButton *>("radarPullButton")->isEnabled());
}

void TestBranchSyncRadarDialog::testDivergenceIsCalledOut() {
  QVERIFY(!pushRemoteCommit("remote work").isEmpty());
  writeFile("g.txt", "local\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "local work"}));
  QVERIFY(git({"fetch"}));

  BranchSyncRadarDialog dialog(m_git, Theme());
  QVERIFY(dialog.syncState().diverged());

  const QString divergence = labelText(dialog, "radarDivergenceLabel");
  QVERIFY2(divergence.contains("separated"), qPrintable(divergence));

  const QString push = labelText(dialog, "radarPushPreview");
  QVERIFY2(push.contains("Will be rejected"), qPrintable(push));
}

void TestBranchSyncRadarDialog::testStrategyFollowsRepositoryConfig() {
  QVERIFY(git({"config", "pull.rebase", "true"}));

  BranchSyncRadarDialog dialog(m_git, Theme());
  QVERIFY(dialog.findChild<QRadioButton *>("radarRebaseRadio")->isChecked());
  QCOMPARE(dialog.selectedStrategy(), GitPullStrategy::Rebase);
  QVERIFY2(labelText(dialog, "radarConfiguredLabel").contains("rebase"),
           qPrintable(labelText(dialog, "radarConfiguredLabel")));
}

void TestBranchSyncRadarDialog::testStrategyPreviewChangesWithSelection() {
  QVERIFY(!pushRemoteCommit("remote work").isEmpty());
  writeFile("g.txt", "local\n");
  QVERIFY(git({"add", "g.txt"}));
  QVERIFY(git({"commit", "-m", "local work"}));
  QVERIFY(git({"fetch"}));

  BranchSyncRadarDialog dialog(m_git, Theme());
  dialog.findChild<QRadioButton *>("radarMergeRadio")->setChecked(true);
  const QString merge = labelText(dialog, "radarStrategyPreview");

  dialog.findChild<QRadioButton *>("radarRebaseRadio")->setChecked(true);
  const QString rebase = labelText(dialog, "radarStrategyPreview");

  dialog.findChild<QRadioButton *>("radarFfRadio")->setChecked(true);
  const QString ff = labelText(dialog, "radarStrategyPreview");

  QVERIFY2(merge.contains("merge commit"), qPrintable(merge));
  QVERIFY2(rebase.contains("new hashes"), qPrintable(rebase));
  QVERIFY2(ff.contains("Refuses to run"), qPrintable(ff));
}

void TestBranchSyncRadarDialog::testPushModeChangesPreviewAndButton() {
  writeFile("f.txt", "two\n");
  QVERIFY(git({"commit", "-am", "local work"}));

  BranchSyncRadarDialog dialog(m_git, Theme());
  QComboBox *mode = dialog.findChild<QComboBox *>("radarPushModeCombo");
  QPushButton *push = dialog.findChild<QPushButton *>("radarPushButton");
  QVERIFY(mode && push);

  QCOMPARE(push->text(), QString("Push"));

  mode->setCurrentIndex(1);
  QCOMPARE(dialog.selectedPushForce(), GitPushForce::WithLease);
  QVERIFY2(push->text().contains("lease"), qPrintable(push->text()));
  const QString lease = labelText(dialog, "radarPushPreview");

  mode->setCurrentIndex(2);
  QCOMPARE(dialog.selectedPushForce(), GitPushForce::Force);
  QVERIFY2(push->text().contains("Force"), qPrintable(push->text()));
  const QString force = labelText(dialog, "radarPushPreview");

  QVERIFY(lease != force);
  QVERIFY2(lease.contains("refused instead of discarding"), qPrintable(lease));
  QVERIFY2(force.contains("unconditionally"), qPrintable(force));
}

void TestBranchSyncRadarDialog::testPullDisabledWhenNothingIncoming() {
  BranchSyncRadarDialog dialog(m_git, Theme());
  QVERIFY(!dialog.findChild<QPushButton *>("radarPullButton")->isEnabled());
}

void TestBranchSyncRadarDialog::testNoUpstreamDisablesStrategies() {
  QVERIFY(git({"checkout", "-b", "lonely"}));

  BranchSyncRadarDialog dialog(m_git, Theme());
  QVERIFY(!dialog.syncState().hasUpstream);
  QVERIFY(!dialog.findChild<QRadioButton *>("radarMergeRadio")->isEnabled());
  QVERIFY(!dialog.findChild<QPushButton *>("radarPullButton")->isEnabled());
  QVERIFY(dialog.findChild<QPushButton *>("radarUpstreamButton")->isEnabled());
  QVERIFY2(labelText(dialog, "radarSummaryLabel").contains("no upstream"),
           qPrintable(labelText(dialog, "radarSummaryLabel")));
}

void TestBranchSyncRadarDialog::testFetchDoesNotIntegrate() {
  QVERIFY(!pushRemoteCommit("remote work").isEmpty());

  BranchSyncRadarDialog dialog(m_git, Theme());
  QCOMPARE(dialog.syncState().behind(), 0);

  dialog.findChild<QPushButton *>("radarFetchButton")->click();

  QCOMPARE(dialog.syncState().behind(), 1);
  QFile file(m_repoPath + "/f.txt");
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(file.readAll()), QString("one\n"));
}

QTEST_MAIN(TestBranchSyncRadarDialog)
#include "test_branchsyncradardialog.moc"
