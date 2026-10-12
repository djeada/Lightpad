#include "mainwindow_internal.h"

void MainWindow::showProblemsPanel() {
  if (m_vimCommandPanelActive) {
    ensureStatusLabels();
    return;
  }
  if (!problemsPanel) {
    problemsPanel = new ProblemsPanel(this);

    connect(problemsPanel, &ProblemsPanel::problemClicked, this,
            [this](const QString &filePath, int line, int column) {
              openFileAndAddToNewTab(filePath);
              TextArea *textArea = getCurrentTextArea();
              if (textArea) {
                QTextCursor cursor = textArea->textCursor();
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                    line);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                    column);
                textArea->setTextCursor(cursor);
                textArea->setFocus();
              }
            });

    connect(problemsPanel, &ProblemsPanel::countsChanged, this,
            &MainWindow::updateProblemsStatusLabel);

    connect(problemsPanel, &ProblemsPanel::refreshRequested, this,
            [this](const QString &filePath) {
              notifyDiagnosticsFileSaved(filePath);
            });

    connect(problemsPanel, &ProblemsPanel::closeRequested, this, [this]() {
      if (m_problemsDock) {
        m_problemsDock->hide();
      }
    });

    if (m_diagnosticsManager) {
      for (const QString &uri : m_diagnosticsManager->allUris()) {
        QList<LspDiagnostic> diags =
            m_diagnosticsManager->diagnosticsForUri(uri);
        problemsPanel->setDiagnostics(uri, diags);
      }
    }

    LightpadTabWidget *tabWidget = currentTabWidget();
    if (tabWidget) {
      QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
      if (!filePath.isEmpty()) {
        problemsPanel->setCurrentFilePath(filePath);
      }
    }

    ensureStatusLabels();

    m_problemsDock = new QDockWidget(tr("Problems"), this);
    m_problemsDock->setObjectName("problemsDock");
    DockUtils::configureToolPanelDock(m_problemsDock);
    m_problemsDock->setWidget(problemsPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_problemsDock);
    tabifyBottomDock(m_problemsDock);
    trackDockLayoutChanges(m_problemsDock);
    m_problemsDock->hide();
    connect(m_problemsDock, &QDockWidget::visibilityChanged, this,
            [this](bool visible) {
              Q_UNUSED(visible)
              syncViewToggleActionStates();
            });
  }

  if (!m_vimCommandPanelActive && m_problemsDock) {
    bool visible = m_problemsDock->isVisible();
    if (!visible) {
      m_problemsDock->show();
      m_problemsDock->raise();
    } else {
      m_problemsDock->hide();
    }
  }
}

void MainWindow::ensureStatusLabels() {
  Theme theme = getTheme();
  if (!problemsStatusLabel) {
    problemsStatusLabel = new QLabel(this);
    problemsStatusLabel->setText("✓ No problems");
    problemsStatusLabel->setCursor(Qt::PointingHandCursor);

    problemsStatusLabel->installEventFilter(this);

    auto layout = qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout());
    if (layout) {

      layout->insertWidget(layout->count() - 1, problemsStatusLabel);
    }
  }

  if (!vimStatusLabel) {
    vimStatusLabel = new QLabel(this);
    vimStatusLabel->setText("");
    vimStatusLabel->setVisible(false);
    vimStatusLabel->setMinimumWidth(70);
    vimStatusLabel->setAlignment(Qt::AlignCenter);

    auto layout = qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout());
    if (layout) {
      int insertIndex = layout->count() - 1;
      if (problemsStatusLabel) {
        insertIndex = qMax(0, layout->indexOf(problemsStatusLabel) + 1);
      }
      layout->insertWidget(insertIndex, vimStatusLabel);
    }
  }

  if (!m_pythonEnvLabel) {
    m_pythonEnvLabel = new QToolButton(this);
    m_pythonEnvLabel->setStyleSheet(statusBarTextButtonStyle());
    m_pythonEnvLabel->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_pythonEnvLabel->setAutoRaise(true);
    m_pythonEnvLabel->setCursor(Qt::PointingHandCursor);
    m_pythonEnvLabel->setToolTip(tr("Python environment (click to configure)"));
    m_pythonEnvLabel->setVisible(false);
    connect(m_pythonEnvLabel, &QToolButton::clicked, this,
            [this]() { openPythonEnvironmentDialog(); });

    auto layout = qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout());
    if (layout) {
      int insertIndex = layout->count() - 1;
      if (vimStatusLabel) {
        insertIndex = qMax(0, layout->indexOf(vimStatusLabel) + 1);
      } else if (problemsStatusLabel) {
        insertIndex = qMax(0, layout->indexOf(problemsStatusLabel) + 1);
      }
      layout->insertWidget(insertIndex, m_pythonEnvLabel);
    }
  }
}

