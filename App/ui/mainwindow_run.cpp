#include "mainwindow_internal.h"

void MainWindow::showFindReplace(bool onlyFind) {
  if (!findReplacePanel) {
    findReplacePanel = new FindReplacePanel(onlyFind);

    auto layout = qobject_cast<QBoxLayout *>(ui->centralwidget->layout());

    if (layout != 0)
      layout->insertWidget(layout->count() - 1, findReplacePanel, 0);

    connect(findReplacePanel, &FindReplacePanel::navigateToFile, this,
            [this](const QString &filePath, int lineNumber, int columnNumber,
                   int matchLength) {
              if (!filePath.isEmpty()) {
                openFileAndAddToNewTab(filePath);
              }
              TextArea *textArea = getCurrentTextArea();
              if (textArea) {
                const QTextBlock block =
                    textArea->document()->findBlockByNumber(
                        qMax(0, lineNumber - 1));
                if (!block.isValid())
                  return;
                QTextCursor cursor(textArea->document());
                cursor.setPosition(
                    block.position() +
                    qBound(0, columnNumber - 1, block.length() - 1));
                cursor.setPosition(
                    qMin(cursor.position() + matchLength,
                         textArea->document()->characterCount() - 1),
                    QTextCursor::KeepAnchor);
                textArea->setTextCursor(cursor);
                textArea->ensureCursorVisible();
              }
            });

    connect(findReplacePanel, &QObject::destroyed, this, [this]() {
      findReplacePanel = nullptr;
      m_vimCommandPanelActive = false;
    });
  }

  bool targetOnlyFind = m_vimCommandPanelActive ? true : onlyFind;
  findReplacePanel->setVisible(true);
  findReplacePanel->setOnlyFind(targetOnlyFind);

  if (findReplacePanel->isVisible() && getCurrentTextArea()) {
    findReplacePanel->setReplaceVisibility(!targetOnlyFind);
  }

  if (findReplacePanel->isVisible()) {
    findReplacePanel->setTextArea(getCurrentTextArea());
    QString projectPath = m_projectRootPath;
    if (projectPath.isEmpty()) {
      LightpadTabWidget *tabWidget = currentTabWidget();
      QString filePath = tabWidget
                             ? tabWidget->getFilePath(tabWidget->currentIndex())
                             : QString();
      projectPath = filePath.isEmpty() ? QDir::currentPath()
                                       : QFileInfo(filePath).absolutePath();
    }
    findReplacePanel->setProjectPath(projectPath);
    findReplacePanel->setMainWindow(this);
    findReplacePanel->setFocusOnSearchBox();
  }
}

void MainWindow::openDialog(Dialog dialog) {
  switch (dialog) {
  case Dialog::runConfiguration:
    openRunConfigurationForFile(runSourceFilePath());
    break;

  case Dialog::formatConfiguration: {

    auto page = currentTabWidget()->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();

    if (filePath.isEmpty()) {
      ThemedMessageBox::information(
          this, "Format Configuration",
          "Please open a file first to configure format settings.");
      return;
    }

    ensureProjectRootForPath(filePath);

    if (findChildren<FormatTemplateSelector *>().isEmpty()) {
      auto selector = new FormatTemplateSelector(filePath, this);
      selector->setAttribute(Qt::WA_DeleteOnClose);
      selector->show();
    }
    break;
  }

  case Dialog::debugConfiguration: {
    auto page = currentTabWidget()->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();

    if (filePath.isEmpty()) {
      ThemedMessageBox::information(
          this, "Debug Configurations",
          "Please open a file first to configure debug settings.");
      return;
    }
    ensureProjectRootForPath(filePath);

    DebugSettings::instance().initialize(m_projectRootPath);
    DebugConfigurationManager::instance().setWorkspaceFolder(m_projectRootPath);
    DebugConfigurationManager::instance().loadFromLightpadDir();

    if (findChildren<DebugConfigurationDialog *>().isEmpty()) {
      auto dialog = new DebugConfigurationDialog(this);
      dialog->setAttribute(Qt::WA_DeleteOnClose);
      dialog->show();
    }
    break;
  }

  case Dialog::shortcuts:
    if (findChildren<ShortcutsDialog *>().isEmpty())
      new ShortcutsDialog(this);
    break;

  default:
    return;
  }
}

void MainWindow::openConfigurationDialog() {
  openDialog(Dialog::runConfiguration);
}

void MainWindow::openFormatConfigurationDialog() {
  openDialog(Dialog::formatConfiguration);
}

void MainWindow::openDebugConfigurationDialog() {
  openDialog(Dialog::debugConfiguration);
}

void MainWindow::openTestConfigurationDialog() {
  ensureTestPanel();

  if (m_projectRootPath.isEmpty()) {
    auto *tabWidget = currentTabWidget();
    auto *page = tabWidget ? tabWidget->getCurrentPage() : nullptr;
    const QString filePath = page ? page->getFilePath() : QString();
    if (filePath.isEmpty()) {
      ThemedMessageBox::information(this, tr("Test Configurations"),
                                    tr("Please open a file or project first to "
                                       "configure test settings."));
      return;
    }
    ensureProjectRootForPath(filePath);
  }

  if (testPanel && !m_projectRootPath.isEmpty())
    testPanel->setWorkspaceFolder(m_projectRootPath);
  if (testPanel)
    testPanel->openConfigurationDialog();
}

void MainWindow::openShortcutsDialog() { openDialog(Dialog::shortcuts); }

TerminalTabWidget *MainWindow::ensureTerminalWidget() {
  if (!terminalWidget) {
    terminalWidget = new TerminalTabWidget();
    terminalWidget->applyTheme(getTheme());

    connect(terminalWidget, &TerminalTabWidget::closeRequested, this, [this]() {
      if (m_terminalDock) {
        m_terminalDock->hide();
      }
    });

    m_terminalDock = new QDockWidget(tr("Terminal"), this);
    m_terminalDock->setObjectName("terminalDock");
    DockUtils::configureToolPanelDock(m_terminalDock);

    m_terminalDock->setWidget(terminalWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
    tabifyBottomDock(m_terminalDock);
    trackDockLayoutChanges(m_terminalDock);
    m_terminalDock->hide();
    connect(m_terminalDock, &QDockWidget::visibilityChanged, this,
            [this](bool visible) {
              Q_UNUSED(visible)
              syncViewToggleActionStates();
            });
  }

  return terminalWidget;
}

void MainWindow::showTerminalPanel() {
  TerminalTabWidget *widget = ensureTerminalWidget();
  if (!widget || !m_terminalDock) {
    return;
  }

  m_terminalDock->show();
  m_terminalDock->raise();
  syncViewToggleActionStates();
}

QString MainWindow::activeEditorFilePath() const {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return QString();
  }
  auto page = tabWidget->getCurrentPage();
  return page ? page->getFilePath() : QString();
}

QString MainWindow::runSourceFilePath() const {

  if (!m_pinnedRunFilePath.isEmpty() &&
      QFileInfo::exists(m_pinnedRunFilePath) &&
      (m_projectRootPath.isEmpty() ||
       FileDirTreeModel::isInside(m_projectRootPath, m_pinnedRunFilePath))) {
    return m_pinnedRunFilePath;
  }
  return activeEditorFilePath();
}

QString MainWindow::effectivePreferredCMakeTarget() const {
  if (m_preferredCMakeTarget.isEmpty() ||
      m_preferredCMakeTargetRoot.isEmpty() || m_projectRootPath.isEmpty()) {
    return m_preferredCMakeTarget;
  }
  return QDir::cleanPath(m_preferredCMakeTargetRoot) ==
                 QDir::cleanPath(m_projectRootPath)
             ? m_preferredCMakeTarget
             : QString();
}

void MainWindow::invalidateRunTargetCache() {
  m_cachedRunTargetKey.clear();
  m_cachedRunTarget = RunTarget();
}

