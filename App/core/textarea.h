#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QList>
#include <QMap>
#include <QPlainTextEdit>
#include <QSet>
#include <QTextCursor>
#include <functional>

#include "../editor/vimmode.h"

class MainWindow;
class QSyntaxHighlighter;
class QCompleter;
class CompletionEngine;
class CompletionWidget;
class LineNumberArea;
class MultiCursorHandler;
class CodeFoldingManager;
class GitIntegration;
struct GitBlameLineInfo;
struct CompletionItem;
struct TextAreaSettings;
struct Theme;

class TextArea : public QPlainTextEdit {

  Q_OBJECT

  friend class LineNumberArea;

public:
  TextArea(QWidget *parent = nullptr);
  TextArea(const TextAreaSettings &settings, QWidget *parent = nullptr);
  void lineNumberAreaPaintEvent(QPaintEvent *event);
  void updateSyntaxHighlightTags(QString searchKey = QString(),
                                 QString chosenLang = QString());
  void increaseFontSize();
  void decreaseFontSize();
  void setFontSize(int size);
  void setFont(QFont font);
  void setPlainText(const QString &text);
  void setMainWindow(MainWindow *window);
  void setTabWidth(int width);
  void removeIconUnsaved();
  void setAutoIdent(bool flag);
  void showLineNumbers(bool flag);
  void highlihtCurrentLine(bool flag);
  void highlihtMatchingBracket(bool flag);
  void loadSettings(const TextAreaSettings settings);
  void applySelectionPalette(const Theme &theme);

  // Legacy completer support (deprecated - use setCompletionEngine)
  void setCompleter(QCompleter *completer);
  QCompleter *completer() const;

  // New completion system
  void setCompletionEngine(CompletionEngine *engine);
  CompletionEngine *completionEngine() const;
  void setLanguage(const QString &languageId);
  QString language() const;
  void triggerCompletion();

  int lineNumberAreaWidth();
  int fontSize();
  QString getSearchWord();
  bool changesUnsaved();

  // Multi-cursor support
  void addCursorAbove();
  void addCursorBelow();
  void addCursorAtNextOccurrence();
  void addCursorsToAllOccurrences();
  void clearExtraCursors();
  bool hasMultipleCursors() const;
  int cursorCount() const;
  void applyToAllCursors(const std::function<void(QTextCursor &)> &operation);
  void splitSelectionIntoLines();
  void startColumnSelection(const QPoint &pos);
  void updateColumnSelection(const QPoint &pos);
  void endColumnSelection();

  // Code folding
  void foldCurrentBlock();
  void unfoldCurrentBlock();
  void foldAll();
  void unfoldAll();
  void toggleFoldAtLine(int line);
  void foldToLevel(int level);
  void foldComments();
  void unfoldComments();

  // Text transformations
  void sortLinesAscending();
  void sortLinesDescending();
  void transformToUppercase();
  void transformToLowercase();
  void transformToTitleCase();

  // Word wrap
  void setWordWrapEnabled(bool enabled);
  bool wordWrapEnabled() const;

  // Whitespace visualization
  void setShowWhitespace(bool show);
  bool showWhitespace() const;

  // Indent guides
  void setShowIndentGuides(bool show);
  bool showIndentGuides() const;

  // Vim mode
  void setVimModeEnabled(bool enabled);
  bool isVimModeEnabled() const;
  VimMode *vimMode() const;

  // Git diff gutter
  void setGitDiffLines(
      const QList<QPair<int, int>>
          &diffLines); // line number, type (0=add, 1=modify, 2=delete)
  void clearGitDiffLines();
  void setGitBlameLines(const QMap<int, QString> &blameLines);
  void clearGitBlameLines();

  // Rich blame data for hover tooltips
  void setRichBlameData(const QMap<int, GitBlameLineInfo> &blameData);
  void setGutterGitIntegration(GitIntegration *git);