void MainWindow::updatePythonEnvironmentLabel() {
  ensureStatusLabels();
  Theme theme = getTheme();

  if (!m_pythonEnvLabel) {
    return;
  }

  auto page =
      currentTabWidget() ? currentTabWidget()->getCurrentPage() : nullptr;
  QString filePath = page ? page->getFilePath() : QString();

  if (filePath.isEmpty()) {
    m_pythonEnvLabel->setVisible(false);
    return;
  }

  const QString ext = QFileInfo(filePath).suffix().toLower();
  const bool isPython = (ext == "py" || ext == "pyw" || ext == "pyi");
  if (!isPython) {
    m_pythonEnvLabel->setVisible(false);
    return;
  }

  RunTemplateManager &manager = RunTemplateManager::instance();
  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  FileTemplateAssignment assignment = manager.getAssignmentForFile(filePath);
  PythonEnvironmentPreference preference;
  preference.mode = assignment.pythonMode;
  preference.customInterpreter = assignment.pythonInterpreter;
  preference.venvPath = assignment.pythonVenvPath;
  preference.requirementsFile = assignment.pythonRequirementsFile;

  const PythonEnvironmentInfo info =
      PythonProjectEnvironment::resolve(preference, m_projectRootPath, filePath,
                                        QFileInfo(filePath).absolutePath());

  QString label;
  QString style;
  QString tooltip;
  if (info.found && info.isVirtualEnvironment()) {
    const QString venvName = QFileInfo(info.venvPath).fileName();
    label = QStringLiteral("%1 (%2)").arg(
        QFileInfo(info.interpreter).fileName(), venvName);
    style = QString("QToolButton { color: %1; padding: 0 8px; font-size: 12px;"
                    " border: none; background: transparent; }"
                    "QToolButton:hover { color: %2; }")
                .arg(theme.successColor.name(),
                     theme.successColor.lighter(130).name());
    tooltip = tr("Python: %1\nVenv: %2\nClick to configure")
                  .arg(info.interpreter, info.venvPath);
  } else if (info.found) {
    label = QStringLiteral("%1").arg(QFileInfo(info.interpreter).fileName());
    style = QString("QToolButton { color: %1; padding: 0 8px; font-size: 12px;"
                    " border: none; background: transparent; }"
                    "QToolButton:hover { color: %2; }")
                .arg(theme.borderColor.name(), theme.foregroundColor.name());
    tooltip =
        tr("Python: %1\nNo virtual environment active\nClick to configure")
            .arg(info.interpreter);
  } else {
    label = QString::fromUtf8("\xe2\x9a\xa0 No Python env");
    style = QString("QToolButton { color: %1; padding: 0 8px; font-size: 12px;"
                    " border: none; background: transparent; }"
                    "QToolButton:hover { color: %2; }")
                .arg(theme.warningColor.name(),
                     theme.warningColor.lighter(130).name());
    tooltip = tr("%1\nClick to configure Python environment")
                  .arg(info.statusMessage.isEmpty()
                           ? tr("Python interpreter not found")
                           : info.statusMessage);
  }

  m_pythonEnvLabel->setText(label);
  m_pythonEnvLabel->setStyleSheet(style);
  m_pythonEnvLabel->setToolTip(tooltip);
  m_pythonEnvLabel->setVisible(true);
}

