#ifndef GITOBJECTEXPLORERDIALOG_H
#define GITOBJECTEXPLORERDIALOG_H

#include "../../git/gitobjectdatabase.h"
#include "styleddialog.h"
#include <atomic>
#include <memory>

class QGraphicsScene;
class QGraphicsView;
class QTreeWidget;
class QTimer;

class GitObjectExplorerDialog : public StyledDialog {
  Q_OBJECT
public:
  explicit GitObjectExplorerDialog(const QString &repositoryPath,
                                   const Theme &theme,
                                   QWidget *parent = nullptr);
  ~GitObjectExplorerDialog() override;
  void searchHash(const QString &hash);

signals:
  void viewCommitRequested(const QString &hash);

private:
  using Cancellation = std::shared_ptr<std::atomic_bool>;
  void refresh();
  void search(bool resetPage = true);
  void inspect(const QString &hash, bool remember = true);
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
  Cancellation m_scanCancel, m_searchCancel, m_inspectCancel, m_verifyCancel;
  QTreeWidget *m_packs = nullptr;
  QTreeWidget *m_objects = nullptr;
  QTreeWidget *m_links = nullptr;
  QLineEdit *m_search = nullptr;
  QLabel *m_summary = nullptr;
  QLabel *m_status = nullptr;
  QLabel *m_objectInfo = nullptr;
  QPlainTextEdit *m_preview = nullptr;
  QPlainTextEdit *m_verification = nullptr;
  QTabWidget *m_detailTabs = nullptr;
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