RunTarget MainWindow::resolveRunTargetForFile(const QString &filePath) {
  const QString key = filePath + QLatin1Char('\n') + m_projectRootPath +
                      QLatin1Char('\n') + effectivePreferredCMakeTarget();
  if (!m_cachedRunTargetKey.isEmpty() && m_cachedRunTargetKey == key) {
    return m_cachedRunTarget;
  }

  RunTargetContext context;
  context.filePath = filePath;
  context.projectRoot = m_projectRootPath;
  context.preferredCMakeTarget = effectivePreferredCMakeTarget();
  if (!filePath.isEmpty()) {
    context.languageId = effectiveLanguageIdForFile(filePath);
  }

  m_cachedRunTarget = RunTargetResolver::resolve(context);
  m_cachedRunTargetKey = key;
  return m_cachedRunTarget;
}

RunTarget MainWindow::currentRunTarget() {
  return resolveRunTargetForFile(runSourceFilePath());
}

void MainWindow::setPinnedRunFilePath(const QString &filePath) {
  const QString normalized =
      filePath.isEmpty() ? QString() : QFileInfo(filePath).absoluteFilePath();
  if (m_pinnedRunFilePath == normalized) {
    return;
  }
  m_pinnedRunFilePath = normalized;
  invalidateRunTargetCache();
  SettingsManager::instance().setValue("pinnedRunTarget", m_pinnedRunFilePath);
  SettingsManager::instance().saveSettings();
  refreshRunTargetButton();
}

void MainWindow::setPreferredCMakeTarget(const QString &targetName) {
  const QString trimmed = targetName.trimmed();
  if (m_preferredCMakeTarget == trimmed) {
    return;
  }
  m_preferredCMakeTarget = trimmed;
  m_preferredCMakeTargetRoot =
      trimmed.isEmpty() ? QString() : m_projectRootPath;
  invalidateRunTargetCache();
  SettingsManager::instance().setValue("preferredCMakeTarget",
                                       m_preferredCMakeTarget);
  SettingsManager::instance().setValue("preferredCMakeTargetRoot",
                                       m_preferredCMakeTargetRoot);
  SettingsManager::instance().saveSettings();
  refreshRunTargetButton();
}

void MainWindow::announceRunTarget(const RunTarget &target) {
  const QString path = target.targetPath();
  if (path.isEmpty()) {
    return;
  }
  statusBar()->showMessage(tr("Running %1").arg(QDir::toNativeSeparators(path)),
                           8000);
}

void MainWindow::runPath(const QString &filePath) {
  if (filePath.isEmpty()) {
    noScriptAssignedWarning();
    return;
  }

  save(filePath);

  invalidateRunTargetCache();
  ensureProjectRootForPath(filePath);
  executeRunTarget(resolveRunTargetForFile(filePath));
}

void MainWindow::revealPathInFileManager(const QString &path) {
  const QFileInfo info(path);
  if (!info.exists()) {
    return;
  }

#ifdef Q_OS_WIN
  QProcess::startDetached(
      "explorer", QStringList() << "/select," + QDir::toNativeSeparators(
                                                    info.absoluteFilePath()));
#elif defined(Q_OS_MAC)
  QProcess::startDetached("open", QStringList()
                                      << "-R" << info.absoluteFilePath());
#else
  const QString directory =
      info.isDir() ? info.absoluteFilePath() : info.absolutePath();
  QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
#endif
}

void MainWindow::openTerminalAtPath(const QString &path) {
  const QFileInfo info(path);
  if (!info.exists()) {
    return;
  }

  const QString directory =
      info.isDir() ? info.absoluteFilePath() : info.absolutePath();

  showTerminalPanel();
  if (terminalWidget) {
    terminalWidget->addNewTerminal(directory);
    terminalWidget->setWorkingDirectory(directory);
  }
}

void MainWindow::runTestsForPath(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }
  ensureProjectRootForPath(filePath);
  ensureTestPanel();
  if (!testPanel || !testDock) {
    return;
  }
  if (!m_projectRootPath.isEmpty()) {
    testPanel->setWorkspaceFolder(m_projectRootPath);
  }
  testPanel->runTestsForPath(filePath);
  testDock->show();
  testDock->raise();
  syncViewToggleActionStates();
}

void MainWindow::runStructuredTestTarget(const RunTarget &target) {
  ensureTestPanel();
  if (!testPanel || !testDock) {
    return;
  }

  if (!m_projectRootPath.isEmpty()) {
    testPanel->setWorkspaceFolder(m_projectRootPath);
  }

  const QString configId = target.templateId == QLatin1String("cpp_make_test")
                               ? QStringLiteral("gtest_make")
                               : QStringLiteral("gtest_cmake");
  bool launched = testPanel->runWithConfigurationId(configId, target.filePath);
  if (!launched) {
    launched = testPanel->runWithConfigurationId(QStringLiteral("gtest_cmake"),
                                                 target.filePath);
  }
  if (!launched) {
    testPanel->runCurrentFile(target.filePath);
  }

  testDock->show();
  testDock->raise();
  syncViewToggleActionStates();
}

