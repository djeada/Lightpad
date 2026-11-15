#ifndef TABNAVIGATION_H
#define TABNAVIGATION_H

#include <QAction>
#include <QKeySequence>
#include <QTabBar>
#include <QTabWidget>

namespace TabNavigation {
// Skip hidden/disabled utility tabs, including the editor's trailing sentinel.
inline void step(QTabWidget *tabs, int direction, bool reorder) {
  const int current = tabs->currentIndex();
  if (current < 0)
    return;
  int target = current + direction;
  while (target >= 0 && target < tabs->count() &&
         (!tabs->isTabEnabled(target) || !tabs->tabBar()->isTabVisible(target)))
    target += direction;
  if (target < 0 || target >= tabs->count())
    return;
  if (reorder)
    tabs->tabBar()->moveTab(current, target);
  else
    tabs->setCurrentIndex(target);
}

inline void install(QTabWidget *tabs, QWidget *shortcutOwner = nullptr) {
  QWidget *owner = shortcutOwner ? shortcutOwner : tabs;
  for (bool reorder : {false, true}) {
    for (int direction : {-1, 1}) {
      const QString label =
          reorder ? (direction < 0 ? QObject::tr("Move Tab Left")
                                   : QObject::tr("Move Tab Right"))
                  : (direction < 0 ? QObject::tr("Previous Tab")
                                   : QObject::tr("Next Tab"));
      auto *action = new QAction(label, owner);
      action->setShortcut(
          QKeySequence((reorder ? Qt::CTRL | Qt::SHIFT : Qt::CTRL) |
                       (direction < 0 ? Qt::Key_PageUp : Qt::Key_PageDown)));
      action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
      owner->addAction(action);
      QObject::connect(action, &QAction::triggered, tabs,
                       [=] { step(tabs, direction, reorder); });
    }
  }
}
} // namespace TabNavigation
#endif