void MainWindow::ensureConflictCenterPanel() {
  if (conflictCenterDock) {
    return;
  }

  conflictCenterPanel = new ConflictCenterPanel(this);
  conflictCenterPanel->setObjectName("conflictCenterPanel");
  conflictCenterPanel->setGitIntegration(m_gitIntegration);
  conflictCenterPanel->applyTheme(getTheme());

  connect(
      conflictCenterPanel, &ConflictCenterPanel::fileOpenRequested, this,
      [this](const QString &filePath) { openFileAndAddToNewTab(filePath); });
  connect(conflictCenterPanel, &ConflictCenterPanel::repositoryChanged, this,
          [this]() {
            if (sourceControlPanel) {
              sourceControlPanel->refresh();
            }
          });
  connect(conflictCenterPanel, &ConflictCenterPanel::attentionNeeded, this,
          &MainWindow::showConflictCenter);

  conflictCenterDock = new QDockWidget(tr("Merge Conflicts"), this);
  conflictCenterDock->setObjectName("conflictCenterDock");
  DockUtils::configureToolPanelDock(conflictCenterDock);
  conflictCenterDock->setWidget(conflictCenterPanel);
  addDockWidget(Qt::RightDockWidgetArea, conflictCenterDock);
  trackDockLayoutChanges(conflictCenterDock);
  conflictCenterDock->hide();

  connect(conflictCenterDock, &QDockWidget::visibilityChanged, this,
          [this](bool) { syncViewToggleActionStates(); });
}

void MainWindow::revealRightDock(QDockWidget *dock, int preferredWidth) {
  if (!dock) {
    return;
  }

  dock->show();
  dock->raise();

  const int ceiling = qMax(260, width() / 3);
  const int target = qBound(220, preferredWidth, ceiling);
  if (dock->width() < target) {
    resizeDocks({dock}, {target}, Qt::Horizontal);
  }
}

void MainWindow::showConflictCenter() {
  ensureConflictCenterPanel();
  if (!conflictCenterDock) {
    return;
  }

  if (sourceControlDock && sourceControlDock->isVisible() &&
      !tabifiedDockWidgets(sourceControlDock).contains(conflictCenterDock)) {
    tabifyDockWidget(sourceControlDock, conflictCenterDock);
  }

  conflictCenterPanel->refresh();
  revealRightDock(conflictCenterDock, 340);
}

MainWindow::FileOpenChoice
MainWindow::confirmLargeOrBinaryOpen(const QString &filePath) {
  const FileOpenGuard::Assessment assessment = FileOpenGuard::assess(filePath);
  if (!assessment.needsConfirmation()) {
    return FileOpenChoice::Whole;
  }

  ThemedMessageBox box(this);
  box.setIcon(ThemedMessageBox::Warning);
  box.setText(assessment.summary);
  box.setInformativeText(assessment.consequence);

  if (assessment.verdict == FileOpenGuard::Verdict::Binary) {
    box.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::Cancel);
    box.setButtonText(ThemedMessageBox::Yes, tr("Look at it read-only"));
  } else {
    box.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No |
                           ThemedMessageBox::Cancel);
    box.setButtonText(
        ThemedMessageBox::Yes,
        tr("Preview the first %1")
            .arg(FileOpenGuard::formatSize(FileOpenGuard::PreviewBytes)));
    box.setButtonText(ThemedMessageBox::No, tr("Open the whole file"));
  }
  box.setButtonText(ThemedMessageBox::Cancel, tr("Do not open it"));
  box.setDefaultButton(ThemedMessageBox::Yes);
  box.setWindowTitle(tr("Open %1?").arg(QFileInfo(filePath).fileName()));

  switch (box.exec()) {
  case ThemedMessageBox::Yes:
    return FileOpenChoice::Preview;
  case ThemedMessageBox::No:
    return FileOpenChoice::Whole;
  default:
    return FileOpenChoice::Cancel;
  }
}

bool MainWindow::isConflictedPath(const QString &filePath) const {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    return false;
  }

  const QString root = m_gitIntegration->repositoryPath();
  if (root.isEmpty() || !filePath.startsWith(root)) {
    return false;
  }

  return m_gitIntegration->getConflictedFiles().contains(
      filePath.mid(root.length() + 1));
}

