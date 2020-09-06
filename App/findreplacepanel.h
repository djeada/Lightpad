#ifndef FINDREPLACEPANEL_H
#define FINDREPLACEPANEL_H

#include <QWidget>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class TextArea;

namespace Ui {
    class FindReplacePanel;
}

/*
class KeyWordsHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

    public:
        KeyWordsHighlighter(QString key, QTextDocument* parent = nullptr);

    protected:
        void highlightBlock(const QString &text) override;

    private:
        QTextCharFormat format;
        QRegularExpression pattern;
};*/

class  FindReplacePanel : public QWidget
{
    Q_OBJECT
    public:
        FindReplacePanel(bool onlyFind = true, QWidget *parent = nullptr);
        ~FindReplacePanel();
        void toggleExtensionVisibility();
        void setReplaceVisibility(bool flag);
        bool isOnlyFind();
        void setOnlyFind(bool flag);
        void setDocument(QTextDocument* doc);
        void setTextArea(TextArea* area);
        void setTheme(QString backgroundColor, QString foregroundColor);

    private slots:
        void on_more_clicked();
        void on_find_clicked();
        void on_close_clicked();

private:
        QWidget* extension;
        QTextDocument* document;
        TextArea* textArea;
        Ui::FindReplacePanel *ui;
        bool onlyFind;
};

#endif // FINDREPLACEPANEL_H
