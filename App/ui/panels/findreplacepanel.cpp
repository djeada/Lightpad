#include "findreplacepanel.h"
#include "../../core/textarea.h"
#include "../../core/lightpadtabwidget.h"
#include "../mainwindow.h"
#include "ui_findreplacepanel.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHeaderView>
#include <QApplication>
#include <QEventLoop>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QShortcut>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QTimer>
#include <QTextDocument>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int kDataRoleFilePath = Qt::UserRole;
constexpr int kDataRoleLineNumber = Qt::UserRole + 1;
constexpr int kDataRoleColumnNumber = Qt::UserRole + 2;
constexpr int kDataRoleMatchStart = Qt::UserRole + 3;
constexpr int kDataRoleMatchLength = Qt::UserRole + 4;
constexpr int kDataRoleResultScope = Qt::UserRole + 5;
constexpr int kScopeLocal = 1;
constexpr int kScopeGlobal = 2;

void setModeLayoutVisible(Ui::FindReplacePanel *ui, bool visible) {
  if (!ui) {
    return;
  }
  ui->modeLabel->setVisible(visible);
  ui->localMode->setVisible(visible);
  ui->globalMode->setVisible(visible);
  if (ui->modeSpacer) {
    if (visible) {
      ui->modeSpacer->changeSize(40, 20, QSizePolicy::Expanding,
                                 QSizePolicy::Minimum);
    } else {
      ui->modeSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
  }
  if (ui->modeLayout) {
    ui->modeLayout->invalidate();
  }
}
} // namespace

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget *parent)
    : QWidget(parent), document(nullptr), textArea(nullptr),
      mainWindow(nullptr), ui(new Ui::FindReplacePanel), onlyFind(onlyFind),
      m_vimCommandMode(false), position(-1), globalResultIndex(-1),
      resultsTree(nullptr), searchHistoryIndex(-1),
      refreshTimer(new QTimer(this)), searchStatusLabel(nullptr),
      searchInProgress(false), searchExecuted(false) {
  ui->setupUi(this);

  show();

  ui->searchFind->installEventFilter(this);
  connect(ui->searchFind, &QLineEdit::textChanged, this,
          &FindReplacePanel::onSearchTextChanged);

  auto *replaceShortcut = new QShortcut(QKeySequence::Replace, this);
  replaceShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(replaceShortcut, &QShortcut::activated, this, [this]() {
    if (m_vimCommandMode) {
      return;
    }
    setGlobalMode(false);
    setOnlyFind(false);
    setReplaceVisibility(true);
    setFocusOnSearchBox();
  });

  auto *findShortcut = new QShortcut(QKeySequence::Find, this);
  findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(findShortcut, &QShortcut::activated, this, [this]() {
    if (m_vimCommandMode) {
      return;
    }
    setGlobalMode(false);
    setOnlyFind(true);
    setReplaceVisibility(false);
    setFocusOnSearchBox();
  });

  ui->options->setVisible(false);
  setReplaceVisibility(onlyFind);

  resultsTree = new QTreeWidget(this);
  resultsTree->setHeaderLabels(QStringList() << "File" << "Line" << "Match");
  resultsTree->setColumnCount(3);
  resultsTree->header()->setStretchLastSection(true);
  resultsTree->setVisible(false);
  resultsTree->setMinimumHeight(150);

  searchStatusLabel = new QLabel(this);
  searchStatusLabel->setVisible(false);

  if (layout()) {
    layout()->addWidget(searchStatusLabel);
    layout()->addWidget(resultsTree);
  }

  connect(resultsTree, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem *item, int column) {
            if (!item) {
              return;
            }
            int scope = item->data(0, kDataRoleResultScope).toInt();
            if (scope == kScopeGlobal) {
              onGlobalResultClicked(item, column);
            } else if (scope == kScopeLocal) {
              onLocalResultClicked(item, column);
            } else if (isGlobalMode()) {
              onGlobalResultClicked(item, column);
            } else {
              onLocalResultClicked(item, column);
            }
          });

  refreshTimer->setSingleShot(true);
  connect(refreshTimer, &QTimer::timeout, this,
          &FindReplacePanel::refreshSearchResults);

  updateModeUI();
  updateCounterLabels();
}

FindReplacePanel::~FindReplacePanel() { delete ui; }

void FindReplacePanel::setReplaceVisibility(bool flag) {
  ui->widget->setVisible(flag);
  ui->replaceSingle->setVisible(flag);
  ui->replaceAll->setVisible(flag);

  ui->preserveCase->setVisible(flag);
}

bool FindReplacePanel::isOnlyFind() { return onlyFind; }

bool FindReplacePanel::isOnlyFind() const { return onlyFind; }

void FindReplacePanel::setOnlyFind(bool flag) { onlyFind = flag; }

void FindReplacePanel::setDocument(QTextDocument *doc) { document = doc; }