bool MainWindow::openConflictResolver(const QString &filePath) {
  if (!m_rawConflictOpenPath.isEmpty() && m_rawConflictOpenPath == filePath) {
    return false;
  }
  if (!isConflictedPath(filePath)) {
    return false;
  }

  const QString relative =
      filePath.mid(m_gitIntegration->repositoryPath().length() + 1);

  ConflictResolverView *view =
      new ConflictResolverView(m_gitIntegration, relative, this);
  view->applyTheme(getTheme());

  connect(view, &ConflictResolverView::fileResolved, this,
          [this](const QString &) {
            if (conflictCenterPanel) {
              conflictCenterPanel->refresh();
            }
            if (sourceControlPanel) {
              sourceControlPanel->refresh();
            }
          });
  connect(view, &ConflictResolverView::progressChanged, this, [this]() {
    if (conflictCenterPanel) {
      conflictCenterPanel->refresh();
    }
  });
  connect(view, &ConflictResolverView::rawFileRequested, this,
          [this](const QString &path) {
            m_rawConflictOpenPath = path;
            openFileAndAddToNewTab(path);
            m_rawConflictOpenPath.clear();
          });

  currentTabWidget()->addViewerTab(view, filePath, m_projectRootPath);
  return true;
}

void MainWindow::ensureSourceControlPanel() {
  if (sourceControlDock) {
    return;
  }

  sourceControlPanel = new SourceControlPanel(this);
  sourceControlPanel->setGitIntegration(m_gitIntegration);
  sourceControlPanel->setWorkingPath(
      m_projectRootPath.isEmpty() ? QDir::currentPath() : m_projectRootPath);

  connect(
      sourceControlPanel, &SourceControlPanel::fileOpenRequested, this,
      [this](const QString &filePath) { openFileAndAddToNewTab(filePath); });
  connect(sourceControlPanel, &SourceControlPanel::diffRequested, this,
          [this](const QString &filePath, bool staged) {
            if (!m_gitIntegration) {
              return;
            }
            QString diff = m_gitIntegration->getFileDiff(filePath, staged);
            if (diff.trimmed().isEmpty()) {
              ThemedMessageBox::information(
                  this, tr("Diff"), tr("No changes to show for this file."));
              return;
            }

            GitDiffDialog dialog(m_gitIntegration, filePath,
                                 GitDiffDialog::DiffTarget::File, staged,
                                 getTheme(), this);
            dialog.setWindowTitle(staged ? tr("Staged Diff")
                                         : tr("Unstaged Diff"));
            dialog.setDiffText(diff);
            dialog.exec();
          });
  connect(sourceControlPanel, &SourceControlPanel::commitDiffRequested, this,
          [this](const QString &commitHash, const QString &shortHash) {
            if (!m_gitIntegration) {
              return;
            }
            QString diff = m_gitIntegration->getCommitDiff(commitHash);
            if (diff.trimmed().isEmpty()) {
              ThemedMessageBox::information(
                  this, tr("Commit Diff"),
                  tr("No changes to show for this commit."));
              return;
            }

            GitDiffDialog dialog(m_gitIntegration, commitHash,
                                 GitDiffDialog::DiffTarget::Commit, false,
                                 getTheme(), this);
            dialog.setWindowTitle(tr("Commit %1").arg(shortHash));

            QString author = m_gitIntegration->getCommitAuthor(commitHash);
            QString date = m_gitIntegration->getCommitDate(commitHash);
            QString message = m_gitIntegration->getCommitMessage(commitHash);
            dialog.setCommitInfo(author, date, message);

            dialog.setDiffText(diff);
            dialog.exec();
          });
  connect(sourceControlPanel, &SourceControlPanel::repositoryInitialized, this,
          [this](const QString &path) {
            setProjectRootPath(path);
            updateGitIntegrationForPath(path);
          });

  connect(
      sourceControlPanel, &SourceControlPanel::openWorktreeRequested, this,
      [this](const QString &path) {
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                     {path})) {
          ThemedMessageBox::warning(
              this, tr("Could not open the worktree"),
              tr("Lightpad could not start a second window for %1.").arg(path));
        }
      });
  connect(sourceControlPanel, &SourceControlPanel::compareRequested, this,
          [this](const QString &baseRef, const QString &compareRef,
                 const QString &filePath) {
            showCompareAnything(baseRef, compareRef, filePath);
          });
  connect(sourceControlPanel, &SourceControlPanel::conflictCenterRequested,
          this, &MainWindow::showConflictCenter);

  sourceControlDock = new QDockWidget(tr("Source Control"), this);
  sourceControlDock->setObjectName("sourceControlDock");
  DockUtils::configureToolPanelDock(sourceControlDock);
  sourceControlDock->setWidget(sourceControlPanel);
  addDockWidget(Qt::RightDockWidgetArea, sourceControlDock);
  trackDockLayoutChanges(sourceControlDock);
  sourceControlDock->hide();
  updateSourceControlDockTitle(
      m_gitIntegration ? m_gitIntegration->repositoryPath() : QString(),
      m_gitIntegration ? m_gitIntegration->isValidRepository() : false);

  connect(sourceControlDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            Q_UNUSED(visible)
            syncViewToggleActionStates();
          });
}

