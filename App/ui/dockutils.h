#ifndef DOCKUTILS_H
#define DOCKUTILS_H

#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QToolButton>
#include <QtGlobal>

namespace DockUtils {

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

  auto *titleBar = new QWidget(dock);
  titleBar->setObjectName("toolPanelTitleBar");
  auto *layout = new QHBoxLayout(titleBar);
  layout->setContentsMargins(8, 0, 4, 0);
  layout->setSpacing(2);
  auto *title = new QLabel(dock->windowTitle(), titleBar);
  title->setTextFormat(Qt::PlainText);
  layout->addWidget(title, 1);
  QObject::connect(dock, &QDockWidget::windowTitleChanged, title,
                   &QLabel::setText);
  auto *expandButton = new QToolButton(titleBar);
  expandButton->setObjectName("toolPanelExpand");
  expandButton->setText(QObject::tr("Expand"));
  expandButton->setToolTip(
      QObject::tr("Expand this panel in a full-size window"));
  expandButton->setAccessibleName(QObject::tr("Expand panel"));
  expandButton->setAutoRaise(true);
  expandButton->setStyleSheet("QToolButton { min-width: 72px; max-width: "
                              "16777215px; padding: 0 8px; font-size: 12px; }");
  layout->addWidget(expandButton);
  QObject::connect(expandButton, &QToolButton::clicked, dock, [dock]() {
    if (dock->isFloating()) {
      dock->setFloating(false);
    } else {
      dock->setFloating(true);
      dock->showMaximized();
    }
  });
  QObject::connect(
      dock, &QDockWidget::topLevelChanged, expandButton,
      [expandButton](bool floating) {
        expandButton->setText(floating ? QObject::tr("Restore")
                                       : QObject::tr("Expand"));
        const auto description =
            floating ? QObject::tr("Return panel to the workspace")
                     : QObject::tr("Expand this panel in a full-size window");
        expandButton->setToolTip(description);
        expandButton->setAccessibleName(description);
      });
  auto *floatButton = new QToolButton(titleBar);
  floatButton->setObjectName("toolPanelFloat");
  floatButton->setText(QStringLiteral("\u2197"));
  floatButton->setToolTip(QObject::tr("Float panel"));
  floatButton->setAccessibleName(QObject::tr("Float panel"));
  floatButton->setAutoRaise(true);
  layout->addWidget(floatButton);
  QObject::connect(floatButton, &QToolButton::clicked, dock,
                   [dock]() { dock->setFloating(!dock->isFloating()); });
  QObject::connect(dock, &QDockWidget::topLevelChanged, floatButton,
                   [floatButton](bool floating) {
                     const auto label = floating ? QObject::tr("Dock panel")
                                                 : QObject::tr("Float panel");
                     floatButton->setText(floating ? QStringLiteral("\u2199")
                                                   : QStringLiteral("\u2197"));
                     floatButton->setToolTip(label);
                     floatButton->setAccessibleName(label);
                   });
  auto *closeButton = new QToolButton(titleBar);
  closeButton->setObjectName("toolPanelClose");
  closeButton->setText(QStringLiteral("\u00d7"));
  closeButton->setToolTip(QObject::tr("Close panel"));
  closeButton->setAccessibleName(QObject::tr("Close panel"));
  closeButton->setAutoRaise(true);
  layout->addWidget(closeButton);
  QObject::connect(closeButton, &QToolButton::clicked, dock,
                   &QDockWidget::close);
  QObject::connect(
      dock, &QDockWidget::featuresChanged, titleBar,
      [floatButton, closeButton](QDockWidget::DockWidgetFeatures features) {
        floatButton->setVisible(
            features.testFlag(QDockWidget::DockWidgetFloatable));
        closeButton->setVisible(
            features.testFlag(QDockWidget::DockWidgetClosable));
      });

  dock->setTitleBarWidget(titleBar);

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
