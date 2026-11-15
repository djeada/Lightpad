#include "sqlconsoleedit.h"
#include "../../core/editor/indentation.h"
#include "../../database/sqlquerycontext.h"
#include "../../settings/settingsmanager.h"
#include "../paneltypography.h"

#include "../../database/sqlkeywords.h"
#include "../../database/sqlstatementsplitter.h"
#include "../uistylehelper.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTextBlock>
#include <algorithm>

SqlHighlighter::SqlHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document) {
  for (const QString &k : SqlKeywords::keywords()) {
    m_keywords.insert(k);
  }
  for (const QString &k : SqlKeywords::types()) {
    m_types.insert(k);
  }
  for (const QString &k : SqlKeywords::functions()) {
    m_functions.insert(k);
  }
}

void SqlHighlighter::setTheme(const Theme &theme) {
  auto fmt = [](const QColor &c, bool bold = false, bool italic = false) {
    QTextCharFormat f;
    if (c.isValid()) {
      f.setForeground(c);
    }
    if (bold) {
      f.setFontWeight(QFont::DemiBold);
    }
    f.setFontItalic(italic);
    return f;
  };
  m_keyword = fmt(theme.keywordFormat_0, true);
  m_type = fmt(theme.typeFormat);
  m_function = fmt(theme.functionFormat);
  m_string = fmt(theme.quotationFormat);
  m_number = fmt(theme.numberFormat);
  m_comment = fmt(theme.singleLineCommentFormat, false, true);
  m_operator = fmt(theme.operatorFormat);
  m_identifier = fmt(theme.classFormat);
  m_variable = fmt(theme.constantFormat);
  rehighlight();
}

void SqlHighlighter::highlightBlock(const QString &text) {
  const int n = text.size();
  int i = 0;

  // States 2–5 preserve single quotes and quoted identifiers across lines.
  const QString quotes = QStringLiteral("'\"`]");
  auto quoted = [&](int start, QChar close, int state, bool opening) {
    int j = start + (opening ? 1 : 0);
    bool closed = false;
    while (j < n) {
      if (text[j] == close) {
        if (j + 1 < n && text[j + 1] == close) {
          j += 2;
          continue;
        }
        ++j;
        closed = true;
        break;
      }
      ++j;
    }
    setFormat(start, j - start, state == 2 ? m_string : m_identifier);
    setCurrentBlockState(closed ? 0 : state);
    return j;
  };
  if (previousBlockState() >= 2 && previousBlockState() <= 5) {
    const int state = previousBlockState();
    i = quoted(0, quotes[state - 2], state, false);
    if (currentBlockState() != 0)
      return;
  }

  if (previousBlockState() == 1) {
    const int close = text.indexOf(QLatin1String("*/"));
    if (close < 0) {
      setFormat(0, n, m_comment);
      setCurrentBlockState(1);
      return;
    }
    setFormat(0, close + 2, m_comment);
    i = close + 2;
  }
  setCurrentBlockState(0);

  while (i < n) {
    const QChar c = text[i];
    if (c.isSpace()) {
      ++i;
      continue;
    }
    if (c == '-' && i + 1 < n && text[i + 1] == '-') {
      setFormat(i, n - i, m_comment);
      return;
    }
    if (c == '/' && i + 1 < n && text[i + 1] == '*') {
      const int close = text.indexOf(QLatin1String("*/"), i + 2);
      if (close < 0) {
        setFormat(i, n - i, m_comment);
        setCurrentBlockState(1);
        return;
      }
      setFormat(i, close + 2 - i, m_comment);
      i = close + 2;
      continue;
    }
    if (c == '\'' || c == '"' || c == '`' || c == '[') {
      const QChar close = c == '[' ? QChar(']') : c;
      i = quoted(i, close, quotes.indexOf(close) + 2, true);
      if (currentBlockState() != 0)
        return;
      continue;
    }
    if (c.isDigit() || (c == '.' && i + 1 < n && text[i + 1].isDigit())) {
      int j = i + 1;
      while (j < n && (text[j].isLetterOrNumber() || text[j] == '.')) {
        ++j;
      }
      setFormat(i, j - i, m_number);
      i = j;
      continue;
    }
    if (c == '@' || c == ':' || c == '$') {
      int j = i + 1;
      while (j < n &&
             (text[j].isLetterOrNumber() || text[j] == '_' || text[j] == '@')) {
        ++j;
      }
      if (j > i + 1) {
        setFormat(i, j - i, m_variable);
        i = j;
        continue;
      }
    }
    if (c.isLetter() || c == '_') {
      int j = i + 1;
      while (j < n &&
             (text[j].isLetterOrNumber() || text[j] == '_' || text[j] == '$')) {
        ++j;
      }
      const QString word = text.mid(i, j - i);
      const QString upper = word.toUpper();
      int k = j;
      while (k < n && text[k] == ' ') {
        ++k;
      }
      const bool call = k < n && text[k] == '(';
      if (m_keywords.contains(upper) &&
          !(call && m_functions.contains(upper))) {
        setFormat(i, j - i, m_keyword);
      } else if (m_types.contains(upper)) {
        setFormat(i, j - i, m_type);
      } else if (call || m_functions.contains(upper)) {
        setFormat(i, j - i, m_function);
      }
      i = j;
      continue;
    }
    if (QStringLiteral("+-*/%=<>!|&^~").contains(c)) {
      setFormat(i, 1, m_operator);
    }
    ++i;
  }
}

