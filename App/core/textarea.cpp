#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QCompleter>
#include <QDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextDocumentLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QtGlobal>
#include <algorithm>
#include <functional>

#include "../completion/completioncontext.h"
#include "../completion/completionengine.h"
#include "../completion/completionitem.h"
#include "../completion/completionwidget.h"
#include "../dap/breakpointmanager.h"
#include "../git/gitintegration.h"
#include "../language/languagecatalog.h"
#include "../settings/textareasettings.h"
#include "../syntax/pluginbasedsyntaxhighlighter.h"
#include "../syntax/syntaxpluginregistry.h"
#include "../test_templates/testfileclassifier.h"
#include "../theme/themeengine.h"
#include "../ui/mainwindow.h"
#include "editor/codefolding.h"
#include "editor/linenumberarea.h"
#include "editor/multicursor.h"
#include "editor/texttransforms.h"
#include "io/filemanager.h"
#include "lightpadpage.h"
#include "lightpadtabwidget.h"
#include "logging/logger.h"
#include "textarea.h"

QMap<QChar, QChar> brackets = {{'{', '}'}, {'(', ')'}, {'[', ']'}};
constexpr int defaultLineSpacingPercent = 130;
constexpr qreal editorDocumentMargin = 4.0;

static bool isCompletionEnabledForLanguage(const QString &languageId) {
  QString normalized = LanguageCatalog::normalize(languageId);
  QString effectiveId =
      normalized.isEmpty() ? languageId.trimmed().toLower() : normalized;
  return !effectiveId.isEmpty() && effectiveId != "plaintext";
}

static QString editorFontStyleSheet(const QFont &font) {
  QString family = font.family();
  family.replace("\\", "\\\\");
  family.replace("\"", "\\\"");

  QString style =
      QString("TextArea, QPlainTextEdit { font-family: \"%1\"; ").arg(family);
  if (font.pointSize() > 0) {
    style += QString("font-size: %1pt; ").arg(font.pointSize());
  }
  style += "}";
  return style;
}

class LineSpacingLayout : public QPlainTextDocumentLayout {
public:
  explicit LineSpacingLayout(QTextDocument *doc)
      : QPlainTextDocumentLayout(doc), m_spacing(0) {}

  void setLineSpacing(int pixels) {
    if (m_spacing != pixels) {
      m_spacing = qMax(0, pixels);
      requestUpdate();
    }
  }

  QRectF blockBoundingRect(const QTextBlock &block) const override {
    QRectF rect = QPlainTextDocumentLayout::blockBoundingRect(block);
    if (m_spacing > 0) {
      rect.setHeight(rect.height() + m_spacing);
    }
    return rect;
  }

private:
  int m_spacing;
};

QIcon TextArea::s_unsavedIcon;
bool TextArea::s_iconsInitialized = false;

static int findClosingParentheses(const QString &text, int pos, QChar startStr,
                                  QChar endStr) {

  int counter = 1;

  while (counter > 0 && pos < text.size() - 1) {
    auto chr = text[++pos];
    if (chr == startStr)
      counter++;

    else if (chr == endStr)
      counter--;
  }

  if (counter != 0)
    return -1;

  return pos;
}

static int findOpeningParentheses(const QString &text, int pos, QChar startStr,
                                  QChar endStr) {

  int counter = 1;
  pos--;

  while (counter > 0 && pos > 0) {
    auto chr = text[--pos];
    if (chr == startStr)
      counter--;

    else if (chr == endStr)
      counter++;
  }

  if (counter != 0)
    return -1;

  return ++pos;
}

static int leadingSpaces(const QString &str, int tabWidth) {

  const int width = qMax(1, tabWidth);
  int column = 0;

  for (int i = 0; i < str.size(); i++) {

    if (str[i] == '\x9') {
      column += width - (column % width);
    }

    else if (!str[i].isSpace())
      return column;
    else
      ++column;
  }

  return column;
}

static int expandedPositionForTabs(const QString &text, int position,
                                   int tabWidth) {
  const int width = qMax(1, tabWidth);
  int column = 0;
  int expandedPosition = 0;
  const int limit = qMin(position, text.size());

  for (int i = 0; i < limit; ++i) {
    const QChar ch = text.at(i);
    if (ch == '\t') {
      const int spaces = width - (column % width);
      expandedPosition += spaces;
      column += spaces;
    } else {
      ++expandedPosition;
      if (ch == '\n' || ch == '\r') {
        column = 0;
      } else {
        ++column;
      }
    }
  }

  return expandedPosition;
}

static bool isLastNonSpaceCharacterOpenBrace(const QString &str) {

  for (int i = str.size() - 1; i >= 0; i--) {
    if (!str[i].isSpace() && str[i] == '{')
      return true;
  }

  return false;
}

static int numberOfDigits(int x) {

  if (x == 0)
    return 1;

  int count = 0;

  if (x < 0)
    x *= -1;

  while (x > 0) {
    x /= 10;
    count++;
  }

  return count;
}

static bool isBacktabKey(const QKeyEvent *event) {
  return event->key() == Qt::Key_Backtab ||
         (event->key() == Qt::Key_Tab &&
          (event->modifiers() & Qt::ShiftModifier));
}

static bool hasCompletionDisallowedModifiers(const QKeyEvent *event) {
  return event->modifiers() &
         (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

static bool isTextInsertionKeyEvent(const QKeyEvent *event) {
  if (hasCompletionDisallowedModifiers(event)) {
    return false;
  }

  const QString text = event->text();
  if (text.isEmpty()) {
    return false;
  }

  const QChar first = text.at(0);
  return first.isLetterOrNumber() || first == '_';
}

struct AutoPairCharacters {
  QString open;
  QString close;
};

struct AutoPairContext {
  bool languageSupportsPairing = true;
  bool inString = false;
  bool inComment = false;
};

static QString normalizedPairingLanguage(const QString &languageId) {
  QString normalized = LanguageCatalog::normalize(languageId);
  if (!normalized.isEmpty()) {
    return normalized;
  }
  return languageId.trimmed().toLower();
}

static bool isAutoPairLanguage(const QString &languageId) {
  static const QSet<QString> supported = {
      "bazel", "c",     "cmake", "cpp",  "css",  "dockerfile", "go",   "glsl",
      "hlsl",  "html",  "java",  "js",   "json", "latex",      "make", "metal",
      "meson", "ninja", "py",    "rust", "sh",   "ts",         "wgsl", "yaml"};
  return supported.contains(normalizedPairingLanguage(languageId));
}

static bool supportsApostrophePairing(const QString &languageId) {
  const QString language = normalizedPairingLanguage(languageId);
  return language != "plaintext" && language != "md" && language != "make" &&
         language != "ninja" && language != "cmake";
}

static bool supportsQuotePairing(const QString &languageId) {
  const QString language = normalizedPairingLanguage(languageId);
  return language != "plaintext" && language != "md";
}

static bool isLikelyRawTextLanguage(const QString &languageId) {
  const QString language = normalizedPairingLanguage(languageId);
  return language == "plaintext" || language == "md";
}

static bool isEscapedAt(const QString &text, int index) {
  int slashCount = 0;
  for (int pos = index - 1; pos >= 0 && text.at(pos) == '\\'; --pos) {
    ++slashCount;
  }
  return slashCount % 2 == 1;
}

static AutoPairCharacters autoPairForKey(int key) {
  switch (key) {
  case Qt::Key_BraceLeft:
    return {"{", "}"};
  case Qt::Key_ParenLeft:
    return {"(", ")"};
  case Qt::Key_BracketLeft:
    return {"[", "]"};
  case Qt::Key_QuoteDbl:
    return {"\"", "\""};
  case Qt::Key_Apostrophe:
    return {"'", "'"};
  default:
    return {};
  }
}

static QString autoPairClosingForKey(int key) {
  switch (key) {
  case Qt::Key_BraceRight:
    return "}";
  case Qt::Key_ParenRight:
    return ")";
  case Qt::Key_BracketRight:
    return "]";
  case Qt::Key_QuoteDbl:
    return "\"";
  case Qt::Key_Apostrophe:
    return "'";
  default:
    return {};
  }
}

static bool isPairBoundaryAfter(const QTextCursor &cursor) {
  QTextCursor next = cursor;
  if (!next.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor)) {
    return true;
  }

  const QString selected = next.selectedText();
  if (selected.isEmpty()) {
    return true;
  }

  const QChar ch = selected.at(0);
  return ch.isSpace() || QStringLiteral(")]};,.:+-*/%&|^!?<>").contains(ch);
}

static bool isPairBoundaryBefore(const QTextCursor &cursor) {
  QTextCursor previous = cursor;
  if (!previous.movePosition(QTextCursor::PreviousCharacter,
                             QTextCursor::KeepAnchor)) {
    return true;
  }

  const QString selected = previous.selectedText();
  if (selected.isEmpty()) {
    return true;
  }

  const QChar ch = selected.at(0);
  return ch.isSpace() || QStringLiteral("([{=,:+-*/%&|^!<>").contains(ch);
}

static bool hasLineCommentAt(const QString &text, int position,
                             const QString &languageId) {
  const QString language = normalizedPairingLanguage(languageId);
  static const QSet<QString> slashCommentLanguages = {
      "c",  "cpp", "go",   "glsl", "hlsl", "java",
      "js", "ts",  "rust", "wgsl", "metal"};
  if (position + 1 < text.size() && text.mid(position, 2) == "//" &&
      slashCommentLanguages.contains(language)) {
    return true;
  }
  if (text.at(position) == '#' &&
      (language == "py" || language == "sh" || language == "yaml" ||
       language == "dockerfile" || language == "make" || language == "cmake")) {
    return true;
  }
  return false;
}

static AutoPairContext autoPairContextForText(const QString &text,
                                              const QString &languageId) {
  AutoPairContext context;
  context.languageSupportsPairing = isAutoPairLanguage(languageId);
  if (!context.languageSupportsPairing || isLikelyRawTextLanguage(languageId)) {
    return context;
  }

  const QString language = normalizedPairingLanguage(languageId);
  QChar stringQuote;
  bool inLineComment = false;
  bool inBlockComment = false;
  bool inTriplePythonString = false;
  QChar triplePythonQuote;

  for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text.at(i);

    if (inLineComment) {
      if (ch == '\n') {
        inLineComment = false;
      }
      continue;
    }

    if (inBlockComment) {
      if (i + 1 < text.size() && text.mid(i, 2) == "*/") {
        inBlockComment = false;
        ++i;
      } else if (language == "html" && i + 2 < text.size() &&
                 text.mid(i, 3) == "-->") {
        inBlockComment = false;
        i += 2;
      }
      continue;
    }

    if (inTriplePythonString) {
      if (i + 2 < text.size() && text.at(i) == triplePythonQuote &&
          text.at(i + 1) == triplePythonQuote &&
          text.at(i + 2) == triplePythonQuote && !isEscapedAt(text, i)) {
        inTriplePythonString = false;
        i += 2;
      }
      continue;
    }

    if (!stringQuote.isNull()) {
      if (ch == stringQuote && !isEscapedAt(text, i)) {
        stringQuote = QChar();
      }
      continue;
    }

    if (hasLineCommentAt(text, i, languageId)) {
      inLineComment = true;
      continue;
    }

    if (i + 1 < text.size() && text.mid(i, 2) == "/*" && language != "py" &&
        language != "sh" && language != "yaml") {
      inBlockComment = true;
      ++i;
      continue;
    }

    if (language == "html" && i + 3 < text.size() && text.mid(i, 4) == "<!--") {
      inBlockComment = true;
      i += 3;
      continue;
    }

    if (language == "py" && i + 2 < text.size() && (ch == '\'' || ch == '"') &&
        text.at(i + 1) == ch && text.at(i + 2) == ch && !isEscapedAt(text, i)) {
      inTriplePythonString = true;
      triplePythonQuote = ch;
      i += 2;
      continue;
    }

    if ((ch == '\'' || ch == '"') && !isEscapedAt(text, i)) {
      stringQuote = ch;
      continue;
    }

    if (ch == '`' && (language == "js" || language == "ts") &&
        !isEscapedAt(text, i)) {
      stringQuote = ch;
      continue;
    }
  }

  context.inString = !stringQuote.isNull() || inTriplePythonString;
  context.inComment = inLineComment || inBlockComment;
  return context;
}

