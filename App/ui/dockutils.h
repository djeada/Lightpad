#ifndef DOCKUTILS_H
#define DOCKUTILS_H

#include <QDockWidget>
#include <QMainWindow>

namespace DockUtils {

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
}

} // namespace DockUtils

#endif
