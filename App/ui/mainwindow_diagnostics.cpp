#include "mainwindow_internal.h"

void MainWindow::setupAutoSave() {
  autoSaveManager = new AutoSaveManager(this, this);
  autoSaveManager->setEnabled(
      SettingsManager::instance().getValue("autoSaveFiles", true).toBool());
}

void MainWindow::setupDiagnostics() {
  SettingsManager &sm = SettingsManager::instance();
  QJsonObject diagSettings =
      sm.getValue("diagnostics", QJsonObject()).toJsonObject();
  bool enabled = diagSettings.value("enabled").toBool(true);
  setupNotificationManager();

  m_diagnosticsManager = enabled ? new DiagnosticsManager(this) : nullptr;
  m_languageFeatureManager = new LanguageFeatureManager(
      enabled ? m_diagnosticsManager : nullptr, this);
  m_languageFeatureManager->setPythonEnvironmentResolver(
      [](const QString &filePath) {
        const auto assignment =
            RunTemplateManager::instance().getAssignmentForFile(filePath);
        return PythonEnvironmentPreference{
            assignment.pythonMode, assignment.pythonInterpreter,
            assignment.pythonVenvPath, assignment.pythonRequirementsFile};
      });
  connect(&RunTemplateManager::instance(),
          &RunTemplateManager::assignmentChanged, m_languageFeatureManager,
          [this](const QString &filePath) {
            if (effectiveLanguageIdForFile(filePath) == "py") {
              m_languageFeatureManager->restartServer("py");
            }
          });

  if (m_diagnosticsManager) {
    connect(m_diagnosticsManager, &DiagnosticsManager::diagnosticsChanged, this,
            &MainWindow::onDiagnosticsChanged);
    connect(m_diagnosticsManager, &DiagnosticsManager::countsChanged, this,
            &MainWindow::updateProblemsStatusLabel);
  }

  connect(
      m_languageFeatureManager, &LanguageFeatureManager::serverError, this,
      [this](const QString &languageId, const QString &message) {
        LOG_WARNING(
            QString("LSP server error for '%1': %2").arg(languageId, message));
        auto *tw = currentTabWidget();
        const QString filePath = (tw && tw->currentIndex() >= 0)
                                     ? tw->getFilePath(tw->currentIndex())
                                     : QString();
        const bool errorBelongsToCurrentFile =
            !filePath.isEmpty() &&
            effectiveLanguageIdForFile(filePath) == languageId;

        if (m_notificationManager && errorBelongsToCurrentFile) {
          QString title =
              tr("Language Server — %1")
                  .arg(LanguageCatalog::displayName(languageId).isEmpty()
                           ? languageId
                           : LanguageCatalog::displayName(languageId));
          m_notificationManager->showKeyedError(
              QString::fromLatin1(kCurrentFileLspErrorNotificationKey), title,
              message);
        }

        if (tw && tw->currentIndex() >= 0) {
          const QString fp = tw->getFilePath(tw->currentIndex());
          if (!fp.isEmpty() && effectiveLanguageIdForFile(fp) == languageId) {
            updateLspStatusLabel(languageId, "error");
          }
        }
      });

  connect(
      m_languageFeatureManager, &LanguageFeatureManager::serverUnavailable,
      this, [this](const QString &languageId, const QString &message) {
        LOG_DEBUG(
            QString("LSP unavailable for '%1': %2").arg(languageId, message));
        auto *tw = currentTabWidget();
        if (!tw || tw->currentIndex() < 0) {
          return;
        }
        const QString filePath = tw->getFilePath(tw->currentIndex());
        if (filePath.isEmpty() ||
            effectiveLanguageIdForFile(filePath) != languageId) {
          return;
        }

        if (m_notificationManager) {
          m_notificationManager->dismiss(
              QString::fromLatin1(kCurrentFileLspErrorNotificationKey));
        }
        updateLspStatusLabel(languageId, "unavailable");
      });

  connect(m_languageFeatureManager, &LanguageFeatureManager::serverStarted,
          this, [this](const QString &languageId) {
            if (m_notificationManager) {
              m_notificationManager->dismiss(
                  QString::fromLatin1(kCurrentFileLspErrorNotificationKey));
              QString displayName =
                  LanguageCatalog::displayName(languageId).isEmpty()
                      ? languageId
                      : LanguageCatalog::displayName(languageId);
              m_notificationManager->showInfo(
                  tr("Language Server Ready"),
                  tr("%1 language server is now active.").arg(displayName),
                  3000);
            }

            auto *tw = currentTabWidget();
            if (tw && tw->currentIndex() >= 0) {
              QString fp = tw->getFilePath(tw->currentIndex());
              if (!fp.isEmpty() && effectiveLanguageIdForFile(fp) == languageId)
                updateLspStatusLabel(languageId, "running");
            }
          });

  connect(m_languageFeatureManager, &LanguageFeatureManager::serverStarted,
          this, [this]() {
            auto *tw = currentTabWidget();
            if (m_lspCompletionProvider && tw && tw->currentIndex() >= 0) {
              m_lspCompletionProvider->setClient(
                  m_languageFeatureManager->clientForFile(
                      tw->getFilePath(tw->currentIndex())));
            }
          });

  connect(m_languageFeatureManager,
          &LanguageFeatureManager::serverHealthChanged, this,
          [this](const QString &languageId, ServerHealthStatus status) {
            auto *tw = currentTabWidget();
            if (!tw || tw->currentIndex() < 0)
              return;
            QString filePath = tw->getFilePath(tw->currentIndex());
            if (filePath.isEmpty())
              return;
            QString currentLang = effectiveLanguageIdForFile(filePath);
            if (currentLang != languageId)
              return;

            switch (status) {
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
              updateLspStatusLabel(languageId, "");
              break;
            }
          });
}