void MainWindow::ensureDebugPanel() {
  if (debugDock) {
    return;
  }

  debugPanel = new DebugPanel(this);
  debugPanel->setObjectName("debugPanel");
  debugPanel->applyTheme(getTheme());
  debugPanel->hide();

  connect(debugPanel, &DebugPanel::locationClicked, this,
          [this](const QString &filePath, int line, int column) {
            QString targetPath = filePath;
            QFileInfo sourceInfo(targetPath);
            if (!targetPath.isEmpty() && sourceInfo.isRelative()) {
              if (!m_projectRootPath.isEmpty()) {
                const QString projectResolved =
                    QDir(m_projectRootPath).absoluteFilePath(targetPath);
                if (QFileInfo::exists(projectResolved)) {
                  targetPath = projectResolved;
                }
              }
              if (QFileInfo(targetPath).isRelative()) {
                const QString cwdResolved =
                    QDir::current().absoluteFilePath(targetPath);
                if (QFileInfo::exists(cwdResolved)) {
                  targetPath = cwdResolved;
                }
              }
            }

            if (!targetPath.isEmpty()) {
              openFileAndAddToNewTab(targetPath);
            }
            updateAllTextAreas(&TextArea::setDebugExecutionLine, 0);
            TextArea *textArea = getCurrentTextArea();
            if (textArea) {
              QTextCursor cursor = textArea->textCursor();
              cursor.movePosition(QTextCursor::Start);
              int targetLine = line > 0 ? line - 1 : 0;
              int targetColumn = column > 0 ? column - 1 : 0;
              cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                  targetLine);
              cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                  targetColumn);
              textArea->setTextCursor(cursor);
              textArea->setDebugExecutionLine(line);
              textArea->centerCursor();
              textArea->setFocus();
            }
          });
  connect(debugPanel, &DebugPanel::inlineValuesChanged, this,
          [this](const QString &filePath, int line,
                 const QHash<QString, QString> &values) {
            if (values.isEmpty() || filePath.isEmpty()) {
              updateAllTextAreas(&TextArea::clearDebugInlineValues);
              return;
            }
            const QString canonical =
                QFileInfo(filePath).canonicalFilePath().isEmpty()
                    ? filePath
                    : QFileInfo(filePath).canonicalFilePath();
            for (LightpadTabWidget *tabWidget : allTabWidgets()) {
              const auto textAreas = tabWidget->findChildren<TextArea *>();
              for (TextArea *area : textAreas) {
                area->setDebugInlineValues(canonical, line, values);
              }
            }
          });
  connect(debugPanel, &DebugPanel::startDebugRequested, this,
          &MainWindow::startDebuggingForCurrentFile);
  connect(debugPanel, &DebugPanel::restartDebugRequested, this, [this]() {
    if (!m_activeDebugSessionId.isEmpty()) {
      if (DebugSession *session =
              DebugSessionManager::instance().session(m_activeDebugSessionId)) {
        session->restart();
        return;
      }
    }
    startDebuggingForCurrentFile();
  });
  connect(debugPanel, &DebugPanel::stopDebugRequested, this, [this]() {
    QString sessionIdToStop = m_activeDebugSessionId;
    if (sessionIdToStop.isEmpty()) {
      if (DebugSession *focused =
              DebugSessionManager::instance().focusedSession()) {
        sessionIdToStop = focused->id();
      }
    }

    if (sessionIdToStop.isEmpty()) {
      return;
    }

    clearDebugSession();
    DebugSessionManager::instance().stopSession(sessionIdToStop, true);
  });

  debugDock = new QDockWidget(tr("Debug"), this);
  debugDock->setObjectName("debugDock");
  DockUtils::configureToolPanelDock(debugDock);
  debugDock->setWidget(debugPanel);
  addDockWidget(Qt::BottomDockWidgetArea, debugDock);
  tabifyBottomDock(debugDock);
  trackDockLayoutChanges(debugDock);
  debugDock->hide();

  for (QAction *action : debugPanel->transportActions()) {
    action->setShortcutContext(Qt::ApplicationShortcut);
    addAction(action);
  }

  connect(debugDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            Q_UNUSED(visible)
            syncViewToggleActionStates();
          });

  connect(&DebugSessionManager::instance(),
          &DebugSessionManager::focusedSessionChanged, this,
          [this](const QString &sessionId) { attachDebugSession(sessionId); });
  connect(&DebugSessionManager::instance(),
          &DebugSessionManager::sessionStarted, this,
          [this](const QString &sessionId) { attachDebugSession(sessionId); });
  connect(&DebugSessionManager::instance(),
          &DebugSessionManager::allSessionsEnded, this,
          [this]() { clearDebugSession(); });
}