void MainWindow::executeRunTarget(const RunTarget &target) {
  if (!target.isValid()) {

    ThemedMessageBox msg(this);
    msg.setIcon(ThemedMessageBox::Warning);
    msg.setWindowTitle(tr("Run"));
    msg.setText(tr("Nothing to run."));
    msg.setInformativeText(target.unavailableReason.isEmpty()
                               ? tr("Open and save a file to run it.")
                               : target.unavailableReason);

    const bool canConfigure = !target.filePath.isEmpty();
    if (canConfigure) {
      msg.setStandardButtons(ThemedMessageBox::Ok | ThemedMessageBox::Cancel);
      msg.setButtonText(ThemedMessageBox::Ok, tr("Choose How to Run…"));
      msg.setButtonText(ThemedMessageBox::Cancel, tr("Close"));
      msg.setDefaultButton(ThemedMessageBox::Ok);
    }

    const int answer = msg.exec();
    if (canConfigure && answer == ThemedMessageBox::Ok) {
      openRunConfigurationForFile(target.filePath);
    }
    return;
  }

  if (target.kind == RunTarget::Kind::CTest) {
    runStructuredTestTarget(target);
    return;
  }

  RunTarget resolved = target;

  if (resolved.requiresBuild) {
    QString programPath;
    QString chosenTarget;
    QString buildError;
    if (!buildCMakeExecutableForSource(resolved.filePath, resolved.program,
                                       resolved.cmakeBinaryDir, &programPath,
                                       &buildError, &chosenTarget)) {
      showBuildFailure(
          tr("Build Failed"),
          tr("Unable to build a runnable target for this file."),
          tr("The build has to succeed before the program can run."),
          buildError);
      return;
    }
    if (!programPath.isEmpty()) {
      resolved.program = programPath;
      resolved.arguments.clear();

      resolved.workingDirectory = QFileInfo(programPath).absolutePath();
      if (!chosenTarget.isEmpty()) {
        resolved.cmakeTargetName = chosenTarget;
      }
    }
  }

  if (resolved.program.isEmpty()) {
    showTerminalPanel();
    terminalWidget->runFile(resolved.filePath, resolved.languageId);
    announceRunTarget(resolved);
    return;
  }

  showTerminalPanel();
  announceRunTarget(resolved);

  const QString filePath = resolved.filePath;
  const QPair<QString, QStringList> command =
      qMakePair(resolved.program, resolved.arguments);
  const QString workingDirectory = resolved.workingDirectory;
  const QMap<QString, QString> customEnv = resolved.environment;

  const FileTemplateAssignment assignment =
      RunTemplateManager::instance().getAssignmentForFile(filePath);

  const QString ext = QFileInfo(filePath).suffix().toLower();
  if (ext == "py" || ext == "pyw" || ext == "pyi") {
    PythonEnvironmentPreference pythonPref;
    pythonPref.mode = assignment.pythonMode;
    pythonPref.customInterpreter = assignment.pythonInterpreter;
    pythonPref.venvPath = assignment.pythonVenvPath;
    pythonPref.requirementsFile = assignment.pythonRequirementsFile;
    const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
        pythonPref, m_projectRootPath, filePath, workingDirectory);
    Terminal *terminal = terminalWidget->currentTerminal();
    if (terminal) {
      terminal->setPythonEnvironmentBanner(info);
    }
  }

  QString preRunCommand = assignment.preRunCommand.trimmed();
  if (!preRunCommand.isEmpty()) {
    preRunCommand = RunTemplateManager::substituteCommandLineVariables(
        preRunCommand, filePath);
  }

  QString postRunCommand = assignment.postRunCommand.trimmed();
  if (!postRunCommand.isEmpty()) {
    postRunCommand = RunTemplateManager::substituteCommandLineVariables(
        postRunCommand, filePath);
  }

  if (m_runProcessFinishedConnection) {
    disconnect(m_runProcessFinishedConnection);
    m_runProcessFinishedConnection = {};
  }
  if (m_runProcessErrorConnection) {
    disconnect(m_runProcessErrorConnection);
    m_runProcessErrorConnection = {};
  }

  auto shellProgramAndArgs =
      [](const QString &commandText) -> QPair<QString, QStringList> {
#ifdef Q_OS_WIN
    return qMakePair(QString("cmd"), QStringList() << "/C" << commandText);
#else
    return qMakePair(QString("bash"), QStringList() << "-lc" << commandText);
#endif
  };

  struct RunExecutionState {
    enum class Stage {
      PreRun,
      MainRun,
      PostRun,
    };

    Stage stage = Stage::MainRun;
  };

  QSharedPointer<RunExecutionState> executionState =
      QSharedPointer<RunExecutionState>::create();
  if (!preRunCommand.isEmpty()) {
    executionState->stage = RunExecutionState::Stage::PreRun;
  }

  QSharedPointer<std::function<void()>> finalizeExecution =
      QSharedPointer<std::function<void()>>::create();
  *finalizeExecution = [this]() {
    if (m_runProcessFinishedConnection) {
      disconnect(m_runProcessFinishedConnection);
      m_runProcessFinishedConnection = {};
    }
    if (m_runProcessErrorConnection) {
      disconnect(m_runProcessErrorConnection);
      m_runProcessErrorConnection = {};
    }
  };

  QSharedPointer<std::function<void()>> runCurrentStage =
      QSharedPointer<std::function<void()>>::create();
  *runCurrentStage = [this, runCurrentStage, executionState, preRunCommand,
                      command, postRunCommand, workingDirectory, customEnv,
                      shellProgramAndArgs]() {
    switch (executionState->stage) {
    case RunExecutionState::Stage::PreRun: {
      QPair<QString, QStringList> shellCommand =
          shellProgramAndArgs(preRunCommand);
      terminalWidget->executeCommand(shellCommand.first, shellCommand.second,
                                     workingDirectory, customEnv);
      return;
    }
    case RunExecutionState::Stage::MainRun:
      terminalWidget->executeCommand(command.first, command.second,
                                     workingDirectory, customEnv);
      return;
    case RunExecutionState::Stage::PostRun: {
      QPair<QString, QStringList> shellCommand =
          shellProgramAndArgs(postRunCommand);
      terminalWidget->executeCommand(shellCommand.first, shellCommand.second,
                                     workingDirectory, customEnv);
      return;
    }
    }
  };

  m_runProcessFinishedConnection = connect(
      terminalWidget, &TerminalTabWidget::processFinished, this,
      [this, executionState, postRunCommand, runCurrentStage, finalizeExecution,
       workingDirectory](int exitCode) {
        if (executionState->stage != RunExecutionState::Stage::PostRun) {
          if (exitCode != 0) {
            publishBuildDiagnostics(terminalWidget->currentRunTranscript(),
                                    workingDirectory);
          } else if (m_diagnosticsManager) {
            m_diagnosticsManager->clearAllForSource(kBuildDiagnosticsSource);
          }
        }

        if (executionState->stage == RunExecutionState::Stage::PreRun) {
          if (exitCode != 0) {
            (*finalizeExecution)();
            return;
          }

          executionState->stage = RunExecutionState::Stage::MainRun;
          (*runCurrentStage)();
          return;
        }

        if (executionState->stage == RunExecutionState::Stage::MainRun) {
          if (!postRunCommand.isEmpty()) {
            executionState->stage = RunExecutionState::Stage::PostRun;
            (*runCurrentStage)();
            return;
          }

          (*finalizeExecution)();
          return;
        }

        if (executionState->stage == RunExecutionState::Stage::PostRun) {
          Q_UNUSED(exitCode);
          (*finalizeExecution)();
          return;
        }
      });

  m_runProcessErrorConnection =
      connect(terminalWidget, &TerminalTabWidget::errorOccurred, this,
              [finalizeExecution](const QString &) { (*finalizeExecution)(); });

  (*runCurrentStage)();
}

void MainWindow::noScriptAssignedWarning() {
  ThemedMessageBox msgBox(this);
  msgBox.setText("No file is currently open.");
  msgBox.setInformativeText(
      "Open a file first, then you can run it or configure a run template.");
  msgBox.setStandardButtons(ThemedMessageBox::Ok | ThemedMessageBox::Cancel);
  int result = msgBox.exec();

  if (result == ThemedMessageBox::Ok)
    on_actionOpen_File_triggered();
}

QString MainWindow::selectedTestConfigurationId() const {
  const TestConfigurationManager &manager =
      TestConfigurationManager::instance();
  const QString explicitId =
      SettingsManager::instance()
          .getValue("activeTestConfigurationId", QString())
          .toString()
          .trimmed();
  if (!explicitId.isEmpty() && manager.configurationById(explicitId).isValid())
    return explicitId;

  const QString defaultId = manager.defaultConfigurationId().trimmed();
  if (!defaultId.isEmpty() && manager.configurationById(defaultId).isValid())
    return defaultId;

  return {};
}