void MainWindow::setupNotificationManager() {
  m_notificationManager = new NotificationManager(this, this);
  m_scanlineOverlay = new HackerScanlineOverlay(this);
  m_scanlineOverlay->setEnabled(
      ThemeEngine::instance().activeTheme().ui.scanlineEffect);
}

void MainWindow::setScanlineEffectEnabled(bool enabled) {
  if (!m_scanlineOverlay)
    m_scanlineOverlay = new HackerScanlineOverlay(this);
  m_scanlineOverlay->setEnabled(enabled);
  if (enabled) {
    m_scanlineOverlay->setGeometry(rect());
    m_scanlineOverlay->raise();
  }
}

void MainWindow::updateLspStatusLabel(const QString &languageId,
                                      const QString &status) {
  ensureStatusLabels();
  Theme theme = getTheme();

  if (!m_lspStatusLabel) {
    m_lspStatusLabel = new QToolButton(this);
    m_lspStatusLabel->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_lspStatusLabel->setAutoRaise(true);
    m_lspStatusLabel->setCursor(Qt::PointingHandCursor);
    m_lspStatusLabel->setToolTip(
        tr("Language server status (click for details)"));
    connect(m_lspStatusLabel, &QToolButton::clicked, this,
            &MainWindow::openLanguageServerStatusDialog);

    auto layout = qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout());
    if (layout) {
      int insertIndex = layout->count() - 1;
      if (m_pythonEnvLabel && layout->indexOf(m_pythonEnvLabel) >= 0) {
        insertIndex = layout->indexOf(m_pythonEnvLabel) + 1;
      } else if (vimStatusLabel && layout->indexOf(vimStatusLabel) >= 0) {
        insertIndex = layout->indexOf(vimStatusLabel) + 1;
      } else if (problemsStatusLabel &&
                 layout->indexOf(problemsStatusLabel) >= 0) {
        insertIndex = layout->indexOf(problemsStatusLabel) + 1;
      }
      layout->insertWidget(insertIndex, m_lspStatusLabel);
    }
  }

  QString displayName = LanguageCatalog::displayName(languageId).isEmpty()
                            ? languageId
                            : LanguageCatalog::displayName(languageId);
  m_lspStatusLanguageId = languageId;
  m_lspStatusState = status;

  UIStyleHelper::Tone tone = UIStyleHelper::Tone::Neutral;
  QString glyph;
  QString tooltip;
  if (status == "running") {
    tone = UIStyleHelper::Tone::Success;
    glyph = QString::fromUtf8("●");
    tooltip = tr("%1 language server is running").arg(displayName);
  } else if (status == "starting") {
    tone = UIStyleHelper::Tone::Warning;
    glyph = QString::fromUtf8("◌");
    tooltip = tr("%1 language server is starting…").arg(displayName);
  } else if (status == "error") {
    tone = UIStyleHelper::Tone::Error;
    glyph = QString::fromUtf8("✖");
    tooltip = tr("%1 language server encountered an error").arg(displayName);
  } else if (status == "stopped") {
    glyph = QString::fromUtf8("○");
    tooltip = tr("%1 language server is stopped").arg(displayName);
  } else if (status == "unavailable") {
    glyph = QString::fromUtf8("○");
    tooltip = tr("No language server configured for %1").arg(displayName);
  } else {
    m_lspStatusLabel->setVisible(false);
    return;
  }

  m_lspStatusLabel->setText(QString("%1 %2").arg(glyph, displayName));
  m_lspStatusLabel->setToolTip(tooltip);
  m_lspStatusLabel->setStyleSheet(
      QString("QToolButton { %1 font-weight: normal; padding: 2px 8px; }"
              "QToolButton:hover { border-color: %2; }")
          .arg(UIStyleHelper::badgeStyle(theme, tone),
               UIStyleHelper::toneColor(theme, tone).name()));
  m_lspStatusLabel->setVisible(true);
}

