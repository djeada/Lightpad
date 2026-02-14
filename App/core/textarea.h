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

  void setCompleter(QCompleter *completer);
  QCompleter *completer() const;

  void setCompletionEngine(CompletionEngine *engine);
  CompletionEngine *completionEngine() const;
  void setLanguage(const QString &languageId);
  QString language() const;
  void triggerCompletion();

  int lineNumberAreaWidth();
  int fontSize();
  QString getSearchWord();
  bool changesUnsaved();

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

  void foldCurrentBlock();
  void unfoldCurrentBlock();
  void foldAll();
  void unfoldAll();
  void toggleFoldAtLine(int line);
  void foldToLevel(int level);
  void foldComments();
  void unfoldComments();

  void sortLinesAscending();
  void sortLinesDescending();
  void transformToUppercase();
  void transformToLowercase();
  void transformToTitleCase();

  void setWordWrapEnabled(bool enabled);
  bool wordWrapEnabled() const;

  void setShowWhitespace(bool show);
  bool showWhitespace() const;

  void setShowIndentGuides(bool show);
  bool showIndentGuides() const;

  void setVimModeEnabled(bool enabled);
  bool isVimModeEnabled() const;
  VimMode *vimMode() const;

  void setGitDiffLines(const QList<QPair<int, int>> &diffLines);
  void clearGitDiffLines();
  void setGitBlameLines(const QMap<int, QString> &blameLines);
  void clearGitBlameLines();

  void setRichBlameData(const QMap<int, GitBlameLineInfo> &blameData);
  void setGutterGitIntegration(GitIntegration *git);

  void setHeatmapData(const QMap<int, qint64> &timestamps);
  void setHeatmapEnabled(bool enabled);
  bool isHeatmapEnabled() const;

  struct CodeLensEntry {
    int line;
    QString text;
    QString symbolName;
  };
  void setCodeLensEntries(const QList<CodeLensEntry> &entries);
  void clearCodeLensEntries();
  void setCodeLensEnabled(bool enabled);
  bool isCodeLensEnabled() const;

  void setInlineBlameData(const QMap<int, QString> &blameData);
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
  QCompleter *m_completer;
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

  static QIcon s_unsavedIcon;
  static bool s_iconsInitialized;
  static void initializeIconCache();

  MultiCursorHandler *m_multiCursor;

  CodeFoldingManager *m_codeFolding;

  bool m_columnSelectionActive;
  QPoint m_columnSelectionStart;
  QPoint m_columnSelectionEnd;

  bool m_showWhitespace;

  bool m_showIndentGuides;

  VimMode *m_vimMode;

  QList<QPair<int, int>> m_gitDiffLines;
  QMap<int, QString> m_gitBlameLines;

  QMap<int, QString> m_inlineBlameData;
  bool m_inlineBlameEnabled;
  int m_lastInlineBlameLine;

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
  void drawExtraCursors();
};

#endif
