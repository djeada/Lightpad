#include "mainwindow_internal.h"

void MainWindow::on_languageHighlight_clicked() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return;
  }

  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());

  QMenu menu(this);
  QActionGroup actionGroup(&menu);
  actionGroup.setExclusive(true);

  QAction *autoDetectAction = menu.addAction("Auto Detect");
  autoDetectAction->setCheckable(true);
  autoDetectAction->setChecked(filePath.isEmpty() ||
                               highlightOverrideForFile(filePath).isEmpty());
  autoDetectAction->setEnabled(!filePath.isEmpty());
  actionGroup.addAction(autoDetectAction);

  menu.addSeparator();

  QVector<LanguageInfo> languages = LanguageCatalog::allLanguages();
  if (languages.isEmpty()) {
    return;
  }
  for (const LanguageInfo &language : languages) {
    QAction *action = menu.addAction(language.displayName);
    action->setCheckable(true);
    action->setData(language.id);
    const TextArea *activeArea = getCurrentTextArea();
    const QString activeLanguageId =
        filePath.isEmpty() ? (activeArea ? activeArea->language() : QString())
                           : effectiveLanguageIdForFile(filePath);
    action->setChecked(activeLanguageId == language.id);
    actionGroup.addAction(action);
  }

  QAction *selectedAction = menu.exec(ui->languageHighlight->mapToGlobal(
      QPoint(0, ui->languageHighlight->height())));
  if (!selectedAction) {
    return;
  }

  QVariant selectedData = selectedAction->data();
  if (!selectedData.isValid()) {
    if (filePath.isEmpty()) {
      return;
    }
    setHighlightOverrideForFile(filePath, "");
    applyHighlightForFile(filePath);
    return;
  }

  applyLanguageOverride(selectedData.toString());
}

void MainWindow::on_actionAbout_triggered() {

  QFile TextFile(":/resources/messages/About.txt");

  if (TextFile.open(QIODevice::ReadOnly)) {
    QTextStream in(&TextFile);
    QString license = in.readAll();
    ThemedMessageBox::information(this, tr("About Lightpad"), license);
    TextFile.close();
  }
}

void MainWindow::on_actionAbout_Qt_triggered() { QApplication::aboutQt(); }

void MainWindow::on_tabWidth_clicked() {

  if (!popupTabWidth) {
    popupTabWidth = new PopupTabWidth(QStringList({"2", "4", "8"}), this);
    auto point = mapToGlobal(ui->tabWidth->pos());
    const QSize popupSize = popupTabWidth->sizeHint();
    QRect rect(point.x(), point.y() - 2 * popupSize.height() + height(),
               popupSize.width(), popupSize.height());
    popupTabWidth->setGeometry(rect);
  }

  else if (popupTabWidth->isHidden())
    popupTabWidth->show();

  else
    popupTabWidth->hide();
}

void MainWindow::on_actionReplace_in_file_triggered() {
  showFindReplace(false);
  if (findReplacePanel) {
    findReplacePanel->setGlobalMode(false);
    if (auto *area = getCurrentTextArea()) {
      const QString selected = area->textCursor().selectedText();
      if (!selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator))
        findReplacePanel->setSearchText(selected);
    }
    findReplacePanel->setFocusOnSearchBox();
  }
}

void MainWindow::on_actionReplace_in_project_triggered() {
  showFindReplace(false);
  if (!findReplacePanel) {
    return;
  }

  QString projectPath = m_projectRootPath;
  if (projectPath.isEmpty()) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    QString filePath = tabWidget
                           ? tabWidget->getFilePath(tabWidget->currentIndex())
                           : QString();
    if (!filePath.isEmpty()) {
      projectPath = QFileInfo(filePath).absolutePath();
    } else {
      projectPath = QDir::currentPath();
    }
  }

  findReplacePanel->setProjectPath(projectPath);
  findReplacePanel->setGlobalMode(true);
  findReplacePanel->setFocusOnSearchBox();
}

void MainWindow::on_actionKeyboard_shortcuts_triggered() {
  openShortcutsDialog();
}

void MainWindow::on_actionPreferences_triggered() {
  if (!preferences) {
    preferences = new Preferences(this);

    connect(preferences, &QObject::destroyed, this,
            [&] { preferences = nullptr; });
  }
}

