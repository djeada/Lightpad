#ifndef LIGHTPADPAGE_H
#define LIGHTPADPAGE_H

#include "../filetree/filedirtreecontroller.h"
#include "../filetree/filedirtreemodel.h"
#include "../filetree/gitfilesystemmodel.h"
#include "textarea.h"

#include <QFileSystemModel>
#include <QTreeView>
#include <QWidget>

class LightpadPage;
class Minimap;
class GitIntegration;
class QLabel;
class QLineEdit;
class QToolButton;
struct Theme;
class ThemeDefinition;

class LightpadTreeView : public QTreeView {

  Q_OBJECT

public:
  LightpadTreeView(LightpadPage *parent = nullptr);
  ~LightpadTreeView();

  QStringList selectedPaths() const;

  QString targetDirectory() const;

  QString rootPath() const;

  FileDirTreeController *fileOperationsController() const {
    return fileController;
  }

  void promptNewFile();
  void promptNewFolder();

signals:
  void runTestsRequested(const QString &path);
  void runFileRequested(const QString &path);
  void debugFileRequested(const QString &path);
  void toggleTestMarkerRequested(const QString &path, bool markAsTest);
  void revealInFileManagerRequested(const QString &path);
  void openInTerminalRequested(const QString &directory);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  LightpadPage *parentPage;
  FileDirTreeModel *fileModel;
  FileDirTreeController *fileController;

  void setupShortcuts();
  void showContextMenu(const QPoint &pos);
  void buildRootMenu(QMenu &menu);
  void buildEntryMenu(QMenu &menu, const QString &filePath,
                      const QStringList &selection);

  QString dropDirectoryAt(const QPoint &position) const;
  bool acceptsDropOf(const QStringList &sources,
                     const QString &destination) const;
  void performDrop(const QStringList &sources, const QString &destination,
                   bool copy);
  void copySelection();
  void cutSelection();
  void pasteIntoTarget();
  void renameSelection();
  void duplicateSelection();
  void deleteSelection(DeleteMode mode);
};

class LightpadPage : public QWidget {

  Q_OBJECT

public:
  LightpadPage(QWidget *parent = nullptr, bool treeViewHidden = true);
  ~LightpadPage(){};
  QTreeView *getTreeView();
  TextArea *getTextArea();
  Minimap *getMinimap();
  void setTreeViewVisible(bool flag);
  void setMinimapVisible(bool flag);
  bool isMinimapVisible() const;
  void setModelRootIndex(QString path);
  void setCustomContentWidget(QWidget *widget);
  void setMainWindow(MainWindow *window);
  void setSharedFileSystemModel(GitFileSystemModel *sharedModel);
  void setFilePath(QString path);
  void closeTabPage(QString path);
  void updateModel();
  QString getFilePath();
  QString getFilePath(const QModelIndex &index);

  bool hasRunTemplate() const;

  QString getAssignedTemplateId() const;

  void setProjectRootPath(const QString &path);

  QString getProjectRootPath() const;

  void setGitIntegration(GitIntegration *git);

  void setGitStatusEnabled(bool enabled);

  void refreshGitStatus();
  void setTreeFilterText(const QString &text);
  QString getTreeFilterText() const;
  void activateTreeIndex(const QModelIndex &index);

  void revealPath(const QString &path);
  void applyTheme(const Theme &theme);
  void applyTheme(const ThemeDefinition &theme);
  MainWindow *getMainWindow() const;

private:
  void applyTreeFilter();
  bool updateTreeVisibilityRecursive(const QModelIndex &parent,
                                     const QString &needle);

  MainWindow *mainWindow;
  QWidget *treeContainer;
  QWidget *treeHeader;
  QLabel *treeTitleLabel;
  QLineEdit *treeFilterEdit;
  QToolButton *treeNewFileButton;
  QToolButton *treeNewFolderButton;
  QToolButton *treeRefreshButton;
  QToolButton *treeCollapseButton;
  QToolButton *treeExpandButton;
  QTreeView *treeView;
  TextArea *textArea;
  Minimap *minimap;
  GitFileSystemModel *model;
  bool m_ownsModel;
  GitIntegration *m_gitIntegration;
  QString m_treeFilterText;
  QString filePath;
  QString projectRootPath;
};

#endif