SqlConsoleEdit::SqlConsoleEdit(QWidget *parent) : QPlainTextEdit(parent) {
  setObjectName("dbConsoleEdit");
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  setTabWidth(SettingsManager::instance().getValue("tabWidth", 4).toInt());
  connect(&SettingsManager::instance(), &SettingsManager::settingChanged, this,
          [this](const QString &key, const QVariant &value) {
            if (key == "tabWidth")
              setTabWidth(value.toInt());
          });
  setLineWrapMode(QPlainTextEdit::NoWrap);
  setPlaceholderText(
      tr("Write SQL here. Run statement (Ctrl+Enter) executes the current "
         "statement or selection. Run script (Ctrl+Shift+Enter) executes "
         "everything."));
  m_highlighter = new SqlHighlighter(document());

  m_completionModel = new QStandardItemModel(this);
  m_completer = new QCompleter(m_completionModel, this);
  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_completer->setFilterMode(Qt::MatchContains);
  m_completer->setMaxVisibleItems(10);
  connect(m_completer,
          QOverload<const QModelIndex &>::of(&QCompleter::activated), this,
          &SqlConsoleEdit::insertCompletion);

  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &SqlConsoleEdit::updateCurrentStatement);
  connect(this, &QPlainTextEdit::selectionChanged, this, [this] {
    if (textCursor().hasSelection())
      m_completer->popup()->hide();
    updateCurrentStatement();
  });
  connect(this, &QPlainTextEdit::textChanged, this,
          &SqlConsoleEdit::updateCurrentStatement);
}

void SqlConsoleEdit::setTabWidth(int width) {
  m_tabWidth = qBound(1, width, 16);
  setTabStopDistance(fontMetrics().horizontalAdvance(' ') * m_tabWidth);
}

void SqlConsoleEdit::setEngine(DbEngine engine) {
  m_engine = engine;
  updateCurrentStatement();
}

QString SqlConsoleEdit::selectionOrCurrentStatement() const {
  const QTextCursor c = textCursor();
  if (c.hasSelection()) {
    return c.selectedText().replace(QChar::ParagraphSeparator, '\n');
  }
  return SqlStatementSplitter::statementAt(toPlainText(), c.position(),
                                           m_engine)
      .text;
}

void SqlConsoleEdit::updateCurrentStatement() {
  QList<QTextEdit::ExtraSelection> selections;
  if (!textCursor().hasSelection()) {
    const QString script = toPlainText();
    const auto all = SqlStatementSplitter::split(script, m_engine);
    if (all.size() > 1) {
      const SqlStatement st = SqlStatementSplitter::statementAt(
          script, textCursor().position(), m_engine);
      if (!st.text.isEmpty()) {
        QTextEdit::ExtraSelection sel;
        QColor bg = m_theme.accentColor.isValid() ? m_theme.accentColor
                                                  : QColor(80, 120, 200);
        bg.setAlpha(28);
        sel.format.setBackground(bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, false);
        sel.cursor = QTextCursor(document());
        sel.cursor.setPosition(st.start);
        sel.cursor.setPosition(qMin(st.end, script.size()),
                               QTextCursor::KeepAnchor);
        selections.append(sel);
      }
    }
  }
  setExtraSelections(selections);
}