void MainWindow::openPythonEnvironmentDialog() {
  if (!findChildren<PythonEnvironmentDialog *>().isEmpty()) {
    auto *existing = findChildren<PythonEnvironmentDialog *>().first();
    existing->raise();
    existing->activateWindow();
    return;
  }

  auto page =
      currentTabWidget() ? currentTabWidget()->getCurrentPage() : nullptr;
  const QString filePath = page ? page->getFilePath() : QString();
  if (filePath.trimmed().isEmpty()) {
    return;
  }

  auto *dialog = new PythonEnvironmentDialog(m_projectRootPath, filePath,
                                             getTheme(), this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &PythonEnvironmentDialog::configurationSaved, this,
          [this]() { updatePythonEnvironmentLabel(); });
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::openLanguageServerStatusDialog() {
  if (m_lspStatusLanguageId.trimmed().isEmpty() || !m_languageFeatureManager) {
    return;
  }

  auto page =
      currentTabWidget() ? currentTabWidget()->getCurrentPage() : nullptr;
  const QString filePath = page ? page->getFilePath() : QString();
  const QString effectiveLanguage = filePath.trimmed().isEmpty()
                                        ? QString()
                                        : effectiveLanguageIdForFile(filePath);
  const QString overrideLanguage = filePath.trimmed().isEmpty()
                                       ? QString()
                                       : highlightOverrideForFile(filePath);

  if (!findChildren<LanguageServerStatusDialog *>().isEmpty()) {
    auto *existing = findChildren<LanguageServerStatusDialog *>().first();
    existing->raise();
    existing->activateWindow();
    return;
  }

  auto *dialog = new LanguageServerStatusDialog(
      m_lspStatusLanguageId, m_projectRootPath, filePath, effectiveLanguage,
      overrideLanguage, m_languageFeatureManager, getTheme(), this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &LanguageServerStatusDialog::configurationApplied, this,
          &MainWindow::retryLanguageServerForCurrentFile);
  connect(dialog, &LanguageServerStatusDialog::pythonEnvironmentRequested, this,
          &MainWindow::openPythonEnvironmentDialog);
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::retryLanguageServerForCurrentFile(
    const QString &languageAssociation) {
  auto page =
      currentTabWidget() ? currentTabWidget()->getCurrentPage() : nullptr;
  const QString filePath = page ? page->getFilePath() : QString();
  TextArea *textArea = getCurrentTextArea();
  if (!m_languageFeatureManager || filePath.trimmed().isEmpty() || !textArea) {
    return;
  }

  if (languageAssociation.trimmed().isEmpty()) {
    setHighlightOverrideForFile(filePath, "");
    applyHighlightForFile(filePath);
    notifyDiagnosticsFileClosed(filePath);
    const QString effectiveLanguage = effectiveLanguageIdForFile(filePath);
    m_languageFeatureManager->openDocument(filePath, effectiveLanguage,
                                           textArea->toPlainText());
    updateLspStatusLabel(effectiveLanguage, "starting");
  } else {
    applyLanguageOverride(languageAssociation);
    updateLspStatusLabel(effectiveLanguageIdForFile(filePath), "starting");
  }
}

static TextArea *textAreaForFile(const QList<LightpadTabWidget *> &tabWidgets,
                                 const QString &filePath) {
  const QString cleanPath = QDir::cleanPath(filePath);
  for (LightpadTabWidget *tabWidget : tabWidgets) {
    for (int i = 0; tabWidget && i < tabWidget->count(); ++i) {
      if (QDir::cleanPath(tabWidget->getFilePath(i)) != cleanPath) {
        continue;
      }
      LightpadPage *page = tabWidget->getPage(i);
      if (page && page->getTextArea()) {
        return page->getTextArea();
      }
    }
  }
  return nullptr;
}

void MainWindow::notifyDiagnosticsFileOpened(const QString &filePath) {
  if (!m_languageFeatureManager || filePath.isEmpty()) {
    return;
  }
  if (m_languageFeatureManager->isExternalDocument(filePath)) {
    if (auto *area = textAreaForFile(allTabWidgets(), filePath)) {
      area->setDocumentUriOverride(
          m_languageFeatureManager->documentUriForFile(filePath));
      area->setReadOnly(true);
    }
    m_documentVersions[filePath] = 1;
    return;
  }

  QString text;
  if (TextArea *textArea = textAreaForFile(allTabWidgets(), filePath)) {
    textArea->setDocumentUriOverride({});
    text = textArea->toPlainText();
  } else {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      return;
    }
    text = QString::fromUtf8(file.readAll());
  }

  QString languageId = effectiveLanguageIdForFile(filePath);
  m_documentVersions[filePath] = 1;
  m_languageFeatureManager->setWorkspaceFolder(m_projectRootPath);
  m_languageFeatureManager->openDocument(filePath, languageId, text);
}

void MainWindow::notifyDiagnosticsFileChanged(const QString &filePath,
                                              const QString &text) {
  if (!m_languageFeatureManager || filePath.isEmpty()) {
    return;
  }

  SettingsManager &sm = SettingsManager::instance();
  QJsonObject diagSettings =
      sm.getValue("diagnostics", QJsonObject()).toJsonObject();
  bool onType = diagSettings.value("onType").toBool(true);
  int version = m_documentVersions.value(filePath, 0) + 1;
  m_documentVersions[filePath] = version;
  const QString uri = DiagnosticUtils::filePathToUri(filePath);
  if (m_diagnosticsManager) {
    m_diagnosticsManager->trackDocumentVersion(uri, version);
  }

  if (!onType) {
    m_pendingDiagnosticsTexts[filePath] = text;
    return;
  }

  const int debounceMs = qMax(0, diagSettings.value("debounceMs").toInt(200));
  if (debounceMs == 0) {
    m_languageFeatureManager->changeDocument(filePath, version, text);
    return;
  }

  m_pendingDiagnosticsTexts[filePath] = text;

  QTimer *timer = m_diagnosticsChangeTimers.value(filePath, nullptr);
  if (!timer) {
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this,
            [this, filePath]() { flushPendingDiagnosticsChange(filePath); });
    m_diagnosticsChangeTimers.insert(filePath, timer);
  }

  timer->start(debounceMs);
}

