#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>

class MainWindow;
class LightpadSyntaxHighlighter;
struct TextAreaSettings;

class TextArea : public QPlainTextEdit {

    Q_OBJECT

    public:
        TextArea(QWidget* parent = nullptr);
        TextArea(TextAreaSettings settings, QWidget* parent = nullptr);
        void lineNumberAreaPaintEvent(QPaintEvent* event);
        void updateSyntaxHighlightTags(QString searchKey = "", QString chosenLang = "");
        int lineNumberAreaWidth();
        void increaseFontSize();
        void decreaseFontSize();
        void setFontSize(int size);
        void setFont(QFont font);
        void setMainWindow(MainWindow* window);
        int fontSize();
        void setTabWidth(int width);
        void removeIconUnsaved();
        void setAutoIdent(bool flag);
        void showLineNumbers(bool flag);
        void highlihtCurrentLine(bool flag);
        void highlihtMatchingBracket(bool flag);
        QString getSearchWord();
        bool changesUnsaved();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void contextMenuEvent(QContextMenuEvent* event) override;
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
        LightpadSyntaxHighlighter* syntaxHighlighter;
        QString searchWord;
        bool areChangesUnsaved;
        bool autoIndent;
        bool showLineNumberArea;
        bool lineHighlighted;
        bool matchingBracketsHighlighted;
        int prevWordCount;
        void setupTextArea();
        void setTabWidgetIcon(QIcon icon);
        void closeParentheses(QString startSr, QString closeStr);
        void handleKeyEnterPressed();
        void drawCurrentLineHighlight();
        void clearLineHighlight();
        void updateRowColDisplay();
        void drawMatchingBrackets();
        void updateCursorPositionChangedCallbacks();
};

#endif
