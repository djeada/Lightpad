#include "../core/documentrecovery.h"
#include "mainwindow_internal.h"

namespace {
void replaceEditorContentsKeepingUndo(TextArea *area, const QString &text) {
  if (area->toPlainText() == text)
    return;
  const auto previous = area->textCursor();
  const int vertical = area->verticalScrollBar()->value();
  const int horizontal = area->horizontalScrollBar()->value();
  QTextCursor edit(area->document());
  edit.beginEditBlock();
  edit.select(QTextCursor::Document);
  edit.insertText(text);
  edit.endEditBlock();
  QTextCursor restored(area->document());
  restored.setPosition(qBound(0, previous.anchor(), int(text.size())));
  restored.setPosition(qBound(0, previous.position(), int(text.size())),
                       QTextCursor::KeepAnchor);
  area->setTextCursor(restored);
  area->verticalScrollBar()->setValue(vertical);
  area->horizontalScrollBar()->setValue(horizontal);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), popupTabWidth(nullptr),
      preferences(nullptr), findReplacePanel(nullptr), terminalWidget(nullptr),
      m_terminalDock(nullptr), m_problemsDock(nullptr), completer(nullptr),
      m_completionEngine(nullptr), highlightLanguage(""),
      font(QApplication::font()), commandPalette(nullptr),
      problemsPanel(nullptr), goToLineDialog(nullptr),
      goToSymbolDialog(nullptr), fileQuickOpen(nullptr),
      recentFilesDialog(nullptr), problemsStatusLabel(nullptr),
      vimStatusLabel(nullptr), m_pythonEnvLabel(nullptr),
      m_vimCommandPanelActive(false), m_connectedVimMode(nullptr),
      breadcrumbWidget(nullptr), recentFilesManager(nullptr),
      navigationHistory(nullptr), m_symbolNavService(nullptr),
      autoSaveManager(nullptr), m_openFileWatcher(nullptr),
      m_gitIntegration(nullptr), sourceControlPanel(nullptr),
      sourceControlDock(nullptr), conflictCenterPanel(nullptr),
      conflictCenterDock(nullptr), m_inlineBlameEnabled(false),
      m_heatmapEnabled(false), m_codeLensEnabled(false),
      m_gitBranchLabel(nullptr), m_gitSyncLabel(nullptr),
      m_gitDirtyLabel(nullptr), m_testTargetMenu(nullptr),
      m_debugTargetMenu(nullptr), m_runTargetMenu(nullptr), debugPanel(nullptr),
      debugDock(nullptr), testPanel(nullptr), testDock(nullptr),
      m_markdownPreviewPanel(nullptr), m_markdownPreviewDock(nullptr),
      m_latexPreviewPanel(nullptr), m_latexPreviewDock(nullptr),
      m_testStatusLabel(nullptr), m_debugStartInProgress(false),
      m_breakpointsSetConnection(), m_breakpointChangedConnection(),
      m_runInTerminalConnection(), m_sessionTerminatedConnection(),
      m_sessionErrorConnection(), m_sessionStateConnection(),
      m_runProcessFinishedConnection(), m_runProcessErrorConnection(),
      m_formatProcessFinishedConnection(), m_formatProcessErrorConnection(),
      m_splitEditorContainer(nullptr), m_diagnosticsManager(nullptr),
      m_languageFeatureManager(nullptr), m_lspCompletionProvider(),
      m_notificationManager(nullptr), m_lspStatusLabel(nullptr),
      m_lspStatusLanguageId(""), m_restoringSession(false),
      m_globalSettingsLoaded(false), m_fileTreeModel(nullptr),
      m_fileTreeSelectionModel(nullptr), m_treeScrollValue(0),
      m_treeScrollValueInitialized(false), m_treeScrollSyncing(false),
      m_treeFilterText(""), m_treeCurrentPath(""),
      m_treeSelectionSyncing(false) {
  QApplication::instance()->installEventFilter(this);
  ui->setupUi(this);
  setDockOptions(DockUtils::mainWindowDockOptions());
  setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
  ui->menubar->setNativeMenuBar(false);
  ui->actionNew_File->setShortcut(QKeySequence::New);
  ui->actionNew_File->setShortcutContext(Qt::ApplicationShortcut);
  ui->actionFind_in_file->setShortcut(QKeySequence::Find);
  ui->actionFind_in_file->setShortcutContext(Qt::ApplicationShortcut);
  ui->actionReplace_in_file->setShortcut(QKeySequence::Replace);
  ui->actionReplace_in_file->setShortcutContext(Qt::ApplicationShortcut);
  ui->actionFind_in_project->setShortcut(QKeySequence("Ctrl+Alt+F"));
  ui->actionReplace_in_project->setShortcut(QKeySequence("Ctrl+Shift+H"));
  if (ui->actionEdit_Test_Configurations) {
    connect(ui->actionEdit_Test_Configurations, &QAction::triggered, this,
            &MainWindow::openTestConfigurationDialog);
  }
  if (ui->menuView) {
    connect(ui->menuView, &QMenu::aboutToShow, this,
            &MainWindow::syncViewToggleActionStates);
  }
  for (QMenu *menu : {ui->menuRun, ui->menuDebug}) {
    if (menu) {
      connect(menu, &QMenu::aboutToShow, this,
              &MainWindow::refreshRunAndDebugActionLabels);
    }
  }
  if (ui->testButton) {
    connect(ui->testButton, &QToolButton::clicked, this,
            &MainWindow::runTestsForCurrentContext);
  }
  ensureFileTreeModel();
  connect(&RunTemplateManager::instance(),
          &RunTemplateManager::assignmentChanged, this,
          [this](const QString &) { updatePythonEnvironmentLabel(); });

  showMaximized();

  auto layout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
  if (layout) {
    int tabIndex = layout->indexOf(ui->tabWidget);
    layout->removeWidget(ui->tabWidget);
    m_splitEditorContainer = new SplitEditorContainer(ui->centralwidget);
    m_splitEditorContainer->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Expanding);
    m_splitEditorContainer->adoptTabWidget(ui->tabWidget);
    layout->insertWidget(tabIndex >= 0 ? tabIndex : 0, m_splitEditorContainer);
    layout->setStretch(layout->indexOf(m_splitEditorContainer), 1);
    layout->setStretch(layout->indexOf(ui->backgroundBottom), 0);
    connect(m_splitEditorContainer, &SplitEditorContainer::currentGroupChanged,
            this, [this](LightpadTabWidget *tabWidget) {
              updateTabWidgetContext(
                  tabWidget, tabWidget ? tabWidget->currentIndex() : -1);
            });
    connect(m_splitEditorContainer, &SplitEditorContainer::splitCountChanged,
            this, [this](int) {
              for (LightpadTabWidget *tabWidget : allTabWidgets()) {
                setupTabWidgetConnections(tabWidget);
                applyTabWidgetTheme(tabWidget);
                updateTabWidgetContext(tabWidget, tabWidget->currentIndex());
              }
            });
  }

  if (m_splitEditorContainer) {
    m_splitEditorContainer->setMainWindow(this);
  } else {
  }
  setupTabWidgetConnections(ui->tabWidget);
  ui->magicButton->setIconSize(0.8 * ui->magicButton->size());
  if (ui->testButton) {
    ui->testButton->setIconSize(0.8 * ui->testButton->size());
  }
  ui->debugButton->setIconSize(0.8 * ui->debugButton->size());

  recentFilesManager = new RecentFilesManager(this);

  setupNavigationHistory();

  setupAutoSave();
  setupOpenFileWatcher();

  setupDiagnostics();
  setupSymbolNavigation();

  setupCompletionSystem();

  QStringList wordList;
  wordList << "break" << "case" << "continue" << "default" << "do" << "else"
           << "for" << "if" << "return" << "switch" << "while";
  wordList << "auto" << "char" << "const" << "class" << "namespace"
           << "template" << "public" << "private" << "protected" << "virtual"
           << "override";
  wordList.sort();
  wordList.removeDuplicates();
  completer = new QCompleter(wordList, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);

  setupTextArea();
  setupTabWidget();
  setupDatabaseMenu();
  setupCommandPalette();
  setupGoToLineDialog();
  setupGoToSymbolDialog();
  setupFileQuickOpen();
  setupRecentFilesDialog();
  setupBreadcrumb();

  for (QAction *menuAction : menuBar()->actions()) {
    if (QMenu *menu = menuAction->menu()) {
      connect(menu, &QMenu::aboutToHide, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
          if (!QApplication::activePopupWidget()) {
            menuBar()->setActiveAction(nullptr);
          }
        });
      });
    }
  }
  setupGitIntegration();
  ensureDebugPanel();
  TestConfigurationManager::instance().loadTemplates();

  if (ui->testButton) {
    ui->testButton->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogContentsView));
    m_testTargetMenu = new QMenu(ui->testButton);
    ui->testButton->setMenu(m_testTargetMenu);
    ui->testButton->setPopupMode(QToolButton::MenuButtonPopup);
    ui->testButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->testButton->setMinimumWidth(150);
    connect(m_testTargetMenu, &QMenu::aboutToShow, this,
            &MainWindow::rebuildTestTargetMenu);
  }
  connect(&TestConfigurationManager::instance(),
          &TestConfigurationManager::templatesLoaded, this,
          &MainWindow::refreshTestTargetButton);
  connect(&TestConfigurationManager::instance(),
          &TestConfigurationManager::configurationsChanged, this,
          &MainWindow::refreshTestTargetButton);
  refreshTestTargetButton();

  if (ui->actionRun_file_name) {

    ui->actionRun_file_name->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F5));
    ui->actionRun_file_name->setShortcutContext(Qt::ApplicationShortcut);
  }

  if (ui->runButton) {
    m_runTargetMenu = new QMenu(ui->runButton);
    ui->runButton->setMenu(m_runTargetMenu);
    ui->runButton->setPopupMode(QToolButton::MenuButtonPopup);
    ui->runButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->runButton->setMinimumWidth(170);
    connect(m_runTargetMenu, &QMenu::aboutToShow, this,
            &MainWindow::rebuildRunTargetMenu);

    m_runTargetMenu->setTitle(tr("Run Target"));
    if (ui->menuRun) {
      ui->menuRun->addSeparator();
      ui->menuRun->addMenu(m_runTargetMenu);
    }
  }
  connect(&RunTemplateManager::instance(), &RunTemplateManager::templatesLoaded,
          this, [this]() {
            invalidateRunTargetCache();
            refreshRunTargetButton();
          });
  connect(&RunTemplateManager::instance(),
          &RunTemplateManager::assignmentChanged, this,
          [this](const QString &) {
            invalidateRunTargetCache();
            refreshRunTargetButton();
          });
  refreshRunTargetButton();

  m_debugTargetMenu = new QMenu(ui->debugButton);
  ui->debugButton->setMenu(m_debugTargetMenu);
  ui->debugButton->setPopupMode(QToolButton::MenuButtonPopup);
  ui->debugButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  ui->debugButton->setMinimumWidth(170);
  connect(m_debugTargetMenu, &QMenu::aboutToShow, this,
          &MainWindow::rebuildDebugTargetMenu);
  connect(&DebugConfigurationManager::instance(),
          &DebugConfigurationManager::configurationsLoaded, this,
          &MainWindow::refreshDebugTargetButton);
  connect(&DebugConfigurationManager::instance(),
          &DebugConfigurationManager::configurationAdded, this,
          &MainWindow::refreshDebugTargetButton);
  connect(&DebugConfigurationManager::instance(),
          &DebugConfigurationManager::configurationChanged, this,
          &MainWindow::refreshDebugTargetButton);
  connect(&DebugConfigurationManager::instance(),
          &DebugConfigurationManager::configurationRemoved, this,
          &MainWindow::refreshDebugTargetButton);
  refreshDebugTargetButton();

  const auto saveBreakpoints = []() {
    if (!BreakpointManager::instance().lightpadBreakpointsPath().isEmpty()) {
      BreakpointManager::instance().saveToLightpadDir();
    }
  };
  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointAdded,
          this, [saveBreakpoints](const Breakpoint &) { saveBreakpoints(); });
  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointRemoved,
          this,
          [saveBreakpoints](int, const QString &, int) { saveBreakpoints(); });
  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointChanged,
          this, [saveBreakpoints](const Breakpoint &) { saveBreakpoints(); });
  connect(&BreakpointManager::instance(),
          &BreakpointManager::allBreakpointsCleared, this,
          [saveBreakpoints]() { saveBreakpoints(); });
  connect(&BreakpointManager::instance(),
          &BreakpointManager::dataBreakpointsChanged, this,
          [saveBreakpoints]() { saveBreakpoints(); });
  connect(&BreakpointManager::instance(),
          &BreakpointManager::functionBreakpointsChanged, this,
          [saveBreakpoints]() { saveBreakpoints(); });
  connect(&BreakpointManager::instance(),
          &BreakpointManager::exceptionBreakpointsChanged, this,
          [saveBreakpoints]() { saveBreakpoints(); });

  const auto saveWatches = []() {
    if (!WatchManager::instance().lightpadWatchesPath().isEmpty()) {
      WatchManager::instance().saveToLightpadDir();
    }
  };
  connect(&WatchManager::instance(), &WatchManager::watchAdded, this,
          [saveWatches](const WatchExpression &) { saveWatches(); });
  connect(&WatchManager::instance(), &WatchManager::watchRemoved, this,
          [saveWatches](int) { saveWatches(); });
  connect(&WatchManager::instance(), &WatchManager::allWatchesCleared, this,
          [saveWatches]() { saveWatches(); });

  setupWorkspaceChrome();
  loadSettings();
  setWindowTitle("LightPad");
  setupDocumentRecovery();
}

LightpadTabWidget *MainWindow::currentTabWidget() const {
  if (m_splitEditorContainer) {
    LightpadTabWidget *tabWidget = m_splitEditorContainer->currentTabWidget();
    if (tabWidget) {
      return tabWidget;
    }
  }
  return ui->tabWidget;
}

QList<LightpadTabWidget *> MainWindow::allTabWidgets() const {
  if (m_splitEditorContainer) {
    return m_splitEditorContainer->allTabWidgets();
  }
  return {ui->tabWidget};
}

void MainWindow::setRowCol(int row, int col) {
  ui->rowCol->setText("Ln " + QString::number(row) + ", Col " +
                      QString::number(col));
  ensureStatusLabels();
}