void MainWindow::notifyDiagnosticsFileSaved(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }

  {
    QFileInfo fi(filePath);
    if (MarkdownPreviewPanel::isMarkdownFile(fi.suffix()) &&
        m_diagnosticsManager) {
      TextArea *textArea = textAreaForFile(allTabWidgets(), filePath);
      if (textArea) {
        QList<LspDiagnostic> mdDiags =
            MarkdownTools::lint(textArea->toPlainText(), filePath);
        QString uri = DiagnosticUtils::filePathToUri(filePath);
        m_diagnosticsManager->upsertDiagnostics(uri, mdDiags, "markdown-lint");
      }
    }
  }

  {
    QFileInfo fi(filePath);
    if (LatexTools::isLatexFile(fi.suffix()) && m_diagnosticsManager) {
      TextArea *textArea = textAreaForFile(allTabWidgets(), filePath);
      if (textArea) {
        QList<LspDiagnostic> texDiags =
            LatexTools::lint(textArea->toPlainText(), filePath);
        QString uri = DiagnosticUtils::filePathToUri(filePath);
        m_diagnosticsManager->upsertDiagnostics(uri, texDiags, "latex-lint");
      }
    }
  }

  if (!m_languageFeatureManager) {
    return;
  }

  flushPendingDiagnosticsChange(filePath);

  SettingsManager &sm = SettingsManager::instance();
  QJsonObject diagSettings =
      sm.getValue("diagnostics", QJsonObject()).toJsonObject();
  bool onSave = diagSettings.value("onSave").toBool(true);
  if (!onSave) {
    return;
  }

  m_languageFeatureManager->saveDocument(filePath);
}

