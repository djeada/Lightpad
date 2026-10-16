#ifndef FINDREPLACEPANEL_H
#define FINDREPLACEPANEL_H

#include "../../search/searchreplaceengine.h"
#include <QHash>
#include <QPointer>
#include <QTextCursor>
#include <QWidget>
#include <atomic>
#include <functional>
#include <memory>

class TextArea;
class MainWindow;
class QPlainTextEdit;
class QLineEdit;
class QComboBox;
class QToolButton;
class QPushButton;
class QCheckBox;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QTimer;
class QKeyEvent;

class FindReplacePanel : public QWidget {
  Q_OBJECT
public:
  struct OpenDocument {
    QString key, path, label;
    QPointer<QPlainTextEdit> editor;
  };
  explicit FindReplacePanel(bool onlyFind = true, QWidget *parent = nullptr);
  ~FindReplacePanel() override;
  void toggleExtensionVisibility();
  void setReplaceVisibility(bool flag);
  bool isOnlyFind() const;
  void setOnlyFind(bool flag);
  void setDocument(QTextDocument *document);
  void setEditor(QPlainTextEdit *editor, const QString &path = {});
  void setTextArea(TextArea *area);
  void setMainWindow(MainWindow *window);
  void setDocumentProvider(std::function<QVector<OpenDocument>()> provider);
  void setProjectPath(const QString &path);
  void setGlobalMode(bool enabled);
  bool isGlobalMode() const;
  void setScope(SearchReplace::Scope scope);
  SearchReplace::Scope scope() const;
  void setFocusOnSearchBox();
  void setVimCommandMode(bool enabled);
  bool isVimCommandMode() const;
  void setSearchPrefix(const QString &prefix);
  void setSearchText(const QString &text);
  void setSearchCursorPosition(int position);
  bool isBusy() const;
  bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
  void findNext();
  void findPrevious();
  void refreshSearchResults();
  void previewReplacements();
  void applyReplacements();
  void undoReplacementBatch();

signals:
  void navigateToFile(const QString &filePath, int lineNumber, int columnNumber,
                      int matchLength = 0);
  void searchCompleted(int matches);
  void replacementFinished(bool success);

protected:
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  struct BufferEdit {
    QPointer<QTextDocument> document;
    QString key, before, after;
    QVector<SearchReplace::Edit> edits;
    int generation = -1;
    int undoSteps = -1;
  };
  struct Batch {
    QVector<BufferEdit> buffers;
    QVector<SearchReplace::DiskChange> disks;
    bool empty() const { return buffers.isEmpty() && disks.isEmpty(); }
  };
  QPointer<QPlainTextEdit> m_editor;
  QPointer<MainWindow> m_window;
  QString m_editorPath, m_projectPath;
  std::function<QVector<OpenDocument>()> m_documents;
  std::function<void(QPlainTextEdit *)> m_activateEditor;
  std::function<void(const QRegularExpression &)> m_highlight;
  std::function<void(QKeyEvent *)> m_vimKey;
  std::function<void(const QStringList &, bool)> m_diskGuard;
  std::function<void()> m_focusEditor;
  QVector<QMetaObject::Connection> m_documentConnections;
  QHash<QTextDocument *, int> m_generations;
  std::shared_ptr<std::atomic_bool> m_searchCancel, m_writeCancel;
  SearchReplace::Results m_results;
  Batch m_undo;
  QTextCursor m_selection;
  int m_requestId = 0, m_current = -1, m_page = 0, m_pendingDirection = 0;
  bool m_onlyFind = true, m_vim = false, m_preview = false,
       m_previewRequested = false;
  bool m_stale = true, m_searching = false, m_busy = false, m_applying = false;
  bool m_navigating = false, m_savedOnlyFind = true;
  bool m_keepVimHighlight = false;
  SearchReplace::Scope m_savedScope = SearchReplace::Scope::CurrentFile;
  QString m_savedQuery;
  QStringList m_findHistory, m_replaceHistory;
  int m_historyIndex = -1;
  static constexpr int PageSize = 200;

  QWidget *m_header, *m_replaceRow, *m_options, *m_filters, *m_footer, *m_pager;
  QLineEdit *m_find, *m_replace, *m_include, *m_exclude;
  QComboBox *m_scope;
  QToolButton *m_case, *m_word, *m_regex, *m_preserve, *m_more,
      *m_resultsToggle;
  QToolButton *m_next, *m_previous, *m_refresh, *m_stop, *m_close,
      *m_findHistoryButton, *m_replaceHistoryButton;
  QCheckBox *m_wrap, *m_escapes, *m_dotAll;
  QPushButton *m_replaceOne, *m_previewButton, *m_applyButton, *m_undoButton;
  QLabel *m_status, *m_pageLabel, *m_queryLabel;
  QToolButton *m_prevPage, *m_nextPage;
  QTreeWidget *m_tree;
  QTimer *m_timer;

  QVector<OpenDocument> documents() const;
  QString currentKey() const;
  SearchReplace::Options options() const;
  SearchReplace::Request request() const;
  void watchDocuments();
  void invalidate(bool clearHighlight = true);
  void cancelSearch();
  void updateUi();
  void updateStatus(const QString &text = {});
  void displayResults();
  void updateApplyButton();
  void onItemChanged(QTreeWidgetItem *item, int column);
  void navigate(int direction);
  void navigateTo(int index, bool focusEditor = false);
  QPair<int, int> occurrenceAt(int index) const;
  void replaceOne();
  void closePanel();
  void remember(QStringList &history, const QString &value);
  void showHistory(QLineEdit *field, QStringList &history);
  void setBusy(bool busy);
  void executeBatch(Batch batch, bool undo);
  bool validateBuffers(const Batch &batch, bool undo, QString *error) const;
};
#endif
