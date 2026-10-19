#include "ui/dialogs/gitobjectexplorerdialog.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTreeWidget>
#include <QtTest/QtTest>

class TestGitObjectExplorer : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void testSearchInspectAndNavigate();
  void testPackScopeAndVerification();
  void testInvalidSearchAndClosingDuringWork();
  void cleanupTestCase() { QThreadPool::globalInstance()->waitForDone(); }

private:
  QTemporaryDir m_temp;
  QString m_repo, m_hash;
  QByteArray git(const QStringList &args, const QByteArray &input = {});
};

QByteArray TestGitObjectExplorer::git(const QStringList &args,
                                      const QByteArray &input) {
  QProcess process;
  process.setWorkingDirectory(m_repo);
  process.start("git", args);
  if (!process.waitForStarted())
    qFatal("Could not start Git");
  process.write(input);
  process.closeWriteChannel();
  if (!process.waitForFinished() || process.exitCode())
    qFatal("Git failed: %s", process.readAllStandardError().constData());
  return process.readAllStandardOutput().trimmed();
}

void TestGitObjectExplorer::initTestCase() {
  QVERIFY(m_temp.isValid());
  m_repo = m_temp.path() + "/repo";
  QVERIFY(QDir().mkpath(m_repo));
  git({"init", "--initial-branch=main"});
  git({"config", "user.name", "Explorer Test"});
  git({"config", "user.email", "explorer@example.com"});
  git({"config", "commit.gpgsign", "false"});
  QFile file(m_repo + "/test.txt");
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("object explorer data");
  file.close();
  git({"add", "."});
  git({"commit", "-m", "Explore this commit"});
  m_hash = QString::fromLatin1(git({"rev-parse", "HEAD"}));
  git({"repack", "-ad"});
}

void TestGitObjectExplorer::testSearchInspectAndNavigate() {
  GitObjectExplorerDialog dialog(m_repo, Theme());
  dialog.searchHash(m_hash);
  QSignalSpy graph(&dialog, &GitObjectExplorerDialog::viewCommitRequested);
  auto *summary = dialog.findChild<QLabel *>("objectDetailSummary");
  auto *results = dialog.findChild<QTreeWidget *>("objectResultsTable");
  auto *links = dialog.findChild<QTreeWidget *>("objectRelationsTable");
  auto *preview = dialog.findChild<QPlainTextEdit *>("objectPreview");
  auto *back = dialog.findChild<QPushButton *>("objectBackButton");
  auto *forward = dialog.findChild<QPushButton *>("objectForwardButton");
  auto *dag = dialog.findChild<QGraphicsView *>("objectDagView");
  QTRY_VERIFY(summary->text().contains(m_hash) &&
              summary->text().contains("commit"));
  QCOMPARE(results->topLevelItemCount(), 1);
  QVERIFY(preview->toPlainText().contains("Explore this commit"));
  QVERIFY(!dag->scene()->items().isEmpty());
  QCOMPARE(links->topLevelItemCount(), 1);
  const QString tree = links->topLevelItem(0)->text(2);
  dialog.findChild<QPushButton *>("objectCommitGraphButton")->click();
  QCOMPARE(graph.count(), 1);
  QCOMPARE(graph.first().first().toString(), m_hash);
  QMetaObject::invokeMethod(links, "itemActivated", Qt::DirectConnection,
                            Q_ARG(QTreeWidgetItem *, links->topLevelItem(0)),
                            Q_ARG(int, 0));
  QTRY_VERIFY(summary->text().contains(tree) &&
              summary->text().contains("tree"));
  QVERIFY(back->isEnabled());
  QVERIFY(
      !dialog.findChild<QPushButton *>("objectCommitGraphButton")->isEnabled());
  back->click();
  QTRY_VERIFY(summary->text().contains(m_hash) &&
              summary->text().contains("commit"));
  QVERIFY(forward->isEnabled());
  forward->click();
  QTRY_VERIFY(summary->text().contains(tree) &&
              summary->text().contains("tree"));
}

void TestGitObjectExplorer::testPackScopeAndVerification() {
  GitObjectExplorerDialog dialog(m_repo, Theme());
  auto *packs = dialog.findChild<QTreeWidget *>("objectPackTable");
  auto *verify = dialog.findChild<QPushButton *>("objectVerifyButton");
  auto *results = dialog.findChild<QTreeWidget *>("objectResultsTable");
  auto *output = dialog.findChild<QPlainTextEdit *>("objectVerificationOutput");
  QTRY_COMPARE(packs->topLevelItemCount(), 2);
  QVERIFY(!verify->isEnabled());
  packs->setCurrentItem(packs->topLevelItem(1));
  QVERIFY(verify->isEnabled());
  QTRY_COMPARE(results->topLevelItemCount(), 3);
  verify->click();
  QTRY_VERIFY(output->toPlainText().contains("verified successfully"));
  for (int i = 0; i < results->topLevelItemCount(); ++i)
    QVERIFY(results->topLevelItem(i)->text(3).endsWith(".pack"));
}

void TestGitObjectExplorer::testInvalidSearchAndClosingDuringWork() {
  auto *dialog = new GitObjectExplorerDialog(m_repo, Theme());
  auto *packs = dialog->findChild<QTreeWidget *>("objectPackTable");
  QTRY_COMPARE(packs->topLevelItemCount(), 2);
  dialog->findChild<QLineEdit *>("objectHashSearch")->setText("--invalid");
  QTRY_VERIFY(dialog->findChild<QLabel *>("objectSearchStatus")
                  ->text()
                  .contains("hexadecimal"));
  dialog->searchHash(m_hash);
  delete dialog;
  QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
  QCoreApplication::processEvents();
}

QTEST_MAIN(TestGitObjectExplorer)
#include "test_gitobjectexplorer.moc"
