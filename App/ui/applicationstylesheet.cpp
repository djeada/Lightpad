#include "applicationstylesheet.h"

#include <QApplication>
#include <QEvent>
#include <QVariant>
#include <QWidget>

namespace {
const char *const kGenerationProperty = "lightpadStyleSheetGeneration";
}

ApplicationStyleSheet &ApplicationStyleSheet::instance() {
  static ApplicationStyleSheet *sheet = new ApplicationStyleSheet();
  return *sheet;
}

ApplicationStyleSheet::ApplicationStyleSheet() : QObject(qApp) {
  if (qApp) {
    qApp->installEventFilter(this);
  }
}

void ApplicationStyleSheet::apply(const QString &sheet, QWidget *window) {
  if (sheet != m_sheet) {
    m_sheet = sheet;
    ++m_generation;
  }
  if (window) {
    adopt(window->window());
  }
  for (QWidget *topLevel : QApplication::topLevelWidgets()) {
    if (topLevel->property(kGenerationProperty).isValid()) {
      adopt(topLevel);
    }
  }
}

bool ApplicationStyleSheet::isApplied(const QWidget *window) const {
  return window &&
         window->property(kGenerationProperty).toInt() == m_generation &&
         window->styleSheet() == m_sheet;
}

void ApplicationStyleSheet::adopt(QWidget *window) {
  if (!window || m_sheet.isEmpty()) {
    return;
  }
  if (window->property(kGenerationProperty).toInt() == m_generation &&
      window->styleSheet() == m_sheet) {
    return;
  }
  window->setProperty(kGenerationProperty, m_generation);
  window->setStyleSheet(m_sheet);
}

bool ApplicationStyleSheet::eventFilter(QObject *watched, QEvent *event) {
  // Runs for every event in the application: keep the common path cheap.
  const QEvent::Type type = event->type();
  if ((type != QEvent::Polish && type != QEvent::Show) ||
      !watched->isWidgetType() || m_sheet.isEmpty()) {
    return false;
  }
  auto *widget = static_cast<QWidget *>(watched);
  if (!widget->isWindow() || widget->parentWidget()) {
    return false;
  }
  const QVariant generation = widget->property(kGenerationProperty);
  if (generation.isValid()) {
    if (generation.toInt() != m_generation) {
      adopt(widget);
    }
  } else if (widget->styleSheet().isEmpty()) {
    // A parentless window with no style of its own (a tooltip, a second
    // main window) looks like the rest of the IDE.
    adopt(widget);
  }
  return false;
}
