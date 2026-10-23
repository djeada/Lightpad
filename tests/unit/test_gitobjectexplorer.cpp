#include "ui/dialogs/gitobjectexplorerdialog.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTreeWidget>
#include <QtTest/QtTest>

class GeometryObserver : public QObject {
public:
  QStringList changes;

protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Move)
      changes << object->objectName();
    return false;
  }
};

class TestGitObjectExplorer : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void testSearchInspectAndNavigate();
  void testPackScopeAndVerification();
  void testInvalidSearchAndClosingDuringWork();
  void testPackSwitchingAndResizing();
  void testDecodedPackEntriesAndLooseFiles();
  void testDagRemainsReadableAndNavigable();
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
  QTRY_COMPARE(packs->topLevelItemCount(), 3);
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
  QTRY_COMPARE(packs->topLevelItemCount(), 3);
  dialog->findChild<QLineEdit *>("objectHashSearch")->setText("--invalid");
  QTRY_VERIFY(dialog->findChild<QLabel *>("objectSearchStatus")
                  ->text()
                  .contains("hexadecimal"));
  dialog->searchHash(m_hash);
  delete dialog;
  QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
  QCoreApplication::processEvents();
}

void TestGitObjectExplorer::testPackSwitchingAndResizing() {
  // A second pack containing the tree/blob but not the selected commit makes
  // stale inspector contents and storage scope mistakes visible.
  const QByteArray tree = git({"rev-parse", "HEAD^{tree}"});
  const QByteArray blob = git({"rev-parse", "HEAD:test.txt"});
  const QString packPrefix = m_repo + "/.git/objects/pack/pack";
  const QString packHash = QString::fromLatin1(
      git({"pack-objects", packPrefix}, tree + '\n' + blob + '\n'));
  GitObjectExplorerDialog dialog(m_repo, Theme());
  dialog.resize(1400, 900);
  dialog.show();
  dialog.searchHash(m_hash);
  auto *packs = dialog.findChild<QTreeWidget *>("objectPackTable");
  auto *results = dialog.findChild<QTreeWidget *>("objectResultsTable");
  auto *storage = dialog.findChild<QSplitter *>("objectStorageSplitter");
  auto *packTrail = dialog.findChild<QSplitter *>("objectPackTrailSplitter");
  auto *inspector =
      dialog.findChild<QSplitter *>("objectResultsInspectorSplitter");
  auto *status = dialog.findChild<QLabel *>("objectSearchStatus");
  QTRY_COMPARE(packs->topLevelItemCount(), 4);
  QTRY_COMPARE(dialog.currentObjectHash(), m_hash);
  QTreeWidgetItem *second = nullptr, *original = nullptr;
  for (int i = 1; i < packs->topLevelItemCount(); ++i) {
    auto *row = packs->topLevelItem(i);
    const QString path = row->data(0, Qt::UserRole).toString();
    if (path.isEmpty())
      continue;
    QCOMPARE(row->text(0), QFileInfo(path).fileName());
    if (path.contains(packHash))
      second = row;
    else
      original = row;
  }
  QVERIFY(second && original);
  packs->setCurrentItem(second);
  QTRY_COMPARE(results->topLevelItemCount(), 2);
  QVERIFY(dialog.currentObjectHash().isEmpty());
  QVERIFY(dialog.findChild<QPlainTextEdit *>("objectPreview")
              ->toPlainText()
              .isEmpty());
  for (int i = 0; i < results->topLevelItemCount(); ++i) {
    auto *row = results->topLevelItem(i);
    QCOMPARE(row->text(3), second->text(0));
    QVERIFY(row->data(0, Qt::UserRole).toString() != m_hash);
  }
  // Once an ordinary browse layout is settled, switching packs must not move
  // its dividers or change the footer height during the asynchronous search.
  QTest::qWait(100);
  const auto storageSizes = storage->sizes();
  const auto inspectorSizes = inspector->sizes();
  const QRect windowGeometry = dialog.geometry();
  const int statusHeight = status->height();
  GeometryObserver geometry;
  for (auto *widget :
       {static_cast<QWidget *>(&dialog), static_cast<QWidget *>(packs),
        inspector->widget(0), inspector->widget(1)})
    widget->installEventFilter(&geometry);
  for (int i = 0; i < 6; ++i) {
    auto *row = i % 2 ? second : original;
    QTest::mouseClick(packs->viewport(), Qt::LeftButton, Qt::NoModifier,
                      packs->visualItemRect(row).center());
    QTRY_COMPARE(results->topLevelItemCount(), i % 2 ? 2 : 3);
    QTest::qWait(40);
    QCOMPARE(dialog.geometry(), windowGeometry);
    QCOMPARE(storage->sizes(), storageSizes);
    QCOMPARE(inspector->sizes(), inspectorSizes);
    QCOMPARE(status->height(), statusHeight);
  }
  QVERIFY2(geometry.changes.isEmpty(), qPrintable(geometry.changes.join(", ")));
  const int nameWidth = packs->columnWidth(0);
  const int metadataWidth = packs->columnWidth(1);
  storage->setSizes({900, 450});
  QTRY_VERIFY(packs->columnWidth(0) > nameWidth + 200);
  QCOMPARE(packs->columnWidth(1), metadataWidth);
  QVERIFY(packs->columnWidth(0) >
          packs->fontMetrics().horizontalAdvance(second->text(0)));
  packs->header()->resizeSection(0, 180);
  QCOMPARE(packs->columnWidth(0), 180);
  const int packHeight = packs->height();
  packTrail->setSizes({550, 150});
  QTRY_VERIFY(packs->height() > packHeight + 100);
  QVERIFY(QFile::remove(packPrefix + '-' + packHash + ".pack"));
  QVERIFY(QFile::remove(packPrefix + '-' + packHash + ".idx"));
}