void FindReplacePanel::setTextArea(TextArea *area) {
  if (textAreaContentsChangedConnection) {
    disconnect(textAreaContentsChangedConnection);
  }

  textArea = area;
  lastObservedPlainText = textArea ? textArea->toPlainText() : QString();

  if (textArea && textArea->document()) {
    textAreaContentsChangedConnection = connect(
        textArea, &QPlainTextEdit::textChanged, this,
        &FindReplacePanel::onTextAreaContentsChanged);
  }

  if (!isVisible() || m_vimCommandMode || !ui) {
    return;
  }

  const QString currentSearch = ui->searchFind->text();
  if (currentSearch.isEmpty()) {
    if (!isGlobalMode()) {
      positions.clear();
      position = -1;
      if (resultsTree) {
        resultsTree->clear();
        resultsTree->setVisible(false);
      }
      updateCounterLabels();
    }
    return;
  }

  if (!textArea && !isGlobalMode()) {
    positions.clear();
    position = -1;
    if (resultsTree) {
      resultsTree->clear();
      resultsTree->setVisible(false);
    }
    updateCounterLabels();
    return;
  }

  searchExecuted = true;
  activeSearchWord = currentSearch;
  if (!isGlobalMode() && textArea) {
    beginSearchFeedback(QString("Searching current file..."));
    QTextCursor cursor(textArea->document());
    findInitial(cursor, currentSearch);
    endSearchFeedback(positions.size());
    updateCounterLabels();
    return;
  }

  if (refreshTimer) {
    refreshTimer->start(0);
  }
}

void FindReplacePanel::setMainWindow(MainWindow *window) {
  mainWindow = window;
}

void FindReplacePanel::setProjectPath(const QString &path) {
  projectPath = path;
}

void FindReplacePanel::setGlobalMode(bool enabled) {
  if (!ui || !ui->globalMode || !ui->localMode) {
    return;
  }
  if (enabled) {
    if (!ui->globalMode->isChecked()) {
      ui->globalMode->setChecked(true);
    } else {
      updateModeUI();
    }
  } else {
    if (!ui->localMode->isChecked()) {
      ui->localMode->setChecked(true);
    } else {
      updateModeUI();
    }
  }
}

void FindReplacePanel::setFocusOnSearchBox() { ui->searchFind->setFocus(); }

void FindReplacePanel::setVimCommandMode(bool enabled) {
  if (m_vimCommandMode == enabled) {
    return;
  }
  m_vimCommandMode = enabled;
  if (enabled) {
    setOnlyFind(true);
    setReplaceVisibility(false);
    setModeLayoutVisible(ui, false);
    ui->options->setVisible(false);
    ui->more->setVisible(false);
    ui->findPrevious->setVisible(false);
    ui->find->setVisible(false);
    ui->replaceSingle->setVisible(false);
    ui->replaceAll->setVisible(false);
    ui->currentIndex->setVisible(false);
    ui->totalFound->setVisible(false);
    ui->label->setVisible(false);
    ui->searchBackward->setChecked(false);
    ui->searchStart->setChecked(true);
  } else {
    setModeLayoutVisible(ui, true);
    ui->more->setVisible(true);
    ui->findPrevious->setVisible(true);
    ui->find->setVisible(true);
    ui->currentIndex->setVisible(true);
    ui->totalFound->setVisible(true);
    ui->label->setVisible(true);
    ui->findWhat->setText("Find what :");
  }
}

bool FindReplacePanel::isVimCommandMode() const { return m_vimCommandMode; }

void FindReplacePanel::setSearchPrefix(const QString &prefix) {
  m_searchPrefix = prefix;
  ui->findWhat->setText(QString("Command (%1):").arg(prefix));
}

void FindReplacePanel::setSearchText(const QString &text) {
  ui->searchFind->setText(text);
  ui->searchFind->setCursorPosition(text.length());
}