void MainWindow::ensureTestPanel() {
  if (testDock) {
    return;
  }

  TestConfigurationManager::instance().loadTemplates();
  testPanel = new TestPanel(this);
  testPanel->setObjectName("testPanel");
  testPanel->applyTheme(getTheme());

  if (!m_projectRootPath.isEmpty()) {
    testPanel->setWorkspaceFolder(m_projectRootPath);
  }

  testPanel->restoreState();

  connect(testPanel, &TestPanel::locationClicked, this,
          [this](const QString &filePath, int line, int column) {
            Q_UNUSED(column)
            if (!filePath.isEmpty()) {
              QString targetPath = filePath;
              QFileInfo fi(targetPath);
              if (fi.isRelative() && !m_projectRootPath.isEmpty()) {
                targetPath =
                    QDir(m_projectRootPath).absoluteFilePath(targetPath);
              }
              openFileAndAddToNewTab(targetPath);
              TextArea *textArea = getCurrentTextArea();
              if (textArea && line > 0) {
                QTextCursor cursor = textArea->textCursor();
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                    line - 1);
                textArea->setTextCursor(cursor);
                textArea->centerCursor();
                textArea->setFocus();
              }
            }
          });

  connect(testPanel, &TestPanel::countsChanged, this,
          [this](int passed, int failed, int skipped, int errored) {
            Theme t = getTheme();
            if (!m_testStatusLabel) {
              m_testStatusLabel = new QLabel(this);
              statusBar()->addPermanentWidget(m_testStatusLabel);
            }
            int total = passed + failed + skipped + errored;
            if (total == 0) {
              m_testStatusLabel->setText("");
            } else if (failed > 0 || errored > 0) {
              m_testStatusLabel->setText(
                  tr("Tests: %1/%2 failed").arg(failed + errored).arg(total));
              m_testStatusLabel->setStyleSheet(
                  QString("QLabel { color: %1; font-weight: bold; }")
                      .arg(t.errorColor.name()));
            } else {
              m_testStatusLabel->setText(tr("Tests: %1 passed").arg(passed));
              m_testStatusLabel->setStyleSheet(
                  QString("QLabel { color: %1; font-weight: bold; }")
                      .arg(t.successColor.name()));
            }
          });

  testDock = new QDockWidget(tr("Tests"), this);
  testDock->setObjectName("testDock");
  DockUtils::configureToolPanelDock(testDock);
  testDock->setWidget(testPanel);
  addDockWidget(Qt::BottomDockWidgetArea, testDock);
  tabifyBottomDock(testDock);
  trackDockLayoutChanges(testDock);
  testDock->hide();
  connect(testDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            Q_UNUSED(visible)
            syncViewToggleActionStates();
          });
}

