#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QCompleter>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPlainTextDocumentLayout>
#include <QtGlobal>
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
constexpr int defaultLineSpacingPercent = 130;

class LineSpacingLayout : public QPlainTextDocumentLayout {
public:
    explicit LineSpacingLayout(QTextDocument* doc) : QPlainTextDocumentLayout(doc), m_spacing(0) {}
    
    void setLineSpacing(int pixels) {
        if (m_spacing != pixels) {
            m_spacing = qMax(0, pixels);
            requestUpdate();
        }
    }

    QRectF blockBoundingRect(const QTextBlock& block) const override {
        QRectF rect = QPlainTextDocumentLayout::blockBoundingRect(block);
        if (m_spacing > 0) {
            rect.setHeight(rect.height() + m_spacing);
        }
        return rect;
    }

private:
    int m_spacing;
};

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
    , m_columnSelectionActive(false)
    , m_showWhitespace(false)
    , m_showIndentGuides(false)
{
    initializeIconCache();
    setupTextArea();
    mainFont = QApplication::font();
    document()->setDefaultFont(mainFont);
    
    auto* layout = new LineSpacingLayout(document());
    document()->setDocumentLayout(layout);
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
    , m_columnSelectionActive(false)
    , m_showWhitespace(false)
    , m_showIndentGuides(false)
{
    initializeIconCache();
    setupTextArea();
    mainFont = settings.mainFont;
    document()->setDefaultFont(mainFont);
    
    auto* layout = new LineSpacingLayout(document());
    document()->setDocumentLayout(layout);
    applyLineSpacing(defaultLineSpacingPercent);
    show();
}

void TextArea::setupTextArea()
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &TextArea::blockCountChanged, this, [&] {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    });

    connect(this, &TextArea::updateRequest, this, [&](const QRect& rect, int) {
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
    // Throttle to avoid excessive updates during fast scrolling
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        // Throttle scroll updates - only update if not already pending
        static bool updateScheduled = false;
        if (!updateScheduled) {
            updateScheduled = true;
            QTimer::singleShot(16, this, [this]() { // ~60fps max
                updateScheduled = false;
                updateHighlighterViewport();
            });
        }
    });

    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    updateCursorPositionChangedCallbacks();
    clearLineHighlight();
}

