#ifndef CONFLICTCENTERPANEL_H
#define CONFLICTCENTERPANEL_H

#include "../../git/gitconflictmodel.h"
#include "../../settings/theme.h"
#include <QWidget>

class FlowLayout;
class GitIntegration;
class QLabel;
class QProgressBar;
class QScrollArea;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class ConflictCenterPanel : public QWidget {
  Q_OBJECT

public:
  explicit ConflictCenterPanel(QWidget *parent = nullptr);

  void setGitIntegration(GitIntegration *git);
  void refresh();
  void applyTheme(const Theme &theme);

  bool hasConflicts() const { return unresolvedFileCount() > 0; }
  int conflictedFileCount() const { return m_tracked.size(); }
  int unresolvedFileCount() const;
  bool operationInProgress() const { return m_operationActive; }

  const GitConflictContext &context() const { return m_context; }

signals:
  void fileOpenRequested(const QString &absolutePath);
  void repositoryChanged();

  void attentionNeeded();

private slots:
  void onFileActivated(QTreeWidgetItem *item, int column);
  void onKeepAllOurs();
  void onKeepAllTheirs();
  void onAbort();
  void onFinish();

private:
  void buildUi();
  void buildHeader(QVBoxLayout *layout);
  void buildFileList(QVBoxLayout *layout);
  void buildGlossary(QVBoxLayout *layout);
  void buildActions(QVBoxLayout *layout);
  QWidget *makeButtonRow(QVBoxLayout *layout, FlowLayout **flow);
  void updateFromContext();
  QString selectedPath() const;
  bool confirm(const QString &title, const QString &question);

  struct TrackedFile {
    QString path;
    GitConflictClass conflictClass = GitConflictClass::Unknown;
    int spots = 0;
    bool done = false;
  };

  void updateTracking();

  GitIntegration *m_git = nullptr;
  GitConflictContext m_context;
  QList<TrackedFile> m_tracked;
  bool m_operationActive = false;
  Theme m_theme;
  bool m_announced = false;

  QScrollArea *m_scroll = nullptr;
  QWidget *m_banner = nullptr;
  QLabel *m_bannerIcon = nullptr;
  QLabel *m_bannerTitle = nullptr;
  QLabel *m_operationLabel = nullptr;
  QLabel *m_summaryLabel = nullptr;
  QProgressBar *m_progress = nullptr;

  QTreeWidget *m_fileTree = nullptr;
  QLabel *m_emptyLabel = nullptr;

  QLabel *m_glossaryOurs = nullptr;
  QLabel *m_glossaryTheirs = nullptr;
  QLabel *m_glossaryBase = nullptr;

  QPushButton *m_openButton = nullptr;
  QPushButton *m_keepAllOursButton = nullptr;
  QPushButton *m_keepAllTheirsButton = nullptr;
  QPushButton *m_abortButton = nullptr;
  QPushButton *m_finishButton = nullptr;
};

#endif