void MainWindow::trackDockLayoutChanges(QDockWidget *dock) {
  if (!dock) {
    return;
  }

  connect(dock, &QDockWidget::visibilityChanged, this, [this](bool) {
    if (m_globalSettingsLoaded && !m_restoringSession) {
      saveSettings();
    }
  });
  connect(dock, &QDockWidget::dockLocationChanged, this,
          [this](Qt::DockWidgetArea) {
            if (m_globalSettingsLoaded && !m_restoringSession) {
              saveSettings();
            }
          });
  connect(dock, &QDockWidget::topLevelChanged, this, [this](bool) {
    if (m_globalSettingsLoaded && !m_restoringSession) {
      saveSettings();
    }
  });
}

void MainWindow::tabifyBottomDock(QDockWidget *dock) {
  QList<QDockWidget *> bottomDocks = {m_terminalDock, m_problemsDock, debugDock,
                                      testDock, databaseDock};
  for (QDockWidget *existing : bottomDocks) {
    if (existing && existing != dock && existing->isVisible()) {
      tabifyDockWidget(existing, dock);
      polishDockTabBars();
      return;
    }
  }

  for (QDockWidget *existing : bottomDocks) {
    if (existing && existing != dock) {
      tabifyDockWidget(existing, dock);
      break;
    }
  }
  polishDockTabBars();
}

void MainWindow::polishDockTabBars() {
  const QList<QTabBar *> tabBars =
      findChildren<QTabBar *>(QString(), Qt::FindDirectChildrenOnly);
  for (QTabBar *tabBar : tabBars) {
    tabBar->setElideMode(Qt::ElideNone);
  }
}

QString MainWindow::activeDebugTarget() const {
  const SettingsManager &settings = SettingsManager::instance();
  const QString target =
      settings.getValue("activeDebugTarget", QString()).toString().trimmed();
  if (target.isEmpty()) {
    return {};
  }
  const QString owner =
      settings.getValue("activeDebugTargetProject", QString()).toString();
  if (QDir::cleanPath(owner) != QDir::cleanPath(m_projectRootPath)) {
    return {};
  }
  return target;
}

void MainWindow::setActiveDebugTarget(const QString &target) {
  SettingsManager &settings = SettingsManager::instance();
  settings.setValue("activeDebugTarget", target);
  settings.setValue("activeDebugTargetProject",
                    target.isEmpty() ? QString() : m_projectRootPath);
}

void MainWindow::on_actionToggle_Test_Panel_triggered() {
  ensureTestPanel();

  bool visible = testDock->isVisible();
  if (!visible) {
    testDock->show();
    testDock->raise();
  } else {
    testDock->hide();
  }
}

void MainWindow::on_actionToggle_Problems_triggered() {
  showProblemsPanel();
  syncViewToggleActionStates();
}