void MainWindow::refreshTestTargetButton() {
  if (!ui->testButton) {
    return;
  }

  TestConfigurationManager &manager = TestConfigurationManager::instance();
  if (manager.allTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  TestConfiguration selectedConfig =
      manager.configurationById(selectedTestConfigurationId());
  if (!selectedConfig.isValid()) {
    QString currentFilePath;
    if (LightpadTabWidget *tabWidget = currentTabWidget()) {
      if (auto *page = tabWidget->getCurrentPage()) {
        currentFilePath = page->getFilePath();
      }
    }
    if (!currentFilePath.isEmpty()) {
      selectedConfig = manager.preferredConfigurationForPath(currentFilePath);
    }
  }

  const QString buttonText =
      selectedConfig.isValid() ? selectedConfig.name : tr("Test");
  const QString tooltip =
      selectedConfig.isValid()
          ? tr("Run tests with configuration: %1").arg(selectedConfig.name)
          : tr("Run tests for the current file or workspace");

  ui->testButton->setText(buttonText);
  ui->testButton->setToolTip(tooltip);
  ui->testButton->setStatusTip(tooltip);
}

void MainWindow::rebuildTestTargetMenu() {
  if (!m_testTargetMenu) {
    return;
  }

  m_testTargetMenu->clear();

  TestConfigurationManager &manager = TestConfigurationManager::instance();
  if (manager.allTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  const QString selectedId =
      SettingsManager::instance()
          .getValue("activeTestConfigurationId", QString())
          .toString()
          .trimmed();

  QAction *automaticAction = m_testTargetMenu->addAction(tr("Automatic"));
  automaticAction->setCheckable(true);
  automaticAction->setChecked(selectedId.isEmpty());
  connect(automaticAction, &QAction::triggered, this, [this]() {
    SettingsManager::instance().setValue("activeTestConfigurationId",
                                         QString());
    SettingsManager::instance().saveSettings();
    refreshTestTargetButton();
  });

  const QList<TestConfiguration> configs = manager.allConfigurations();
  if (!configs.isEmpty()) {
    m_testTargetMenu->addSeparator();
    for (const TestConfiguration &config : configs) {
      if (!config.isValid() || config.id.trimmed().isEmpty()) {
        continue;
      }

      QString label = config.name;
      if (manager.defaultConfigurationId() == config.id) {
        label = tr("%1 (Default)").arg(config.name);
      }

      QAction *action = m_testTargetMenu->addAction(label);
      action->setCheckable(true);
      action->setChecked(config.id == selectedId);
      connect(action, &QAction::triggered, this,
              [this, configId = config.id]() {
                SettingsManager::instance().setValue(
                    "activeTestConfigurationId", configId);
                SettingsManager::instance().saveSettings();
                refreshTestTargetButton();
              });
    }
  }

  m_testTargetMenu->addSeparator();
  QAction *panelAction = m_testTargetMenu->addAction(tr("Show Test Panel"));
  connect(panelAction, &QAction::triggered, this, [this]() {
    ensureTestPanel();
    if (testPanel && !m_projectRootPath.isEmpty())
      testPanel->setWorkspaceFolder(m_projectRootPath);
    if (testDock) {
      testDock->show();
      testDock->raise();
    }
    syncViewToggleActionStates();
  });

  QAction *editAction =
      m_testTargetMenu->addAction(tr("Edit Test Configurations..."));
  connect(editAction, &QAction::triggered, this,
          &MainWindow::openTestConfigurationDialog);
}

QString MainWindow::selectedDebugConfigurationName() const {
  QString selectedName = activeDebugTarget().trimmed();
  if (selectedName.isEmpty() ||
      selectedName.startsWith(QLatin1String(kCompoundDebugTargetPrefix))) {
    return {};
  }

  for (const DebugConfiguration &config :
       DebugConfigurationManager::instance().allConfigurations()) {
    if (config.name == selectedName) {
      return selectedName;
    }
  }

  return {};
}

QString MainWindow::selectedCompoundDebugConfigurationName() const {
  QString selectedName = activeDebugTarget().trimmed();
  if (!selectedName.startsWith(QLatin1String(kCompoundDebugTargetPrefix))) {
    return {};
  }

  selectedName.remove(0, int(strlen(kCompoundDebugTargetPrefix)));
  if (selectedName.isEmpty()) {
    return {};
  }

  for (const CompoundDebugConfiguration &config :
       DebugConfigurationManager::instance().allCompoundConfigurations()) {
    if (config.name == selectedName) {
      return selectedName;
    }
  }

  return {};
}

namespace {
QString elideMiddle(const QString &text, int maxLength) {
  if (text.length() <= maxLength || maxLength < 8) {
    return text;
  }
  const int head = maxLength / 2 - 1;
  const int tail = maxLength - head - 1;
  return text.left(head) + QChar(0x2026) + text.right(tail);
}
} // namespace

void MainWindow::refreshRunTargetButton() {
  if (!ui->runButton) {
    return;
  }

  const QString sourcePath = runSourceFilePath();
  const RunTarget target = resolveRunTargetForFile(sourcePath);
  const bool pinned = !m_pinnedRunFilePath.isEmpty();

  QString label;
  if (!target.isValid()) {
    label = tr("Run");
  } else if (target.needsTargetChoice) {
    label = tr("Run: choose target…");
  } else {
    label = tr("Run: %1").arg(elideMiddle(target.displayName(), 28));
  }
  if (pinned) {

    label = tr("%1 (pinned)").arg(label);
  }
  ui->runButton->setText(label);

  QStringList lines;
  auto addField = [&lines](const QString &caption, const QString &value) {
    lines << QStringLiteral("<b>%1</b> %2")
                 .arg(caption.toHtmlEscaped(), value.toHtmlEscaped());
  };

  if (!target.isValid()) {
    lines
        << QStringLiteral("<b>%1</b>").arg(tr("Nothing to run").toHtmlEscaped())
        << target.unavailableReason.toHtmlEscaped();
  } else {
    addField(tr("Runs:"), QDir::toNativeSeparators(target.targetPath()));
    const QString commandLine = target.commandLine();
    if (!commandLine.isEmpty() && commandLine != target.targetPath()) {
      addField(tr("Command:"), commandLine);
    }
    if (!target.workingDirectory.isEmpty()) {
      addField(tr("Directory:"),
               QDir::toNativeSeparators(target.workingDirectory));
    }
    const QString rationale = target.rationale();
    if (!rationale.isEmpty()) {
      lines << rationale.toHtmlEscaped();
    }
    if (pinned) {
      lines << tr("Pinned, so switching tabs will not change this.")
                   .toHtmlEscaped();
    }
  }

  const QString tooltip =
      QStringLiteral("<div style='white-space:pre-wrap;'>") +
      lines.join(QStringLiteral("<br>")) + QStringLiteral("</div>");
  ui->runButton->setToolTip(tooltip);
  ui->runButton->setStatusTip(
      target.isValid()
          ? tr("Run %1").arg(QDir::toNativeSeparators(target.targetPath()))
          : target.unavailableReason);
  ui->runButton->setAccessibleName(label);

  if (ui->actionRun_file_name) {
    ui->actionRun_file_name->setToolTip(tooltip);
  }
}

void MainWindow::rebuildRunTargetMenu() {
  if (!m_runTargetMenu) {
    return;
  }

  m_runTargetMenu->clear();

  const QString activePath = activeEditorFilePath();
  const QString sourcePath = runSourceFilePath();
  const RunTarget target = resolveRunTargetForFile(sourcePath);

  QAction *summary = m_runTargetMenu->addAction(
      target.isValid()
          ? tr("Runs: %1").arg(QDir::toNativeSeparators(target.targetPath()))
          : tr("Nothing to run"));
  summary->setEnabled(false);

  if (target.isValid()) {
    const QString commandLine = target.commandLine();
    if (!commandLine.isEmpty()) {
      QAction *copyAction = m_runTargetMenu->addAction(tr("Copy Run Command"));
      connect(copyAction, &QAction::triggered, this, [commandLine]() {
        QGuiApplication::clipboard()->setText(commandLine);
      });
    }

    m_runTargetMenu->addSeparator();
    QAction *runNow =
        m_runTargetMenu->addAction(tr("Run %1").arg(target.displayName()));
    connect(runNow, &QAction::triggered, this,
            [this, sourcePath]() { runPath(sourcePath); });
    QAction *debugNow =
        m_runTargetMenu->addAction(tr("Debug %1").arg(target.displayName()));
    connect(debugNow, &QAction::triggered, this,
            [this, sourcePath]() { startDebuggingForFile(sourcePath); });
  }

  m_runTargetMenu->addSeparator();

  QAction *followAction =
      m_runTargetMenu->addAction(tr("Follow Active Editor"));
  followAction->setCheckable(true);
  followAction->setChecked(m_pinnedRunFilePath.isEmpty());
  connect(followAction, &QAction::triggered, this,
          [this]() { setPinnedRunFilePath(QString()); });

  if (!activePath.isEmpty()) {
    QAction *pinAction = m_runTargetMenu->addAction(
        tr("Always Run %1").arg(QFileInfo(activePath).fileName()));
    pinAction->setCheckable(true);
    pinAction->setChecked(!m_pinnedRunFilePath.isEmpty() &&
                          QFileInfo(m_pinnedRunFilePath) ==
                              QFileInfo(activePath));
    connect(pinAction, &QAction::triggered, this,
            [this, activePath](bool checked) {
              setPinnedRunFilePath(checked ? activePath : QString());
            });
  }

  if (!m_pinnedRunFilePath.isEmpty() &&
      (activePath.isEmpty() ||
       QFileInfo(m_pinnedRunFilePath) != QFileInfo(activePath))) {
    QAction *pinnedAction = m_runTargetMenu->addAction(
        tr("Pinned: %1").arg(QFileInfo(m_pinnedRunFilePath).fileName()));
    pinnedAction->setCheckable(true);
    pinnedAction->setChecked(true);
    connect(pinnedAction, &QAction::triggered, this,
            [this]() { setPinnedRunFilePath(QString()); });
  }

  addCMakeTargetActions(m_runTargetMenu, sourcePath);
  addRunTemplateActions(m_runTargetMenu, sourcePath);

  m_runTargetMenu->addSeparator();
  QAction *configureAction = m_runTargetMenu->addAction(
      sourcePath.isEmpty()
          ? tr("Run Configuration…")
          : tr("Configure How %1 Runs…").arg(QFileInfo(sourcePath).fileName()));
  connect(configureAction, &QAction::triggered, this,
          [this, sourcePath]() { openRunConfigurationForFile(sourcePath); });
}

void MainWindow::addCMakeTargetActions(QMenu *menu, const QString &filePath) {
  if (!menu || filePath.isEmpty()) {
    return;
  }

  const QString cmakeRoot =
      RunTargetResolver::cmakeRootFor(filePath, m_projectRootPath);
  if (cmakeRoot.isEmpty()) {
    return;
  }

  CMakeProject project;
  const QList<CMakeTargetInfo> executables =
      project.parseExecutableTargets(cmakeRoot);
  if (executables.size() < 2) {
    return;
  }

  QMenu *targetMenu = menu->addMenu(tr("CMake Target"));
  QAction *autoAction = targetMenu->addAction(tr("Automatic (target owning "
                                                 "the file)"));
  autoAction->setCheckable(true);
  autoAction->setChecked(effectivePreferredCMakeTarget().isEmpty());
  connect(autoAction, &QAction::triggered, this,
          [this]() { setPreferredCMakeTarget(QString()); });

  targetMenu->addSeparator();
  for (const CMakeTargetInfo &executable : executables) {
    QAction *action = targetMenu->addAction(executable.name);
    action->setCheckable(true);
    action->setChecked(executable.name == effectivePreferredCMakeTarget());
    connect(
        action, &QAction::triggered, this,
        [this, name = executable.name]() { setPreferredCMakeTarget(name); });
  }
}

void MainWindow::addRunTemplateActions(QMenu *menu, const QString &filePath) {
  if (!menu || filePath.isEmpty()) {
    return;
  }

  RunTemplateManager &manager = RunTemplateManager::instance();
  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  QList<RunTemplate> candidates = manager.getTemplatesForFilePath(filePath);
  const QString languageId = effectiveLanguageIdForFile(filePath);
  for (const RunTemplate &languageTemplate :
       manager.getTemplatesForLanguageId(languageId)) {
    bool alreadyListed = false;
    for (const RunTemplate &candidate : candidates) {
      if (candidate.id == languageTemplate.id) {
        alreadyListed = true;
        break;
      }
    }
    if (!alreadyListed) {
      candidates.append(languageTemplate);
    }
  }

  if (candidates.isEmpty()) {
    return;
  }

  const QString activeId =
      manager.effectiveTemplateIdForFile(filePath, languageId);
  QMenu *templateMenu = menu->addMenu(tr("Run Template"));
  for (const RunTemplate &candidate : candidates) {
    QAction *action = templateMenu->addAction(candidate.name);
    action->setCheckable(true);
    action->setChecked(candidate.id == activeId);
    action->setToolTip(candidate.description);
    connect(action, &QAction::triggered, this,
            [this, filePath, id = candidate.id]() {
              RunTemplateManager &templates = RunTemplateManager::instance();
              FileTemplateAssignment assignment =
                  templates.getAssignmentForFile(filePath);
              assignment.filePath = filePath;
              assignment.templateId = id;
              templates.assignTemplateToFile(filePath, assignment);
              refreshRunTargetButton();
            });
  }
}

void MainWindow::openRunConfigurationForFile(const QString &filePath) {
  if (filePath.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Run Configuration"),
        tr("Please open a file first to configure run settings."));
    return;
  }

  ensureProjectRootForPath(filePath);

  if (findChildren<RunTemplateSelector *>().isEmpty()) {
    auto selector = new RunTemplateSelector(filePath, this);
    selector->setAttribute(Qt::WA_DeleteOnClose);
    connect(selector, &QObject::destroyed, this,
            [this]() { refreshRunTargetButton(); });
    selector->show();
  }
}

void MainWindow::refreshDebugTargetButton() {
  const QString selectedCompoundName = selectedCompoundDebugConfigurationName();
  const QString selectedName = selectedDebugConfigurationName();

  const QString quickSource = runSourceFilePath();
  const RunTarget quickTarget = resolveRunTargetForFile(quickSource);
  const QString quickName = quickTarget.isValid()
                                ? quickTarget.displayName()
                                : QFileInfo(quickSource).fileName();

  QString buttonText;
  QString tooltip;
  if (!selectedCompoundName.isEmpty()) {
    buttonText = tr("Compound: %1").arg(selectedCompoundName);
    tooltip =
        tr("Start compound debug configuration: %1").arg(selectedCompoundName);
  } else if (!selectedName.isEmpty()) {
    buttonText = selectedName;
    tooltip = tr("Start debug configuration: %1").arg(selectedName);
  } else if (!quickName.isEmpty()) {
    buttonText = tr("Debug: %1").arg(elideMiddle(quickName, 26));
    tooltip = tr("Debug %1")
                  .arg(QDir::toNativeSeparators(quickTarget.isValid()
                                                    ? quickTarget.targetPath()
                                                    : quickSource));
  } else {
    buttonText = tr("Debug");
    tooltip = tr("Open a file to debug it.");
  }

  ui->debugButton->setText(buttonText);
  ui->debugButton->setToolTip(tooltip);
  ui->debugButton->setStatusTip(tooltip);
  ui->debugButton->setAccessibleName(buttonText);

  if (!selectedCompoundName.isEmpty()) {
    setActiveDebugTarget(QString::fromLatin1(kCompoundDebugTargetPrefix) +
                         selectedCompoundName);
  } else if (!selectedName.isEmpty()) {
    setActiveDebugTarget(selectedName);
  } else if (!activeDebugTarget().isEmpty()) {
    setActiveDebugTarget(QString());
  }
}

void MainWindow::rebuildDebugTargetMenu() {
  if (!m_debugTargetMenu) {
    return;
  }

  m_debugTargetMenu->clear();

  const QString selectedCompoundName = selectedCompoundDebugConfigurationName();
  const QString selectedName = selectedDebugConfigurationName();
  const QString quickSource = runSourceFilePath();
  QAction *quickDebugAction = m_debugTargetMenu->addAction(
      quickSource.isEmpty()
          ? tr("Quick Debug Current File")
          : tr("Quick Debug %1").arg(QFileInfo(quickSource).fileName()));
  quickDebugAction->setCheckable(true);
  quickDebugAction->setChecked(selectedName.isEmpty() &&
                               selectedCompoundName.isEmpty());
  connect(quickDebugAction, &QAction::triggered, this, [this]() {
    setActiveDebugTarget(QString());
    SettingsManager::instance().saveSettings();
    refreshDebugTargetButton();
  });

  const QList<DebugConfiguration> configs =
      DebugConfigurationManager::instance().allConfigurations();
  if (!configs.isEmpty()) {
    m_debugTargetMenu->addSeparator();
    for (const DebugConfiguration &config : configs) {
      if (config.name.trimmed().isEmpty()) {
        continue;
      }

      QAction *action = m_debugTargetMenu->addAction(config.name);
      action->setCheckable(true);
      action->setChecked(config.name == selectedName);
      connect(action, &QAction::triggered, this,
              [this, configName = config.name]() {
                setActiveDebugTarget(configName);
                SettingsManager::instance().setValue("lastDebugConfiguration",
                                                     configName);
                SettingsManager::instance().saveSettings();
                refreshDebugTargetButton();
              });
    }
  }

  const QList<CompoundDebugConfiguration> compounds =
      DebugConfigurationManager::instance().allCompoundConfigurations();
  if (!compounds.isEmpty()) {
    m_debugTargetMenu->addSeparator();
    for (const CompoundDebugConfiguration &compound : compounds) {
      if (compound.name.trimmed().isEmpty()) {
        continue;
      }

      QAction *action =
          m_debugTargetMenu->addAction(tr("Compound: %1").arg(compound.name));
      action->setCheckable(true);
      action->setChecked(compound.name == selectedCompoundName);
      connect(action, &QAction::triggered, this,
              [this, compoundName = compound.name]() {
                setActiveDebugTarget(
                    QString::fromLatin1(kCompoundDebugTargetPrefix) +
                    compoundName);
                SettingsManager::instance().setValue("lastDebugConfiguration",
                                                     compoundName);
                SettingsManager::instance().saveSettings();
                refreshDebugTargetButton();
              });
    }
  }

  m_debugTargetMenu->addSeparator();
  QAction *chooseAction =
      m_debugTargetMenu->addAction(tr("Choose Configuration..."));
  connect(chooseAction, &QAction::triggered, this,
          &MainWindow::on_actionStart_Debug_Configuration_triggered);
  QAction *editAction =
      m_debugTargetMenu->addAction(tr("Edit Debug Configurations..."));
  connect(editAction, &QAction::triggered, this,
          &MainWindow::on_actionEdit_Debug_Configurations_triggered);
}

bool MainWindow::runBuildProcessWithProgress(const QString &title,
                                             const QStringList &command,
                                             const QString &workingDirectory,
                                             QString *output,
                                             QString *errorMessage) {
  if (output) {
    output->clear();
  }
  if (command.isEmpty()) {
    if (errorMessage) {
      *errorMessage = tr("No build command to run.");
    }
    return false;
  }

  if (m_buildInProgress) {
    if (errorMessage) {
      *errorMessage = tr("A build is already running.");
    }
    return false;
  }

  m_lastBuildDirectory = workingDirectory;

  TerminalTabWidget *terminal = ensureTerminalWidget();
  if (!terminal) {
    if (errorMessage) {
      *errorMessage = tr("The terminal panel is unavailable.");
    }
    return false;
  }

  showTerminalPanel();
  statusBar()->showMessage(title);

  if (m_runProcessFinishedConnection) {
    disconnect(m_runProcessFinishedConnection);
    m_runProcessFinishedConnection = {};
  }
  if (m_runProcessErrorConnection) {
    disconnect(m_runProcessErrorConnection);
    m_runProcessErrorConnection = {};
  }

  m_buildInProgress = true;

  QEventLoop loop;
  int exitCode = -1;
  bool startFailed = false;
  QString startError;

  const QMetaObject::Connection finishedConnection =
      connect(terminal, &TerminalTabWidget::processFinished, &loop,
              [&loop, &exitCode](int code) {
                exitCode = code;
                loop.quit();
              });
  const QMetaObject::Connection errorConnection =
      connect(terminal, &TerminalTabWidget::errorOccurred, &loop,
              [&loop, &startFailed, &startError](const QString &message) {
                startFailed = true;
                startError = message;
                loop.quit();
              });

  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  timeoutTimer.setInterval(600000);
  connect(&timeoutTimer, &QTimer::timeout, terminal,
          [terminal]() { terminal->stopCurrentProcess(); });
  timeoutTimer.start();

  terminal->executeCommand(command.first(), command.mid(1), workingDirectory);
  loop.exec();

  timeoutTimer.stop();
  disconnect(finishedConnection);
  disconnect(errorConnection);
  m_buildInProgress = false;

  const QString transcript = terminal->currentRunTranscript();
  if (output) {
    *output = transcript;
  }

  if (startFailed) {
    if (errorMessage) {
      *errorMessage = tr("Could not run '%1': %2")
                          .arg(command.join(QLatin1Char(' ')), startError);
    }
    statusBar()->clearMessage();
    return false;
  }

  if (exitCode != 0) {
    if (errorMessage) {
      const QString details = transcript.trimmed();
      *errorMessage =
          tr("'%1' failed:\n%2")
              .arg(command.join(QLatin1Char(' ')),
                   details.isEmpty() ? tr("exited with code %1").arg(exitCode)
                                     : details.right(8000));
    }
    statusBar()->showMessage(tr("%1 failed").arg(title), 8000);
    return false;
  }

  if (m_diagnosticsManager) {

    m_diagnosticsManager->clearAllForSource(kBuildDiagnosticsSource);
  }
  statusBar()->clearMessage();
  return true;
}

bool MainWindow::runPreLaunchTask(const QString &taskCommand,
                                  QString *errorMessage) {
  const QString command = taskCommand.trimmed();
  if (command.isEmpty()) {
    return true;
  }

#ifdef Q_OS_WIN
  const QString shell = QStringLiteral("cmd.exe");
  const QStringList args = {QStringLiteral("/C"), command};
#else
  const QString shell = QStringLiteral("sh");
  const QStringList args = {QStringLiteral("-c"), command};
#endif

  QProcess process;
  process.setWorkingDirectory(m_projectRootPath.isEmpty() ? QDir::homePath()
                                                          : m_projectRootPath);
  process.start(shell, args);

  if (!process.waitForStarted(5000)) {
    if (errorMessage) {
      *errorMessage = tr("Failed to run pre-launch task '%1': %2")
                          .arg(command, process.errorString());
    }
    return false;
  }
  if (!process.waitForFinished(600000)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = tr("Pre-launch task timed out: %1").arg(command);
    }
    return false;
  }

  if (process.exitCode() != 0 || process.exitStatus() != QProcess::NormalExit) {
    const QString details = QString::fromUtf8(process.readAllStandardOutput() +
                                              process.readAllStandardError())
                                .trimmed();
    if (errorMessage) {
      *errorMessage =
          tr("Pre-launch task failed:\n%1\n\n%2")
              .arg(command,
                   details.isEmpty()
                       ? tr("exited with code %1").arg(process.exitCode())
                       : details.left(4000));
    }
    return false;
  }

  LOG_INFO(QString("Pre-launch task completed: %1").arg(command));
  return true;
}