  // Heatmap (age-based gutter coloring)
  void setHeatmapData(const QMap<int, qint64> &timestamps);
  void setHeatmapEnabled(bool enabled);
  bool isHeatmapEnabled() const;

  // CodeLens (git info above functions/classes)
  struct CodeLensEntry {
    int line;               ///< 0-based line number
    QString text;           ///< Display text (e.g., "2 authors | 5 changes")
    QString symbolName;     ///< Name of the symbol
  };
  void setCodeLensEntries(const QList<CodeLensEntry> &entries);
  void clearCodeLensEntries();
  void setCodeLensEnabled(bool enabled);
  bool isCodeLensEnabled() const;

  // Inline blame (GitLens-style ghost text at end of current line)
  void setInlineBlameData(
      const QMap<int, QString> &blameData); // line -> "author, time • summary"
  void clearInlineBlameData();
  void setInlineBlameEnabled(bool enabled);
  bool isInlineBlameEnabled() const;

  void setDebugExecutionLine(int line);
  int debugExecutionLine() const { return m_debugExecutionLine; }

protected:
  void resizeEvent(QResizeEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  MainWindow *mainWindow;
  LineNumberArea *lineNumberArea;
  QColor highlightColor;
  QColor lineNumberAreaPenColor;
  QColor defaultPenColor;
  QColor backgroundColor;
  QString bufferText;
  QString highlightLang;
  QFont mainFont;
  QSyntaxHighlighter *syntaxHighlighter;
  QCompleter *m_completer; // Legacy - deprecated
  CompletionEngine *m_completionEngine;
  CompletionWidget *m_completionWidget;
  QString m_languageId;
  QString searchWord;
  bool areChangesUnsaved;
  bool autoIndent;
  bool showLineNumberArea;
  bool lineHighlighted;
  bool matchingBracketsHighlighted;
  int prevWordCount;

  // Cached icons to avoid repeated disk loads
  static QIcon s_unsavedIcon;
  static bool s_iconsInitialized;
  static void initializeIconCache();

  // Multi-cursor handler
  MultiCursorHandler *m_multiCursor;

  // Code folding handler
  CodeFoldingManager *m_codeFolding;

  // Column selection state
  bool m_columnSelectionActive;
  QPoint m_columnSelectionStart;
  QPoint m_columnSelectionEnd;

  // Whitespace visualization
  bool m_showWhitespace;

  // Indent guides
  bool m_showIndentGuides;

  // Vim mode
  VimMode *m_vimMode;

  // Git diff gutter
  QList<QPair<int, int>>
      m_gitDiffLines; // line number, type (0=add, 1=modify, 2=delete)
  QMap<int, QString> m_gitBlameLines;

  // Inline blame (ghost text)
  QMap<int, QString> m_inlineBlameData; // line -> blame text
  bool m_inlineBlameEnabled;
  int m_lastInlineBlameLine;

  // CodeLens
  QList<CodeLensEntry> m_codeLensEntries;
  bool m_codeLensEnabled;

  int m_debugExecutionLine;

  void setupTextArea();
  void setTabWidgetIcon(QIcon icon);
  void closeParentheses(QString startSr, QString closeStr);
  void handleKeyEnterPressed();
  void drawCurrentLineHighlight();
  void clearLineHighlight();
  void updateRowColDisplay();
  void drawMatchingBrackets();
  void updateExtraSelections();
  void updateCursorPositionChangedCallbacks();
  void insertCompletion(const QString &completion);
  void insertCompletionItem(const CompletionItem &item);
  QString textUnderCursor() const;
  QString getDocumentUri() const;
  void showCompletionPopup();
  void hideCompletionPopup();
  void applyLineSpacing(int percent);
  void updateHighlighterViewport();
  void updateLineNumberAreaLayout();
  QString resolveFilePath() const;

private slots:
  void onCompletionsReady(const QList<CompletionItem> &items);
  void onCompletionAccepted(const CompletionItem &item);

private:
  // Multi-cursor helper
  void drawExtraCursors();
};

#endif
