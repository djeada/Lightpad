#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QList>
#include <QTextCursor>
#include <QSet>
#include <functional>

class MainWindow;
class QSyntaxHighlighter;
class QCompleter;
class CompletionEngine;
class CompletionWidget;
struct CompletionItem;
struct TextAreaSettings;

class TextArea : public QPlainTextEdit {

    Q_OBJECT

public:
    TextArea(QWidget* parent = nullptr);
    TextArea(const TextAreaSettings& settings, QWidget* parent = nullptr);
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    void updateSyntaxHighlightTags(QString searchKey = "", QString chosenLang = "");
    void increaseFontSize();
    void decreaseFontSize();
    void setFontSize(int size);
    void setFont(QFont font);
    void setPlainText(const QString& text);
    void setMainWindow(MainWindow* window);
    void setTabWidth(int width);
    void removeIconUnsaved();
    void setAutoIdent(bool flag);
    void showLineNumbers(bool flag);
    void highlihtCurrentLine(bool flag);
    void highlihtMatchingBracket(bool flag);
    void loadSettings(const TextAreaSettings settings);
    
    // Legacy completer support (deprecated - use setCompletionEngine)
    void setCompleter(QCompleter* completer);
    QCompleter* completer() const;
    
    // New completion system
    void setCompletionEngine(CompletionEngine* engine);
    CompletionEngine* completionEngine() const;
    void setLanguage(const QString& languageId);
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

    // Code folding
    void foldCurrentBlock();
    void unfoldCurrentBlock();
    void foldAll();
    void unfoldAll();
    void toggleFoldAtLine(int line);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    MainWindow* mainWindow;
    QWidget* lineNumberArea;
    QColor highlightColor;
    QColor lineNumberAreaPenColor;
    QColor defaultPenColor;
    QColor backgroundColor;
    QString bufferText;
    QString highlightLang;
    QFont mainFont;
    QSyntaxHighlighter* syntaxHighlighter;
    QCompleter* m_completer;  // Legacy - deprecated
    CompletionEngine* m_completionEngine;
    CompletionWidget* m_completionWidget;
    QString m_languageId;
    QString searchWord;
    bool areChangesUnsaved;
    bool autoIndent;
    bool showLineNumberArea;
    bool lineHighlighted;
    bool matchingBracketsHighlighted;
    int prevWordCount;

    // Multi-cursor state
    QList<QTextCursor> m_extraCursors;
    QString m_lastSelectedWord;

    // Folding state
    QSet<int> m_foldedBlocks;

    void setupTextArea();
    void setTabWidgetIcon(QIcon icon);
    void closeParentheses(QString startSr, QString closeStr);
    void handleKeyEnterPressed();
    void drawCurrentLineHighlight();
    void clearLineHighlight();
    void updateRowColDisplay();
    void drawMatchingBrackets();
    void updateCursorPositionChangedCallbacks();
    void insertCompletion(const QString& completion);
    void insertCompletionItem(const CompletionItem& item);
    QString textUnderCursor() const;
    QString getDocumentUri() const;
    void showCompletionPopup();
    void hideCompletionPopup();
    void applyLineSpacing(int percent);
    
private slots:
    void onCompletionsReady(const QList<CompletionItem>& items);
    void onCompletionAccepted(const CompletionItem& item);

private:
    // Multi-cursor helpers
    void drawExtraCursors();
    void applyToAllCursors(const std::function<void(QTextCursor&)>& operation);
    void mergeOverlappingCursors();

    // Folding helpers
    int findFoldEndBlock(int startBlock) const;
    bool isFoldable(int blockNumber) const;
};

#endif
