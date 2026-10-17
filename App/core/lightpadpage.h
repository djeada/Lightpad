#ifndef LIGHTPADPAGE_H
#define LIGHTPADPAGE_H

#include "../filetree/filedirtreecontroller.h"
#include "../filetree/filedirtreemodel.h"
#include "../filetree/gitfilesystemmodel.h"
#include "../filetree/lightpadtreeview.h"
#include "../filetree/projectsearch.h"
#include "textarea.h"

#include <QFileSystemModel>
#include <QTreeView>
#include <QWidget>

class LightpadPage;
class Minimap;
class GitIntegration;
class QLabel;
class QToolButton;
class ExplorerSearchWidget;
class QSplitter;
struct Theme;
class ThemeDefinition;

class LightpadPage : public QWidget {

  Q_OBJECT

public:
  LightpadPage(QWidget *parent = nullptr, bool treeViewHidden = true);
  ~LightpadPage(){};
  QTreeView *getTreeView();
  TextArea *getTextArea();
  Minimap *getMinimap();
  void setTreeViewVisible(bool flag);
  void refreshTreeVisibility();
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
  void setTreeSearchOptions(const ProjectSearch::Options &options);
  void setExplorerWidth(int width);
  void activateTreeIndex(const QModelIndex &index);

  void revealPath(const QString &path);
  void applyTheme(const Theme &theme);
  void applyTheme(const ThemeDefinition &theme);
  MainWindow *getMainWindow() const;

private:
  MainWindow *mainWindow;
  QWidget *treeContainer;
  QWidget *treeHeader;
  QLabel *treeTitleLabel;
  QLabel *treePathLabel;

  void updateTreePathLabel();
  ExplorerSearchWidget *treeSearch;
  QToolButton *treeSearchButton;
  QToolButton *treeNewFileButton;
  QToolButton *treeNewFolderButton;
  QToolButton *treeMoreButton;
  QTreeView *treeView;
  QWidget *editorContainer;
  QSplitter *m_explorerSplitter = nullptr;
  TextArea *textArea;
  Minimap *minimap;
  GitFileSystemModel *model;
  bool m_ownsModel;
  GitIntegration *m_gitIntegration;
  QString m_treeFilterText;
  bool m_treeWanted = false;
  QString filePath;
  QString projectRootPath;
};

#endif
