#include "mainwindow_internal.h"
#include <QToolTip>
#include <memory>

void MainWindow::runCurrentScript() { runPath(runSourceFilePath()); }

int MainWindow::publishBuildDiagnostics(const QString &buildOutput,
                                        const QString &workingDirectory) {
  if (!m_diagnosticsManager) {
    return 0;
  }

  const QMap<QString, QList<LspDiagnostic>> byFile =
      CompilerDiagnosticParser::parseByFile(buildOutput, workingDirectory);

  m_diagnosticsManager->clearAllForSource(kBuildDiagnosticsSource);
  if (byFile.isEmpty()) {
    return 0;
  }

  int published = 0;
  QString firstErrorFile;
  int firstErrorLine = 0;
  int firstErrorColumn = 0;

  for (auto it = byFile.constBegin(); it != byFile.constEnd(); ++it) {
    m_diagnosticsManager->upsertDiagnostics(
        DiagnosticUtils::filePathToUri(it.key()), it.value(),
        kBuildDiagnosticsSource);
    published += it.value().size();

    if (firstErrorFile.isEmpty()) {
      for (const LspDiagnostic &diagnostic : it.value()) {
        if (diagnostic.severity == LspDiagnosticSeverity::Error) {
          firstErrorFile = it.key();
          firstErrorLine = diagnostic.range.start.line;
          firstErrorColumn = diagnostic.range.start.character;
          break;
        }
      }
    }
  }

  showProblemsPanel();
  if (m_problemsDock) {
    m_problemsDock->show();
    m_problemsDock->raise();
  }
  syncViewToggleActionStates();

  if (!firstErrorFile.isEmpty()) {
    openFileAndAddToNewTab(firstErrorFile);
    if (TextArea *textArea = getCurrentTextArea()) {
      QTextCursor cursor = textArea->textCursor();
      cursor.movePosition(QTextCursor::Start);
      cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                          firstErrorLine);
      cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                          firstErrorColumn);
      textArea->setTextCursor(cursor);
      textArea->centerCursor();
      textArea->setFocus();
    }
  }

  return published;
}

void MainWindow::showBuildFailure(const QString &title, const QString &text,
                                  const QString &informativeText,
                                  const QString &details) {

  const int published = publishBuildDiagnostics(details, m_lastBuildDirectory);
  if (published > 0) {
    const int errors =
        m_diagnosticsManager ? m_diagnosticsManager->errorCount() : published;

    const int count = errors > 0 ? errors : published;
    const QString noun = errors > 0
                             ? (count == 1 ? tr("error") : tr("errors"))
                             : (count == 1 ? tr("message") : tr("messages"));
    const QString summary =
        tr("%1 %2 - see the Problems panel").arg(count).arg(noun);

    if (terminalWidget) {
      terminalWidget->appendNotice(
          QStringLiteral("\n%1").arg(tr("%1 - %2").arg(title, summary)), true);
    }
    if (m_notificationManager) {
      m_notificationManager->showWarning(title, summary);
    }
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, summary),
                             10000);
    return;
  }

  ThemedMessageBox msg(this);
  msg.setIcon(ThemedMessageBox::Warning);
  msg.setWindowTitle(title);
  msg.setText(text);
  msg.setInformativeText(informativeText);
  msg.setDetailedText(details);
  msg.exec();
}

void MainWindow::showDebugBuildFailure(const QString &details) {
  showBuildFailure(
      tr("Debug Build Failed"),
      tr("Unable to prepare a debuggable target for this file."),
      tr("The build has to succeed before the debugger can launch."), details);
}

bool MainWindow::prepareDebugTargetForFile(const QString &filePath,
                                           const QString &languageId,
                                           QString *resolvedProgram,
                                           QString *errorMessage) {
  if (resolvedProgram) {
    resolvedProgram->clear();
  }

  const QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (canonicalLanguageId != "cpp" && canonicalLanguageId != "c") {
    return true;
  }

  QFileInfo sourceInfo(filePath);
  if (!sourceInfo.exists()) {
    if (errorMessage) {
      *errorMessage = tr("Source file does not exist: %1").arg(filePath);
    }
    return false;
  }

  if (!cmakeRootForSource(filePath).isEmpty()) {
    QString programPath;
    if (!buildCMakeExecutableForSource(filePath, QString(), QString(),
                                       &programPath, errorMessage)) {
      return false;
    }
    if (!programPath.isEmpty()) {
      if (resolvedProgram) {
        *resolvedProgram = programPath;
      }
      return true;
    }
  }

  QString outputPath =
      sourceInfo.absolutePath() + "/" + sourceInfo.completeBaseName();
#ifdef Q_OS_WIN
  outputPath += ".exe";
#endif

  QFileInfo outputInfo(outputPath);
  const bool needsBuild = !outputInfo.exists() ||
                          outputInfo.lastModified() < sourceInfo.lastModified();
  if (needsBuild && !compileSourceForDebug(filePath, canonicalLanguageId,
                                           outputPath, errorMessage)) {
    return false;
  }

  if (resolvedProgram) {
    *resolvedProgram = outputPath;
  }
  return true;
}