bool FindReplacePanel::eventFilter(QObject *obj, QEvent *event) {
  if (obj == ui->searchFind && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (!m_vimCommandMode &&
        (keyEvent->modifiers() & Qt::ControlModifier) &&
        keyEvent->key() == Qt::Key_R) {
      setGlobalMode(false);
      setOnlyFind(false);
      setReplaceVisibility(true);
      return true;
    }
    if (!m_vimCommandMode &&
        (keyEvent->modifiers() & Qt::ControlModifier) &&
        keyEvent->key() == Qt::Key_F) {
      setGlobalMode(false);
      setOnlyFind(true);
      setReplaceVisibility(false);
      return true;
    }
    if (m_vimCommandMode) {
      handleVimCommandKey(keyEvent);
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void FindReplacePanel::handleVimCommandKey(QKeyEvent *event) {
  if (!textArea || !textArea->vimMode() || !textArea->isVimModeEnabled()) {
    return;
  }

  VimMode *vimMode = textArea->vimMode();
  const int key = event->key();
  const Qt::KeyboardModifiers mods = event->modifiers();
  const QString text = event->text();
  const QString prefix =
      m_searchPrefix.isEmpty() ? QString(":") : m_searchPrefix;

  if ((mods & Qt::ControlModifier) &&
      (key == Qt::Key_C || key == Qt::Key_BracketLeft)) {
    QKeyEvent escEvent(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    vimMode->processKeyEvent(&escEvent);
    return;
  }

  if (key == Qt::Key_Escape) {
    vimMode->processKeyEvent(event);
    return;
  }

  if (key == Qt::Key_Return || key == Qt::Key_Enter) {
    QString command = prefix + ui->searchFind->text();
    for (int i = 0; i < command.size(); ++i) {
      QKeyEvent cmdEvent(QEvent::KeyPress, 0, Qt::NoModifier,
                         command.mid(i, 1));
      vimMode->processKeyEvent(&cmdEvent);
    }
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    vimMode->processKeyEvent(&enterEvent);
    return;
  }

  if (key == Qt::Key_Backspace) {
    QKeyEvent backspaceEvent(QEvent::KeyPress, Qt::Key_Backspace,
                             Qt::NoModifier);
    vimMode->processKeyEvent(&backspaceEvent);
    return;
  }

  if (key == Qt::Key_Up || key == Qt::Key_Down) {
    vimMode->processKeyEvent(event);
    return;
  }

  if (!text.isEmpty()) {
    QKeyEvent textEvent(QEvent::KeyPress, 0, Qt::NoModifier, text);
    vimMode->processKeyEvent(&textEvent);
  }
}

bool FindReplacePanel::isGlobalMode() const {
  return ui->globalMode->isChecked();
}

void FindReplacePanel::on_localMode_toggled(bool checked) {
  if (checked) {
    updateModeUI();
  }
}

void FindReplacePanel::on_globalMode_toggled(bool checked) {
  if (checked) {
    updateModeUI();
  }
}

void FindReplacePanel::updateModeUI() {
  bool isGlobal = ui->globalMode->isChecked();

  if (resultsTree) {
    if (isGlobal) {
      resultsTree->setVisible(!globalResults.isEmpty());
    } else {
      resultsTree->setVisible(!positions.isEmpty());
    }
  }

  ui->searchStart->setEnabled(!isGlobal);
  ui->searchBackward->setEnabled(!isGlobal);

  if (isGlobal) {
    positions.clear();
    position = -1;
  } else {
    globalResults.clear();
    globalResultsByFile.clear();
    globalResultIndex = -1;
  }

  if (resultsTree) {
    resultsTree->clear();
    resultsTree->setVisible(false);
  }

  clearSearchFeedback();
  updateCounterLabels();
}

void FindReplacePanel::on_more_clicked() {
  ui->options->setVisible(!ui->wholeWords->isVisible());
}

QRegularExpression
FindReplacePanel::buildSearchPattern(const QString &searchWord) const {
  QString pattern = searchWord;

  if (!ui->useRegex->isChecked()) {
    pattern = QRegularExpression::escape(pattern);
  }

  if (ui->wholeWords->isChecked()) {
    pattern = "\\b" + pattern + "\\b";
  }

  QRegularExpression::PatternOptions options =
      QRegularExpression::NoPatternOption;

  if (!ui->matchCase->isChecked()) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  return QRegularExpression(pattern, options);
}

QString FindReplacePanel::applyPreserveCase(const QString &replaceWord,
                                            const QString &matchedText) const {
  if (!ui->preserveCase->isChecked() || matchedText.isEmpty()) {
    return replaceWord;
  }

  QString result = replaceWord;

  bool allUpper = true;
  bool allLower = true;
  bool firstUpper = false;

  const int textLength = matchedText.length();
  for (int i = 0; i < textLength; ++i) {
    QChar c = matchedText.at(i);
    if (c.isLetter()) {
      if (c.isUpper()) {
        allLower = false;
        if (i == 0)
          firstUpper = true;
      } else {
        allUpper = false;
      }
    }
  }

  if (allUpper && !allLower) {

    return result.toUpper();
  } else if (allLower && !allUpper) {

    return result.toLower();
  } else if (firstUpper && textLength > 1) {

    result = result.toLower();
    if (!result.isEmpty()) {
      result[0] = result[0].toUpper();
    }
    return result;
  }

  return replaceWord;
}

void FindReplacePanel::addToSearchHistory(const QString &searchTerm) {
  if (searchTerm.isEmpty()) {
    return;
  }

  searchHistory.removeAll(searchTerm);

  searchHistory.prepend(searchTerm);

  while (searchHistory.size() > MAX_SEARCH_HISTORY) {
    searchHistory.removeLast();
  }

  searchHistoryIndex = -1;
}

void FindReplacePanel::on_find_clicked() {
  if (m_vimCommandMode) {
    return;
  }
  QString searchWord = ui->searchFind->text();

  if (searchWord.isEmpty()) {
    return;
  }

  addToSearchHistory(searchWord);
  searchExecuted = true;
  activeSearchWord = searchWord;

  if (isGlobalMode()) {
    performGlobalSearch(searchWord);
    return;
  }

  if (textArea) {
    textArea->setFocus();
    QTextCursor newCursor(textArea->document());

    if (textArea->getSearchWord() != searchWord) {
      findInitial(newCursor, searchWord);
    } else {
      findNext(newCursor, searchWord);
    }

    updateCounterLabels();
  }
}

void FindReplacePanel::on_findPrevious_clicked() {
  if (m_vimCommandMode) {
    return;
  }
  QString searchWord = ui->searchFind->text();

  if (searchWord.isEmpty()) {
    return;
  }

  addToSearchHistory(searchWord);
  searchExecuted = true;
  activeSearchWord = searchWord;

  if (isGlobalMode()) {
    if (!globalResults.isEmpty()) {
      globalResultIndex--;
      if (globalResultIndex < 0) {
        globalResultIndex = globalResults.size() - 1;
      }
      navigateToGlobalResult(globalResultIndex);
      updateCounterLabels();
    }
    return;
  }

  if (textArea) {
    textArea->setFocus();
    QTextCursor newCursor(textArea->document());

    if (textArea->getSearchWord() != searchWord) {
      findInitial(newCursor, searchWord);
    } else {
      findPrevious(newCursor, searchWord);
    }

    updateCounterLabels();
  }
}

void FindReplacePanel::on_replaceSingle_clicked() {
  if (m_vimCommandMode) {
    return;
  }
  if (textArea) {
    textArea->setFocus();
    QString searchWord = ui->searchFind->text();
    QString replaceWord = ui->fieldReplace->text();

    if (searchWord.isEmpty()) {
      return;
    }

    addToSearchHistory(searchWord);
    searchExecuted = true;
    activeSearchWord = searchWord;
    QTextCursor newCursor(textArea->document());

    if (textArea->getSearchWord() != searchWord) {
      findInitial(newCursor, searchWord);
    }

    replaceNext(newCursor, replaceWord);

    if (!positions.isEmpty()) {
      findNext(newCursor, searchWord);
    }

    updateCounterLabels();
  }
}

void FindReplacePanel::on_close_clicked() {
  if (m_vimCommandMode) {
    setVimCommandMode(false);
    if (textArea) {
      textArea->setFocus();
    }
  }
  if (textArea)
    textArea->updateSyntaxHighlightTags();

  clearSearchFeedback();
  close();
}

void FindReplacePanel::selectSearchWord(QTextCursor &cursor, int n,
                                        int offset) {
  cursor.setPosition(positions[++position] - offset);

  if (!cursor.isNull()) {
    cursor.clearSelection();
    cursor.setPosition(positions[position] - offset + n,
                       QTextCursor::KeepAnchor);
    textArea->setTextCursor(cursor);
  }
}

void FindReplacePanel::clearSelectionFormat(QTextCursor &cursor, int n) {
  if (!positions.isEmpty() && position >= 0) {
    cursor.setPosition(positions[position] + n);
    textArea->setTextCursor(cursor);
  }
}

void FindReplacePanel::replaceNext(QTextCursor &cursor,
                                   const QString &replaceWord) {
  if (!cursor.selectedText().isEmpty() && !positions.isEmpty() &&
      position >= 0) {
    QString matchedText = cursor.selectedText();
    QString finalReplacement = applyPreserveCase(replaceWord, matchedText);

    cursor.removeSelectedText();
    cursor.insertText(finalReplacement);
    textArea->setTextCursor(cursor);

    if (position < positions.size()) {
      int oldLength = matchedText.length();
      int newLength = finalReplacement.length();
      int lengthDiff = newLength - oldLength;

      positions.removeAt(position);

      for (int i = position; i < positions.size(); ++i) {
        positions[i] += lengthDiff;
      }

      if (position > 0) {
        position--;
      } else if (positions.isEmpty()) {
        position = -1;
      } else {
        position = -1;
      }
    }
  }
}

void FindReplacePanel::updateCounterLabels() {

  if (isGlobalMode()) {
    if (globalResults.isEmpty()) {
      ui->currentIndex->hide();
      ui->totalFound->hide();
      ui->label->hide();
    } else {
      if (ui->currentIndex->isHidden()) {
        ui->currentIndex->show();
        ui->totalFound->show();
        ui->label->show();
      }

      ui->currentIndex->setText(QString::number(globalResultIndex + 1));
      ui->totalFound->setText(QString::number(globalResults.size()));
    }
    return;
  }

  if (positions.isEmpty()) {
    ui->currentIndex->hide();
    ui->totalFound->hide();
    ui->label->hide();
  } else {
    if (ui->currentIndex->isHidden()) {
      ui->currentIndex->show();
      ui->totalFound->show();
      ui->label->show();
    }

    ui->currentIndex->setText(QString::number(position + 1));
    ui->totalFound->setText(QString::number(positions.size()));
  }
}

void FindReplacePanel::findInitial(QTextCursor &cursor,
                                   const QString &searchWord) {
  if (!positions.isEmpty()) {
    clearSelectionFormat(cursor, searchWord.size());
    positions.clear();
  }

  textArea->updateSyntaxHighlightTags(searchWord);

  QRegularExpression pattern = buildSearchPattern(searchWord);
  QString text = textArea->toPlainText();

  int startPos = 0;
  if (!ui->searchStart->isChecked() && textArea) {
    startPos = textArea->textCursor().position();
  }

  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);
  QVector<int> allPositions;
  QVector<int> matchLengths;

  while (matches.hasNext()) {
    QRegularExpressionMatch match = matches.next();
    allPositions.push_back(match.capturedStart());
    matchLengths.push_back(match.capturedLength());
  }

  if (ui->searchBackward->isChecked()) {

    std::reverse(allPositions.begin(), allPositions.end());
  }

  positions = allPositions;

  if (!positions.isEmpty()) {
    position = -1;

    if (!ui->searchStart->isChecked() && startPos > 0) {
      if (ui->searchBackward->isChecked()) {

        for (int i = 0; i < positions.size(); ++i) {
          if (positions[i] < startPos) {

            position = i - 1;
            break;
          }
        }

      } else {

        for (int i = 0; i < positions.size(); ++i) {
          if (positions[i] >= startPos) {
            position = i - 1;
            break;
          }
        }
      }
    }

    int matchLength = searchWord.size();
    if (!allPositions.isEmpty() && !matchLengths.isEmpty()) {
      matchLength = matchLengths.first();
    }

    selectSearchWord(cursor, matchLength);
  }

  displayLocalResults(searchWord);
}

void FindReplacePanel::findNext(QTextCursor &cursor, const QString &searchWord,
                                int offset) {
  QRegularExpression pattern = buildSearchPattern(searchWord);

  QString text = textArea->toPlainText();
  int matchLength = searchWord.size();

  if (!positions.isEmpty() && position >= 0 && position < positions.size()) {
    QRegularExpressionMatch match = pattern.match(text, positions[position]);
    if (match.hasMatch()) {
      matchLength = match.capturedLength();
    }
  }

  clearSelectionFormat(cursor, matchLength);

  if (!positions.isEmpty()) {
    if (position >= positions.size() - 1)
      position = -1;

    if (position + 1 < positions.size()) {
      QRegularExpressionMatch nextMatch =
          pattern.match(text, positions[position + 1]);
      if (nextMatch.hasMatch()) {
        matchLength = nextMatch.capturedLength();
      }
    }

    selectSearchWord(cursor, matchLength, offset);
  }
}

void FindReplacePanel::findPrevious(QTextCursor &cursor,
                                    const QString &searchWord) {
  QRegularExpression pattern = buildSearchPattern(searchWord);

  QString text = textArea->toPlainText();
  int matchLength = searchWord.size();

  if (!positions.isEmpty() && position >= 0 && position < positions.size()) {
    QRegularExpressionMatch match = pattern.match(text, positions[position]);
    if (match.hasMatch()) {
      matchLength = match.capturedLength();
    }
  }

  clearSelectionFormat(cursor, matchLength);

  if (!positions.isEmpty()) {

    position--;
    if (position < 0) {
      position = positions.size() - 1;
    }

    if (position >= 0 && position < positions.size()) {
      QRegularExpressionMatch prevMatch =
          pattern.match(text, positions[position]);
      if (prevMatch.hasMatch()) {
        matchLength = prevMatch.capturedLength();
      }
    }

    cursor.setPosition(positions[position]);

    if (!cursor.isNull()) {
      cursor.clearSelection();
      cursor.setPosition(positions[position] + matchLength,
                         QTextCursor::KeepAnchor);
      textArea->setTextCursor(cursor);
    }
  }
}

void FindReplacePanel::on_replaceAll_clicked() {
  if (m_vimCommandMode) {
    return;
  }
  if (textArea) {
    textArea->setFocus();
    QString searchWord = ui->searchFind->text();
    QString replaceWord = ui->fieldReplace->text();

    if (searchWord.isEmpty()) {
      return;
    }

    addToSearchHistory(searchWord);
    searchExecuted = true;
    activeSearchWord = searchWord;

    QRegularExpression pattern = buildSearchPattern(searchWord);
    QString text = textArea->toPlainText();

    QVector<QPair<int, int>> matchRanges;
    QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

    while (matches.hasNext()) {
      QRegularExpressionMatch match = matches.next();
      matchRanges.push_back(
          qMakePair(match.capturedStart(), match.capturedLength()));
    }

    if (matchRanges.isEmpty()) {
      position = -1;
      positions.clear();
      updateCounterLabels();
      return;
    }

    QTextCursor cursor(textArea->document());
    cursor.beginEditBlock();

    for (int i = matchRanges.size() - 1; i >= 0; --i) {
      int start = matchRanges[i].first;
      int length = matchRanges[i].second;

      cursor.setPosition(start);
      cursor.setPosition(start + length, QTextCursor::KeepAnchor);

      QString matchedText = cursor.selectedText();
      QString finalReplacement = applyPreserveCase(replaceWord, matchedText);

      cursor.removeSelectedText();
      cursor.insertText(finalReplacement);
    }

    cursor.endEditBlock();
    textArea->setTextCursor(cursor);

    position = -1;
    positions.clear();
    textArea->updateSyntaxHighlightTags();
    updateCounterLabels();
  }
}

void FindReplacePanel::onSearchTextChanged(const QString &text) {
  if (m_vimCommandMode) {
    return;
  }

  activeSearchWord = text;
  searchExecuted = !text.isEmpty();
  if (textArea) {
    lastObservedPlainText = textArea->toPlainText();
  }
  if (text.isEmpty()) {
    clearSearchFeedback();
  } else {
    beginSearchFeedback();
  }
  if (refreshTimer) {
    refreshTimer->start(120);
  }
}

void FindReplacePanel::beginSearchFeedback(const QString &message) {
  if (!searchStatusLabel) {
    return;
  }
  searchInProgress = true;
  searchStatusLabel->setText(message);
  searchStatusLabel->setVisible(true);
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void FindReplacePanel::updateSearchFeedback(const QString &message) {
  if (!searchStatusLabel) {
    return;
  }
  searchStatusLabel->setText(message);
  if (!searchStatusLabel->isVisible()) {
    searchStatusLabel->setVisible(true);
  }
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void FindReplacePanel::endSearchFeedback(int matchCount) {
  if (!searchStatusLabel) {
    return;
  }
  searchInProgress = false;
  searchStatusLabel->setText(QString("%1 matches").arg(matchCount));
  searchStatusLabel->setVisible(true);
}

void FindReplacePanel::clearSearchFeedback() {
  if (!searchStatusLabel) {
    return;
  }
  searchInProgress = false;
  searchStatusLabel->clear();
  searchStatusLabel->setVisible(false);
}

QStringList FindReplacePanel::getProjectFiles() const {
  QStringList files;

  if (projectPath.isEmpty()) {
    return files;
  }

  QDirIterator it(projectPath, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);

  QStringList searchableExtensions = {
      "cpp",   "hpp",  "c",       "h",    "cc",   "cxx",  "hxx",  "py",
      "pyw",   "js",   "jsx",     "ts",   "tsx",  "java", "go",   "rs",
      "rb",    "php",  "swift",   "kt",   "kts",  "cs",   "html", "htm",
      "css",   "scss", "sass",    "less", "json", "xml",  "yaml", "yml",
      "toml",  "md",   "txt",     "rst",  "sql",  "sh",   "bash", "zsh",
      "cmake", "make", "makefile"};

  while (it.hasNext()) {
    QString filePath = it.next();
    QFileInfo fileInfo(filePath);
    QString ext = fileInfo.suffix().toLower();
    QString fileNameLower = fileInfo.baseName().toLower();

    if (searchableExtensions.contains(ext) ||
        searchableExtensions.contains(fileNameLower)) {
      files.append(filePath);
    }
  }

  return files;
}

void FindReplacePanel::performGlobalSearch(const QString &searchWord,
                                           bool navigateToResult) {
  beginSearchFeedback(QString("Searching project..."));

  globalResults.clear();
  globalResultsByFile.clear();
  globalResultIndex = -1;

  if (resultsTree) {
    resultsTree->clear();
  }

  QRegularExpression pattern = buildSearchPattern(searchWord);

  if (!pattern.isValid()) {
    clearSearchFeedback();
    updateCounterLabels();
    return;
  }

  QStringList files = getProjectFiles();
  const QString currentPath = currentFilePath();
  const int totalFiles = files.size();

  for (int i = 0; i < totalFiles; ++i) {
    const QString &filePath = files[i];
    if ((i % 100) == 0) {
      updateSearchFeedback(
          QString("Searching project... %1/%2 files").arg(i + 1).arg(totalFiles));
    }
    if (!currentPath.isEmpty() && filePath == currentPath && textArea) {
      globalResultsByFile[filePath] =
          collectMatchesInContent(filePath, textArea->toPlainText(), pattern);
      continue;
    }
    searchInFile(filePath, pattern);
  }

  for (auto it = globalResultsByFile.cbegin(); it != globalResultsByFile.cend();
       ++it) {
    globalResults += it.value();
  }

  displayGlobalResults();

  if (navigateToResult && !globalResults.isEmpty()) {
    globalResultIndex = 0;
    navigateToGlobalResult(0);
  } else if (!globalResults.isEmpty()) {
    globalResultIndex = 0;
    navigateToGlobalResult(0, false);
  }

  endSearchFeedback(globalResults.size());
  updateCounterLabels();
}

void FindReplacePanel::onTextAreaContentsChanged() {
  if (!isVisible() || m_vimCommandMode || !refreshTimer) {
    return;
  }
  if (!searchExecuted) {
    return;
  }
  if (!textArea) {
    return;
  }
  QString currentPlainText = textArea->toPlainText();
  if (currentPlainText == lastObservedPlainText) {
    return;
  }
  lastObservedPlainText = currentPlainText;
  refreshTimer->start(250);
}

void FindReplacePanel::refreshSearchResults() {
  if (!isVisible() || m_vimCommandMode || !searchExecuted) {
    return;
  }

  beginSearchFeedback();

  QString searchWord = ui->searchFind->text();
  if (searchWord.isEmpty()) {
    positions.clear();
    position = -1;
    globalResults.clear();
    globalResultsByFile.clear();
    globalResultIndex = -1;
    searchExecuted = false;
    activeSearchWord.clear();
    if (resultsTree) {
      resultsTree->clear();
      resultsTree->setVisible(false);
    }
    if (textArea) {
      textArea->updateSyntaxHighlightTags();
    }
    clearSearchFeedback();
    updateCounterLabels();
    return;
  }

  if (searchWord != activeSearchWord) {
    clearSearchFeedback();
    return;
  }

  if (isGlobalMode()) {
    refreshGlobalResultsForCurrentFile(searchWord);
    return;
  }

  if (!textArea) {
    if (!isGlobalMode()) {
      positions.clear();
      position = -1;
      if (resultsTree) {
        resultsTree->clear();
        resultsTree->setVisible(false);
      }
      updateCounterLabels();
    }
    clearSearchFeedback();
    return;
  }

  QRegularExpression pattern = buildSearchPattern(searchWord);
  if (!pattern.isValid()) {
    positions.clear();
    position = -1;
    if (resultsTree) {
      resultsTree->clear();
      resultsTree->setVisible(false);
    }
    clearSearchFeedback();
    updateCounterLabels();
    return;
  }

  QString text = textArea->toPlainText();
  QVector<int> refreshedPositions;
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);
  while (matches.hasNext()) {
    QRegularExpressionMatch match = matches.next();
    refreshedPositions.push_back(match.capturedStart());
  }

  if (ui->searchBackward->isChecked()) {
    std::reverse(refreshedPositions.begin(), refreshedPositions.end());
  }

  int previousPosition = -1;
  if (position >= 0 && position < positions.size()) {
    previousPosition = positions[position];
  }

  positions = refreshedPositions;
  position = -1;

  if (!positions.isEmpty()) {
    if (previousPosition >= 0) {
      for (int i = 0; i < positions.size(); ++i) {
        if (positions[i] == previousPosition) {
          position = i;
          break;
        }
      }
    }

    if (position < 0) {
      int cursorPos = textArea->textCursor().selectionStart();
      if (ui->searchBackward->isChecked()) {
        for (int i = 0; i < positions.size(); ++i) {
          if (positions[i] <= cursorPos) {
            position = i;
            break;
          }
        }
        if (position < 0) {
          position = positions.size() - 1;
        }
      } else {
        for (int i = 0; i < positions.size(); ++i) {
          if (positions[i] >= cursorPos) {
            position = i;
            break;
          }
        }
        if (position < 0) {
          position = 0;
        }
      }
    }
  }

  textArea->updateSyntaxHighlightTags(searchWord);
  displayLocalResults(searchWord);

  if (resultsTree && position >= 0 && position < positions.size()) {
    for (int i = 0; i < resultsTree->topLevelItemCount(); ++i) {
      QTreeWidgetItem *resultItem = resultsTree->topLevelItem(i);
      if (!resultItem) {
        continue;
      }
      int matchStart = resultItem->data(0, kDataRoleMatchStart).toInt();
      if (matchStart == positions[position]) {
        resultsTree->setCurrentItem(resultItem);
        break;
      }
    }
  }

  endSearchFeedback(positions.size());
  updateCounterLabels();
}

void FindReplacePanel::searchInFile(const QString &filePath,
                                    const QRegularExpression &pattern) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }

  QTextStream stream(&file);
  QString content = stream.readAll();
  file.close();

  globalResultsByFile[filePath] =
      collectMatchesInContent(filePath, content, pattern);
}

QVector<GlobalSearchResult> FindReplacePanel::collectMatchesInContent(
    const QString &filePath, const QString &content,
    const QRegularExpression &pattern) const {
  QVector<GlobalSearchResult> matchesForFile;
  QStringList lines = content.split('\n');

  for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
    const QString &line = lines[lineNum];

    QRegularExpressionMatchIterator matches = pattern.globalMatch(line);
    while (matches.hasNext()) {
      QRegularExpressionMatch match = matches.next();

      GlobalSearchResult result;
      result.filePath = filePath;
      result.lineNumber = lineNum + 1;
      result.columnNumber = match.capturedStart() + 1;
      result.matchLength = match.capturedLength();
      result.lineContent = line.trimmed();
      matchesForFile.append(result);
    }
  }

  return matchesForFile;
}

QString FindReplacePanel::currentFilePath() const {
  if (!mainWindow) {
    return QString();
  }

  LightpadTabWidget *tabWidget = mainWindow->currentTabWidget();
  if (!tabWidget) {
    return QString();
  }

  int tabIndex = tabWidget->currentIndex();
  if (tabIndex < 0) {
    return QString();
  }
  return tabWidget->getFilePath(tabIndex);
}

void FindReplacePanel::refreshGlobalResultsForCurrentFile(
    const QString &searchWord) {
  updateSearchFeedback(QString("Searching current file..."));
  if (globalResultsByFile.isEmpty()) {
    performGlobalSearch(searchWord, false);
    return;
  }

  QString filePath = currentFilePath();
  if (filePath.isEmpty() || !textArea) {
    return;
  }

  if (!globalResultsByFile.contains(filePath)) {
    return;
  }

  QRegularExpression pattern = buildSearchPattern(searchWord);
  if (!pattern.isValid()) {
    return;
  }

  QString selectedFilePath;
  int selectedLine = -1;
  int selectedColumn = -1;
  if (globalResultIndex >= 0 && globalResultIndex < globalResults.size()) {
    const GlobalSearchResult &selectedResult = globalResults[globalResultIndex];
    selectedFilePath = selectedResult.filePath;
    selectedLine = selectedResult.lineNumber;
    selectedColumn = selectedResult.columnNumber;
  }

  globalResultsByFile[filePath] =
      collectMatchesInContent(filePath, textArea->toPlainText(), pattern);

  globalResults.clear();
  for (auto it = globalResultsByFile.cbegin(); it != globalResultsByFile.cend();
       ++it) {
    globalResults += it.value();
  }

  displayGlobalResults();

  if (globalResults.isEmpty()) {
    globalResultIndex = -1;
    endSearchFeedback(0);
    updateCounterLabels();
    return;
  }

  int nextIndex = 0;
  if (!selectedFilePath.isEmpty()) {
    for (int i = 0; i < globalResults.size(); ++i) {
      const GlobalSearchResult &result = globalResults[i];
      if (result.filePath == selectedFilePath && result.lineNumber == selectedLine &&
          result.columnNumber == selectedColumn) {
        nextIndex = i;
        break;
      }
    }
  } else if (globalResultIndex >= 0 && globalResultIndex < globalResults.size()) {
    nextIndex = globalResultIndex;
  }

  globalResultIndex = qBound(0, nextIndex, globalResults.size() - 1);
  navigateToGlobalResult(globalResultIndex, false);
  endSearchFeedback(globalResults.size());
  updateCounterLabels();
}

void FindReplacePanel::displayGlobalResults() {
  if (!resultsTree) {
    return;
  }

  resultsTree->clear();

  QMap<QString, QVector<GlobalSearchResult>> fileGroups;
  for (const GlobalSearchResult &result : globalResults) {
    fileGroups[result.filePath].append(result);
  }

  for (auto it = fileGroups.begin(); it != fileGroups.end(); ++it) {
    const QString &filePath = it.key();
    const QVector<GlobalSearchResult> &results = it.value();

    QFileInfo fileInfo(filePath);
    QString displayPath = filePath;
    if (!projectPath.isEmpty() && filePath.startsWith(projectPath)) {
      displayPath = filePath.mid(projectPath.length() + 1);
    }

    QTreeWidgetItem *fileItem = new QTreeWidgetItem(resultsTree);
    fileItem->setText(0, displayPath);
    fileItem->setText(1, QString::number(results.size()) + " matches");
    fileItem->setData(0, kDataRoleFilePath, filePath);
    fileItem->setData(0, kDataRoleLineNumber, -1);
    fileItem->setData(0, kDataRoleResultScope, kScopeGlobal);

    for (int i = 0; i < results.size(); ++i) {
      const GlobalSearchResult &result = results[i];

      QTreeWidgetItem *resultItem = new QTreeWidgetItem(fileItem);
      resultItem->setText(0, "");
      resultItem->setText(1, QString::number(result.lineNumber));
      resultItem->setText(2, result.lineContent);
      resultItem->setData(0, kDataRoleFilePath, filePath);
      resultItem->setData(0, kDataRoleLineNumber, result.lineNumber);
      resultItem->setData(0, kDataRoleColumnNumber, result.columnNumber);
      resultItem->setData(0, kDataRoleResultScope, kScopeGlobal);
    }

    fileItem->setExpanded(true);
  }

  resultsTree->setVisible(!globalResults.isEmpty());
}

void FindReplacePanel::navigateToGlobalResult(int index, bool emitNavigation) {
  if (index < 0 || index >= globalResults.size()) {
    return;
  }

  const GlobalSearchResult &result = globalResults[index];

  if (resultsTree) {
    bool found = false;
    int currentIndex = 0;
    for (int i = 0; i < resultsTree->topLevelItemCount(); ++i) {
      QTreeWidgetItem *fileItem = resultsTree->topLevelItem(i);
      for (int j = 0; j < fileItem->childCount(); ++j) {
        if (currentIndex == index) {
          resultsTree->setCurrentItem(fileItem->child(j));
          found = true;
          break;
        }
        currentIndex++;
      }
      if (found) {
        break;
      }
    }
  }

  if (emitNavigation) {
    emit navigateToFile(result.filePath, result.lineNumber, result.columnNumber);
  }
}

void FindReplacePanel::onGlobalResultClicked(QTreeWidgetItem *item,
                                             int column) {
  if (!item) {
    return;
  }

  if (item->data(0, kDataRoleResultScope).toInt() == kScopeLocal) {
    onLocalResultClicked(item, column);
    return;
  }

  QString filePath = item->data(0, kDataRoleFilePath).toString();
  int lineNumber = item->data(0, kDataRoleLineNumber).toInt();

  if (lineNumber < 0) {
    return;
  }

  int columnNumber = item->data(0, kDataRoleColumnNumber).toInt();

  for (int i = 0; i < globalResults.size(); ++i) {
    const GlobalSearchResult &result = globalResults[i];
    if (result.filePath == filePath && result.lineNumber == lineNumber &&
        result.columnNumber == columnNumber) {
      globalResultIndex = i;
      break;
    }
  }

  emit navigateToFile(filePath, lineNumber, columnNumber);

  updateCounterLabels();
}

void FindReplacePanel::displayLocalResults(const QString &searchWord) {
  if (!resultsTree || !textArea) {
    return;
  }

  resultsTree->clear();

  if (positions.isEmpty()) {
    resultsTree->setVisible(false);
    return;
  }

  QString text = textArea->toPlainText();
  QString filePath = currentFilePath();
  QStringList lines = text.split('\n');
  QRegularExpression pattern = buildSearchPattern(searchWord);

  QVector<int> lineStarts;
  int pos = 0;
  for (const QString &line : lines) {
    lineStarts.append(pos);
    pos += line.length() + 1;
  }

  for (int i = 0; i < positions.size(); ++i) {
    int matchPos = positions[i];

    int lineNum = 0;
    for (int j = 0; j < lineStarts.size(); ++j) {
      if (j + 1 < lineStarts.size() && matchPos >= lineStarts[j + 1]) {
        continue;
      }
      lineNum = j;
      break;
    }

    int columnNum = matchPos - lineStarts[lineNum] + 1;
    QString lineContent =
        (lineNum < lines.size()) ? lines[lineNum].trimmed() : QString();
    QRegularExpressionMatch match = pattern.match(text, matchPos);
    int matchLength =
        (match.hasMatch() && match.capturedStart() == matchPos)
            ? match.capturedLength()
            : searchWord.size();

    QTreeWidgetItem *resultItem = new QTreeWidgetItem(resultsTree);
    resultItem->setText(0, "Current File");
    resultItem->setText(1, QString::number(lineNum + 1));
    resultItem->setText(2, lineContent);
    resultItem->setData(0, kDataRoleFilePath, filePath);
    resultItem->setData(0, kDataRoleLineNumber, lineNum + 1);
    resultItem->setData(0, kDataRoleColumnNumber, columnNum);
    resultItem->setData(0, kDataRoleMatchStart, matchPos);
    resultItem->setData(0, kDataRoleMatchLength, matchLength);
    resultItem->setData(0, kDataRoleResultScope, kScopeLocal);
  }

  resultsTree->setVisible(true);
}

void FindReplacePanel::onLocalResultClicked(QTreeWidgetItem *item, int column) {
  if (!item || !textArea) {
    return;
  }

  if (item->data(0, kDataRoleResultScope).toInt() == kScopeGlobal) {
    onGlobalResultClicked(item, column);
    return;
  }

  QString itemFilePath = item->data(0, kDataRoleFilePath).toString();
  int lineNumber = item->data(0, kDataRoleLineNumber).toInt();
  int columnNumber = item->data(0, kDataRoleColumnNumber).toInt();
  int matchStart = item->data(0, kDataRoleMatchStart).toInt();
  int matchLength = item->data(0, kDataRoleMatchLength).toInt();

  if (lineNumber <= 0) {
    return;
  }

  if (!itemFilePath.isEmpty() && itemFilePath != currentFilePath()) {
    emit navigateToFile(itemFilePath, lineNumber, columnNumber);
    return;
  }

  QTextCursor cursor(textArea->document());
  if (matchStart >= 0) {
    cursor.setPosition(matchStart);
    int selectionLength = qMax(1, matchLength);
    cursor.setPosition(matchStart + selectionLength, QTextCursor::KeepAnchor);
  } else {
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                        lineNumber - 1);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                        columnNumber - 1);
  }

  textArea->setTextCursor(cursor);
  textArea->setFocus();

  int selectedPosition = matchStart >= 0 ? matchStart : cursor.selectionStart();
  for (int i = 0; i < positions.size(); ++i) {
    if (positions[i] == selectedPosition) {
      position = i;
      break;
    }
  }

  updateCounterLabels();
}