QString MainWindow::cmakeRootForSource(const QString &filePath) const {

  return RunTargetResolver::cmakeRootFor(filePath, m_projectRootPath);
}

bool MainWindow::buildCMakeExecutableForSource(
    const QString &filePath, const QString &configuredProgram,
    const QString &configuredBinaryDir, QString *programPath,
    QString *errorMessage, QString *chosenTargetName) {
  if (programPath) {
    programPath->clear();
  }
  if (chosenTargetName) {
    chosenTargetName->clear();
  }

  const QString cmakeRoot = cmakeRootForSource(filePath);
  if (cmakeRoot.isEmpty()) {

    return true;
  }

  CMakeProject project;
  const QList<CMakeTargetInfo> executables =
      project.parseExecutableTargets(cmakeRoot);

  const QString binaryDir =
      CMakeProject::resolveBinaryDirectory(cmakeRoot, configuredBinaryDir);

  QString exePath;
  QString targetName;
  if (!executables.isEmpty()) {
    if (!configuredProgram.isEmpty()) {
      const QString programFile = QFileInfo(configuredProgram).fileName();
      const QString programStem =
          QFileInfo(configuredProgram).completeBaseName();
      for (const CMakeTargetInfo &target : executables) {
        if (target.name == programFile || target.name == programStem) {
          exePath = CMakeProject::executablePathFor(target, binaryDir);
          targetName = target.name;
          break;
        }
      }
    }

    if (exePath.isEmpty()) {
      const QString owner =
          CMakeProject::targetForSource(executables, cmakeRoot, filePath);
      if (!owner.isEmpty()) {
        for (const CMakeTargetInfo &target : executables) {
          if (target.name == owner) {
            exePath = CMakeProject::executablePathFor(target, binaryDir);
            targetName = target.name;
            break;
          }
        }
      }
    }

    const QString preferredTarget = effectivePreferredCMakeTarget();
    if (exePath.isEmpty() && !preferredTarget.isEmpty()) {
      for (const CMakeTargetInfo &target : executables) {
        if (target.name == preferredTarget) {
          exePath = CMakeProject::executablePathFor(target, binaryDir);
          targetName = target.name;
          break;
        }
      }
    }

    const bool sourceIsCMakeLists =
        QFileInfo(filePath).fileName().compare(QStringLiteral("CMakeLists.txt"),
                                               Qt::CaseInsensitive) == 0;
    if (exePath.isEmpty() && executables.size() == 1 &&
        (sourceIsCMakeLists ||
         CMakeProject::hasUnresolvedSources(executables))) {
      exePath = CMakeProject::executablePathFor(executables.first(), binaryDir);
      targetName = executables.first().name;
    }

    if (exePath.isEmpty()) {
      CMakeTargetPickerDialog picker(executables, this);
      picker.setWindowModality(Qt::ApplicationModal);
      if (picker.exec() != QDialog::Accepted ||
          picker.selectedTargetName().isEmpty()) {
        if (errorMessage) {
          *errorMessage = tr("A build target must be selected to run or debug "
                             "this CMake project.");
        }
        return false;
      }
      for (const CMakeTargetInfo &target : executables) {
        if (target.name == picker.selectedTargetName()) {
          exePath = CMakeProject::executablePathFor(target, binaryDir);
          targetName = target.name;
          break;
        }
      }
      setPreferredCMakeTarget(targetName);
    }
  } else if (configuredProgram.isEmpty()) {
    if (errorMessage) {
      *errorMessage =
          tr("No executable targets were found in %1. Add an "
             "add_executable() declaration or point 'program' at a built "
             "binary.")
              .arg(cmakeRoot);
    }
    return false;
  }

  QString configureOutput;
  if (CMakeProject::needsConfigure(binaryDir)) {
    if (!runBuildProcessWithProgress(
            tr("Configuring CMake project…"),
            CMakeProject::configureCommand(cmakeRoot, binaryDir), cmakeRoot,
            &configureOutput, errorMessage)) {
      return false;
    }
  }

  QString buildOutput;
  const int jobs = qMax(1, QThread::idealThreadCount());

  const QString buildTitle = targetName.isEmpty()
                                 ? tr("Building project…")
                                 : tr("Building %1…").arg(targetName);
  if (!runBuildProcessWithProgress(
          buildTitle, CMakeProject::buildCommand(binaryDir, jobs, targetName),
          binaryDir, &buildOutput, errorMessage)) {
    return false;
  }

  LOG_INFO(QString("CMake build output tail: %1")
               .arg(buildOutput.trimmed().section(QLatin1Char('\n'), -3, -1)));

  const QString resolved = exePath.isEmpty() ? configuredProgram : exePath;
  if (resolved.isEmpty() || !QFileInfo::exists(resolved)) {
    if (errorMessage) {
      *errorMessage = tr("The built program was not found: %1")
                          .arg(resolved.isEmpty() ? tr("(no executable target)")
                                                  : resolved);
    }
    return false;
  }

  if (programPath) {
    *programPath = resolved;
  }
  if (chosenTargetName) {
    *chosenTargetName = targetName;
  }
  LOG_INFO(QString("CMake target ready: %1").arg(resolved));
  return true;
}