static int selectionEndBlock(const QTextCursor &cursor) {
  int selectionStart = cursor.selectionStart();
  int selectionEnd = cursor.selectionEnd();

  QTextCursor endCursor(cursor.document());
  endCursor.setPosition(selectionEnd);

  int endBlock = endCursor.blockNumber();

  if (selectionEnd > selectionStart && endCursor.positionInBlock() == 0) {
    endBlock--;
  }

  return endBlock;
}

void TextArea::initializeIconCache() {
  if (!s_iconsInitialized) {
    s_unsavedIcon = QIcon(":/resources/icons/unsaved.png");
    s_iconsInitialized = true;
  }
}

TextArea::TextArea(QWidget *parent)
    : QPlainTextEdit(parent), mainWindow(nullptr),
      highlightColor(QColor(Qt::green).darker(250)),
      lineNumberAreaPenColor(QColor(Qt::gray).lighter(150)),
      defaultPenColor(QColor(Qt::white)),
      backgroundColor(QColor(Qt::gray).darker(200)), bufferText(""),
      highlightLang(""), syntaxHighlighter(nullptr), m_completer(nullptr),
      m_completionEngine(nullptr), m_completionWidget(nullptr),
      m_languageId("plaintext"), searchWord(""), areChangesUnsaved(false),
      autoIndent(true), showLineNumberArea(true), lineHighlighted(true),
      matchingBracketsHighlighted(true), prevWordCount(1),
      m_multiCursor(nullptr), m_columnSelectionActive(false),
      m_showWhitespace(false), m_showIndentGuides(false), m_vimMode(nullptr),
      m_inlineBlameEnabled(false), m_lastInlineBlameLine(-1),
      m_codeLensEnabled(false), m_debugExecutionLine(0) {
  initializeIconCache();
  m_multiCursor = new MultiCursorHandler(this);
  m_codeFolding = new CodeFoldingManager(document());
  m_vimMode = new VimMode(this, this);
  mainFont = QApplication::font();
  QPlainTextEdit::setFont(mainFont);
  QPlainTextEdit::setStyleSheet(editorFontStyleSheet(mainFont));
  setupTextArea();
  document()->setDefaultFont(mainFont);
  document()->setDocumentMargin(editorDocumentMargin);

  auto *layout = new LineSpacingLayout(document());
  document()->setDocumentLayout(layout);
  document()->setDocumentMargin(editorDocumentMargin);
  applyLineSpacing(defaultLineSpacingPercent);
  QTimer::singleShot(0, this, [this]() {
    updateLineNumberAreaLayout();
    if (lineNumberArea) {
      lineNumberArea->update();
    }
  });
  show();
}

TextArea::TextArea(const TextAreaSettings &settings, QWidget *parent)
    : QPlainTextEdit(parent), mainWindow(nullptr),
      highlightColor(ThemeEngine::instance().classicTheme().highlightColor),
      lineNumberAreaPenColor(
          ThemeEngine::instance().classicTheme().lineNumberAreaColor),
      defaultPenColor(ThemeEngine::instance().classicTheme().foregroundColor),
      backgroundColor(ThemeEngine::instance().classicTheme().backgroundColor),
      bufferText(""), highlightLang(""), syntaxHighlighter(nullptr),
      m_completer(nullptr), m_completionEngine(nullptr),
      m_completionWidget(nullptr), m_languageId("plaintext"), searchWord(""),
      areChangesUnsaved(false), autoIndent(settings.autoIndent),
      showLineNumberArea(settings.showLineNumberArea),
      lineHighlighted(settings.lineHighlighted),
      matchingBracketsHighlighted(settings.matchingBracketsHighlighted),
      prevWordCount(1), m_multiCursor(nullptr), m_columnSelectionActive(false),
      m_showWhitespace(false), m_showIndentGuides(false), m_vimMode(nullptr),
      m_inlineBlameEnabled(false), m_lastInlineBlameLine(-1),
      m_codeLensEnabled(false), m_debugExecutionLine(0) {
  initializeIconCache();
  m_multiCursor = new MultiCursorHandler(this);
  m_codeFolding = new CodeFoldingManager(document());
  m_vimMode = new VimMode(this, this);
  mainFont = settings.mainFont;
  QPlainTextEdit::setFont(mainFont);
  QPlainTextEdit::setStyleSheet(editorFontStyleSheet(mainFont));
  setupTextArea();
  document()->setDefaultFont(mainFont);
  document()->setDocumentMargin(editorDocumentMargin);

  auto *layout = new LineSpacingLayout(document());
  document()->setDocumentLayout(layout);
  document()->setDocumentMargin(editorDocumentMargin);
  applyLineSpacing(defaultLineSpacingPercent);
  QTimer::singleShot(0, this, [this]() {
    updateLineNumberAreaLayout();
    if (lineNumberArea) {
      lineNumberArea->update();
    }
  });
  show();
}

void TextArea::setupTextArea() {
  lineNumberArea = new LineNumberArea(this, this);
  lineNumberArea->setFont(mainFont);

  connect(this, &TextArea::blockCountChanged, this,
          [this] { updateLineNumberAreaLayout(); });

  connect(this, &TextArea::updateRequest, this, [this](const QRect &rect, int) {
    lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect())) {
      updateLineNumberAreaLayout();
    }
  });

  connect(document(), &QTextDocument::undoCommandAdded, this, [&] {
    if (!areChangesUnsaved) {

      setTabWidgetIcon(s_unsavedIcon);
      areChangesUnsaved = true;
    }
  });

  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
    static bool updateScheduled = false;
    if (!updateScheduled) {
      updateScheduled = true;
      QTimer::singleShot(16, this, [this]() {
        updateScheduled = false;
        updateHighlighterViewport();
      });
    }
  });

  auto &breakpointManager = BreakpointManager::instance();
  auto refreshBreakpoints = [this](const QString &filePath) {
    if (!filePath.isEmpty() && filePath == resolveFilePath()) {
      updateExtraSelections();
      if (lineNumberArea) {
        lineNumberArea->update();
      }
    }
  };

  connect(&breakpointManager, &BreakpointManager::fileBreakpointsChanged, this,
          refreshBreakpoints);
  connect(&breakpointManager, &BreakpointManager::breakpointChanged, this,
          [this, refreshBreakpoints](const Breakpoint &bp) {
            refreshBreakpoints(bp.filePath);
          });
  connect(&breakpointManager, &BreakpointManager::allBreakpointsCleared, this,
          [this]() {
            if (!resolveFilePath().isEmpty()) {
              updateExtraSelections();
              if (lineNumberArea) {
                lineNumberArea->update();
              }
            }
          });

  updateLineNumberAreaLayout();
  updateCursorPositionChangedCallbacks();
  clearLineHighlight();
}

void TextArea::applyLineSpacing(int percent) {
  if (auto *layout =
          dynamic_cast<LineSpacingLayout *>(document()->documentLayout())) {
    QFontMetrics fm(mainFont);
    int extraPixels = fm.height() * (percent - 100) / 100;
    layout->setLineSpacing(extraPixels);
  }
}

int TextArea::lineNumberAreaWidth() {
  if (showLineNumberArea && lineNumberArea) {
    return lineNumberArea->calculateWidth();
  }

  return 0;
}

void TextArea::increaseFontSize() { setFontSize(mainFont.pointSize() + 1); }

void TextArea::decreaseFontSize() { setFontSize(mainFont.pointSize() - 1); }

void TextArea::setFontSize(int size) {
  auto doc = document();

  if (doc) {
    mainFont.setPointSize(size);
    QPlainTextEdit::setFont(mainFont);
    if (viewport()) {
      viewport()->setFont(mainFont);
    }
    QPlainTextEdit::setStyleSheet(editorFontStyleSheet(mainFont));
    doc->setDefaultFont(mainFont);
    doc->setDocumentMargin(editorDocumentMargin);
    applyLineSpacing(defaultLineSpacingPercent);
  }
  if (lineNumberArea) {
    lineNumberArea->setFont(mainFont);
    updateLineNumberAreaLayout();
  }
}

void TextArea::setFont(QFont font) {
  mainFont = font;
  QPlainTextEdit::setFont(mainFont);
  if (viewport()) {
    viewport()->setFont(mainFont);
  }
  QPlainTextEdit::setStyleSheet(editorFontStyleSheet(mainFont));
  auto doc = document();

  if (doc) {
    doc->setDefaultFont(mainFont);
    doc->setDocumentMargin(editorDocumentMargin);
  }
  if (lineNumberArea) {
    lineNumberArea->setFont(mainFont);
    updateLineNumberAreaLayout();
  }
  applyLineSpacing(defaultLineSpacingPercent);
}

void TextArea::setPlainText(const QString &text) {
  QPlainTextEdit::setPlainText(text);
  applyLineSpacing(defaultLineSpacingPercent);
}

void TextArea::setMainWindow(MainWindow *window) {
  mainWindow = window;
  if (m_completionWidget && mainWindow) {
    m_completionWidget->applyTheme(ThemeEngine::instance().activeTheme());
  }
  if (mainWindow) {
    applySelectionPalette(mainWindow->getTheme());
  }
}

int TextArea::fontSize() { return mainFont.pointSize(); }