void MainWindow::connectVimMode(TextArea *textArea) {
  if (!textArea) {
    return;
  }
  VimMode *vimMode = textArea->vimMode();
  if (!vimMode) {
    return;
  }

  disconnectVimMode();
  ensureStatusLabels();
  m_connectedVimMode = vimMode;

  connect(
      vimMode, &VimMode::modeChanged, this, [this, textArea](VimEditMode mode) {
        if (!textArea || !textArea->isVimModeEnabled()) {
          updateVimStatusLabel("");
          hideVimCommandPanel();
          return;
        }
        if (mode == VimEditMode::Command) {
          VimMode *vim = textArea->vimMode();
          showVimCommandPanel(QString(vim->commandType()), vim->commandText());
        } else {
          hideVimCommandPanel();
        }
        updateVimStatusLabel(textArea->vimMode()->modeName());
      });

  connect(vimMode, &VimMode::statusMessage, this,
          [this](const QString &message) { showVimStatusMessage(message); });

  connect(vimMode, &VimMode::commandBufferChanged, this,
          [this, textArea](const QString &) {
            if (!textArea || !textArea->isVimModeEnabled()) {
              return;
            }
            VimMode *currentVim = textArea->vimMode();
            if (!currentVim || currentVim->mode() != VimEditMode::Command) {
              return;
            }
            showVimCommandPanel(QString(currentVim->commandType()),
                                currentVim->commandText());
            if (findReplacePanel)
              findReplacePanel->setSearchCursorPosition(
                  currentVim->commandCursorPosition());
          });

  connect(vimMode, &VimMode::commandExecuted, this,
          [this, textArea](const QString &command) {
            auto later = [this](std::function<void()> action) {
              QTimer::singleShot(0, this, std::move(action));
            };
            if (command == "save") {
              on_actionSave_triggered();
            } else if (command == "saveAll") {
              for (LightpadTabWidget *tabWidget : allTabWidgets()) {
                if (!tabWidget)
                  continue;
                for (int i = 0; i < tabWidget->count(); ++i) {
                  LightpadPage *page = tabWidget->getPage(i);
                  const QString path = tabWidget->getFilePath(i);
                  if (page && page->getTextArea() && !path.isEmpty() &&
                      page->getTextArea()->changesUnsaved())
                    save(path);
                }
              }
            } else if (command == "saveAs") {
              on_actionSave_as_triggered();
            } else if (command == "quit") {
              later([this]() { closeCurrentTab(); });
            } else if (command == "closeWithoutSaving") {
              QPointer<TextArea> area(textArea);
              later([this, area]() {
                if (area)
                  area->removeIconUnsaved();
                closeCurrentTab();
              });
            } else if (command == "quitAll") {
              later([this]() { on_actionQuit_triggered(); });
            } else if (command == "forceQuit") {
              later([this]() {
                for (LightpadTabWidget *tabWidget : allTabWidgets()) {
                  for (int i = 0; tabWidget && i < tabWidget->count(); ++i) {
                    LightpadPage *page = tabWidget->getPage(i);
                    if (page && page->getTextArea())
                      page->getTextArea()->removeIconUnsaved();
                  }
                }
                on_actionQuit_triggered();
              });
            } else if (command == "newFile") {
              later([this]() { on_actionNew_File_triggered(); });
            } else if (command == "closeSplit") {
              later([this]() { on_actionClose_Editor_Group_triggered(); });
            } else if (command == "unsplitAll") {
              later([this]() { on_actionUnsplit_All_triggered(); });
            } else if (command == "focusNextSplit") {
              later([this]() { on_actionFocus_Next_Group_triggered(); });
            } else if (command == "focusPrevSplit") {
              later([this]() { on_actionFocus_Previous_Group_triggered(); });
            } else if (command == "goToDefinition") {
              goToDefinitionAtCursor();
            } else if (command == "fold") {
              on_actionFold_Current_triggered();
            } else if (command == "unfold") {
              on_actionUnfold_Current_triggered();
            } else if (command == "toggleFold") {
              if (TextArea *area = getCurrentTextArea())
                area->toggleFoldAtLine(area->textCursor().blockNumber());
            } else if (command == "foldAll") {
              on_actionFold_All_triggered();
            } else if (command == "unfoldAll") {
              on_actionUnfold_All_triggered();
            } else if (command == "vim:on") {
              if (!settings.vimModeEnabled) {
                on_actionToggle_Vim_Mode_triggered();
              }
            } else if (command == "vim:off") {
              if (settings.vimModeEnabled) {
                on_actionToggle_Vim_Mode_triggered();
              }
            } else if (command == "nextTab") {
              LightpadTabWidget *tabWidget = currentTabWidget();
              if (tabWidget && tabWidget->count() > 1) {
                tabWidget->setCurrentIndex((tabWidget->currentIndex() + 1) %
                                           (tabWidget->count() - 1));
              }
            } else if (command == "prevTab") {
              LightpadTabWidget *tabWidget = currentTabWidget();
              if (tabWidget && tabWidget->count() > 1) {
                int idx = tabWidget->currentIndex() - 1;
                if (idx < 0)
                  idx = tabWidget->count() - 2;
                tabWidget->setCurrentIndex(idx);
              }
            } else if (command == "splitHorizontal") {
              on_actionSplit_Horizontally_triggered();
            } else if (command == "splitVertical") {
              on_actionSplit_Vertically_triggered();
            } else {
              handleVimEditorCommand(textArea, command);
            }
          });

  connect(vimMode, &VimMode::pendingKeysChanged, this,
          [this, textArea](const QString &keys) {
            if (!textArea || !textArea->isVimModeEnabled())
              return;
            if (keys.isEmpty()) {
              if (vimStatusLabel)
                updateVimStatusLabel(textArea->vimMode()->modeName());
            } else {
              showVimStatusMessage(textArea->vimMode()->modeName() + "  " +
                                   keys);
            }
          });

  connect(vimMode, &VimMode::macroRecordingChanged, this,
          [this](bool recording, QChar reg) {
            if (recording) {
              showVimStatusMessage(QString("recording @%1").arg(reg));
            }
          });

  connect(vimMode, &VimMode::searchHighlightRequested, this,
          [this, textArea](const QString &pattern, bool enabled) {
            if (!textArea)
              return;
            if (enabled && !pattern.isEmpty()) {

              textArea->updateSyntaxHighlightTags(pattern);
            } else {

              textArea->updateSyntaxHighlightTags(QString());
            }
          });

  updateVimStatusLabel(textArea->isVimModeEnabled() ? vimMode->modeName() : "");
  if (!textArea->isVimModeEnabled()) {
    hideVimCommandPanel();
  }
}

void MainWindow::disconnectVimMode() {
  if (!m_connectedVimMode) {
    return;
  }
  m_connectedVimMode->disconnect(this);
  m_connectedVimMode = nullptr;
}

void MainWindow::showVimCommandPanel(const QString &prefix,
                                     const QString &buffer) {
  m_vimCommandPanelActive = true;
  if (findReplacePanel) {
    findReplacePanel->setVimCommandMode(true);
  }
  showFindReplace(true);
  if (!findReplacePanel) {
    return;
  }
  ensureStatusLabels();
  findReplacePanel->setReplaceVisibility(false);
  findReplacePanel->setVimCommandMode(true);
  findReplacePanel->setSearchPrefix(prefix);
  findReplacePanel->setSearchText(buffer);
  findReplacePanel->setFocusOnSearchBox();
}

void MainWindow::hideVimCommandPanel() {
  const bool wasActive =
      findReplacePanel && findReplacePanel->isVimCommandMode();
  if (wasActive) {
    findReplacePanel->setVimCommandMode(false);
    findReplacePanel->close();
  }
  m_vimCommandPanelActive = false;
  if (wasActive) {
    if (TextArea *textArea = getCurrentTextArea())
      textArea->setFocus();
  }
}

void MainWindow::setTabWidthLabel(QString text) { ui->tabWidth->setText(text); }

void MainWindow::setLanguageHighlightLabel(QString text) {
  ui->languageHighlight->setText(text);
}

MainWindow::~MainWindow() {
  m_globalSettingsLoaded = false;
  if (findReplacePanel)
    findReplacePanel->setMainWindow(nullptr);
  hide();
  delete m_documentRecovery;
  m_documentRecovery = nullptr;
  delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (findReplacePanel && findReplacePanel->isBusy()) {
    event->ignore();
    statusBar()->showMessage(
        tr("Wait for the replacement batch to finish, or stop it in Search."),
        5000);
    return;
  }
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }
    for (int i = 0; i < tabWidget->count(); ++i) {
      if (!maybeSaveTab(tabWidget, i)) {
        event->ignore();
        return;
      }
    }
  }

  LOG_INFO("closeEvent: saving settings before close");
  saveSettings();

  if (preferences) {
    preferences->close();
  }

  QMainWindow::closeEvent(event);
  if (event->isAccepted() && m_documentRecovery) {
    QString error;
    if (!m_documentRecovery->finishSession(&error))
      LOG_WARNING("Recovery cleanup: " + error);
  }
}

QString MainWindow::textAreaSettingsPath() const {
  QString settingsDir = SettingsManager::instance().getSettingsDirectory();
  if (!settingsDir.isEmpty()) {
    QDir dir;
    dir.mkpath(settingsDir);
    return QDir(settingsDir).filePath("editor_settings.json");
  }

  QString fallbackDir = QDir::home().filePath(".lightpad");
  QDir dir;
  dir.mkpath(fallbackDir);
  return QDir(fallbackDir).filePath("editor_settings.json");
}

void MainWindow::loadSettings() {
  QString editorSettingsPath = textAreaSettingsPath();
  if (QFileInfo(editorSettingsPath).exists()) {
    settings.loadSettings(editorSettingsPath);
  } else if (QFileInfo("settings.json").exists()) {

    settings.loadSettings("settings.json");
    settings.saveSettings(editorSettingsPath);
    LOG_INFO(QString("Migrated editor settings to global path: %1")
                 .arg(editorSettingsPath));
  } else {
    setTabWidth(defaultTabWidth);
  }

  SettingsManager &globalSettings = SettingsManager::instance();
  globalSettings.loadSettings();
  m_restoringSession = true;
  setInlineBlameEnabled(
      globalSettings.getValue("inlineBlameEnabled", false).toBool());

  m_pinnedRunFilePath =
      globalSettings.getValue("pinnedRunTarget", QString()).toString();
  m_preferredCMakeTarget =
      globalSettings.getValue("preferredCMakeTarget", QString()).toString();
  m_preferredCMakeTargetRoot =
      globalSettings.getValue("preferredCMakeTargetRoot", QString()).toString();
  invalidateRunTargetCache();
  refreshRunTargetButton();

  if (autoSaveManager) {
    autoSaveManager->setEnabled(
        globalSettings.getValue("autoSaveFiles", true).toBool());
  }
  QString fontFamily =
      globalSettings.getValue("fontFamily", "Ubuntu Mono").toString();
  int fontSize = globalSettings.getValue("fontSize", defaultFontSize).toInt();
  int fontWeight = SettingsManager::normalizeFontWeight(
      globalSettings.getValue("fontWeight", static_cast<int>(QFont::Normal))
          .toInt());
  bool fontItalic = globalSettings.getValue("fontItalic", false).toBool();
  settings.mainFont = QFont(fontFamily, fontSize, fontWeight, fontItalic);

  updateAllTextAreas(&TextArea::loadSettings, settings);
  const QJsonObject savedThemeObject =
      globalSettings.getSettingsObject().value("theme").toObject();
  if (!savedThemeObject.isEmpty()) {
    settings.theme.read(savedThemeObject);
  }
  ThemeDefinition savedThemeDefinition;
  bool hasSavedThemeDefinition = false;
  const QJsonObject savedThemeDefinitionObject =
      settingsObject(globalSettings, "activeThemeDefinition");
  if (!savedThemeDefinitionObject.isEmpty()) {
    savedThemeDefinition.read(savedThemeDefinitionObject);
    hasSavedThemeDefinition = !savedThemeDefinition.name.trimmed().isEmpty();
  }
  const QString savedThemeName =
      globalSettings.getValue("activeThemeName", QString()).toString();
  if (!savedThemeName.isEmpty() &&
      ThemeEngine::instance().hasTheme(savedThemeName)) {
    setTheme(ThemeEngine::instance().themeByName(savedThemeName));
  } else if (hasSavedThemeDefinition) {
    setTheme(savedThemeDefinition);
  } else {
    setTheme(settings.theme);
  }
  if (ui->actionToggle_Vim_Mode) {
    ui->actionToggle_Vim_Mode->setChecked(settings.vimModeEnabled);
  }

  QString lastProject =
      globalSettings.getValue("lastProjectPath", "").toString();
  if (!lastProject.isEmpty() && QDir(lastProject).exists()) {
    const QString resolvedRoot = resolveProjectRootForPath(lastProject);
    setProjectRootPath(resolvedRoot.isEmpty() ? lastProject : resolvedRoot);
    QDir::setCurrent(m_projectRootPath);
  }

  loadTreeStateFromSettings(m_projectRootPath);

  const QJsonArray openTabs = settingsArray(globalSettings, "openTabs");
  const QJsonObject savedFilePositions =
      settingsObject(globalSettings, "currentFilePositions");
  LOG_INFO(
      QString("loadSettings: openTabs has %1 entries").arg(openTabs.size()));
  for (const QJsonValue &val : openTabs) {
    QJsonObject tabState;
    QString filePath;
    if (val.isObject()) {
      tabState = val.toObject();
      filePath = tabState.value(kSessionTabPathKey).toString();
    } else {
      filePath = val.toString();
      tabState = savedFilePositions.value(filePath).toObject();
    }
    LOG_INFO(QString("loadSettings: restoring tab '%1' exists=%2")
                 .arg(filePath)
                 .arg(QFileInfo(filePath).exists()));
    if (!filePath.isEmpty() && QFileInfo(filePath).exists()) {
      openFileAndAddToNewTab(filePath);
      if (TextArea *textArea = getCurrentTextArea()) {
        QTextCursor cursor = textArea->textCursor();
        cursor.setPosition(qBound(0,
                                  tabState.value(kSessionTabCursorKey).toInt(),
                                  textArea->document()->characterCount() - 1));
        textArea->setTextCursor(cursor);
        if (QScrollBar *scrollBar = textArea->verticalScrollBar()) {
          scrollBar->setValue(
              tabState.value(kSessionTabVerticalScrollKey).toInt());
        }
        if (QScrollBar *scrollBar = textArea->horizontalScrollBar()) {
          scrollBar->setValue(
              tabState.value(kSessionTabHorizontalScrollKey).toInt());
        }
      }
    }
  }

  applyTreeExpandedStateToViews();
  restoreSessionUiState();
  m_restoringSession = false;
  m_globalSettingsLoaded = true;
}