bool MainWindow::compileSourceForDebug(const QString &filePath,
                                       const QString &languageId,
                                       const QString &outputPath,
                                       QString *errorMessage) const {
  const QString compiler = (languageId == "c") ? "gcc" : "g++";

  QStringList args;
  if (languageId == "c") {
    args << "-g" << "-O0" << "-std=c11";
  } else {
    args << "-g" << "-O0" << "-std=c++17";
  }

  FileTemplateAssignment assignment =
      RunTemplateManager::instance().getAssignmentForFile(filePath);

  for (const QString &flag : assignment.compilerFlags) {
    args << RunTemplateManager::substituteVariables(flag, filePath);
  }

  args << "-o" << outputPath << filePath;

  for (const QString &src : assignment.sourceFiles) {
    args << RunTemplateManager::substituteVariables(src, filePath);
  }

  QProcess process;
  process.setProgram(compiler);
  process.setArguments(args);
  process.setWorkingDirectory(QFileInfo(filePath).absolutePath());
  process.start();

  if (!process.waitForStarted(5000)) {
    if (errorMessage) {
      *errorMessage = tr("Failed to start compiler '%1': %2")
                          .arg(compiler, process.errorString());
    }
    return false;
  }

  if (!process.waitForFinished(120000)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = tr("Compilation timed out for %1").arg(filePath);
    }
    return false;
  }

  const QByteArray output =
      process.readAllStandardOutput() + process.readAllStandardError();
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      const QString commandLine = compiler + " " + args.join(" ");
      QString details = QString::fromUtf8(output).trimmed();
      if (details.isEmpty()) {
        details = tr("Compiler exited with code %1").arg(process.exitCode());
      }
      *errorMessage =
          tr("Debug build command failed:\n%1\n\n%2").arg(commandLine, details);
    }
    return false;
  }

  return true;
}

void MainWindow::startDebuggingForCurrentFile() {
  startDebuggingForFile(runSourceFilePath());
}

void MainWindow::startDebuggingForFile(const QString &filePath) {
  if (m_debugStartInProgress) {
    return;
  }

  if (!m_activeDebugSessionId.isEmpty()) {
    if (DebugSession *activeSession =
            DebugSessionManager::instance().session(m_activeDebugSessionId)) {
      if (activeSession->state() != DebugSession::State::Idle &&
          activeSession->state() != DebugSession::State::Terminated) {
        attachDebugSession(m_activeDebugSessionId);
        if (debugDock) {
          debugDock->show();
          debugDock->raise();
        }
        return;
      }
    }
  }

  if (filePath.isEmpty()) {
    noScriptAssignedWarning();
    return;
  }

  save(filePath);
  ensureProjectRootForPath(filePath);

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

  const QString languageId = effectiveLanguageIdForFile(filePath);
  QString prepareError;
  QString preparedProgram;
  if (!prepareDebugTargetForFile(filePath, languageId, &preparedProgram,
                                 &prepareError)) {
    showDebugBuildFailure(prepareError);
    m_debugStartInProgress = false;
    return;
  }

  QString sessionId = DebugSessionManager::instance().quickStart(
      filePath, languageId, preparedProgram);
  if (sessionId.isEmpty()) {
    QString details = DebugSessionManager::instance().lastError();
    DebugConfiguration quickConfig =
        DebugConfigurationManager::instance().createQuickConfig(filePath,
                                                                languageId);
    if (!quickConfig.type.isEmpty() && details.isEmpty()) {
      const auto adapters =
          DebugAdapterRegistry::instance().adaptersForConfiguration(
              quickConfig);
      QStringList adapterStatuses;
      for (const auto &adapter : adapters) {
        if (!adapter) {
          continue;
        }
        adapterStatuses << QString("%1: %2").arg(
            adapter->config().name,
            adapter->statusMessageForConfiguration(quickConfig));
      }
      details = adapterStatuses.join("\n");
    }

    ThemedMessageBox msg(this);
    msg.setIcon(ThemedMessageBox::Warning);
    msg.setWindowTitle(tr("Debug"));
    msg.setText(tr("Unable to start debug session for this file."));
    if (!details.isEmpty()) {
      msg.setInformativeText(details);
    }
    msg.exec();
    m_debugStartInProgress = false;
    return;
  }

  attachDebugSession(sessionId);
  if (debugDock) {
    debugDock->show();
    debugDock->raise();
  }
  m_debugStartInProgress = false;
}

void MainWindow::runFileByPath(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }

  openFileAndAddToNewTab(filePath);

  runPath(filePath);
}