bool MainWindow::buildCMakeDebugTarget(DebugConfiguration *resolvedConfig,
                                       const QString &currentFilePath,
                                       QString *errorMessage) {
  if (!resolvedConfig) {
    return false;
  }
  if (resolvedConfig->request.compare("attach", Qt::CaseInsensitive) == 0) {
    return true;
  }

  const QString languageId = currentFilePath.isEmpty()
                                 ? QString()
                                 : effectiveLanguageIdForFile(currentFilePath);
  const QString normalizedLanguageId = LanguageCatalog::normalize(languageId);
  const bool cppLike =
      normalizedLanguageId == "cpp" || normalizedLanguageId == "c";
  if (!cppLike) {
    return true;
  }

  const QVariant configuredBinaryDir =
      resolvedConfig->adapterConfig.value(QStringLiteral("cmakeBinaryDir"));

  QString programPath;
  if (!buildCMakeExecutableForSource(
          currentFilePath, resolvedConfig->program.trimmed(),
          configuredBinaryDir.toString(), &programPath, errorMessage)) {
    return false;
  }
  if (!programPath.isEmpty()) {
    resolvedConfig->program = programPath;
  }
  return true;
}

bool MainWindow::prepareDebugConfigurationForStart(
    const DebugConfiguration &config, const QString &currentFilePath,
    DebugConfiguration *resolvedConfig, QString *errorMessage) {
  if (!resolvedConfig) {
    if (errorMessage) {
      *errorMessage =
          tr("Internal error: missing resolved configuration output.");
    }
    return false;
  }

  *resolvedConfig = DebugConfigurationManager::instance().resolveVariables(
      config, currentFilePath);

  if (resolvedConfig->request.compare("attach", Qt::CaseInsensitive) == 0 &&
      resolvedConfig->processId <= 0) {
    const bool placeholderInExpression =
        resolvedConfig->processIdExpression.trimmed() ==
        QStringLiteral("${command:pickProcess}");
    const QVariant adapterProcessId =
        resolvedConfig->adapterConfig.value(QStringLiteral("processId"));
    const bool placeholderInAdapterConfig =
        adapterProcessId.isValid() &&
        adapterProcessId.toString().trimmed() ==
            QStringLiteral("${command:pickProcess}");

    if (placeholderInExpression || placeholderInAdapterConfig) {
      ProcessPickerDialog picker(this);
      picker.setWindowModality(Qt::ApplicationModal);
      if (picker.exec() != QDialog::Accepted || picker.selectedPid() <= 0) {
        if (errorMessage) {
          *errorMessage =
              tr("A process must be selected to attach the debugger.");
        }
        return false;
      }

      resolvedConfig->processId = static_cast<int>(
          qBound<qint64>(1, picker.selectedPid(), qint64(4194304)));
      resolvedConfig->processIdExpression.clear();
      if (placeholderInAdapterConfig) {
        resolvedConfig->adapterConfig.remove(QStringLiteral("processId"));
      }
    }
  }

  if (!runPreLaunchTask(resolvedConfig->preLaunchTask, errorMessage)) {
    return false;
  }

  if (!buildCMakeDebugTarget(resolvedConfig, currentFilePath, errorMessage)) {
    return false;
  }

  const auto hasUnresolvedCurrentFileVariable = [](const QString &value) {
    return value.contains("${file}") || value.contains("${fileDirname}") ||
           value.contains("${fileBasename}") ||
           value.contains("${fileBasenameNoExtension}") ||
           value.contains("${fileExtname}") ||
           value.contains("${relativeFile}");
  };

  if (currentFilePath.isEmpty()) {
    bool needsCurrentFile = hasUnresolvedCurrentFileVariable(config.program) ||
                            hasUnresolvedCurrentFileVariable(config.cwd);
    for (const QString &arg : config.args) {
      if (hasUnresolvedCurrentFileVariable(arg)) {
        needsCurrentFile = true;
        break;
      }
    }
    if (needsCurrentFile) {
      if (errorMessage) {
        *errorMessage =
            tr("The selected debug configuration requires an open file.");
      }
      return false;
    }
  }

  if (resolvedConfig->request == "launch" && !currentFilePath.isEmpty()) {
    const QString absoluteCurrentFile =
        QFileInfo(currentFilePath).absoluteFilePath();
    const QString absoluteProgramPath =
        resolvedConfig->program.isEmpty()
            ? QString()
            : QFileInfo(resolvedConfig->program).absoluteFilePath();
    if (absoluteProgramPath == absoluteCurrentFile) {
      const QString languageId = effectiveLanguageIdForFile(currentFilePath);
      QString prepareError;
      QString preparedProgram;
      if (!prepareDebugTargetForFile(currentFilePath, languageId,
                                     &preparedProgram, &prepareError)) {
        if (errorMessage) {
          *errorMessage = prepareError;
        }
        return false;
      }

      if (!preparedProgram.isEmpty()) {
        resolvedConfig->program = preparedProgram;
      }
    }
  }

  return true;
}

