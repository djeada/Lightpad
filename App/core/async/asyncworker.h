#ifndef ASYNCWORKER_H
#define ASYNCWORKER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <functional>

/**
 * @brief Base class for async operations
 * 
 * Provides a way to run operations in background threads
 * without blocking the UI. Supports cancellation and progress reporting.
 */
class AsyncWorker : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Worker state enumeration
     */
    enum class State {
        Idle,
        Running,
        Completed,
        Cancelled,
        Error
    };
    Q_ENUM(State)

    explicit AsyncWorker(QObject* parent = nullptr);
    virtual ~AsyncWorker();

    /**
     * @brief Get current state
     * @return Current worker state
     */
    State state() const;

    /**
     * @brief Check if worker is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Check if cancellation was requested
     * @return true if cancelled
     */
    bool isCancelled() const;

    /**
     * @brief Get last error message
     * @return Error message or empty string
     */
    QString errorMessage() const;

public slots:
    /**
     * @brief Start the async operation
     */
    virtual void start();

    /**
     * @brief Request cancellation of the operation
     */
    virtual void cancel();

signals:
    /**
     * @brief Emitted when the operation starts
     */
    void started();

    /**
     * @brief Emitted when the operation completes
     */
    void finished();

    /**
     * @brief Emitted on progress updates
     * @param percent Progress percentage (0-100)
     * @param message Optional status message
     */
    void progress(int percent, const QString& message = QString());

    /**
     * @brief Emitted when an error occurs
     * @param error Error message
     */
    void error(const QString& error);

    /**
     * @brief Emitted when cancelled
     */
    void cancelled();

public:
    /**
     * @brief Report progress (calls progress signal)
     * @param percent Progress percentage (0-100)
     * @param message Optional status message
     */
    void reportProgress(int percent, const QString& message = QString());

protected:
    /**
     * @brief The actual work to be done (implement in subclass)
     * Override this method to perform the async operation.
     * Check isCancelled() periodically to support cancellation.
     */
    virtual void doWork() = 0;

    /**
     * @brief Set error state with message
     * @param message Error message
     */
    void setError(const QString& message);

private:
    State m_state;
    QString m_errorMessage;
    bool m_cancelled;
    mutable QMutex m_mutex;
};

/**
 * @brief Generic async task for running lambdas in background
 */
class AsyncTask : public AsyncWorker {
    Q_OBJECT

public:
    using TaskFunction = std::function<void(AsyncTask*)>;

    explicit AsyncTask(TaskFunction task, QObject* parent = nullptr);

protected:
    void doWork() override;

private:
    TaskFunction m_task;
};

/**
 * @brief Thread pool for managing async workers
 */
class AsyncThreadPool : public QObject {
    Q_OBJECT

public:
    static AsyncThreadPool& instance();

    /**
     * @brief Submit a worker to run in a background thread
     * @param worker Worker to execute
     */
    void submit(AsyncWorker* worker);

    /**
     * @brief Submit a lambda to run in background
     * @param task Lambda function to execute
     * @return AsyncTask that will run the lambda
     */
    AsyncTask* submitTask(AsyncTask::TaskFunction task);

    /**
     * @brief Cancel all running workers
     */
    void cancelAll();

    /**
     * @brief Wait for all workers to complete
     */
    void waitAll();

private:
    AsyncThreadPool();
    ~AsyncThreadPool();
    AsyncThreadPool(const AsyncThreadPool&) = delete;
    AsyncThreadPool& operator=(const AsyncThreadPool&) = delete;

    QList<QThread*> m_threads;
    QList<AsyncWorker*> m_workers;
    QMutex m_mutex;
};

#endif // ASYNCWORKER_H