void TextArea::setTabWidth(int width) {
  QFontMetrics metrics(mainFont);
  setTabStopDistance(QFontMetricsF(mainFont).horizontalAdvance(' ') * width);
}

void TextArea::removeIconUnsaved() {
  setTabWidgetIcon(QIcon());
  areChangesUnsaved = false;
}

void TextArea::setAutoIdent(bool flag) { autoIndent = flag; }

void TextArea::showLineNumbers(bool flag) {
  showLineNumberArea = flag;
  LOG_DEBUG(QString("Show line numbers: %1").arg(flag ? "true" : "false"));
  if (lineNumberArea) {
    lineNumberArea->setVisible(showLineNumberArea);
  }
  updateLineNumberAreaLayout();
}

void TextArea::highlihtCurrentLine(bool flag) {
  lineHighlighted = flag;
  updateCursorPositionChangedCallbacks();
}

void TextArea::highlihtMatchingBracket(bool flag) {
  matchingBracketsHighlighted = flag;
  updateCursorPositionChangedCallbacks();
}

void TextArea::loadSettings(const TextAreaSettings settings) {
  const Theme theme = mainWindow ? mainWindow->getTheme()
                                 : ThemeEngine::instance().classicTheme();
  highlightColor = theme.highlightColor;
  lineNumberAreaPenColor = theme.lineNumberAreaColor;
  defaultPenColor = theme.foregroundColor;
  backgroundColor = theme.backgroundColor;
  if (m_completionWidget) {
    m_completionWidget->applyTheme(ThemeEngine::instance().activeTheme());
  }
  applySelectionPalette(theme);
  setAutoIdent(settings.autoIndent);
  showLineNumbers(settings.showLineNumberArea);
  highlihtCurrentLine(settings.lineHighlighted);
  highlihtMatchingBracket(settings.matchingBracketsHighlighted);
  setFont(settings.mainFont);
  setVimModeEnabled(settings.vimModeEnabled);
}

void TextArea::applySelectionPalette(const Theme &theme) {
  QPalette pal = palette();
  pal.setColor(QPalette::Base, theme.backgroundColor);
  pal.setColor(QPalette::Text, theme.foregroundColor);
  pal.setColor(QPalette::Highlight, theme.accentSoftColor);
  pal.setColor(QPalette::HighlightedText, theme.foregroundColor);
  setPalette(pal);
  if (viewport()) {
    viewport()->setPalette(pal);
  }

  if (lineNumberArea) {
    lineNumberArea->setBackgroundColor(theme.lineNumberAreaColor);
    lineNumberArea->setTextColor(theme.foregroundColor);
  }

  if (m_completionWidget) {
    m_completionWidget->applyTheme(theme);
  }
}

QString TextArea::getSearchWord() { return searchWord; }

bool TextArea::changesUnsaved() { return areChangesUnsaved; }

void TextArea::resizeEvent(QResizeEvent *e) {
  QPlainTextEdit::resizeEvent(e);
  updateLineNumberAreaLayout();
}

void TextArea::focusOutEvent(QFocusEvent *event) {
  hideCompletionPopup();
  QPlainTextEdit::focusOutEvent(event);
}

void TextArea::keyPressEvent(QKeyEvent *keyEvent) {

  if (keyEvent->matches(QKeySequence::ZoomOut) ||
      keyEvent->matches(QKeySequence::ZoomIn)) {
    mainWindow->keyPressEvent(keyEvent);
    return;
  }

  if (m_vimMode && m_vimMode->isEnabled() &&
      m_vimMode->processKeyEvent(keyEvent)) {
    expandTabsToSpacesInDocument();
    return;
  }

  if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
    if (keyEvent->key() == Qt::Key_Up) {
      addCursorAbove();
      return;
    } else if (keyEvent->key() == Qt::Key_Down) {
      addCursorBelow();
      return;
    }
  }

  if (keyEvent->modifiers() == Qt::ControlModifier &&
      keyEvent->key() == Qt::Key_D) {
    addCursorAtNextOccurrence();
    return;
  }

  if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
      keyEvent->key() == Qt::Key_L) {
    addCursorsToAllOccurrences();
    return;
  }

  if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
      keyEvent->key() == Qt::Key_I) {
    splitSelectionIntoLines();
    return;
  }

  if (keyEvent->key() == Qt::Key_Escape && hasMultipleCursors()) {
    clearExtraCursors();
    return;
  }

  if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
    if (keyEvent->key() == Qt::Key_BracketLeft) {
      foldCurrentBlock();
      return;
    } else if (keyEvent->key() == Qt::Key_BracketRight) {
      unfoldCurrentBlock();
      return;
    }
  }

  if (m_completionWidget && m_completionWidget->isVisible()) {
    switch (keyEvent->key()) {
    case Qt::Key_Up:
      m_completionWidget->selectPrevious();
      return;
    case Qt::Key_Down:
      m_completionWidget->selectNext();
      return;
    case Qt::Key_PageUp:
      m_completionWidget->selectPageUp();
      return;
    case Qt::Key_PageDown:
      m_completionWidget->selectPageDown();
      return;
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab:
      onCompletionAccepted(m_completionWidget->selectedItem());
      return;
    case Qt::Key_Escape:
      hideCompletionPopup();
      return;
    default:
      break;
    }
  }

  if (m_completer && m_completer->popup()->isVisible()) {

    switch (keyEvent->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Escape:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
      keyEvent->ignore();
      return;
    default:
      break;
    }
  }

  const bool isBacktab = isBacktabKey(keyEvent);
  const bool isPlainTab = keyEvent->key() == Qt::Key_Tab && !isBacktab;
  const Qt::KeyboardModifiers disallowedTabModifiers =
      Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

  if ((isPlainTab || isBacktab) &&
      !(keyEvent->modifiers() & disallowedTabModifiers)) {
    QTextCursor cursor = textCursor();
    const bool hasSelection = cursor.hasSelection();

    int tabWidth = 4;
    if (mainWindow) {
      tabWidth = qMax(1, mainWindow->getTabWidth());
    }

    if (hasSelection || isBacktab) {
      int startPosition =
          hasSelection ? cursor.selectionStart() : cursor.block().position();
      int startColumn = hasSelection ? 0 : cursor.positionInBlock();

      QTextCursor startCursor(document());
      startCursor.setPosition(startPosition);
      int startBlock = startCursor.blockNumber();
      int endBlock = hasSelection ? selectionEndBlock(cursor) : startBlock;

      if (endBlock < startBlock) {
        endBlock = startBlock;
      }

      int removedFromCurrentLine = 0;
      cursor.beginEditBlock();
      for (int blockIndex = startBlock; blockIndex <= endBlock; ++blockIndex) {
        QTextBlock block = document()->findBlockByNumber(blockIndex);
        if (!block.isValid()) {
          continue;
        }

        QTextCursor lineCursor(block);
        lineCursor.movePosition(QTextCursor::StartOfBlock);

        if (isBacktab) {
          const QString lineText = block.text();
          int removeCount = 0;
          if (lineText.startsWith('\t')) {
            removeCount = 1;
          } else {
            while (removeCount < tabWidth && removeCount < lineText.size() &&
                   lineText[removeCount] == ' ') {
              ++removeCount;
            }
          }

          if (removeCount > 0) {
            lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                                    removeCount);
            lineCursor.removeSelectedText();

            if (!hasSelection && blockIndex == startBlock) {
              removedFromCurrentLine = removeCount;
            }
          }
        } else {
          lineCursor.insertText(QString(" ").repeated(tabWidth));
        }
      }
      cursor.endEditBlock();

      if (hasSelection) {
        QTextBlock selectionStartBlock =
            document()->findBlockByNumber(startBlock);
        QTextBlock selectionEndBlock = document()->findBlockByNumber(endBlock);

        if (selectionStartBlock.isValid() && selectionEndBlock.isValid()) {
          QTextCursor selectionCursor(document());
          selectionCursor.setPosition(selectionStartBlock.position());

          QTextCursor blockEndCursor(selectionEndBlock);
          blockEndCursor.movePosition(QTextCursor::EndOfBlock);

          selectionCursor.setPosition(blockEndCursor.position(),
                                      QTextCursor::KeepAnchor);
          setTextCursor(selectionCursor);
        }
      } else if (isBacktab) {
        QTextBlock currentBlock = document()->findBlockByNumber(startBlock);
        if (currentBlock.isValid()) {
          int currentLineLength = qMax(0, currentBlock.length() - 1);
          int nextColumn = qMin(currentLineLength,
                                qMax(0, startColumn - removedFromCurrentLine));
          QTextCursor updatedCursor(currentBlock);
          updatedCursor.movePosition(QTextCursor::StartOfBlock);
          updatedCursor.movePosition(QTextCursor::Right,
                                     QTextCursor::MoveAnchor, nextColumn);
          setTextCursor(updatedCursor);
        }
      } else {
        cursor.insertText(QString(" ").repeated(tabWidth));
        setTextCursor(cursor);
      }

      keyEvent->accept();
      return;
    }
  }

  if (hasMultipleCursors() && !keyEvent->text().isEmpty() &&
      keyEvent->modifiers() == Qt::NoModifier) {

    QString text = keyEvent->text();
    applyToAllCursors(
        [&text](QTextCursor &cursor) { cursor.insertText(text); });
    expandTabsToSpacesInDocument();
    return;
  }

  if (hasMultipleCursors() && keyEvent->key() == Qt::Key_Backspace) {
    applyToAllCursors([](QTextCursor &cursor) {
      if (!cursor.hasSelection()) {
        cursor.deletePreviousChar();
      } else {
        cursor.removeSelectedText();
      }
    });
    return;
  }

  if (hasMultipleCursors() && keyEvent->key() == Qt::Key_Delete) {
    applyToAllCursors([](QTextCursor &cursor) {
      if (!cursor.hasSelection()) {
        cursor.deleteChar();
      } else {
        cursor.removeSelectedText();
      }
    });
    return;
  }

  if (handleAutoPairBackspace(keyEvent)) {
    return;
  }

  if (handleAutoPairKey(keyEvent)) {
    return;
  }

  bool isShortcut = ((keyEvent->modifiers() & Qt::ControlModifier) &&
                     keyEvent->key() == Qt::Key_Space);

  if (!isShortcut) {

    QPlainTextEdit::keyPressEvent(keyEvent);

    if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)
      handleKeyEnterPressed();
    expandTabsToSpacesInDocument();
  }

  if (m_completionEngine) {
    if (!isCompletionEnabledForLanguage(m_languageId)) {
      invalidateCompletionRequest();
      hideCompletionPopup();
      return;
    }

    static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-=");
    const bool isTypingEvent = isTextInsertionKeyEvent(keyEvent);
    QString completionPrefix = textUnderCursor();

    if (!isShortcut && (!isTypingEvent || completionPrefix.length() < 2 ||
                        eow.contains(keyEvent->text().right(1)))) {
      invalidateCompletionRequest();
      hideCompletionPopup();
      return;
    }

    QTextCursor cursor = textCursor();
    m_lastCompletionRequestPosition = cursor.position();
    CompletionContext ctx;
    ctx.documentUri = getDocumentUri();
    ctx.languageId = m_languageId;
    ctx.prefix = completionPrefix;
    ctx.line = cursor.blockNumber();
    ctx.column = cursor.positionInBlock();
    ctx.lineText = cursor.block().text();
    ctx.triggerKind = isShortcut ? CompletionTriggerKind::Invoked
                                 : CompletionTriggerKind::TriggerCharacter;
    ctx.isAutoComplete = !isShortcut;

    m_completionEngine->requestCompletions(ctx);
    return;
  }

  if (!m_completer)
    return;

  static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-=");
  const bool isTypingEvent = isTextInsertionKeyEvent(keyEvent);
  QString completionPrefix = textUnderCursor();

  if (!isShortcut && (!isTypingEvent || completionPrefix.length() < 3 ||
                      eow.contains(keyEvent->text().right(1)))) {
    m_completer->popup()->hide();
    return;
  }

  if (completionPrefix != m_completer->completionPrefix()) {
    m_completer->setCompletionPrefix(completionPrefix);
    m_completer->popup()->setCurrentIndex(
        m_completer->completionModel()->index(0, 0));
  }
  QRect cr = cursorRect();
  cr.setWidth(m_completer->popup()->sizeHintForColumn(0) +
              m_completer->popup()->verticalScrollBar()->sizeHint().width());
  m_completer->complete(cr);
}

