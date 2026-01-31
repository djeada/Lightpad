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
#include <QMouseEvent>
#include <functional>
#include <algorithm>

#include "lightpadpage.h"
#include "logging/logger.h"
#include "../syntax/lightpadsyntaxhighlighter.h"
#include "../syntax/pluginbasedsyntaxhighlighter.h"
#include "../syntax/syntaxpluginregistry.h"
#include "lightpadtabwidget.h"
#include "../ui/mainwindow.h"
#include "textarea.h"
#include "../settings/textareasettings.h"
#include "../completion/completionengine.h"
#include "../completion/completionwidget.h"
#include "../completion/completionitem.h"
#include "../completion/completioncontext.h"

QMap<QString, Lang> convertStrToEnum = { { "cpp", Lang::cpp }, { "h", Lang::cpp }, { "js", Lang::js }, { "py", Lang::py } };
QMap<QChar, QChar> brackets = { { '{', '}' }, { '(', ')' }, { '[', ']' } };
constexpr int defaultLineSpacingPercent = 200;

// Static icon cache - initialized once, reused everywhere
QIcon TextArea::s_unsavedIcon;
bool TextArea::s_iconsInitialized = false;

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

void TextArea::initializeIconCache()
{
    if (!s_iconsInitialized) {
        s_unsavedIcon = QIcon(":/resources/icons/unsaved.png");
        s_iconsInitialized = true;
    }
}

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
    , m_completionEngine(nullptr)
    , m_completionWidget(nullptr)
    , m_languageId("")
    , searchWord("")
    , areChangesUnsaved(false)
    , autoIndent(true)
    , showLineNumberArea(true)
    , lineHighlighted(true)
    , matchingBracketsHighlighted(true)
    , prevWordCount(1)
{
    initializeIconCache();
    setupTextArea();
    mainFont = QApplication::font();
    document()->setDefaultFont(mainFont);
    applyLineSpacing(defaultLineSpacingPercent);
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
    , m_completionEngine(nullptr)
    , m_completionWidget(nullptr)
    , m_languageId("")
    , searchWord("")
    , areChangesUnsaved(false)
    , autoIndent(settings.autoIndent)
    , showLineNumberArea(settings.showLineNumberArea)
    , lineHighlighted(settings.lineHighlighted)
    , matchingBracketsHighlighted(settings.matchingBracketsHighlighted)
    , prevWordCount(1)
{
    initializeIconCache();
    setupTextArea();
    mainFont = settings.mainFont;
    document()->setDefaultFont(mainFont);
    applyLineSpacing(defaultLineSpacingPercent);
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
            // Use cached icon to avoid disk I/O on every keystroke
            setTabWidgetIcon(s_unsavedIcon);
            areChangesUnsaved = true;
        }
    });
    
    // Update highlighter viewport on scroll for performance optimization
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        updateHighlighterViewport();
    });

    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    updateCursorPositionChangedCallbacks();
    clearLineHighlight();
}

void TextArea::applyLineSpacing(int percent)
{
    if (percent <= 0)
        return;

    auto* doc = document();
    if (!doc)
        return;

    QTextCursor previousCursor = textCursor();
    const bool undoEnabled = doc->isUndoRedoEnabled();
    doc->setUndoRedoEnabled(false);

    const int baseHeight = QFontMetrics(mainFont).lineSpacing();
    const int extraHeight = qMax(0, (baseHeight * (percent - 100)) / 100);

    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        QTextCursor blockCursor(block);
        QTextBlockFormat format = block.blockFormat();
        if (extraHeight > 0) {
            format.setLineHeight(extraHeight, QTextBlockFormat::LineDistanceHeight);
        } else {
            format.setLineHeight(100, QTextBlockFormat::ProportionalHeight);
        }
        blockCursor.setBlockFormat(format);
    }
    cursor.endEditBlock();

    doc->setUndoRedoEnabled(undoEnabled);
    setTextCursor(previousCursor);
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
        applyLineSpacing(defaultLineSpacingPercent);
    }
}

void TextArea::setFont(QFont font)
{
    mainFont = font;
    auto doc = document();

    if (doc)
        doc->setDefaultFont(font);
    applyLineSpacing(defaultLineSpacingPercent);
}

