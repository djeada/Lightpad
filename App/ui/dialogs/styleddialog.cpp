#include "styleddialog.h"
#include "../../theme/themedefinition.h"
#include "../uistylehelper.h"
#include <QAbstractSpinBox>
#include <QRadioButton>
#include <QTreeWidget>

namespace {
Theme classicThemeFromDefinition(const ThemeDefinition &themeDefinition) {
  Theme theme;
  const ThemeColors &c = themeDefinition.colors;

  theme.backgroundColor = c.surfaceBase;
  theme.foregroundColor = c.textPrimary;
  theme.highlightColor = c.editorSelection;
  theme.lineNumberAreaColor = c.editorGutter;

  theme.keywordFormat_0 = c.syntaxKeyword;
  theme.keywordFormat_1 = c.syntaxKeyword2;
  theme.keywordFormat_2 = c.syntaxKeyword3;
  theme.searchFormat = c.editorFindMatchActive;
  theme.singleLineCommentFormat = c.textSecondary;
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
  theme.infoColor = c.statusInfo;
  theme.operatorFormat = c.syntaxOperator;
  theme.constantFormat = c.syntaxConstant;
  theme.escapeFormat = c.syntaxEscape;
  theme.regexFormat = c.syntaxRegex;

  theme.diagnosticErrorColor = c.diagnosticError;
  theme.diagnosticWarningColor = c.diagnosticWarning;
  theme.diagnosticInfoColor = c.diagnosticInfo;
  theme.diagnosticHintColor = c.diagnosticHint;
  theme.gitAddedColor = c.gitAdded;
  theme.gitModifiedColor = c.gitModified;
  theme.gitDeletedColor = c.gitDeleted;
  theme.gitRenamedColor = c.gitRenamed;
  theme.gitCopiedColor = c.gitCopied;
  theme.gitUntrackedColor = c.gitUntracked;
  theme.gitConflictedColor = c.gitConflicted;
  theme.gitIgnoredColor = c.gitIgnored;
  theme.diffAddedColor = c.diffAdded;
  theme.diffModifiedColor = c.diffModified;
  theme.diffRemovedColor = c.diffRemoved;
  theme.diffConflictColor = c.diffConflict;
  theme.testPassedColor = c.testPassed;
  theme.testFailedColor = c.testFailed;
  theme.testSkippedColor = c.testSkipped;
  theme.testRunningColor = c.testRunning;
  theme.testQueuedColor = c.testQueued;
  theme.debugReadyColor = c.debugReady;
  theme.debugStartingColor = c.debugStarting;
  theme.debugRunningColor = c.debugRunning;
  theme.debugPausedColor = c.debugPaused;
  theme.debugErrorColor = c.debugError;
  theme.debugBreakpointColor = c.debugBreakpoint;
  theme.debugCurrentLineColor = c.debugCurrentLine;

  theme.borderRadius = themeDefinition.ui.borderRadius;
  theme.glowIntensity = themeDefinition.ui.glowIntensity;
  theme.chromeOpacity = themeDefinition.ui.chromeOpacity;
  theme.scanlineEffect = themeDefinition.ui.scanlineEffect;
  theme.panelBorders = themeDefinition.ui.panelBorders;
  return theme;
}
} // namespace

template <typename ThemeT>
StyledDialog::SemanticStyleCache buildSemanticStyles(const ThemeT &theme) {
  StyledDialog::SemanticStyleCache styles;
  styles.formDialog = UIStyleHelper::formDialogStyle(theme);
  styles.groupBox = UIStyleHelper::groupBoxStyle(theme);
  styles.lineEdit = UIStyleHelper::lineEditStyle(theme);
  styles.comboBox = UIStyleHelper::comboBoxStyle(theme);
  styles.checkBox = UIStyleHelper::checkBoxStyle(theme);
  styles.resultList = UIStyleHelper::listWidgetStyle(theme);
  styles.treeWidget = UIStyleHelper::treeWidgetStyle(theme);
  styles.secondaryButton = UIStyleHelper::secondaryButtonStyle(theme);
  styles.tableWidget = UIStyleHelper::tableWidgetStyle(theme);
  styles.plainTextEdit = UIStyleHelper::plainTextEditStyle(theme);
  styles.spinBox = UIStyleHelper::spinBoxStyle(theme);
  styles.tabWidget = UIStyleHelper::tabWidgetStyle(theme);
  styles.primaryButton = UIStyleHelper::primaryButtonStyle(theme);
  styles.dangerButton = UIStyleHelper::dangerButtonStyle(theme);
  styles.titleLabel = UIStyleHelper::titleLabelStyle(theme);
  styles.subduedLabel = UIStyleHelper::subduedLabelStyle(theme);
  styles.sectionLabel = UIStyleHelper::sectionLabelStyle(theme);
  styles.emptyState = UIStyleHelper::emptyStateStyle(theme);
  return styles;
}

