#ifndef FINDREPLACEPANEL_H
#define FINDREPLACEPANEL_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QWidget>
#include <QStringList>
#include <QTreeWidget>

class TextArea;
class MainWindow;

namespace Ui {
class FindReplacePanel;
}

// Represents a search result in global mode
struct GlobalSearchResult {
    QString filePath;
    int lineNumber;
    int columnNumber;
    int matchLength;
    QString lineContent;
};

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
    void setMainWindow(MainWindow* window);
    void setProjectPath(const QString& path);
    void setFocusOnSearchBox();
    
    // Search mode
    bool isGlobalMode() const;

signals:
    // Emitted when user clicks on a search result to navigate to it
    void navigateToFile(const QString& filePath, int lineNumber, int columnNumber);

private slots:
    void on_more_clicked();
    void on_find_clicked();
    void on_findPrevious_clicked();
    void on_close_clicked();
    void on_replaceSingle_clicked();
    void on_replaceAll_clicked();
    void on_localMode_toggled(bool checked);
    void on_globalMode_toggled(bool checked);
    void onGlobalResultClicked(QTreeWidgetItem* item, int column);

private:
    QWidget* extension;
    QTextDocument* document;
    TextArea* textArea;
    MainWindow* mainWindow;
    Ui::FindReplacePanel* ui;
    QVector<int> positions;
    QTextCharFormat prevFormat;
    QTextCharFormat colorFormat;
    bool onlyFind;
    int position;
    
    // Project path for global search
    QString projectPath;
    
    // Global search results
    QVector<GlobalSearchResult> globalResults;
    int globalResultIndex;
    QTreeWidget* resultsTree;
    
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
    
    // Global search methods
    void performGlobalSearch(const QString& searchWord);
    void searchInFile(const QString& filePath, const QRegularExpression& pattern);
    void displayGlobalResults();
    void navigateToGlobalResult(int index);
    void updateModeUI();
    QStringList getProjectFiles() const;
    
    // Local search results display
    void displayLocalResults(const QString& searchWord);
    void onLocalResultClicked(QTreeWidgetItem* item, int column);
};

#endif // FINDREPLACEPANEL_H