void MainWindow::saveSettings() {
  LOG_DEBUG(QString("Saving settings, showLineNumberArea: %1")
                .arg(settings.showLineNumberArea));
  settings.saveSettings(textAreaSettingsPath());

  SettingsManager &globalSettings = SettingsManager::instance();
  globalSettings.setValue("lastProjectPath", m_projectRootPath);
  globalSettings.setValue("showSourceControlDock",
                          sourceControlDock && sourceControlDock->isVisible());
  globalSettings.setValue("showDebugDock", debugDock && debugDock->isVisible());
  globalSettings.setValue("showTerminalDock",
                          m_terminalDock && m_terminalDock->isVisible());
  globalSettings.setValue("showProblemsDock",
                          m_problemsDock && m_problemsDock->isVisible());
  globalSettings.setValue("showTestDock", testDock && testDock->isVisible());
  globalSettings.setValue("showDatabaseDock",
                          databaseDock && databaseDock->isVisible());
  globalSettings.setValue("showMarkdownPreviewDock",
                          m_markdownPreviewDock &&
                              m_markdownPreviewDock->isVisible());
  globalSettings.setValue("showLatexPreviewDock",
                          m_latexPreviewDock &&
                              m_latexPreviewDock->isVisible());
  globalSettings.setValue(
      "markdownPreviewFilePath",
      m_markdownPreviewPanel ? m_markdownPreviewPanel->filePath() : QString());
  globalSettings.setValue("latexPreviewFilePath",
                          m_latexPreviewPanel ? m_latexPreviewPanel->filePath()
                                              : QString());
  globalSettings.setValue("showFindReplacePanel",
                          findReplacePanel && findReplacePanel->isVisible());
  globalSettings.setValue("findReplaceOnlyFind",
                          !findReplacePanel || findReplacePanel->isOnlyFind());
  globalSettings.setValue("findReplaceGlobalMode",
                          findReplacePanel && findReplacePanel->isVisible() &&
                              findReplacePanel->isGlobalMode());
  globalSettings.setValue("showPreferencesDialog",
                          preferences && preferences->isVisible());
  globalSettings.setValue("showRunConfigurationDialog",
                          hasVisibleChildWidget<RunTemplateSelector>(this));
  globalSettings.setValue("showFormatConfigurationDialog",
                          hasVisibleChildWidget<FormatTemplateSelector>(this));
  globalSettings.setValue(
      "showDebugConfigurationDialog",
      hasVisibleChildWidget<DebugConfigurationDialog>(this));
  globalSettings.setValue("showTestConfigurationDialog",
                          hasVisibleChildWidget<TestConfigurationDialog>(this));
  globalSettings.setValue("showShortcutsDialog",
                          hasVisibleChildWidget<ShortcutsDialog>(this));
  globalSettings.setValue("showCommandPalette",
                          commandPalette && commandPalette->isVisible());
  globalSettings.setValue("showGoToLineDialog",
                          goToLineDialog && goToLineDialog->isVisible());
  globalSettings.setValue("showGoToSymbolDialog",
                          goToSymbolDialog && goToSymbolDialog->isVisible());
  globalSettings.setValue("showFileQuickOpen",
                          fileQuickOpen && fileQuickOpen->isVisible());
  globalSettings.setValue("showRecentFilesDialog",
                          recentFilesDialog && recentFilesDialog->isVisible());
  globalSettings.setValue(
      "mainWindowDockState",
      QString::fromLatin1(QMainWindow::saveState().toBase64()));

  QJsonArray openTabs;
  QJsonObject filePositions;
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      QString filePath = tabWidget->getFilePath(i);
      LOG_INFO(
          QString("saveSettings: tab %1 filePath='%2'").arg(i).arg(filePath));
      if (!filePath.isEmpty()) {
        QJsonObject tabState;
        tabState[kSessionTabPathKey] = filePath;
        if (LightpadPage *page = tabWidget->getPage(i)) {
          if (TextArea *textArea = page->getTextArea()) {
            tabState[kSessionTabCursorKey] = textArea->textCursor().position();
            if (QScrollBar *scrollBar = textArea->verticalScrollBar()) {
              tabState[kSessionTabVerticalScrollKey] = scrollBar->value();
            }
            if (QScrollBar *scrollBar = textArea->horizontalScrollBar()) {
              tabState[kSessionTabHorizontalScrollKey] = scrollBar->value();
            }
          }
        }
        openTabs.append(tabState);
        filePositions[filePath] = tabState;
      }
    }
  }
  LOG_INFO(QString("saveSettings: saving %1 open tabs").arg(openTabs.size()));
  globalSettings.setValue("openTabs", openTabs);
  globalSettings.setValue("currentFilePositions", filePositions);
  QJsonObject themeJson;
  settings.theme.write(themeJson);
  globalSettings.setValue("theme", themeJson);
  const ThemeDefinition activeThemeDefinition =
      ThemeEngine::instance().activeThemeSource();
  const QString activeThemeName = persistedThemeName(activeThemeDefinition);
  QJsonObject activeThemeJson;
  if (activeThemeName.isEmpty()) {
    activeThemeDefinition.write(activeThemeJson);
  }
  globalSettings.setValue("activeThemeDefinition", activeThemeJson);
  globalSettings.setValue("activeThemeName", activeThemeName);
  LightpadTabWidget *tabWidget = currentTabWidget();
  const int currentIndex = tabWidget ? tabWidget->currentIndex() : -1;
  globalSettings.setValue("currentFilePath",
                          (tabWidget && currentIndex >= 0)
                              ? tabWidget->getFilePath(currentIndex)
                              : QString());
  if (testPanel) {
    testPanel->saveState();
  }
  if (databasePanel) {
    databasePanel->saveState();
  }
  persistTreeStateToSettings();
  globalSettings.saveSettings();
}

void MainWindow::restoreSessionUiState() {
  SettingsManager &globalSettings = SettingsManager::instance();
  const QString currentFilePath =
      globalSettings.getValue("currentFilePath", "").toString();
  const bool showSourceControlDock =
      globalSettings.getValue("showSourceControlDock", true).toBool();
  const bool showDebugDock =
      globalSettings.getValue("showDebugDock", false).toBool();
  const bool showTerminalDock =
      globalSettings.getValue("showTerminalDock", false).toBool();
  const bool showProblemsDock =
      globalSettings.getValue("showProblemsDock", false).toBool();
  const bool showTestDock =
      globalSettings.getValue("showTestDock", false).toBool();
  const bool showDatabaseDock =
      globalSettings.getValue("showDatabaseDock", false).toBool();
  const QString dockStateBase64 =
      globalSettings.getValue("mainWindowDockState", "").toString();

  if (!dockStateBase64.isEmpty()) {
    ensureTerminalWidget();
    if (!m_problemsDock) {
      showProblemsPanel();
      if (m_problemsDock) {
        m_problemsDock->hide();
      }
    }
    ensureSourceControlPanel();
    ensureDebugPanel();
    ensureTestPanel();
    ensureDatabasePanel();
  }

  if (showSourceControlDock) {
    ensureSourceControlPanel();
    revealRightDock(sourceControlDock, 340);
  }

  if (showDebugDock) {
    ensureDebugPanel();
    if (debugDock) {
      debugDock->show();
    }
  }

  if (showTerminalDock) {
    showTerminalPanel();
  }

  if (showProblemsDock) {
    showProblemsPanel();
  }

  if (showTestDock) {
    ensureTestPanel();
    if (testDock) {
      testDock->show();
      testDock->raise();
    }
  }

  if (showDatabaseDock) {
    showDatabasePanel();
  }

  const QString markdownPreviewFilePath =
      globalSettings.getValue("markdownPreviewFilePath", "").toString();
  if (globalSettings.getValue("showMarkdownPreviewDock", false).toBool() &&
      !markdownPreviewFilePath.isEmpty() &&
      QFileInfo(markdownPreviewFilePath).exists()) {
    openFileAndAddToNewTab(markdownPreviewFilePath);
    on_actionPreview_Markdown_triggered();
  }

  const QString latexPreviewFilePath =
      globalSettings.getValue("latexPreviewFilePath", "").toString();
  if (globalSettings.getValue("showLatexPreviewDock", false).toBool() &&
      !latexPreviewFilePath.isEmpty() &&
      QFileInfo(latexPreviewFilePath).exists()) {
    openFileAndAddToNewTab(latexPreviewFilePath);
    previewLatexDocument();
  }

  if (!dockStateBase64.isEmpty()) {
    const bool restored = QMainWindow::restoreState(
        QByteArray::fromBase64(dockStateBase64.toLatin1()));
    if (restored && testDock)
      testDock->setProperty("initialLayoutPending", false);
    polishDockTabBars();
  }
  setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

  if (showTerminalDock) {
    ensureTerminalWidget();
    if (m_terminalDock) {
      m_terminalDock->show();
      m_terminalDock->raise();
    }
  }
  if (showProblemsDock) {
    showProblemsPanel();
  }
  if (showDebugDock) {
    ensureDebugPanel();
    if (debugDock) {
      debugDock->show();
      debugDock->raise();
    }
  }
  if (showTestDock) {
    ensureTestPanel();
    if (testDock) {
      testDock->show();
      testDock->raise();
    }
  }
  if (ui->actionToggle_Merge_Conflicts) {
    ui->actionToggle_Merge_Conflicts->setChecked(
        conflictCenterDock && conflictCenterDock->isVisible());
  }
  if (ui->actionToggle_Terminal) {
    ui->actionToggle_Terminal->setChecked(m_terminalDock &&
                                          m_terminalDock->isVisible());
  }

  if (findReplacePanel && findReplacePanel->isVisible()) {
    findReplacePanel->hide();
  }
  if (globalSettings.getValue("showFindReplacePanel", false).toBool()) {
    const bool onlyFind =
        globalSettings.getValue("findReplaceOnlyFind", true).toBool();
    showFindReplace(onlyFind);
    if (findReplacePanel) {
      findReplacePanel->setGlobalMode(
          globalSettings.getValue("findReplaceGlobalMode", false).toBool());
      findReplacePanel->setReplaceVisibility(!onlyFind);
    }
  }

  if (globalSettings.getValue("showPreferencesDialog", false).toBool()) {
    on_actionPreferences_triggered();
  }
  if (globalSettings.getValue("showRunConfigurationDialog", false).toBool()) {
    openConfigurationDialog();
  }
  if (globalSettings.getValue("showFormatConfigurationDialog", false)
          .toBool()) {
    openFormatConfigurationDialog();
  }
  if (globalSettings.getValue("showDebugConfigurationDialog", false).toBool()) {
    openDebugConfigurationDialog();
  }
  if (globalSettings.getValue("showTestConfigurationDialog", false).toBool()) {
    openTestConfigurationDialog();
  }
  if (globalSettings.getValue("showShortcutsDialog", false).toBool()) {
    openShortcutsDialog();
  }
  if (globalSettings.getValue("showCommandPalette", false).toBool()) {
    showCommandPalette();
  }
  if (globalSettings.getValue("showGoToLineDialog", false).toBool()) {
    showGoToLineDialog();
  }
  if (globalSettings.getValue("showGoToSymbolDialog", false).toBool()) {
    showGoToSymbolDialog();
  }
  if (globalSettings.getValue("showFileQuickOpen", false).toBool()) {
    showFileQuickOpen();
  }
  if (globalSettings.getValue("showRecentFilesDialog", false).toBool()) {
    showRecentFilesDialog();
  }

  if (!currentFilePath.isEmpty() && QFileInfo(currentFilePath).exists()) {
    openFileAndAddToNewTab(currentFilePath);
  }
  syncViewToggleActionStates();
}

bool MainWindow::isCurrentMinimapVisible() const {
  LightpadTabWidget *tabWidget =
      const_cast<MainWindow *>(this)->currentTabWidget();
  if (!tabWidget) {
    return false;
  }

  LightpadPage *page = qobject_cast<LightpadPage *>(tabWidget->currentWidget());
  return page && page->isMinimapVisible();
}

void MainWindow::syncViewToggleActionStates() {
  if (ui->actionToggle_Source_Control) {
    ui->actionToggle_Source_Control->setChecked(sourceControlDock &&
                                                sourceControlDock->isVisible());
  }
  if (ui->actionToggle_Terminal) {
    ui->actionToggle_Terminal->setChecked(m_terminalDock &&
                                          m_terminalDock->isVisible());
  }
  if (ui->actionToggle_Test_Panel) {
    ui->actionToggle_Test_Panel->setChecked(testDock && testDock->isVisible());
  }
  if (m_databaseToggleAction) {
    m_databaseToggleAction->setChecked(databaseDock &&
                                       databaseDock->isVisible());
  }
  if (ui->actionToggle_Problems) {
    ui->actionToggle_Problems->setChecked(m_problemsDock &&
                                          m_problemsDock->isVisible());
  }
  if (ui->actionToggle_Minimap) {
    ui->actionToggle_Minimap->setChecked(isCurrentMinimapVisible());
  }
}

void MainWindow::ensureProjectWorkspaceVisible() {
  if (m_projectRootPath.isEmpty()) {
    return;
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }

    bool hasPage = false;
    for (int i = 0; i < tabWidget->count(); i++) {
      if (tabWidget->getPage(i)) {
        hasPage = true;
        break;
      }
    }

    if (!hasPage) {
      tabWidget->addNewTab();
    }
  }
}

void MainWindow::applyLanguageOverride(const QString &languageId) {
  auto textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());

  QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (canonicalLanguageId.isEmpty()) {
    LOG_WARNING(
        QString("No canonical language ID found for: %1").arg(languageId));
    return;
  }

  if (!filePath.isEmpty()) {
    setHighlightOverrideForFile(filePath, canonicalLanguageId);
  }
  textArea->updateSyntaxHighlightTags("", canonicalLanguageId);
  textArea->setLanguage(canonicalLanguageId);
  highlightLanguage = canonicalLanguageId;
  QString displayName = LanguageCatalog::displayName(canonicalLanguageId);
  setLanguageHighlightLabel(displayName.isEmpty() ? canonicalLanguageId
                                                  : displayName);

  if (m_languageFeatureManager && !filePath.isEmpty()) {
    notifyDiagnosticsFileClosed(filePath);
    notifyDiagnosticsFileOpened(filePath);
  }
}

void MainWindow::applyHighlightForFile(const QString &filePath) {
  auto textArea = getCurrentTextArea();
  if (!textArea || filePath.isEmpty()) {
    return;
  }

  QString languageId = effectiveLanguageIdForFile(filePath);
  if (languageId.isEmpty()) {
    languageId = "plaintext";
  }

  QString extension = QFileInfo(filePath).completeSuffix().toLower();
  QString displayName = displayNameForLanguage(languageId, extension);
  if (!displayName.isEmpty()) {
    setLanguageHighlightLabel(displayName);
  }

  highlightLanguage = languageId;
  textArea->setLanguage(languageId);
  textArea->updateSyntaxHighlightTags("", languageId);

  updateGitGutterForCurrentFile(filePath);

  showGitBlameForCurrentFile(isGitBlameEnabledForFile(filePath));
  updateInlineBlameForCurrentFile();
  updateGitStatusBar();
}

QString MainWindow::effectiveLanguageIdForFile(const QString &filePath) {
  QString overrideLanguageId = highlightOverrideForFile(filePath);
  if (!overrideLanguageId.isEmpty()) {
    QString canonicalOverride = LanguageCatalog::normalize(overrideLanguageId);
    if (!canonicalOverride.isEmpty()) {
      return canonicalOverride;
    }
  }

  QString detectedLanguageId =
      LanguageCatalog::normalize(detectLanguageIdForFile(filePath));
  if (!detectedLanguageId.isEmpty()) {
    return detectedLanguageId;
  }

  return "plaintext";
}

void MainWindow::showGitBlameForCurrentFile(bool enable) {
  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  QString filePath;
  QObject *parent = textArea;
  while (parent && filePath.isEmpty()) {
    if (auto *page = qobject_cast<LightpadPage *>(parent)) {
      filePath = page->getFilePath();
      break;
    }
    parent = parent->parent();
  }
  if (filePath.isEmpty()) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    filePath = tabWidget ? tabWidget->getFilePath(tabWidget->currentIndex())
                         : QString();
  }
  if (!enable || !m_gitIntegration || filePath.isEmpty()) {
    textArea->clearGitBlameLines();
    return;
  }

  if (textArea->document()->isModified()) {
    textArea->markGitBlameOutdated();
    statusBar()->showMessage(
        tr("Save the file to refresh blame for the edited lines."), 5000);
    return;
  }

  QList<GitBlameLineInfo> blameLines = m_gitIntegration->getBlameInfo(filePath);
  if (blameLines.isEmpty()) {
    statusBar()->showMessage(tr("Git blame is unavailable for this file."),
                             5000);
  }
  QMap<int, QString> blameMap;
  QMap<int, GitBlameLineInfo> richBlameMap;
  for (const auto &info : blameLines) {
    QString label =
        QString("%1 \u2022 %2").arg(info.shortHash).arg(info.author);
    blameMap.insert(info.lineNumber, label);
    richBlameMap.insert(info.lineNumber, info);
  }
  textArea->setGitBlameLines(blameMap);
  textArea->setRichBlameData(richBlameMap);
  textArea->setGutterGitIntegration(m_gitIntegration);
}