void TextArea::contextMenuEvent(QContextMenuEvent *event) {
  auto menu = createStandardContextMenu();
  menu->addSeparator();

  QAction *goToDefAction = menu->addAction(tr("Go to Definition"));
  goToDefAction->setShortcut(QKeySequence(Qt::Key_F12));
  connect(goToDefAction, &QAction::triggered, this, [this]() {
    if (mainWindow) {
      mainWindow->goToDefinitionAtCursor();
    }
  });

  menu->addAction(tr("Refactor"));

  menu->addSeparator();

  const QString contextFilePath = resolveFilePath();
  const QString contextFileName = QFileInfo(contextFilePath).fileName();

  QAction *runFileAction = menu->addAction(
      contextFileName.isEmpty() ? tr("Run File")
                                : tr("Run %1").arg(contextFileName));
  runFileAction->setEnabled(!contextFilePath.isEmpty());
  connect(runFileAction, &QAction::triggered, this, [this, contextFilePath]() {
    if (mainWindow && !contextFilePath.isEmpty()) {
      mainWindow->runFileByPath(contextFilePath);
    }
  });

  QAction *debugFileAction = menu->addAction(
      contextFileName.isEmpty() ? tr("Debug File")
                                : tr("Debug %1").arg(contextFileName));
  debugFileAction->setEnabled(!contextFilePath.isEmpty());
  connect(debugFileAction, &QAction::triggered, this,
          [this, contextFilePath]() {
            if (mainWindow && !contextFilePath.isEmpty()) {
              mainWindow->debugFileByPath(contextFilePath);
            }
          });

  const QString currentFilePath = contextFilePath;
  if (!currentFilePath.isEmpty()) {
    bool isTest = TestFileClassifier::instance().isTestFile(currentFilePath);
    QAction *runAsTestAction = menu->addAction(tr("Run as Test"));
    connect(runAsTestAction, &QAction::triggered, this,
            [this, currentFilePath]() {
              if (mainWindow) {

                mainWindow->runTestsForPath(currentFilePath);
              }
            });

    QString markLabel =
        isTest ? tr("Unmark as Test File") : tr("Mark as Test File");
    QAction *markTestAction = menu->addAction(markLabel);
    connect(markTestAction, &QAction::triggered, this,
            [currentFilePath, isTest]() {
              TestFileClassifier::instance().setTestOverride(currentFilePath,
                                                             !isTest);
            });
  }

  if (mainWindow) {
    auto *gitIntegration = mainWindow->getGitIntegration();
    QString filePath = resolveFilePath();
    if (gitIntegration && gitIntegration->isValidRepository() &&
        !filePath.isEmpty()) {
      menu->addSeparator();

      QTextCursor cursor = textCursor();
      int startLine = cursor.blockNumber() + 1;
      int endLine = startLine;
      if (cursor.hasSelection()) {
        QTextCursor startCursor(document());
        startCursor.setPosition(cursor.selectionStart());
        startLine = startCursor.blockNumber() + 1;
        QTextCursor endCursor(document());
        endCursor.setPosition(cursor.selectionEnd());
        endLine = endCursor.blockNumber() + 1;
      }

      QAction *lineHistoryAction = menu->addAction(
          startLine == endLine
              ? tr("Line History (line %1)").arg(startLine)
              : tr("Line History (lines %1-%2)").arg(startLine).arg(endLine));

      connect(
          lineHistoryAction, &QAction::triggered, this,
          [this, gitIntegration, filePath, startLine, endLine]() {
            QList<GitCommitInfo> commits =
                gitIntegration->getLineHistory(filePath, startLine, endLine);
            if (commits.isEmpty()) {
              return;
            }

            QString html =
                QStringLiteral("<html><body style='font-family: monospace;'>"
                               "<h3>Line History: lines %1-%2</h3>")
                    .arg(startLine)
                    .arg(endLine);
            for (const auto &c : commits) {
              html += QStringLiteral(
                          "<div style='margin: 6px 0; padding: 4px; "
                          "border-left: 3px solid #4caf50;'>"
                          "<b>%1</b> %2<br>"
                          "<span style='color:#888;'>%3 — %4</span></div>")
                          .arg(c.shortHash.toHtmlEscaped())
                          .arg(c.subject.toHtmlEscaped())
                          .arg(c.author.toHtmlEscaped())
                          .arg(c.relativeDate.toHtmlEscaped());
            }
            html += "</body></html>";

            QDialog dlg(this);
            dlg.setWindowTitle(tr("Line History"));
            dlg.resize(500, 400);
            auto *layout = new QVBoxLayout(&dlg);
            auto *view = new QTextEdit(&dlg);
            view->setReadOnly(true);
            view->setHtml(html);
            layout->addWidget(view);
            auto *closeBtn = new QPushButton(tr("Close"), &dlg);
            connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
            layout->addWidget(closeBtn);
            dlg.exec();
          });

      QAction *fileHistoryAction = menu->addAction(tr("File History"));
      connect(fileHistoryAction, &QAction::triggered, this, [this]() {
        if (mainWindow) {
          mainWindow->showFileHistory();
        }
      });

      QAction *openAtRevisionAction =
          menu->addAction(tr("Open File at Revision..."));
      connect(
          openAtRevisionAction, &QAction::triggered, this,
          [this, gitIntegration, filePath]() {
            QList<GitCommitInfo> commits =
                gitIntegration->getFileLog(filePath, 20);
            if (commits.isEmpty())
              return;

            QStringList items;
            for (const auto &c : commits) {
              items << QString("%1 — %2 (%3)")
                           .arg(c.shortHash)
                           .arg(c.subject)
                           .arg(c.relativeDate);
            }

            bool ok;
            QString selected = QInputDialog::getItem(
                this, tr("Open File at Revision"), tr("Select a revision:"),
                items, 0, false, &ok);
            if (!ok || selected.isEmpty())
              return;

            int idx = items.indexOf(selected);
            if (idx < 0 || idx >= commits.size())
              return;

            QString content =
                gitIntegration->getFileAtRevision(filePath, commits[idx].hash);
            if (content.isEmpty())
              return;

            if (mainWindow) {
              mainWindow->openReadOnlyTab(content,
                                          QFileInfo(filePath).fileName() +
                                              " @ " + commits[idx].shortHash,
                                          filePath);
            }
          });
    }
  }

  menu->exec(event->globalPos());
  delete menu;
}

void TextArea::insertFromMimeData(const QMimeData *source) {
  QPlainTextEdit::insertFromMimeData(source);
  expandTabsToSpacesInDocument();
}

void TextArea::setTabWidgetIcon(QIcon icon) {
  auto page = qobject_cast<LightpadPage *>(parentWidget());

  if (page) {

    auto stackedWidget =
        qobject_cast<QStackedWidget *>(parentWidget()->parentWidget());

    if (stackedWidget) {

      auto tabWidget = qobject_cast<LightpadTabWidget *>(
          parentWidget()->parentWidget()->parentWidget());

      if (tabWidget) {
        auto index = tabWidget->indexOf(page);

        if (index != -1)
          tabWidget->setTabIcon(index, icon);
      }
    }
  }
}

bool TextArea::handleAutoPairKey(QKeyEvent *event) {
  if (!event || (event->modifiers() &
                 (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
    return false;
  }

  const AutoPairCharacters pair = autoPairForKey(event->key());
  const QString closing = autoPairClosingForKey(event->key());
  if (pair.open.isEmpty() && closing.isEmpty()) {
    return false;
  }

  const AutoPairContext context = autoPairContextForText(
      toPlainText().left(textCursor().position()), m_languageId);

  QTextCursor cursor = textCursor();

  if (!closing.isEmpty() && context.languageSupportsPairing &&
      !context.inComment &&
      (!context.inString || closing == "\"" || closing == "'") &&
      !cursor.hasSelection()) {
    QTextCursor next = cursor;
    if (next.movePosition(QTextCursor::NextCharacter,
                          QTextCursor::KeepAnchor) &&
        next.selectedText() == closing) {
      cursor.movePosition(QTextCursor::NextCharacter);
      setTextCursor(cursor);
      event->accept();
      return true;
    }
  }

  if (pair.open.isEmpty()) {
    return false;
  }

  if (!context.languageSupportsPairing || context.inComment ||
      context.inString) {
    return false;
  }

  const bool isQuotePair = pair.open == pair.close;
  if (pair.open == "\"" && !supportsQuotePairing(m_languageId)) {
    return false;
  }
  if (pair.open == "'" && !supportsApostrophePairing(m_languageId)) {
    return false;
  }
  if (isQuotePair && !cursor.hasSelection() &&
      (!isPairBoundaryBefore(cursor) || !isPairBoundaryAfter(cursor))) {
    return false;
  }
  if (!isQuotePair && !cursor.hasSelection() && !isPairBoundaryAfter(cursor)) {
    return false;
  }

  closeParentheses(pair.open, pair.close);
  event->accept();
  return true;
}

bool TextArea::handleAutoPairBackspace(QKeyEvent *event) {
  if (!event || event->key() != Qt::Key_Backspace ||
      event->modifiers() != Qt::NoModifier) {
    return false;
  }

  QTextCursor cursor = textCursor();
  if (cursor.hasSelection()) {
    return false;
  }

  QTextCursor previous = cursor;
  QTextCursor next = cursor;
  if (!previous.movePosition(QTextCursor::PreviousCharacter,
                             QTextCursor::KeepAnchor) ||
      !next.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor)) {
    return false;
  }

  const QString open = previous.selectedText();
  const QString close = next.selectedText();
  if (!((open == "{" && close == "}") || (open == "(" && close == ")") ||
        (open == "[" && close == "]") || (open == "\"" && close == "\"") ||
        (open == "'" && close == "'"))) {
    return false;
  }

  cursor.beginEditBlock();
  cursor.deletePreviousChar();
  cursor.deleteChar();
  cursor.endEditBlock();
  setTextCursor(cursor);
  event->accept();
  return true;
}

void TextArea::closeParentheses(QString startStr, QString endStr) {
  auto cursor = textCursor();

  if (cursor.hasSelection()) {
    auto start = cursor.selectionStart();
    auto end = cursor.selectionEnd();
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.insertText(startStr);
    cursor.setPosition(end + startStr.size(), QTextCursor::MoveAnchor);
    cursor.insertText(endStr);
  }

  else {
    auto pos = cursor.position();
    cursor.setPosition(pos, QTextCursor::MoveAnchor);
    cursor.insertText(startStr + endStr);
    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor,
                        endStr.size());
  }

  setTextCursor(cursor);
}

