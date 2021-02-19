#include "textarea.h"
#include "mainwindow.h"
#include "lightpadpage.h"
#include "lightpadtabwidget.h"
#include "lightpadsyntaxhighlighter.h"

#include <QPainter>
#include <QTextBlock>
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextCursor>
#include <QApplication>
#include <QStackedWidget>

enum lang {cpp, js, py};
QMap<QString, lang> convertStrToEnum = {{"cpp", cpp}, {"h", cpp}, {"js", js}, {"py", py}};
QMap<QChar, QChar> brackets = {{'{', '}'}, {'(', ')'}, {'[', ']'}};

static int findClosingParentheses(const QString& text, int pos, QChar startStr, QChar endStr) {

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

static int findOpeningParentheses(const QString& text, int pos, QChar startStr, QChar endStr) {

    int counter = 1;
    pos--;

    while (counter > 0  && pos > 0) {
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


static int leadingSpaces(const QString& str, int tabWidth) {

    int n = 0;

    for (int i = 0; i < str.size(); i++) {

        //check if character tabulation
        if (str[i] == '\x9')
            n += (tabWidth - 1);

        else if (!str[i].isSpace())
            return i + n;
    }

    return str.size();
}

static bool isLastNonSpaceCharacterOpenBrace(const QString& str) {

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

class LineNumberArea : public QWidget {
    public:
        LineNumberArea(TextArea* editor) : QWidget(editor), textArea(editor)
        {}

        QSize sizeHint() const override {
            return QSize(textArea->lineNumberAreaWidth(), 0);
        }

    protected:
        void paintEvent(QPaintEvent* event) override {
            textArea->lineNumberAreaPaintEvent(event);
        }

    private:
        TextArea* textArea;
};

TextArea::TextArea(QWidget* parent) :
    QPlainTextEdit(parent),
    mainWindow(nullptr),
    highlightColor(QColor(Qt::green).darker(250)),
    lineNumberAreaPenColor(QColor(Qt::gray).lighter(150)),
    defaultPenColor(QColor(Qt::white)),
    backgroundColor(QColor(Qt::gray).darker(200)),
    bufferText(""),
    highlightLang(""),
    syntaxHighlighter(nullptr),
    searchWord(""),
    areChangesUnsaved(false),
    autoIndent(true),
    showLineNumberArea(true),
    lineHighlighted(true),
    matchingBracketsHighlighted(true),
    prevWordCount(1)
     {

        lineNumberArea = new LineNumberArea(this);

        connect(this, &TextArea::blockCountChanged, this, [&] {
            setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
        });

        connect(this, &TextArea::updateRequest, this, [&] (const QRect &rect, int dy) {
            if (dy)
                lineNumberArea->scroll(0, dy);
            else
                lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

            if (rect.contains(viewport()->rect()))
                setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
        });

        connect(document(), &QTextDocument::undoCommandAdded, this, [&] {
            if (!areChangesUnsaved) {
                setTabWidgetIcon(QIcon(":/resources/icons/unsaved.png"));
                areChangesUnsaved = true;
            }
        });

        updateCursorPositionChangedCallbacks();
        clearLineHighlight();

        mainFont = QApplication::font();
        document()->setDefaultFont(mainFont);

        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(highlightColor);
        show();
}

int TextArea::lineNumberAreaWidth() {
    if (showLineNumberArea) {
        QFontMetrics fm(mainFont);
        return 3 + fm.horizontalAdvance(QLatin1Char('9'))*  1.8*  numberOfDigits(blockCount());
    }

    return 0;
}

void TextArea::increaseFontSize()
{
    setFontSize(mainFont.pointSize() + 1);
}

void TextArea::decreaseFontSize()
{
    setFontSize(mainFont.pointSize() - 1);
}

void TextArea::setFontSize(int size) {
    auto doc = document();

    if (doc) {
        mainFont.setPointSize(size);
        doc->setDefaultFont(mainFont);
    }
}

void TextArea::setFont(QFont font)
{
    mainFont = font;
    auto doc = document();

    if (doc)
      doc->setDefaultFont(font);
}

void TextArea::setMainWindow(MainWindow* window)
{
    mainWindow = window;
}

int TextArea::fontSize()
{
    return mainFont.pointSize();
}

void TextArea::setTabWidth(int width)
{
    QFontMetrics metrics(mainFont);
    setTabStopWidth(metrics.horizontalAdvance(' ') * width);
}

void TextArea::removeIconUnsaved()
{
    setTabWidgetIcon(QIcon());
    areChangesUnsaved = false;
}

void TextArea::setAutoIdent(bool flag)
{
    autoIndent = flag;
}

void TextArea::showLineNumbers(bool flag)
{
    showLineNumberArea = flag;
    resizeEvent(new QResizeEvent(size(), size()));
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void TextArea::highlihtCurrentLine(bool flag)
{
   lineHighlighted = flag;
   updateCursorPositionChangedCallbacks();
}

void TextArea::highlihtMatchingBracket(bool flag)
{
    matchingBracketsHighlighted = flag;
    updateCursorPositionChangedCallbacks();
}

QString TextArea::getSearchWord()
{
    return searchWord;
}

bool TextArea::changesUnsaved()
{
    return areChangesUnsaved;
}

void TextArea::resizeEvent(QResizeEvent* e) {
    QPlainTextEdit::resizeEvent(e);
    lineNumberArea->setGeometry(0, 0, lineNumberAreaWidth(), height());
}

void TextArea::keyPressEvent(QKeyEvent* keyEvent) {

    if (keyEvent->matches(QKeySequence::ZoomOut) || keyEvent->matches(QKeySequence::ZoomIn)) {
        mainWindow->keyPressEvent(keyEvent);
        return;
     }

    if (keyEvent->key() == Qt::Key_BraceLeft) {
        closeParentheses("{", "}");
        return;
    }

    else if (keyEvent->key() == Qt::Key_ParenLeft) {
        closeParentheses("(", ")");
        return;
    }

    else if (keyEvent->key() == Qt::Key_BracketLeft) {
        closeParentheses("[","]");
        return;
    }

    else if (keyEvent->key() == Qt::Key_QuoteDbl) {
        closeParentheses("\"", "\"");
        return;
    }

    else if (keyEvent->key() == Qt::Key_Apostrophe) {
        closeParentheses("\'", "\'");
        return;
    }

    QPlainTextEdit::keyPressEvent(keyEvent);

    if (keyEvent->key()==Qt::Key_Enter || keyEvent->key()==Qt::Key_Return)
        handleKeyEnterPressed();
}

void TextArea::setTabWidgetIcon(QIcon icon)
{
    auto page = qobject_cast<LightpadPage*>(parentWidget());

    if (page) {

        auto stackedWidget = qobject_cast<QStackedWidget*>(parentWidget()->parentWidget());

        if (stackedWidget) {

            auto tabWidget =  qobject_cast<LightpadTabWidget*>(parentWidget()->parentWidget()->parentWidget());

            if (tabWidget) {
                auto index = tabWidget->indexOf(page);

                if (index != -1)
                    tabWidget->setTabIcon(index, icon);
            }
        }
    }
}

void TextArea::closeParentheses(QString startStr, QString endStr)
{
    auto cursor = textCursor();

    if (cursor.hasSelection()) {
        auto start = cursor.selectionStart();
        auto end = cursor.selectionEnd();
        cursor.setPosition(start, cursor.MoveAnchor);
        cursor.insertText(startStr);
        cursor.setPosition(end + startStr.size(), cursor.MoveAnchor);
        cursor.insertText(endStr);
    }

    else if (startStr == "{") {
        auto pos = cursor.position();
        cursor.setPosition(pos, cursor.MoveAnchor);
        cursor.insertText("{\n\t\n}");
        cursor.setPosition(pos + 3);
    }

    else {
        auto pos = cursor.position();
        cursor.setPosition(pos, cursor.MoveAnchor);
        cursor.insertText(startStr + endStr);
    }

    setTextCursor(cursor);
}

void TextArea::handleKeyEnterPressed()
{
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

void TextArea::drawCurrentLineHighlight()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    QTextEdit::ExtraSelection selection;

    QColor color = mainWindow ? mainWindow->getTheme().highlightColor : highlightColor;
    selection.format.setBackground(color);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);

    setExtraSelections(extraSelections);
}

void TextArea::clearLineHighlight()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    setExtraSelections(extraSelections);
}

void TextArea::updateRowColDisplay()
{
    if (mainWindow)
        mainWindow->setRowCol(textCursor().blockNumber(), textCursor().positionInBlock());
}

void TextArea::drawMatchingBrackets()
{
    auto _drawMatchingBrackets  = [&](QTextCursor::MoveOperation op, const QChar& startStr,
            const QChar& endStr, std::function<int(const QString&, int, QChar, QChar)> function){
        
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
        auto pos = function(plainText, textCursor().position(), startStr, endStr);

        if (pos != -1) {
            selection.cursor.setPosition(pos);
            selection.cursor.movePosition(op, QTextCursor::KeepAnchor);
            extraSelections.append(selection);
            setExtraSelections(extraSelections);
        }
    };

    auto cursor = textCursor();
    auto result = cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    auto startStr =  result ? cursor.selectedText().front() : QChar(' ');

    cursor = textCursor();
    result = cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    auto endStr = result ? cursor.selectedText().front() : QChar(' ');

    if (brackets.contains(startStr))
        _drawMatchingBrackets(QTextCursor::NextCharacter, startStr, brackets[startStr], &findClosingParentheses);

    else if (brackets.values().contains(endStr))
        _drawMatchingBrackets(QTextCursor::PreviousCharacter, brackets.key(endStr), endStr, &findOpeningParentheses);
}

void TextArea::updateCursorPositionChangedCallbacks()
{

    disconnect(this, &TextArea::cursorPositionChanged, 0, 0);

    if (lineHighlighted && matchingBracketsHighlighted) {
        connect(this, &TextArea::cursorPositionChanged, this, [&]() {
            drawCurrentLineHighlight();
            drawMatchingBrackets();
            updateRowColDisplay();
        });
    }

    else if (lineHighlighted && !matchingBracketsHighlighted) {
        clearLineHighlight();
        connect(this, &TextArea::cursorPositionChanged, this, [&]() {
            drawCurrentLineHighlight();
            updateRowColDisplay();
        });
    }

    else if (!lineHighlighted && matchingBracketsHighlighted) {
        clearLineHighlight();
        connect(this, &TextArea::cursorPositionChanged, this, [&]() {
            drawMatchingBrackets();
            updateRowColDisplay();
        });
    }

    else {
        clearLineHighlight();
        connect(this, &TextArea::cursorPositionChanged, this, [&]() {
            updateRowColDisplay();
        });
    }

    emit cursorPositionChanged();
}

void TextArea::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(lineNumberArea);
    painter.setFont(mainFont);

    auto color = mainWindow ? mainWindow->getTheme().lineNumberAreaColor : backgroundColor;

    painter.fillRect(event->rect(), color);

    auto block = firstVisibleBlock();
    auto blockNumber = block.blockNumber();
    auto height = QFontMetrics(mainFont).height();
    auto top = blockBoundingGeometry(block).translated(contentOffset()).top();
    auto bottom = height + top;
    color = mainWindow ? mainWindow->getTheme().foregroundColor : lineNumberAreaPenColor;

    while (block.isValid() && top <= event->rect().bottom()) {

        if (block.isVisible() && bottom >= event->rect().top()) {
            auto number = QString::number(blockNumber);
            painter.setPen(color);
            painter.drawText(0, top, lineNumberArea->width(), height, Qt::AlignCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + height;
        ++blockNumber;
    }
}

void TextArea::updateSyntaxHighlightTags(QString searchKey, QString chosenLang) {

    searchWord = searchKey;

    auto colors = mainWindow->getTheme();

    if (!chosenLang.isEmpty())
        highlightLang = chosenLang;

    if (syntaxHighlighter) {
        delete syntaxHighlighter;
        syntaxHighlighter = nullptr;
    }

    if (document() && convertStrToEnum.contains(highlightLang)) {

        switch(convertStrToEnum[highlightLang]) {
            case cpp:
                syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesCpp(colors, searchKey), QRegularExpression(QStringLiteral("/\\*")),  QRegularExpression(QStringLiteral("\\*/")), document());
                break;

            case js:
                syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesJs(colors, searchKey), QRegularExpression(QStringLiteral("/\\*")),  QRegularExpression(QStringLiteral("\\*/")), document());
                break;

            case py:
                syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesPy(colors, searchKey), QRegularExpression(QStringLiteral("/'''")),  QRegularExpression(QStringLiteral("\\'''")), document());
                break;
         }
    }
}