void TextArea::setPlainText(const QString& text)
{
    QPlainTextEdit::setPlainText(text);
    applyLineSpacing(defaultLineSpacingPercent);
}

void TextArea::setMainWindow(MainWindow* window)
{
    mainWindow = window;
    if (m_completionWidget && mainWindow) {
        m_completionWidget->applyTheme(mainWindow->getTheme());
    }
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
    if (m_completionWidget) {
        m_completionWidget->applyTheme(settings.theme);
    }
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

    // Multi-cursor shortcuts
    if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
        if (keyEvent->key() == Qt::Key_Up) {
            addCursorAbove();
            return;
        } else if (keyEvent->key() == Qt::Key_Down) {
            addCursorBelow();
            return;
        }
    }
    
    // Ctrl+D - Add cursor at next occurrence
    if (keyEvent->modifiers() == Qt::ControlModifier && keyEvent->key() == Qt::Key_D) {
        addCursorAtNextOccurrence();
        return;
    }
    
    // Ctrl+Shift+L - Add cursors to all occurrences
    if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && keyEvent->key() == Qt::Key_L) {
        addCursorsToAllOccurrences();
        return;
    }
    
    // Escape clears extra cursors
    if (keyEvent->key() == Qt::Key_Escape && hasMultipleCursors()) {
        clearExtraCursors();
        return;
    }
    
    // Code folding shortcuts
    if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (keyEvent->key() == Qt::Key_BracketLeft) {
            foldCurrentBlock();
            return;
        } else if (keyEvent->key() == Qt::Key_BracketRight) {
            unfoldCurrentBlock();
            return;
        }
    }

    // Handle new completion widget navigation
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

    // Handle legacy completer popup navigation (deprecated)
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

    // Handle multi-cursor typing
    if (hasMultipleCursors() && !keyEvent->text().isEmpty() && 
        keyEvent->modifiers() == Qt::NoModifier) {
        
        // Apply typed character to all cursors
        QString text = keyEvent->text();
        applyToAllCursors([&text](QTextCursor& cursor) {
            cursor.insertText(text);
        });
        return;
    }
    
    // Handle multi-cursor backspace
    if (hasMultipleCursors() && keyEvent->key() == Qt::Key_Backspace) {
        applyToAllCursors([](QTextCursor& cursor) {
            if (!cursor.hasSelection()) {
                cursor.deletePreviousChar();
            } else {
                cursor.removeSelectedText();
            }
        });
        return;
    }
    
    // Handle multi-cursor delete
    if (hasMultipleCursors() && keyEvent->key() == Qt::Key_Delete) {
        applyToAllCursors([](QTextCursor& cursor) {
            if (!cursor.hasSelection()) {
                cursor.deleteChar();
            } else {
                cursor.removeSelectedText();
            }
        });
        return;
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

    // Handle new completion engine (preferred)
    if (m_completionEngine) {
        static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="); // end of word
        const bool ctrlOrShift = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
        bool hasModifier = (keyEvent->modifiers() != Qt::NoModifier) && !ctrlOrShift;
        QString completionPrefix = textUnderCursor();

        if (!isShortcut && (hasModifier || keyEvent->text().isEmpty() || completionPrefix.length() < 2
                              || eow.contains(keyEvent->text().right(1)))) {
            hideCompletionPopup();
            return;
        }

        // Trigger completion
        QTextCursor cursor = textCursor();
        CompletionContext ctx;
        ctx.documentUri = getDocumentUri();
        ctx.languageId = m_languageId;
        ctx.prefix = completionPrefix;
        ctx.line = cursor.blockNumber();
        ctx.column = cursor.positionInBlock();
        ctx.lineText = cursor.block().text();
        ctx.triggerKind = isShortcut ? CompletionTriggerKind::Invoked : CompletionTriggerKind::TriggerCharacter;
        ctx.isAutoComplete = !isShortcut;
        
        m_completionEngine->requestCompletions(ctx);
        return;
    }

    // Handle legacy autocompletion (deprecated - fallback)
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
    auto top = blockBoundingGeometry(block).translated(contentOffset()).top();
    auto bottom = top + blockBoundingRect(block).height();
    color = mainWindow ? mainWindow->getTheme().foregroundColor : lineNumberAreaPenColor;

    while (block.isValid() && top <= event->rect().bottom()) {

        if (block.isVisible() && bottom >= event->rect().top()) {
            const qreal height = blockBoundingRect(block).height();
            auto number = QString::number(blockNumber);
            painter.setPen(color);
            painter.drawText(0, top, lineNumberArea->width(), height, Qt::AlignCenter, number);
        }

        block = block.next();
        top = bottom;
        if (!block.isValid())
            break;
        bottom = top + blockBoundingRect(block).height();
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

    // NEW PLUGIN-BASED SYSTEM
    // Try to get plugin from registry first
    auto& registry = SyntaxPluginRegistry::instance();
    ISyntaxPlugin* plugin = registry.getPluginByLanguageId(highlightLang);
    
    if (plugin && document()) {
        // Use new plugin-based highlighter
        auto* pluginHighlighter = new PluginBasedSyntaxHighlighter(plugin, colors, searchKey, document());
        syntaxHighlighter = pluginHighlighter;
        return;
    }

    // FALLBACK TO OLD SYSTEM (for backward compatibility)
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
    
    // Update highlighter viewport after setting up
    updateHighlighterViewport();
}

void TextArea::updateHighlighterViewport()
{
    if (!syntaxHighlighter) {
        return;
    }
    
    // Calculate visible block range
    int firstVisible = firstVisibleBlock().blockNumber();
    int visibleLines = viewport()->height() / fontMetrics().height();
    int lastVisible = firstVisible + visibleLines + 1;
    
    // Update the highlighter's viewport knowledge
    // This allows it to skip highlighting off-screen blocks
    if (auto* legacyHighlighter = qobject_cast<LightpadSyntaxHighlighter*>(syntaxHighlighter)) {
        legacyHighlighter->setVisibleBlockRange(firstVisible, lastVisible);
    } else if (auto* pluginHighlighter = qobject_cast<PluginBasedSyntaxHighlighter*>(syntaxHighlighter)) {
        pluginHighlighter->setVisibleBlockRange(firstVisible, lastVisible);
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

// ============================================================================
// New Completion System
// ============================================================================

void TextArea::setCompletionEngine(CompletionEngine* engine)
{
    m_completionEngine = engine;
    
    if (!m_completionEngine)
        return;
    
    // Create completion widget if needed
    if (!m_completionWidget) {
        m_completionWidget = new CompletionWidget(this);
        if (mainWindow) {
            m_completionWidget->applyTheme(mainWindow->getTheme());
        }
        connect(m_completionWidget, &CompletionWidget::itemAccepted,
                this, &TextArea::onCompletionAccepted);
        connect(m_completionWidget, &CompletionWidget::cancelled,
                this, &TextArea::hideCompletionPopup);
    }
    
    // Connect engine signals
    connect(m_completionEngine, &CompletionEngine::completionsReady,
            this, &TextArea::onCompletionsReady);
    
    m_completionEngine->setLanguage(m_languageId);
}

CompletionEngine* TextArea::completionEngine() const
{
    return m_completionEngine;
}

void TextArea::setLanguage(const QString& languageId)
{
    m_languageId = languageId;
    if (m_completionEngine) {
        m_completionEngine->setLanguage(languageId);
    }
}

QString TextArea::language() const
{
    return m_languageId;
}

QString TextArea::getDocumentUri() const
{
    return QString("file://%1").arg(objectName());
}

void TextArea::triggerCompletion()
{
    if (!m_completionEngine)
        return;
    
    QString prefix = textUnderCursor();
    QTextCursor cursor = textCursor();
    
    CompletionContext ctx;
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

void TextArea::onCompletionsReady(const QList<CompletionItem>& items)
{
    if (items.isEmpty()) {
        hideCompletionPopup();
        return;
    }
    
    showCompletionPopup();
    m_completionWidget->setItems(items);
}

void TextArea::onCompletionAccepted(const CompletionItem& item)
{
    insertCompletionItem(item);
    hideCompletionPopup();
}

void TextArea::insertCompletionItem(const CompletionItem& item)
{
    QTextCursor tc = textCursor();
    QString prefix = textUnderCursor();
    
    // Move to end of current word and delete the prefix
    tc.movePosition(QTextCursor::EndOfWord);
    tc.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
    
    QString insertText = item.effectiveInsertText();
    
    if (item.isSnippet) {
        // Simple snippet expansion - replace placeholders with default text or empty
        // Full tabstop navigation would be more complex
        // Handle ${1:default} -> default, ${1} -> ""
        static const QRegularExpression tabstopWithDefaultRe(R"(\$\{(\d+):([^}]*)\})");
        insertText.replace(tabstopWithDefaultRe, "\\2");
        static const QRegularExpression tabstopNoDefaultRe(R"(\$\{(\d+)\})");
        insertText.replace(tabstopNoDefaultRe, "");
        static const QRegularExpression simpleTabstopRe(R"(\$(\d+))");
        insertText.replace(simpleTabstopRe, "");
    }
    
    tc.insertText(insertText);
    setTextCursor(tc);
}

void TextArea::showCompletionPopup()
{
    if (!m_completionWidget)
        return;
    
    QRect cr = cursorRect();
    QPoint pos = mapToGlobal(cr.bottomLeft());
    m_completionWidget->showAt(pos);
}

void TextArea::hideCompletionPopup()
{
    if (m_completionWidget) {
        m_completionWidget->hide();
    }
}

// ============================================================================
// Multi-Cursor Support
// ============================================================================

void TextArea::addCursorAbove()
{
    QTextCursor cursor = textCursor();
    int col = cursor.positionInBlock();
    
    if (cursor.blockNumber() > 0) {
        QTextCursor newCursor = cursor;
        newCursor.movePosition(QTextCursor::PreviousBlock);
        
        // Try to maintain column position
        int lineLength = newCursor.block().text().length();
        int targetCol = qMin(col, lineLength);
        newCursor.movePosition(QTextCursor::StartOfBlock);
        newCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, targetCol);
        
        m_extraCursors.append(cursor);
        setTextCursor(newCursor);
        drawExtraCursors();
    }
}

void TextArea::addCursorBelow()
{
    QTextCursor cursor = textCursor();
    int col = cursor.positionInBlock();
    
    if (cursor.blockNumber() < document()->blockCount() - 1) {
        QTextCursor newCursor = cursor;
        newCursor.movePosition(QTextCursor::NextBlock);
        
        // Try to maintain column position
        int lineLength = newCursor.block().text().length();
        int targetCol = qMin(col, lineLength);
        newCursor.movePosition(QTextCursor::StartOfBlock);
        newCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, targetCol);
        
        m_extraCursors.append(cursor);
        setTextCursor(newCursor);
        drawExtraCursors();
    }
}

void TextArea::addCursorAtNextOccurrence()
{
    QTextCursor cursor = textCursor();
    QString word;
    
    if (cursor.hasSelection()) {
        word = cursor.selectedText();
    } else {
        cursor.select(QTextCursor::WordUnderCursor);
        word = cursor.selectedText();
        setTextCursor(cursor);
    }
    
    if (word.isEmpty())
        return;
    
    m_lastSelectedWord = word;
    
    // Find next occurrence after current cursor
    QTextCursor searchCursor = cursor;
    searchCursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    int endPos = searchCursor.position();
    
    // Start search from after current selection
    int startSearchPos = cursor.selectionEnd();
    
    // Search forward from cursor
    QTextDocument* doc = document();
    QTextCursor found = doc->find(word, startSearchPos);
    
    // Wrap around if not found
    if (found.isNull()) {
        found = doc->find(word, 0);
    }
    
    if (!found.isNull() && found.selectionStart() != cursor.selectionStart()) {
        m_extraCursors.append(cursor);
        setTextCursor(found);
        drawExtraCursors();
    }
}

void TextArea::addCursorsToAllOccurrences()
{
    QTextCursor cursor = textCursor();
    QString word;
    
    if (cursor.hasSelection()) {
        word = cursor.selectedText();
    } else {
        cursor.select(QTextCursor::WordUnderCursor);
        word = cursor.selectedText();
    }
    
    if (word.isEmpty())
        return;
    
    m_lastSelectedWord = word;
    m_extraCursors.clear();
    
    // Find all occurrences
    QTextDocument* doc = document();
    QTextCursor searchCursor(doc);
    QTextCursor firstCursor;
    bool first = true;
    
    while (true) {
        QTextCursor found = doc->find(word, searchCursor);
        if (found.isNull())
            break;
        
        if (first) {
            firstCursor = found;
            first = false;
        } else {
            m_extraCursors.append(found);
        }
        
        searchCursor = found;
        searchCursor.movePosition(QTextCursor::Right);
    }
    
    if (!first) {
        setTextCursor(firstCursor);
        drawExtraCursors();
    }
}

void TextArea::clearExtraCursors()
{
    m_extraCursors.clear();
    m_lastSelectedWord.clear();
    viewport()->update();
}

bool TextArea::hasMultipleCursors() const
{
    return !m_extraCursors.isEmpty();
}

int TextArea::cursorCount() const
{
    return m_extraCursors.size() + 1;
}

void TextArea::drawExtraCursors()
{
    QList<QTextEdit::ExtraSelection> selections = extraSelections();
    
    // Add extra cursor highlights
    for (const QTextCursor& cursor : m_extraCursors) {
        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        
        if (cursor.hasSelection()) {
            // Highlight selection
            selection.format.setBackground(QColor(38, 79, 120));
        } else {
            // Show cursor line
            selection.format.setBackground(highlightColor.lighter(110));
            selection.format.setProperty(QTextFormat::FullWidthSelection, false);
        }
        
        selections.append(selection);
    }
    
    setExtraSelections(selections);
    viewport()->update();
}

void TextArea::applyToAllCursors(const std::function<void(QTextCursor&)>& operation)
{
    // Apply to main cursor
    QTextCursor mainCursor = textCursor();
    operation(mainCursor);
    setTextCursor(mainCursor);
    
    // Apply to extra cursors
    for (QTextCursor& cursor : m_extraCursors) {
        operation(cursor);
    }
    
    mergeOverlappingCursors();
    drawExtraCursors();
}

void TextArea::mergeOverlappingCursors()
{
    if (m_extraCursors.isEmpty())
        return;
    
    QTextCursor mainCursor = textCursor();
    QList<QTextCursor> allCursors;
    allCursors.append(mainCursor);
    allCursors.append(m_extraCursors);
    
    // Sort by position
    std::sort(allCursors.begin(), allCursors.end(), [](const QTextCursor& a, const QTextCursor& b) {
        return a.position() < b.position();
    });
    
    // Remove duplicates
    QList<QTextCursor> unique;
    for (const QTextCursor& cursor : allCursors) {
        bool duplicate = false;
        for (const QTextCursor& existing : unique) {
            if (cursor.position() == existing.position()) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            unique.append(cursor);
        }
    }
    
    if (unique.size() > 0) {
        setTextCursor(unique.first());
        m_extraCursors = unique.mid(1);
    }
}

void TextArea::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);
    
    // Draw extra cursors as vertical lines
    if (!m_extraCursors.isEmpty()) {
        QPainter painter(viewport());
        painter.setPen(QPen(defaultPenColor, 2));
        
        for (const QTextCursor& cursor : m_extraCursors) {
            if (!cursor.hasSelection()) {
                QRect cursorRect = this->cursorRect(cursor);
                painter.drawLine(cursorRect.topLeft(), cursorRect.bottomLeft());
            }
        }
    }
}

