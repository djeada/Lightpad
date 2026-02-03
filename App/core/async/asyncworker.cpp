#include "asyncworker.h"
#include "../logging/logger.h"

#include <QThread>

// AsyncWorker implementation

AsyncWorker::AsyncWorker(QObject *parent)
    : QObject(parent), m_state(State::Idle), m_cancelled(false) {}

AsyncWorker::~AsyncWorker() {
  if (isRunning()) {
    cancel();
  }
}

AsyncWorker::State AsyncWorker::state() const {
  QMutexLocker locker(&m_mutex);
  return m_state;
}

bool AsyncWorker::isRunning() const {
  QMutexLocker locker(&m_mutex);
  return m_state == State::Running;
}

bool AsyncWorker::isCancelled() const {
  QMutexLocker locker(&m_mutex);
  return m_cancelled;
}

QString AsyncWorker::errorMessage() const {
  QMutexLocker locker(&m_mutex);
  return m_errorMessage;
}

void AsyncWorker::start() {
  {
    QMutexLocker locker(&m_mutex);
    if (m_state == State::Running) {
      return;
    }
    m_state = State::Running;
    m_cancelled = false;
    m_errorMessage.clear();
  }

  emit started();

  try {
    doWork();

    QMutexLocker locker(&m_mutex);
    if (m_cancelled) {
      m_state = State::Cancelled;
      emit cancelled();
    } else if (m_state == State::Running) {
      m_state = State::Completed;
      emit finished();
    }
  } catch (const std::exception &e) {
    setError(QString::fromStdString(e.what()));
  } catch (...) {
    setError("Unknown error occurred");
  }
}

void AsyncWorker::cancel() {
  QMutexLocker locker(&m_mutex);
  m_cancelled = true;
  LOG_DEBUG("Worker cancellation requested");
}

void AsyncWorker::setError(const QString &message) {
  {
    QMutexLocker locker(&m_mutex);
    m_state = State::Error;
    m_errorMessage = message;
  }
  LOG_ERROR(QString("AsyncWorker error: %1").arg(message));
  emit error(message);
}

void AsyncWorker::reportProgress(int percent, const QString &message) {
  emit progress(qBound(0, percent, 100), message);
}

// AsyncTask implementation

AsyncTask::AsyncTask(TaskFunction task, QObject *parent)
    : AsyncWorker(parent), m_task(task) {}

void AsyncTask::doWork() {
  if (m_task) {
    m_task(this);
  }
}

// AsyncThreadPool implementation

AsyncThreadPool &AsyncThreadPool::instance() {
  static AsyncThreadPool instance;
  return instance;
}

AsyncThreadPool::AsyncThreadPool() : QObject(nullptr) {}

AsyncThreadPool::~AsyncThreadPool() {
  cancelAll();
  waitAll();
}

void AsyncThreadPool::submit(AsyncWorker *worker) {
  QThread *thread = new QThread();
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &AsyncWorker::start);
  connect(worker, &AsyncWorker::finished, thread, &QThread::quit);
  connect(worker, &AsyncWorker::cancelled, thread, &QThread::quit);
  connect(worker, &AsyncWorker::error, thread, &QThread::quit);
  connect(thread, &QThread::finished, thread, &QThread::deleteLater);
  connect(thread, &QThread::finished, this, [this, thread, worker]() {
    QMutexLocker locker(&m_mutex);
    m_threads.removeOne(thread);
    m_workers.removeOne(worker);
  });

  {
    QMutexLocker locker(&m_mutex);
    m_threads.append(thread);
    m_workers.append(worker);
  }

  thread->start();
  LOG_DEBUG("Submitted worker to thread pool");
}

AsyncTask *AsyncThreadPool::submitTask(AsyncTask::TaskFunction task) {
  AsyncTask *asyncTask = new AsyncTask(task);
  submit(asyncTask);
  return asyncTask;
}

void AsyncThreadPool::cancelAll() {
  QMutexLocker locker(&m_mutex);
  for (AsyncWorker *worker : m_workers) {
    worker->cancel();
  }
  LOG_DEBUG("Cancelled all workers");
}

void AsyncThreadPool::waitAll() {
  QList<QThread *> threadsCopy;
  {
    QMutexLocker locker(&m_mutex);
    threadsCopy = m_threads;
  }

  for (QThread *thread : threadsCopy) {
    if (thread->isRunning()) {
      thread->wait();
    }
  }
}
