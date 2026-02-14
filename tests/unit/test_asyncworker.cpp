#include "core/async/asyncworker.h"
#include <QtTest/QtTest>

class TestAsyncWorker : public QObject {
  Q_OBJECT

private slots:
  void testAsyncTaskExecution();
  void testAsyncTaskCancellation();
  void testAsyncTaskProgress();
  void testAsyncThreadPool();
};

void TestAsyncWorker::testAsyncTaskExecution() {
  bool executed = false;

  AsyncTask task([&executed](AsyncTask *task) {
    Q_UNUSED(task);
    executed = true;
  });

  task.start();

  QVERIFY(executed);
  QCOMPARE(task.state(), AsyncWorker::State::Completed);
}

void TestAsyncWorker::testAsyncTaskCancellation() {

  AsyncTask task([](AsyncTask *task) { Q_UNUSED(task); });

  QVERIFY(!task.isCancelled());

  task.cancel();
  QVERIFY(task.isCancelled());
}

void TestAsyncWorker::testAsyncTaskProgress() {
  QList<int> progressValues;

  AsyncTask task([](AsyncTask *task) {
    for (int i = 0; i <= 100; i += 25) {
      task->reportProgress(i, QString("Step %1").arg(i));
    }
  });

  QObject::connect(&task, &AsyncTask::progress,
                   [&progressValues](int percent, const QString &) {
                     progressValues.append(percent);
                   });

  task.start();

  QCOMPARE(progressValues.size(), 5);
  QCOMPARE(progressValues.first(), 0);
  QCOMPARE(progressValues.last(), 100);
}

void TestAsyncWorker::testAsyncThreadPool() {
  AsyncThreadPool &pool = AsyncThreadPool::instance();

  AsyncThreadPool &pool2 = AsyncThreadPool::instance();
  QCOMPARE(&pool, &pool2);
}

QTEST_MAIN(TestAsyncWorker)
#include "test_asyncworker.moc"