void TextArea::handleKeyEnterPressed() {
  if (mainWindow && autoIndent) {
    auto cursor = textCursor();
    auto pos = cursor.position();
    cursor.movePosition(QTextCursor::PreviousBlock);

    const auto prevLine = cursor.block().text();
    auto tabWidth = mainWindow->getTabWidth();
    auto n = leadingSpaces(prevLine, tabWidth);

    if (isLastNonSpaceCharacterOpenBrace(prevLine))
      n += tabWidth;

    cursor.setPosition(pos, cursor.MoveAnchor);
    cursor.insertText(QString(" ").repeated(n));
    setTextCursor(cursor);
  }
}

void TextArea::drawCurrentLineHighlight() {
  QList<QTextEdit::ExtraSelection> extraSelections;

  QTextEdit::ExtraSelection selection;

  QColor color =
      mainWindow ? mainWindow->getTheme().highlightColor : highlightColor;
  selection.format.setBackground(color);
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = textCursor();
  selection.cursor.clearSelection();
  extraSelections.append(selection);

  setExtraSelections(extraSelections);
}

void TextArea::clearLineHighlight() {
  QList<QTextEdit::ExtraSelection> extraSelections;
  setExtraSelections(extraSelections);
}

void TextArea::updateRowColDisplay() {
  if (mainWindow)
    mainWindow->setRowCol(textCursor().blockNumber(),
                          textCursor().positionInBlock());
}

void TextArea::drawMatchingBrackets() {
  auto _drawMatchingBrackets =
      [&](QTextCursor::MoveOperation op, const QChar &startStr,
          const QChar &endStr,
          std::function<int(const QString &, int, QChar, QChar)> function) {
        QList<QTextEdit::ExtraSelection> extraSelections;

        if (lineHighlighted) {
          extraSelections = this->extraSelections();
          while (extraSelections.size() > 1)
            extraSelections.pop_back();
        }

        QTextEdit::ExtraSelection selection;

        selection.format.setForeground(QColor("yellow"));

        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selection.cursor.movePosition(op, QTextCursor::KeepAnchor);
        extraSelections.append(selection);

        auto plainText = toPlainText();
        auto pos =
            function(plainText, textCursor().position(), startStr, endStr);

        if (pos != -1) {
          selection.cursor.setPosition(pos);
          selection.cursor.movePosition(op, QTextCursor::KeepAnchor);
          extraSelections.append(selection);
          setExtraSelections(extraSelections);
        }
      };

  auto cursor = textCursor();
  auto result =
      cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  auto startStr = result ? cursor.selectedText().front() : QChar(' ');

  cursor = textCursor();
  result = cursor.movePosition(QTextCursor::PreviousCharacter,
                               QTextCursor::KeepAnchor);
  auto endStr = result ? cursor.selectedText().front() : QChar(' ');

  if (brackets.contains(startStr))
    _drawMatchingBrackets(QTextCursor::NextCharacter, startStr,
                          brackets[startStr], &findClosingParentheses);

  else if (brackets.values().contains(endStr))
    _drawMatchingBrackets(QTextCursor::PreviousCharacter, brackets.key(endStr),
                          endStr, &findOpeningParentheses);
}

void TextArea::updateExtraSelections() {
  QList<QTextEdit::ExtraSelection> extraSelections;
  QTextCursor cursor = textCursor();
  QString filePath = resolveFilePath();

  QMap<int, Breakpoint> breakpointsByLine;
  if (!filePath.isEmpty()) {
    const QList<Breakpoint> breakpoints =
        BreakpointManager::instance().breakpointsForFile(filePath);
    for (const Breakpoint &bp : breakpoints) {
      int displayLine =
          (bp.verified && bp.boundLine > 0) ? bp.boundLine : bp.line;
      if (displayLine <= 0) {
        continue;
      }
      if (!breakpointsByLine.contains(displayLine) || bp.enabled) {
        breakpointsByLine[displayLine] = bp;
      }
    }
  }

  if (!breakpointsByLine.isEmpty()) {
    QColor baseColor(231, 76, 60);
    if (mainWindow) {
      baseColor = mainWindow->getTheme().debugBreakpointColor;
    }

    for (auto it = breakpointsByLine.cbegin(); it != breakpointsByLine.cend();
         ++it) {
      QTextBlock block = document()->findBlockByNumber(it.key() - 1);
      if (!block.isValid()) {
        continue;
      }

      QTextEdit::ExtraSelection selection;
      QColor breakpointHighlight = baseColor;
      if (!it.value().enabled) {
        breakpointHighlight = QColor(140, 140, 140);
      } else if (!it.value().verified) {
        breakpointHighlight = baseColor.lighter(115);
      }
      breakpointHighlight.setAlpha(60);

      selection.format.setBackground(breakpointHighlight);
      selection.format.setProperty(QTextFormat::FullWidthSelection, true);
      selection.cursor = QTextCursor(block);
      selection.cursor.clearSelection();
      extraSelections.append(selection);
    }
  }

  if (m_debugExecutionLine > 0) {
    QTextBlock debugBlock =
        document()->findBlockByNumber(m_debugExecutionLine - 1);
    if (debugBlock.isValid()) {
      QTextEdit::ExtraSelection selection;
      QColor debugColor = mainWindow
                              ? mainWindow->getTheme().debugCurrentLineColor
                              : QColor(255, 193, 7);
      debugColor.setAlpha(95);
      selection.format.setBackground(debugColor);
      selection.format.setProperty(QTextFormat::FullWidthSelection, true);
      selection.cursor = QTextCursor(debugBlock);
      selection.cursor.clearSelection();
      extraSelections.append(selection);
    }
  }

  for (const LspDiagnostic &diag : m_diagnostics) {
    int startLine = diag.range.start.line;
    int startCol = diag.range.start.character;
    int endLine = diag.range.end.line;
    int endCol = diag.range.end.character;

    QTextBlock startBlock = document()->findBlockByNumber(startLine);
    QTextBlock endBlock = document()->findBlockByNumber(endLine);
    if (!startBlock.isValid())
      continue;
    if (!endBlock.isValid())
      endBlock = startBlock;

    QTextEdit::ExtraSelection selection;

    QColor underlineColor;
    switch (diag.severity) {
    case LspDiagnosticSeverity::Error:
      underlineColor = mainWindow ? mainWindow->getTheme().diagnosticErrorColor
                                  : QColor(231, 76, 60);
      break;
    case LspDiagnosticSeverity::Warning:
      underlineColor = mainWindow
                           ? mainWindow->getTheme().diagnosticWarningColor
                           : QColor(241, 196, 15);
      break;
    case LspDiagnosticSeverity::Information:
      underlineColor = mainWindow ? mainWindow->getTheme().diagnosticInfoColor
                                  : QColor(52, 152, 219);
      break;
    case LspDiagnosticSeverity::Hint:
      underlineColor = mainWindow ? mainWindow->getTheme().diagnosticHintColor
                                  : QColor(149, 165, 166);
      break;
    }

    QTextCharFormat fmt;
    if (diag.severity == LspDiagnosticSeverity::Hint) {
      fmt.setUnderlineStyle(QTextCharFormat::DotLine);
    } else {
      fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    }
    fmt.setUnderlineColor(underlineColor);
    selection.format = fmt;

    QTextCursor diagCursor(startBlock);
    int maxStartCol = qMax(0, startBlock.length() - 1);
    diagCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                            qMin(startCol, maxStartCol));
    if (startLine == endLine && endCol > startCol) {
      diagCursor.movePosition(
          QTextCursor::Right, QTextCursor::KeepAnchor,
          qMin(endCol - startCol, qMax(0, startBlock.length() - 1 - startCol)));
    } else if (endLine > startLine) {
      diagCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    } else {
      diagCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }

    selection.cursor = diagCursor;
    extraSelections.append(selection);
  }

  if (lineHighlighted && !cursor.hasSelection()) {
    QTextEdit::ExtraSelection selection;
    QColor color =
        mainWindow ? mainWindow->getTheme().highlightColor : highlightColor;
    if (m_debugExecutionLine > 0 &&
        m_debugExecutionLine == cursor.blockNumber() + 1) {
      color = mainWindow ? mainWindow->getTheme().debugCurrentLineColor
                         : QColor(255, 193, 7);
      color.setAlpha(120);
    }
    if (breakpointsByLine.contains(cursor.blockNumber() + 1)) {
      color.setAlpha(qMin(color.alpha(), 160));
    }
    selection.format.setBackground(color);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = cursor;
    selection.cursor.clearSelection();
    extraSelections.append(selection);
  }

  constexpr int kBracketScanCharacterLimit = 200000;
  const bool canScanBrackets =
      document() && document()->characterCount() <= kBracketScanCharacterLimit;
  if (matchingBracketsHighlighted && canScanBrackets) {
    auto addBracketSelection =
        [&](QTextCursor::MoveOperation op, const QChar &startStr,
            const QChar &endStr,
            std::function<int(const QString &, int, QChar, QChar)> function) {
          QTextEdit::ExtraSelection selection;
          selection.format.setForeground(QColor("yellow"));

          QTextCursor current = textCursor();
          current.clearSelection();
          current.movePosition(op, QTextCursor::KeepAnchor);
          if (current.selectedText().isEmpty())
            return;

          selection.cursor = current;
          extraSelections.append(selection);

          auto plainText = toPlainText();
          auto pos =
              function(plainText, textCursor().position(), startStr, endStr);
          if (pos != -1) {
            QTextCursor match = textCursor();
            match.setPosition(pos);
            match.movePosition(op, QTextCursor::KeepAnchor);
            selection.cursor = match;
            extraSelections.append(selection);
          }
        };

    auto nextCursor = textCursor();
    auto nextResult = nextCursor.movePosition(QTextCursor::NextCharacter,
                                              QTextCursor::KeepAnchor);
    auto startStr = nextResult ? nextCursor.selectedText().front() : QChar(' ');

    auto prevCursor = textCursor();
    auto prevResult = prevCursor.movePosition(QTextCursor::PreviousCharacter,
                                              QTextCursor::KeepAnchor);
    auto endStr = prevResult ? prevCursor.selectedText().front() : QChar(' ');

    if (brackets.contains(startStr))
      addBracketSelection(QTextCursor::NextCharacter, startStr,
                          brackets[startStr], &findClosingParentheses);
    else if (brackets.values().contains(endStr))
      addBracketSelection(QTextCursor::PreviousCharacter, brackets.key(endStr),
                          endStr, &findOpeningParentheses);
  }

  setExtraSelections(extraSelections);
}