void TextArea::mousePressEvent(QMouseEvent* event)
{
    // Clear extra cursors on regular click
    if (!m_extraCursors.isEmpty() && !(event->modifiers() & Qt::ControlModifier)) {
        clearExtraCursors();
    }
    
    // Ctrl+Click to add cursor
    if (event->modifiers() & Qt::ControlModifier && event->modifiers() & Qt::AltModifier) {
        QTextCursor cursor = cursorForPosition(event->pos());
        m_extraCursors.append(textCursor());
        setTextCursor(cursor);
        drawExtraCursors();
        return;
    }
    
    QPlainTextEdit::mousePressEvent(event);
}

// ============================================================================
// Code Folding Support
// ============================================================================

void TextArea::foldCurrentBlock()
{
    int blockNum = textCursor().blockNumber();
    if (isFoldable(blockNum) && !m_foldedBlocks.contains(blockNum)) {
        m_foldedBlocks.insert(blockNum);
        
        int endBlock = findFoldEndBlock(blockNum);
        QTextBlock block = document()->findBlockByNumber(blockNum + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(false);
            block = block.next();
        }
        
        viewport()->update();
        document()->markContentsDirty(0, document()->characterCount());
    }
}

void TextArea::unfoldCurrentBlock()
{
    int blockNum = textCursor().blockNumber();
    
    // Also check if we're inside a folded region
    for (int foldedBlock : m_foldedBlocks) {
        int endBlock = findFoldEndBlock(foldedBlock);
        if (blockNum >= foldedBlock && blockNum <= endBlock) {
            blockNum = foldedBlock;
            break;
        }
    }
    
    if (m_foldedBlocks.contains(blockNum)) {
        m_foldedBlocks.remove(blockNum);
        
        int endBlock = findFoldEndBlock(blockNum);
        QTextBlock block = document()->findBlockByNumber(blockNum + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(true);
            block = block.next();
        }
        
        viewport()->update();
        document()->markContentsDirty(0, document()->characterCount());
    }
}