void MainWindow::on_actionPreview_Markdown_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  int currentIndex = tabWidget->currentIndex();
  QString filePath;
  if (currentIndex >= 0)
    filePath = tabWidget->getFilePath(currentIndex);

  QFileInfo fi(filePath);
  if (!MarkdownPreviewPanel::isMarkdownFile(fi.suffix())) {
    if (m_notificationManager) {
      m_notificationManager->showWarning(
          tr("Markdown Preview"), tr("Current file is not a Markdown file"));
    }
    return;
  }

  auto connectPreviewSource = [this, textArea]() {
    disconnect(m_markdownPreviewTextConnection);
    m_markdownPreviewTextConnection = connect(
        textArea, &QPlainTextEdit::textChanged, m_markdownPreviewPanel,
        [textArea, this]() {
          if (m_markdownPreviewPanel)
            m_markdownPreviewPanel->setMarkdown(textArea->toPlainText());
        });
  };

  if (m_markdownPreviewDock) {
    m_markdownPreviewPanel->setFilePath(filePath);
    m_markdownPreviewPanel->setMarkdown(textArea->toPlainText());
    m_markdownPreviewPanel->updatePreview();
    connectPreviewSource();
    m_markdownPreviewDock->show();
    m_markdownPreviewDock->raise();
    return;
  }

  m_markdownPreviewPanel = new MarkdownPreviewPanel(this);
  m_markdownPreviewPanel->setFilePath(filePath);
  m_markdownPreviewPanel->setMarkdown(textArea->toPlainText());

  m_markdownPreviewDock = new QDockWidget(tr("Markdown Preview"), this);
  m_markdownPreviewDock->setObjectName("markdownPreviewDock");
  m_markdownPreviewDock->setWidget(m_markdownPreviewPanel);
  DockUtils::configureToolPanelDock(m_markdownPreviewDock);
  m_markdownPreviewDock->setMinimumWidth(350);
  addDockWidget(Qt::RightDockWidgetArea, m_markdownPreviewDock);
  trackDockLayoutChanges(m_markdownPreviewDock);

  QTimer::singleShot(100, m_markdownPreviewPanel,
                     &MarkdownPreviewPanel::updatePreview);

  connectPreviewSource();

  connect(m_markdownPreviewPanel, &MarkdownPreviewPanel::linkClicked, this,
          [this](const QString &path) {
            if (QFileInfo::exists(path)) {
              openFileAndAddToNewTab(path);
            }
          });

  connect(m_markdownPreviewDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (!visible && m_markdownPreviewDock) {
              m_markdownPreviewDock->hide();
            }
          });

  LOG_INFO(QString("Opened Markdown preview for: %1").arg(filePath));
}

void MainWindow::on_actionPreview_LaTeX_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  int currentIndex = tabWidget->currentIndex();
  QString filePath;
  if (currentIndex >= 0)
    filePath = tabWidget->getFilePath(currentIndex);

  QFileInfo fi(filePath);
  if (!LatexPreviewPanel::isLatexFile(fi.suffix())) {
    if (m_notificationManager) {
      m_notificationManager->showWarning(
          tr("LaTeX Build"), tr("Current file is not a LaTeX file"));
    }
    return;
  }

  if (m_latexPreviewDock) {
    m_latexPreviewPanel->setFilePath(filePath);
    m_latexPreviewDock->show();
    m_latexPreviewDock->raise();
    return;
  }

  m_latexPreviewPanel = new LatexPreviewPanel(this);
  m_latexPreviewPanel->setFilePath(filePath);

  m_latexPreviewDock = new QDockWidget(tr("LaTeX Build"), this);
  m_latexPreviewDock->setObjectName("latexPreviewDock");
  m_latexPreviewDock->setWidget(m_latexPreviewPanel);
  DockUtils::configureToolPanelDock(m_latexPreviewDock);
  m_latexPreviewDock->setMinimumWidth(350);
  addDockWidget(Qt::RightDockWidgetArea, m_latexPreviewDock);
  trackDockLayoutChanges(m_latexPreviewDock);

  connect(m_latexPreviewPanel, &LatexPreviewPanel::diagnosticsReady, this,
          [this](const QList<LspDiagnostic> &diags) {
            if (m_diagnosticsManager && m_latexPreviewPanel) {
              QString uri = DiagnosticUtils::filePathToUri(
                  m_latexPreviewPanel->filePath());
              m_diagnosticsManager->upsertDiagnostics(uri, diags, "latex");
            }
          });
  connect(m_latexPreviewDock, &QDockWidget::visibilityChanged, this,
          [this](bool) {});

  LOG_INFO(QString("Opened LaTeX build panel for: %1").arg(filePath));
}

