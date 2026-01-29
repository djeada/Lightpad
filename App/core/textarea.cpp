#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QCompleter>
#include <QAbstractItemView>
#include <QScrollBar>

#include "lightpadpage.h"
#include "logging/logger.h"
#include "../syntax/lightpadsyntaxhighlighter.h"
#include "lightpadtabwidget.h"
#include "../ui/mainwindow.h"
#include "textarea.h"
#include "../settings/textareasettings.h"

QMap<QString, Lang> convertStrToEnum = { { "cpp", Lang::cpp }, { "h", Lang::cpp }, { "js", Lang::js }, { "py", Lang::py } };
QMap<QChar, QChar> brackets = { { '{', '}' }, { '(', ')' }, { '[', ']' } };

static int findClosingParentheses(const QString& text, int pos, QChar startStr, QChar endStr)
{

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

static int findOpeningParentheses(const QString& text, int pos, QChar startStr, QChar endStr)
{

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

static int leadingSpaces(const QString& str, int tabWidth)
{

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

static bool isLastNonSpaceCharacterOpenBrace(const QString& str)
{

    for (int i = str.size() - 1; i >= 0; i--) {
        if (!str[i].isSpace() && str[i] == '{')
            return true;
    }

    return false;
}

static int numberOfDigits(int x)
{

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
    LineNumberArea(TextArea* editor)
        : QWidget(editor)
        , textArea(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(textArea->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        textArea->lineNumberAreaPaintEvent(event);
    }

private:
    TextArea* textArea;
};

TextArea::TextArea(QWidget* parent)
    : QPlainTextEdit(parent)
    , mainWindow(nullptr)
    , highlightColor(QColor(Qt::green).darker(250))
    , lineNumberAreaPenColor(QColor(Qt::gray).lighter(150))
    , defaultPenColor(QColor(Qt::white))
    , backgroundColor(QColor(Qt::gray).darker(200))
    , bufferText("")
    , highlightLang("")
    , syntaxHighlighter(nullptr)
    , m_completer(nullptr)
    , searchWord("")
    , areChangesUnsaved(false)
    , autoIndent(true)
    , showLineNumberArea(true)
    , lineHighlighted(true)
    , matchingBracketsHighlighted(true)
    , prevWordCount(1)
{
    setupTextArea();
    mainFont = QApplication::font();
    document()->setDefaultFont(mainFont);
    show();
}

TextArea::TextArea(const TextAreaSettings& settings, QWidget* parent)
    : QPlainTextEdit(parent)
    , mainWindow(nullptr)
    , highlightColor(settings.theme.highlightColor)
    , lineNumberAreaPenColor(settings.theme.lineNumberAreaColor)
    , defaultPenColor(settings.theme.foregroundColor)
    , backgroundColor(settings.theme.backgroundColor)
    , bufferText("")
    , highlightLang("")
    , syntaxHighlighter(nullptr)
    , m_completer(nullptr)
    , searchWord("")
    , areChangesUnsaved(false)
    , autoIndent(settings.autoIndent)
    , showLineNumberArea(settings.showLineNumberArea)
    , lineHighlighted(settings.lineHighlighted)
    , matchingBracketsHighlighted(settings.matchingBracketsHighlighted)
    , prevWordCount(1)
{
    setupTextArea();
    mainFont = settings.mainFont;
    document()->setDefaultFont(mainFont);
    show();
}

void TextArea::setupTextArea()
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &TextArea::blockCountChanged, this, [&] {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    });

    connect(this, &TextArea::updateRequest, this, [&](const QRect& rect, int dy) {
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

    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    updateCursorPositionChangedCallbacks();
    clearLineHighlight();
}

int TextArea::lineNumberAreaWidth()
{
    if (showLineNumberArea) {
        QFontMetrics fm(mainFont);
        return 3 + fm.horizontalAdvance(QLatin1Char('9')) * 1.8 * numberOfDigits(blockCount());
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

void TextArea::setFontSize(int size)
{
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
    setTabStopDistance(QFontMetricsF(mainFont).horizontalAdvance(' ') * width);
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
    LOG_DEBUG(QString("Show line numbers: %1").arg(flag ? "true" : "false"));
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

void TextArea::loadSettings(const TextAreaSettings settings)
{
    highlightColor = settings.theme.highlightColor;
    lineNumberAreaPenColor = settings.theme.lineNumberAreaColor;
    defaultPenColor = settings.theme.foregroundColor;
    backgroundColor = settings.theme.backgroundColor;
    setAutoIdent(settings.autoIndent);
    showLineNumbers(settings.showLineNumberArea);
    highlihtCurrentLine(settings.lineHighlighted);
    highlihtMatchingBracket(settings.matchingBracketsHighlighted);
}

QString TextArea::getSearchWord()
{
    return searchWord;
}

bool TextArea::changesUnsaved()
{
    return areChangesUnsaved;
}

void TextArea::resizeEvent(QResizeEvent* e)
{
    QPlainTextEdit::resizeEvent(e);
    lineNumberArea->setGeometry(0, 0, lineNumberAreaWidth(), height());
}

void TextArea::keyPressEvent(QKeyEvent* keyEvent)
{

    if (keyEvent->matches(QKeySequence::ZoomOut) || keyEvent->matches(QKeySequence::ZoomIn)) {
        mainWindow->keyPressEvent(keyEvent);
        return;
    }

    // Handle completer popup navigation
    if (m_completer && m_completer->popup()->isVisible()) {
        // The following keys are forwarded by the completer to the widget
        switch (keyEvent->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            keyEvent->ignore();
            return; // let the completer do default behavior
        default:
            break;
        }
    }

    // Handle auto-parentheses before processing other keys
    if (keyEvent->key() == Qt::Key_BraceLeft) {
        closeParentheses("{", "}");
        return;
    }

    else if (keyEvent->key() == Qt::Key_ParenLeft) {
        closeParentheses("(", ")");
        return;
    }

    else if (keyEvent->key() == Qt::Key_BracketLeft) {
        closeParentheses("[", "]");
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

    // Check for completion shortcut (Ctrl+Space)
    bool isShortcut = ((keyEvent->modifiers() & Qt::ControlModifier) && keyEvent->key() == Qt::Key_Space);
    
    if (!isShortcut) {
        // Normal key processing
        QPlainTextEdit::keyPressEvent(keyEvent);

        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)
            handleKeyEnterPressed();
    }

    // Handle autocompletion (both manual trigger and auto-popup)
    if (!m_completer)
        return;

    const bool ctrlOrShift = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    
    static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="); // end of word
    bool hasModifier = (keyEvent->modifiers() != Qt::NoModifier) && !ctrlOrShift;
    QString completionPrefix = textUnderCursor();

    if (!isShortcut && (hasModifier || keyEvent->text().isEmpty() || completionPrefix.length() < 3
                          || eow.contains(keyEvent->text().right(1)))) {
        m_completer->popup()->hide();
        return;
    }

    if (completionPrefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(completionPrefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }
    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(cr); // popup it up!
}

void TextArea::contextMenuEvent(QContextMenuEvent* event)
{
    auto menu = createStandardContextMenu();
    menu->addSeparator();
    menu->addAction(tr("Refactor"));
    menu->exec(event->globalPos());
    delete menu;
}

void TextArea::setTabWidgetIcon(QIcon icon)
{
    auto page = qobject_cast<LightpadPage*>(parentWidget());

    if (page) {

        auto stackedWidget = qobject_cast<QStackedWidget*>(parentWidget()->parentWidget());

        if (stackedWidget) {

            auto tabWidget = qobject_cast<LightpadTabWidget*>(parentWidget()->parentWidget()->parentWidget());

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
    auto _drawMatchingBrackets = [&](QTextCursor::MoveOperation op, const QChar& startStr,
                                     const QChar& endStr, std::function<int(const QString&, int, QChar, QChar)> function) {
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
    auto startStr = result ? cursor.selectedText().front() : QChar(' ');

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

void TextArea::lineNumberAreaPaintEvent(QPaintEvent* event)
{
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

void TextArea::updateSyntaxHighlightTags(QString searchKey, QString chosenLang)
{

    searchWord = searchKey;

    auto colors = mainWindow->getTheme();

    if (!chosenLang.isEmpty())
        highlightLang = chosenLang;

    if (syntaxHighlighter) {
        delete syntaxHighlighter;
        syntaxHighlighter = nullptr;
    }

    if (document() && convertStrToEnum.contains(highlightLang)) {

        switch (convertStrToEnum[highlightLang]) {
        case Lang::cpp:
            syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesCpp(colors, searchKey), QRegularExpression(QStringLiteral("/\\*")), QRegularExpression(QStringLiteral("\\*/")), document());
            break;

        case Lang::js:
            syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesJs(colors, searchKey), QRegularExpression(QStringLiteral("/\\*")), QRegularExpression(QStringLiteral("\\*/")), document());
            break;

        case Lang::py:
            syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesPy(colors, searchKey), QRegularExpression(QStringLiteral("/'''")), QRegularExpression(QStringLiteral("\\'''")), document());
            break;
        }
    }
}

void TextArea::setCompleter(QCompleter* completer)
{
    if (m_completer)
        m_completer->disconnect(this);

    m_completer = completer;

    if (!m_completer)
        return;

    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    QObject::connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated),
                     this, &TextArea::insertCompletion);
}

QCompleter* TextArea::completer() const
{
    return m_completer;
}

void TextArea::insertCompletion(const QString& completion)
{
    if (m_completer->widget() != this)
        return;
    QTextCursor tc = textCursor();
    int extra = completion.length() - m_completer->completionPrefix().length();
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra));
    setTextCursor(tc);
}

QString TextArea::textUnderCursor() const
{
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}