void TextArea::updateCursorPositionChangedCallbacks() {

  disconnect(this, &TextArea::cursorPositionChanged, 0, 0);
  disconnect(this, &TextArea::selectionChanged, 0, 0);

  auto refresh = [this]() {
    invalidateCompletionRequest();
    hideCompletionPopup();
    updateRowColDisplay();
    scheduleExtraSelectionsRefresh();
  };

  connect(this, &TextArea::cursorPositionChanged, this, refresh);
  connect(this, &TextArea::selectionChanged, this, refresh);

  refresh();
}

void TextArea::scheduleExtraSelectionsRefresh() {
  if (m_extraSelectionRefreshPending) {
    return;
  }

  m_extraSelectionRefreshPending = true;
  QTimer::singleShot(16, this, [this]() {
    m_extraSelectionRefreshPending = false;
    updateExtraSelections();
  });
}

void TextArea::lineNumberAreaPaintEvent(QPaintEvent *event) {

  Q_UNUSED(event);
  if (lineNumberArea) {
    lineNumberArea->update();
  }
}

void TextArea::updateSyntaxHighlightTags(QString searchKey,
                                         QString chosenLang) {
  bool languageChanged = false;
  if (!chosenLang.isNull()) {
    QString normalized = LanguageCatalog::normalize(chosenLang);
    languageChanged = (normalized != highlightLang);
    highlightLang = normalized;
  }

  const bool searchChanged = (searchWord != searchKey);
  searchWord = searchKey;

  auto colors = mainWindow->getTheme();

  if (!languageChanged && syntaxHighlighter) {
    if (auto *pluginHighlighter =
            qobject_cast<PluginBasedSyntaxHighlighter *>(syntaxHighlighter)) {
      if (searchChanged) {
        pluginHighlighter->setSearchKeyword(searchKey);
      }
      updateHighlighterViewport();
      return;
    }

    if (searchChanged) {
      syntaxHighlighter->rehighlight();
    }
    updateHighlighterViewport();
    return;
  }

  if (syntaxHighlighter) {
    delete syntaxHighlighter;
    syntaxHighlighter = nullptr;
  }

  auto &registry = SyntaxPluginRegistry::instance();
  ISyntaxPlugin *plugin = registry.getPluginByLanguageId(highlightLang);

  if (plugin && document()) {

    auto *pluginHighlighter =
        new PluginBasedSyntaxHighlighter(plugin, colors, searchKey, document());
    syntaxHighlighter = pluginHighlighter;
    updateHighlighterViewport();
    return;
  }

  updateHighlighterViewport();
}

void TextArea::updateHighlighterViewport() {
  if (!syntaxHighlighter) {
    return;
  }

  int firstVisible = firstVisibleBlock().blockNumber();
  int visibleLines = viewport()->height() / fontMetrics().height();
  int lastVisible = firstVisible + visibleLines + 1;

  if (auto *pluginHighlighter =
          qobject_cast<PluginBasedSyntaxHighlighter *>(syntaxHighlighter)) {
    pluginHighlighter->setVisibleBlockRange(firstVisible, lastVisible);
  }
}

void TextArea::setCompleter(QCompleter *completer) {
  if (m_completer)
    m_completer->disconnect(this);

  m_completer = completer;

  if (!m_completer)
    return;

  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::PopupCompletion);
  m_completer->setCaseSensitivity(Qt::CaseInsensitive);
  QObject::connect(m_completer,
                   QOverload<const QString &>::of(&QCompleter::activated), this,
                   &TextArea::insertCompletion);
}

QCompleter *TextArea::completer() const { return m_completer; }

void TextArea::insertCompletion(const QString &completion) {
  if (m_completer->widget() != this)
    return;
  QTextCursor tc = textCursor();
  int extra = completion.length() - m_completer->completionPrefix().length();
  tc.movePosition(QTextCursor::EndOfWord);
  tc.insertText(completion.right(extra));
  setTextCursor(tc);
  expandTabsToSpacesInDocument();
}

QString TextArea::textUnderCursor() const {
  QTextCursor tc = textCursor();
  tc.select(QTextCursor::WordUnderCursor);
  return tc.selectedText();
}

void TextArea::setCompletionEngine(CompletionEngine *engine) {
  if (m_completionEngine == engine)
    return;

  if (m_completionEngine) {
    disconnect(m_completionEngine, &CompletionEngine::completionsReady, this,
               &TextArea::onCompletionsReady);
  }

  m_completionEngine = engine;

  if (!m_completionEngine)
    return;

  if (!m_completionWidget) {
    m_completionWidget = new CompletionWidget(this);
    if (mainWindow) {
      m_completionWidget->applyTheme(ThemeEngine::instance().activeTheme());
    }
    connect(m_completionWidget, &CompletionWidget::itemAccepted, this,
            &TextArea::onCompletionAccepted);
    connect(m_completionWidget, &CompletionWidget::cancelled, this,
            &TextArea::hideCompletionPopup);
  }

  connect(m_completionEngine, &CompletionEngine::completionsReady, this,
          &TextArea::onCompletionsReady);

  m_completionEngine->setLanguage(m_languageId);
}

CompletionEngine *TextArea::completionEngine() const {
  return m_completionEngine;
}

void TextArea::setLanguage(const QString &languageId) {
  m_languageId = LanguageCatalog::normalize(languageId);
  if (m_languageId.isEmpty()) {
    m_languageId = languageId.trimmed().toLower();
  }
  if (m_completionEngine) {
    m_completionEngine->setLanguage(m_languageId);
  }
}

QString TextArea::language() const { return m_languageId; }

QString TextArea::getDocumentUri() const {
  return QString("file://%1").arg(objectName());
}

QString TextArea::resolveFilePath() const {
  QString filePath;

  QObject *parentObj = parent();
  while (parentObj && filePath.isEmpty()) {
    if (auto *page = qobject_cast<LightpadPage *>(parentObj)) {
      filePath = page->getFilePath();
      break;
    }
    parentObj = parentObj->parent();
  }

  if (filePath.isEmpty() && mainWindow) {
    LightpadTabWidget *tabWidget = mainWindow->currentTabWidget();
    filePath = tabWidget ? tabWidget->getFilePath(tabWidget->currentIndex())
                         : QString();
  }

  return filePath;
}

int TextArea::effectiveTabWidth() const {
  if (mainWindow) {
    return qMax(1, mainWindow->getTabWidth());
  }
  return 4;
}

bool TextArea::shouldExpandTabsToSpaces() const {
  const QString normalizedLanguage = LanguageCatalog::normalize(m_languageId);
  if (normalizedLanguage == "py") {
    return true;
  }

  if (!normalizedLanguage.isEmpty() && normalizedLanguage != "plaintext") {
    return false;
  }

  const QString filePath = resolveFilePath();
  if (FileManager::isPythonFile(filePath)) {
    return true;
  }

  return filePath.isEmpty() &&
         FileManager::isPythonFile(filePath, toPlainText());
}

void TextArea::expandTabsToSpacesInDocument() {
  if (!shouldExpandTabsToSpaces()) {
    return;
  }

  const QString text = toPlainText();
  if (!text.contains('\t')) {
    return;
  }

  const int tabWidth = effectiveTabWidth();
  const QString expanded = FileManager::expandTabsToSpaces(text, tabWidth);
  QTextCursor currentCursor = textCursor();
  const int position = currentCursor.position();
  const int anchor = currentCursor.anchor();

  QTextCursor editCursor(document());
  editCursor.beginEditBlock();
  editCursor.select(QTextCursor::Document);
  editCursor.insertText(expanded);
  editCursor.endEditBlock();

  QTextCursor restored(document());
  const int restoredAnchor = expandedPositionForTabs(text, anchor, tabWidth);
  const int restoredPosition =
      expandedPositionForTabs(text, position, tabWidth);
  restored.setPosition(qMin(restoredAnchor, expanded.length()));
  restored.setPosition(qMin(restoredPosition, expanded.length()),
                       QTextCursor::KeepAnchor);
  setTextCursor(restored);
}

void TextArea::triggerCompletion() {
  if (!m_completionEngine || !isCompletionEnabledForLanguage(m_languageId))
    return;

  QString prefix = textUnderCursor();
  QTextCursor cursor = textCursor();

  CompletionContext ctx;
  m_lastCompletionRequestPosition = cursor.position();
  ctx.documentUri = getDocumentUri();
  ctx.languageId = m_languageId;
  ctx.prefix = prefix;
  ctx.line = cursor.blockNumber();
  ctx.column = cursor.positionInBlock();
  ctx.lineText = cursor.block().text();
  ctx.triggerKind = CompletionTriggerKind::Invoked;
  ctx.isAutoComplete = false;

  m_completionEngine->requestCompletions(ctx);
}

void TextArea::onCompletionsReady(const QList<CompletionItem> &items) {
  if (m_lastCompletionRequestPosition != textCursor().position()) {
    hideCompletionPopup();
    return;
  }

  if (!isCompletionEnabledForLanguage(m_languageId)) {
    hideCompletionPopup();
    return;
  }

  QWidget *focusWidget = QApplication::focusWidget();
  bool isActiveEditor =
      (focusWidget == this) || (focusWidget && isAncestorOf(focusWidget));

  if (!isActiveEditor) {

    hideCompletionPopup();
    return;
  }

  if (items.isEmpty()) {
    hideCompletionPopup();
    return;
  }

  m_completionWidget->setItems(items);
  showCompletionPopup();
}

void TextArea::onCompletionAccepted(const CompletionItem &item) {
  insertCompletionItem(item);
  hideCompletionPopup();
}