void MainWindow::notifyDiagnosticsFileClosed(const QString &filePath) {
  if (!m_languageFeatureManager || filePath.isEmpty()) {
    return;
  }

  flushPendingDiagnosticsChange(filePath);
  m_languageFeatureManager->closeDocument(filePath);
  m_documentVersions.remove(filePath);
  clearPendingDiagnosticsChange(filePath);
}

void MainWindow::flushPendingLanguageServerChanges(const QString &filePath) {
  flushPendingDiagnosticsChange(filePath);
}

void MainWindow::flushPendingDiagnosticsChange(const QString &filePath) {
  if (!m_languageFeatureManager || filePath.isEmpty() ||
      !m_pendingDiagnosticsTexts.contains(filePath)) {
    return;
  }

  if (QTimer *timer = m_diagnosticsChangeTimers.value(filePath, nullptr)) {
    timer->stop();
  }

  const QString text = m_pendingDiagnosticsTexts.take(filePath);
  const int version = m_documentVersions.value(filePath, 0);
  if (version <= 0) {
    return;
  }

  m_languageFeatureManager->changeDocument(filePath, version, text);
}

void MainWindow::clearPendingDiagnosticsChange(const QString &filePath) {
  m_pendingDiagnosticsTexts.remove(filePath);

  if (QTimer *timer = m_diagnosticsChangeTimers.take(filePath)) {
    timer->stop();
    timer->deleteLater();
  }
}

void MainWindow::onDiagnosticsChanged(const QString &uri) {
  if (!m_diagnosticsManager) {
    return;
  }

  QString filePath = DiagnosticUtils::uriToFilePath(uri);
  QList<LspDiagnostic> diagnostics =
      m_diagnosticsManager->diagnosticsForUri(uri);

  if (problemsPanel) {
    problemsPanel->setDiagnostics(uri, diagnostics);
  }

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
      page->getTextArea()->setDiagnostics(diagnostics);
    }
  }

  refreshProblemsStatusForCurrentFile();
}

void MainWindow::setupGitIntegration() {
  if (m_gitIntegration) {
    return;
  }

  m_gitIntegration = new GitIntegration(this);

  m_gitBranchLabel = new QLabel(this);
  m_gitBranchLabel->setToolTip(tr("Current branch (click to switch)"));
  m_gitBranchLabel->setCursor(Qt::PointingHandCursor);
  m_gitBranchLabel->setStyleSheet("QLabel { padding: 0 6px; }");

  m_gitSyncLabel = new QLabel(this);
  m_gitSyncLabel->setToolTip(tr("Ahead/behind upstream"));
  m_gitSyncLabel->setStyleSheet("QLabel { padding: 0 4px; }");

  m_gitDirtyLabel = new QLabel(this);
  m_gitDirtyLabel->setToolTip(tr("Working tree status"));
  m_gitDirtyLabel->setStyleSheet("QLabel { padding: 0 4px; }");

  statusBar()->addPermanentWidget(m_gitDirtyLabel);
  statusBar()->addPermanentWidget(m_gitSyncLabel);
  statusBar()->addPermanentWidget(m_gitBranchLabel);

  m_gitStatusBarTimer.setSingleShot(true);
  m_gitStatusBarTimer.setInterval(500);
  connect(&m_gitStatusBarTimer, &QTimer::timeout, this,
          &MainWindow::updateGitStatusBar);

  connect(m_gitIntegration, &GitIntegration::statusChanged, this,
          [this]() { m_gitStatusBarTimer.start(); });
  connect(m_gitIntegration, &GitIntegration::branchChanged, this,
          [this](const QString &) { m_gitStatusBarTimer.start(); });

  connect(m_gitIntegration, &GitIntegration::mergeConflictsDetected, this,
          [this](const QStringList &) { showConflictCenter(); });

  m_gitIntegration->setAutoRefreshGate([]() {
    QWidget *modal = QApplication::activeModalWidget();
    return !QApplication::activePopupWidget() &&
           (!modal || qobject_cast<StyledDialog *>(modal));
  });
  connect(m_gitIntegration, &GitIntegration::externalChangesDetected, this,
          [this]() {
            updateGitGutterForCurrentFile();
            updateInlineBlameForCurrentFile();
          });
  connect(qApp, &QGuiApplication::applicationStateChanged, this,
          [this](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive && m_gitIntegration) {
              m_gitIntegration->checkForExternalChanges();
            }
          });

  updateGitIntegrationForPath(QDir::currentPath());
}