bool MainWindow::isGitBlameEnabledForFile(const QString &filePath) const {
  return !filePath.isEmpty() && m_blameEnabledFiles.contains(filePath);
}

void MainWindow::setGitBlameEnabledForFile(const QString &filePath,
                                           bool enabled) {
  if (filePath.isEmpty()) {
    return;
  }

  if (enabled) {
    m_blameEnabledFiles.insert(filePath);
  } else {
    m_blameEnabledFiles.remove(filePath);
  }
}

void MainWindow::setInlineBlameEnabled(bool enabled) {
  m_inlineBlameEnabled = enabled;
  updateAllTextAreas(&TextArea::setInlineBlameEnabled, enabled);
  if (!enabled) {
    updateAllTextAreas(&TextArea::clearInlineBlameData);
  }
  updateInlineBlameForCurrentFile();

  auto &globalSettings = SettingsManager::instance();
  if (globalSettings.getValue("inlineBlameEnabled", false).toBool() != enabled) {
    globalSettings.setValue("inlineBlameEnabled", enabled);
    globalSettings.saveSettings();
  }
}

bool MainWindow::isInlineBlameEnabled() const { return m_inlineBlameEnabled; }

void MainWindow::updateInlineBlameForCurrentFile() {
  TextArea *textArea = getCurrentTextArea();
  if (!textArea || !m_gitIntegration || !m_inlineBlameEnabled) {
    if (textArea) {
      textArea->setInlineBlameEnabled(false);
      textArea->clearInlineBlameData();
    }
    return;
  }

  QString filePath;
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget) {
    filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  }

  if (filePath.isEmpty() || !m_gitIntegration->isValidRepository() ||
      textArea->document()->isModified()) {
    textArea->clearInlineBlameData();
    return;
  }

  QList<GitBlameLineInfo> blameLines = m_gitIntegration->getBlameInfo(filePath);
  QMap<int, QString> inlineData;
  for (const auto &info : blameLines) {
    const bool uncommitted =
        info.shortHash.isEmpty() ||
        info.shortHash.count(QLatin1Char('0')) == info.shortHash.size();
    QString text = uncommitted ? tr("You \u2022 Uncommitted changes")
                               : QString("%1, %2 \u2022 %3")
                                     .arg(info.author)
                                     .arg(info.relativeDate)
                                     .arg(info.summary);
    inlineData.insert(info.lineNumber, text);
  }
  textArea->setInlineBlameEnabled(true);
  textArea->setInlineBlameData(inlineData);
}

void MainWindow::updateGitGutterForCurrentFile(const QString &path) {
  TextArea *textArea = getCurrentTextArea();
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!textArea || (path.isEmpty() && !tabWidget)) {
    return;
  }

  const QString filePath =
      path.isEmpty() ? tabWidget->getFilePath(tabWidget->currentIndex()) : path;
  if (!m_gitIntegration || filePath.isEmpty()) {
    textArea->clearGitDiffLines();
    return;
  }

  const QList<GitDiffLineInfo> diffLines =
      m_gitIntegration->getDiffLines(filePath);
  QList<QPair<int, int>> gutterLines;
  gutterLines.reserve(diffLines.size());
  for (const auto &info : diffLines) {
    int type = 1;
    if (info.type == GitDiffLineInfo::Type::Added) {
      type = 0;
    } else if (info.type == GitDiffLineInfo::Type::Deleted) {
      type = 2;
    }
    gutterLines.append(qMakePair(info.lineNumber, type));
  }
  textArea->setGitDiffLines(gutterLines);
}

void MainWindow::updateGitStatusBar() {
  if (!m_gitIntegration) {
    return;
  }

  const auto syncVisibility = [this]() {
    for (QLabel *label : {m_gitBranchLabel, m_gitSyncLabel, m_gitDirtyLabel}) {
      label->setVisible(!label->text().isEmpty());
    }
  };

  if (!m_gitIntegration->isValidRepository()) {
    m_gitBranchLabel->clear();
    m_gitSyncLabel->clear();
    m_gitDirtyLabel->clear();
    syncVisibility();
    return;
  }

  QString branch = m_gitIntegration->currentBranch();
  m_gitBranchLabel->setText(branch.isEmpty() ? QStringLiteral("HEAD") : branch);

  int ahead = 0, behind = 0;
  if (m_gitIntegration->getAheadBehind(ahead, behind)) {
    QString syncText;
    if (ahead > 0)
      syncText += QString("\u2191%1").arg(ahead);
    if (behind > 0) {
      if (!syncText.isEmpty())
        syncText += " ";
      syncText += QString("\u2193%1").arg(behind);
    }
    if (syncText.isEmpty())
      syncText = "\u2713";
    m_gitSyncLabel->setText(syncText);
    m_gitSyncLabel->setToolTip(
        QString("Ahead: %1, Behind: %2").arg(ahead).arg(behind));
  } else {
    m_gitSyncLabel->clear();
  }

  bool dirty = m_gitIntegration->isDirty();
  m_gitDirtyLabel->setText(dirty ? "\u25CF" : "");
  m_gitDirtyLabel->setToolTip(dirty ? tr("Uncommitted changes")
                                    : tr("Working tree clean"));
  syncVisibility();
}

QString
MainWindow::detectLanguageIdForExtension(const QString &extension) const {
  return LanguageCatalog::languageForExtension(extension);
}

QString MainWindow::detectLanguageIdForFile(const QString &filePath) const {
  const QFileInfo info(filePath);
  const QString fileName = info.fileName();
  const QString fileNameLower = fileName.toLower();
  if (fileNameLower == "dockerfile" || fileNameLower == "containerfile") {
    return "dockerfile";
  }
  if (fileNameLower == "makefile" || fileNameLower == "gnumakefile") {
    return "make";
  }
  if (fileName == "BUILD" || fileName == "WORKSPACE" ||
      fileNameLower == "build.bazel" || fileNameLower == "workspace.bazel" ||
      fileNameLower == "module.bazel") {
    return "bazel";
  }
  if (fileNameLower == "meson.build" || fileNameLower == "meson_options.txt") {
    return "meson";
  }
  if (fileNameLower == "build.ninja") {
    return "ninja";
  }
  if (fileNameLower == "cmakelists.txt") {
    return "cmake";
  }
  return detectLanguageIdForExtension(info.completeSuffix().toLower());
}

QString MainWindow::displayNameForLanguage(const QString &languageId,
                                           const QString &extension) const {
  Q_UNUSED(extension);
  QString displayName = LanguageCatalog::displayName(languageId);
  if (!displayName.isEmpty()) {
    return displayName;
  }
  return languageId;
}

void MainWindow::loadHighlightOverridesForDir(const QString &dirPath) {
  if (dirPath.isEmpty()) {
    return;
  }

  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/highlight_config.json";
  if (m_loadedHighlightOverrideDirs.contains(configDir)) {
    return;
  }

  if (!QFileInfo(configFile).exists()) {
    m_loadedHighlightOverrideDirs.insert(configDir);
    return;
  }

  QFile file(configFile);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARNING(QString("Failed to open highlight config: %1").arg(configFile));
    return;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    LOG_WARNING(QString("Failed to parse highlight config: %1")
                    .arg(parseError.errorString()));
    return;
  }

  QJsonObject root = doc.object();
  QJsonArray assignments = root.value("assignments").toArray();
  for (const QJsonValue &value : assignments) {
    QJsonObject obj = value.toObject();
    QString fileName = obj.value("file").toString();
    QString languageId =
        LanguageCatalog::normalize(obj.value("language").toString());
    if (fileName.isEmpty() || languageId.isEmpty()) {
      continue;
    }
    QString absolutePath = dirPath + "/" + fileName;
    m_highlightOverrides[absolutePath] = languageId;
  }

  m_loadedHighlightOverrideDirs.insert(configDir);
  LOG_INFO(QString("Loaded %1 highlight overrides from %2")
               .arg(assignments.size())
               .arg(configFile));
}

bool MainWindow::saveHighlightOverridesForDir(const QString &dirPath) const {
  if (dirPath.isEmpty()) {
    return false;
  }

  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/highlight_config.json";

  QJsonArray assignments;
  for (auto it = m_highlightOverrides.begin(); it != m_highlightOverrides.end();
       ++it) {
    QFileInfo fileInfo(it.key());
    if (fileInfo.absoluteDir().path() == dirPath) {
      QJsonObject obj;
      obj["file"] = fileInfo.fileName();
      obj["language"] = it.value();
      assignments.append(obj);
    }
  }

  QDir dir;
  if (!dir.exists(configDir)) {
    if (!dir.mkpath(configDir)) {
      LOG_ERROR(
          QString("Failed to create config directory: %1").arg(configDir));
      return false;
    }
  }

  QJsonObject root;
  root["version"] = "1.0";
  root["assignments"] = assignments;

  QFile file(configFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to write highlight config: %1").arg(configFile));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  LOG_INFO(QString("Saved %1 highlight overrides to %2")
               .arg(assignments.size())
               .arg(configFile));
  return true;
}

QString MainWindow::highlightOverrideForFile(const QString &filePath) {
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists()) {
    return QString();
  }

  loadHighlightOverridesForDir(fileInfo.absoluteDir().path());
  auto it = m_highlightOverrides.find(filePath);
  if (it != m_highlightOverrides.end()) {
    return it.value();
  }
  return QString();
}

void MainWindow::setHighlightOverrideForFile(const QString &filePath,
                                             const QString &languageId) {
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists()) {
    return;
  }

  QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (canonicalLanguageId.isEmpty()) {
    m_highlightOverrides.remove(filePath);
  } else {
    m_highlightOverrides[filePath] = canonicalLanguageId;
  }
  saveHighlightOverridesForDir(fileInfo.absoluteDir().path());
}

void MainWindow::ensureProjectSettings(const QString &path) {
  if (path.isEmpty()) {
    return;
  }

  QFileInfo rootInfo(path);
  if (!rootInfo.exists() || !rootInfo.isDir()) {
    LOG_WARNING(
        QString("Skipping project settings initialization for invalid path: %1")
            .arg(path));
    return;
  }

  QDir rootDir(path);
  QFileInfo configInfo(rootDir.filePath(".lightpad"));
  if (configInfo.exists() && !configInfo.isDir()) {
    LOG_ERROR(QString(".lightpad exists but is not a directory: %1")
                  .arg(configInfo.absoluteFilePath()));
    return;
  }

  if (!rootDir.exists(".lightpad")) {
    if (!rootDir.mkpath(".lightpad")) {
      LOG_ERROR(QString("Failed to create project config directory: %1")
                    .arg(path + "/.lightpad"));
      return;
    }
  }

  QString configDir = rootDir.filePath(".lightpad");

  QString highlightConfigPath = configDir + "/highlight_config.json";
  if (!QFileInfo(highlightConfigPath).exists()) {
    saveHighlightOverridesForDir(path);
  }

  DebugSettings::instance().initialize(path);
}

QString MainWindow::resolveProjectRootForPath(const QString &path) const {
  if (path.isEmpty()) {
    return QString();
  }

  QFileInfo pathInfo(path);
  QString startDirPath;
  if (pathInfo.exists() && pathInfo.isDir()) {
    startDirPath = pathInfo.absoluteFilePath();
  } else if (pathInfo.exists()) {
    startDirPath = pathInfo.absolutePath();
  } else {
    QFileInfo absoluteInfo(QDir::current().absoluteFilePath(path));
    startDirPath = absoluteInfo.isDir() ? absoluteInfo.absoluteFilePath()
                                        : absoluteInfo.absolutePath();
  }

  if (startDirPath.isEmpty()) {
    return QString();
  }

  const QString homePath = QDir::cleanPath(QDir::homePath());

  QDir dir(startDirPath);
  QString outermostLightpadRoot;
  QString innermostBuildRoot;

  while (dir.exists()) {
    const QString current = QDir::cleanPath(dir.absolutePath());

    const bool atHome = current == homePath;

    QFileInfo gitInfo(dir.filePath(".git"));
    if (gitInfo.exists() && !atHome) {
      return current;
    }

    if (innermostBuildRoot.isEmpty() && !atHome &&
        QFileInfo::exists(dir.filePath("CMakeLists.txt"))) {
      innermostBuildRoot = current;
    }

    QFileInfo lightpadInfo(dir.filePath(".lightpad"));
    if (lightpadInfo.exists() && lightpadInfo.isDir() && !atHome) {
      outermostLightpadRoot = current;
    }

    if (atHome || !dir.cdUp()) {
      break;
    }
  }

  if (!outermostLightpadRoot.isEmpty()) {
    return outermostLightpadRoot;
  }

  if (!innermostBuildRoot.isEmpty()) {
    return innermostBuildRoot;
  }

  return QDir::cleanPath(startDirPath);
}

bool MainWindow::isPathWithinRoot(const QString &path,
                                  const QString &rootPath) const {
  if (path.isEmpty() || rootPath.isEmpty()) {
    return false;
  }

  QFileInfo pathInfo(path);
  QString normalizedPath =
      pathInfo.isDir() ? pathInfo.absoluteFilePath() : pathInfo.absolutePath();
  normalizedPath = QDir::cleanPath(normalizedPath);
  const QString normalizedRoot = QDir::cleanPath(rootPath);

  if (normalizedPath == normalizedRoot) {
    return true;
  }

#ifdef Q_OS_WIN
  return normalizedPath.startsWith(normalizedRoot + "/") ||
         normalizedPath.startsWith(normalizedRoot + "\\");
#else
  return normalizedPath.startsWith(normalizedRoot + "/");
#endif
}

void MainWindow::ensureProjectRootForPath(const QString &path) {
  const QString resolvedRoot = resolveProjectRootForPath(path);
  if (resolvedRoot.isEmpty()) {
    return;
  }

  const QString normalizedCurrent = QDir::cleanPath(m_projectRootPath);
  const QString normalizedResolved = QDir::cleanPath(resolvedRoot);
  if (normalizedCurrent == normalizedResolved) {
    return;
  }

  if (normalizedCurrent.isEmpty()) {
    setProjectRootPath(normalizedResolved);
    return;
  }

  if (isPathWithinRoot(path, normalizedCurrent)) {
    return;
  }

  QFileInfo gitInfo(normalizedResolved + "/.git");
  if (gitInfo.exists() &&
      isPathWithinRoot(normalizedCurrent, normalizedResolved)) {
    setProjectRootPath(normalizedResolved);
    return;
  }
}

