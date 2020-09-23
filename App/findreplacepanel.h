#ifndef FINDREPLACEPANEL_H
#define FINDREPLACEPANEL_H

#include <QWidget>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class TextArea;

namespace Ui {
    class FindReplacePanel;
}

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
        void setFocusOnSearchBox();

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
        QVector<int> positions;
        int position;
        QTextCharFormat prevFormat;
        void clearSelectionFormat(QTextCursor cursor, int n);
};

#endif // FINDREPLACEPANEL_H