void MainWindow::showThemeGallery() {
  auto *dlg = new ThemeGalleryDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  connect(dlg, &ThemeGalleryDialog::themeSelected, this,
          [this](const ThemeDefinition &def) { setTheme(def); });
  dlg->show();
}

void MainWindow::runTestsForCurrentContext() {
  // A dock created while restoring an earlier layout is not a fresh panel.
  const bool firstOpen =
      !testDock && SettingsManager::instance()
                       .getValue("mainWindowDockState", QString())
                       .toString()
                       .isEmpty();
  ensureTestPanel();

  QString currentFilePath;
  if (LightpadTabWidget *tabWidget = currentTabWidget()) {
    if (auto *page = tabWidget->getCurrentPage()) {
      currentFilePath = page->getFilePath();
    }
  }

  if (!currentFilePath.isEmpty()) {
    on_actionSave_triggered();
    ensureProjectRootForPath(currentFilePath);
  }

  if (m_projectRootPath.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Test"), tr("Open a file or project first to run tests."));
    return;
  }

  TestConfigurationManager::instance().setWorkspaceFolder(m_projectRootPath);
  TestConfigurationManager::instance().loadUserConfigurations(
      m_projectRootPath);
  refreshTestTargetButton();

  if (!testPanel) {
    return;
  }

  testPanel->setWorkspaceFolder(m_projectRootPath);

  bool started = false;
  const QString selectedId = selectedTestConfigurationId();
  if (!selectedId.isEmpty()) {
    started = testPanel->runWithConfigurationId(selectedId, currentFilePath);
    if (!started && currentFilePath.isEmpty()) {
      started = testPanel->runWithConfigurationId(selectedId);
    }
  }

  if (!started) {
    if (!currentFilePath.isEmpty())
      testPanel->runTestsForPath(currentFilePath);
    else
      testPanel->runAll();
  }

  revealTestDock(firstOpen);
  syncViewToggleActionStates();
}

void MainWindow::on_runButton_clicked() { runCurrentScript(); }

void MainWindow::on_debugButton_clicked() {
  const QString compoundName = selectedCompoundDebugConfigurationName();
  if (!compoundName.isEmpty()) {
    startCompoundDebugConfigurationByName(compoundName);
    return;
  }

  const QString configurationName = selectedDebugConfigurationName();
  if (configurationName.isEmpty()) {
    startDebuggingForCurrentFile();
    return;
  }

  startDebugConfigurationByName(configurationName);
}

void MainWindow::on_actionRun_file_name_triggered() { runCurrentScript(); }

void MainWindow::on_actionDebug_file_name_triggered() {
  startDebuggingForCurrentFile();
}