void SqlConsoleEdit::applyTheme(const Theme &theme) {
  m_theme = theme;
  const QColor text = UIStyleHelper::readableText(theme, theme.backgroundColor,
                                                  theme.foregroundColor);
  setStyleSheet(
      QString("QPlainTextEdit#dbConsoleEdit { background: %1; color: %2; "
              "border: none;"
              "  padding: 4px 6px; selection-background-color: %3; }")
          .arg(theme.backgroundColor.name(), text.name(),
               (theme.accentSoftColor.isValid() ? theme.accentSoftColor
                                                : theme.highlightColor)
                   .name()));
  m_highlighter->setTheme(theme);
  m_completer->popup()->setStyleSheet(UIStyleHelper::resultListStyle(theme));
  updateCurrentStatement();
}

bool SqlConsoleEdit::event(QEvent *e) {
  if (e->type() == QEvent::FontChange)
    setTabWidth(m_tabWidth);

  if (e->type() == QEvent::ShortcutOverride) {
    auto *ke = static_cast<QKeyEvent *>(e);
    if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
        (ke->modifiers() & Qt::ControlModifier)) {
      e->accept();
      return true;
    }
  }
  return QPlainTextEdit::event(e);
}

QString SqlConsoleEdit::prefixAtCursor(int *start) const {
  const auto prefix =
      SqlQueryContext::identifierPrefix(toPlainText(), textCursor().position());
  if (start)
    *start = prefix.start;
  return prefix.text;
}

void SqlConsoleEdit::showCompletions(bool explicitRequest) {
  if (isReadOnly() || textCursor().hasSelection()) {
    m_completer->popup()->hide();
    return;
  }
  int start = 0;
  const QString prefix = prefixAtCursor(&start);
  const QTextCursor c = textCursor();
  const QString script = toPlainText();
  const int caret = c.position();

  const SqlStatement st =
      SqlStatementSplitter::statementAt(script, caret, m_engine);
  int stmtStart = st.text.isEmpty() ? caret : st.start;
  QString full = script.mid(st.start, st.end - st.start);

  if (!st.text.isEmpty() && caret > st.start + 0 &&
      script.mid(st.start, st.end - st.start).endsWith(';') &&
      caret >= st.end) {
    stmtStart = caret;
    full.clear();
  }
  if (stmtStart > caret) {
    stmtStart = caret;
  }
  const QString before = script.mid(stmtStart, caret - stmtStart);
  if (full.size() < before.size()) {
    full = before;
  }

  const DbSchema *schema = m_schemaProvider ? m_schemaProvider() : nullptr;
  m_suggestions =
      SqlCompletion::suggest(schema, m_engine, before, full, prefix);

  m_suggestions.erase(std::remove_if(m_suggestions.begin(), m_suggestions.end(),
                                     [&](const SqlSuggestion &s) {
                                       return !prefix.isEmpty() &&
                                              s.insertText == prefix;
                                     }),
                      m_suggestions.end());
  if (m_suggestions.isEmpty() ||
      (!explicitRequest && prefix.isEmpty() && !before.endsWith('.'))) {
    m_completer->popup()->hide();
    return;
  }
  m_suggestions.resize(qMin(m_suggestions.size(), 200));

  m_completionModel->clear();
  auto *popup = m_completer->popup();
  popup->setAccessibleName(tr("SQL completions"));
  popup->setFont(font());
  popup->setStyleSheet(PanelTypography::styleForFont(
      UIStyleHelper::resultListStyle(m_theme), font()));
  for (int i = 0; i < m_suggestions.size(); ++i) {
    const SqlSuggestion &s = m_suggestions[i];
    auto *item = new QStandardItem(
        s.detail.isEmpty() ? s.label
                           : s.label + QStringLiteral("    ") + s.detail);
    item->setData(i, Qt::UserRole);
    item->setToolTip(s.label + '\n' + s.detail);
    item->setSizeHint(QSize(fontMetrics().horizontalAdvance(item->text()) + 24,
                            fontMetrics().height() + 12));
    m_completionModel->appendRow(item);
  }
  m_completer->setCompletionPrefix(QString());
  QRect rect = cursorRect();
  rect.setLeft(rect.left() - fontMetrics().horizontalAdvance(prefix));
  const int maxWidth = qMax(260, screen()->availableGeometry().width() - 40);
  popup->setMaximumWidth(maxWidth);
  rect.setWidth(
      qMin(maxWidth, popup->sizeHintForColumn(0) +
                         popup->verticalScrollBar()->sizeHint().width() + 16));
  m_completer->complete(rect);
  m_completer->popup()->setCurrentIndex(
      m_completer->completionModel()->index(0, 0));
}