void TextArea::insertCompletionItem(const CompletionItem &item) {
  QTextCursor tc = textCursor();
  QString prefix = textUnderCursor();

  tc.movePosition(QTextCursor::EndOfWord);
  tc.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);

  QString insertText = item.effectiveInsertText();

  if (item.isSnippet) {

    static const QRegularExpression tabstopWithDefaultRe(
        R"(\$\{(\d+):([^}]*)\})");
    insertText.replace(tabstopWithDefaultRe, "\\2");
    static const QRegularExpression tabstopNoDefaultRe(R"(\$\{(\d+)\})");
    insertText.replace(tabstopNoDefaultRe, "");
    static const QRegularExpression simpleTabstopRe(R"(\$(\d+))");
    insertText.replace(simpleTabstopRe, "");
  }

  tc.insertText(insertText);
  setTextCursor(tc);
  expandTabsToSpacesInDocument();
}

void TextArea::showCompletionPopup() {
  if (!m_completionWidget)
    return;

  QRect cr = cursorRect();
  QPoint pos = mapToGlobal(cr.bottomLeft());
  m_completionWidget->showAt(pos);
}

void TextArea::hideCompletionPopup() {
  if (m_completionWidget) {
    m_completionWidget->hide();
  }
}

void TextArea::invalidateCompletionRequest() {
  m_lastCompletionRequestPosition = -1;
}

void TextArea::addCursorAbove() {
  if (m_multiCursor) {
    m_multiCursor->addCursorAbove();
    drawExtraCursors();
  }
}

void TextArea::addCursorBelow() {
  if (m_multiCursor) {
    m_multiCursor->addCursorBelow();
    drawExtraCursors();
  }
}

void TextArea::addCursorAtNextOccurrence() {
  if (m_multiCursor) {
    m_multiCursor->addCursorAtNextOccurrence();
    drawExtraCursors();
  }
}

void TextArea::addCursorsToAllOccurrences() {
  if (m_multiCursor) {
    m_multiCursor->addCursorsToAllOccurrences();
    drawExtraCursors();
  }
}

void TextArea::clearExtraCursors() {
  if (m_multiCursor) {
    m_multiCursor->clearExtraCursors();
  }
  viewport()->update();
}

bool TextArea::hasMultipleCursors() const {
  return m_multiCursor ? m_multiCursor->hasMultipleCursors() : false;
}

int TextArea::cursorCount() const {
  return m_multiCursor ? m_multiCursor->cursorCount() : 1;
}

void TextArea::drawExtraCursors() {
  if (m_multiCursor) {
    m_multiCursor->updateExtraSelections(highlightColor);
  }
  viewport()->update();
}

void TextArea::applyToAllCursors(
    const std::function<void(QTextCursor &)> &operation) {
  if (m_multiCursor) {
    m_multiCursor->applyToAllCursors(operation);
    drawExtraCursors();
  } else {

    QTextCursor cursor = textCursor();
    operation(cursor);
    setTextCursor(cursor);
  }
}

void TextArea::paintEvent(QPaintEvent *event) {
  QPlainTextEdit::paintEvent(event);

  if (m_showIndentGuides) {
    QPainter painter(viewport());
    painter.setPen(QPen(QColor(128, 128, 128, 60), 1, Qt::DotLine));

    QFontMetrics fm(mainFont);
    int spaceWidth = fm.horizontalAdvance(' ');
    int indentWidth = spaceWidth * 4;

    QTextBlock block = firstVisibleBlock();
    int top =
        qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
      if (block.isVisible() && bottom >= event->rect().top()) {
        QString text = block.text();

        int indent = 0;
        for (QChar c : text) {
          if (c == ' ')
            indent++;
          else if (c == '\t')
            indent += 4;
          else
            break;
        }

        QTextCursor blockStart(block);
        blockStart.setPosition(block.position());
        QRect startRect = cursorRect(blockStart);
        int xOffset = startRect.left();

        int numGuides = indent / 4;
        for (int i = 1; i <= numGuides; ++i) {
          int x = xOffset + (i * indentWidth) - indentWidth;
          painter.drawLine(x, top, x, bottom);
        }
      }

      block = block.next();
      top = bottom;
      bottom = top + qRound(blockBoundingRect(block).height());
    }
  }

  if (m_showWhitespace) {
    QPainter painter(viewport());
    painter.setPen(QPen(QColor(128, 128, 128, 80), 1));

    QFontMetrics fm(mainFont);
    int spaceWidth = fm.horizontalAdvance(' ');
    int tabWidth = fm.horizontalAdvance(' ') * 4;

    QTextBlock block = firstVisibleBlock();
    int top =
        qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
      if (block.isVisible() && bottom >= event->rect().top()) {
        QString text = block.text();

        QTextCursor blockStart(block);
        blockStart.setPosition(block.position());
        QRect startRect = cursorRect(blockStart);
        int xOffset = startRect.left();
        int yCenter = startRect.center().y();

        int x = xOffset;
        for (int i = 0; i < text.length(); ++i) {
          if (text[i] == ' ') {

            painter.drawPoint(x + spaceWidth / 2, yCenter);
            x += spaceWidth;
          } else if (text[i] == '\t') {

            int arrowEnd = x + 10;
            painter.drawLine(x + 2, yCenter, arrowEnd, yCenter);
            painter.drawLine(arrowEnd - 3, yCenter - 3, arrowEnd, yCenter);
            painter.drawLine(arrowEnd - 3, yCenter + 3, arrowEnd, yCenter);
            x += tabWidth;
          } else {
            x += fm.horizontalAdvance(text[i]);
          }
        }
      }

      block = block.next();
      top = bottom;
      bottom = top + qRound(blockBoundingRect(block).height());
    }
  }

  if (m_codeLensEnabled && !m_codeLensEntries.isEmpty()) {
    QPainter painter(viewport());
    QFont codeLensFont = mainFont;
    codeLensFont.setPointSizeF(mainFont.pointSizeF() * 0.85);
    codeLensFont.setItalic(true);
    painter.setFont(codeLensFont);
    QColor codeLensColor(160, 160, 160, 180);
    painter.setPen(codeLensColor);

    for (const CodeLensEntry &entry : m_codeLensEntries) {
      QTextBlock block = document()->findBlockByNumber(entry.line);
      if (!block.isValid() || !block.isVisible())
        continue;

      QRectF blockGeom =
          blockBoundingGeometry(block).translated(contentOffset());
      if (blockGeom.bottom() < 0 || blockGeom.top() > viewport()->height())
        continue;

      QFontMetrics cfm(codeLensFont);
      int yPos = static_cast<int>(blockGeom.top()) - cfm.height() + 2;
      if (yPos < 0)
        continue;

      QTextCursor blockStart(block);
      blockStart.setPosition(block.position());
      int xPos = cursorRect(blockStart).left();

      painter.drawText(xPos, yPos, viewport()->width() - xPos, cfm.height(),
                       Qt::AlignVCenter | Qt::AlignLeft, entry.text);
    }
  }

  if (m_inlineBlameEnabled && !m_inlineBlameData.isEmpty()) {
    int currentLine = textCursor().blockNumber() + 1;
    auto it = m_inlineBlameData.find(currentLine);
    if (it != m_inlineBlameData.end()) {
      QPainter painter(viewport());
      QTextBlock block = document()->findBlockByNumber(currentLine - 1);
      if (block.isValid() && block.isVisible()) {
        QRectF blockGeom =
            blockBoundingGeometry(block).translated(contentOffset());
        QString lineText = block.text();
        QFontMetrics fm(mainFont);
        int textWidth = fm.horizontalAdvance(lineText);

        QTextCursor blockStart(block);
        blockStart.setPosition(block.position());
        QRect startRect = cursorRect(blockStart);
        int xPos = startRect.left() + textWidth + fm.horizontalAdvance("    ");
        int yPos = static_cast<int>(blockGeom.top());

        QColor ghostColor(128, 128, 128, 140);
        painter.setPen(ghostColor);
        QFont ghostFont = mainFont;
        ghostFont.setItalic(true);
        painter.setFont(ghostFont);
        painter.drawText(xPos, yPos, viewport()->width() - xPos, fm.height(),
                         Qt::AlignVCenter | Qt::AlignLeft, it.value());
      }
    }
  }

  if (m_multiCursor && m_multiCursor->hasMultipleCursors()) {
    QPainter painter(viewport());
    painter.setPen(QPen(defaultPenColor, 2));

    for (const QTextCursor &cursor : m_multiCursor->extraCursors()) {
      if (!cursor.hasSelection()) {
        QRect cursorRect = this->cursorRect(cursor);
        painter.drawLine(cursorRect.topLeft(), cursorRect.bottomLeft());
      }
    }
  }

  if (hasFocus()) {
    QPainter glowPainter(viewport());
    glowPainter.setRenderHint(QPainter::Antialiasing, true);
    QRect cr = cursorRect();
    QColor glow = ThemeEngine::instance().activeTheme().colors.accentPrimary;
    glow.setAlpha(50);
    glowPainter.setPen(Qt::NoPen);
    glowPainter.setBrush(glow);
    QRect halo = cr.adjusted(-2, -1, 3, 1);
    glowPainter.drawRoundedRect(halo, 2, 2);
  }
}

void TextArea::mousePressEvent(QMouseEvent *event) {
  invalidateCompletionRequest();
  hideCompletionPopup();

  if (m_multiCursor && m_multiCursor->hasMultipleCursors() &&
      !(event->modifiers() & Qt::ControlModifier)) {
    clearExtraCursors();
  }

  if ((event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier)) ==
          (Qt::AltModifier | Qt::ShiftModifier) &&
      event->button() == Qt::LeftButton) {
    startColumnSelection(event->pos());
    return;
  }

  if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) ==
      (Qt::ControlModifier | Qt::AltModifier)) {
    QTextCursor cursor = cursorForPosition(event->pos());
    if (m_multiCursor) {
      m_multiCursor->extraCursorsRef().append(textCursor());
    }
    setTextCursor(cursor);
    drawExtraCursors();
    return;
  }

  if (event->button() == Qt::LeftButton &&
      (event->modifiers() & Qt::ControlModifier) &&
      !(event->modifiers() & Qt::AltModifier)) {
    QTextCursor cursor = cursorForPosition(event->pos());
    setTextCursor(cursor);
    if (mainWindow) {
      mainWindow->goToDefinitionAtCursor();
    }
    return;
  }

  QPlainTextEdit::mousePressEvent(event);
}

void TextArea::mouseMoveEvent(QMouseEvent *event) {

  if (m_columnSelectionActive && (event->buttons() & Qt::LeftButton)) {
    updateColumnSelection(event->pos());
    return;
  }

  QPlainTextEdit::mouseMoveEvent(event);
}

void TextArea::mouseReleaseEvent(QMouseEvent *event) {

  if (m_columnSelectionActive) {
    endColumnSelection();
    return;
  }

  QPlainTextEdit::mouseReleaseEvent(event);
}

