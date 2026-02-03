#include "commandpalette.h"
#include "../uistylehelper.h"
#include <QApplication>
#include <QMenu>
#include <QScreen>
#include <QSettings>
#include <algorithm>

CommandPalette::CommandPalette(QWidget *parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint),
      m_searchBox(nullptr), m_resultsList(nullptr), m_layout(nullptr) {
  setupUI();
  loadRecentCommands();
}

CommandPalette::~CommandPalette() {}

void CommandPalette::setupUI() {
  setMinimumWidth(500);
  setMaximumHeight(400);

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(8, 8, 8, 8);
  m_layout->setSpacing(4);

  // Search box
  m_searchBox = new QLineEdit(this);
  m_searchBox->setPlaceholderText(tr("Type a command..."));
  m_searchBox->setStyleSheet("QLineEdit {"
                             "  padding: 8px;"
                             "  font-size: 14px;"
                             "  border: 1px solid #2a3241;"
                             "  border-radius: 4px;"
                             "  background: #1f2632;"
                             "  color: #e6edf3;"
                             "}");
  m_layout->addWidget(m_searchBox);

  // Results list
  m_resultsList = new QListWidget(this);
  m_resultsList->setStyleSheet("QListWidget {"
                               "  border: none;"
                               "  background: #0e1116;"
                               "  color: #e6edf3;"
                               "}"
                               "QListWidget::item {"
                               "  padding: 8px;"
                               "  border-bottom: 1px solid #2a3241;"
                               "}"
                               "QListWidget::item:selected {"
                               "  background: #1b2a43;"
                               "}"
                               "QListWidget::item:hover {"
                               "  background: #222a36;"
                               "}");
  m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_layout->addWidget(m_resultsList);

  // Connections
  connect(m_searchBox, &QLineEdit::textChanged, this,
          &CommandPalette::onSearchTextChanged);
  connect(m_resultsList, &QListWidget::itemActivated, this,
          &CommandPalette::onItemActivated);
  connect(m_resultsList, &QListWidget::itemClicked, this,
          &CommandPalette::onItemClicked);

  // Install event filter for keyboard navigation
  m_searchBox->installEventFilter(this);

  setStyleSheet("CommandPalette { background: #171c24; border: 1px solid "
                "#2a3241; border-radius: 8px; }");
}

void CommandPalette::registerAction(QAction *action, const QString &category) {
  if (!action || action->text().isEmpty())
    return;

  CommandItem item;
  item.id =
      action->objectName().isEmpty() ? action->text() : action->objectName();
  item.name =
      category.isEmpty() ? action->text() : category + ": " + action->text();
  item.name.remove('&'); // Remove mnemonics
  item.shortcut = action->shortcut().toString(QKeySequence::NativeText);
  item.action = action;
  item.score = 0;

  m_commands.append(item);
}

void CommandPalette::registerMenu(QMenu *menu, const QString &category) {
  if (!menu)
    return;

  QString cat = category.isEmpty() ? menu->title().remove('&') : category;

  for (QAction *action : menu->actions()) {
    if (action->isSeparator())
      continue;

    if (action->menu()) {
      registerMenu(action->menu(), cat);
    } else {
      registerAction(action, cat);
    }
  }
}

void CommandPalette::clearCommands() {
  m_commands.clear();
  m_filteredIndices.clear();
  m_resultsList->clear();
}

void CommandPalette::showPalette() {
  m_searchBox->clear();
  updateResults(QString());

  // Position at top center of parent
  if (parentWidget()) {
    QPoint parentCenter =
        parentWidget()->mapToGlobal(parentWidget()->rect().center());
    int x = parentCenter.x() - width() / 2;
    int y = parentWidget()->mapToGlobal(QPoint(0, 0)).y() + 50;
    move(x, y);
  }

  show();
  m_searchBox->setFocus();

  if (m_resultsList->count() > 0) {
    m_resultsList->setCurrentRow(0);
  }
}

void CommandPalette::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Escape:
    hide();
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (m_resultsList->currentRow() >= 0) {
      executeCommand(m_resultsList->currentRow());
    }
    break;
  case Qt::Key_Up:
    selectPrevious();
    break;
  case Qt::Key_Down:
    selectNext();
    break;
  default:
    QDialog::keyPressEvent(event);
  }
}

bool CommandPalette::eventFilter(QObject *obj, QEvent *event) {
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
        executeCommand(m_resultsList->currentRow());
      }
      return true;
    }
  }
  return QDialog::eventFilter(obj, event);
}

void CommandPalette::onSearchTextChanged(const QString &text) {
  updateResults(text);
}

void CommandPalette::onItemActivated(QListWidgetItem *item) {
  int row = m_resultsList->row(item);
  if (row >= 0) {
    executeCommand(row);
  }
}

void CommandPalette::onItemClicked(QListWidgetItem *item) {
  onItemActivated(item);
}

