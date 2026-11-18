#include "mainwindow_internal.h"

void MainWindow::showFindReplace(bool onlyFind) {
  if (!findReplacePanel) {
    findReplacePanel = new FindReplacePanel(onlyFind);

    auto layout = qobject_cast<QBoxLayout *>(ui->centralwidget->layout());

    if (layout != 0)
      layout->addWidget(findReplacePanel, 0);

    connect(
        findReplacePanel, &FindReplacePanel::navigateToFile, this,
        [this](const QString &filePath, int lineNumber, int columnNumber,
               int matchLength) {
          if (!filePath.isEmpty()) {
            openFileAndAddToNewTab(filePath);
          }
          TextArea *textArea = getCurrentTextArea();
          if (textArea) {
            const QTextBlock block = textArea->document()->findBlockByNumber(
                qMax(0, lineNumber - 1));
            if (!block.isValid())
              return;
            QTextCursor cursor(textArea->document());
            cursor.setPosition(block.position() +
                               qBound(0, columnNumber - 1, block.length() - 1));
            cursor.setPosition(qMin(cursor.position() + matchLength,
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
    openLaunchConfigurations(runSourceFilePath(), activeLaunchTarget());
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

  case Dialog::debugConfiguration:
    openLaunchConfigurations(runSourceFilePath(), activeLaunchTarget());
    break;

  case Dialog::shortcuts:
    if (findChildren<ShortcutsDialog *>().isEmpty())
      new ShortcutsDialog(this);
    break;

  default:
    return;
  }
}

void MainWindow::openFormatConfigurationDialog() {
  openDialog(Dialog::formatConfiguration);
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
    if (!m_projectRootPath.isEmpty() &&
        QDir::cleanPath(m_projectRootPath) != QDir::currentPath()) {
      terminalWidget->resetForWorkspace(m_projectRootPath);
    }

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

  const QString buttonText = selectedConfig.isValid()
                                 ? tr("Test: %1").arg(selectedConfig.name)
                                 : tr("Test");
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
    revealTestDock();
    syncViewToggleActionStates();
  });

  QAction *editAction =
      m_testTargetMenu->addAction(tr("Edit Test Configurations..."));
  connect(editAction, &QAction::triggered, this,
          &MainWindow::openTestConfigurationDialog);
}
