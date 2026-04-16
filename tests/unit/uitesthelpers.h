#ifndef UITESTHELPERS_H
#define UITESTHELPERS_H

#include <QApplication>
#include <QPixmap>
#include <QTest>
#include <QWidget>

namespace UiTestHelpers {

inline void showWidget(QWidget &widget, const QSize &size = QSize(960, 640)) {
  widget.resize(size);
  widget.show();
  QApplication::processEvents();
  QTest::qWait(20);
}

inline QPixmap captureSnapshot(QWidget &widget,
                               const QSize &size = QSize(960, 640)) {
  showWidget(widget, size);
  return widget.grab();
}

} // namespace UiTestHelpers

#endif
