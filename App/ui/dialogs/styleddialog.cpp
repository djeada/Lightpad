#include "styleddialog.h"
#include "../uistylehelper.h"

StyledDialog::StyledDialog(QWidget *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags) {}

void StyledDialog::applyTheme(const Theme &theme) {
  m_theme = theme;

  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (auto *w : findChildren<QGroupBox *>())
    w->setStyleSheet(UIStyleHelper::groupBoxStyle(theme));
  for (auto *w : findChildren<QLineEdit *>())
    w->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  for (auto *w : findChildren<QComboBox *>())
    w->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  for (auto *w : findChildren<QCheckBox *>())
    w->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  for (auto *w : findChildren<QListWidget *>())
    w->setStyleSheet(UIStyleHelper::resultListStyle(theme));
  for (auto *w : findChildren<QPushButton *>())
    w->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  for (auto *w : findChildren<QTableWidget *>())
    w->setStyleSheet(UIStyleHelper::tableWidgetStyle(theme));
  for (auto *w : findChildren<QPlainTextEdit *>())
    w->setStyleSheet(UIStyleHelper::plainTextEditStyle(theme));
  for (auto *w : findChildren<QTextEdit *>())
    w->setStyleSheet(UIStyleHelper::plainTextEditStyle(theme));
  for (auto *w : findChildren<QSpinBox *>())
    w->setStyleSheet(UIStyleHelper::spinBoxStyle(theme));
  for (auto *w : findChildren<QTabWidget *>())
    w->setStyleSheet(UIStyleHelper::tabWidgetStyle(theme));
}

void StyledDialog::stylePrimaryButton(QPushButton *btn) {
  if (btn)
    btn->setStyleSheet(UIStyleHelper::primaryButtonStyle(m_theme));
}

void StyledDialog::styleSecondaryButton(QPushButton *btn) {
  if (btn)
    btn->setStyleSheet(UIStyleHelper::secondaryButtonStyle(m_theme));
}

void StyledDialog::styleDangerButton(QPushButton *btn) {
  if (btn)
    btn->setStyleSheet(UIStyleHelper::dangerButtonStyle(m_theme));
}

void StyledDialog::styleTitleLabel(QLabel *label) {
  if (label)
    label->setStyleSheet(UIStyleHelper::titleLabelStyle(m_theme));
}

void StyledDialog::styleSubduedLabel(QLabel *label) {
  if (label)
    label->setStyleSheet(UIStyleHelper::subduedLabelStyle(m_theme));
}