void CommandPalette::updateResults(const QString &query) {
  m_resultsList->clear();
  m_filteredIndices.clear();

  QList<QPair<int, int>> scored; // score, index

  for (int i = 0; i < m_commands.size(); ++i) {
    int score = 0;
    if (query.isEmpty()) {
      // When empty, prioritize recent commands
      int recentBonus = getRecentBonus(m_commands[i].id);
      score = 1000 - i + recentBonus;
    } else {
      score = fuzzyMatch(query.toLower(), m_commands[i].name.toLower());
      // Add bonus for recent commands even when searching
      if (score > 0) {
        score += getRecentBonus(m_commands[i].id) / 2;
      }
    }

    if (score > 0) {
      scored.append(qMakePair(score, i));
    }
  }

  // Sort by score descending
  std::sort(scored.begin(), scored.end(),
            [](const QPair<int, int> &a, const QPair<int, int> &b) {
              return a.first > b.first;
            });

  // Limit results
  int maxResults = 15;
  for (int i = 0; i < qMin(scored.size(), maxResults); ++i) {
    int idx = scored[i].second;
    m_filteredIndices.append(idx);

    const CommandItem &cmd = m_commands[idx];
    QListWidgetItem *item = new QListWidgetItem();

    QString displayText = cmd.name;
    if (!cmd.shortcut.isEmpty()) {
      displayText += "  [" + cmd.shortcut + "]";
    }
    // Mark recent commands
    if (m_recentCommands.contains(cmd.id) && query.isEmpty()) {
      displayText = "⏱ " + displayText;
    }
    item->setText(displayText);
    item->setData(Qt::UserRole, idx);
    m_resultsList->addItem(item);
  }

  if (m_resultsList->count() > 0) {
    m_resultsList->setCurrentRow(0);
  }

  // Adjust height
  int itemHeight = 35;
  int newHeight = qMin(m_resultsList->count() * itemHeight + 60, 400);
  setFixedHeight(newHeight);
}

int CommandPalette::fuzzyMatch(const QString &pattern, const QString &text) {
  if (pattern.isEmpty())
    return 1000;

  // Exact match gets highest score
  if (text.contains(pattern))
    return 2000 + (1000 - text.indexOf(pattern));

  // Fuzzy matching - all characters must appear in order
  int patternIdx = 0;
  int score = 0;
  int lastMatchIdx = -1;

  for (int i = 0; i < text.length() && patternIdx < pattern.length(); ++i) {
    if (text[i] == pattern[patternIdx]) {
      // Bonus for consecutive matches
      if (lastMatchIdx == i - 1) {
        score += 15;
      }
      // Bonus for word boundary matches
      if (i == 0 || text[i - 1] == ' ' || text[i - 1] == ':') {
        score += 10;
      }
      score += 10;
      lastMatchIdx = i;
      patternIdx++;
    }
  }

  // All pattern characters must be matched
  if (patternIdx != pattern.length())
    return 0;

  return score;
}

void CommandPalette::executeCommand(int row) {
  if (row < 0 || row >= m_filteredIndices.size())
    return;

  int cmdIdx = m_filteredIndices[row];
  if (cmdIdx < 0 || cmdIdx >= m_commands.size())
    return;

  // Track this command as recently used
  addToRecentCommands(m_commands[cmdIdx].id);

  hide();

  QAction *action = m_commands[cmdIdx].action;
  if (action && action->isEnabled()) {
    action->trigger();
  }
}

void CommandPalette::selectNext() {
  int current = m_resultsList->currentRow();
  if (current < m_resultsList->count() - 1) {
    m_resultsList->setCurrentRow(current + 1);
  }
}

void CommandPalette::selectPrevious() {
  int current = m_resultsList->currentRow();
  if (current > 0) {
    m_resultsList->setCurrentRow(current - 1);
  }
}

void CommandPalette::addToRecentCommands(const QString &commandId) {
  // Remove if already in list
  m_recentCommands.removeAll(commandId);

  // Add to front
  m_recentCommands.prepend(commandId);

  // Trim to max size
  while (m_recentCommands.size() > MAX_RECENT_COMMANDS) {
    m_recentCommands.removeLast();
  }

  saveRecentCommands();
}

void CommandPalette::loadRecentCommands() {
  QSettings settings("Lightpad", "Lightpad");
  m_recentCommands =
      settings.value("commandPalette/recentCommands").toStringList();
}

void CommandPalette::saveRecentCommands() {
  QSettings settings("Lightpad", "Lightpad");
  settings.setValue("commandPalette/recentCommands", m_recentCommands);
}

int CommandPalette::getRecentBonus(const QString &commandId) const {
  int index = m_recentCommands.indexOf(commandId);
  if (index < 0) {
    return 0;
  }
  // Most recent gets highest bonus, decreasing for older commands
  return (MAX_RECENT_COMMANDS - index) * 100;
}

void CommandPalette::applyTheme(const Theme &theme) {
  setStyleSheet("CommandPalette { " + UIStyleHelper::popupDialogStyle(theme) +
                " }");
  m_searchBox->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  m_resultsList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
}
