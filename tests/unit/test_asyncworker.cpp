#include "core/async/asyncworker.h"
#include <QPointer>
#include <QProcess>
#include <QSemaphore>
#include <QtTest/QtTest>

class TestAsyncWorker : public QObject {
  Q_OBJECT

private slots:
  void testAsyncTaskExecution();
  void testAsyncTaskCancellation();
  void testAsyncTaskProgress();
  void testAsyncThreadPool();
  void testSubmittedTaskReportsFinishedToLateListener();
  void testWaitAllDoesNotNeedTheGuiEventLoop();
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

void TestAsyncWorker::testSubmittedTaskReportsFinishedToLateListener() {
  AsyncTask *task = AsyncThreadPool::instance().submitTask([](AsyncTask *) {});
  QPointer<AsyncTask> guard(task);

  QThread::msleep(50);

  QObject context;
  int finishedCount = 0;
  QObject::connect(task, &AsyncTask::finished, &context,
                   [&finishedCount]() { ++finishedCount; });

  QTRY_COMPARE(finishedCount, 1);
  QTRY_VERIFY(guard.isNull());
}

void TestAsyncWorker::testWaitAllDoesNotNeedTheGuiEventLoop() {
  QProcess process;
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert("LIGHTPAD_TEST_POOL_SHUTDOWN", "1");
  process.setProcessEnvironment(environment);
  process.start(QCoreApplication::applicationFilePath(), {});
  QVERIFY(process.waitForStarted());
  const bool finished = process.waitForFinished(3000);
  if (!finished) {
    process.kill();
    process.waitForFinished();
  }
  QVERIFY2(finished, "Waiting for a search worker must not depend on the GUI "
                     "event loop processing quit signals");
  QCOMPARE(process.exitStatus(), QProcess::NormalExit);
  QCOMPARE(process.exitCode(), 0);
}

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  if (qEnvironmentVariableIsSet("LIGHTPAD_TEST_POOL_SHUTDOWN")) {
    QSemaphore started, release;
    auto &pool = AsyncThreadPool::instance();
    pool.submitTask([&](AsyncTask *) {
      started.release();
      release.acquire();
    });
    QCoreApplication::processEvents();
    started.acquire();
    release.release();
    pool.waitAll();
    QCoreApplication::processEvents();
    return 0;
  }
  TestAsyncWorker test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_asyncworker.moc"
