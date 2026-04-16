#include "uistylehelper.h"

QString UIStyleHelper::popupDialogStyle(const Theme &theme) {
  return QString("background: %1; "
                 "border: 1px solid %2; "
                 "border-radius: 8px;")
      .arg(theme.surfaceColor.name())
      .arg(theme.borderColor.name());
}

QString UIStyleHelper::searchBoxStyle(const Theme &theme) {
  return QString("QLineEdit {"
                 "  padding: 8px;"
                 "  font-size: 14px;"
                 "  border: 1px solid %1;"
                 "  border-radius: 4px;"
                 "  background: %2;"
                 "  color: %3;"
                 "  selection-background-color: %5;"
                 "  selection-color: %4;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.borderColor.name())
      .arg(theme.surfaceColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.accentSoftColor.name());
}

QString UIStyleHelper::resultListStyle(const Theme &theme) {
  return QString("QListWidget {"
                 "  border: none;"
                 "  background: %1;"
                 "  color: %2;"
                 "}"
                 "QListWidget::item {"
                 "  padding: 8px;"
                 "  border-bottom: 1px solid %3;"
                 "}"
                 "QListWidget::item:selected {"
                 "  background: %4;"
                 "  color: %5;"
                 "}"
                 "QListWidget::item:hover {"
                 "  background: %4;"
                 "}")
      .arg(theme.backgroundColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::panelHeaderStyle(const Theme &theme) {
  return QString("background: %1; "
                 "border-bottom: 1px solid %2;")
      .arg(theme.surfaceColor.name())
      .arg(theme.borderColor.name());
}

QString UIStyleHelper::treeViewStyle(const Theme &theme) {
  return QString("QTreeView {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  outline: none;"
                 "  font-size: 12px;"
                 "  padding: 6px 4px;"
                 "}"
                 "QTreeView::item {"
                 "  padding: 6px 8px;"
                 "  border-radius: 8px;"
                 "  margin: 1px 2px;"
                 "}"
                 "QTreeView::item:focus {"
                 "  outline: none;"
                 "  border: none;"
                 "}"
                 "QTreeView::item:selected {"
                 "  background: transparent;"
                 "  color: %1;"
                 "}"
                 "QTreeView::item:hover:!selected {"
                 "  background: transparent;"
                 "}"
                 "QTreeView::branch {"
                 "  background: transparent;"
                 "  border: none;"
                 "}"
                 "QTreeView::branch:has-children:closed,"
                 "QTreeView::branch:closed:has-children:has-siblings {"
                 "  image: url(:/resources/icons/branch_closed.png);"
                 "}"
                 "QTreeView::branch:has-children:open,"
                 "QTreeView::branch:open:has-children:has-siblings {"
                 "  image: url(:/resources/icons/branch_open.png);"
                 "}"
                 "QTreeView::branch:selected {"
                 "  background: transparent;"
                 "  border: none;"
                 "}"
                 "QTreeView::branch:hover {"
                 "  background: transparent;"
                 "}"
                 "QHeaderView::section {"
                 "  background: %2;"
                 "  color: %3;"
                 "  border: none;"
                 "  border-bottom: 1px solid %4;"
                 "  padding: 6px 12px;"
                 "  font-size: 11px;"
                 "  font-weight: bold;"
                 "  text-transform: uppercase;"
                 "}"
                 "QScrollBar:vertical {"
                 "  background: transparent;"
                 "  width: 6px;"
                 "  margin: 0;"
                 "}"
                 "QScrollBar::handle:vertical {"
                 "  background: %4;"
                 "  min-height: 20px;"
                 "  border-radius: 4px;"
                 "}"
                 "QScrollBar::handle:vertical:hover {"
                 "  background: %3;"
                 "}"
                 "QScrollBar::add-line:vertical,"
                 "QScrollBar::sub-line:vertical {"
                 "  height: 0;"
                 "}"
                 "QScrollBar:horizontal {"
                 "  background: transparent;"
                 "  height: 6px;"
                 "  margin: 0;"
                 "}"
                 "QScrollBar::handle:horizontal {"
                 "  background: %4;"
                 "  min-width: 20px;"
                 "  border-radius: 4px;"
                 "}"
                 "QScrollBar::handle:horizontal:hover {"
                 "  background: %3;"
                 "}"
                 "QScrollBar::add-line:horizontal,"
                 "QScrollBar::sub-line:horizontal {"
                 "  width: 0;"
                 "}")
      .arg(theme.foregroundColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.borderColor.name());
}

QString UIStyleHelper::contextMenuStyle(const Theme &theme) {
  return QString("QMenu {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 6px;"
                 "  padding: 4px;"
                 "}"
                 "QMenu::item {"
                 "  padding: 6px 24px 6px 8px;"
                 "  border-radius: 4px;"
                 "  margin: 1px 4px;"
                 "}"
                 "QMenu::item:selected {"
                 "  background: %4;"
                 "  color: %5;"
                 "}"
                 "QMenu::separator {"
                 "  height: 1px;"
                 "  background: %3;"
                 "  margin: 4px 8px;"
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::treeWidgetStyle(const Theme &theme) {
  return QString("QTreeWidget {"
                 "  background: %1;"
                 "  alternate-background-color: %8;"
                 "  color: %2;"
                 "  border: none;"
                 "  selection-background-color: %3;"
                 "  selection-color: %9;"
                 "}"
                 "QTreeWidget::item {"
                 "  padding: 4px;"
                 "}"
                 "QTreeWidget::item:selected {"
                 "  background: %3;"
                 "  color: %9;"
                 "}"
                 "QTreeWidget::item:!active:selected {"
                 "  background: %3;"
                 "  color: %9;"
                 "}"
                 "QTreeWidget::item:hover {"
                 "  background: %4;"
                 "}"
                 "QHeaderView::section {"
                 "  background: %5;"
                 "  color: %6;"
                 "  padding: 4px;"
                 "  border: none;"
                 "  border-right: 1px solid %7;"
                 "}")
      .arg(theme.backgroundColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.surfaceColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::subduedLabelStyle(const Theme &theme) {
  return QString("color: %1;").arg(theme.singleLineCommentFormat.name());
}

QString UIStyleHelper::titleLabelStyle(const Theme &theme) {
  return QString("font-weight: bold; color: %1;")
      .arg(theme.foregroundColor.name());
}

QString UIStyleHelper::comboBoxStyle(const Theme &theme) {
  return QString("QComboBox {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  padding: 2px 8px;"
                 "  border-radius: 4px;"
                 "}"
                 "QComboBox::drop-down {"
                 "  border: none;"
                 "}"
                 "QComboBox QAbstractItemView {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  selection-background-color: %4;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name());
}

QString UIStyleHelper::checkBoxStyle(const Theme &theme) {
  return QString("QCheckBox {"
                 "  color: %1;"
                 "  spacing: 8px;"
                 "}"
                 "QCheckBox::indicator {"
                 "  width: 16px;"
                 "  height: 16px;"
                 "  border-radius: 4px;"
                 "  border: 1px solid %2;"
                 "  background: %3;"
                 "}"
                 "QCheckBox::indicator:checked {"
                 "  background: %4;"
                 "  border-color: %4;"
                 "}"
                 "QCheckBox::indicator:hover {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::formDialogStyle(const Theme &theme) {
  return QString("QDialog {"
                 "  background: %1;"
                 "}")
      .arg(theme.backgroundColor.name());
}

QString UIStyleHelper::groupBoxStyle(const Theme &theme) {
  return QString("QGroupBox {"
                 "  background: %1;"
                 "  border: 1px solid %2;"
                 "  border-radius: 6px;"
                 "  margin-top: 12px;"
                 "  padding: 12px;"
                 "  padding-top: 24px;"
                 "  font-weight: bold;"
                 "  color: %3;"
                 "}"
                 "QGroupBox::title {"
                 "  subcontrol-origin: margin;"
                 "  subcontrol-position: top left;"
                 "  left: 12px;"
                 "  padding: 0 6px;"
                 "  color: %4;"
                 "  font-size: 11px;"
                 "  text-transform: uppercase;"
                 "  letter-spacing: 1px;"
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::lineEditStyle(const Theme &theme) {
  return QString("QLineEdit {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 6px;"
                 "  padding: 8px 12px;"
                 "  font-size: 12px;"
                 "  selection-background-color: %5;"
                 "  selection-color: %4;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.accentSoftColor.name());
}

QString UIStyleHelper::primaryButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: %1;"
                 "  border: 1px solid %1;"
                 "  color: %2;"
                 "  border-radius: 6px;"
                 "  padding: 8px 16px;"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %3;"
                 "  border-color: %3;"
                 "}")
      .arg(theme.accentColor.name())
      .arg(theme.backgroundColor.name())
      .arg(theme.accentColor.lighter(120).name());
}

QString UIStyleHelper::secondaryButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 6px;"
                 "  padding: 8px 16px;"
                 "  font-size: 12px;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %4;"
                 "  border-color: %5;"
                 "  color: %5;"
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::breadcrumbButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  padding: 2px 8px;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "}"
                 "QPushButton:hover {"
                 "  color: %2;"
                 "  background: %3;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.singleLineCommentFormat.name())
      .arg(theme.accentColor.name())
      .arg(theme.accentSoftColor.name());
}

QString UIStyleHelper::breadcrumbActiveButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  padding: 2px 8px;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  color: %2;"
                 "  background: %3;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.accentColor.name())
      .arg(theme.accentColor.lighter(115).name())
      .arg(theme.accentSoftColor.name());
}

QString UIStyleHelper::breadcrumbSeparatorStyle(const Theme &theme) {
  return QString("QLabel {"
                 "  color: %1;"
                 "  font-family: 'Ubuntu Mono', 'JetBrains Mono', 'Monospace';"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "  padding: 0 2px;"
                 "}")
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::infoLabelStyle(const Theme &theme) {
  return QString("color: %1; font-size: 11px;")
      .arg(theme.singleLineCommentFormat.name());
}

QString UIStyleHelper::successInfoLabelStyle(const Theme &theme) {
  return QString("color: %1; font-size: 11px;").arg(theme.successColor.name());
}

QString UIStyleHelper::errorInfoLabelStyle(const Theme &theme) {
  return QString("color: %1; font-size: 11px;").arg(theme.errorColor.name());
}

QString UIStyleHelper::tabWidgetStyle(const Theme &theme) {
  return QString("QTabWidget::pane {"
                 "  border: 1px solid %1;"
                 "  background: %2;"
                 "  border-radius: 4px;"
                 "}"
                 "QTabBar::tab {"
                 "  background: %3;"
                 "  color: %4;"
                 "  padding: 8px 20px;"
                 "  border: none;"
                 "  border-bottom: 2px solid transparent;"
                 "}"
                 "QTabBar::tab:selected {"
                 "  background: %2;"
                 "  color: %5;"
                 "  border-bottom: 2px solid %6;"
                 "}"
                 "QTabBar::tab:hover:!selected {"
                 "  background: %7;"
                 "  color: %5;"
                 "}")
      .arg(theme.borderColor.name())
      .arg(theme.surfaceColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.singleLineCommentFormat.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.hoverColor.name());
}

QString UIStyleHelper::tableWidgetStyle(const Theme &theme) {
  return QString("QTableWidget {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 4px;"
                 "  gridline-color: %3;"
                 "}"
                 "QTableWidget::item {"
                 "  padding: 4px;"
                 "}"
                 "QTableWidget::item:selected {"
                 "  background: %4;"
                 "  color: %7;"
                 "}"
                 "QHeaderView::section {"
                 "  background: %5;"
                 "  color: %6;"
                 "  border: none;"
                 "  border-bottom: 1px solid %3;"
                 "  padding: 4px 8px;"
                 "  font-size: 11px;"
                 "  text-transform: uppercase;"
                 "  letter-spacing: 1px;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.surfaceColor.name())
      .arg(theme.accentColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::plainTextEditStyle(const Theme &theme) {
  return QString("QPlainTextEdit, QTextEdit {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 6px;"
                 "  padding: 8px;"
                 "  font-family: monospace;"
                 "  font-size: 12px;"
                 "}"
                 "QPlainTextEdit:focus, QTextEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::spinBoxStyle(const Theme &theme) {
  return QString("QSpinBox {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 4px;"
                 "  padding: 4px 8px;"
                 "}"
                 "QSpinBox:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::dangerButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: %1;"
                 "  color: white;"
                 "  border: 1px solid %1;"
                 "  border-radius: 6px;"
                 "  padding: 8px 16px;"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %2;"
                 "  border-color: %2;"
                 "}")
      .arg(theme.errorColor.name())
      .arg(theme.errorColor.lighter(110).name());
}

QString UIStyleHelper::progressBarStyle(const Theme &theme) {
  return QString("QProgressBar {"
                 "  background: %1;"
                 "  border: 1px solid %2;"
                 "  border-radius: 4px;"
                 "  text-align: center;"
                 "  color: %3;"
                 "  height: 8px;"
                 "}"
                 "QProgressBar::chunk {"
                 "  background: %4;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.surfaceAltColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::toolBarStyle(const Theme &theme) {
  return QString("QToolBar {"
                 "  background: %1;"
                 "  border: none;"
                 "  spacing: 2px;"
                 "  padding: 2px;"
                 "}"
                 "QToolButton {"
                 "  background: transparent;"
                 "  color: %2;"
                 "  border: 1px solid transparent;"
                 "  border-radius: 4px;"
                 "  padding: 4px 8px;"
                 "}"
                 "QToolButton:hover {"
                 "  background: %3;"
                 "  border-color: %4;"
                 "  color: %4;"
                 "}"
                 "QToolButton:pressed {"
                 "  background: %5;"
                 "}"
                 "QToolButton:checked {"
                 "  background: %6;"
                 "  border-color: %7;"
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.hoverColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.pressedColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.accentColor.name());
}
