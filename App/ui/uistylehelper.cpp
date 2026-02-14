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
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.borderColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.accentColor.name());
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
                 "}"
                 "QListWidget::item:hover {"
                 "  background: %5;"
                 "}")
      .arg(theme.backgroundColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentSoftColor.name())
      .arg(theme.hoverColor.name());
}

QString UIStyleHelper::panelHeaderStyle(const Theme &theme) {
  return QString("background: %1; "
                 "border-bottom: 1px solid %2;")
      .arg(theme.surfaceColor.name())
      .arg(theme.borderColor.name());
}

QString UIStyleHelper::treeWidgetStyle(const Theme &theme) {
  return QString("QTreeWidget {"
                 "  background: %1;"
                 "  alternate-background-color: %8;"
                 "  color: %2;"
                 "  border: none;"
                 "  selection-background-color: %3;"
                 "  selection-color: %2;"
                 "}"
                 "QTreeWidget::item {"
                 "  padding: 4px;"
                 "}"
                 "QTreeWidget::item:selected {"
                 "  background: %3;"
                 "  color: %2;"
                 "}"
                 "QTreeWidget::item:!active:selected {"
                 "  background: %3;"
                 "  color: %2;"
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
      .arg(theme.hoverColor.name())
      .arg(theme.surfaceColor.name())
      .arg(theme.singleLineCommentFormat.name())
      .arg(theme.borderColor.name())
      .arg(theme.surfaceAltColor.name());
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
                 "}")
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.surfaceAltColor.name())
      .arg(theme.successColor.name());
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
                 "}")
      .arg(theme.surfaceColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.singleLineCommentFormat.name());
}

QString UIStyleHelper::lineEditStyle(const Theme &theme) {
  return QString("QLineEdit {"
                 "  background: %1;"
                 "  color: %2;"
                 "  border: 1px solid %3;"
                 "  border-radius: 6px;"
                 "  padding: 8px 12px;"
                 "  font-size: 12px;"
                 "}"
                 "QLineEdit:focus {"
                 "  border-color: %4;"
                 "}")
      .arg(theme.hoverColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.accentColor.name());
}

QString UIStyleHelper::primaryButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: %1;"
                 "  border: 1px solid %1;"
                 "  color: white;"
                 "  border-radius: 6px;"
                 "  padding: 8px 16px;"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  background: %2;"
                 "  border-color: %2;"
                 "}")
      .arg(theme.successColor.name())
      .arg(theme.successColor.lighter(110).name());
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
                 "}")
      .arg(theme.hoverColor.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name())
      .arg(theme.pressedColor.name());
}

QString UIStyleHelper::breadcrumbButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  padding: 2px 6px;"
                 "  font-size: 12px;"
                 "}"
                 "QPushButton:hover {"
                 "  color: %2;"
                 "  background: %3;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.singleLineCommentFormat.name())
      .arg(theme.foregroundColor.name())
      .arg(theme.borderColor.name());
}

QString UIStyleHelper::breadcrumbActiveButtonStyle(const Theme &theme) {
  return QString("QPushButton {"
                 "  background: transparent;"
                 "  color: %1;"
                 "  border: none;"
                 "  padding: 2px 6px;"
                 "  font-size: 12px;"
                 "  font-weight: bold;"
                 "}"
                 "QPushButton:hover {"
                 "  color: %2;"
                 "  background: %3;"
                 "  border-radius: 3px;"
                 "}")
      .arg(theme.foregroundColor.name())
      .arg(theme.foregroundColor.lighter(110).name())
      .arg(theme.borderColor.name());
}

QString UIStyleHelper::breadcrumbSeparatorStyle(const Theme &theme) {
  return QString("QLabel {"
                 "  color: %1;"
                 "  font-size: 12px;"
                 "}")
      .arg(theme.borderColor.darker(110).name());
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
