#ifndef TERMINALPTY_H
#define TERMINALPTY_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

#ifndef Q_OS_WIN

class QSocketNotifier;
class QTimer;

class TerminalPty : public QObject {
  Q_OBJECT

public:
  explicit TerminalPty(QObject *parent = nullptr);
  ~TerminalPty() override;

  bool start(const QString &program, const QStringList &arguments,
             const QString &workingDirectory,
             const QMap<QString, QString> &environment);
  void stop();
  bool isRunning() const;
  qint64 processId() const;

  qint64 writeData(const QByteArray &data);
  bool interruptProcessGroup();
  void resize(int columns, int rows);

signals:
  void readyRead(const QByteArray &data);
  void finished(int exitCode, bool crashed);
  void errorOccurred(const QString &message);

private slots:
  void readAvailable();
  void reapChild();

private:
  void closeMaster();
  void finishFromStatus(int status);

  int m_masterFd;
  qint64 m_pid;
  QSocketNotifier *m_readNotifier;
  QTimer *m_reapTimer;
  bool m_running;
};

#endif

#endif
