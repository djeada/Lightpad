#include "terminalpty.h"

#ifndef Q_OS_WIN

#include <QFile>
#include <QProcessEnvironment>
#include <QSocketNotifier>
#include <QTimer>
#include <QVector>

#if defined(Q_OS_MACOS)
#include <util.h>
#else
#include <pty.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

TerminalPty::TerminalPty(QObject *parent)
    : QObject(parent), m_masterFd(-1), m_pid(-1), m_readNotifier(nullptr),
      m_reapTimer(new QTimer(this)), m_running(false) {
  m_reapTimer->setInterval(80);
  connect(m_reapTimer, &QTimer::timeout, this, &TerminalPty::reapChild);
}

TerminalPty::~TerminalPty() { stop(); }

bool TerminalPty::start(const QString &program, const QStringList &arguments,
                        const QString &workingDirectory,
                        const QMap<QString, QString> &environment) {
  stop();

  QByteArray programBytes = QFile::encodeName(program);
  QByteArray cwdBytes = QFile::encodeName(workingDirectory);
  QList<QByteArray> argBytes;
  argBytes.reserve(arguments.size() + 1);
  argBytes.append(programBytes);
  for (const QString &arg : arguments) {
    argBytes.append(arg.toLocal8Bit());
  }

  QList<QPair<QByteArray, QByteArray>> envBytes;
  QProcessEnvironment systemEnv = QProcessEnvironment::systemEnvironment();
  for (const QString &key : systemEnv.keys()) {
    envBytes.append({key.toLocal8Bit(), systemEnv.value(key).toLocal8Bit()});
  }
  for (auto it = environment.constBegin(); it != environment.constEnd(); ++it) {
    envBytes.append({it.key().toLocal8Bit(), it.value().toLocal8Bit()});
  }

  QVector<char *> argv;
  argv.reserve(argBytes.size() + 1);
  for (QByteArray &arg : argBytes) {
    argv.append(arg.data());
  }
  argv.append(nullptr);

  int masterFd = -1;
  pid_t childPid = forkpty(&masterFd, nullptr, nullptr, nullptr);
  if (childPid < 0) {
    emit errorOccurred(QString::fromLocal8Bit(strerror(errno)));
    return false;
  }

  if (childPid == 0) {
    if (!cwdBytes.isEmpty()) {
      if (::chdir(cwdBytes.constData()) != 0) {
        int ignored = ::chdir(getenv("HOME") ? getenv("HOME") : "/");
        (void)ignored;
      }
    }

    for (const auto &entry : envBytes) {
      ::setenv(entry.first.constData(), entry.second.constData(), 1);
    }

    ::setenv("TERM", "xterm-256color", 1);
    ::setenv("COLORTERM", "truecolor", 1);
    ::setenv("LIGHTPAD_TERMINAL", "1", 1);

    ::execvp(programBytes.constData(), argv.data());
    _exit(127);
  }

  m_masterFd = masterFd;
  m_pid = childPid;
  m_running = true;

  int flags = ::fcntl(m_masterFd, F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);
  }

  m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
  connect(m_readNotifier, &QSocketNotifier::activated, this,
          &TerminalPty::readAvailable);
  m_reapTimer->start();
  return true;
}

void TerminalPty::stop() {
  if (m_running && m_pid > 0) {
    pid_t pid = static_cast<pid_t>(m_pid);
    ::kill(-pid, SIGTERM);
    ::kill(pid, SIGTERM);
    for (int i = 0; i < 4; ++i) {
      int status = 0;
      pid_t result = ::waitpid(pid, &status, WNOHANG);
      if (result == pid) {
        finishFromStatus(status);
        break;
      }
      usleep(25000);
    }
    if (m_running) {
      ::kill(-pid, SIGKILL);
      ::kill(pid, SIGKILL);
      int status = 0;
      if (::waitpid(pid, &status, 0) == pid) {
        finishFromStatus(status);
      }
    }
  }

  closeMaster();
  m_running = false;
  m_pid = -1;
  if (m_reapTimer) {
    m_reapTimer->stop();
  }
}

bool TerminalPty::isRunning() const { return m_running; }

qint64 TerminalPty::processId() const { return m_pid; }

qint64 TerminalPty::writeData(const QByteArray &data) {
  if (!m_running || m_masterFd < 0 || data.isEmpty()) {
    return -1;
  }
  return ::write(m_masterFd, data.constData(), data.size());
}

bool TerminalPty::interruptProcessGroup() {
  if (!m_running || m_pid <= 0) {
    return false;
  }
  pid_t pid = static_cast<pid_t>(m_pid);
  if (::kill(-pid, SIGINT) == 0) {
    return true;
  }
  return ::kill(pid, SIGINT) == 0;
}

void TerminalPty::resize(int columns, int rows) {
  if (!m_running || m_masterFd < 0 || columns <= 0 || rows <= 0) {
    return;
  }

  winsize size;
  memset(&size, 0, sizeof(size));
  size.ws_col = static_cast<unsigned short>(columns);
  size.ws_row = static_cast<unsigned short>(rows);
  ::ioctl(m_masterFd, TIOCSWINSZ, &size);
  if (m_pid > 0) {
    pid_t pid = static_cast<pid_t>(m_pid);
    if (::kill(-pid, SIGWINCH) != 0) {
      ::kill(pid, SIGWINCH);
    }
  }
}

void TerminalPty::readAvailable() {
  if (m_masterFd < 0) {
    return;
  }

  QByteArray data;
  char buffer[8192];
  while (true) {
    ssize_t count = ::read(m_masterFd, buffer, sizeof(buffer));
    if (count > 0) {
      data.append(buffer, static_cast<int>(count));
      continue;
    }
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
    if (count == 0 || (count < 0 && errno != EINTR)) {
      break;
    }
  }

  if (!data.isEmpty()) {
    emit readyRead(data);
  }
}

void TerminalPty::reapChild() {
  if (!m_running || m_pid <= 0) {
    return;
  }

  int status = 0;
  pid_t result = ::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);
  if (result == m_pid) {
    finishFromStatus(status);
  }
}

void TerminalPty::closeMaster() {
  if (m_readNotifier) {
    m_readNotifier->setEnabled(false);
    m_readNotifier->deleteLater();
    m_readNotifier = nullptr;
  }
  if (m_masterFd >= 0) {
    ::close(m_masterFd);
    m_masterFd = -1;
  }
}

void TerminalPty::finishFromStatus(int status) {
  const bool crashed = WIFSIGNALED(status);
  int exitCode = 0;
  if (WIFEXITED(status)) {
    exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exitCode = 128 + WTERMSIG(status);
  }

  closeMaster();
  m_running = false;
  m_pid = -1;
  if (m_reapTimer) {
    m_reapTimer->stop();
  }
  emit finished(exitCode, crashed);
}

#endif