void MainWindow::on_actionStart_Debug_Configuration_triggered() {
  if (m_debugStartInProgress) {
    return;
  }

  QString currentFilePath;
  if (LightpadTabWidget *tabWidget = currentTabWidget()) {
    if (auto page = tabWidget->getCurrentPage()) {
      currentFilePath = page->getFilePath();
    }
  }

  if (!currentFilePath.isEmpty()) {
    on_actionSave_triggered();
    ensureProjectRootForPath(currentFilePath);
  }

  if (m_projectRootPath.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Debug Configuration"),
        tr("Open a file or project first to start a debug configuration."));
    return;
  }

  DebugSettings::instance().initialize(m_projectRootPath);
  DebugConfigurationManager::instance().setWorkspaceFolder(m_projectRootPath);
  DebugConfigurationManager::instance().loadFromLightpadDir();

  const QList<CompoundDebugConfiguration> compounds =
      DebugConfigurationManager::instance().allCompoundConfigurations();
  const QList<DebugConfiguration> configs =
      DebugConfigurationManager::instance().allConfigurations();
  QStringList configNames;
  QMap<QString, QString> displayToTarget;
  for (const DebugConfiguration &cfg : configs) {
    if (!cfg.name.trimmed().isEmpty()) {
      const QString display = tr("Config: %1").arg(cfg.name);
      configNames.append(display);
      displayToTarget[display] = cfg.name;
    }
  }
  for (const CompoundDebugConfiguration &compound : compounds) {
    if (!compound.name.trimmed().isEmpty()) {
      const QString display = tr("Compound: %1").arg(compound.name);
      configNames.append(display);
      displayToTarget[display] =
          QString::fromLatin1(kCompoundDebugTargetPrefix) + compound.name;
    }
  }

  if (configNames.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Debug Configuration"),
        tr("No debug configurations were found in this workspace."));
    openDebugConfigurationDialog();
    return;
  }

  QString lastTarget = activeDebugTarget();
  if (lastTarget.isEmpty()) {
    lastTarget = SettingsManager::instance()
                     .getValue("lastDebugConfiguration", QString())
                     .toString();
  }
  QString initialDisplay;
  for (auto it = displayToTarget.constBegin(); it != displayToTarget.constEnd();
       ++it) {
    if (it.value() == lastTarget) {
      initialDisplay = it.key();
      break;
    }
  }
  if (initialDisplay.isEmpty()) {
    const QString lastConfigName =
        SettingsManager::instance()
            .getValue("lastDebugConfiguration", QString())
            .toString();
    initialDisplay = tr("Config: %1").arg(lastConfigName);
  }
  int currentIndex = configNames.indexOf(initialDisplay);
  if (currentIndex < 0) {
    currentIndex = 0;
  }

  bool ok = false;
  const QString selectedName = QInputDialog::getItem(
      this, tr("Start Debug Configuration"), tr("Configuration:"), configNames,
      currentIndex, false, &ok);
  if (!ok || selectedName.trimmed().isEmpty()) {
    return;
  }

  const QString selectedTarget = displayToTarget.value(selectedName);
  setActiveDebugTarget(selectedTarget);
  SettingsManager::instance().saveSettings();
  refreshDebugTargetButton();
  if (selectedTarget.startsWith(QLatin1String(kCompoundDebugTargetPrefix))) {
    startCompoundDebugConfigurationByName(
        selectedTarget.mid(int(strlen(kCompoundDebugTargetPrefix))));
    return;
  }

  startDebugConfigurationByName(selectedTarget);
}

void MainWindow::on_actionEdit_Configurations_triggered() {
  openRunConfigurationForFile(runSourceFilePath());
}

void MainWindow::on_actionEdit_Debug_Configurations_triggered() {
  openDebugConfigurationDialog();
}

void MainWindow::on_magicButton_clicked() { formatCurrentDocument(); }

void MainWindow::on_actionFormat_Document_triggered() {
  formatCurrentDocument();
}

void MainWindow::on_actionEdit_Format_Configurations_triggered() {
  openFormatConfigurationDialog();
}

void MainWindow::on_actionGo_to_Line_triggered() { showGoToLineDialog(); }

void MainWindow::on_actionToggle_Minimap_triggered() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  LightpadPage *page = qobject_cast<LightpadPage *>(tabWidget->currentWidget());
  if (page) {
    bool visible = page->isMinimapVisible();

    for (LightpadTabWidget *targetWidget : allTabWidgets()) {
      for (int i = 0; i < targetWidget->count(); i++) {
        LightpadPage *p = qobject_cast<LightpadPage *>(targetWidget->widget(i));
        if (p) {
          p->setMinimapVisible(!visible);
        }
      }
    }
  }
  syncViewToggleActionStates();
}

void MainWindow::on_actionSplit_Horizontally_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->splitHorizontal();
    saveSettings();
  }
}

void MainWindow::on_actionSplit_Vertically_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->splitVertical();
    saveSettings();
  }
}

void MainWindow::on_actionClose_Editor_Group_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->closeCurrentGroup();
    saveSettings();
  }
}

void MainWindow::on_actionFocus_Next_Group_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->focusNextGroup();
  }
}

void MainWindow::on_actionFocus_Previous_Group_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->focusPreviousGroup();
  }
}

void MainWindow::on_actionUnsplit_All_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->unsplitAll();
    saveSettings();
  }
}

void MainWindow::on_actionToggle_Terminal_triggered() {
  ensureTerminalWidget();
  if (!m_terminalDock)
    return;
  bool visible = m_terminalDock->isVisible();
  if (!visible) {
    m_terminalDock->show();
    m_terminalDock->raise();
    if (Terminal *terminal = terminalWidget->currentTerminal()) {
      terminal->setFocus();
    }
  } else {
    m_terminalDock->hide();
    if (TextArea *editor = getCurrentTextArea()) {
      editor->setFocus();
    }
  }
  syncViewToggleActionStates();
}

