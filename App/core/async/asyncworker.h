#ifndef ASYNCWORKER_H
#define ASYNCWORKER_H

#include <QMutex>
#include <QObject>
#include <QThread>
#include <functional>

class AsyncWorker : public QObject {
  Q_OBJECT

public:
  enum class State { Idle, Running, Completed, Cancelled, Error };
  Q_ENUM(State)

  explicit AsyncWorker(QObject *parent = nullptr);
  virtual ~AsyncWorker();

  State state() const;

  bool isRunning() const;

  bool isCancelled() const;

  QString errorMessage() const;

public slots:

  virtual void start();

  virtual void cancel();

signals:

  void started();

  void finished();

  void progress(int percent, const QString &message = QString());

  void error(const QString &error);

  void cancelled();

public:
  void reportProgress(int percent, const QString &message = QString());

protected:
  virtual void doWork() = 0;

  void setError(const QString &message);

private:
  State m_state;
  QString m_errorMessage;
  bool m_cancelled;
  mutable QMutex m_mutex;
};

class AsyncTask : public AsyncWorker {
  Q_OBJECT

public:
  using TaskFunction = std::function<void(AsyncTask *)>;

  explicit AsyncTask(TaskFunction task, QObject *parent = nullptr);

protected:
  void doWork() override;

private:
  TaskFunction m_task;
};

class AsyncThreadPool : public QObject {
  Q_OBJECT

public:
  static AsyncThreadPool &instance();

  void submit(AsyncWorker *worker);

  AsyncTask *submitTask(AsyncTask::TaskFunction task);

  void cancelAll();

  void waitAll();

private:
  AsyncThreadPool();
  ~AsyncThreadPool();
  AsyncThreadPool(const AsyncThreadPool &) = delete;
  AsyncThreadPool &operator=(const AsyncThreadPool &) = delete;

  QList<QThread *> m_threads;
  QList<AsyncWorker *> m_workers;
  QMutex m_mutex;
};

#endif