bool MainWindow::startDebugConfigurationByName(
    const QString &configurationName) {
  const QString selectedName = configurationName.trimmed();
  if (selectedName.isEmpty()) {
    return false;
  }

  if (m_debugStartInProgress) {
    return false;
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
    return false;
  }

  m_debugStartInProgress = true;

  DebugSettings::instance().initialize(m_projectRootPath);
  DebugConfigurationManager::instance().setWorkspaceFolder(m_projectRootPath);
  DebugConfigurationManager::instance().loadFromLightpadDir();
  BreakpointManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (BreakpointManager::instance().allBreakpoints().isEmpty()) {
    BreakpointManager::instance().loadFromLightpadDir();
  }
  WatchManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (WatchManager::instance().allWatches().isEmpty()) {
    WatchManager::instance().loadFromLightpadDir();
  }

  const DebugConfiguration config =
      DebugConfigurationManager::instance().configuration(selectedName);
  if (config.name.trimmed().isEmpty()) {
    ThemedMessageBox::warning(
        this, tr("Debug Configuration"),
        tr("The selected debug configuration could not be found."));
    setActiveDebugTarget(QString());
    SettingsManager::instance().saveSettings();
    refreshDebugTargetButton();
    m_debugStartInProgress = false;
    return false;
  }

  SettingsManager::instance().setValue("lastDebugConfiguration", selectedName);
  setActiveDebugTarget(selectedName);
  SettingsManager::instance().saveSettings();
  refreshDebugTargetButton();

  DebugConfiguration resolvedConfig;
  QString prepareError;
  if (!prepareDebugConfigurationForStart(config, currentFilePath,
                                         &resolvedConfig, &prepareError)) {
    ThemedMessageBox msg(this);
    msg.setIcon(ThemedMessageBox::Warning);
    msg.setWindowTitle(prepareError.contains('\n') ? tr("Debug Build Failed")
                                                   : tr("Debug Configuration"));
    msg.setText(prepareError.contains('\n')
                    ? tr("Unable to prepare a debuggable target for this "
                         "configuration.")
                    : prepareError);
    if (prepareError.contains('\n')) {
      msg.setInformativeText(prepareError);
    }
    msg.exec();
    m_debugStartInProgress = false;
    return false;
  }

  const QString sessionId =
      DebugSessionManager::instance().startSession(resolvedConfig);
  if (sessionId.isEmpty()) {
    ThemedMessageBox::warning(
        this, tr("Debug Configuration"),
        DebugSessionManager::instance().lastError().isEmpty()
            ? tr("Unable to start the selected debug configuration.")
            : DebugSessionManager::instance().lastError());
    m_debugStartInProgress = false;
    return false;
  }

  attachDebugSession(sessionId);
  if (debugDock) {
    debugDock->show();
    debugDock->raise();
  }
  m_debugStartInProgress = false;
  return true;
}