void MainWindow::on_actionToggle_Source_Control_triggered() {
  ensureSourceControlPanel();

  if (sourceControlDock->isVisible()) {
    sourceControlDock->hide();
  } else {
    revealRightDock(sourceControlDock, 340);
  }
  syncViewToggleActionStates();
}

void MainWindow::on_actionToggle_Merge_Conflicts_triggered() {
  ensureConflictCenterPanel();

  if (conflictCenterDock->isVisible()) {
    conflictCenterDock->hide();
  } else {
    showConflictCenter();
  }
  syncViewToggleActionStates();
}

void MainWindow::on_actionOpen_To_Side_triggered() {
  if (!m_splitEditorContainer)
    return;

  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  QString filePath;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget) {
    int index = tabWidget->currentIndex();
    if (index >= 0) {
      filePath = tabWidget->getFilePath(index);
    }
  }

  if (filePath.isEmpty())
    return;

  LightpadTabWidget *newGroup = m_splitEditorContainer->splitHorizontal();
  if (newGroup) {
    openFileAndAddToNewTab(filePath, false);
    saveSettings();
  }
}

void MainWindow::on_actionTransform_Uppercase_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->transformToUppercase();
  }
}

void MainWindow::on_actionTransform_Lowercase_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->transformToLowercase();
  }
}

void MainWindow::on_actionTransform_Title_Case_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->transformToTitleCase();
  }
}

void MainWindow::on_actionSort_Lines_Ascending_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->sortLinesAscending();
  }
}

void MainWindow::on_actionSort_Lines_Descending_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->sortLinesDescending();
  }
}

void MainWindow::on_actionToggle_Word_Wrap_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    bool enabled = textArea->wordWrapEnabled();
    textArea->setWordWrapEnabled(!enabled);
    ui->actionToggle_Word_Wrap->setChecked(!enabled);
  }
}

void MainWindow::on_actionToggle_Vim_Mode_triggered() {
  bool enabled = !settings.vimModeEnabled;
  updateAllTextAreas(&TextArea::setVimModeEnabled, enabled);
  settings.vimModeEnabled = enabled;
  ui->actionToggle_Vim_Mode->setChecked(enabled);
  if (enabled) {
    connectVimMode(getCurrentTextArea());
  } else {
    updateVimStatusLabel("");
    hideVimCommandPanel();
  }
  saveSettings();
}

void MainWindow::on_actionSelect_All_triggered() {
  if (TextArea *textArea = getCurrentTextArea()) {
    textArea->selectAll();
  }
}

void MainWindow::on_actionToggle_Line_Comment_triggered() {
  if (TextArea *textArea = getCurrentTextArea()) {
    textArea->toggleLineComment();
  }
}

void MainWindow::on_actionMove_Line_Up_triggered() {
  if (TextArea *textArea = getCurrentTextArea()) {
    textArea->moveLines(true);
  }
}

void MainWindow::on_actionMove_Line_Down_triggered() {
  if (TextArea *textArea = getCurrentTextArea()) {
    textArea->moveLines(false);
  }
}

void MainWindow::on_actionDuplicate_Line_Down_triggered() {
  if (TextArea *textArea = getCurrentTextArea()) {
    textArea->duplicateLines(false);
  }
}

void MainWindow::on_actionDuplicate_Line_Up_triggered() {
  if (TextArea *textArea = getCurrentTextArea()) {
    textArea->duplicateLines(true);
  }
}

void MainWindow::on_actionFold_Current_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->foldCurrentBlock();
  }
}

void MainWindow::on_actionUnfold_Current_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->unfoldCurrentBlock();
  }
}

void MainWindow::on_actionFold_All_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->foldAll();
  }
}

void MainWindow::on_actionUnfold_All_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->unfoldAll();
  }
}

void MainWindow::on_actionFold_Comments_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->foldComments();
  }
}

void MainWindow::on_actionUnfold_Comments_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->unfoldComments();
  }
}