void TestGitObjectExplorer::testDecodedPackEntriesAndLooseFiles() {
  const QString looseHash = QString::fromLatin1(
      git({"hash-object", "-w", "--stdin"}, "A physical loose file\n"));
  GitObjectExplorerDialog dialog(m_repo, Theme());
  dialog.resize(1400, 900);
  dialog.show();
  auto *packs = dialog.findChild<QTreeWidget *>("objectPackTable");
  auto *results = dialog.findChild<QTreeWidget *>("objectResultsTable");
  auto *tabs = dialog.findChild<QTabWidget *>("objectDetailTabs");
  auto *diskPage = dialog.findChild<QWidget *>("objectDiskPage");
  auto *decoded = dialog.findChild<QPlainTextEdit *>("objectDiskDecoded");
  auto *path = dialog.findChild<QLineEdit *>("objectDiskPath");
  auto *diskSummary = dialog.findChild<QLabel *>("objectDiskSummary");
  QTRY_COMPARE(packs->topLevelItemCount(), 3);
  QTest::mouseClick(packs->viewport(), Qt::LeftButton, Qt::NoModifier,
                    packs->visualItemRect(packs->topLevelItem(1)).center());
  QTRY_COMPARE(results->topLevelItemCount(), 3);
  QCOMPARE(tabs->currentWidget(), diskPage);
  QVERIFY(decoded->toPlainText().contains("PACKFILE HEADER"));
  QTreeWidgetItem *commit = nullptr;
  for (int i = 0; i < results->topLevelItemCount(); ++i)
    if (results->topLevelItem(i)->data(0, Qt::UserRole).toString() == m_hash)
      commit = results->topLevelItem(i);
  QVERIFY(commit);
  QTest::mouseClick(results->viewport(), Qt::LeftButton, Qt::NoModifier,
                    results->visualItemRect(commit).center());
  QTRY_VERIFY(decoded->toPlainText().contains("Explore this commit"));
  QVERIFY(path->text().endsWith(".pack"));
  QVERIFY(diskSummary->text().contains("object hash matches"));
  dialog.findChild<QPushButton *>("objectDiskCopyPath")->click();
  QCOMPARE(QApplication::clipboard()->text(), path->text());
  auto *raw = dialog.findChild<QPlainTextEdit *>("objectDiskBytes");
  QVERIFY(raw->toPlainText().contains("file offsets are hexadecimal"));
  auto *loose = packs->topLevelItem(packs->topLevelItemCount() - 1);
  QTest::mouseClick(packs->viewport(), Qt::LeftButton, Qt::NoModifier,
                    packs->visualItemRect(loose).center());
  QTRY_COMPARE(results->topLevelItemCount(), 1);
  QTRY_VERIFY(decoded->toPlainText().contains("A physical loose file"));
  QCOMPARE(tabs->currentWidget(), diskPage);
  QVERIFY(!dialog.findChild<QPushButton *>("objectVerifyButton")->isEnabled());
  QCOMPARE(results->topLevelItem(0)->data(0, Qt::UserRole).toString(),
           looseHash);
  QCOMPARE(path->text(), m_repo + "/.git/objects/" + looseHash.left(2) + '/' +
                             looseHash.mid(2));
  QVERIFY(results->topLevelItem(0)->text(3).contains('/'));
  QVERIFY(diskSummary->text().contains("object hash matches"));
  const auto details =
      dialog.findChild<QPlainTextEdit *>("objectDiskDetails")->toPlainText();
  QVERIFY(details.contains("Inflated header: blob 22\\0"));
  dialog.focusDag();
  QCOMPARE(tabs->currentIndex(), 2);
}

