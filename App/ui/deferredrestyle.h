#ifndef DEFERREDRESTYLE_H
#define DEFERREDRESTYLE_H

#include <QEvent>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>

// Runs styling work for a widget when it can actually be seen.
//
// Applying a theme or font to a panel nobody is looking at is wasted time:
// Qt polishes every widget a style sheet reaches, a hidden dock (Debug,
// Database, Tests) holds hundreds of widgets, and once polished they are
// re-polished on every later application style change as well. Updates for
// a hidden widget are queued instead, one per key with the latest winning,
// and run in order just before the widget is first shown again.
class DeferredRestyle : public QObject {
public:
  // Runs `update` now when `target` is visible, otherwise on its next show.
  static void apply(QWidget *target, const QString &key,
                    std::function<void()> update) {
    if (!target) {
      return;
    }
    if (target->isVisible()) {
      // Anything older that is still queued must not run after this.
      if (DeferredRestyle *queue = existing(target)) {
        queue->drop(key);
      }
      update();
      return;
    }
    DeferredRestyle *queue = existing(target);
    if (!queue) {
      queue = new DeferredRestyle(target);
    }
    queue->drop(key);
    queue->m_pending.append({key, std::move(update)});
  }

  // Updates still waiting for `target` to be shown.
  static int pendingCount(const QWidget *target) {
    const DeferredRestyle *queue = existing(target);
    return queue ? int(queue->m_pending.size()) : 0;
  }

  // Runs whatever is queued for `target` now.
  static void flush(QWidget *target) {
    if (DeferredRestyle *queue = existing(target)) {
      queue->runPending();
    }
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == m_target && event->type() == QEvent::Show) {
      runPending();
    }
    return QObject::eventFilter(watched, event);
  }

private:
  explicit DeferredRestyle(QWidget *target)
      : QObject(target), m_target(target) {
    setObjectName(QStringLiteral("lightpadDeferredRestyle"));
    target->installEventFilter(this);
  }

  static DeferredRestyle *existing(const QWidget *target) {
    if (!target) {
      return nullptr;
    }
    for (QObject *child : target->children()) {
      if (child->objectName() == QLatin1String("lightpadDeferredRestyle")) {
        return static_cast<DeferredRestyle *>(child);
      }
    }
    return nullptr;
  }

  void drop(const QString &key) {
    for (int i = m_pending.size() - 1; i >= 0; --i) {
      if (m_pending.at(i).first == key) {
        m_pending.removeAt(i);
      }
    }
  }

  void runPending() {
    // Updates may queue new work (or show other widgets); take the list
    // first so each runs once.
    const auto pending = std::move(m_pending);
    m_pending.clear();
    for (const auto &entry : pending) {
      entry.second();
    }
  }

  QPointer<QWidget> m_target;
  QList<QPair<QString, std::function<void()>>> m_pending;
};

#endif
