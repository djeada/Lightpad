#ifndef EXPLORERSEARCHWIDGET_H
#define EXPLORERSEARCHWIDGET_H

#include "projectsearch.h"
#include <QWidget>
#include <atomic>
#include <memory>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class ThemeDefinition;

class ExplorerSearchWidget : public QWidget {
  Q_OBJECT

public:
  explicit ExplorerSearchWidget(QWidget *parent = nullptr);
  ~ExplorerSearchWidget() override;

  ProjectSearch::Options options() const;
  void setOptions(const ProjectSearch::Options &options);
  void setRootPath(const QString &path);
  void setSearchText(const QString &text);
  void searchInFolder(const QString &path);
  void focusSearch(ProjectSearch::Mode mode);
  void refresh();
  void applyTheme(const ThemeDefinition &theme);

signals:
  void optionsChanged();
  void searchActiveChanged(bool active);
  void navigateRequested(const QString &path, int line, int column, int length);
  void revealRequested(const QString &path);
  void entryContextMenuRequested(const QString &path, const QPoint &globalPos);

protected:
  bool eventFilter(QObject *object, QEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;

private:
  void changed();
  void updateControls();
  void scheduleSearch();
  void startSearch();
  void cancelSearch();
  void displayResults(const ProjectSearch::Results &results);
  void activateItem(QTreeWidgetItem *item);
  const ProjectSearch::Match *matchForItem(QTreeWidgetItem *item) const;

  ProjectSearch::Options m_options;
  ProjectSearch::Results m_results;
  std::shared_ptr<std::atomic_bool> m_cancelled;
  int m_request = 0;
  bool m_settingOptions = false;
  bool m_dirty = true;
  QTimer *m_timer;
  QLineEdit *m_query;
  QLineEdit *m_includes;
  QLineEdit *m_excludes;
  QToolButton *m_namesButton;
  QToolButton *m_contentsButton;
  QToolButton *m_caseButton;
  QToolButton *m_wordButton;
  QToolButton *m_regexButton;
  QToolButton *m_filtersButton;
  QToolButton *m_scopeButton;
  QToolButton *m_stopButton;
  QCheckBox *m_hidden;
  QWidget *m_filters;
  QWidget *m_feedback;
  QLabel *m_status;
  QTreeWidget *m_tree;
};

#endif
