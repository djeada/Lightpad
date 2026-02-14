#ifndef DEBUGSESSION_H
#define DEBUGSESSION_H

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <memory>

#include "dapclient.h"
#include "debugconfiguration.h"
#include "idebugadapter.h"

class DebugSession : public QObject {
  Q_OBJECT

public:
  enum class State { Idle, Starting, Running, Stopped, Terminated };
  Q_ENUM(State)

  explicit DebugSession(const QString &id, QObject *parent = nullptr);
  ~DebugSession();

  QString id() const { return m_id; }

  QString name() const { return m_configuration.name; }

  State state() const { return m_state; }

  DapClient *client() const { return m_client.get(); }

  DebugConfiguration configuration() const { return m_configuration; }

  std::shared_ptr<IDebugAdapter> adapter() const { return m_adapter; }

  bool start(const DebugConfiguration &config,
             std::shared_ptr<IDebugAdapter> adapter);

  void stop(bool terminate = true);

  void restart();

  void continueExecution() {
    if (m_client)
      m_client->continueExecution();
  }
  void pause() {
    if (m_client)
      m_client->pause();
  }
  void stepOver() {
    if (m_client)
      m_client->stepOver(0);
  }
  void stepInto() {
    if (m_client)
      m_client->stepInto(0);
  }
  void stepOut() {
    if (m_client)
      m_client->stepOut(0);
  }

signals:
  void stateChanged(State state);
  void started();
  void stopped(const DapStoppedEvent &event);
  void terminated();
  void outputReceived(const DapOutputEvent &event);
  void error(const QString &message);

private slots:
  void onClientStateChanged(DapClient::State state);
  void onClientAdapterInitialized();
  void onClientStopped(const DapStoppedEvent &event);
  void onClientTerminated();
  void onClientOutput(const DapOutputEvent &event);
  void onClientError(const QString &message);

private:
  void setState(State state);

  QString m_id;
  State m_state;
  DebugConfiguration m_configuration;
  std::shared_ptr<IDebugAdapter> m_adapter;
  std::unique_ptr<DapClient> m_client;
  bool m_launchRequestSent;
  bool m_adapterInitializedReceived;
  bool m_configurationDoneSent;
};

class DebugSessionManager : public QObject {
  Q_OBJECT

public:
  static DebugSessionManager &instance();

  QString startSession(const DebugConfiguration &config);

  QString startSession(const DebugConfiguration &config,
                       std::shared_ptr<IDebugAdapter> adapter);

  QString quickStart(const QString &filePath,
                     const QString &languageId = QString());

  void stopSession(const QString &sessionId, bool terminate = true);

  void stopAllSessions(bool terminate = true);

  DebugSession *session(const QString &sessionId) const;

  QList<DebugSession *> allSessions() const;

  DebugSession *focusedSession() const;

  void setFocusedSession(const QString &sessionId);

  int sessionCount() const { return m_sessions.size(); }

  bool hasActiveSessions() const;

signals:
  void sessionStarted(const QString &sessionId);
  void sessionStopped(const QString &sessionId, const DapStoppedEvent &event);
  void sessionTerminated(const QString &sessionId);
  void sessionError(const QString &sessionId, const QString &message);
  void focusedSessionChanged(const QString &sessionId);
  void allSessionsEnded();

private slots:
  void onSessionStateChanged(DebugSession::State state);
  void onSessionTerminated();

private:
  DebugSessionManager();
  ~DebugSessionManager() = default;
  DebugSessionManager(const DebugSessionManager &) = delete;
  DebugSessionManager &operator=(const DebugSessionManager &) = delete;

  QString generateSessionId();

  QMap<QString, DebugSession *> m_sessions;
  QString m_focusedSessionId;
  int m_nextSessionNumber;
};

#endif
