#include "filequickopen.h"
#include "../../core/async/asyncworker.h"
#include "../../filetree/projectsearch.h"
#include "../uistylehelper.h"
#include <QDir>
#include <QFileInfo>
#include <QHideEvent>
#include <QLabel>
#include <QTimer>

FileQuickOpen::FileQuickOpen(QWidget *parent)
    : StyledPopupDialog(parent), m_searchBox(nullptr), m_resultsList(nullptr),
      m_layout(nullptr) {
  setupUI();
}

FileQuickOpen::~FileQuickOpen() {
  if (m_cancelled)
    m_cancelled->store(true);
}

void FileQuickOpen::setupUI() {
  setMinimumWidth(600);
  setMaximumHeight(450);

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(8, 8, 8, 8);
  m_layout->setSpacing(4);

  m_searchBox = new QLineEdit(this);
  m_searchBox->setObjectName("quickOpenSearch");
  m_searchBox->setAccessibleName(tr("Search project files"));
  m_searchBox->setPlaceholderText(tr("Search files by name..."));
  m_layout->addWidget(m_searchBox);
  m_status = new QLabel(this);
  m_status->setObjectName("quickOpenStatus");
  m_status->setTextFormat(Qt::PlainText);
  m_layout->addWidget(m_status);

  m_resultsList = new QListWidget(this);
  m_resultsList->setObjectName("quickOpenResults");
  m_resultsList->setAccessibleName(tr("Matching project files"));
  m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_layout->addWidget(m_resultsList);

  connect(m_searchBox, &QLineEdit::textChanged, this,
          &FileQuickOpen::onSearchTextChanged);
  connect(m_resultsList, &QListWidget::itemClicked, this,
          &FileQuickOpen::onItemClicked);

  m_searchBox->installEventFilter(this);
  m_searchTimer = new QTimer(this);
  m_searchTimer->setSingleShot(true);
  m_searchTimer->setInterval(120);
  connect(m_searchTimer, &QTimer::timeout, this, &FileQuickOpen::runSearch);
}

void FileQuickOpen::setRootDirectory(const QString &path) {
  if (m_rootPath == path)
    return;
  ++m_request;
  if (m_cancelled)
    m_cancelled->store(true);
  m_searchTimer->stop();
  m_resultsList->clear();
  m_filteredFiles.clear();
  m_rootPath = path;
  if (isVisible())
    updateResults(m_searchBox->text());
}

void FileQuickOpen::showDialog() {
  m_searchBox->clear();
  updateResults(QString());

  showCentered();
  m_searchBox->setFocus();

  if (m_resultsList->count() > 0) {
    m_resultsList->setCurrentRow(0);
  }
}

void FileQuickOpen::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (m_resultsList->currentRow() >= 0) {
      selectFile(m_resultsList->currentRow());
    }
    break;
  case Qt::Key_Up:
    selectPrevious();
    break;
  case Qt::Key_Down:
    selectNext();
    break;
  default:
    StyledPopupDialog::keyPressEvent(event);
  }
}

bool FileQuickOpen::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_searchBox && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    switch (keyEvent->key()) {
    case Qt::Key_Up:
      selectPrevious();
      return true;
    case Qt::Key_Down:
      selectNext();
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      if (m_resultsList->currentRow() >= 0) {
        selectFile(m_resultsList->currentRow());
      }
      return true;
    }
  }
  return StyledPopupDialog::eventFilter(obj, event);
}

void FileQuickOpen::onSearchTextChanged(const QString &text) {
  updateResults(text);
}

void FileQuickOpen::onItemActivated(QListWidgetItem *item) {
  int row = m_resultsList->row(item);
  if (row >= 0) {
    selectFile(row);
  }
}

void FileQuickOpen::onItemClicked(QListWidgetItem *item) {
  onItemActivated(item);
}

void FileQuickOpen::updateResults(const QString &) {
  ++m_request;
  if (m_cancelled)
    m_cancelled->store(true);
  m_resultsList->clear();
  m_filteredFiles.clear();
  m_status->setText(tr("Searching project files…"));
  m_searchTimer->start();
}

void FileQuickOpen::runSearch() {
  const int request = m_request;
  auto token = std::make_shared<std::atomic_bool>(false);
  m_cancelled = token;
  ProjectSearch::Options options;
  options.rootPath = m_rootPath;
  options.query = m_searchBox->text().trimmed().isEmpty() ? QStringLiteral("*")
                                                          : m_searchBox->text();
  options.includeDirectories = false;
  options.maxResults = 50;
  auto results = std::make_shared<ProjectSearch::Results>();
  auto *task = new AsyncTask([options, token, results](AsyncTask *) {
    *results =
        ProjectSearch::search(options, [token]() { return token->load(); });
  });
  connect(task, &AsyncWorker::finished, this, [this, request, results]() {
    if (request != m_request)
      return;
    m_cancelled.reset();
    m_resultsList->clear();
    m_filteredFiles.clear();
    for (const auto &match : results->matches) {
      m_filteredFiles.append(match.relativePath);
      auto *item = new QListWidgetItem(m_resultsList);
      item->setText(QFileInfo(match.relativePath).fileName() +
                    QStringLiteral("  ·  ") +
                    QFileInfo(match.relativePath).path());
      item->setToolTip(match.filePath);
      item->setData(Qt::UserRole, match.relativePath);
    }
    m_status->setText(
        !results->error.isEmpty()    ? results->error
        : results->matches.isEmpty() ? tr("No matching files")
        : results->truncated
            ? tr("Showing the first %n files · refine your search", nullptr,
                 results->matches.size())
        : results->matches.size() == 1
            ? tr("1 file · Enter to open")
            : tr("%n files · Enter to open", nullptr, results->matches.size()));
    if (m_resultsList->count())
      m_resultsList->setCurrentRow(0);
    setFixedHeight(qBound(160, m_resultsList->count() * 35 + 90, 450));
  });
  connect(task, &AsyncWorker::error, this,
          [this, request](const QString &error) {
            if (request == m_request)
              m_status->setText(error);
          });
  AsyncThreadPool::instance().submit(task);
}

void FileQuickOpen::hideEvent(QHideEvent *event) {
  ++m_request;
  m_searchTimer->stop();
  if (m_cancelled)
    m_cancelled->store(true);
  StyledPopupDialog::hideEvent(event);
}

void FileQuickOpen::selectFile(int row) {
  if (row < 0 || row >= m_filteredFiles.size())
    return;

  QString relativePath = m_filteredFiles[row];
  QString fullPath =
      QDir::cleanPath(m_rootPath + QDir::separator() + relativePath);
  const QFileInfo file(fullPath);
  if (!file.isFile() || !file.isReadable()) {
    m_filteredFiles.removeAt(row);
    delete m_resultsList->takeItem(row);
    m_status->setText(tr("\"%1\" is no longer available. Choose another file "
                         "or search again.").arg(file.fileName()));
    return;
  }

  hide();

  emit fileSelected(fullPath);
}

void FileQuickOpen::selectNext() {
  int current = m_resultsList->currentRow();
  if (current < m_resultsList->count() - 1) {
    m_resultsList->setCurrentRow(current + 1);
  }
}

void FileQuickOpen::selectPrevious() {
  int current = m_resultsList->currentRow();
  if (current > 0) {
    m_resultsList->setCurrentRow(current - 1);
  }
}

void FileQuickOpen::applyTheme(const Theme &theme) {
  StyledPopupDialog::applyTheme(theme);
}
