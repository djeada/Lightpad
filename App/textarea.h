#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include "lightpadsyntaxhighlighter.h"

class TextArea : public QPlainTextEdit {

    Q_OBJECT

    public:
        TextArea(QWidget *parent = nullptr);
        void lineNumberAreaPaintEvent(QPaintEvent *event);
        void updateStyle();
        void updateSyntaxHighlightTags(QString path);
        int lineNumberAreaWidth();
        void increaseFontSize();
        void decreaseFontSize();
        void changeFontSize(int size);

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        QWidget *lineNumberArea;
        QColor highlightColor;
        QColor lineNumberAreaPenColor;
        QColor defaultPenColor;
        QColor backgroundColor;
        QString bufferText;
        int prevWordCount;
        QFont mainFont;
        QStringList highlightTags;
        LightpadSyntaxHighlighter* syntaxHihglighter;
};

#endif
