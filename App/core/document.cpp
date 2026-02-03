#include "document.h"
#include "io/filemanager.h"
#include "logging/logger.h"

#include <QFileInfo>
#include <QHash>

Document::Document(QObject *parent)
    : QObject(parent), m_state(State::New), m_lastModified(QDateTime()) {}

Document::Document(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath), m_state(State::New),
      m_lastModified(QDateTime()) {
  if (!filePath.isEmpty()) {
    load();
  }
}

QString Document::content() const { return m_content; }

void Document::setContent(const QString &content) {
  if (m_content != content) {
    m_content = content;
    markAsModified();
    emit contentChanged();
  }
}

QString Document::filePath() const { return m_filePath; }

void Document::setFilePath(const QString &path) {
  if (m_filePath != path) {
    m_filePath = path;
    emit filePathChanged(path);
  }
}

QString Document::fileName() const {
  if (m_filePath.isEmpty()) {
    return "Untitled";
  }
  return QFileInfo(m_filePath).fileName();
}

QString Document::fileExtension() const {
  return QFileInfo(m_filePath).completeSuffix();
}

Document::State Document::state() const { return m_state; }

bool Document::isModified() const { return m_state == State::Modified; }

bool Document::isNew() const {
  return m_state == State::New || m_filePath.isEmpty();
}

void Document::markAsSaved() {
  bool wasModified = isModified();
  updateState(State::Saved);
  m_lastModified = QDateTime::currentDateTime();
  if (wasModified) {
    emit modificationStateChanged(false);
  }
}

void Document::markAsModified() {
  if (m_state != State::Modified) {
    updateState(State::Modified);
    emit modificationStateChanged(true);
  }
}

bool Document::load() {
  if (m_filePath.isEmpty()) {
    QString errorMsg = "Cannot load document: no file path specified";
    LOG_WARNING(errorMsg);
    emit error(errorMsg);
    return false;
  }

  FileManager::FileResult result = FileManager::instance().readFile(m_filePath);

  if (!result.success) {
    updateState(State::Error);
    emit error(result.errorMessage);
    return false;
  }

  m_content = result.content;
  m_lastModified = QFileInfo(m_filePath).lastModified();
  updateState(State::Saved);

  LOG_INFO(QString("Document loaded: %1").arg(m_filePath));
  emit loaded();
  return true;
}

bool Document::save() {
  if (m_filePath.isEmpty()) {
    QString errorMsg = "Cannot save document: no file path specified";
    LOG_WARNING(errorMsg);
    emit error(errorMsg);
    return false;
  }

  FileManager::FileResult result =
      FileManager::instance().writeFile(m_filePath, m_content);

  if (!result.success) {
    updateState(State::Error);
    emit error(result.errorMessage);
    return false;
  }

  markAsSaved();
  LOG_INFO(QString("Document saved: %1").arg(m_filePath));
  emit saved();
  return true;
}

bool Document::saveAs(const QString &path) {
  QString oldPath = m_filePath;
  m_filePath = path;

  if (save()) {
    emit filePathChanged(path);
    return true;
  }

  // Restore old path on failure
  m_filePath = oldPath;
  return false;
}

QDateTime Document::lastModified() const { return m_lastModified; }

QString Document::languageHint() const { return detectLanguage(); }

void Document::updateState(State newState) {
  if (m_state != newState) {
    m_state = newState;
  }
}

QString Document::detectLanguage() const {
  QString ext = fileExtension().toLower();

  // Map extensions to language identifiers
  static const QHash<QString, QString> extensionMap = {{"cpp", "cpp"},
                                                       {"cc", "cpp"},
                                                       {"cxx", "cpp"},
                                                       {"c", "cpp"},
                                                       {"h", "cpp"},
                                                       {"hpp", "cpp"},
                                                       {"hxx", "cpp"},
                                                       {"py", "py"},
                                                       {"python", "py"},
                                                       {"js", "js"},
                                                       {"javascript", "js"},
                                                       {"ts", "js"},
                                                       {"jsx", "js"},
                                                       {"tsx", "js"},
                                                       {"java", "java"},
                                                       {"rb", "ruby"},
                                                       {"go", "go"},
                                                       {"rs", "rust"},
                                                       {"php", "php"},
                                                       {"sh", "bash"},
                                                       {"bash", "bash"},
                                                       {"zsh", "bash"},
                                                       {"html", "html"},
                                                       {"htm", "html"},
                                                       {"css", "css"},
                                                       {"scss", "css"},
                                                       {"sass", "css"},
                                                       {"json", "json"},
                                                       {"xml", "xml"},
                                                       {"md", "markdown"},
                                                       {"markdown", "markdown"},
                                                       {"sql", "sql"},
                                                       {"yaml", "yaml"},
                                                       {"yml", "yaml"}};

  return extensionMap.value(ext, "text");
}
