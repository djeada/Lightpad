#ifndef FINDREPLACEPANEL_H
#define FINDREPLACEPANEL_H

#include <QRegularExpression>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTreeWidget>
#include <QWidget>

class TextArea;
class MainWindow;
class QTimer;
class QLabel;

namespace Ui {
class FindReplacePanel;
}

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
  FindReplacePanel(bool onlyFind = true, QWidget *parent = nullptr);
  ~FindReplacePanel();
  void toggleExtensionVisibility();
  void setReplaceVisibility(bool flag);
  bool isOnlyFind();
  bool isOnlyFind() const;
  void setOnlyFind(bool flag);
  void setDocument(QTextDocument *doc);
  void setTextArea(TextArea *area);
  void setMainWindow(MainWindow *window);
  void setProjectPath(const QString &path);
  void setGlobalMode(bool enabled);
  void setFocusOnSearchBox();
  void setVimCommandMode(bool enabled);
  bool isVimCommandMode() const;
  void setSearchPrefix(const QString &prefix);
  void setSearchText(const QString &text);

  bool eventFilter(QObject *obj, QEvent *event) override;

  bool isGlobalMode() const;

signals:

  void navigateToFile(const QString &filePath, int lineNumber,
                      int columnNumber);

private slots:
  void on_more_clicked();
  void on_find_clicked();
  void on_findPrevious_clicked();
  void on_close_clicked();
  void on_replaceSingle_clicked();
  void on_replaceAll_clicked();
  void on_localMode_toggled(bool checked);
  void on_globalMode_toggled(bool checked);
  void onGlobalResultClicked(QTreeWidgetItem *item, int column);
  void onSearchTextChanged(const QString &text);
  void onTextAreaContentsChanged();
  void refreshSearchResults();

private:
  void handleVimCommandKey(QKeyEvent *event);
  QWidget *extension;
  QTextDocument *document;
  TextArea *textArea;
  MainWindow *mainWindow;
  Ui::FindReplacePanel *ui;
  QVector<int> positions;
  bool onlyFind;
  bool m_vimCommandMode;
  QString m_searchPrefix;
  int position;

  QString projectPath;

  QVector<GlobalSearchResult> globalResults;
  int globalResultIndex;
  QTreeWidget *resultsTree;

  QStringList searchHistory;
  int searchHistoryIndex;
  static const int MAX_SEARCH_HISTORY = 20;
  QMetaObject::Connection textAreaContentsChangedConnection;
  QTimer *refreshTimer;
  QLabel *searchStatusLabel;
  bool searchInProgress;
  bool searchExecuted;
  QString activeSearchWord;
  QString lastObservedPlainText;

  void updateCounterLabels();
  void selectSearchWord(QTextCursor &cursor, int n, int offset = 0);
  void clearSelectionFormat(QTextCursor &cursor, int n);
  void findInitial(QTextCursor &cursor, const QString &searchWord);
  void findNext(QTextCursor &cursor, const QString &searchWord, int offset = 0);
  void findPrevious(QTextCursor &cursor, const QString &searchWord);
  void replaceNext(QTextCursor &cursor, const QString &replaceWord);

  QRegularExpression buildSearchPattern(const QString &searchWord) const;
  QString applyPreserveCase(const QString &replaceWord,
                            const QString &matchedText) const;
  void addToSearchHistory(const QString &searchTerm);

  void performGlobalSearch(const QString &searchWord,
                           bool navigateToResult = true);
  void refreshGlobalResultsForCurrentFile(const QString &searchWord);
  void searchInFile(const QString &filePath, const QRegularExpression &pattern);
  QVector<GlobalSearchResult>
  collectMatchesInContent(const QString &filePath, const QString &content,
                          const QRegularExpression &pattern) const;
  QString currentFilePath() const;
  void displayGlobalResults();
  void navigateToGlobalResult(int index, bool emitNavigation = true);
  void updateModeUI();
  QStringList getProjectFiles() const;

  void displayLocalResults(const QString &searchWord);
  void onLocalResultClicked(QTreeWidgetItem *item, int column);
  void beginSearchFeedback(const QString &message = QString("Searching..."));
  void updateSearchFeedback(const QString &message);
  void endSearchFeedback(int matchCount);
  void clearSearchFeedback();
  QMap<QString, QVector<GlobalSearchResult>> globalResultsByFile;
};

#endif
