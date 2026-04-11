#include "filequickopen.h"
#include "../uistylehelper.h"
#include <QDirIterator>
#include <algorithm>

FileQuickOpen::FileQuickOpen(QWidget *parent)
    : StyledPopupDialog(parent),
      m_searchBox(nullptr), m_resultsList(nullptr), m_layout(nullptr) {
  setupUI();
}

FileQuickOpen::~FileQuickOpen() {}

void FileQuickOpen::setupUI() {
  setMinimumWidth(600);
  setMaximumHeight(450);

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(8, 8, 8, 8);
  m_layout->setSpacing(4);

  m_searchBox = new QLineEdit(this);
  m_searchBox->setPlaceholderText(tr("Search files by name..."));
  m_layout->addWidget(m_searchBox);

  m_resultsList = new QListWidget(this);
  m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_layout->addWidget(m_resultsList);

  connect(m_searchBox, &QLineEdit::textChanged, this,
          &FileQuickOpen::onSearchTextChanged);
  connect(m_resultsList, &QListWidget::itemActivated, this,
          &FileQuickOpen::onItemActivated);
  connect(m_resultsList, &QListWidget::itemClicked, this,
          &FileQuickOpen::onItemClicked);

  m_searchBox->installEventFilter(this);
}

void FileQuickOpen::setRootDirectory(const QString &path) {
  m_rootPath = path;
  scanDirectory();
}

void FileQuickOpen::scanDirectory() {
  m_allFiles.clear();

  if (m_rootPath.isEmpty() || !QDir(m_rootPath).exists()) {
    return;
  }

  QDirIterator it(m_rootPath, QDir::Files, QDirIterator::Subdirectories);

  QStringList skipDirs = {".git", "node_modules", "build",
                          "dist", ".cache",       "__pycache__"};
  QString sep = QDir::separator();

  while (it.hasNext()) {
    QString filePath = it.next();

    bool shouldSkip = false;
    for (const QString &skipDir : skipDirs) {

      if (filePath.contains(sep + skipDir + sep) ||
          filePath.endsWith(sep + skipDir)) {
        shouldSkip = true;
        break;
      }

      if (filePath.contains("/" + skipDir + "/") ||
          filePath.endsWith("/" + skipDir)) {
        shouldSkip = true;
        break;
      }
    }

    if (!shouldSkip) {

      QString relativePath = filePath.mid(m_rootPath.length());
      if (relativePath.startsWith('/') || relativePath.startsWith('\\') ||
          relativePath.startsWith(sep)) {
        relativePath = relativePath.mid(1);
      }
      m_allFiles.append(relativePath);
    }
  }

  m_allFiles.sort(Qt::CaseInsensitive);
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

void FileQuickOpen::updateResults(const QString &query) {
  m_resultsList->clear();
  m_filteredFiles.clear();

  QList<QPair<int, QString>> scored;

  for (const QString &file : m_allFiles) {
    int score = 0;
    if (query.isEmpty()) {
      score = 1000;
    } else {

      QString fileName = QFileInfo(file).fileName();
      score = fuzzyMatch(query.toLower(), fileName.toLower());

      if (score == 0) {
        score = fuzzyMatch(query.toLower(), file.toLower()) / 2;
      }
    }

    if (score > 0) {
      scored.append(qMakePair(score, file));
    }
  }

  std::sort(scored.begin(), scored.end(),
            [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
              return a.first > b.first;
            });

  int maxResults = 20;
  for (int i = 0; i < qMin(scored.size(), maxResults); ++i) {
    QString filePath = scored[i].second;
    m_filteredFiles.append(filePath);

    QListWidgetItem *item = new QListWidgetItem();

    QString fileName = QFileInfo(filePath).fileName();
    QString dirPath = QFileInfo(filePath).path();

    QString displayText = fileName;
    if (!dirPath.isEmpty() && dirPath != ".") {
      displayText += "  (" + dirPath + ")";
    }

    item->setText(displayText);
    item->setData(Qt::UserRole, filePath);
    m_resultsList->addItem(item);
  }

  if (m_resultsList->count() > 0) {
    m_resultsList->setCurrentRow(0);
  }

  int itemHeight = 35;
  int newHeight = qMin(m_resultsList->count() * itemHeight + 60, 450);
  setFixedHeight(newHeight);
}

int FileQuickOpen::fuzzyMatch(const QString &pattern, const QString &text) {
  if (pattern.isEmpty())
    return 1000;

  if (text.contains(pattern))
    return 2000 + (1000 - text.indexOf(pattern));

  int patternIdx = 0;
  int score = 0;
  int lastMatchIdx = -1;

  for (int i = 0; i < text.length() && patternIdx < pattern.length(); ++i) {
    if (text[i] == pattern[patternIdx]) {

      if (lastMatchIdx == i - 1) {
        score += 15;
      }

      if (i == 0 || text[i - 1] == '/' || text[i - 1] == '\\' ||
          text[i - 1] == '.' || text[i - 1] == '_' || text[i - 1] == '-') {
        score += 10;
      }
      score += 10;
      lastMatchIdx = i;
      patternIdx++;
    }
  }

  if (patternIdx != pattern.length())
    return 0;

  return score;
}

void FileQuickOpen::selectFile(int row) {
  if (row < 0 || row >= m_filteredFiles.size())
    return;

  QString relativePath = m_filteredFiles[row];
  QString fullPath =
      QDir::cleanPath(m_rootPath + QDir::separator() + relativePath);

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