void MainWindow::updateGitIntegrationForPath(const QString &path) {
  if (!m_gitIntegration || path.isEmpty()) {
    return;
  }

  recheckOpenFilesForExternalChanges();

  bool isRepo = m_gitIntegration->setRepositoryPath(path);
  if (!isRepo) {
    m_gitIntegration->setWorkingPath(path);
  } else {
    m_gitIntegration->setWorkingPath(m_gitIntegration->repositoryPath());
  }

  applyGitIntegrationToAllPages();
  if (conflictCenterPanel) {
    conflictCenterPanel->setGitIntegration(m_gitIntegration);
  }
  m_gitIntegration->refresh();

  if (sourceControlPanel) {
    sourceControlPanel->setWorkingPath(
        isRepo ? m_gitIntegration->repositoryPath() : path);
    sourceControlPanel->refresh();
  }
  updateSourceControlDockTitle(m_gitIntegration->repositoryPath(), isRepo);

  if (isRepo && m_gitIntegration->hasMergeConflicts()) {
    showConflictCenter();
  }

  auto textArea = getCurrentTextArea();
  LightpadTabWidget *tabWidget = currentTabWidget();
  QString currentFilePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (textArea && !currentFilePath.isEmpty()) {
    textArea->setGutterGitIntegration(m_gitIntegration);
    updateGitGutterForCurrentFile(currentFilePath);
  }

  showGitBlameForCurrentFile(isGitBlameEnabledForFile(currentFilePath));
  updateInlineBlameForCurrentFile();
  updateGitStatusBar();
}

void MainWindow::updateSourceControlDockTitle(const QString &repoRoot,
                                              bool isRepo) {
  if (!sourceControlDock) {
    return;
  }

  if (isRepo && !repoRoot.isEmpty()) {
    const QString absolute = QDir(repoRoot).absolutePath();
    sourceControlDock->setWindowTitle(
        tr("Source Control — %1").arg(QFileInfo(absolute).fileName()));
    sourceControlDock->setToolTip(absolute);
    return;
  }

  sourceControlDock->setWindowTitle(tr("Source Control"));
}

void MainWindow::applyGitIntegrationToAllPages() {
  if (!m_gitIntegration) {
    return;
  }

  if (m_fileTreeModel) {
    m_fileTreeModel->setGitIntegration(m_gitIntegration);
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (page) {
        page->setGitIntegration(m_gitIntegration);
      }
    }
  }
}

void MainWindow::updateProblemsStatusLabel(int errors, int warnings,
                                           int infos) {
  (void)errors;
  (void)warnings;
  (void)infos;
  refreshProblemsStatusForCurrentFile();
}

void MainWindow::restyleStatusBarItems() {
  if (m_pythonEnvLabel)
    m_pythonEnvLabel->setStyleSheet(statusBarTextButtonStyle());
  if (vimStatusLabel)
    updateVimStatusLabel(vimStatusLabel->text());
  refreshProblemsStatusForCurrentFile();
  updateDatabaseStatusChip();
  if (m_lspStatusLabel && m_lspStatusLabel->isVisible() &&
      !m_lspStatusState.isEmpty()) {
    updateLspStatusLabel(m_lspStatusLanguageId, m_lspStatusState);
  }
}

