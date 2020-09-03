#include "textarea.h"

#include <QPainter>
#include <QTextBlock>
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextCursor>
#include <QApplication>

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

//static int countWords(QString text) {
//    return text.split(QRegularExpression("(\\s|\\n|\\r)+"), QString::SkipEmptyParts).count();
//}

class LineNumberArea : public QWidget {
    public:
        LineNumberArea(TextArea *editor) : QWidget(editor), textArea(editor)
        {}

        QSize sizeHint() const override {
            return QSize(textArea->lineNumberAreaWidth(), 0);
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            textArea->lineNumberAreaPaintEvent(event);
        }

    private:
        TextArea *textArea;
};

TextArea::TextArea(QWidget *parent) :
    QPlainTextEdit(parent),
    highlightColor(QColor(Qt::green).darker(250)),
    lineNumberAreaPenColor(QColor(Qt::gray).lighter(150)),
    defaultPenColor(QColor(Qt::white)),
    backgroundColor(QColor(Qt::gray).darker(200)),
    bufferText(""),
    prevWordCount(1),
    syntaxHihglighter(nullptr)
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

            if (!isReadOnly()) {
                QTextEdit::ExtraSelection selection;
                selection.format.setBackground(highlightColor);
                selection.format.setProperty(QTextFormat::FullWidthSelection, true);
                selection.cursor = textCursor();
                selection.cursor.clearSelection();
                extraSelections.append(selection);
            }

            setExtraSelections(extraSelections);
         });

        mainFont = QApplication::font();
        document()->setDefaultFont(mainFont);

        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(highlightColor);
        updateStyle();
        show();
        updateSyntaxHighlightTags(":/highlight/Cpp/0.txt");
}

int TextArea::lineNumberAreaWidth() {
    QFontMetrics fm(mainFont);

    return 3 + fm.horizontalAdvance(QLatin1Char('9')) * 1.8 * numberOfDigits(blockCount());
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
    QTextDocument *doc = document();
    mainFont.setPointSize(size);
    doc->setDefaultFont(mainFont);
}

int TextArea::fontSize()
{
    return mainFont.pointSize();
}

void TextArea::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);
    lineNumberArea->setGeometry(0, 0, lineNumberAreaWidth(), height());
}

void TextArea::lineNumberAreaPaintEvent(QPaintEvent *event) {
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
    setStyleSheet("TextArea {color: " + defaultPenColor.name() + "; background-color: " + backgroundColor.name() + "; }");
}

void TextArea::updateSyntaxHighlightTags(QString path) {

    QFile TextFile(path);
    highlightTags.clear();

    if (TextFile.open(QIODevice::ReadOnly)) {
        while (!TextFile.atEnd()) {
                QString line = TextFile.readLine();
                if (line.size() > 2)
                    highlightTags.append("\\b" + line.left(line.size() - 1) + "\\b");
       }

        if (syntaxHihglighter)
            delete syntaxHihglighter;

        if (document() && !highlightTags.isEmpty())
            syntaxHihglighter = new LightpadSyntaxHighlighter(highlightTags, document());

        TextFile.close();
    }

}