void TextArea::applyLineSpacing(int percent)
{
    if (auto* layout = dynamic_cast<LineSpacingLayout*>(document()->documentLayout())) {
        QFontMetrics fm(mainFont);
        int extraPixels = fm.height() * (percent - 100) / 100;
        layout->setLineSpacing(extraPixels);
    }
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
    if (mainWindow) {
        applySelectionPalette(mainWindow->getTheme());
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
    applySelectionPalette(settings.theme);
    setAutoIdent(settings.autoIndent);
    showLineNumbers(settings.showLineNumberArea);
    highlihtCurrentLine(settings.lineHighlighted);
    highlihtMatchingBracket(settings.matchingBracketsHighlighted);
    setFont(settings.mainFont);
}

void TextArea::applySelectionPalette(const Theme& theme)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Base, theme.backgroundColor);
    pal.setColor(QPalette::Text, theme.foregroundColor);
    pal.setColor(QPalette::Highlight, theme.accentSoftColor);
    pal.setColor(QPalette::HighlightedText, theme.foregroundColor);
    setPalette(pal);
    if (viewport()) {
        viewport()->setPalette(pal);
    }
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
    
    // Ctrl+Shift+I - Split selection into lines
    if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && keyEvent->key() == Qt::Key_I) {
        splitSelectionIntoLines();
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

void TextArea::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextCursor cursor = textCursor();

    if (lineHighlighted && !cursor.hasSelection()) {
        QTextEdit::ExtraSelection selection;
        QColor color = mainWindow ? mainWindow->getTheme().highlightColor : highlightColor;
        selection.format.setBackground(color);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = cursor;
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    if (matchingBracketsHighlighted) {
        auto addBracketSelection = [&](QTextCursor::MoveOperation op, const QChar& startStr,
                                       const QChar& endStr,
                                       std::function<int(const QString&, int, QChar, QChar)> function) {
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
            auto pos = function(plainText, textCursor().position(), startStr, endStr);
            if (pos != -1) {
                QTextCursor match = textCursor();
                match.setPosition(pos);
                match.movePosition(op, QTextCursor::KeepAnchor);
                selection.cursor = match;
                extraSelections.append(selection);
            }
        };

        auto nextCursor = textCursor();
        auto nextResult = nextCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        auto startStr = nextResult ? nextCursor.selectedText().front() : QChar(' ');

        auto prevCursor = textCursor();
        auto prevResult = prevCursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        auto endStr = prevResult ? prevCursor.selectedText().front() : QChar(' ');

        if (brackets.contains(startStr))
            addBracketSelection(QTextCursor::NextCharacter, startStr, brackets[startStr], &findClosingParentheses);
        else if (brackets.values().contains(endStr))
            addBracketSelection(QTextCursor::PreviousCharacter, brackets.key(endStr), endStr, &findOpeningParentheses);
    }

    setExtraSelections(extraSelections);
}

void TextArea::updateCursorPositionChangedCallbacks()
{

    disconnect(this, &TextArea::cursorPositionChanged, 0, 0);
    disconnect(this, &TextArea::selectionChanged, 0, 0);

    auto refresh = [this]() {
        updateExtraSelections();
        updateRowColDisplay();
    };

    connect(this, &TextArea::cursorPositionChanged, this, refresh);
    connect(this, &TextArea::selectionChanged, this, refresh);

    refresh();
}

void TextArea::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(lineNumberArea);
    painter.setFont(mainFont);

    auto color = mainWindow ? mainWindow->getTheme().lineNumberAreaColor : backgroundColor;

    painter.fillRect(event->rect(), color);

    auto block = firstVisibleBlock();
    auto blockNumber = block.blockNumber();
    auto rect = blockBoundingGeometry(block).translated(contentOffset());
    auto top = rect.top();
    auto bottom = top + rect.height();
    const int fontHeight = QFontMetrics(mainFont).height();
    color = mainWindow ? mainWindow->getTheme().foregroundColor : lineNumberAreaPenColor;

    // Prepare a map for quick lookup of git diff lines
    QMap<int, int> diffLineMap;  // line -> type (0=add, 1=modify, 2=delete)
    for (const auto& diffLine : m_gitDiffLines) {
        diffLineMap[diffLine.first] = diffLine.second;
    }

    while (block.isValid() && top <= event->rect().bottom()) {

        if (block.isVisible() && bottom >= event->rect().top()) {
            auto number = QString::number(blockNumber);
            painter.setPen(color);
            painter.drawText(0, top, lineNumberArea->width(), fontHeight, Qt::AlignCenter, number);
            
            // Draw git diff indicator on the left edge
            int lineNum = blockNumber + 1;  // 1-based line numbers
            if (diffLineMap.contains(lineNum)) {
                int diffType = diffLineMap[lineNum];
                QColor diffColor;
                if (diffType == 0) {
                    diffColor = QColor(76, 175, 80);  // Green for added lines
                } else if (diffType == 1) {
                    diffColor = QColor(33, 150, 243);  // Blue for modified lines
                } else {
                    diffColor = QColor(244, 67, 54);  // Red for deleted lines
                }
                painter.fillRect(0, top, 3, bottom - top, diffColor);
            }
        }

        block = block.next();
        top = bottom;
        if (!block.isValid())
            break;
        rect = blockBoundingGeometry(block).translated(contentOffset());
        bottom = top + rect.height();
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
    
    // Draw indent guides if enabled
    if (m_showIndentGuides) {
        QPainter painter(viewport());
        painter.setPen(QPen(QColor(128, 128, 128, 60), 1, Qt::DotLine));
        
        QFontMetrics fm(mainFont);
        int spaceWidth = fm.horizontalAdvance(' ');
        int indentWidth = spaceWidth * 4;  // 4-space indent guides
        
        QTextBlock block = firstVisibleBlock();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());
        
        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                QString text = block.text();
                
                // Count leading whitespace to determine indent level
                int indent = 0;
                for (QChar c : text) {
                    if (c == ' ') indent++;
                    else if (c == '\t') indent += 4;
                    else break;
                }
                
                // Get the x position of the start of this block
                QTextCursor blockStart(block);
                blockStart.setPosition(block.position());
                QRect startRect = cursorRect(blockStart);
                int xOffset = startRect.left();
                
                // Draw vertical lines at each indent level
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
    
    // Draw whitespace characters if enabled
    if (m_showWhitespace) {
        QPainter painter(viewport());
        painter.setPen(QPen(QColor(128, 128, 128, 80), 1));
        
        QFontMetrics fm(mainFont);
        int spaceWidth = fm.horizontalAdvance(' ');
        int tabWidth = fm.horizontalAdvance(' ') * 4;  // Assume 4-space tabs
        
        QTextBlock block = firstVisibleBlock();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());
        
        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                QString text = block.text();
                
                // Get the x position of the start of this block
                QTextCursor blockStart(block);
                blockStart.setPosition(block.position());
                QRect startRect = cursorRect(blockStart);
                int xOffset = startRect.left();
                int yCenter = startRect.center().y();
                
                // Calculate x positions using font metrics (more efficient)
                int x = xOffset;
                for (int i = 0; i < text.length(); ++i) {
                    if (text[i] == ' ') {
                        // Draw a middle dot for spaces
                        painter.drawPoint(x + spaceWidth / 2, yCenter);
                        x += spaceWidth;
                    } else if (text[i] == '\t') {
                        // Draw an arrow for tabs
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
    
    // Alt+Shift+Click to start column/box selection
    if ((event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier)) == (Qt::AltModifier | Qt::ShiftModifier) && event->button() == Qt::LeftButton) {
        startColumnSelection(event->pos());
        return;
    }
    
    // Ctrl+Alt+Click to add cursor
    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) == (Qt::ControlModifier | Qt::AltModifier)) {
        QTextCursor cursor = cursorForPosition(event->pos());
        m_extraCursors.append(textCursor());
        setTextCursor(cursor);
        drawExtraCursors();
        return;
    }
    
    QPlainTextEdit::mousePressEvent(event);
}

