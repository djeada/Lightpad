#ifndef CLISESSION_H
#define CLISESSION_H

#include "clidialect.h"
#include "idbsession.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
#include <memory>

class CliSession : public IDbSession {
  Q_OBJECT

public:
  CliSession(const DbConnectionProfile &profile, QObject *parent = nullptr);
  ~CliSession() override;

  void open(const QString &password) override;
  void close() override;
  bool isOpen() const override {
    return m_state == State::Ready || m_state == State::Busy;
  }
  bool isBusy() const override { return m_state == State::Busy; }
  void execute(quint64 requestId, const QStringList &statements,
               bool stopOnError) override;
  void cancel() override;

  const DbConnectionProfile &profile() const { return m_profile; }

private slots:
  void onStdout();
  void onStderr();
  void onFinished(int exitCode, QProcess::ExitStatus status);
  void onErrorOccurred(QProcess::ProcessError error);

private:
  enum class State { Closed, Starting, Probing, Ready, Busy };

  struct Job {
    quint64 requestId = 0;
    QStringList statements;
    bool stopOnError = true;
    int index = 0;
  };

  void startNextStatement();
  void sendStatement(const QString &sql);
  void checkStatementDone(bool forceFinish);
  void finishStatement();
  void finishJob(bool cancelled);
  void failOpen(const QString &message);
  void teardownProcess();

  DbConnectionProfile m_profile;
  std::unique_ptr<CliDialect> m_dialect;
  QProcess *m_process = nullptr;
  QString m_password;
  State m_state = State::Closed;

  Job m_job;
  bool m_jobActive = false;
  QString m_marker;
  QString m_outBuffer;
  QString m_errBuffer;
  QByteArray m_outPending;
  QByteArray m_errPending;
  bool m_outDone = false;
  bool m_errDone = false;
  SqlStatementKind m_kind = SqlStatementKind::Other;
  QString m_currentSql;
  QElapsedTimer m_timer;
  QTimer m_graceTimer;
  bool m_closing = false;
};

#endif