bool MainWindow::startCompoundDebugConfigurationByName(
    const QString &compoundName) {
  const QString selectedName = compoundName.trimmed();
  if (selectedName.isEmpty()) {
    return false;
  }

  if (m_debugStartInProgress) {
    return false;
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
        this, tr("Compound Debug Configuration"),
        tr("Open a file or project first to start a compound configuration."));
    return false;
  }

  m_debugStartInProgress = true;

  DebugSettings::instance().initialize(m_projectRootPath);
  DebugConfigurationManager::instance().setWorkspaceFolder(m_projectRootPath);
  DebugConfigurationManager::instance().loadFromLightpadDir();
  BreakpointManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (BreakpointManager::instance().allBreakpoints().isEmpty()) {
    BreakpointManager::instance().loadFromLightpadDir();
  }
  WatchManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (WatchManager::instance().allWatches().isEmpty()) {
    WatchManager::instance().loadFromLightpadDir();
  }

  CompoundDebugConfiguration selectedCompound;
  bool foundCompound = false;
  for (const CompoundDebugConfiguration &compound :
       DebugConfigurationManager::instance().allCompoundConfigurations()) {
    if (compound.name == selectedName) {
      selectedCompound = compound;
      foundCompound = true;
      break;
    }
  }

  if (!foundCompound) {
    ThemedMessageBox::warning(
        this, tr("Compound Debug Configuration"),
        tr("The selected compound configuration could not be found."));
    setActiveDebugTarget(QString());
    SettingsManager::instance().saveSettings();
    refreshDebugTargetButton();
    m_debugStartInProgress = false;
    return false;
  }

  if (selectedCompound.configurations.isEmpty()) {
    ThemedMessageBox::warning(
        this, tr("Compound Debug Configuration"),
        tr("The selected compound configuration is empty."));
    m_debugStartInProgress = false;
    return false;
  }

  setActiveDebugTarget(QString::fromLatin1(kCompoundDebugTargetPrefix) +
                       selectedName);
  SettingsManager::instance().setValue("lastDebugConfiguration", selectedName);
  SettingsManager::instance().saveSettings();
  refreshDebugTargetButton();

  QList<DebugConfiguration> resolvedConfigs;
  for (const QString &configurationName : selectedCompound.configurations) {
    const DebugConfiguration config =
        DebugConfigurationManager::instance().configuration(configurationName);
    if (config.name.trimmed().isEmpty()) {
      ThemedMessageBox::warning(
          this, tr("Compound Debug Configuration"),
          tr("Compound entry '%1' does not match a saved debug configuration.")
              .arg(configurationName));
      m_debugStartInProgress = false;
      return false;
    }

    DebugConfiguration resolvedConfig;
    QString prepareError;
    if (!prepareDebugConfigurationForStart(config, currentFilePath,
                                           &resolvedConfig, &prepareError)) {
      ThemedMessageBox msg(this);
      msg.setIcon(ThemedMessageBox::Warning);
      msg.setWindowTitle(prepareError.contains('\n')
                             ? tr("Debug Build Failed")
                             : tr("Compound Debug Configuration"));
      msg.setText(prepareError.contains('\n')
                      ? tr("Unable to prepare '%1' for debugging.")
                            .arg(configurationName)
                      : prepareError);
      if (prepareError.contains('\n')) {
        msg.setInformativeText(prepareError);
      }
      msg.exec();
      m_debugStartInProgress = false;
      return false;
    }

    resolvedConfigs.append(resolvedConfig);
  }

  QStringList startedSessionIds;
  for (const DebugConfiguration &resolvedConfig : resolvedConfigs) {
    const QString sessionId =
        DebugSessionManager::instance().startSession(resolvedConfig);
    if (sessionId.isEmpty()) {
      for (const QString &startedSessionId : startedSessionIds) {
        DebugSessionManager::instance().stopSession(startedSessionId,
                                                    selectedCompound.stopAll);
      }
      ThemedMessageBox::warning(
          this, tr("Compound Debug Configuration"),
          DebugSessionManager::instance().lastError().isEmpty()
              ? tr("Unable to start one of the selected debug configurations.")
              : DebugSessionManager::instance().lastError());
      m_debugStartInProgress = false;
      return false;
    }

    startedSessionIds.append(sessionId);
  }

  if (!startedSessionIds.isEmpty()) {
    attachDebugSession(startedSessionIds.first());
    if (debugDock) {
      debugDock->show();
      debugDock->raise();
    }
  }

  m_debugStartInProgress = false;
  return !startedSessionIds.isEmpty();
}