void TextArea::mouseMoveEvent(QMouseEvent* event)
{
    // Update column selection while dragging
    if (m_columnSelectionActive && (event->buttons() & Qt::LeftButton)) {
        updateColumnSelection(event->pos());
        return;
    }
    
    QPlainTextEdit::mouseMoveEvent(event);
}

void TextArea::mouseReleaseEvent(QMouseEvent* event)
{
    // End column selection
    if (m_columnSelectionActive) {
        endColumnSelection();
        return;
    }
    
    QPlainTextEdit::mouseReleaseEvent(event);
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
    
    // Check for #region marker - find matching #endregion
    if (isRegionStart(startBlock)) {
        return findRegionEndBlock(startBlock);
    }
    
    // Check for comment block start - find end of comment block
    if (isCommentBlockStart(startBlock)) {
        return findCommentBlockEnd(startBlock);
    }
    
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
    QString trimmed = text.trimmed();
    
    // Check for #region marker (supports #region, //region, //#region, // #region)
    if (isRegionStart(blockNumber))
        return true;
    
    // Check for multi-line comment block start
    if (isCommentBlockStart(blockNumber))
        return true;
    
    // Foldable if line ends with { or :
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

int TextArea::getFoldingLevel(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return 0;
    
    QString text = block.text();
    int indent = 0;
    
    // Calculate indentation level
    for (QChar c : text) {
        if (c == ' ') indent++;
        else if (c == '\t') indent += 4;
        else break;
    }
    
    // Also count brace nesting level by looking at preceding blocks.
    // Note: This has O(n) complexity per call. For foldToLevel() which calls this
    // for each block, the total complexity is O(n²). This is acceptable for the
    // infrequent fold-to-level operation, but could be optimized with caching
    // if needed for real-time use cases.
    int braceLevel = 0;
    QTextBlock prevBlock = document()->begin();
    while (prevBlock.isValid() && prevBlock.blockNumber() < blockNumber) {
        QString prevText = prevBlock.text();
        for (QChar c : prevText) {
            if (c == '{') braceLevel++;
            else if (c == '}') braceLevel--;
        }
        prevBlock = prevBlock.next();
    }
    
    // Use a combination of indent and brace level
    // Divide indent by 4 (assuming 4-space tabs) to get rough level
    int indentLevel = indent / 4;
    
    // Return the maximum of indent-based and brace-based level
    return qMax(indentLevel, braceLevel);
}

void TextArea::foldToLevel(int level)
{
    // First, unfold everything
    unfoldAll();
    
    // Then fold all blocks that are at a level greater than the specified level
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        int blockNum = block.blockNumber();
        if (isFoldable(blockNum)) {
            int blockLevel = getFoldingLevel(blockNum);
            
            // Fold this block if it's at or above the specified level
            if (blockLevel >= level) {
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

// ============================================================================
// Whitespace Visualization
// ============================================================================

void TextArea::setShowWhitespace(bool show)
{
    if (m_showWhitespace != show) {
        m_showWhitespace = show;
        viewport()->update();
    }
}

bool TextArea::showWhitespace() const
{
    return m_showWhitespace;
}

// ============================================================================
// Indent Guides Support
// ============================================================================

void TextArea::setShowIndentGuides(bool show)
{
    if (m_showIndentGuides != show) {
        m_showIndentGuides = show;
        viewport()->update();
    }
}

bool TextArea::showIndentGuides() const
{
    return m_showIndentGuides;
}

// ============================================================================
// Git Diff Gutter Support
// ============================================================================

void TextArea::setGitDiffLines(const QList<QPair<int, int>>& diffLines)
{
    m_gitDiffLines = diffLines;
    lineNumberArea->update();
}

void TextArea::clearGitDiffLines()
{
    m_gitDiffLines.clear();
    lineNumberArea->update();
}

// ============================================================================
// Column/Box Selection Support
// ============================================================================

void TextArea::startColumnSelection(const QPoint& pos)
{
    m_columnSelectionActive = true;
    m_columnSelectionStart = pos;
    m_columnSelectionEnd = pos;
    
    // Clear existing extra cursors
    m_extraCursors.clear();
    
    // Set initial cursor position
    QTextCursor cursor = cursorForPosition(pos);
    cursor.clearSelection();
    setTextCursor(cursor);
}

void TextArea::updateColumnSelection(const QPoint& pos)
{
    if (!m_columnSelectionActive)
        return;
    
    m_columnSelectionEnd = pos;
    
    // Get the line/column at start and end positions
    QTextCursor startCursor = cursorForPosition(m_columnSelectionStart);
    QTextCursor endCursor = cursorForPosition(m_columnSelectionEnd);
    
    int startLine = startCursor.blockNumber();
    int endLine = endCursor.blockNumber();
    
    // Ensure startLine <= endLine
    if (startLine > endLine) {
        std::swap(startLine, endLine);
    }
    
    // Get column positions in pixels for proper column selection
    int startCol = startCursor.positionInBlock();
    int endCol = endCursor.positionInBlock();
    
    // Ensure startCol <= endCol for selection
    int leftCol = qMin(startCol, endCol);
    int rightCol = qMax(startCol, endCol);
    
    // Clear extra cursors
    m_extraCursors.clear();
    
    // Create a cursor for each line in the selection
    bool first = true;
    for (int line = startLine; line <= endLine; ++line) {
        QTextBlock block = document()->findBlockByNumber(line);
        if (!block.isValid())
            continue;
        
        QTextCursor cursor(block);
        int lineLength = block.text().length();
        
        // Move to the left column position (clamped to line length)
        int actualLeftCol = qMin(leftCol, lineLength);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, actualLeftCol);
        
        // Select to the right column (clamped to line length)
        int actualRightCol = qMin(rightCol, lineLength);
        int selectionLength = actualRightCol - actualLeftCol;
        if (selectionLength > 0) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, selectionLength);
        }
        
        if (first) {
            setTextCursor(cursor);
            first = false;
        } else {
            m_extraCursors.append(cursor);
        }
    }
    
    drawExtraCursors();
}

void TextArea::endColumnSelection()
{
    m_columnSelectionActive = false;
    // Keep the cursors in place, they're already set up
}

// ============================================================================
// Split Selection Into Lines
// ============================================================================

void TextArea::splitSelectionIntoLines()
{
    QTextCursor cursor = textCursor();
    
    if (!cursor.hasSelection())
        return;
    
    // Get the selected text and split by lines
    int selStart = cursor.selectionStart();
    int selEnd = cursor.selectionEnd();
    
    // Find the block/line of the selection start and end
    QTextCursor startCursor(document());
    startCursor.setPosition(selStart);
    int startLine = startCursor.blockNumber();
    int startCol = startCursor.positionInBlock();
    
    QTextCursor endCursor(document());
    endCursor.setPosition(selEnd);
    int endLine = endCursor.blockNumber();
    int endCol = endCursor.positionInBlock();
    
    // If selection is on a single line, nothing to split
    if (startLine == endLine)
        return;
    
    // Clear extra cursors
    m_extraCursors.clear();
    
    // Create a cursor at the end of selection on each line
    bool first = true;
    for (int line = startLine; line <= endLine; ++line) {
        QTextBlock block = document()->findBlockByNumber(line);
        if (!block.isValid())
            continue;
        
        QTextCursor lineCursor(block);
        lineCursor.movePosition(QTextCursor::StartOfBlock);
        
        if (line == startLine) {
            // First line: from startCol to end of line
            lineCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, startCol);
            lineCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        } else if (line == endLine) {
            // Last line: from start of line to endCol
            lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, endCol);
        } else {
            // Middle lines: select entire line content (not the newline)
            lineCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        }
        
        if (first) {
            setTextCursor(lineCursor);
            first = false;
        } else {
            m_extraCursors.append(lineCursor);
        }
    }
    
    drawExtraCursors();
}