void TestGitObjectExplorer::testDagRemainsReadableAndNavigable() {
  // A large tree with identical contents must retain every filename, and its
  // initial overview must scroll rather than shrink text into an unreadable
  // dot.
  const QString blob = QString::fromLatin1(git({"rev-parse", "HEAD:test.txt"}));
  QByteArray entries;
  for (int i = 0; i < 30; ++i)
    entries += "100644 blob " + blob.toLatin1() + "\tfile-" +
               QByteArray::number(i).rightJustified(2, '0') + "\n";
  const QString tree = QString::fromLatin1(git({"mktree"}, entries));
  GitObjectExplorerDialog dialog(m_repo, Theme());
  dialog.resize(1220, 800);
  dialog.show();
  auto *dag = dialog.findChild<QGraphicsView *>("objectDagView");
  auto *tabs = dialog.findChild<QTabWidget *>("objectDetailTabs");
  QVERIFY(tabs);
  tabs->setCurrentIndex(2);
  dialog.searchHash(tree);
  auto *summary = dialog.findChild<QLabel *>("objectDetailSummary");
  QTRY_VERIFY(summary->text().contains(tree) &&
              summary->text().contains("tree"));
  QTRY_VERIFY(dag->viewport()->height() >= 170);
  QVERIFY(dag->transform().m11() >= 0.85);
  int cards = 0;
  for (auto *item : dag->scene()->items()) {
    if (item->data(0).toString() == blob)
      ++cards;
  }
  QCOMPARE(cards, 24);
  QVERIFY(dialog.findChild<QLabel *>("objectDagStatus")->text().contains("24"));
  auto *expand = dialog.findChild<QPushButton *>("objectDagExpandButton");
  QVERIFY(expand->isChecked());
  expand->click();
  QTRY_VERIFY(
      dialog.findChild<QTreeWidget *>("objectResultsTable")->isVisible());
  QCoreApplication::processEvents();
  const int height = dag->viewport()->height();
  expand->click();
  QVERIFY(!dialog.findChild<QTreeWidget *>("objectResultsTable")->isVisible());
  QTRY_VERIFY(dag->viewport()->height() > height);
  dialog.findChild<QPushButton *>("objectDagZoomInButton")->click();
  QVERIFY(dag->transform().m11() > 0.85);
  dialog.findChild<QPushButton *>("objectDagActualSizeButton")->click();
  QCOMPARE(dag->transform().m11(), 1.0);
  dialog.findChild<QPushButton *>("objectDagFitButton")->click();
  QVERIFY(dag->transform().m11() < 1.0);

  // Follow an actual scene card, then navigate back to the original tree.
  QGraphicsItem *target = nullptr;
  for (auto *item : dag->scene()->items())
    if (item->data(0).toString() == blob) {
      target = item;
      break;
    }
  QVERIFY(target);
  dialog.findChild<QPushButton *>("objectDagActualSizeButton")->click();
  dag->centerOn(target);
  QTest::mouseClick(dag->viewport(), Qt::LeftButton, Qt::NoModifier,
                    dag->mapFromScene(target->sceneBoundingRect().center()));
  QTRY_VERIFY(summary->text().contains(blob) &&
              summary->text().contains("blob"));
  dialog.findChild<QPushButton *>("objectBackButton")->click();
  QTRY_VERIFY(summary->text().contains(tree) &&
              summary->text().contains("tree"));
  QVERIFY(dag->transform().m11() >= 0.85);
}

QTEST_MAIN(TestGitObjectExplorer)
#include "test_gitobjectexplorer.moc"
