#include "mainwindow_internal.h"
#include "workspaceicons.h"
#include <QFontDatabase>

void MainWindow::setTheme(Theme theme) {
  setTheme(ThemeDefinition::fromClassicTheme(theme, "Custom"));
}

void MainWindow::setTheme(const ThemeDefinition &requestedTheme) {
  ThemeEngine::instance().setActiveTheme(requestedTheme);
  const ThemeDefinition &themeDefinition =
      ThemeEngine::instance().activeTheme();
  Theme theme = themeDefinition.toClassicTheme();
  settings.theme = theme;
  ThemedMessageBox::setGlobalTheme(themeDefinition);

  const ThemeColors &tc = themeDefinition.colors;
  const qreal glowLevel = qBound(0.0, themeDefinition.ui.glowIntensity, 1.0);
  QColor glowSource = tc.accentPrimary.lighter(130 + qRound(glowLevel * 45.0));
  if (tc.accentGlow.isValid()) {
    QColor accentGlow = tc.accentGlow;
    accentGlow.setAlpha(255);
    glowSource = blendColor(accentGlow, glowSource, 0.55);
  }
  const bool panelBordersEnabled = themeDefinition.ui.panelBorders;

  QString bgColor = tc.surfaceBase.name();
  QString terminalBgColor =
      tc.termBg.isValid() ? tc.termBg.name() : tc.surfaceBase.name();
  QString fgColor = tc.textPrimary.name();
  QString surfaceColor =
      glowShift(tc.surfaceRaised, glowSource, glowLevel, 0.18).name();
  QString surfaceAltColor =
      glowShift(tc.surfaceOverlay, glowSource, glowLevel, 0.14).name();
  QString pressedColor = tc.btnGhostActive.name();

  QString rowHoverColor = tc.treeHoverBg.isValid() ? tc.treeHoverBg.name()
                                                   : tc.btnGhostHover.name();
  QString borderColor =
      panelBordersEnabled
          ? glowShift(tc.borderDefault, glowSource, glowLevel, 0.42).name()
          : QStringLiteral("transparent");
  QString borderSubtle =
      panelBordersEnabled
          ? glowShift(tc.borderSubtle, glowSource, glowLevel, 0.28).name()
          : QStringLiteral("transparent");
  QString accentColor =
      glowShift(tc.accentPrimary, glowSource, glowLevel, 0.34).name();
  QString accentSoftColor =
      glowShift(tc.accentSoft, glowSource, glowLevel, 0.46).name();
  QString secondaryText = tc.textSecondary.name();
  QString disabledText = tc.textDisabled.name();
  QString inputBg = glowShift(tc.inputBg, glowSource, glowLevel, 0.12).name();
  QString scrollThumb = tc.scrollThumb.name();
  QString scrollThumbHover = tc.scrollThumbHover.name();
  QString tabActiveBg =
      glowShift(tc.tabActiveBg, glowSource, glowLevel, 0.20).name();
  QString tabActiveBorder =
      glowShift(tc.tabActiveBorder, glowSource, glowLevel, 0.44).name();
  QString tabFg = tc.tabFg.name();
  QString tabActiveFg = tc.tabActiveFg.name();
  QString btnPrimaryBg = tc.btnPrimaryBg.name();
  QString btnPrimaryFg = tc.btnPrimaryFg.name();
  QString btnPrimaryHover =
      glowShift(tc.btnPrimaryHover, glowSource, glowLevel, 0.32).name();
  const QColor accentSoftSolid = QColor(accentSoftColor);
  QString accentOnSoft =
      ColorContrast::ensure(QColor(accentColor), accentSoftSolid).name();
  QString textOnSoft =
      ColorContrast::bestOf(accentSoftSolid, {tc.textPrimary, tc.textInverse})
          .name();
  QString btnPrimaryText =
      ColorContrast::bestOf(tc.btnPrimaryBg,
                            {tc.btnPrimaryFg, tc.textInverse, tc.textPrimary})
          .name();
  QString btnPrimaryHoverText =
      ColorContrast::bestOf(QColor(btnPrimaryHover),
                            {tc.btnPrimaryFg, tc.textInverse, tc.textPrimary})
          .name();
  QString inputSelection = tc.inputSelection.name();
  QString inputSelectionText =
      ColorContrast::bestOf(tc.inputSelection, {tc.inputFg, tc.textInverse})
          .name();
  QString popoverColor =
      glowShift(tc.surfacePopover, glowSource, glowLevel, 0.18).name();
  QString editorFontFamily = settings.mainFont.family();
  editorFontFamily.replace("\\", "\\\\");
  editorFontFamily.replace("\"", "\\\"");
  QString uiFontFamily =
      QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
  uiFontFamily.replace("\\", "\\\\");
  uiFontFamily.replace("\"", "\\\"");
  QString editorFontSizeRule;
  if (settings.mainFont.pointSize() > 0) {
    editorFontSizeRule =
        QString("font-size: %1pt; ").arg(settings.mainFont.pointSize());
  }

  QString styleSheet =

      "QWidget { "
      "background-color: " +
      bgColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "font-family: \"" +
      uiFontFamily +
      "\"; font-size: 13px; "
      "}"
      "QDialog { background-color: " +
      bgColor + "; border: 1px solid " + accentColor +
      "; }"

      "QMenu { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      popoverColor +
      "; "
      "selection-background-color: " +
      accentSoftColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 8px; "
      "padding: 4px; "
      "}"
      "QMenu::item { "
      "padding: 8px 32px 8px 12px; "
      "border-radius: 4px; "
      "margin: 2px 4px; "
      "}"
      "QMenu::item:selected { "
      "background-color: " +
      accentSoftColor +
      "; "
      "color: " +
      accentOnSoft +
      "; "
      "}"
      "QMenu::item:disabled { "
      "color: " +
      disabledText +
      "; "
      "}"
      "QMenu::separator { "
      "height: 1px; "
      "background: " +
      borderSubtle +
      "; "
      "margin: 4px 8px; "
      "}"
      "QMenu::icon { "
      "padding-left: 8px; "
      "}"

      "QMenuBar { "
      "background-color: " +
      surfaceColor +
      "; "
      "border-bottom: 1px solid " +
      borderSubtle +
      "; "
      "spacing: 2px; "
      "padding: 3px 6px; "
      "min-height: 28px; "
      "font-size: 13px; "
      "font-weight: 500; "
      "}"
      "QMenuBar::item { "
      "color: " +
      secondaryText +
      "; "
      "padding: 4px 9px; "
      "margin: 0 1px; "
      "border-radius: 3px; "
      "border-bottom: 2px solid transparent; "
      "}"
      "QMenuBar::item:selected { "
      "background-color: " +
      accentSoftColor +
      "; "
      "color: " +
      accentOnSoft +
      "; "
      "border-bottom: 2px solid " +
      accentColor +
      "; "
      "}"
      "QMenuBar::item:pressed { "
      "background-color: " +
      pressedColor +
      "; "
      "}"

      "QMessageBox { background-color: " +
      surfaceColor + "; color: " + fgColor +
      "; }"
      "QMessageBox QLabel { color: " +
      fgColor +
      "; }"
      "QMessageBox QCheckBox { color: " +
      fgColor +
      "; }"
      "QMessageBox QTextEdit, QMessageBox QPlainTextEdit { "
      "background-color: " +
      surfaceAltColor + "; color: " + fgColor + "; border: 1px solid " +
      borderColor +
      "; border-radius: 4px; "
      "}"

      "QPushButton { "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "padding: 8px 16px; "
      "background-color: " +
      surfaceColor +
      "; "
      "border-radius: 6px; "
      "font-weight: 500; "
      "}"
      "QPushButton:hover { "
      "background-color: " +
      accentSoftColor +
      "; "
      "border-color: " +
      accentColor +
      "; "
      "color: " +
      accentOnSoft +
      "; "
      "}"
      "QPushButton:disabled { "
      "color: " +
      disabledText +
      "; "
      "border-color: " +
      borderSubtle +
      "; "
      "}"
      "QPushButton:pressed { "
      "background-color: " +
      pressedColor +
      "; "
      "}"
      "QPushButton:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "outline: none; "
      "}"
      "QPushButton:default { "
      "background-color: " +
      btnPrimaryBg +
      "; "
      "border: 1px solid " +
      btnPrimaryBg +
      "; "
      "color: " +
      btnPrimaryText +
      "; "
      "font-weight: bold; "
      "}"
      "QPushButton:default:hover { "
      "background-color: " +
      btnPrimaryHover +
      "; "
      "color: " +
      btnPrimaryHoverText +
      "; "
      "}"

      "QToolButton { "
      "color: " +
      secondaryText +
      "; "
      "font-size: 12px; "
      "border: 1px solid transparent; "
      "padding: 6px 10px; "
      "background-color: transparent; "
      "border-radius: 6px; "
      "}"
      "QToolButton:hover { "
      "background-color: " +
      accentSoftColor +
      "; "
      "border-color: " +
      borderColor +
      "; "
      "color: " +
      accentOnSoft +
      "; "
      "}"
      "QToolButton:pressed { "
      "background-color: " +
      pressedColor +
      "; "
      "}"
      "QToolButton:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QToolButton:disabled { "
      "color: " +
      disabledText +
      "; "
      "}"

      "QToolButton#runButton { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "border: 1px solid transparent; "
      "padding: 6px; "
      "border-radius: 6px; "
      "}"
      "QToolButton#testButton, QToolButton#debugButton, "
      "QToolButton#magicButton "
      "{ "
      "background-color: transparent; "
      "border: 1px solid transparent; "
      "padding: 6px; "
      "border-radius: 6px; "
      "}"
      "QToolButton#runButton:hover, QToolButton#testButton:hover, "
      "QToolButton#debugButton:hover, QToolButton#magicButton:hover { "
      "background-color: " +
      accentSoftColor +
      "; "
      "}"

      "QToolButton#runButton::menu-button, "
      "QToolButton#testButton::menu-button, "
      "QToolButton#debugButton::menu-button { "
      "background: transparent; "
      "border: none; "
      "width: 18px; "
      "}"
      "QToolButton#languageHighlight, QToolButton#tabWidth { "
      "background-color: transparent; "
      "border: 1px solid transparent; "
      "color: " +
      secondaryText +
      "; "
      "padding: 6px 10px; "
      "font-size: 12px; "
      "}"
      "QToolButton#languageHighlight:hover, QToolButton#tabWidth:hover { "
      "background-color: " +
      accentSoftColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "}"
      "QLabel#rowCol { "
      "color: " +
      secondaryText +
      "; "
      "font-size: 12px; "
      "padding: 0 4px; "
      "}"

      "QAbstractItemView { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      bgColor +
      "; "
      "outline: 0; "
      "border: none; "
      "border-radius: 0px; "
      "}"
      "QAbstractItemView::item { "
      "padding: 4px 8px; "
      "border-radius: 3px; "
      "margin: 1px 2px; "
      "}"
      "QAbstractItemView::item:hover { "
      "background-color: " +
      rowHoverColor +
      "; "
      "}"

      "QAbstractItemView::item:focus { "
      "outline: none; "
      "border: none; "
      "}"

      "QAbstractItemView::item:selected { "
      "background-color: " +
      accentSoftColor +
      "; "
      "color: " +
      textOnSoft +
      "; "
      "}"

      "QHeaderView::section { "
      "background-color: transparent; "
      "color: " +
      secondaryText +
      "; "
      "padding: 5px 10px; "
      "border: none; "
      "font-weight: 600; "
      "text-transform: uppercase; "
      "font-size: 12px; "
      "letter-spacing: 1px; "
      "}"

      "QLineEdit { "
      "background-color: " +
      inputBg +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 8px 12px; "
      "selection-background-color: " +
      inputSelection +
      "; "
      "selection-color: " +
      inputSelectionText +
      "; "
      "}"
      "QLineEdit:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QLineEdit:disabled { "
      "background-color: " +
      surfaceColor +
      "; "
      "color: " +
      disabledText +
      "; "
      "}"

      "QComboBox { "
      "background-color: " +
      inputBg +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 6px 12px; "
      "min-height: 20px; "
      "}"
      "QComboBox::drop-down { "
      "border: none; "
      "width: 20px; "
      "}"
      "QComboBox:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QComboBox::down-arrow { "
      "width: 9px; "
      "height: 6px; " +
      UIStyleHelper::comboArrowImageRule(QColor(secondaryText)) +
      "}"
      "QComboBox QAbstractItemView { "
      "background-color: " +
      surfaceColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 4px; "
      "selection-background-color: " +
      accentSoftColor +
      "; "
      "selection-color: " +
      textOnSoft +
      "; "
      "outline: none; "
      "}"
      "QComboBox:disabled { "
      "color: " +
      disabledText +
      "; "
      "}"

      "QPlainTextEdit { "
      "color: " +
      tc.editorFg.name() +
      "; "
      "background-color: " +
      tc.editorBg.name() +
      "; "
      "font-family: \"" +
      editorFontFamily + "\"; " + editorFontSizeRule +
      "border: none; "
      "}"
      "QTextEdit { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      bgColor +
      "; "
      "border: none; "
      "}"

      "QGroupBox { "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 8px; "
      "margin-top: 16px; "
      "padding: 12px; "
      "font-weight: 600; "
      "}"
      "QGroupBox::title { "
      "subcontrol-origin: margin; "
      "subcontrol-position: top left; "
      "padding: 0 8px; "
      "color: " +
      ColorContrast::ensure(QColor(accentColor), QColor(bgColor)).name() +
      "; "
      "font-size: 12px; "
      "}"

      "QRadioButton { "
      "color: " +
      fgColor +
      "; "
      "spacing: 8px; "
      "}"
      "QRadioButton::indicator { "
      "width: 16px; "
      "height: 16px; "
      "border-radius: 8px; "
      "}"
      "QRadioButton::indicator:checked { "
      "background-color: " +
      accentColor +
      "; "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"
      "QRadioButton::indicator:unchecked { "
      "background-color: " +
      inputBg +
      "; "
      "border: 2px solid " +
      borderColor +
      "; "
      "}"
      "QRadioButton::indicator:unchecked:hover { "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"

      "QCheckBox { "
      "color: " +
      fgColor +
      "; "
      "spacing: 8px; "
      "}"
      "QCheckBox::indicator { "
      "width: 16px; "
      "height: 16px; "
      "border-radius: 4px; "
      "border: 2px solid " +
      borderColor +
      "; "
      "background-color: " +
      inputBg +
      "; "
      "}"
      "QCheckBox::indicator:checked { "
      "background-color: " +
      accentColor +
      "; "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"
      "QCheckBox::indicator:hover { "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"

      "QScrollBar:vertical { "
      "background-color: transparent; "
      "width: 8px; "
      "margin: 0; "
      "}"
      "QScrollBar::handle:vertical { "
      "background-color: " +
      scrollThumb +
      "; "
      "min-height: 32px; "
      "border-radius: 4px; "
      "margin: 2px 1px; "
      "}"
      "QScrollBar::handle:vertical:hover { "
      "background-color: " +
      scrollThumbHover +
      "; "
      "}"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
      "height: 0; "
      "}"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
      "background: none; "
      "}"
      "QScrollBar:horizontal { "
      "background-color: transparent; "
      "height: 8px; "
      "margin: 0; "
      "}"
      "QScrollBar::handle:horizontal { "
      "background-color: " +
      scrollThumb +
      "; "
      "min-width: 32px; "
      "border-radius: 4px; "
      "margin: 1px 2px; "
      "}"
      "QScrollBar::handle:horizontal:hover { "
      "background-color: " +
      scrollThumbHover +
      "; "
      "}"
      "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
      "width: 0; "
      "}"
      "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
      "background: none; "
      "}"

      "QToolTip { "
      "background-color: " +
      popoverColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 6px 10px; "
      "}"

      "QSplitter::handle { background-color: transparent; }"
      "QSplitter::handle:horizontal { "
      "width: 5px; "
      "border-left: 1px solid " +
      borderSubtle +
      "; "
      "}"
      "QSplitter::handle:vertical { "
      "height: 5px; "
      "border-top: 1px solid " +
      borderSubtle +
      "; "
      "}"
      "QSplitter::handle:hover { "
      "background-color: " +
      accentSoftColor +
      "; "
      "}"

      "QDockWidget { "
      "color: " +
      fgColor +
      "; "
      "titlebar-close-icon: url(none); "
      "titlebar-normal-icon: url(none); "
      "}"
      "QDockWidget::title { "
      "background-color: " +
      surfaceColor +
      "; "
      "padding: 6px 8px; "
      "border-bottom: 1px solid " +
      borderColor +
      "; "
      "}"
      "QDockWidget::close-button, QDockWidget::float-button { "
      "background: transparent; "
      "border: none; "
      "padding: 2px; "
      "}"
      "QDockWidget::close-button:hover, QDockWidget::float-button:hover { "
      "background-color: " +
      accentSoftColor +
      "; "
      "border-radius: 4px; "
      "}"
      "QDockWidget#terminalDock { "
      "background-color: " +
      terminalBgColor +
      "; "
      "border: none; "
      "margin: 0; "
      "padding: 0; "
      "}"
      "QDockWidget#terminalDock::title { "
      "background-color: " +
      terminalBgColor +
      "; "
      "color: " +
      terminalBgColor +
      "; "
      "border: none; "
      "border-bottom: 1px solid " +
      terminalBgColor +
      "; "
      "padding: 0; "
      "}"

      "QMainWindow::separator { "
      "background-color: " +
      borderSubtle +
      "; "
      "width: 6px; "
      "height: 6px; "
      "}"
      "QMainWindow::separator:hover { "
      "background-color: " +
      accentColor +
      "; "
      "}"

      "QTabBar::tab { "
      "font-size: 12px; "
      "background-color: " +
      surfaceColor +
      "; "
      "color: " +
      tabFg +
      "; "
      "padding: 7px 16px; "
      "border: none; "
      "border-bottom: 3px solid transparent; "
      "font-weight: 500; "
      "}"
      "QTabBar::tab:selected { "
      "color: " +
      ColorContrast::ensure(QColor(accentColor), QColor(tabActiveBg)).name() +
      "; "
      "background-color: " +
      tabActiveBg +
      "; "
      "border-bottom: 3px solid " +
      accentColor +
      "; "
      "}"
      "QTabBar::tab:hover:!selected { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      accentSoftColor +
      "; "
      "border-bottom: 3px solid " +
      accentSoftColor +
      "; "
      "}"

      "QStatusBar { "
      "background-color: " +
      surfaceColor +
      "; "
      "color: " +
      secondaryText +
      "; "
      "border-top: 1px solid " +
      borderSubtle +
      "; "
      "font-size: 12px; min-height: 26px; "
      "}"
      "QStatusBar::item { border: none; }"
      "QWidget#toolPanelTitleBar { background: " +
      surfaceAltColor + "; border-bottom: 1px solid " + borderSubtle +
      "; }"
      "QWidget#toolPanelTitleBar QLabel { background: transparent; color: " +
      secondaryText +
      "; font-size: 12px; }"
      "QWidget#toolPanelTitleBar QToolButton { color: " +
      fgColor +
      "; background: transparent; border: none; min-width: 22px; max-width: "
      "22px; "
      "min-height: 22px; max-height: 22px; padding: 0; font-size: 16px; }"
      "QWidget#toolPanelTitleBar QToolButton:hover { background: " +
      accentSoftColor +
      "; }"

      "QWidget#workspaceHeader { background-color: " +
      surfaceColor + "; border-bottom: 1px solid " + borderSubtle +
      "; }"
      "QWidget#workspaceHeader QToolButton { min-height: 22px; max-height: "
      "22px; "
      "padding: 4px 8px; font-size: 12px; }"
      "QWidget#workspaceHeader QToolButton#workspaceFileSearch { background: " +
      inputBg + "; border: 1px solid " + borderSubtle + "; color: " + fgColor +
      "; }"
      "QWidget#workspaceHeader QToolButton#runButton { background: " +
      accentSoftColor + "; color: " + textOnSoft +
      "; font-weight: 600; }"

      "QWidget#backgroundBottom { "
      "background-color: " +
      surfaceColor +
      "; "
      "border: none; "
      "}"
      "QWidget#backgroundBottom QLabel { "
      "font-size: 12px; "
      "padding: 0 6px; border: none; "
      "}"
      "QWidget#backgroundBottom QToolButton { "
      "min-height: 22px; "
      "max-height: 22px; "
      "font-size: 12px; "
      "padding: 0 6px; border: none; "
      "}"
      "QDockWidget { "
      "font-size: 12px; "
      "}"
      "QDockWidget::title { "
      "text-align: left; "
      "font-size: 12px; "
      "padding: 4px 10px; "
      "background-color: " +
      surfaceAltColor +
      "; "
      "color: " +
      ColorContrast::ensure(QColor(secondaryText), QColor(surfaceAltColor))
          .name() +
      "; "
      "border-bottom: 1px solid " +
      borderSubtle +
      "; "
      "font-weight: 500; "
      "}"
      "QDockWidget#terminalDock { "
      "background-color: " +
      terminalBgColor +
      "; "
      "border: none; "
      "margin: 0; "
      "padding: 0; "
      "}"
      "QDockWidget#terminalDock::title { "
      "background-color: " +
      terminalBgColor +
      "; "
      "color: " +
      terminalBgColor +
      "; "
      "border: none; "
      "border-bottom: 1px solid " +
      terminalBgColor +
      "; "
      "padding: 0; "
      "margin: 0; "
      "}"
      "QWidget#FindReplacePanel { "
      "background-color: " +
      surfaceColor +
      "; "
      "border-top: 1px solid " +
      borderColor +
      "; "
      "}"
      "QWidget#FindReplacePanel QToolButton:checked { background-color: " +
      accentSoftColor + "; color: " + accentOnSoft + "; border: 1px solid " +
      accentColor +
      "; }"
      "QWidget#FindReplacePanel QPushButton#applyReplacements { "
      "background-color: " +
      btnPrimaryBg + "; color: " + btnPrimaryText +
      "; font-weight: bold; }"
      "QWidget#Terminal { "
      "background-color: " +
      terminalBgColor +
      "; "
      "border: none; "
      "margin: 0; "
      "padding: 0; "
      "}"
      "QWidget#TerminalTabWidget { "
      "background-color: " +
      terminalBgColor +
      "; "
      "border: none; "
      "margin: 0; "
      "padding: 0; "
      "}"

      "QDialog QPushButton { "
      "min-height: 32px; "
      "}"
      "QDialogButtonBox QPushButton { "
      "min-width: 80px; "
      "min-height: 32px; "
      "}"

      "LineEditIcon { "
      "background-color: " +
      inputBg +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 2px; "
      "}"
      "LineEditIcon:hover { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "LineEditIcon QLineEdit { "
      "background: transparent; "
      "border: none; "
      "padding: 4px 6px; "
      "color: " +
      fgColor +
      "; "
      "}"
      "LineEditIcon QToolButton { "
      "background: transparent; "
      "border: none; "
      "padding: 4px; "
      "}"

      "QSpinBox { "
      "background-color: " +
      inputBg +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 4px 8px; "
      "}"
      "QSpinBox:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QSpinBox::up-button, QSpinBox::down-button { "
      "background-color: " +
      surfaceColor +
      "; "
      "border: none; "
      "width: 16px; "
      "}"
      "QSpinBox::up-button:hover, QSpinBox::down-button:hover { "
      "background-color: " +
      accentSoftColor +
      "; "
      "}"

      "QProgressBar { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 4px; "
      "text-align: center; "
      "color: " +
      fgColor +
      "; "
      "}"
      "QProgressBar::chunk { "
      "background-color: " +
      accentColor +
      "; "
      "border-radius: 3px; "
      "}";

  qApp->setPalette(ThemePalette::build(themeDefinition));
  qApp->setStyleSheet(styleSheet);
  const auto chromeIcon = [&](WorkspaceIcons::Glyph glyph) {
    return WorkspaceIcons::icon(
        glyph, ColorContrast::ensure(tc.textSecondary, QColor(surfaceColor)));
  };
  ui->runButton->setIcon(
      WorkspaceIcons::icon(WorkspaceIcons::Glyph::Run, QColor(textOnSoft)));
  ui->debugButton->setIcon(chromeIcon(WorkspaceIcons::Glyph::Debug));
  ui->testButton->setIcon(chromeIcon(WorkspaceIcons::Glyph::Tests));
  ui->magicButton->setIcon(chromeIcon(WorkspaceIcons::Glyph::Format));
  updateAllTextAreas(&TextArea::setFont, settings.mainFont);

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    applyTabWidgetTheme(tabWidget);
  }
  if (terminalWidget) {
    terminalWidget->applyTheme(theme);
  }

  if (commandPalette) {
    commandPalette->applyTheme(themeDefinition);
  }
  if (goToLineDialog) {
    goToLineDialog->applyTheme(themeDefinition);
  }
  if (goToSymbolDialog) {
    goToSymbolDialog->applyTheme(themeDefinition);
  }
  if (fileQuickOpen) {
    fileQuickOpen->applyTheme(themeDefinition);
  }
  if (recentFilesDialog) {
    recentFilesDialog->applyTheme(themeDefinition);
  }
  if (breadcrumbWidget) {
    breadcrumbWidget->applyTheme(themeDefinition);
  }
  if (problemsPanel) {
    problemsPanel->applyTheme(theme);
  }
  if (sourceControlPanel) {
    sourceControlPanel->applyTheme(theme);
  }
  if (conflictCenterPanel) {
    conflictCenterPanel->applyTheme(theme);
  }
  if (debugPanel) {
    debugPanel->applyTheme(theme);
  }
  if (testPanel) {
    testPanel->applyTheme(theme);
  }
  if (databasePanel) {
    databasePanel->applyTheme(theme);
  }
  restyleStatusBarItems();
  for (StyledDialog *dialog : findChildren<StyledDialog *>()) {
    if (dialog->isVisible())
      dialog->applyTheme(theme);
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (page) {
        page->applyTheme(themeDefinition);
      }
    }
    for (ConflictResolverView *view :
         tabWidget->findChildren<ConflictResolverView *>()) {
      view->applyTheme(theme);
    }
  }

  updateAllTextAreas(&TextArea::applyTheme, theme);
  setScanlineEffectEnabled(themeDefinition.ui.scanlineEffect);

  if (m_globalSettingsLoaded && !m_restoringSession) {
    saveSettings();
  }
}
