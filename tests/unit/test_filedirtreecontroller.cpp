#include "filetree/filedirtreecontroller.h"
#include "ui/dialogs/themedmessagebox.h"

#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class TestFileDirTreeController : public QObject {
  Q_OBJECT

private slots:
  void testCancelledMoveLeavesEverySourceInPlace();
  void testConfirmedMoveTransfersEverySource();
  void testCopyDoesNotAskForMoveConfirmation();
  void testPermanentDeletionDefaultsToNo();
};

void TestFileDirTreeController::testCancelledMoveLeavesEverySourceInPlace() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString first = dir.filePath("first.txt");
  const QString second = dir.filePath("second.txt");
  QVERIFY(QFile(first).open(QIODevice::WriteOnly));
  QVERIFY(QFile(second).open(QIODevice::WriteOnly));
  QVERIFY(QDir().mkdir(dir.filePath("destination")));
  FileDirTreeModel model;
  FileDirTreeController controller(&model);
  QSignalSpy completed(&controller, &FileDirTreeController::actionCompleted);
  bool prompted = false;
  bool cancelIsDefault = false;
  QTimer::singleShot(0, &controller, [&]() {
    auto *box = qobject_cast<ThemedMessageBox *>(QApplication::activeModalWidget());
    if (!box)
      return;
    prompted = true;
    cancelIsDefault = box->button(ThemedMessageBox::No)->isDefault();
    // Enter must choose cancellation, including for accidental drops.
    QTest::keyClick(box, Qt::Key_Return);
  });
  QVERIFY(controller.handleTransfer({first, second},
                                     dir.filePath("destination"), false).isEmpty());
  QVERIFY(prompted);
  QVERIFY(cancelIsDefault);
  QVERIFY(QFile::exists(first));
  QVERIFY(QFile::exists(second));
  QVERIFY(QDir(dir.filePath("destination")).isEmpty());
  QCOMPARE(completed.count(), 0);
}

void TestFileDirTreeController::testConfirmedMoveTransfersEverySource() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString first = dir.filePath("first.txt");
  const QString second = dir.filePath("second.txt");
  QVERIFY(QFile(first).open(QIODevice::WriteOnly));
  QVERIFY(QFile(second).open(QIODevice::WriteOnly));
  QVERIFY(QDir().mkdir(dir.filePath("destination")));
  FileDirTreeModel model;
  FileDirTreeController controller(&model);
  QTimer::singleShot(0, &controller, []() {
    auto *box = qobject_cast<ThemedMessageBox *>(QApplication::activeModalWidget());
    if (box)
      box->button(ThemedMessageBox::Yes)->click();
  });
  const QStringList result = controller.handleTransfer(
      {first, second}, dir.filePath("destination"), false);
  QCOMPARE(result.size(), 2);
  QVERIFY(!QFile::exists(first));
  QVERIFY(!QFile::exists(second));
  QVERIFY(QFile::exists(dir.filePath("destination/first.txt")));
  QVERIFY(QFile::exists(dir.filePath("destination/second.txt")));
}

void TestFileDirTreeController::testCopyDoesNotAskForMoveConfirmation() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString source = dir.filePath("source.txt");
  QVERIFY(QFile(source).open(QIODevice::WriteOnly));
  QVERIFY(QDir().mkdir(dir.filePath("destination")));
  FileDirTreeModel model;
  FileDirTreeController controller(&model);
  bool prompted = false;
  QTimer::singleShot(0, &controller, [&]() {
    if (auto *box = QApplication::activeModalWidget()) {
      prompted = true;
      box->close();
    }
  });
  const QStringList result = controller.handleTransfer(
      {source}, dir.filePath("destination"), true);
  QCoreApplication::processEvents();
  QVERIFY(!prompted);
  QCOMPARE(result.size(), 1);
  QVERIFY(QFile::exists(source));
  QVERIFY(QFile::exists(dir.filePath("destination/source.txt")));
}

void TestFileDirTreeController::testPermanentDeletionDefaultsToNo() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString source = dir.filePath("source.txt");
  QVERIFY(QFile(source).open(QIODevice::WriteOnly));
  FileDirTreeModel model;
  FileDirTreeController controller(&model);
  QTimer::singleShot(0, &controller, []() {
    auto *box = qobject_cast<ThemedMessageBox *>(QApplication::activeModalWidget());
    if (box)
      QTest::keyClick(box, Qt::Key_Return);
  });
  controller.handleRemove({source}, DeleteMode::Permanent);
  QVERIFY(QFile::exists(source));
}

QTEST_MAIN(TestFileDirTreeController)
#include "test_filedirtreecontroller.moc"
