#ifndef SQLITESESSION_H
#define SQLITESESSION_H

#include "idbsession.h"

#include <QThread>

class SqliteWorker;

class SqliteSession : public IDbSession {
  Q_OBJECT

public:
  explicit SqliteSession(const DbConnectionProfile &profile,
                         QObject *parent = nullptr);
  ~SqliteSession() override;

  void open(const QString &password) override;
  void close() override;
  bool isOpen() const override { return m_open; }
  bool isBusy() const override { return m_busy; }
  void execute(quint64 requestId, const QStringList &statements,
               bool stopOnError) override;
  void cancel() override;

private:
  friend class SqliteWorker;
  DbConnectionProfile m_profile;
  QThread m_thread;
  SqliteWorker *m_worker = nullptr;
  bool m_open = false;
  bool m_busy = false;
  quint64 m_activeRequest = 0;
  quint64 m_generation = 0;
};

#endif
