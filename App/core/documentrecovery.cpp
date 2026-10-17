#include "documentrecovery.h"
#include "async/asyncworker.h"
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTimer>
#include <atomic>

struct DocumentRecovery::State {
  explicit State(const QString &path) : store(path) {}
  Recovery::Store store;
  QMutex mutex;
  std::atomic_bool closed{false};
};
struct DocumentRecovery::Version {
  std::atomic_int value{0};
};

DocumentRecovery::DocumentRecovery(QObject *parent, const QString &rootPath)
    : QObject(parent),
      m_state(std::make_shared<State>(
          rootPath.isEmpty() ? QDir(QStandardPaths::writableLocation(
                                        QStandardPaths::AppLocalDataLocation))
                                   .filePath("recovery")
                             : rootPath)),
      m_timer(new QTimer(this)) {
  m_state->store.start(&m_initializationError);
  m_timer->setInterval(1000);
  connect(m_timer, &QTimer::timeout, this, &DocumentRecovery::checkpoint);
}
DocumentRecovery::~DocumentRecovery() { m_timer->stop(); }
QString DocumentRecovery::initializationError() const {
  return m_initializationError;
}
QString DocumentRecovery::sessionId() const {
  return m_state->store.sessionId();
}

void DocumentRecovery::track(QPlainTextEdit *editor, const QString &path,
                             const QString &title,
                             const FileOpenGuard::TextFormat &format) {
  if (!editor || m_state->closed)
    return;
  if (!m_documents.contains(editor)) {
    Document document;
    document.editor = editor;
    document.draft.id = Recovery::newId();
    document.draft.sessionId = sessionId();
    document.version = std::make_shared<Version>();
    m_documents.insert(editor, document);
    connect(editor->document(), &QTextDocument::contentsChange, this,
            [this, editor](int, int removed, int added) {
              if (removed || added)
                changed(editor);
            });
    connect(editor->document(), &QTextDocument::modificationChanged, this,
            [this, editor](bool modified) {
              if (modified) {
                auto it = m_documents.find(editor);
                if (it != m_documents.end())
                  it->pending = true;
                updateTimer();
              } else
                clear(editor);
            });
    // Destruction alone is not a discard instruction. A window may be torn down
    // after an error; only the actual tab-close path calls forget().
    connect(editor, &QObject::destroyed, this, [this, editor]() {
      m_documents.remove(editor);
      updateTimer();
    });
  }
  auto &document = m_documents[editor];
  if (document.draft.originalPath != path || document.draft.title != title ||
      document.draft.format.encoding != format.encoding ||
      document.draft.format.hasBom != format.hasBom ||
      document.draft.format.lineEnding != format.lineEnding) {
    document.pending |= editor->document()->isModified();
    document.draft.originalPath = path;
    document.draft.title = title;
    document.draft.format = format;
  }
  document.pending |= editor->document()->isModified();
  updateTimer();
}
void DocumentRecovery::changed(QPlainTextEdit *editor) {
  auto it = m_documents.find(editor);
  if (it == m_documents.end() || !it->editor || m_state->closed)
    return;
  if (editor->document()->isModified())
    it->pending = true;
  updateTimer();
}
void DocumentRecovery::clear(QPlainTextEdit *editor) {
  auto it = m_documents.find(editor);
  if (it == m_documents.end() || m_state->closed)
    return;
  it->pending = false;
  it->lastError.clear();
  if (it->hasSnapshot)
    removeSnapshot(it->draft, it->version);
  it->hasSnapshot = false;
  updateTimer();
}
void DocumentRecovery::forget(QPlainTextEdit *editor) {
  auto it = m_documents.find(editor);
  if (it == m_documents.end())
    return;
  const auto draft = it->draft;
  const auto version = it->version;
  const bool hasSnapshot = it->hasSnapshot;
  m_documents.erase(it);
  if (hasSnapshot)
    removeSnapshot(draft, version);
  updateTimer();
}
void DocumentRecovery::removeSnapshot(const Recovery::Draft &draft,
                                      const std::shared_ptr<Version> &version) {
  const int serial = ++version->value;
  const auto state = m_state;
  auto error = std::make_shared<QString>();
  auto *task =
      new AsyncTask([state, version, serial, draft, error](AsyncTask *) {
        QMutexLocker lock(&state->mutex);
        if (state->closed || version->value != serial)
          return;
        state->store.remove(draft.id, error.get());
      });
  connect(task, &AsyncTask::finished, this, [this, error]() {
    if (!error->isEmpty())
      emit recoveryError(*error);
  });
  AsyncThreadPool::instance().submit(task);
}
void DocumentRecovery::updateTimer() {
  bool pending = false;
  for (const auto &document : m_documents)
    pending |= document.pending;
  if (pending && !m_timer->isActive() && !m_state->closed &&
      m_initializationError.isEmpty())
    m_timer->start();
  else if (!pending)
    m_timer->stop();
}
void DocumentRecovery::checkpoint() {
  if (m_state->closed || !m_initializationError.isEmpty() ||
      m_checkpointRunning)
    return;
  struct Job {
    QPointer<QPlainTextEdit> editor;
    Recovery::Draft draft;
    std::shared_ptr<Version> version;
    int serial = 0;
    bool saved = false;
    QString error;
  };
  auto jobs = std::make_shared<QVector<Job>>();
  for (auto it = m_documents.begin(); it != m_documents.end(); ++it) {
    auto &document = it.value();
    if (!document.pending || !document.editor ||
        !document.editor->document()->isModified())
      continue;
    Recovery::Draft draft = document.draft;
    draft.text = document.editor->toPlainText();
    draft.cursor = document.editor->textCursor().position();
    draft.updated = QDateTime::currentDateTimeUtc();
    document.hasSnapshot = true;
    jobs->append({document.editor,
                  std::move(draft),
                  document.version,
                  ++document.version->value,
                  false,
                  {}});
  }
  if (jobs->isEmpty()) {
    updateTimer();
    return;
  }
  m_checkpointRunning = true;
  const auto state = m_state;
  auto *task = new AsyncTask([state, jobs](AsyncTask *) {
    for (auto &job : *jobs) {
      QMutexLocker lock(&state->mutex);
      if (state->closed)
        break;
      if (job.version->value != job.serial)
        continue;
      job.saved = state->store.save(job.draft, &job.error);
    }
  });
  auto completed = [this, jobs]() {
    m_checkpointRunning = false;
    for (const auto &job : *jobs) {
      if (!job.editor)
        continue;
      auto current = m_documents.find(job.editor);
      if (current == m_documents.end() || current->draft.id != job.draft.id ||
          job.version->value != job.serial)
        continue;
      if (job.saved) {
        current->pending =
            job.editor->document()->isModified() &&
            (job.editor->toPlainText() != job.draft.text ||
             current->draft.originalPath != job.draft.originalPath ||
             current->draft.title != job.draft.title ||
             current->draft.format.encoding != job.draft.format.encoding ||
             current->draft.format.hasBom != job.draft.format.hasBom ||
             current->draft.format.lineEnding != job.draft.format.lineEnding);
        current->lastError.clear();
        emit checkpointed(job.draft.id);
      } else if (!job.error.isEmpty() && current->lastError != job.error) {
        current->lastError = job.error;
        emit recoveryError(job.error);
      }
    }
    updateTimer();
  };
  connect(task, &AsyncTask::finished, this, completed);
  connect(task, &AsyncTask::error, this,
          [jobs, completed](const QString &message) {
            for (auto &job : *jobs)
              if (!job.saved)
                job.error = message;
            completed();
          });
  AsyncThreadPool::instance().submit(task);
  updateTimer();
}
Recovery::Scan DocumentRecovery::recoverable() const {
  if (!m_initializationError.isEmpty())
    return {{}, {m_initializationError}};
  QMutexLocker lock(&m_state->mutex);
  return m_state->store.scan();
}
bool DocumentRecovery::adopt(const Recovery::Draft &draft,
                             QPlainTextEdit *editor, QString *error) {
  auto it = m_documents.find(editor);
  if (it == m_documents.end() || !editor || editor->toPlainText() != draft.text)
    return false;
  ++it->version->value;
  Recovery::Draft adopted;
  {
    QMutexLocker lock(&m_state->mutex);
    if (!m_state->store.transfer(draft, it->draft.id, &adopted, error))
      return false;
  }
  it->draft = adopted;
  it->hasSnapshot = true;
  editor->document()->setModified(true);
  it->pending = false;
  return true;
}
bool DocumentRecovery::discard(const Recovery::Draft &draft, QString *error) {
  QMutexLocker lock(&m_state->mutex);
  return m_state->store.discard(draft, error);
}
bool DocumentRecovery::finishSession(QString *error) {
  m_timer->stop();
  m_state->closed = true;
  QMutexLocker lock(&m_state->mutex);
  return m_state->store.finish(error);
}
