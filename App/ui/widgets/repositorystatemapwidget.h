#ifndef REPOSITORYSTATEMAPWIDGET_H
#define REPOSITORYSTATEMAPWIDGET_H

#include "../../git/gitrepositorystate.h"
#include "../../settings/theme.h"
#include <QWidget>

class QLabel;
class QToolButton;
class FlowLayout;

class RepositoryStateMapWidget : public QWidget {
  Q_OBJECT

public:
  enum class Layer {
    WorkingTree,
    Index,
    Head,
    Branch,
    Upstream,
    Stash,
    Operation,
  };
  Q_ENUM(Layer)

  enum class CollapseMode { Auto, Expanded, Collapsed };
  Q_ENUM(CollapseMode)

  explicit RepositoryStateMapWidget(QWidget *parent = nullptr);

  void setState(const GitRepositoryState &state);
  const GitRepositoryState &state() const { return m_state; }

  void setCollapseMode(CollapseMode mode);
  CollapseMode collapseMode() const { return m_collapseMode; }
  bool isCollapsed() const { return m_collapsed; }

  void applyTheme(const Theme &theme);

signals:

  void layerActivated(RepositoryStateMapWidget::Layer layer);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QToolButton *addChip(Layer layer, const QString &objectName,
                       bool leadingArrow);
  void rebuildChips();
  void updateCollapsedState();
  void styleChip(QToolButton *chip, const QColor &accent) const;

  GitRepositoryState m_state;
  Theme m_theme;
  bool m_themeInitialized;

  CollapseMode m_collapseMode;
  bool m_collapsed;

  QWidget *m_expandedWidget;
  QToolButton *m_operationChip;
  QWidget *m_chipsWidget;
  FlowLayout *m_chipsLayout;
  QLabel *m_summaryLabel;
  QToolButton *m_collapsedButton;
};

#endif