void MainWindow::refreshProblemsStatusForCurrentFile() {
  if (!problemsStatusLabel)
    return;

  int errors = 0, warnings = 0;

  if (m_diagnosticsManager) {
    auto *tw = currentTabWidget();
    if (tw && tw->currentIndex() >= 0) {
      QString filePath = tw->getFilePath(tw->currentIndex());
      if (!filePath.isEmpty()) {
        QString uri = filePath;
        if (!uri.startsWith("file://"))
          uri = "file://" + uri;
        QList<LspDiagnostic> diags =
            m_diagnosticsManager->diagnosticsForUri(uri);
        for (const auto &d : diags) {
          if (d.severity == LspDiagnosticSeverity::Error)
            ++errors;
          else if (d.severity == LspDiagnosticSeverity::Warning)
            ++warnings;
        }
      }
    }
  }

  QString text;
  UIStyleHelper::Tone tone = UIStyleHelper::Tone::Neutral;
  if (errors > 0 || warnings > 0) {
    text = QString("⛔ %1  ⚠️ %2").arg(errors).arg(warnings);
    tone =
        errors > 0 ? UIStyleHelper::Tone::Error : UIStyleHelper::Tone::Warning;
  } else {
    text = "✓ No problems";
  }
  problemsStatusLabel->setText(text);
  const QString color =
      tone == UIStyleHelper::Tone::Neutral
          ? ThemeEngine::instance().activeTheme().colors.textSecondary.name()
          : UIStyleHelper::toneColor(getTheme(), tone).name();
  problemsStatusLabel->setStyleSheet(
      QString("color: %1; padding: 0 8px;").arg(color));
}

void MainWindow::updateVimStatusLabel(const QString &text) {
  if (!vimStatusLabel)
    return;
  vimStatusLabel->setText(text);
  vimStatusLabel->setVisible(!text.isEmpty());
  if (text.isEmpty())
    return;

  UIStyleHelper::Tone tone = UIStyleHelper::Tone::Neutral;
  if (text == "NORMAL")
    tone = UIStyleHelper::Tone::Success;
  else if (text == "INSERT")
    tone = UIStyleHelper::Tone::Accent;
  else if (text == "VISUAL" || text == "V-LINE" || text == "V-BLOCK")
    tone = UIStyleHelper::Tone::Warning;
  else if (text == "REPLACE")
    tone = UIStyleHelper::Tone::Error;
  else if (text == "COMMAND")
    tone = UIStyleHelper::Tone::Info;
  vimStatusLabel->setStyleSheet(
      QString("QLabel { %1 letter-spacing: 1px; padding: 1px 10px; }")
          .arg(UIStyleHelper::badgeStyle(getTheme(), tone)));
}

void MainWindow::showVimStatusMessage(const QString &message) {
  if (!vimStatusLabel) {
    return;
  }
  updateVimStatusLabel(message);
  QTimer::singleShot(2500, this, [this, message]() {
    if (vimStatusLabel && vimStatusLabel->text() == message) {
      updateVimStatusLabel("");
    }
  });
}

void MainWindow::setMainWindowTitle(QString title) {
  setWindowTitle(title + " - Lightpad");
}

void MainWindow::setFont(QFont newFont) {
  updateAllTextAreas(&TextArea::setFont, newFont);
  font = newFont;
  settings.mainFont = newFont;

  SettingsManager &sm = SettingsManager::instance();
  sm.setValue("fontFamily", newFont.family());
  sm.setValue("fontSize", newFont.pointSize());
  sm.setValue("fontWeight", newFont.weight());
  sm.setValue("fontItalic", newFont.italic());
  sm.saveSettings();
  settings.saveSettings(textAreaSettingsPath());
}

void MainWindow::showLineNumbers(bool flag) {
  updateAllTextAreas(&TextArea::showLineNumbers, flag);
  settings.showLineNumberArea = flag;
  saveSettings();
}

void MainWindow::highlihtCurrentLine(bool flag) {
  updateAllTextAreas(&TextArea::highlihtCurrentLine, flag);
  settings.lineHighlighted = flag;
  saveSettings();
}

void MainWindow::highlihtMatchingBracket(bool flag) {
  updateAllTextAreas(&TextArea::highlihtMatchingBracket, flag);
  settings.matchingBracketsHighlighted = flag;
  saveSettings();
}

void MainWindow::setAutoSaveEnabled(bool enabled) {
  if (autoSaveManager) {
    autoSaveManager->setEnabled(enabled);
  }

  SettingsManager &globalSettings = SettingsManager::instance();
  if (globalSettings.getValue("autoSaveFiles", true).toBool() != enabled) {
    globalSettings.setValue("autoSaveFiles", enabled);
    globalSettings.saveSettings();
  }
}

bool MainWindow::isAutoSaveEnabled() const {
  if (autoSaveManager) {
    return autoSaveManager->isEnabled();
  }
  return SettingsManager::instance().getValue("autoSaveFiles", true).toBool();
}