void SqlConsoleEdit::insertCompletion(const QModelIndex &index) {
  if (isReadOnly() || textCursor().hasSelection())
    return;
  bool ok = false;
  int idx = index.data(Qt::UserRole).toInt(&ok);
  if (!ok) {

    idx = m_completer->completionModel()->data(index, Qt::UserRole).toInt();
  }
  if (idx < 0 || idx >= m_suggestions.size()) {
    return;
  }
  const auto prefix =
      SqlQueryContext::identifierPrefix(toPlainText(), textCursor().position());
  QTextCursor c = textCursor();
  c.setPosition(prefix.start);
  c.setPosition(prefix.end, QTextCursor::KeepAnchor);
  QString insert = m_suggestions[idx].insertText;
  if (insert.endsWith('(') && toPlainText().mid(prefix.end, 1) == "(")
    insert.chop(1);
  c.insertText(insert);
  setTextCursor(c);
}

void SqlConsoleEdit::keyPressEvent(QKeyEvent *event) {
  QAbstractItemView *popup = m_completer->popup();
  const bool backtab =
      event->key() == Qt::Key_Backtab ||
      (event->key() == Qt::Key_Tab && event->modifiers() & Qt::ShiftModifier);
  if (textCursor().hasSelection() || backtab ||
      (event->modifiers() &
       (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    popup->hide();
  if (popup->isVisible()) {
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab: {
      if (event->modifiers() & Qt::ControlModifier) {
        popup->hide();
        break;
      }
      const QModelIndex cur = popup->currentIndex();
      if (cur.isValid()) {
        insertCompletion(cur);
      }
      popup->hide();
      event->accept();
      return;
    }
    case Qt::Key_Escape:
      popup->hide();
      event->accept();
      return;
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:

      event->ignore();
      return;
    default:
      break;
    }
  }

  const bool ctrl = event->modifiers() & Qt::ControlModifier;
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
      ctrl) {
    if (event->modifiers() & Qt::ShiftModifier) {
      emit runScriptRequested();
    } else {
      emit runStatementRequested();
    }
    return;
  }
  if (event->modifiers() == Qt::AltModifier &&
      (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
    emit historyStep(event->key() == Qt::Key_Up ? -1 : 1);
    return;
  }
  if (ctrl && event->key() == Qt::Key_Space) {
    showCompletions(true);
    return;
  }

  const bool plain =
      !(event->modifiers() &
        (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
  if (plain &&
      (event->key() == Qt::Key_Tab || backtab ||
       event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
    if (!isReadOnly()) {
      const bool spaces =
          SettingsManager::instance().getValue("insertSpaces", true).toBool();
      QTextCursor cursor = textCursor();
      if (event->key() == Qt::Key_Tab || backtab)
        EditorIndentation::tab(cursor, m_tabWidth, spaces, backtab);
      else
        EditorIndentation::insertNewline(
            cursor,
            {"sql", m_tabWidth,
             SettingsManager::instance().getValue("autoIndent", true).toBool(),
             spaces, !spaces});
      setTextCursor(cursor);
      ensureCursorVisible();
    }
    event->accept();
    return;
  }
  if (!isReadOnly() && event->key() == Qt::Key_Backspace &&
      event->modifiers() == Qt::NoModifier) {
    auto cursor = textCursor();
    if (EditorIndentation::backspace(cursor, m_tabWidth)) {
      setTextCursor(cursor);
      popup->hide();
      return;
    }
  }

  QPlainTextEdit::keyPressEvent(event);

  if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
    showCompletions(false);
    return;
  }

  const QString typed = event->text();
  if (typed.isEmpty() || ctrl || (event->modifiers() & Qt::AltModifier)) {
    return;
  }
  const QChar ch = typed[0];
  if (ch == '.') {
    showCompletions(true);
  } else if (ch.isLetterOrNumber() || ch == '_') {
    int start = 0;
    if (prefixAtCursor(&start).size() >= 2) {
      showCompletions(false);
    } else {
      popup->hide();
    }
  } else {
    popup->hide();
  }
}