// ============================================================================
// Custom Fold Regions (#region/#endregion)
// ============================================================================

bool TextArea::isRegionStart(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text().trimmed();
    
    // Support various region marker styles:
    // #region, //region, //#region, // #region, /* #region
    // Also support Python/Shell: # region
    static const QStringList regionPatterns = {
        "#region", "// region", "//region", "//#region", "// #region",
        "/* region", "/*region", "/* #region", "/*#region",
        "# region", "#pragma region"
    };
    
    QString lowerText = text.toLower();
    for (const QString& pattern : regionPatterns) {
        if (lowerText.startsWith(pattern.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool TextArea::isRegionEnd(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text().trimmed();
    
    // Support various endregion marker styles
    static const QStringList endregionPatterns = {
        "#endregion", "// endregion", "//endregion", "//#endregion", "// #endregion",
        "/* endregion", "/*endregion", "/* #endregion", "/*#endregion",
        "# endregion", "#pragma endregion"
    };
    
    QString lowerText = text.toLower();
    for (const QString& pattern : endregionPatterns) {
        if (lowerText.startsWith(pattern.toLower())) {
            return true;
        }
    }
    
    return false;
}

int TextArea::findRegionEndBlock(int startBlock) const
{
    // Find matching #endregion, handling nested regions
    int depth = 1;
    QTextBlock block = document()->findBlockByNumber(startBlock + 1);
    
    while (block.isValid()) {
        int blockNum = block.blockNumber();
        
        if (isRegionStart(blockNum)) {
            depth++;
        } else if (isRegionEnd(blockNum)) {
            depth--;
            if (depth == 0) {
                return blockNum;
            }
        }
        
        block = block.next();
    }
    
    // No matching endregion found, fold to end of document
    return document()->blockCount() - 1;
}

// ============================================================================
// Comment Block Folding
// ============================================================================

bool TextArea::isCommentBlockStart(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text().trimmed();
    
    // Check for C-style block comment start: /*
    if (text.startsWith("/*") && !text.contains("*/")) {
        return true;
    }
    
    // Check for consecutive single-line comments (3 or more lines)
    // Only mark the first line as foldable
    if (text.startsWith("//") || text.startsWith("#")) {
        // Check if previous line is NOT a comment (this is the start)
        QTextBlock prevBlock = block.previous();
        if (prevBlock.isValid()) {
            QString prevText = prevBlock.text().trimmed();
            bool prevIsComment = prevText.startsWith("//") || 
                                (prevText.startsWith("#") && !prevText.startsWith("#include") && 
                                 !prevText.startsWith("#define") && !prevText.startsWith("#pragma") &&
                                 !prevText.startsWith("#if") && !prevText.startsWith("#else") &&
                                 !prevText.startsWith("#endif") && !prevText.startsWith("#region") &&
                                 !prevText.startsWith("#endregion"));
            if (prevIsComment) {
                return false;  // Not the start of a comment block
            }
        }
        
        // Check if there are at least 2 more consecutive comment lines
        int consecutiveComments = 1;
        QTextBlock nextBlock = block.next();
        while (nextBlock.isValid() && consecutiveComments < 3) {
            QString nextText = nextBlock.text().trimmed();
            bool isComment = nextText.startsWith("//") || 
                            (nextText.startsWith("#") && !nextText.startsWith("#include") && 
                             !nextText.startsWith("#define") && !nextText.startsWith("#pragma") &&
                             !nextText.startsWith("#if") && !nextText.startsWith("#else") &&
                             !nextText.startsWith("#endif") && !nextText.startsWith("#region") &&
                             !nextText.startsWith("#endregion"));
            if (isComment) {
                consecutiveComments++;
                nextBlock = nextBlock.next();
            } else {
                break;
            }
        }
        
        return consecutiveComments >= 3;
    }
    
    return false;
}

int TextArea::findCommentBlockEnd(int startBlock) const
{
    QTextBlock block = document()->findBlockByNumber(startBlock);
    if (!block.isValid())
        return startBlock;
    
    QString text = block.text().trimmed();
    
    // Handle C-style block comments
    if (text.startsWith("/*")) {
        QTextBlock searchBlock = block;
        while (searchBlock.isValid()) {
            if (searchBlock.text().contains("*/")) {
                return searchBlock.blockNumber();
            }
            searchBlock = searchBlock.next();
        }
        return document()->blockCount() - 1;
    }
    
    // Handle consecutive single-line comments
    int lastCommentBlock = startBlock;
    QTextBlock nextBlock = block.next();
    
    while (nextBlock.isValid()) {
        QString nextText = nextBlock.text().trimmed();
        bool isComment = nextText.startsWith("//") || 
                        (nextText.startsWith("#") && !nextText.startsWith("#include") && 
                         !nextText.startsWith("#define") && !nextText.startsWith("#pragma") &&
                         !nextText.startsWith("#if") && !nextText.startsWith("#else") &&
                         !nextText.startsWith("#endif") && !nextText.startsWith("#region") &&
                         !nextText.startsWith("#endregion"));
        
        if (isComment) {
            lastCommentBlock = nextBlock.blockNumber();
            nextBlock = nextBlock.next();
        } else {
            break;
        }
    }
    
    return lastCommentBlock;
}

void TextArea::foldComments()
{
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        int blockNum = block.blockNumber();
        if (isCommentBlockStart(blockNum) && !m_foldedBlocks.contains(blockNum)) {
            m_foldedBlocks.insert(blockNum);
            
            int endBlock = findCommentBlockEnd(blockNum);
            QTextBlock innerBlock = block.next();
            
            while (innerBlock.isValid() && innerBlock.blockNumber() <= endBlock) {
                innerBlock.setVisible(false);
                innerBlock = innerBlock.next();
            }
        }
        block = block.next();
    }
    
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
}

void TextArea::unfoldComments()
{
    QList<int> commentBlocks;
    for (int blockNum : m_foldedBlocks) {
        if (isCommentBlockStart(blockNum)) {
            commentBlocks.append(blockNum);
        }
    }
    
    for (int blockNum : commentBlocks) {
        m_foldedBlocks.remove(blockNum);
        
        int endBlock = findCommentBlockEnd(blockNum);
        QTextBlock block = document()->findBlockByNumber(blockNum + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(true);
            block = block.next();
        }
    }
    
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
}

// ============================================================================
// Text Transformations
// ============================================================================

void TextArea::sortLinesAscending()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        // Select all text if nothing is selected
        cursor.select(QTextCursor::Document);
    }
    
    QString selectedText = cursor.selectedText();
    // QTextCursor uses Unicode paragraph separator, convert to newlines
    selectedText.replace(QChar::ParagraphSeparator, '\n');
    
    QStringList lines = selectedText.split('\n');
    std::sort(lines.begin(), lines.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    
    cursor.insertText(lines.join('\n'));
}

void TextArea::sortLinesDescending()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::Document);
    }
    
    QString selectedText = cursor.selectedText();
    selectedText.replace(QChar::ParagraphSeparator, '\n');
    
    QStringList lines = selectedText.split('\n');
    std::sort(lines.begin(), lines.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) > 0;
    });
    
    cursor.insertText(lines.join('\n'));
}