void MainWindow::updateAllTextAreas(void (TextArea::*f)(const Theme &),
                                    const Theme &theme) {
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    auto textAreas = tabWidget->findChildren<TextArea *>();
    for (auto &textArea : textAreas)
      (textArea->*f)(theme);
  }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == problemsStatusLabel &&
      event->type() == QEvent::MouseButtonPress) {
    showProblemsPanel();
    return true;
  }

  const bool isTrackedSessionWidget =
      watched == preferences || watched == findReplacePanel ||
      watched == commandPalette || watched == goToLineDialog ||
      watched == goToSymbolDialog || watched == fileQuickOpen ||
      watched == recentFilesDialog ||
      qobject_cast<RunTemplateSelector *>(watched) ||
      qobject_cast<FormatTemplateSelector *>(watched) ||
      qobject_cast<DebugConfigurationDialog *>(watched) ||
      qobject_cast<ShortcutsDialog *>(watched);
  const bool isVisibilityEvent = event->type() == QEvent::Show ||
                                 event->type() == QEvent::Hide ||
                                 event->type() == QEvent::Close;
  if (m_globalSettingsLoaded && !m_restoringSession && isTrackedSessionWidget &&
      isVisibilityEvent) {
    saveSettings();
  }

  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *keyEvent) {

  if (keyEvent->matches(QKeySequence::Undo))
    undo();

  else if (keyEvent->matches(QKeySequence::Redo))
    redo();

  else if (keyEvent->matches(QKeySequence::ZoomIn))
    on_actionIncrease_Font_Size_triggered();

  else if (keyEvent->matches(QKeySequence::ZoomOut))
    on_actionDecrease_Font_Size_triggered();

  else if (keyEvent->matches(QKeySequence::Save))
    on_actionSave_triggered();

  else if (keyEvent->matches(QKeySequence::SaveAs))
    on_actionSave_as_triggered();

  else if (keyEvent->matches(QKeySequence::Find))
    showFindReplace();

  else if (keyEvent->matches(QKeySequence::Replace))
    showFindReplace(false);

  else if (keyEvent->key() == Qt::Key_F3 && findReplacePanel &&
           !findReplacePanel->isVimCommandMode()) {
    if (keyEvent->modifiers().testFlag(Qt::ShiftModifier))
      findReplacePanel->findPrevious();
    else
      findReplacePanel->findNext();
  }

  else if (keyEvent->key() == Qt::Key_Escape && m_vimCommandPanelActive) {
    hideVimCommandPanel();
  }

  else if (keyEvent->matches(QKeySequence::Close))
    closeCurrentTab();

  else if (keyEvent->matches(QKeySequence::AddTab))
    currentTabWidget()->addNewTab();

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_P) {
    showCommandPalette();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_G) {
    showGoToLineDialog();
  }

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_O) {
    showGoToSymbolDialog();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_P) {
    showFileQuickOpen();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_E) {
    showRecentFilesDialog();
  }

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_W) {
    toggleShowWhitespace();
  }

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_I) {
    toggleShowIndentGuides();
  }

  else if (keyEvent->key() == Qt::Key_F12 &&
           keyEvent->modifiers() == Qt::NoModifier) {
    goToDefinitionAtCursor();
  }

  else if (keyEvent->key() == Qt::Key_F2 &&
           keyEvent->modifiers() == Qt::NoModifier) {
    renameSymbolAtCursor();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_B) {
    goToDefinitionAtCursor();
  }

  else if (keyEvent->modifiers() == Qt::AltModifier &&
           keyEvent->key() == Qt::Key_Left) {
    navigateBack();
  }

  else if (keyEvent->modifiers() == Qt::AltModifier &&
           keyEvent->key() == Qt::Key_Right) {
    navigateForward();
  }
}

int MainWindow::getTabWidth() { return settings.tabWidth; }

int MainWindow::getFontSize() { return settings.mainFont.pointSize(); }

void MainWindow::openFileAndAddToNewTab(QString filePath, bool reuseAnyGroup) {
  LightpadTabWidget *tabWidget = currentTabWidget();

  QFileInfo fileInfo(filePath);
  if (filePath.isEmpty() || !fileInfo.exists() || fileInfo.isDir())
    return;

  if (!m_restoringSession) {
    ensureProjectRootForPath(filePath);
  }

  if (m_projectRootPath.isEmpty()) {
    updateGitIntegrationForPath(filePath);
  }

  const QList<LightpadTabWidget *> searchedGroups =
      reuseAnyGroup ? allTabWidgets() : QList<LightpadTabWidget *>{tabWidget};
  const QString cleanFilePath = QDir::cleanPath(filePath);
  bool closedConflictedTab = false;
  for (LightpadTabWidget *group : searchedGroups) {
    if (!group || closedConflictedTab) {
      continue;
    }
    for (int i = 0; i < group->count(); i++) {
      const QString tabFilePath = group->getFilePath(i);
      if (tabFilePath.isEmpty() ||
          QDir::cleanPath(tabFilePath) != cleanFilePath) {
        continue;
      }

      if (!group->isViewerTab(i) && isConflictedPath(filePath)) {
        LightpadPage *openPage = group->getPage(i);
        TextArea *openArea = openPage ? openPage->getTextArea() : nullptr;
        if (!openArea || !openArea->changesUnsaved()) {
          group->forceCloseTab(i);
          closedConflictedTab = true;
          break;
        }
      }

      if (m_splitEditorContainer) {
        m_splitEditorContainer->setCurrentTabWidget(group);
      }
      group->setCurrentIndex(i);
      return;
    }
  }

  QString extension = fileInfo.suffix().toLower();

  if (ImageViewer::isSupportedImageFormat(extension)) {
    ImageViewer *imageViewer = new ImageViewer(this);
    if (imageViewer->loadImage(filePath)) {
      tabWidget->addViewerTab(imageViewer, filePath, m_projectRootPath);
    } else {
      delete imageViewer;
    }
    return;
  }

  if (openConflictResolver(filePath)) {
    return;
  }

  const FileOpenChoice choice = confirmLargeOrBinaryOpen(filePath);
  if (choice == FileOpenChoice::Cancel) {
    return;
  }
  if (choice == FileOpenChoice::Preview) {
    m_previewOnlyFiles.insert(filePath);
  } else {
    m_previewOnlyFiles.remove(filePath);
  }

#ifdef HAVE_PDF_SUPPORT

  if (PdfViewer::isSupportedPdfFormat(extension)) {
    PdfViewer *pdfViewer = new PdfViewer(this);
    if (pdfViewer->loadPdf(filePath)) {
      tabWidget->addViewerTab(pdfViewer, filePath, m_projectRootPath);
    } else {
      delete pdfViewer;
    }
    return;
  }
#endif

  TextArea *currentTextArea = getCurrentTextArea();
  bool currentIsViewer = tabWidget->isViewerTab(tabWidget->currentIndex());
  if (tabWidget->count() == 0 || currentIsViewer || !currentTextArea ||
      !currentTextArea->toPlainText().isEmpty()) {
    tabWidget->addNewTab();
  }

  open(filePath);
  setFilePathAsTabText(filePath);

  auto page = tabWidget->getCurrentPage();

  if (page) {

    if (!m_projectRootPath.isEmpty()) {
      page->setProjectRootPath(m_projectRootPath);
      page->setModelRootIndex(m_projectRootPath);
    }
    page->setTreeViewVisible(!m_projectRootPath.isEmpty());
    page->setFilePath(filePath);
  }

  if (getCurrentTextArea())
    applyHighlightForFile(filePath);

  notifyDiagnosticsFileOpened(filePath);

  if (problemsPanel) {
    problemsPanel->setCurrentFilePath(filePath);
  }

  if (recentFilesManager) {
    recentFilesManager->addFile(filePath);
  }

  updateBreadcrumb(filePath);

  tabWidget->currentChanged(tabWidget->currentIndex());
  if (!m_restoringSession) {
    saveSettings();
  }
}

void MainWindow::closeTabPage(QString filePath) {
  m_previewOnlyFiles.remove(filePath);
  const QString removedPath = QDir::cleanPath(filePath);

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = tabWidget->count() - 1; i >= 0; --i) {
      const QString tabPath = tabWidget->getFilePath(i);
      if (tabPath.isEmpty()) {
        continue;
      }
      const QString cleanTabPath = QDir::cleanPath(tabPath);
      if (cleanTabPath == removedPath ||
          cleanTabPath.startsWith(removedPath + QLatin1Char('/'))) {
        m_previewOnlyFiles.remove(tabPath);
        tabWidget->forceCloseTab(i);
      }
    }
  }
  if (!m_restoringSession) {
    saveSettings();
  }
}

void MainWindow::retargetOpenPaths(const QString &oldPath,
                                   const QString &newPath) {
  const QString oldClean = QDir::cleanPath(oldPath);
  const QString newClean = QDir::cleanPath(newPath);
  bool changed = false;
  const auto retarget = [&](const QString &path) {
    const QString clean = QDir::cleanPath(path);
    return clean == oldClean || clean.startsWith(oldClean + '/')
               ? newClean + clean.mid(oldClean.size())
               : path;
  };
  QSet<QString> expanded;
  for (const QString &path : m_treeExpandedPaths)
    expanded.insert(retarget(path));
  m_treeExpandedPaths = expanded;
  if (!m_treeCurrentPath.isEmpty())
    m_treeCurrentPath = retarget(m_treeCurrentPath);
  if (!m_treeSearchOptions.scopePath.isEmpty()) {
    auto options = m_treeSearchOptions;
    options.scopePath = retarget(options.scopePath);
    if (!isPathWithinRoot(options.scopePath, m_projectRootPath))
      options.scopePath.clear();
    setTreeSearchOptions(options);
  }
  if (navigationHistory)
    navigationHistory->retargetPaths(oldPath, newPath);
  if (recentFilesManager) {
    for (const QString &recent : recentFilesManager->recentFiles()) {
      const QString updated = retarget(recent);
      if (updated != recent) {
        recentFilesManager->removeFile(recent);
        recentFilesManager->addFile(updated);
      }
    }
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); ++i) {
      const QString tabPath = tabWidget->getFilePath(i);
      if (tabPath.isEmpty()) {
        continue;
      }
      const QString cleanTabPath = QDir::cleanPath(tabPath);
      QString retargeted;
      if (cleanTabPath == oldClean) {
        retargeted = newClean;
      } else if (cleanTabPath.startsWith(oldClean + QLatin1Char('/'))) {
        retargeted = newClean + cleanTabPath.mid(oldClean.size());
      } else {
        continue;
      }

      tabWidget->setFilePath(i, retargeted);
      tabWidget->setTabText(i, QFileInfo(retargeted).fileName());
      if (m_previewOnlyFiles.remove(tabPath)) {
        m_previewOnlyFiles.insert(retargeted);
      }
      unwatchOpenFileIfUnused(tabPath);
      recordFileTimestamp(retargeted);
      watchOpenFile(retargeted);
      if (auto *page = tabWidget->getPage(i))
        trackRecoveryEditor(page->getTextArea());
      if (autoSaveManager) {
        autoSaveManager->markSaved(tabPath);
        LightpadPage *page = tabWidget->getPage(i);
        TextArea *textArea = page ? page->getTextArea() : nullptr;
        if (textArea && textArea->document()->isModified()) {
          autoSaveManager->markModified(retargeted);
        }
      }
      changed = true;
    }
  }

  if (changed && !m_restoringSession) {
    saveSettings();
  }
}

void MainWindow::openPathsFromCommandLine(const QStringList &paths) {
  for (const QString &path : paths) {
    QFileInfo info(path);
    if (!info.exists()) {
      continue;
    }

    if (info.isDir()) {
      const QString newRoot = info.absoluteFilePath();
      for (LightpadTabWidget *tabWidget : allTabWidgets()) {
        for (int i = tabWidget->count() - 1; i >= 0; --i) {
          const QString tabPath = tabWidget->getFilePath(i);
          if (tabPath.isEmpty() ||
              FileDirTreeModel::isInside(newRoot, tabPath)) {
            continue;
          }
          LightpadPage *page = tabWidget->getPage(i);
          if (page && page->getTextArea() &&
              page->getTextArea()->changesUnsaved()) {
            continue;
          }
          tabWidget->forceCloseTab(i);
        }
      }
      setProjectRootPath(newRoot);
      QDir::setCurrent(info.absoluteFilePath());
      setMainWindowTitle(info.fileName());
      if (fileQuickOpen) {
        fileQuickOpen->setRootDirectory(info.absoluteFilePath());
      }
    } else {
      openFileAndAddToNewTab(info.absoluteFilePath());
    }
  }
}

void MainWindow::on_actionToggle_Full_Screen_triggered() {
  const auto state = windowState();
  if (state & Qt::WindowFullScreen)
    setWindowState(state & ~Qt::WindowFullScreen);
  else
    setWindowState(state | Qt::WindowFullScreen);
}

void MainWindow::on_actionQuit_triggered() { close(); }

void MainWindow::undo() {
  if (getCurrentTextArea())
    getCurrentTextArea()->undo();
}

void MainWindow::redo() {
  if (getCurrentTextArea())
    getCurrentTextArea()->redo();
}

TextArea *MainWindow::getCurrentTextArea() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget->currentWidget()->findChild<LightpadPage *>("widget"))
    return tabWidget->currentWidget()
        ->findChild<LightpadPage *>("widget")
        ->getTextArea();

  else if (tabWidget->currentWidget()->findChild<TextArea *>(""))
    return tabWidget->currentWidget()->findChild<TextArea *>("");

  return nullptr;
}

Theme MainWindow::getTheme() { return ThemeEngine::instance().classicTheme(); }

QFont MainWindow::getFont() { return settings.mainFont; }

TextAreaSettings MainWindow::getSettings() { return settings; }

void MainWindow::setTabWidth(int width) {
  updateAllTextAreas(&TextArea::setTabWidth, width);
  settings.tabWidth = width;
  saveSettings();
}

void MainWindow::on_actionToggle_Undo_triggered() { undo(); }

void MainWindow::on_actionToggle_Redo_triggered() { redo(); }

void MainWindow::on_actionIncrease_Font_Size_triggered() {
  stepEditorFontSize(1);
}

void MainWindow::on_actionDecrease_Font_Size_triggered() {
  stepEditorFontSize(-1);
}

void MainWindow::stepEditorFontSize(int steps) {
  if (steps == 0) {
    return;
  }
  const auto step =
      steps > 0 ? &TextArea::increaseFontSize : &TextArea::decreaseFontSize;
  for (int i = 0; i < qAbs(steps); ++i) {
    updateAllTextAreas(step);
  }
  if (!getCurrentTextArea()) {
    return;
  }
  settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
  SettingsManager::instance().setValue("fontSize",
                                       settings.mainFont.pointSize());
  SettingsManager::instance().saveSettings();
  settings.saveSettings(textAreaSettingsPath());
}

void MainWindow::on_actionReset_Font_Size_triggered() {
  updateAllTextAreas(&TextArea::setFontSize, defaultFontSize);
  if (!getCurrentTextArea()) {
    return;
  }
  settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
  SettingsManager::instance().setValue("fontSize",
                                       settings.mainFont.pointSize());
  SettingsManager::instance().saveSettings();
  settings.saveSettings(textAreaSettingsPath());
}

void MainWindow::on_actionCut_triggered() {

  if (getCurrentTextArea())
    getCurrentTextArea()->cut();
}

void MainWindow::on_actionCopy_triggered() {

  if (getCurrentTextArea())
    getCurrentTextArea()->copy();
}

void MainWindow::on_actionPaste_triggered() {

  if (getCurrentTextArea())
    getCurrentTextArea()->paste();
}

void MainWindow::on_actionNew_Window_triggered() {
  auto *window = new MainWindow();
  window->setAttribute(Qt::WA_DeleteOnClose);
}

void MainWindow::on_actionClose_Tab_triggered() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget) {
    tabWidget->requestCloseTab(tabWidget->currentIndex());
  }
}