StyledDialog::StyledDialog(QWidget *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags) {}

void StyledDialog::applyTheme(const Theme &theme) {
  m_theme = theme;
  m_semanticStyles = m_pendingDefinition
                         ? buildSemanticStyles(*m_pendingDefinition)
                         : buildSemanticStyles(theme);
  m_hasSemanticStyles = true;
  applySemanticStyles();
}

void StyledDialog::applyTheme(const ThemeDefinition &theme) {
  m_pendingDefinition = &theme;
  applyTheme(classicThemeFromDefinition(theme));
  m_pendingDefinition = nullptr;
}

void StyledDialog::applySemanticStyles() {
  const SemanticStyleCache &s = m_semanticStyles;
  setStyleSheet(s.formDialog);

  for (auto *w : findChildren<QGroupBox *>())
    w->setStyleSheet(s.groupBox);
  for (auto *w : findChildren<QLineEdit *>())
    w->setStyleSheet(s.lineEdit);
  for (auto *w : findChildren<QComboBox *>())
    w->setStyleSheet(s.comboBox);
  for (auto *w : findChildren<QCheckBox *>())
    w->setStyleSheet(s.checkBox);
  for (auto *w : findChildren<QRadioButton *>())
    w->setStyleSheet(s.checkBox);
  for (auto *w : findChildren<QListWidget *>())
    w->setStyleSheet(s.resultList);
  for (auto *w : findChildren<QTreeWidget *>())
    w->setStyleSheet(s.treeWidget);
  for (auto *w : findChildren<QPushButton *>())
    w->setStyleSheet(s.secondaryButton);
  for (auto *w : findChildren<QTableWidget *>())
    w->setStyleSheet(s.tableWidget);
  for (auto *w : findChildren<QPlainTextEdit *>())
    w->setStyleSheet(s.plainTextEdit);
  for (auto *w : findChildren<QTextEdit *>())
    w->setStyleSheet(s.plainTextEdit);
  for (auto *w : findChildren<QAbstractSpinBox *>())
    w->setStyleSheet(s.spinBox);
  for (auto *w : findChildren<QTabWidget *>())
    w->setStyleSheet(s.tabWidget);
}

void StyledDialog::setKeyboardDefault(QPushButton *button) {
  for (QPushButton *candidate : findChildren<QPushButton *>()) {
    candidate->setAutoDefault(false);
    candidate->setDefault(false);
  }
  if (button) {
    button->setAutoDefault(true);
    button->setDefault(true);
  }
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

void StyledDialog::styleSectionLabel(QLabel *label) {
  if (label)
    label->setStyleSheet(m_hasSemanticStyles
                             ? m_semanticStyles.sectionLabel
                             : UIStyleHelper::sectionLabelStyle(m_theme));
}

void StyledDialog::styleEmptyState(QLabel *label) {
  if (label)
    label->setStyleSheet(m_hasSemanticStyles
                             ? m_semanticStyles.emptyState
                             : UIStyleHelper::emptyStateStyle(m_theme));
}

void StyledDialog::styleToneLabel(QLabel *label, UIStyleHelper::Tone tone) {
  if (label)
    label->setStyleSheet(UIStyleHelper::toneLabelStyle(m_theme, tone));
}

void StyledDialog::styleBadge(QLabel *label, UIStyleHelper::Tone tone) {
  if (label)
    label->setStyleSheet(UIStyleHelper::badgeStyle(m_theme, tone));
}
