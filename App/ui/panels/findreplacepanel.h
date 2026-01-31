#ifndef FINDREPLACEPANEL_H
#define FINDREPLACEPANEL_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QWidget>
#include <QStringList>

class TextArea;

namespace Ui {
class FindReplacePanel;
}

class FindReplacePanel : public QWidget {
    Q_OBJECT
public:
    FindReplacePanel(bool onlyFind = true, QWidget* parent = nullptr);
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
    void on_findPrevious_clicked();
    void on_close_clicked();
    void on_replaceSingle_clicked();
    void on_replaceAll_clicked();

private:
    QWidget* extension;
    QTextDocument* document;
    TextArea* textArea;
    Ui::FindReplacePanel* ui;
    QVector<int> positions;
    QTextCharFormat prevFormat;
    QTextCharFormat colorFormat;
    bool onlyFind;
    int position;
    
    // Search history
    QStringList searchHistory;
    int searchHistoryIndex;
    static const int MAX_SEARCH_HISTORY = 20;
    
    void updateCounterLabels();
    void selectSearchWord(QTextCursor& cursor, int n, int offset = 0);
    void clearSelectionFormat(QTextCursor& cursor, int n);
    void findInitial(QTextCursor& cursor, const QString& searchWord);
    void findNext(QTextCursor& cursor, const QString& searchWord, int offset = 0);
    void findPrevious(QTextCursor& cursor, const QString& searchWord);
    void replaceNext(QTextCursor& cursor, const QString& replaceWord);
    
    // Search options
    QRegularExpression buildSearchPattern(const QString& searchWord) const;
    QString applyPreserveCase(const QString& replaceWord, const QString& matchedText) const;
    void addToSearchHistory(const QString& searchTerm);
};

#endif // FINDREPLACEPANEL_H