void MainWindow::on_actionClose_All_Tabs_triggered() {
  currentTabWidget()->closeAllTabs();
}

void MainWindow::on_actionFind_in_file_triggered() {
  showFindReplace(true);
  if (findReplacePanel) {
    findReplacePanel->setGlobalMode(false);

    if (auto *area = getCurrentTextArea()) {
      const QString selected = area->textCursor().selectedText();
      if (!selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator))
        findReplacePanel->setSearchText(selected);
    }

    TextArea *textArea = getCurrentTextArea();
    if (textArea && textArea->isVimModeEnabled() && textArea->vimMode()) {
      QString vimPattern = textArea->vimMode()->searchPattern();
      if (!vimPattern.isEmpty()) {
        QString searchTerm = vimPattern;
        searchTerm.remove("\\<");
        searchTerm.remove("\\>");
        searchTerm.remove("\\V");
        findReplacePanel->setSearchText(searchTerm);
      }
    }
    findReplacePanel->setFocusOnSearchBox();
  }
}

void MainWindow::on_actionFind_in_project_triggered() {
  showFindReplace(true);
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

void MainWindow::on_actionNew_File_triggered() {
  currentTabWidget()->addNewTab();

  if (auto *textArea = getCurrentTextArea()) {
    textArea->setFocus(Qt::OtherFocusReason);
  }
}

void MainWindow::on_actionOpen_File_triggered() {
  auto filePath =
      QFileDialog::getOpenFileName(this, tr("Open Document"), QDir::homePath());

  openFileAndAddToNewTab(filePath);
}

void MainWindow::on_actionOpen_Project_triggered() {
  QString folderPath = QFileDialog::getExistingDirectory(
      this, tr("Open Project"), QDir::homePath(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (folderPath.isEmpty()) {
    return;
  }

  const QString normalizedCurrentRoot = QDir::cleanPath(m_projectRootPath);
  const QString normalizedNextRoot = QDir::cleanPath(folderPath);
  bool replacingProject = false;
  if (!normalizedCurrentRoot.isEmpty() &&
      normalizedCurrentRoot != normalizedNextRoot) {
    ThemedMessageBox confirmBox(this);
    confirmBox.setWindowTitle(tr("Replace Project"));
    confirmBox.setIcon(ThemedMessageBox::Question);
    confirmBox.setText(
        tr("A project is already open. Replace it with the selected project?"));
    confirmBox.setInformativeText(
        tr("Open editors from the current project will be closed and the "
           "workspace view will be reset."));
    confirmBox.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No);
    confirmBox.setDefaultButton(ThemedMessageBox::No);

    if (confirmBox.exec() != ThemedMessageBox::Yes) {
      return;
    }

    for (LightpadTabWidget *tabWidget : allTabWidgets()) {
      if (tabWidget && !tabWidget->closeAllTabs()) {
        return;
      }
    }

    if (m_splitEditorContainer && m_splitEditorContainer->hasSplits() &&
        !m_splitEditorContainer->unsplitAll()) {
      return;
    }

    m_treeCurrentPath.clear();
    m_treeScrollValue = 0;
    m_treeScrollValueInitialized = false;
    replacingProject = true;
  }

  setProjectRootPath(folderPath);

  QDir::setCurrent(folderPath);
  if (terminalWidget) {
    if (replacingProject) {
      terminalWidget->resetForWorkspace(folderPath);
    } else {
      terminalWidget->setWorkingDirectory(folderPath);
    }
  }
  setMainWindowTitle(QFileInfo(folderPath).fileName());
  if (fileQuickOpen) {
    fileQuickOpen->setRootDirectory(folderPath);
  }
  if (m_globalSettingsLoaded && !m_restoringSession) {
    saveSettings();
  }
}

void MainWindow::on_actionSave_triggered() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  auto filePath = tabWidget->getFilePath(tabIndex);

  if (filePath.isEmpty()) {
    on_actionSave_as_triggered();
    return;
  }

  save(filePath);
}

void MainWindow::on_actionSave_as_triggered() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget) {
    saveTabAs(tabWidget, tabWidget->currentIndex());
  }
}

bool MainWindow::saveTab(LightpadTabWidget *tabWidget, int tabIndex) {
  if (!tabWidget || tabIndex < 0) {
    return false;
  }
  const QString filePath = tabWidget->getFilePath(tabIndex);
  if (filePath.isEmpty()) {
    return saveTabAs(tabWidget, tabIndex);
  }
  return save(filePath);
}

bool MainWindow::saveTabAs(LightpadTabWidget *tabWidget, int tabIndex,
                           const QString &presetPath) {
  if (!tabWidget || tabIndex < 0) {
    return false;
  }
  if (findReplacePanel && findReplacePanel->isBusy()) {
    statusBar()->showMessage(
        tr("Wait for the replacement batch before saving."), 5000);
    return false;
  }

  QString filePath = presetPath;
  if (filePath.isEmpty())
    filePath = QFileDialog::getSaveFileName(this, tr("Save Document"),
                                            QDir::homePath());

  if (filePath.isEmpty())
    return false;

  const QString previousPath = tabWidget->getFilePath(tabIndex);
  const QString cleanPreviousPath = QDir::cleanPath(previousPath);
  const QString cleanNewPath = QDir::cleanPath(filePath);
  const auto identity = [](const QString &path) {
    const QFileInfo info(path);
    return info.canonicalFilePath().isEmpty() ? info.absoluteFilePath()
                                              : info.canonicalFilePath();
  };
  auto *savingPage = tabWidget->getPage(tabIndex);
  for (auto *group : allTabWidgets()) {
    for (int i = 0; i < group->count(); ++i) {
      const QString otherPath = group->getFilePath(i);
      if (group->getPage(i) != savingPage && !otherPath.isEmpty() &&
          identity(otherPath) == identity(filePath)) {
        ThemedMessageBox::warning(
            this, tr("File Already Open"),
            tr("This destination is open in another editor. Choose a different "
               "path or close that editor before saving here."));
        return false;
      }
    }
  }
  const bool hadFormat = m_fileFormats.contains(cleanNewPath);
  const auto previousFormat = m_fileFormats.value(cleanNewPath);
  if (previousPath.isEmpty() && savingPage &&
      savingPage->getTextArea()->property("recoveryFormat").isValid()) {
    const auto data =
        savingPage->getTextArea()->property("recoveryFormat").toMap();
    m_fileFormats.insert(
        cleanNewPath,
        {FileOpenGuard::TextEncoding(data.value("encoding").toInt()),
         data.value("bom").toBool(), data.value("lineEnding").toString()});
  }
  if (!previousPath.isEmpty() && m_fileFormats.contains(cleanPreviousPath) &&
      !m_fileFormats.contains(cleanNewPath)) {
    m_fileFormats.insert(cleanNewPath, m_fileFormats.value(cleanPreviousPath));
  }

  tabWidget->setFilePath(tabIndex, filePath);

  if (!save(filePath)) {
    tabWidget->setFilePath(tabIndex, previousPath);
    if (hadFormat)
      m_fileFormats.insert(cleanNewPath, previousFormat);
    else
      m_fileFormats.remove(cleanNewPath);
    if (savingPage)
      trackRecoveryEditor(savingPage->getTextArea());
    return false;
  }

  if (tabWidget == currentTabWidget() &&
      tabIndex == tabWidget->currentIndex()) {
    applyHighlightForFile(filePath);
  }
  notifyDiagnosticsFileOpened(filePath);
  return true;
}

bool MainWindow::maybeSaveTab(LightpadTabWidget *tabWidget, int tabIndex) {
  if (!tabWidget) {
    return true;
  }
  LightpadPage *page = tabWidget->getPage(tabIndex);
  TextArea *textArea = page ? page->getTextArea() : nullptr;
  if (!textArea || !textArea->changesUnsaved()) {
    return true;
  }

  if (m_splitEditorContainer) {
    m_splitEditorContainer->setCurrentTabWidget(tabWidget);
  }
  tabWidget->setCurrentIndex(tabIndex);

  const QString filePath = tabWidget->getFilePath(tabIndex);
  const QString displayName = filePath.isEmpty()
                                  ? tabWidget->tabText(tabIndex)
                                  : QFileInfo(filePath).fileName();

  ThemedMessageBox msgBox(this);
  msgBox.setWindowTitle(tr("Unsaved Changes"));
  msgBox.setIcon(ThemedMessageBox::Warning);
  msgBox.setText(
      tr("Save changes to <b>%1</b> before closing?").arg(displayName));
  msgBox.setInformativeText(
      tr("Your changes will be lost if you don't save them."));
  msgBox.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No |
                            ThemedMessageBox::Cancel);
  msgBox.setButtonText(ThemedMessageBox::Yes, tr("Save"));
  msgBox.setButtonText(ThemedMessageBox::No, tr("Don't Save"));
  msgBox.setDefaultButton(ThemedMessageBox::Yes);
  const int result = msgBox.exec();

  if (result == ThemedMessageBox::No) {
    return true;
  }
  if (result != ThemedMessageBox::Yes) {
    return false;
  }

  const int currentPosition = tabWidget->indexOf(page);
  if (currentPosition < 0) {
    return true;
  }
  return saveTab(tabWidget, currentPosition) && !textArea->changesUnsaved();
}

bool MainWindow::isPreviewOnlyFile(const QString &filePath) const {
  if (m_previewOnlyFiles.contains(filePath)) {
    return true;
  }
  const QString normalizedPath = QDir::cleanPath(filePath);
  for (const QString &previewPath : m_previewOnlyFiles) {
    if (QDir::cleanPath(previewPath) == normalizedPath) {
      return true;
    }
  }
  return false;
}

void MainWindow::open(const QString &filePath) {

  const bool previewOnly = m_previewOnlyFiles.contains(filePath);

  bool ok = false;
  bool truncated = false;
  FileOpenGuard::TextFormat format;
  const QString content = FileOpenGuard::readTextCapped(
      filePath, previewOnly ? FileOpenGuard::PreviewBytes : 0, &truncated, &ok,
      &format);

  if (!ok) {
    ThemedMessageBox::critical(this, tr("Error"), tr("Can't open file."));
    return;
  }

  m_fileFormats.insert(QDir::cleanPath(filePath), format);
  if (format.encoding == FileOpenGuard::TextEncoding::Latin1) {
    statusBar()->showMessage(
        tr("%1 is not valid UTF-8, so it was opened as Latin-1 and will be "
           "saved the same way.")
            .arg(QFileInfo(filePath).fileName()),
        8000);
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  int tabIndex = tabWidget->currentIndex();
  tabWidget->setFilePath(tabIndex, filePath);

  if (getCurrentTextArea()) {
    auto *textArea = getCurrentTextArea();
    textArea->setPlainText(content);
    textArea->moveCursor(QTextCursor::Start);
    textArea->centerCursor();

    textArea->setReadOnly(truncated);
    if (truncated) {
      textArea->setToolTip(
          tr("Preview only - the first %1 of this file, read-only.")
              .arg(FileOpenGuard::formatSize(FileOpenGuard::PreviewBytes)));
    }
  }

  recordFileTimestamp(filePath);
  watchOpenFile(filePath);
  trackRecoveryEditor(getCurrentTextArea());
}

bool MainWindow::save(const QString &filePath, bool isAutoSave) {
  if (filePath.isEmpty()) {
    return false;
  }

  if (m_previewOnlyFiles.contains(filePath)) {
    if (!isAutoSave) {
      ThemedMessageBox::warning(
          this, tr("This is only a preview"),
          tr("Lightpad loaded just the first part of %1, so saving would throw "
             "away everything after it. The file has been left untouched.")
              .arg(QFileInfo(filePath).fileName()));
    }
    return false;
  }

  TextArea *textArea = nullptr;
  LightpadTabWidget *targetTabWidget = nullptr;
  int targetTabIndex = -1;

  LightpadTabWidget *currentWidget = currentTabWidget();
  if (currentWidget) {
    int currentIndex = currentWidget->currentIndex();
    if (currentIndex >= 0 &&
        currentWidget->getFilePath(currentIndex) == filePath) {
      LightpadPage *page = currentWidget->getPage(currentIndex);
      if (page && page->getTextArea()) {
        textArea = page->getTextArea();
        targetTabWidget = currentWidget;
        targetTabIndex = currentIndex;
      }
    }
  }

  if (!textArea) {
    for (LightpadTabWidget *tabWidget : allTabWidgets()) {
      if (!tabWidget) {
        continue;
      }
      for (int i = 0; i < tabWidget->count(); ++i) {
        if (tabWidget->getFilePath(i) != filePath) {
          continue;
        }
        LightpadPage *page = tabWidget->getPage(i);
        if (!page || !page->getTextArea()) {
          continue;
        }
        textArea = page->getTextArea();
        targetTabWidget = tabWidget;
        targetTabIndex = i;
        break;
      }
      if (textArea) {
        break;
      }
    }
  }

  if (!textArea || !targetTabWidget || targetTabIndex < 0) {
    return false;
  }
  if (textArea->isReadOnly() ||
      (findReplacePanel && findReplacePanel->isBusy())) {
    if (!isAutoSave)
      statusBar()->showMessage(
          tr("This editor is read-only or a replacement batch is active."),
          5000);
    return false;
  }

  if (checkExternalModification(filePath)) {
    if (isAutoSave) {

      return false;
    }

    if (!handleExternalModification(filePath, false)) {
      return false;
    }
  }

  SettingsManager &sm = SettingsManager::instance();
  QTextCursor saveEdits(textArea->document());
  saveEdits.beginEditBlock();
  if (sm.getValue("trimTrailingWhitespace", false).toBool()) {
    trimTrailingWhitespace(textArea);
  }
  if (sm.getValue("insertFinalNewline", false).toBool()) {
    ensureFinalNewline(textArea);
  }

  targetTabWidget->setFilePath(targetTabIndex, filePath);

  const QString normalizedSavePath = QDir::cleanPath(filePath);
  if (QFileInfo(filePath).fileName().compare(QStringLiteral("CMakeLists.txt"),
                                             Qt::CaseInsensitive) == 0) {

    invalidateRunTargetCache();
  }
  m_internalFileWrites.insert(normalizedSavePath);
  QTimer::singleShot(1000, this, [this, normalizedSavePath]() {
    m_internalFileWrites.remove(normalizedSavePath);
  });
  const QString languageId = effectiveLanguageIdForFile(filePath);
  const QString textToSave = normalizeEditorTextForSave(
      filePath, languageId, textArea->toPlainText(), getTabWidth());
  replaceEditorContentsKeepingUndo(textArea, textToSave);
  saveEdits.endEditBlock();
  FileOpenGuard::TextFormat format = m_fileFormats.value(normalizedSavePath);
  bool encodingChanged = false;
  const QByteArray bytes =
      FileOpenGuard::encodeText(textToSave, &format, &encodingChanged);
  QString writeError;
  if (!FileOpenGuard::writeFileAtomically(filePath, bytes, &writeError)) {
    m_internalFileWrites.remove(normalizedSavePath);
    const QString message =
        tr("Could not save %1: %2")
            .arg(QFileInfo(filePath).fileName(), writeError);
    if (isAutoSave) {
      statusBar()->showMessage(message, 8000);
    } else {
      ThemedMessageBox::critical(this, tr("Save Failed"), message);
    }
    return false;
  }
  m_fileFormats.insert(normalizedSavePath, format);
  if (encodingChanged) {
    statusBar()->showMessage(
        tr("%1 now contains characters Latin-1 cannot store, so it was saved "
           "as UTF-8.")
            .arg(QFileInfo(filePath).fileName()),
        8000);
  }

  textArea->document()->setModified(false);
  textArea->removeIconUnsaved();
  trackRecoveryEditor(textArea);

  recordFileTimestamp(filePath);
  watchOpenFile(filePath);

  const QString fileName = QFileInfo(filePath).fileName();
  targetTabWidget->setTabText(targetTabIndex, fileName);
  if (targetTabWidget == currentTabWidget() &&
      targetTabIndex == targetTabWidget->currentIndex()) {
    setMainWindowTitle(fileName);
  }

  if (problemsPanel) {
    problemsPanel->onFileSaved(filePath);
  }

  notifyDiagnosticsFileSaved(filePath);

  if (targetTabWidget == currentTabWidget() &&
      targetTabIndex == targetTabWidget->currentIndex()) {
    showGitBlameForCurrentFile(isGitBlameEnabledForFile(filePath));
    updateInlineBlameForCurrentFile();
  }

  if (autoSaveManager) {
    autoSaveManager->markSaved(filePath);
  }

  if (testPanel) {
    testPanel->notifyFileSaved(filePath);
  }

  return true;
}

void MainWindow::recordFileTimestamp(const QString &filePath) {
  if (!filePath.isEmpty()) {
    const QString normalizedPath = QDir::cleanPath(filePath);
    m_fileTimestamps[normalizedPath] = QFileInfo(normalizedPath).lastModified();
  }
}

bool MainWindow::checkExternalModification(const QString &filePath) const {
  const QString normalizedPath = QDir::cleanPath(filePath);
  if (m_searchReplaceWrites.contains(normalizedPath))
    return false;
  if (normalizedPath.isEmpty() || !m_fileTimestamps.contains(normalizedPath)) {
    return false;
  }
  QFileInfo info(normalizedPath);
  if (!info.exists()) {
    return false;
  }
  return info.lastModified() > m_fileTimestamps.value(normalizedPath);
}

bool MainWindow::isFileOpenInEditor(const QString &filePath) const {
  const QString normalizedPath = QDir::cleanPath(filePath);
  if (normalizedPath.isEmpty()) {
    return false;
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }
    for (int i = 0; i < tabWidget->count(); ++i) {
      if (QDir::cleanPath(tabWidget->getFilePath(i)) == normalizedPath) {
        return true;
      }
    }
  }
  return false;
}

