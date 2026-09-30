#include "clisession.h"

#include <QProcessEnvironment>
#include <QUuid>

namespace {
constexpr qint64 kMaxBufferedChars = 256 * 1024 * 1024;
constexpr int kStderrGraceMs = 250;

void appendUtf8(QString &buffer, QByteArray &pending, const QByteArray &data) {
  pending += data;

  int keep = 0;
  for (int back = 1; back <= 3 && back <= pending.size(); ++back) {
    const unsigned char c =
        static_cast<unsigned char>(pending[pending.size() - back]);
    if ((c & 0xC0) == 0x80) {
      continue;
    }
    int needed = 1;
    if ((c & 0xE0) == 0xC0) {
      needed = 2;
    } else if ((c & 0xF0) == 0xE0) {
      needed = 3;
    } else if ((c & 0xF8) == 0xF0) {
      needed = 4;
    }
    if (needed > back) {
      keep = back;
    }
    break;
  }
  const QByteArray complete = pending.left(pending.size() - keep);
  pending = pending.right(keep);
  buffer += QString::fromUtf8(complete);
}
} // namespace

CliSession::CliSession(const DbConnectionProfile &profile, QObject *parent)
    : IDbSession(parent), m_profile(profile),
      m_dialect(CliDialect::create(profile.engine)) {
  m_graceTimer.setSingleShot(true);
  m_graceTimer.setInterval(kStderrGraceMs);
  connect(&m_graceTimer, &QTimer::timeout, this,
          [this]() { checkStatementDone(true); });
}

CliSession::~CliSession() {
  m_closing = true;
  teardownProcess();
}

void CliSession::teardownProcess() {
  m_graceTimer.stop();
  if (!m_process) {
    return;
  }
  QProcess *p = m_process;
  m_process = nullptr;
  p->disconnect(this);
  if (p->state() != QProcess::NotRunning) {
    p->closeWriteChannel();
    if (!p->waitForFinished(300)) {
      p->kill();
      p->waitForFinished(500);
    }
  }
  p->deleteLater();
}

void CliSession::open(const QString &password) {
  if (m_state != State::Closed) {
    return;
  }
  if (!m_dialect) {
    failOpen(QStringLiteral("Unsupported database engine."));
    return;
  }
  m_password = password;
  const CliLaunch spec = m_dialect->launch(m_profile, password);
  if (!spec.error.isEmpty()) {
    failOpen(spec.error);
    return;
  }

  m_state = State::Starting;
  m_outBuffer.clear();
  m_errBuffer.clear();
  m_outPending.clear();
  m_errPending.clear();

  m_process = new QProcess(this);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  for (auto it = spec.environment.begin(); it != spec.environment.end(); ++it) {
    env.insert(it.key(), it.value());
  }
  m_process->setProcessEnvironment(env);
  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &CliSession::onStdout);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &CliSession::onStderr);
  connect(m_process, &QProcess::finished, this, &CliSession::onFinished);
  connect(m_process, &QProcess::errorOccurred, this,
          &CliSession::onErrorOccurred);
  m_process->start(spec.program, spec.arguments);
  if (!spec.stdinPreamble.isEmpty()) {
    m_process->write(spec.stdinPreamble);
  }

  m_job = Job();
  m_job.statements = {m_dialect->probeStatement()};
  m_jobActive = true;
  m_state = State::Probing;
  startNextStatement();
}

void CliSession::close() {
  m_closing = true;
  const bool wasOpen = m_state != State::Closed;
  teardownProcess();
  m_state = State::Closed;
  m_jobActive = false;
  m_closing = false;
  if (wasOpen) {
    emit closed(QString());
  }
}

void CliSession::failOpen(const QString &message) {
  teardownProcess();
  m_state = State::Closed;
  m_jobActive = false;
  emit openFailed(message);
}

void CliSession::execute(quint64 requestId, const QStringList &statements,
                         bool stopOnError) {
  if (m_state != State::Ready) {
    DbStatementResult res;
    res.ok = false;
    res.error = m_state == State::Busy
                    ? QStringLiteral("The connection is busy running another "
                                     "query.")
                    : QStringLiteral("The connection is not open.");
    if (!statements.isEmpty()) {
      res.sql = statements.first();
    }
    emit statementFinished(requestId, 0, res);
    emit requestFinished(requestId, false);
    return;
  }
  m_job = Job();
  m_job.requestId = requestId;
  m_job.statements = statements;
  m_job.stopOnError = stopOnError;
  m_jobActive = true;
  m_state = State::Busy;
  startNextStatement();
}

void CliSession::cancel() {
  if (!m_jobActive || m_state == State::Closed) {
    return;
  }
  if (m_state == State::Probing || m_state == State::Starting) {
    close();
    return;
  }

  const quint64 id = m_job.requestId;
  m_jobActive = false;
  m_closing = true;
  teardownProcess();
  m_state = State::Closed;
  m_closing = false;
  emit requestFinished(id, true);
  emit closed(QStringLiteral("The query was cancelled and the session was "
                             "closed."));
}

void CliSession::startNextStatement() {
  while (m_job.index < m_job.statements.size()) {
    const QString sql = m_job.statements[m_job.index];
    if (!SqlStatementSplitter::stripComments(sql).trimmed().isEmpty()) {
      sendStatement(sql);
      return;
    }
    ++m_job.index;
  }
  finishJob(false);
}