void TextArea::foldAll()
{
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        if (isFoldable(block.blockNumber())) {
            int blockNum = block.blockNumber();
            if (!m_foldedBlocks.contains(blockNum)) {
                m_foldedBlocks.insert(blockNum);
                
                int endBlock = findFoldEndBlock(blockNum);
                QTextBlock innerBlock = block.next();
                
                while (innerBlock.isValid() && innerBlock.blockNumber() <= endBlock) {
                    innerBlock.setVisible(false);
                    innerBlock = innerBlock.next();
                }
            }
        }
        block = block.next();
    }
    
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::unfoldAll()
{
    m_foldedBlocks.clear();
    
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        block.setVisible(true);
        block = block.next();
    }
    
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::toggleFoldAtLine(int line)
{
    if (m_foldedBlocks.contains(line)) {
        m_foldedBlocks.remove(line);
        
        int endBlock = findFoldEndBlock(line);
        QTextBlock block = document()->findBlockByNumber(line + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(true);
            block = block.next();
        }
    } else if (isFoldable(line)) {
        m_foldedBlocks.insert(line);
        
        int endBlock = findFoldEndBlock(line);
        QTextBlock block = document()->findBlockByNumber(line + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(false);
            block = block.next();
        }
    }
    
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
}

int TextArea::findFoldEndBlock(int startBlock) const
{
    QTextBlock block = document()->findBlockByNumber(startBlock);
    if (!block.isValid())
        return startBlock;
    
    QString text = block.text();
    int startIndent = 0;
    for (QChar c : text) {
        if (c == ' ') startIndent++;
        else if (c == '\t') startIndent += 4;
        else break;
    }
    
    // Check for brace-based folding
    bool braceStyle = text.contains('{');
    int braceCount = 0;
    
    if (braceStyle) {
        for (QChar c : text) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
        }
    }
    
    block = block.next();
    int lastNonEmpty = startBlock;
    
    while (block.isValid()) {
        text = block.text().trimmed();
        
        if (braceStyle) {
            for (QChar c : block.text()) {
                if (c == '{') braceCount++;
                else if (c == '}') braceCount--;
            }
            
            if (braceCount <= 0) {
                return block.blockNumber();
            }
        } else {
            // Indent-based folding
            if (!text.isEmpty()) {
                int indent = 0;
                for (QChar c : block.text()) {
                    if (c == ' ') indent++;
                    else if (c == '\t') indent += 4;
                    else break;
                }
                
                if (indent <= startIndent) {
                    return lastNonEmpty;
                }
                lastNonEmpty = block.blockNumber();
            }
        }
        
        block = block.next();
    }
    
    return lastNonEmpty;
}

bool TextArea::isFoldable(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text();
    
    // Foldable if line ends with { or :
    QString trimmed = text.trimmed();
    if (trimmed.endsWith('{') || trimmed.endsWith(':'))
        return true;
    
    // Or if next line has greater indent
    QTextBlock nextBlock = block.next();
    if (nextBlock.isValid()) {
        QString nextText = nextBlock.text();
        
        int currentIndent = 0;
        for (QChar c : text) {
            if (c == ' ') currentIndent++;
            else if (c == '\t') currentIndent += 4;
            else break;
        }
        
        int nextIndent = 0;
        for (QChar c : nextText) {
            if (c == ' ') nextIndent++;
            else if (c == '\t') nextIndent += 4;
            else break;
        }
        
        if (nextIndent > currentIndent && !nextText.trimmed().isEmpty())
            return true;
    }
    
    return false;
}
