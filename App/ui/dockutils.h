#ifndef DOCKUTILS_H
#define DOCKUTILS_H

#include <QDockWidget>
#include <QMainWindow>
#include <QtGlobal>

namespace DockUtils {

// Give a newly opened bottom panel enough room for controls without taking
// over the editor. Later resizes and restored layouts are user-owned.
inline int preferredInitialBottomDockHeight(int windowHeight) {
  return qBound(160, windowHeight / 5, 240);
}

inline Qt::DockWidgetAreas toolPanelAllowedAreas() {
  return Qt::AllDockWidgetAreas;
}

inline QDockWidget::DockWidgetFeatures toolPanelFeatures() {
  return QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
         QDockWidget::DockWidgetFloatable;
}

inline QMainWindow::DockOptions mainWindowDockOptions() {
  return QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks |
         QMainWindow::AnimatedDocks | QMainWindow::GroupedDragging;
}

inline void configureToolPanelDock(QDockWidget *dock) {
  if (!dock) {
    return;
  }

  dock->setAllowedAreas(toolPanelAllowedAreas());
  dock->setFeatures(toolPanelFeatures());

  QObject::connect(
      dock, &QDockWidget::topLevelChanged, dock, [dock](bool topLevel) {
        if (topLevel) {
          dock->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint |
                               Qt::WindowMinimizeButtonHint |
                               Qt::WindowMaximizeButtonHint |
                               Qt::WindowCloseButtonHint);
          dock->show();
        }
      });
}

} // namespace DockUtils

#endif