void TextArea::transformToUppercase()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
    }
    
    if (cursor.hasSelection()) {
        QString selectedText = cursor.selectedText();
        cursor.insertText(selectedText.toUpper());
    }
}

void TextArea::transformToLowercase()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
    }
    
    if (cursor.hasSelection()) {
        QString selectedText = cursor.selectedText();
        cursor.insertText(selectedText.toLower());
    }
}

void TextArea::transformToTitleCase()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
    }
    
    if (cursor.hasSelection()) {
        QString selectedText = cursor.selectedText();
        QString result;
        bool capitalizeNext = true;
        
        for (int i = 0; i < selectedText.length(); ++i) {
            QChar c = selectedText.at(i);
            if (c.isLetter()) {
                if (capitalizeNext) {
                    result.append(c.toUpper());
                    capitalizeNext = false;
                } else {
                    result.append(c.toLower());
                }
            } else {
                result.append(c);
                if (c.isSpace() || c == '-' || c == '_') {
                    capitalizeNext = true;
                }
            }
        }
        
        cursor.insertText(result);
    }
}

// ============================================================================
// Word Wrap
// ============================================================================

void TextArea::setWordWrapEnabled(bool enabled)
{
    if (enabled) {
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
    } else {
        setLineWrapMode(QPlainTextEdit::NoWrap);
    }
}

bool TextArea::wordWrapEnabled() const
{
    return lineWrapMode() == QPlainTextEdit::WidgetWidth;
}