void MainWindow::setupOpenFileWatcher() {
  if (m_openFileWatcher) {
    return;
  }

  m_openFileWatcher = new QFileSystemWatcher(this);
  connect(m_openFileWatcher, &QFileSystemWatcher::fileChanged, this,
          [this](const QString &path) {
            const QString filePath = QDir::cleanPath(path);
            if (m_searchReplaceWrites.contains(filePath))
              return;
            if (m_internalFileWrites.remove(filePath)) {
              watchOpenFile(filePath);
              return;
            }

            QTimer::singleShot(100, this, [this, filePath]() {
              if (!QFileInfo::exists(filePath)) {
                return;
              }
              watchOpenFile(filePath);
              if (checkExternalModification(filePath)) {
                if (isFileOpenInEditor(filePath)) {
                  handleExternalModification(filePath, true);
                } else {
                  recordFileTimestamp(filePath);
                  unwatchOpenFileIfUnused(filePath);
                }
              }
            });
          });
}

void MainWindow::watchOpenFile(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }
  setupOpenFileWatcher();

  const QString normalizedPath = QDir::cleanPath(filePath);
  if (!QFileInfo::exists(normalizedPath) ||
      m_openFileWatcher->files().contains(normalizedPath)) {
    return;
  }
  m_openFileWatcher->addPath(normalizedPath);
}

void MainWindow::unwatchOpenFileIfUnused(const QString &filePath) {
  if (!m_openFileWatcher || filePath.isEmpty()) {
    return;
  }

  const QString normalizedPath = QDir::cleanPath(filePath);
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }
    for (int i = 0; i < tabWidget->count(); ++i) {
      if (QDir::cleanPath(tabWidget->getFilePath(i)) == normalizedPath) {
        return;
      }
    }
  }

  if (m_openFileWatcher->files().contains(normalizedPath)) {
    m_openFileWatcher->removePath(normalizedPath);
  }
  m_fileTimestamps.remove(normalizedPath);
  m_externalChangePrompts.remove(normalizedPath);
  m_internalFileWrites.remove(normalizedPath);
  m_fileFormats.remove(normalizedPath);
}

void MainWindow::recheckOpenFilesForExternalChanges() {
  QSet<QString> checkedPaths;
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }
    for (int i = 0; i < tabWidget->count(); ++i) {
      const QString filePath = QDir::cleanPath(tabWidget->getFilePath(i));
      if (filePath.isEmpty() || checkedPaths.contains(filePath)) {
        continue;
      }
      checkedPaths.insert(filePath);
      if (checkExternalModification(filePath)) {
        handleExternalModification(filePath, true);
      }
    }
  }
}

bool MainWindow::isOpenFileModified(const QString &filePath) const {
  const QString normalizedPath = QDir::cleanPath(filePath);
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }
    for (int i = 0; i < tabWidget->count(); ++i) {
      if (QDir::cleanPath(tabWidget->getFilePath(i)) != normalizedPath) {
        continue;
      }
      LightpadPage *page = tabWidget->getPage(i);
      TextArea *area = page ? page->getTextArea() : nullptr;
      if (area && area->document() && area->document()->isModified()) {
        return true;
      }
    }
  }
  return false;
}

bool MainWindow::handleExternalModification(const QString &filePath,
                                            bool allowOverwrite) {
  const QString normalizedPath = QDir::cleanPath(filePath);
  if (normalizedPath.isEmpty() ||
      m_externalChangePrompts.contains(normalizedPath)) {
    return false;
  }

  const QString fileName = QFileInfo(normalizedPath).fileName();

  if (allowOverwrite && !isOpenFileModified(normalizedPath)) {
    reloadOpenFileFromDisk(normalizedPath);
    statusBar()->showMessage(
        tr("%1 changed on disk and was reloaded.").arg(fileName), 5000);
    return false;
  }

  m_externalChangePrompts.insert(normalizedPath);
  ThemedMessageBox msgBox(this);
  msgBox.setWindowTitle(tr("File Changed on Disk"));
  msgBox.setIcon(ThemedMessageBox::Warning);
  msgBox.setText(
      tr("<b>%1</b> has been modified by another program.").arg(fileName));
  msgBox.setInformativeText(
      allowOverwrite
          ? tr("You have unsaved changes. Choose Reload from Disk to discard "
               "them and take the new file, Overwrite to replace the disk file "
               "with the editor contents, or Cancel to decide later.")
          : tr("Choose Reload from Disk to take the new file, Overwrite to "
               "replace it with the editor contents, or Cancel to stop "
               "saving."));
  msgBox.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No |
                            ThemedMessageBox::Cancel);
  msgBox.setButtonText(ThemedMessageBox::Yes, tr("Overwrite"));
  msgBox.setButtonText(ThemedMessageBox::No, tr("Reload from Disk"));
  msgBox.setDefaultButton(ThemedMessageBox::Cancel);
  const int result = msgBox.exec();
  m_externalChangePrompts.remove(normalizedPath);

  if (result == ThemedMessageBox::No) {
    reloadOpenFileFromDisk(normalizedPath);
    return false;
  }
  if (result == ThemedMessageBox::Yes) {
    if (allowOverwrite) {
      writeOpenFileToDisk(normalizedPath);
      return false;
    }
    return true;
  }
  return false;
}

bool MainWindow::reloadOpenFileFromDisk(const QString &filePath) {
  const bool previewOnly = isPreviewOnlyFile(filePath);
  bool ok = false;
  bool truncated = false;
  FileOpenGuard::TextFormat format;
  const QString diskText = FileOpenGuard::readTextCapped(
      filePath, previewOnly ? FileOpenGuard::PreviewBytes : 0, &truncated, &ok,
      &format);
  if (!ok) {
    return false;
  }
  m_fileFormats.insert(QDir::cleanPath(filePath), format);

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }
    for (int i = 0; i < tabWidget->count(); ++i) {
      if (QDir::cleanPath(tabWidget->getFilePath(i)) != filePath) {
        continue;
      }
      LightpadPage *page = tabWidget->getPage(i);
      TextArea *area = page ? page->getTextArea() : nullptr;
      if (!area) {
        continue;
      }
      const int cursorPos = area->textCursor().position();
      area->setPlainText(diskText);
      QTextCursor cursor = area->textCursor();
      cursor.setPosition(qMin(cursorPos, area->toPlainText().length()));
      area->setTextCursor(cursor);
      area->document()->setModified(false);
      area->removeIconUnsaved();
      if (previewOnly) {
        area->setReadOnly(truncated);
      }
    }
  }

  recordFileTimestamp(filePath);
  watchOpenFile(filePath);
  if (autoSaveManager) {
    autoSaveManager->markSaved(filePath);
  }
  notifyDiagnosticsFileOpened(filePath);
  return true;
}

bool MainWindow::writeOpenFileToDisk(const QString &filePath) {
  TextArea *textArea = nullptr;
  LightpadTabWidget *targetTabWidget = nullptr;
  int targetTabIndex = -1;

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    if (!tabWidget) {
      continue;
    }
    for (int i = 0; i < tabWidget->count(); ++i) {
      if (QDir::cleanPath(tabWidget->getFilePath(i)) != filePath) {
        continue;
      }
      LightpadPage *page = tabWidget->getPage(i);
      if (page && page->getTextArea()) {
        textArea = page->getTextArea();
        targetTabWidget = tabWidget;
        targetTabIndex = i;
        break;
      }
    }
    if (textArea) {
      break;
    }
  }

  if (!textArea) {
    return false;
  }
  if (textArea->isReadOnly() ||
      (findReplacePanel && findReplacePanel->isBusy()))
    return false;

  if (isPreviewOnlyFile(filePath)) {
    ThemedMessageBox::warning(
        this, tr("This is only a preview"),
        tr("Lightpad loaded just part of %1, so overwriting it would throw "
           "away the rest. The file has been left untouched.")
            .arg(QFileInfo(filePath).fileName()));
    return false;
  }

  const QString normalizedPath = QDir::cleanPath(filePath);
  m_internalFileWrites.insert(normalizedPath);
  QTimer::singleShot(1000, this, [this, normalizedPath]() {
    m_internalFileWrites.remove(normalizedPath);
  });
  const QString languageId = effectiveLanguageIdForFile(filePath);
  const QString textToSave = normalizeEditorTextForSave(
      filePath, languageId, textArea->toPlainText(), getTabWidth());
  replaceEditorContentsKeepingUndo(textArea, textToSave);
  FileOpenGuard::TextFormat format = m_fileFormats.value(normalizedPath);
  const QByteArray bytes = FileOpenGuard::encodeText(textToSave, &format);
  QString writeError;
  if (!FileOpenGuard::writeFileAtomically(normalizedPath, bytes, &writeError)) {
    m_internalFileWrites.remove(normalizedPath);
    ThemedMessageBox::critical(
        this, tr("Save Failed"),
        tr("Could not save %1: %2")
            .arg(QFileInfo(normalizedPath).fileName(), writeError));
    return false;
  }
  m_fileFormats.insert(normalizedPath, format);

  textArea->document()->setModified(false);
  textArea->removeIconUnsaved();
  recordFileTimestamp(normalizedPath);
  watchOpenFile(normalizedPath);

  if (targetTabWidget && targetTabIndex >= 0) {
    targetTabWidget->setTabText(targetTabIndex,
                                QFileInfo(normalizedPath).fileName());
  }
  notifyDiagnosticsFileSaved(normalizedPath);
  if (autoSaveManager) {
    autoSaveManager->markSaved(normalizedPath);
  }
  if (testPanel) {
    testPanel->notifyFileSaved(normalizedPath);
  }
  return true;
}