void MainWindow::debugFileByPath(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }

  openFileAndAddToNewTab(filePath);
  startDebuggingForFile(filePath);
}

bool MainWindow::requestLspHover(TextArea *area, int line, int column,
                                 const QPoint &globalPos, QWidget *anchor,
                                 const QRect &anchorRect) {
  if (!m_languageFeatureManager || !area || area != getCurrentTextArea()) {
    return false;
  }
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return false;
  }
  const QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty()) {
    return false;
  }
  LspClient *client = m_languageFeatureManager->clientForFile(filePath);
  if (!client || !client->isReady()) {
    return false;
  }

  flushPendingLanguageServerChanges(filePath);

  const int requestId =
      client->requestHover(area->getDocumentUri(), LspPosition{line, column});
  auto connection = std::make_shared<QMetaObject::Connection>();
  QPointer<QWidget> anchorGuard(anchor);
  *connection =
      connect(client, &LspClient::hoverReceived, this,
              [requestId, connection, globalPos, anchorGuard,
               anchorRect](int id, const QString &contents) {
                if (id != requestId) {
                  return;
                }
                QObject::disconnect(*connection);
                const QString text = contents.trimmed();
                if (text.isEmpty() || !anchorGuard) {
                  return;
                }

                QTextDocument doc;
                doc.setMarkdown(text);
                QString html = doc.toHtml();
                QToolTip::showText(globalPos, html, anchorGuard, anchorRect);
              });
  return true;
}

bool MainWindow::requestDebugHover(const QString &expression,
                                   const QPoint &globalPos, QWidget *anchor,
                                   const QRect &anchorRect) {
  return debugPanel && debugPanel->requestHoverEvaluation(expression, globalPos,
                                                          anchor, anchorRect);
}

