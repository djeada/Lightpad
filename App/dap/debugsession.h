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

/**
 * @brief Represents a single debug session
 *
 * Encapsulates a debug adapter client and its associated configuration.
 * Provides high-level control over the debug session lifecycle.
 */
class DebugSession : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Session states
   */
  enum class State {
    Idle,      // Not started
    Starting,  // Adapter starting
    Running,   // Debuggee running
    Stopped,   // Stopped at breakpoint/step
    Terminated // Session ended
  };
  Q_ENUM(State)

  explicit DebugSession(const QString &id, QObject *parent = nullptr);
  ~DebugSession();

  /**
   * @brief Get session ID
   */
  QString id() const { return m_id; }

  /**
   * @brief Get session name (from configuration)
   */
  QString name() const { return m_configuration.name; }

  /**
   * @brief Get current state
   */
  State state() const { return m_state; }

  /**
   * @brief Get the DAP client
   */
  DapClient *client() const { return m_client.get(); }

  /**
   * @brief Get the debug configuration
   */
  DebugConfiguration configuration() const { return m_configuration; }

  /**
   * @brief Get the debug adapter
   */
  std::shared_ptr<IDebugAdapter> adapter() const { return m_adapter; }

  /**
   * @brief Start the debug session
   * @param config Debug configuration
   * @param adapter Debug adapter to use
   * @return true if started successfully
   */
  bool start(const DebugConfiguration &config,
             std::shared_ptr<IDebugAdapter> adapter);

  /**
   * @brief Stop the debug session
   * @param terminate Whether to terminate the debuggee
   */
  void stop(bool terminate = true);

  /**
   * @brief Restart the debug session
   */
  void restart();

  // Execution control shortcuts

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
};

/**
 * @brief Manages multiple debug sessions
 *
 * The DebugSessionManager provides:
 * - Creating and managing debug sessions
 * - Multi-target debugging support
 * - Session lifecycle management
 * - Focus management for active session
 */
class DebugSessionManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static DebugSessionManager &instance();

  /**
   * @brief Create and start a new debug session
   * @param config Debug configuration
   * @return Session ID, or empty string on failure
   */
  QString startSession(const DebugConfiguration &config);

  /**
   * @brief Create and start a session with a specific adapter
   * @param config Debug configuration
   * @param adapter Debug adapter to use
   * @return Session ID, or empty string on failure
   */
  QString startSession(const DebugConfiguration &config,
                       std::shared_ptr<IDebugAdapter> adapter);

  /**
   * @brief Quick-start debugging for a file
   *
   * Auto-selects adapter and creates configuration.
   *
   * @param filePath Path to file to debug
   * @return Session ID, or empty string on failure
   */
  QString quickStart(const QString &filePath);

  /**
   * @brief Stop a debug session
   * @param sessionId Session ID
   * @param terminate Whether to terminate debuggee
   */
  void stopSession(const QString &sessionId, bool terminate = true);

  /**
   * @brief Stop all active sessions
   * @param terminate Whether to terminate debuggees
   */
  void stopAllSessions(bool terminate = true);

  /**
   * @brief Get a session by ID
   */
  DebugSession *session(const QString &sessionId) const;

  /**
   * @brief Get all active sessions
   */
  QList<DebugSession *> allSessions() const;

  /**
   * @brief Get the focused (active) session
   */
  DebugSession *focusedSession() const;

  /**
   * @brief Set the focused session
   */
  void setFocusedSession(const QString &sessionId);

  /**
   * @brief Get session count
   */
  int sessionCount() const { return m_sessions.size(); }

  /**
   * @brief Check if any session is active
   */
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

#endif // DEBUGSESSION_H
