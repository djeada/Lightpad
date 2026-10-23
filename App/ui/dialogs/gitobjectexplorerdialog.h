#ifndef GITOBJECTEXPLORERDIALOG_H
#define GITOBJECTEXPLORERDIALOG_H

#include "../../git/gitobjectdatabase.h"
#include "gitdialog.h"
#include <atomic>
#include <memory>

class QGraphicsScene;
class QGraphicsView;
class QTreeWidget;
class QTimer;

class GitObjectExplorerDialog : public GitDialog {
  Q_OBJECT
public:
  explicit GitObjectExplorerDialog(const QString &repositoryPath,
                                   const Theme &theme,
                                   QWidget *parent = nullptr);
  ~GitObjectExplorerDialog() override;
  void searchHash(const QString &hash);
  void focusDag();
  QString currentObjectHash() const { return m_details.hash; }

signals:
  void viewCommitRequested(const QString &hash);

private:
  using Cancellation = std::shared_ptr<std::atomic_bool>;
  void refresh();
  void search(bool resetPage = true);
  void clearInspection();
  void inspect(const QString &hash, bool remember = true,
               const GitObjectLocation &location = {});
  void loadStorage(const GitObjectLocation &location = {});
  void showStorage(const GitStoredObject &object);
  void showStorageOverview();
  void showDetails(const GitObjectDetails &details);
  void drawDag(const GitObjectDetails &details);
  void navigate(int delta);
  void updateNavigation();
  void verifySelectedPack();
  void cancelRequests();
  void applyTheme(const Theme &theme) override;

  QString m_repositoryPath;
  GitObjectStore m_store;
  GitObjectDetails m_details;
  QStringList m_history;
  int m_historyIndex = -1;
  qint64 m_skip = 0;
  QString m_pendingHash;
  QString m_loadedDiskKey;
  Cancellation m_scanCancel, m_searchCancel, m_inspectCancel, m_verifyCancel;
  Cancellation m_diskCancel;
  QTreeWidget *m_packs = nullptr;
  QTreeWidget *m_objects = nullptr;
  QTreeWidget *m_links = nullptr;
  QTreeWidget *m_trail = nullptr;
  QLineEdit *m_search = nullptr;
  QLabel *m_summary = nullptr;
  QLabel *m_status = nullptr;
  QLabel *m_objectInfo = nullptr;
  QPlainTextEdit *m_preview = nullptr;
  QPlainTextEdit *m_dagPreview = nullptr;
  QLabel *m_breadcrumb = nullptr;
  QPlainTextEdit *m_verification = nullptr;
  QTabWidget *m_detailTabs = nullptr;
  QWidget *m_diskPage = nullptr;
  QLineEdit *m_diskPath = nullptr;
  QLabel *m_diskSummary = nullptr;
  QPlainTextEdit *m_diskDecoded = nullptr;
  QPlainTextEdit *m_diskBytes = nullptr;
  QPlainTextEdit *m_diskDetails = nullptr;
  QGraphicsView *m_dag = nullptr;
  QGraphicsScene *m_scene = nullptr;
  QPushButton *m_back = nullptr;
  QPushButton *m_forward = nullptr;
  QPushButton *m_previous = nullptr;
  QPushButton *m_next = nullptr;
  QPushButton *m_copy = nullptr;
  QPushButton *m_graph = nullptr;
  QPushButton *m_verify = nullptr;
  QTimer *m_debounce = nullptr;
};

#endif
