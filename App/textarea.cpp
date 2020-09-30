#include "textarea.h"
#include "mainwindow.h"
#include "lightpadpage.h"
#include "lightpadtabwidget.h"

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

        connect(this, &TextArea::cursorPositionChanged, this, [&] {
            QList<QTextEdit::ExtraSelection> extraSelections;

            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(highlightColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            extraSelections.append(selection);

            setExtraSelections(extraSelections);

            if (mainWindow)
                mainWindow->setRowCol(textCursor().blockNumber(), textCursor().positionInBlock());

         });

        connect(document(), &QTextDocument::undoCommandAdded, this, [&] {

            LightpadPage* page = qobject_cast<LightpadPage*>(parentWidget());

            if (page) {

                QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(parentWidget()->parentWidget());

                if (stackedWidget) {

                    LightpadTabWidget* tabWidget =  qobject_cast<LightpadTabWidget*>(parentWidget()->parentWidget()->parentWidget());

                    if (tabWidget) {
                        int index = tabWidget->indexOf(page);

                        if (index != -1)
                            tabWidget->setTabIcon(index, QIcon(":/resources/icons/unsaved.png"));
                    }
                }
            }
        });

        mainFont = QApplication::font();
        document()->setDefaultFont(mainFont);

        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(highlightColor);
        updateStyle();
        show();
        //updateSyntaxHighlightTags();

}

int TextArea::lineNumberAreaWidth() {
    QFontMetrics fm(mainFont);

    return 3 + fm.horizontalAdvance(QLatin1Char('9'))*  1.8*  numberOfDigits(blockCount());
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
    QTextDocument* doc = document();

    if (doc) {
        mainFont.setPointSize(size);
        doc->setDefaultFont(mainFont);
    }
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
    setTabStopWidth(metrics.horizontalAdvance(' ')*  width);
}

QString TextArea::getSearchWord()
{
    return searchWord;
}

void TextArea::resizeEvent(QResizeEvent* e) {
    QPlainTextEdit::resizeEvent(e);
    lineNumberArea->setGeometry(0, 0, lineNumberAreaWidth(), height());
}

void TextArea::keyPressEvent(QKeyEvent* keyEvent)
{
    if (keyEvent->matches(QKeySequence::ZoomOut) || keyEvent->matches(QKeySequence::ZoomIn))
        mainWindow->keyPressEvent(keyEvent);

    else
        QPlainTextEdit::keyPressEvent(keyEvent);
}

void TextArea::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(lineNumberArea);
    painter.setFont(mainFont);
    painter.fillRect(event->rect(), backgroundColor);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int height = QFontMetrics(mainFont).height();
    int top = blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = height + top;

    while (block.isValid() && top <= event->rect().bottom()) {

        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(lineNumberAreaPenColor);
            painter.drawText(0, top, lineNumberArea->width(), height, Qt::AlignCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + height;
        ++blockNumber;
    }
}

void TextArea::updateStyle() {
    setStyleSheet(""
                  "TextArea {"
                  "color: " + defaultPenColor.name() + "; "
                  "background-color: " + backgroundColor.name() + "; }"
                  );
}

void TextArea::updateSyntaxHighlightTags(QString searchKey, QString chosenLang) {

    searchWord = searchKey;

    if (!chosenLang.isEmpty())
        highlightLang = chosenLang;

    if (syntaxHighlighter) {
        delete syntaxHighlighter;
        syntaxHighlighter = nullptr;
    }

    if (document() && convertStrToEnum.contains(highlightLang)) {

        switch(convertStrToEnum[highlightLang]) {
            case cpp:
                syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesCpp(searchKey), QRegularExpression(QStringLiteral("/\\*")),  QRegularExpression(QStringLiteral("\\*/")), document());
                break;

            case js:
                syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesJs(searchKey), QRegularExpression(QStringLiteral("/\\*")),  QRegularExpression(QStringLiteral("\\*/")), document());
                break;

            case py:
                syntaxHighlighter = new LightpadSyntaxHighlighter(highlightingRulesPy(searchKey), QRegularExpression(QStringLiteral("/'''")),  QRegularExpression(QStringLiteral("\\'''")), document());
                break;
         }
    }
}
