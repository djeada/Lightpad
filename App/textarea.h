#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include "lightpadsyntaxhighlighter.h"

class MainWindow;

class TextArea : public QPlainTextEdit {

    Q_OBJECT

    public:
        TextArea(QWidget *parent = nullptr);
        void lineNumberAreaPaintEvent(QPaintEvent *event);
        void updateStyle();
        void updateSyntaxHighlightTags(QString searchKey = "", QString chosenLang = "");
        int lineNumberAreaWidth();
        void increaseFontSize();
        void decreaseFontSize();
        void setFontSize(int size);
        void setMainWindow(MainWindow* window);
        int fontSize();
        void setTabWidth(int width);

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;

    private:
        MainWindow* mainWindow;
        QWidget *lineNumberArea;
        QColor highlightColor;
        QColor lineNumberAreaPenColor;
        QColor defaultPenColor;
        QColor backgroundColor;
        QString bufferText;
        QString highlightLang;
        int prevWordCount;
        QFont mainFont;
        LightpadSyntaxHighlighter* syntaxHighlighter;
};

#endif
