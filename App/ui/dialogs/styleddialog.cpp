#include "styleddialog.h"
#include "../../theme/themedefinition.h"
#include "../uistylehelper.h"

namespace {
Theme classicThemeFromDefinition(const ThemeDefinition &themeDefinition) {
  Theme theme;
  const ThemeColors &c = themeDefinition.colors;

  theme.backgroundColor = c.editorBg;
  theme.foregroundColor = c.editorFg;
  theme.highlightColor = c.editorSelection;
  theme.lineNumberAreaColor = c.editorGutter;

  theme.keywordFormat_0 = c.syntaxKeyword;
  theme.keywordFormat_1 = c.syntaxKeyword2;
  theme.keywordFormat_2 = c.syntaxKeyword3;
  theme.searchFormat = c.editorFindMatchActive;
  theme.singleLineCommentFormat = c.syntaxComment;
  theme.functionFormat = c.syntaxFunction;
  theme.quotationFormat = c.syntaxString;
  theme.classFormat = c.syntaxClass;
  theme.numberFormat = c.syntaxNumber;

  theme.surfaceColor = c.surfaceRaised;
  theme.surfaceAltColor = c.surfaceOverlay;
  theme.borderColor = c.borderDefault;
  theme.hoverColor = c.btnGhostHover;
  theme.pressedColor = c.btnGhostActive;
  theme.accentColor = c.accentPrimary;
  theme.accentSoftColor = c.accentSoft;
  theme.successColor = c.statusSuccess;
  theme.warningColor = c.statusWarning;
  theme.errorColor = c.statusError;

  theme.borderRadius = themeDefinition.ui.borderRadius;
  theme.glowIntensity = themeDefinition.ui.glowIntensity;
  theme.chromeOpacity = themeDefinition.ui.chromeOpacity;
  theme.scanlineEffect = themeDefinition.ui.scanlineEffect;
  theme.panelBorders = themeDefinition.ui.panelBorders;
  return theme;
}
} // namespace

StyledDialog::StyledDialog(QWidget *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags) {}

void StyledDialog::applyTheme(const Theme &theme) {
  m_theme = theme;
  m_hasSemanticStyles = false;

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

void StyledDialog::applyTheme(const ThemeDefinition &theme) {
  m_theme = classicThemeFromDefinition(theme);
  m_hasSemanticStyles = true;
  m_semanticStyles.formDialog = UIStyleHelper::formDialogStyle(theme);
  m_semanticStyles.groupBox = UIStyleHelper::groupBoxStyle(theme);
  m_semanticStyles.lineEdit = UIStyleHelper::lineEditStyle(theme);
  m_semanticStyles.comboBox = UIStyleHelper::comboBoxStyle(theme);
  m_semanticStyles.checkBox = UIStyleHelper::checkBoxStyle(theme);
  m_semanticStyles.resultList = UIStyleHelper::resultListStyle(theme);
  m_semanticStyles.secondaryButton = UIStyleHelper::secondaryButtonStyle(theme);
  m_semanticStyles.tableWidget = UIStyleHelper::tableWidgetStyle(theme);
  m_semanticStyles.plainTextEdit = UIStyleHelper::plainTextEditStyle(theme);
  m_semanticStyles.spinBox = UIStyleHelper::spinBoxStyle(theme);
  m_semanticStyles.tabWidget = UIStyleHelper::tabWidgetStyle(theme);
  m_semanticStyles.primaryButton = UIStyleHelper::primaryButtonStyle(theme);
  m_semanticStyles.dangerButton = UIStyleHelper::dangerButtonStyle(theme);
  m_semanticStyles.titleLabel = UIStyleHelper::titleLabelStyle(theme);
  m_semanticStyles.subduedLabel = UIStyleHelper::subduedLabelStyle(theme);

  setStyleSheet(m_semanticStyles.formDialog);

  for (auto *w : findChildren<QGroupBox *>())
    w->setStyleSheet(m_semanticStyles.groupBox);
  for (auto *w : findChildren<QLineEdit *>())
    w->setStyleSheet(m_semanticStyles.lineEdit);
  for (auto *w : findChildren<QComboBox *>())
    w->setStyleSheet(m_semanticStyles.comboBox);
  for (auto *w : findChildren<QCheckBox *>())
    w->setStyleSheet(m_semanticStyles.checkBox);
  for (auto *w : findChildren<QListWidget *>())
    w->setStyleSheet(m_semanticStyles.resultList);
  for (auto *w : findChildren<QPushButton *>())
    w->setStyleSheet(m_semanticStyles.secondaryButton);
  for (auto *w : findChildren<QTableWidget *>())
    w->setStyleSheet(m_semanticStyles.tableWidget);
  for (auto *w : findChildren<QPlainTextEdit *>())
    w->setStyleSheet(m_semanticStyles.plainTextEdit);
  for (auto *w : findChildren<QTextEdit *>())
    w->setStyleSheet(m_semanticStyles.plainTextEdit);
  for (auto *w : findChildren<QSpinBox *>())
    w->setStyleSheet(m_semanticStyles.spinBox);
  for (auto *w : findChildren<QTabWidget *>())
    w->setStyleSheet(m_semanticStyles.tabWidget);
}

void StyledDialog::stylePrimaryButton(QPushButton *btn) {
  if (btn)
    btn->setStyleSheet(m_hasSemanticStyles
                           ? m_semanticStyles.primaryButton
                           : UIStyleHelper::primaryButtonStyle(m_theme));
}

void StyledDialog::styleSecondaryButton(QPushButton *btn) {
  if (btn)
    btn->setStyleSheet(m_hasSemanticStyles
                           ? m_semanticStyles.secondaryButton
                           : UIStyleHelper::secondaryButtonStyle(m_theme));
}

void StyledDialog::styleDangerButton(QPushButton *btn) {
  if (btn)
    btn->setStyleSheet(m_hasSemanticStyles
                           ? m_semanticStyles.dangerButton
                           : UIStyleHelper::dangerButtonStyle(m_theme));
}

void StyledDialog::styleTitleLabel(QLabel *label) {
  if (label)
    label->setStyleSheet(m_hasSemanticStyles
                             ? m_semanticStyles.titleLabel
                             : UIStyleHelper::titleLabelStyle(m_theme));
}

void StyledDialog::styleSubduedLabel(QLabel *label) {
  if (label)
    label->setStyleSheet(m_hasSemanticStyles
                             ? m_semanticStyles.subduedLabel
                             : UIStyleHelper::subduedLabelStyle(m_theme));
}