void MainWindow::trimTrailingWhitespace(TextArea *textArea) {
  if (!textArea) {
    return;
  }

  QTextCursor cursor(textArea->document());
  cursor.beginEditBlock();

  QTextBlock block = textArea->document()->firstBlock();
  while (block.isValid()) {
    QString text = block.text();
    int originalLength = text.length();

    int i = originalLength - 1;
    while (i >= 0 && (text[i] == ' ' || text[i] == '\t')) {
      --i;
    }

    int newLength = i + 1;
    if (newLength < originalLength) {
      cursor.setPosition(block.position() + newLength);
      cursor.setPosition(block.position() + originalLength,
                         QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
    }

    block = block.next();
  }

  cursor.endEditBlock();
}

void MainWindow::ensureFinalNewline(TextArea *textArea) {
  if (!textArea) {
    return;
  }

  QString text = textArea->toPlainText();
  if (!text.isEmpty() && !text.endsWith('\n')) {
    QTextCursor cursor(textArea->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n");
  }
}

void MainWindow::formatCurrentDocument() {
  auto page = currentTabWidget()->getCurrentPage();
  QString filePath = page ? page->getFilePath() : QString();

  if (filePath.isEmpty()) {
    ThemedMessageBox::information(
        this, "Format Document",
        "Please save the file first before formatting.");
    return;
  }

  auto textArea = getCurrentTextArea();
  on_actionSave_triggered();

  if (textArea && textArea->changesUnsaved()) {
    ThemedMessageBox::warning(this, "Format Document",
                              "Could not save the file. Formatting cancelled.");
    return;
  }

  FormatTemplateManager &manager = FormatTemplateManager::instance();

  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  QPair<QString, QStringList> command = manager.buildCommand(filePath);

  if (command.first.isEmpty()) {
    ThemedMessageBox::information(
        this, "Format Document",
        "No formatter found for this file type.\nUse Format > Edit Format "
        "Configurations to assign one.");
    return;
  }

  ensureProjectRootForPath(filePath);
  showTerminalPanel();

  if (m_formatProcessFinishedConnection) {
    disconnect(m_formatProcessFinishedConnection);
    m_formatProcessFinishedConnection = {};
  }
  if (m_formatProcessErrorConnection) {
    disconnect(m_formatProcessErrorConnection);
    m_formatProcessErrorConnection = {};
  }

  FileFormatAssignment assignment = manager.getAssignmentForFile(filePath);

  QString workingDirectory = manager.getWorkingDirectory(filePath);
  QMap<QString, QString> customEnv = manager.getEnvironment(filePath);

  QString preFormatCommand = assignment.preFormatCommand.trimmed();
  if (!preFormatCommand.isEmpty()) {
    preFormatCommand = FormatTemplateManager::substituteCommandLineVariables(
        preFormatCommand, filePath);
  }
  QString postFormatCommand = assignment.postFormatCommand.trimmed();
  if (!postFormatCommand.isEmpty()) {
    postFormatCommand = FormatTemplateManager::substituteCommandLineVariables(
        postFormatCommand, filePath);
  }

  auto shellProgramAndArgs =
      [](const QString &commandText) -> QPair<QString, QStringList> {
#ifdef Q_OS_WIN
    return qMakePair(QString("cmd"), QStringList() << "/C" << commandText);
#else
    return qMakePair(QString("bash"), QStringList() << "-lc" << commandText);
#endif
  };

  struct FormatExecutionState {
    enum class Stage {
      PreFormat,
      Formatter,
      PostFormat,
    };

    Stage stage = Stage::Formatter;
    bool formatterRan = false;
    int formatterExitCode = 0;
    QString activeStageName;
  };

  QSharedPointer<FormatExecutionState> executionState =
      QSharedPointer<FormatExecutionState>::create();
  if (!preFormatCommand.isEmpty()) {
    executionState->stage = FormatExecutionState::Stage::PreFormat;
  }

  QPointer<TextArea> targetTextArea = textArea;
  QSharedPointer<std::function<void()>> finalizeExecution =
      QSharedPointer<std::function<void()>>::create();
  *finalizeExecution = [this, filePath, targetTextArea, executionState]() {
    if (m_formatProcessFinishedConnection) {
      disconnect(m_formatProcessFinishedConnection);
      m_formatProcessFinishedConnection = {};
    }
    if (m_formatProcessErrorConnection) {
      disconnect(m_formatProcessErrorConnection);
      m_formatProcessErrorConnection = {};
    }

    if (executionState->formatterRan &&
        executionState->formatterExitCode != 0) {
      LOG_WARNING(QString("Formatter exited with code %1")
                      .arg(executionState->formatterExitCode));
      ThemedMessageBox::warning(
          this, "Format Document",
          QString("Formatter exited with error code %1.\nCheck the "
                  "Terminal panel for details.")
              .arg(executionState->formatterExitCode));
    }

    if (!targetTextArea || !executionState->formatterRan) {
      return;
    }

    bool readOk = false;
    FileOpenGuard::TextFormat format;
    const QString newContent =
        FileOpenGuard::readTextCapped(filePath, 0, nullptr, &readOk, &format);
    if (readOk) {
      m_fileFormats.insert(QDir::cleanPath(filePath), format);

      if (targetTextArea->toPlainText() != newContent) {
        int cursorPos = targetTextArea->textCursor().position();

        targetTextArea->setPlainText(newContent);
        targetTextArea->document()->setModified(false);

        QTextCursor cursor = targetTextArea->textCursor();
        cursor.setPosition(
            qMin(cursorPos, targetTextArea->toPlainText().length()));
        targetTextArea->setTextCursor(cursor);
      }
    } else {
      LOG_WARNING(
          QString("Failed to reload file after formatting: %1").arg(filePath));
    }
  };

  QSharedPointer<std::function<void()>> runCurrentStage =
      QSharedPointer<std::function<void()>>::create();
  *runCurrentStage = [this, runCurrentStage, executionState, preFormatCommand,
                      command, postFormatCommand, workingDirectory, customEnv,
                      shellProgramAndArgs]() {
    switch (executionState->stage) {
    case FormatExecutionState::Stage::PreFormat: {
      executionState->activeStageName = "pre-format command";
      QPair<QString, QStringList> shellCommand =
          shellProgramAndArgs(preFormatCommand);
      terminalWidget->executeCommand(shellCommand.first, shellCommand.second,
                                     workingDirectory, customEnv);
      return;
    }
    case FormatExecutionState::Stage::Formatter:
      executionState->activeStageName = "formatter";
      terminalWidget->executeCommand(command.first, command.second,
                                     workingDirectory, customEnv);
      return;
    case FormatExecutionState::Stage::PostFormat: {
      executionState->activeStageName = "post-format command";
      QPair<QString, QStringList> shellCommand =
          shellProgramAndArgs(postFormatCommand);
      terminalWidget->executeCommand(shellCommand.first, shellCommand.second,
                                     workingDirectory, customEnv);
      return;
    }
    }
  };

  m_formatProcessFinishedConnection = connect(
      terminalWidget, &TerminalTabWidget::processFinished, this,
      [this, executionState, postFormatCommand, runCurrentStage,
       finalizeExecution](int exitCode) {
        if (executionState->stage == FormatExecutionState::Stage::PreFormat) {
          if (exitCode != 0) {
            ThemedMessageBox::warning(
                this, "Format Document",
                QString("Pre-format command failed with exit code %1.\nCheck "
                        "the Terminal panel for details.")
                    .arg(exitCode));
            (*finalizeExecution)();
            return;
          }

          executionState->stage = FormatExecutionState::Stage::Formatter;
          (*runCurrentStage)();
          return;
        }

        if (executionState->stage == FormatExecutionState::Stage::Formatter) {
          executionState->formatterRan = true;
          executionState->formatterExitCode = exitCode;

          if (!postFormatCommand.isEmpty()) {
            executionState->stage = FormatExecutionState::Stage::PostFormat;
            (*runCurrentStage)();
            return;
          }

          (*finalizeExecution)();
          return;
        }

        if (executionState->stage == FormatExecutionState::Stage::PostFormat) {
          if (exitCode != 0) {
            ThemedMessageBox::warning(
                this, "Format Document",
                QString("Post-format command failed with exit code %1.\nCheck "
                        "the Terminal panel for details.")
                    .arg(exitCode));
          }
          (*finalizeExecution)();
          return;
        }
      });

  m_formatProcessErrorConnection = connect(
      terminalWidget, &TerminalTabWidget::errorOccurred, this,
      [this, executionState, finalizeExecution](const QString &errorMessage) {
        ThemedMessageBox::warning(
            this, "Format Document",
            QString("Failed to start %1.\n\n%2")
                .arg(executionState->activeStageName, errorMessage));
        (*finalizeExecution)();
      });

  (*runCurrentStage)();
}

void MainWindow::setFilePathAsTabText(QString filePath) {

  auto fileName = QFileInfo(filePath).fileName();

  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  auto tabText = tabWidget->tabText(tabIndex);

  setMainWindowTitle(fileName);
  tabWidget->setTabText(tabIndex, fileName);
}

void MainWindow::closeCurrentTab() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget)
    tabWidget->requestCloseTab(tabWidget->currentIndex());
}

void MainWindow::setupTabWidgetConnections(LightpadTabWidget *tabWidget) {
  if (!tabWidget || tabWidget->property("mainWindowConnected").toBool()) {
    return;
  }
  tabWidget->setProperty("mainWindowConnected", true);
  connect(tabWidget, &LightpadTabWidget::editorClosed, this,
          [this](QPlainTextEdit *editor) {
            if (m_documentRecovery)
              m_documentRecovery->forget(editor);
          });

  tabWidget->setCloseGuard([this](LightpadTabWidget *owner, int index) {
    return maybeSaveTab(owner, index);
  });
  QObject::connect(tabWidget, &QTabWidget::currentChanged, this,
                   [this, tabWidget](int index) {
                     QWidget *page = tabWidget->widget(index);
                     if (page && page != m_vimCurrentPage &&
                         index < tabWidget->count() - 1) {
                       m_vimAlternatePage = m_vimCurrentPage;
                       m_vimCurrentPage = page;
                     }
                     updateTabWidgetContext(tabWidget, index);
                     if (m_globalSettingsLoaded && !m_restoringSession) {
                       saveSettings();
                     }
                   });
  QObject::connect(tabWidget, &LightpadTabWidget::tabClosed, this,
                   [this](const QString &filePath) {
                     if (!filePath.isEmpty() && !isFileOpenInEditor(filePath)) {
                       notifyDiagnosticsFileClosed(filePath);
                       unwatchOpenFileIfUnused(filePath);
                     }
                     if (findReplacePanel) {
                       findReplacePanel->setTextArea(getCurrentTextArea());
                     }
                     QTimer::singleShot(0, this, [this]() {
                       if (m_globalSettingsLoaded && !m_restoringSession) {
                         saveSettings();
                       }
                     });
                   });
}

void MainWindow::refreshRunAndDebugActionLabels() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  const QString fileName =
      tabWidget ? tabWidget->tabText(tabWidget->currentIndex()) : QString();
  updateRunAndDebugActionLabels(fileName);
}

void MainWindow::updateRunAndDebugActionLabels(const QString &fileName) {
  refreshRunTargetButton();
  refreshDebugTargetButton();
  const QString label = fileName.trimmed();
  if (ui->actionRun_file_name) {
    ui->actionRun_file_name->setText(label.isEmpty() ? tr("Run Current File")
                                                     : tr("Run %1").arg(label));
  }
  if (ui->actionDebug_file_name) {

    const QString debugLabel =
        label.isEmpty() ? tr("Debug Current File") : tr("Debug %1").arg(label);
    ui->actionDebug_file_name->setText(debugLabel + QLatin1String("\tF5"));
  }
}

void MainWindow::updateTabWidgetContext(LightpadTabWidget *tabWidget,
                                        int index) {
  if (!tabWidget) {
    return;
  }

  auto text = tabWidget->tabText(index);
  setMainWindowTitle(text);

  updateRunAndDebugActionLabels(text);

  QString filePath = tabWidget->getFilePath(index);
  if (!filePath.isEmpty()) {
    watchOpenFile(filePath);
    if (checkExternalModification(filePath)) {
      handleExternalModification(filePath, true);
    }
  }
  applyHighlightForFile(filePath);

  setupTextArea();

  if (findReplacePanel && tabWidget == currentTabWidget()) {
    findReplacePanel->setTextArea(getCurrentTextArea());
  }

  auto *page = tabWidget->getPage(index);
  if (page) {
    auto *view = qobject_cast<LightpadTreeView *>(page->getTreeView());
    if (view) {
      registerTreeView(view);
    }
    if (tabWidget == currentTabWidget() && !filePath.isEmpty()) {
      page->revealPath(filePath);
    }
  }
  if (tabWidget == currentTabWidget()) {
    updateBreadcrumb(filePath);
  }

  updatePythonEnvironmentLabel();
  refreshTestTargetButton();
  syncViewToggleActionStates();
  if (problemsPanel && !filePath.isEmpty()) {
    problemsPanel->setCurrentFilePath(filePath);
  }
  refreshProblemsStatusForCurrentFile();
  if (m_languageFeatureManager && !filePath.isEmpty()) {
    const QString languageId = effectiveLanguageIdForFile(filePath);
    switch (m_languageFeatureManager->serverHealth(languageId)) {
    case ServerHealthStatus::Starting:
      updateLspStatusLabel(languageId, "starting");
      break;
    case ServerHealthStatus::Running:
      updateLspStatusLabel(languageId, "running");
      break;
    case ServerHealthStatus::Error:
      updateLspStatusLabel(languageId, "error");
      break;
    case ServerHealthStatus::Stopped:
      updateLspStatusLabel(languageId, "stopped");
      break;
    default:
      if (m_lspStatusLabel) {
        m_lspStatusLabel->setVisible(false);
      }
      break;
    }
  } else if (m_lspStatusLabel) {
    m_lspStatusLabel->setVisible(false);
  }
}

void MainWindow::applyTabWidgetTheme(LightpadTabWidget *tabWidget) {
  if (!tabWidget) {
    return;
  }
  const Theme theme = getTheme();
  tabWidget->setTheme(theme.backgroundColor.name(),
                      theme.foregroundColor.name(), theme.surfaceColor.name(),
                      theme.accentSoftColor.name(), theme.accentColor.name(),
                      theme.borderColor.name());
}

void MainWindow::setupTabWidget() {
  applyTabWidgetTheme(ui->tabWidget);
  setupTabWidgetConnections(ui->tabWidget);
  updateTabWidgetContext(ui->tabWidget, 0);
}

void MainWindow::setupCompletionSystem() {

  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(std::make_shared<KeywordCompletionProvider>());

  registry.registerProvider(std::make_shared<SnippetCompletionProvider>());

  registry.registerProvider(std::make_shared<PluginCompletionProvider>());
  registerSqlCompletion();

  m_lspCompletionProvider = std::make_shared<LspCompletionProvider>(nullptr);
  registry.registerProvider(m_lspCompletionProvider);

  connect(qApp, &QApplication::focusChanged, this,
          [this](QWidget *, QWidget *now) {
            if (!now || now->window() != this || !m_lspCompletionProvider) {
              return;
            }
            auto &providers = CompletionProviderRegistry::instance();
            if (providers.getProvider(m_lspCompletionProvider->id()) !=
                m_lspCompletionProvider) {
              providers.registerProvider(m_lspCompletionProvider);
            }
          });

  m_completionEngine = new CompletionEngine(this);

  QStringList providerIds = registry.allProviderIds();
  if (providerIds.isEmpty()) {
    LOG_WARNING("Completion system initialized but no providers registered");
  } else {
    LOG_INFO("Completion system initialized with providers: " +
             providerIds.join(", "));
  }
}

void MainWindow::setupTextArea() {

  if (getCurrentTextArea()) {
    auto *textArea = getCurrentTextArea();
    textArea->setMainWindow(this);
    textArea->setFont(settings.mainFont);
    textArea->setTabWidth(settings.tabWidth);
    textArea->setVimModeEnabled(settings.vimModeEnabled);
    trackRecoveryEditor(textArea);

    if (autoSaveManager && !textArea->property("autoSaveHooked").toBool()) {
      connect(textArea->document(), &QTextDocument::modificationChanged, this,
              [this, textArea](bool modified) {
                if (!autoSaveManager || !textArea) {
                  return;
                }

                QString filePath;
                QObject *parentObject = textArea;
                while (parentObject && filePath.isEmpty()) {
                  if (auto *page = qobject_cast<LightpadPage *>(parentObject)) {
                    filePath = page->getFilePath();
                  }
                  parentObject = parentObject->parent();
                }

                if (filePath.isEmpty()) {
                  return;
                }

                if (modified) {
                  autoSaveManager->markModified(filePath);
                } else {
                  autoSaveManager->markSaved(filePath);
                }
              });
      textArea->setProperty("autoSaveHooked", true);
    }

    if (m_languageFeatureManager &&
        !textArea->property("diagnosticsHooked").toBool()) {
      connect(textArea, &QPlainTextEdit::textChanged, this, [this, textArea]() {
        if (!m_languageFeatureManager || !textArea) {
          return;
        }
        QString filePath;
        QObject *parentObject = textArea;
        while (parentObject && filePath.isEmpty()) {
          if (auto *page = qobject_cast<LightpadPage *>(parentObject)) {
            filePath = page->getFilePath();
          }
          parentObject = parentObject->parent();
        }
        if (!filePath.isEmpty()) {
          notifyDiagnosticsFileChanged(filePath, textArea->toPlainText());
        }
      });
      textArea->setProperty("diagnosticsHooked", true);
    }

    ensureStatusLabels();
    connectVimMode(textArea);

    if (m_completionEngine) {
      textArea->setCompletionEngine(m_completionEngine);
      LightpadTabWidget *tabWidget = currentTabWidget();
      QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
      if (m_lspCompletionProvider) {
        LspClient *client =
            (m_languageFeatureManager && !filePath.isEmpty())
                ? m_languageFeatureManager->clientForFile(filePath)
                : nullptr;
        m_lspCompletionProvider->setClient(client);
      }
      if (filePath.isEmpty()) {
        textArea->setLanguage("plaintext");
        textArea->updateSyntaxHighlightTags("", "plaintext");
        QString displayName = LanguageCatalog::displayName("plaintext");
        setLanguageHighlightLabel(displayName.isEmpty() ? "Normal Text"
                                                        : displayName);
      } else {
        applyHighlightForFile(filePath);
      }
    }

    if (completer && !m_completionEngine)
      textArea->setCompleter(completer);
  }
}
