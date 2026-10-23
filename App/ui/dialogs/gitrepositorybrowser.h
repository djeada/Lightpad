#ifndef GITREPOSITORYBROWSER_H
#define GITREPOSITORYBROWSER_H

#include "../../settings/theme.h"
#include <QWidget>

class GitIntegration;
class GitObjectExplorerDialog;
class GitLogDialog;
class QSplitter;
class QButtonGroup;

// A repository-pinned, modeless workspace. Inspection never opens another
// blocking history/object dialog or follows the editor into a different repo.
class GitRepositoryBrowser : public QWidget {
  Q_OBJECT
public:
  explicit GitRepositoryBrowser(const QString &repository, const Theme &theme,
                                QWidget *parent = nullptr);
  QString repositoryPath() const;
  void showObject(const QString &hash = {});
  void showHistory(const QString &hash = {});

signals:
  void repositoryChanged();
  void fileOpenRequested(const QString &path);

private:
  void setMode(int mode);
  void openDiff(const QString &hash, const QString &filePath);
  Theme m_theme;
  GitIntegration *m_git;
  GitObjectExplorerDialog *m_objects;
  GitLogDialog *m_history;
  QSplitter *m_splitter;
  QButtonGroup *m_modes;
};

#endif