void TextArea::foldCurrentBlock() {
  int blockNum = textCursor().blockNumber();
  if (m_codeFolding->foldBlock(blockNum)) {
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
  }
}

void TextArea::unfoldCurrentBlock() {
  int blockNum = textCursor().blockNumber();
  if (m_codeFolding->unfoldBlock(blockNum)) {
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
  }
}

void TextArea::foldAll() {
  m_codeFolding->foldAll();
  viewport()->update();
  document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::unfoldAll() {
  m_codeFolding->unfoldAll();
  viewport()->update();
  document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::toggleFoldAtLine(int line) {
  m_codeFolding->toggleFoldAtLine(line);
  viewport()->update();
  document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::foldToLevel(int level) {
  m_codeFolding->foldToLevel(level);
  viewport()->update();
  document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::setShowWhitespace(bool show) {
  if (m_showWhitespace != show) {
    m_showWhitespace = show;
    viewport()->update();
  }
}

bool TextArea::showWhitespace() const { return m_showWhitespace; }

void TextArea::setShowIndentGuides(bool show) {
  if (m_showIndentGuides != show) {
    m_showIndentGuides = show;
    viewport()->update();
  }
}

bool TextArea::showIndentGuides() const { return m_showIndentGuides; }

void TextArea::setVimModeEnabled(bool enabled) {
  if (m_vimMode) {
    m_vimMode->setEnabled(enabled);
  }
}

bool TextArea::isVimModeEnabled() const {
  return m_vimMode && m_vimMode->isEnabled();
}

VimMode *TextArea::vimMode() const { return m_vimMode; }

void TextArea::setGitDiffLines(const QList<QPair<int, int>> &diffLines) {
  m_gitDiffLines = diffLines;
  if (lineNumberArea) {
    lineNumberArea->setGitDiffLines(diffLines);
  }
}

void TextArea::clearGitDiffLines() {
  m_gitDiffLines.clear();
  if (lineNumberArea) {
    lineNumberArea->clearGitDiffLines();
  }
}

void TextArea::setGitBlameLines(const QMap<int, QString> &blameLines) {
  m_gitBlameLines = blameLines;
  if (lineNumberArea) {
    lineNumberArea->setGitBlameLines(blameLines);
    updateLineNumberAreaLayout();
  }
}

void TextArea::clearGitBlameLines() {
  m_gitBlameLines.clear();
  if (lineNumberArea) {
    lineNumberArea->clearGitBlameLines();
    updateLineNumberAreaLayout();
  }
}

void TextArea::setRichBlameData(const QMap<int, GitBlameLineInfo> &blameData) {
  if (lineNumberArea) {
    lineNumberArea->setRichBlameData(blameData);
  }
}

void TextArea::setGutterGitIntegration(GitIntegration *git) {
  if (lineNumberArea) {
    lineNumberArea->setGitIntegration(git);
  }
}

void TextArea::setInlineBlameData(const QMap<int, QString> &blameData) {
  m_inlineBlameData = blameData;
  viewport()->update();
}

void TextArea::clearInlineBlameData() {
  m_inlineBlameData.clear();
  viewport()->update();
}

void TextArea::setInlineBlameEnabled(bool enabled) {
  m_inlineBlameEnabled = enabled;
  if (!enabled) {
    m_lastInlineBlameLine = -1;
  }
  viewport()->update();
}

bool TextArea::isInlineBlameEnabled() const { return m_inlineBlameEnabled; }

void TextArea::setHeatmapData(const QMap<int, qint64> &timestamps) {
  if (lineNumberArea)
    lineNumberArea->setHeatmapData(timestamps);
}

void TextArea::setHeatmapEnabled(bool enabled) {
  if (lineNumberArea)
    lineNumberArea->setHeatmapEnabled(enabled);
}

bool TextArea::isHeatmapEnabled() const {
  return lineNumberArea ? lineNumberArea->isHeatmapEnabled() : false;
}

void TextArea::setCodeLensEntries(const QList<CodeLensEntry> &entries) {
  m_codeLensEntries = entries;
  viewport()->update();
}

void TextArea::clearCodeLensEntries() {
  m_codeLensEntries.clear();
  viewport()->update();
}

void TextArea::setCodeLensEnabled(bool enabled) {
  m_codeLensEnabled = enabled;
  viewport()->update();
}

bool TextArea::isCodeLensEnabled() const { return m_codeLensEnabled; }

void TextArea::setDebugExecutionLine(int line) {
  const int normalizedLine = line > 0 ? line : 0;
  if (m_debugExecutionLine == normalizedLine) {
    return;
  }
  m_debugExecutionLine = normalizedLine;
  updateExtraSelections();
}

void TextArea::updateLineNumberAreaLayout() {
  if (!lineNumberArea) {
    return;
  }

  int width = showLineNumberArea ? lineNumberArea->calculateWidth() : 0;
  setViewportMargins(width, 0, 0, 0);
  lineNumberArea->setFixedWidth(width);
  lineNumberArea->setGeometry(0, 0, width, height());
  viewport()->update();
}

void TextArea::startColumnSelection(const QPoint &pos) {
  m_columnSelectionActive = true;
  m_columnSelectionStart = pos;
  m_columnSelectionEnd = pos;

  if (m_multiCursor) {
    m_multiCursor->clearExtraCursors();
  }

  QTextCursor cursor = cursorForPosition(pos);
  cursor.clearSelection();
  setTextCursor(cursor);
}

void TextArea::updateColumnSelection(const QPoint &pos) {
  if (!m_columnSelectionActive)
    return;

  m_columnSelectionEnd = pos;

  QTextCursor startCursor = cursorForPosition(m_columnSelectionStart);
  QTextCursor endCursor = cursorForPosition(m_columnSelectionEnd);

  int startLine = startCursor.blockNumber();
  int endLine = endCursor.blockNumber();

  if (startLine > endLine) {
    std::swap(startLine, endLine);
  }

  int startCol = startCursor.positionInBlock();
  int endCol = endCursor.positionInBlock();

  int leftCol = qMin(startCol, endCol);
  int rightCol = qMax(startCol, endCol);

  if (m_multiCursor) {
    m_multiCursor->clearExtraCursors();
  }

  bool first = true;
  for (int line = startLine; line <= endLine; ++line) {
    QTextBlock block = document()->findBlockByNumber(line);
    if (!block.isValid())
      continue;

    QTextCursor cursor(block);
    int lineLength = block.text().length();

    int actualLeftCol = qMin(leftCol, lineLength);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                        actualLeftCol);

    int actualRightCol = qMin(rightCol, lineLength);
    int selectionLength = actualRightCol - actualLeftCol;
    if (selectionLength > 0) {
      cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                          selectionLength);
    }

    if (first) {
      setTextCursor(cursor);
      first = false;
    } else if (m_multiCursor) {
      m_multiCursor->extraCursorsRef().append(cursor);
    }
  }

  drawExtraCursors();
}

void TextArea::endColumnSelection() { m_columnSelectionActive = false; }

void TextArea::splitSelectionIntoLines() {
  QTextCursor cursor = textCursor();

  if (!cursor.hasSelection())
    return;

  int selStart = cursor.selectionStart();
  int selEnd = cursor.selectionEnd();

  QTextCursor startCursor(document());
  startCursor.setPosition(selStart);
  int startLine = startCursor.blockNumber();
  int startCol = startCursor.positionInBlock();

  QTextCursor endCursor(document());
  endCursor.setPosition(selEnd);
  int endLine = endCursor.blockNumber();
  int endCol = endCursor.positionInBlock();

  if (startLine == endLine)
    return;

  if (m_multiCursor) {
    m_multiCursor->clearExtraCursors();
  }

  bool first = true;
  for (int line = startLine; line <= endLine; ++line) {
    QTextBlock block = document()->findBlockByNumber(line);
    if (!block.isValid())
      continue;

    QTextCursor lineCursor(block);
    lineCursor.movePosition(QTextCursor::StartOfBlock);

    if (line == startLine) {

      lineCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                              startCol);
      lineCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    } else if (line == endLine) {

      lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                              endCol);
    } else {

      lineCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }

    if (first) {
      setTextCursor(lineCursor);
      first = false;
    } else if (m_multiCursor) {
      m_multiCursor->extraCursorsRef().append(lineCursor);
    }
  }

  drawExtraCursors();
}

void TextArea::foldComments() {
  m_codeFolding->foldComments();
  viewport()->update();
  document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::unfoldComments() {
  m_codeFolding->unfoldComments();
  viewport()->update();
  document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::sortLinesAscending() {
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::Document);
  }

  QString selectedText = cursor.selectedText();
  selectedText.replace(QChar::ParagraphSeparator, '\n');
  cursor.insertText(TextTransforms::sortLinesAscending(selectedText));
}

void TextArea::sortLinesDescending() {
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::Document);
  }

  QString selectedText = cursor.selectedText();
  selectedText.replace(QChar::ParagraphSeparator, '\n');
  cursor.insertText(TextTransforms::sortLinesDescending(selectedText));
}

void TextArea::transformToUppercase() {
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
  }

  if (cursor.hasSelection()) {
    cursor.insertText(TextTransforms::toUppercase(cursor.selectedText()));
  }
}

void TextArea::transformToLowercase() {
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
  }

  if (cursor.hasSelection()) {
    cursor.insertText(TextTransforms::toLowercase(cursor.selectedText()));
  }
}

void TextArea::transformToTitleCase() {
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
  }

  if (cursor.hasSelection()) {
    cursor.insertText(TextTransforms::toTitleCase(cursor.selectedText()));
  }
}

void TextArea::setWordWrapEnabled(bool enabled) {
  if (enabled) {
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
  } else {
    setLineWrapMode(QPlainTextEdit::NoWrap);
  }
}

bool TextArea::wordWrapEnabled() const {
  return lineWrapMode() == QPlainTextEdit::WidgetWidth;
}

void TextArea::setDiagnostics(const QList<LspDiagnostic> &diagnostics) {
  m_diagnostics = diagnostics;

  QMap<int, LspDiagnosticSeverity> diagnosticLines;
  for (const LspDiagnostic &diag : diagnostics) {
    int line = diag.range.start.line + 1;
    if (!diagnosticLines.contains(line) ||
        static_cast<int>(diag.severity) <
            static_cast<int>(diagnosticLines[line])) {
      diagnosticLines[line] = diag.severity;
    }
  }
  if (lineNumberArea) {
    lineNumberArea->setDiagnosticLines(diagnosticLines);
  }

  scheduleExtraSelectionsRefresh();
}

void TextArea::clearDiagnostics() {
  m_diagnostics.clear();
  if (lineNumberArea) {
    lineNumberArea->clearDiagnosticLines();
  }
  scheduleExtraSelectionsRefresh();
}