void MainWindow::attachDebugSession(const QString &sessionId) {
  if (sessionId.isEmpty()) {
    return;
  }

  DebugSession *session = DebugSessionManager::instance().session(sessionId);
  if (!session || !session->client()) {
    return;
  }

  if (sessionId == m_activeDebugSessionId && debugPanel &&
      debugPanel->dapClient() == session->client()) {
    if (debugDock) {
      debugDock->show();
      debugDock->raise();
    }
    return;
  }

  m_activeDebugSessionId = sessionId;
  DebugSessionManager::instance().setFocusedSession(sessionId);
  debugPanel->setDapClient(session->client());
  WatchManager::instance().setDapClient(session->client());
  if (debugDock) {
    debugDock->show();
    debugDock->raise();
  }
  if (m_breakpointsSetConnection) {
    disconnect(m_breakpointsSetConnection);
  }
  if (m_breakpointChangedConnection) {
    disconnect(m_breakpointChangedConnection);
  }
  if (m_runInTerminalConnection) {
    disconnect(m_runInTerminalConnection);
  }
  if (m_sessionTerminatedConnection) {
    disconnect(m_sessionTerminatedConnection);
  }
  if (m_sessionErrorConnection) {
    disconnect(m_sessionErrorConnection);
  }
  if (m_sessionStateConnection) {
    disconnect(m_sessionStateConnection);
  }
  m_breakpointsSetConnection = connect(
      session->client(), &DapClient::breakpointsSet, this,
      [](const QString &sourcePath, const QList<DapBreakpoint> &breakpoints) {
        if (!sourcePath.isEmpty()) {
          BreakpointManager::instance().updateVerification(sourcePath,
                                                           breakpoints);
        }
      });
  m_breakpointChangedConnection =
      connect(session->client(), &DapClient::breakpointChanged, this,
              [](const DapBreakpoint &breakpoint, const QString &) {
                if (!breakpoint.source.path.isEmpty()) {
                  BreakpointManager::instance().updateVerification(
                      breakpoint.source.path, {breakpoint});
                }
              });
  m_runInTerminalConnection = connect(
      session->client(), &DapClient::runInTerminalRequested, this,
      [this, client = session->client()](
          int requestSeq, const QString &kind, const QString &title,
          const QStringList &commandLine, const QString &cwd,
          const QMap<QString, QString> &env) {
        Q_UNUSED(title);

        if (!client || commandLine.isEmpty()) {
          if (client) {
            client->respondToRunInTerminal(requestSeq, false, 0,
                                           tr("Invalid terminal command"));
          }
          return;
        }

        if (kind == QLatin1String("external")) {
          const ExternalTerminalLaunch launch =
              buildExternalTerminalLaunch(commandLine);
          if (!launch.program.isEmpty()) {
            QProcess external;
            external.setProgram(launch.program);
            external.setArguments(launch.arguments);
            const QString externalCwd =
                cwd.isEmpty()
                    ? (m_projectRootPath.isEmpty() ? QDir::currentPath()
                                                   : m_projectRootPath)
                    : cwd;
            external.setWorkingDirectory(externalCwd);
            QProcessEnvironment processEnv =
                QProcessEnvironment::systemEnvironment();
            for (auto it = env.constBegin(); it != env.constEnd(); ++it) {
              processEnv.insert(it.key(), it.value());
            }
            external.setProcessEnvironment(processEnv);
            qint64 externalPid = 0;
            if (external.startDetached(&externalPid)) {
              client->respondToRunInTerminal(requestSeq, true, externalPid);
              return;
            }
            LOG_WARNING(QString("Failed to start external terminal %1; "
                                "falling back to integrated terminal")
                            .arg(launch.program));
          } else {
            LOG_WARNING("No external terminal emulator found; falling back to "
                        "the integrated terminal");
          }
        }

        TerminalTabWidget *tabs = ensureTerminalWidget();
        if (!tabs) {
          client->respondToRunInTerminal(requestSeq, false, 0,
                                         tr("Terminal panel unavailable"));
          return;
        }

        const bool keepDebugPanelInFront = debugDock && debugDock->isVisible();
        showTerminalPanel();
        if (keepDebugPanelInFront) {
          debugDock->raise();
        }

        QStringList args = commandLine;
        const QString program = args.takeFirst();
        const QString workingDirectory =
            cwd.isEmpty() ? (m_projectRootPath.isEmpty() ? QDir::currentPath()
                                                         : m_projectRootPath)
                          : cwd;

        QPointer<DapClient> clientPtr(client);
        QPointer<Terminal> terminal = tabs->addNewTerminal(workingDirectory);
        QSharedPointer<bool> responded = QSharedPointer<bool>::create(false);

        connect(
            terminal, &Terminal::processStarted, this,
            [clientPtr, terminal, responded, requestSeq]() {
              if (*responded || !clientPtr) {
                return;
              }

              *responded = true;
              clientPtr->respondToRunInTerminal(
                  requestSeq, true, terminal ? terminal->runProcessId() : 0);
            },
            Qt::SingleShotConnection);
        connect(
            terminal, &Terminal::processError, this,
            [clientPtr, responded, requestSeq](const QString &message) {
              if (*responded || !clientPtr) {
                return;
              }

              *responded = true;
              clientPtr->respondToRunInTerminal(requestSeq, false, 0, message);
            },
            Qt::SingleShotConnection);

        terminal->executeCommand(program, args, workingDirectory, env);
      });

  m_sessionTerminatedConnection =
      connect(session, &DebugSession::terminated, this, [this, sessionId]() {
        if (sessionId == m_activeDebugSessionId) {
          clearDebugSession();
        }
      });
  m_sessionErrorConnection = connect(
      session, &DebugSession::error, this, [this](const QString &message) {
        statusBar()->showMessage(tr("Debug: %1").arg(message), 10000);
        if (m_notificationManager) {
          m_notificationManager->showError(tr("Debug Session Error"), message);
        }
      });
  m_sessionStateConnection =
      connect(session, &DebugSession::stateChanged, this,
              [this](DebugSession::State state) {
                if (state != DebugSession::State::Stopped) {
                  updateAllTextAreas(&TextArea::setDebugExecutionLine, 0);
                  updateAllTextAreas(&TextArea::clearDebugInlineValues);
                } else if (state == DebugSession::State::Stopped && debugDock) {
                  debugDock->show();
                  debugDock->raise();
                }
              });
}

void MainWindow::clearDebugSession() {
  m_activeDebugSessionId.clear();
  BreakpointManager::instance().setDapClient(nullptr);
  BreakpointManager::instance().resetVerification();
  BreakpointManager::instance().clearDataBreakpoints();
  if (debugPanel) {
    debugPanel->setDapClient(nullptr);
  }
  WatchManager::instance().setDapClient(nullptr);
  if (m_breakpointsSetConnection) {
    disconnect(m_breakpointsSetConnection);
    m_breakpointsSetConnection = {};
  }
  if (m_breakpointChangedConnection) {
    disconnect(m_breakpointChangedConnection);
    m_breakpointChangedConnection = {};
  }
  if (m_runInTerminalConnection) {
    disconnect(m_runInTerminalConnection);
    m_runInTerminalConnection = {};
  }
  if (m_sessionTerminatedConnection) {
    disconnect(m_sessionTerminatedConnection);
    m_sessionTerminatedConnection = {};
  }
  if (m_sessionErrorConnection) {
    disconnect(m_sessionErrorConnection);
    m_sessionErrorConnection = {};
  }
  if (m_sessionStateConnection) {
    disconnect(m_sessionStateConnection);
    m_sessionStateConnection = {};
  }
  updateAllTextAreas(&TextArea::setDebugExecutionLine, 0);
  updateAllTextAreas(&TextArea::clearDebugInlineValues);
}
