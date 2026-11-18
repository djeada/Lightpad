#ifndef APPLICATIONSTYLESHEET_H
#define APPLICATIONSTYLESHEET_H

#include <QObject>
#include <QString>

class QWidget;

// The IDE's theme style sheet, applied per window instead of with
// QApplication::setStyleSheet.
//
// Changing the application style sheet makes Qt re-polish every styled
// widget once for itself and again for each styled ancestor, so a theme
// switch cost several hundred milliseconds even with most panels hidden.
// Setting the same sheet on each top-level window re-polishes each widget
// once. Main windows and parentless windows (tooltips, a second main
// window) take the current sheet when they are polished or shown; widgets
// parented to a main window inherit it as before, with the same precedence
// below their own and their panels' style sheets.
class ApplicationStyleSheet : public QObject {
public:
  static ApplicationStyleSheet &instance();

  // Makes `sheet` current and applies it to `window` and every window that
  // already uses it.
  void apply(const QString &sheet, QWidget *window);
  const QString &current() const { return m_sheet; }
  // True when `window` shows the current sheet.
  bool isApplied(const QWidget *window) const;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  ApplicationStyleSheet();
  void adopt(QWidget *window);

  QString m_sheet;
  int m_generation = 0;
};

#endif