void CliSession::sendStatement(const QString &sql) {
  m_marker = QStringLiteral("__LP_%1__")
                 .arg(QUuid::createUuid().toString(QUuid::Id128));
  m_currentSql = sql;
  m_kind = SqlStatementSplitter::classify(sql, m_profile.engine);
  m_outDone = false;
  m_errDone = !m_dialect->marksStderr();

  m_outBuffer.clear();
  m_errBuffer.clear();
  m_timer.restart();
  if (!m_process || m_process->state() == QProcess::NotRunning) {
    return;
  }
  m_process->write(m_dialect->frame(sql, m_kind, m_marker));
}

void CliSession::onStdout() {
  if (!m_process) {
    return;
  }
  appendUtf8(m_outBuffer, m_outPending, m_process->readAllStandardOutput());
  if (m_outBuffer.size() > kMaxBufferedChars) {
    failOpen(QStringLiteral("The client produced too much output."));
    return;
  }
  checkStatementDone(false);
}

void CliSession::onStderr() {
  if (!m_process) {
    return;
  }
  appendUtf8(m_errBuffer, m_errPending, m_process->readAllStandardError());
  checkStatementDone(false);
}

static bool containsMarkerLine(const QString &buffer, const QString &marker) {
  int from = 0;
  while (true) {
    const int pos = buffer.indexOf(marker, from);
    if (pos < 0) {
      return false;
    }
    const bool startsLine = pos == 0 || buffer[pos - 1] == '\n';
    const int after = pos + marker.size();
    const bool endsLine =
        after < buffer.size() &&
        (buffer[after] == '\n' ||
         (buffer[after] == '\r' && after + 1 < buffer.size()));
    if (startsLine && endsLine) {
      return true;
    }
    from = pos + 1;
  }
}

void CliSession::checkStatementDone(bool forceFinish) {
  if (!m_jobActive || m_marker.isEmpty()) {
    return;
  }
  if (!m_outDone && containsMarkerLine(m_outBuffer, m_marker)) {
    m_outDone = true;
  }
  if (!m_errDone && containsMarkerLine(m_errBuffer, m_marker)) {
    m_errDone = true;
  }
  if (m_outDone && (m_errDone || forceFinish)) {
    m_graceTimer.stop();
    finishStatement();
  } else if (m_outDone && !m_errDone && !m_graceTimer.isActive()) {
    m_graceTimer.start();
  }
}

static QString cutAtMarker(const QString &buffer, const QString &marker,
                           bool wholeLineOnly) {
  QStringList kept;
  const QStringList lines = buffer.split('\n');
  for (const QString &raw : lines) {
    QString line = raw;
    if (line.endsWith('\r')) {
      line.chop(1);
    }
    if (wholeLineOnly ? (line == marker) : line.startsWith(marker)) {
      break;
    }
    kept << line;
  }
  return kept.join('\n');
}

void CliSession::finishStatement() {
  const qint64 elapsed = m_timer.elapsed();
  const QString out = cutAtMarker(m_outBuffer, m_marker, false);
  const QString err = cutAtMarker(m_errBuffer, m_marker, true);
  m_marker.clear();

  DbStatementResult result =
      m_dialect->parse(m_currentSql, m_kind, out, err, m_maxRows);
  result.elapsedMs = elapsed;

  if (m_state == State::Probing) {
    if (!result.ok) {
      failOpen(result.error);
      return;
    }
    m_jobActive = false;
    m_state = State::Ready;
    emit opened();
    return;
  }

  emit statementFinished(m_job.requestId, m_job.index, result);

  if (m_state != State::Busy || !m_jobActive) {
    return;
  }
  if (!result.ok && m_job.stopOnError) {
    finishJob(false);
    return;
  }
  ++m_job.index;
  startNextStatement();
}

void CliSession::finishJob(bool cancelled) {
  const quint64 id = m_job.requestId;
  m_jobActive = false;
  m_marker.clear();
  if (m_state == State::Busy) {
    m_state = State::Ready;
  }
  emit requestFinished(id, cancelled);
}

void CliSession::onErrorOccurred(QProcess::ProcessError error) {
  if (m_closing || !m_process) {
    return;
  }
  if (error == QProcess::FailedToStart) {
    const QString program = m_process->program();
    failOpen(QStringLiteral("Could not start \"%1\": %2")
                 .arg(program, m_process->errorString()));
  }
}

void CliSession::onFinished(int exitCode, QProcess::ExitStatus status) {
  Q_UNUSED(status)
  if (m_closing || !m_process) {
    return;
  }

  appendUtf8(m_outBuffer, m_outPending, m_process->readAllStandardOutput());
  appendUtf8(m_errBuffer, m_errPending, m_process->readAllStandardError());
  const QString err = m_errBuffer.trimmed();
  const QString out = m_outBuffer.trimmed();

  const State before = m_state;
  const QString reason = m_dialect->startupError(err, out);
  if (before == State::Probing || before == State::Starting) {
    failOpen(
        reason.isEmpty()
            ? QStringLiteral("The client exited with code %1.").arg(exitCode)
            : reason);
    return;
  }

  const quint64 id = m_job.requestId;
  const bool hadJob = m_jobActive;
  const QString sql = m_currentSql;
  teardownProcess();
  m_state = State::Closed;
  m_jobActive = false;
  if (hadJob) {
    DbStatementResult res;
    res.sql = sql;
    res.ok = false;
    res.error = QStringLiteral("The connection was lost. ") + reason;
    emit statementFinished(id, m_job.index, res);
    emit requestFinished(id, false);
  }
  emit closed(
      reason.isEmpty()
          ? QStringLiteral("The client exited with code %1.").arg(exitCode)
          : reason);
}
