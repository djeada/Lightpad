#ifndef PANELTYPOGRAPHY_H
#define PANELTYPOGRAPHY_H

#include <QAbstractButton>
#include <QComboBox>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSplitter>
#include <QTableView>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWidget>

// Inspection content follows the editor's point size, including widgets with
// their own stylesheets. A setFont() alone loses to the application's pixel
// QSS.
namespace PanelTypography {
inline QFont contentFont(const QWidget *widget) {
  for (const QWidget *parent = widget; parent;
       parent = parent->parentWidget()) {
    const auto value = parent->property("lightpadPanelFont");
    if (value.isValid())
      return value.value<QFont>();
  }
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSizeF(qMax(12.0, font.pointSizeF()));
  return font;
}

inline QFont normalizedFont(QFont font, const QWidget *widget) {
  if (font.pointSizeF() <= 0 && font.pixelSize() > 0)
    font.setPointSizeF(font.pixelSize() * 72.0 / widget->logicalDpiY());
  if (font.pointSizeF() <= 0)
    font.setPointSizeF(12);
  return font;
}

inline QString styleForFont(QString style, const QFont &font) {
  const QString marker = QStringLiteral("/* lightpad inspection typography */");
  const auto start = style.indexOf(marker);
  if (start >= 0)
    style.truncate(start);
  const QString points = QString::number(font.pointSizeF(), 'f', 2);
  static const QRegularExpression fixedSize(
      R"(font-size\s*:\s*[\d.]+\s*(?:px|pt))");
  style.replace(fixedSize, "font-size: " + points + "pt");
  const int rowHeight = QFontMetrics(font).height() + 10;
  return style + marker +
         QString("QWidget { font-size: %1pt; }"
                 "QHeaderView::section, QTabBar::tab { font-size: %1pt; }"
                 "QTreeView::item { min-height: %2px; }"
                 "QWidget#toolPanelTitleBar QLabel, QWidget#toolPanelTitleBar "
                 "QToolButton { font-size: %1pt; }"
                 "QWidget#toolPanelTitleBar QToolButton { min-width: %2px; "
                 "max-width: %2px; min-height: %2px; max-height: %2px; }"
                 "QWidget#toolPanelTitleBar QToolButton#toolPanelExpand { "
                 "min-width: %3px; max-width: 16777000px; padding: 0 8px; }")
             .arg(points)
             .arg(rowHeight)
             .arg(QFontMetrics(font).horizontalAdvance(
                      QStringLiteral("Restore")) +
                  20);
}

inline void apply(QWidget *root, const QFont &requestedFont) {
  if (!root)
    return;
  const QFont codeFont = normalizedFont(requestedFont, root);
  QFont uiFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  uiFont.setPointSizeF(codeFont.pointSizeF());
  root->setProperty("lightpadPanelFont", codeFont);
  root->setFont(uiFont);
  root->setStyleSheet(styleForFont(root->styleSheet(), codeFont));

  const auto widgets = root->findChildren<QWidget *>();
  for (QWidget *widget : widgets) {
    if (widget->property("lightpadPanelFont").isValid())
      widget->setProperty("lightpadPanelFont", codeFont);
    // Local styles otherwise override inherited typography in inspectors,
    // results, combo popups and detached windows.
    if (!widget->styleSheet().isEmpty())
      widget->setStyleSheet(styleForFont(widget->styleSheet(), codeFont));
    const bool code = qobject_cast<QAbstractItemView *>(widget) ||
                      qobject_cast<QPlainTextEdit *>(widget) ||
                      qobject_cast<QTextEdit *>(widget) ||
                      widget->objectName() == "debugConsoleInput" ||
                      widget->objectName() == "debugWatchInput";
    widget->setFont(code ? codeFont : uiFont);
    widget->ensurePolished();
    const QFontMetrics metrics(widget->font());
    const int rowHeight = metrics.height() + 10;
    const int controlHeight = metrics.height() + 12;
    if (qobject_cast<QLineEdit *>(widget) ||
        qobject_cast<QComboBox *>(widget) ||
        qobject_cast<QAbstractButton *>(widget)) {
      if (widget->maximumHeight() < controlHeight)
        widget->setMaximumHeight(QWIDGETSIZE_MAX);
      widget->setMinimumHeight(controlHeight);
    }
    if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
      const int icon = qMax(18, metrics.height() - 4);
      button->setIconSize(QSize(icon, icon));
    }
    if (auto *toolbar = qobject_cast<QToolBar *>(widget)) {
      const int icon = qMax(18, metrics.height() - 4);
      toolbar->setIconSize(QSize(icon, icon));
    }
    if (auto *table = qobject_cast<QTableView *>(widget)) {
      table->verticalHeader()->setMinimumSectionSize(rowHeight);
      table->verticalHeader()->setDefaultSectionSize(rowHeight);
      table->horizontalHeader()->setMinimumHeight(controlHeight);
    }
    if (auto *tree = qobject_cast<QTreeWidget *>(widget)) {
      tree->setIconSize(QSize(qMax(18, metrics.height() - 4),
                              qMax(18, metrics.height() - 4)));
      tree->setIndentation(qMax(20, metrics.height()));
      tree->header()->setFixedHeight(controlHeight);
      for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        for (int column = 0; column < tree->columnCount(); ++column) {
          const auto explicitFont = (*it)->data(column, Qt::FontRole);
          if (explicitFont.isValid()) {
            QFont itemFont = explicitFont.value<QFont>();
            itemFont.setPointSizeF(codeFont.pointSizeF());
            (*it)->setFont(column, itemFont);
          }
        }
      }
      tree->doItemsLayout();
    }
    if (auto *splitter = qobject_cast<QSplitter *>(widget))
      splitter->setHandleWidth(qMax(6, metrics.height() / 4));
    widget->updateGeometry();
  }
  root->updateGeometry();
}
} // namespace PanelTypography

#endif
