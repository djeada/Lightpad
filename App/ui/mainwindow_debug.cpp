#include "mainwindow_internal.h"
#include <QToolTip>
#include <memory>

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

void MainWindow::startDebuggingTests(const QStringList &targets,
                                     const TestConfiguration &configuration) {
  if (m_projectRootPath.isEmpty() || configuration.outputFormat != "pytest")
    return;
  DebugSettings::instance().initialize(m_projectRootPath);
  auto &configurations = DebugConfigurationManager::instance();
  configurations.setWorkspaceFolder(m_projectRootPath);
  BreakpointManager::instance().setWorkspaceFolder(m_projectRootPath);
  const auto config = configurations.createPytestConfig(targets, configuration);
  const QString id = DebugSessionManager::instance().startSession(config);
  if (id.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Debug Tests"),
                              DebugSessionManager::instance().lastError());
    return;
  }
  attachDebugSession(id);
  if (debugDock) {
    debugDock->show();
    debugDock->raise();
  }
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

// Adapters such as GDB's DAP server give the debuggee no stdin, so scanf/cin
// see EOF. For those, run the debuggee on the pty of a dedicated terminal tab:
// a placeholder process owns the pty and reports its tty name, which the
// session hands to the debugger before launch.
void MainWindow::hookInferiorTerminal(DebugSession *session) {
  static const char *const kHooked = "lightpadInferiorTerminalHooked";
  if (!session || session->property(kHooked).toBool()) {
    return;
  }
  session->setProperty(kHooked, true);

  const QString sessionId = session->id();
  QPointer<DebugSession> guarded(session);
  connect(session, &DebugSession::inferiorTerminalRequested, this,
          [this, guarded, sessionId]() {
            if (!guarded) {
              return;
            }
#ifdef Q_OS_WIN
            guarded->provideInferiorTerminal({});
#else
        TerminalTabWidget *tabs = ensureTerminalWidget();
        if (!tabs) {
          guarded->provideInferiorTerminal({});
          return;
        }

        QString cwd = guarded->configuration().cwd;
        if (cwd.isEmpty()) {
          cwd = m_projectRootPath.isEmpty() ? QDir::currentPath()
                                            : m_projectRootPath;
        }

        QPointer<Terminal> terminal = m_debugTerminals.value(sessionId);
        if (terminal) {
          terminal->stopShell();
          terminal->clear();
        } else {
          terminal = tabs->addNewTerminal(cwd, tr("Debuggee I/O"));
          registerDebugTerminal(sessionId, terminal);
        }

        const QString ttyFile = QDir::temp().filePath(
            QStringLiteral("lightpad-debug-tty-%1")
                .arg(QUuid::createUuid().toString(QUuid::Id128)));
        const ShellProfile defaultProfile =
            ShellProfileManager::instance().defaultProfile();
        const QString pidFile = ttyFile + QStringLiteral(".pid");
        // The debuggee has no controlling terminal, so Ctrl+C typed here would
        // reach nobody. The placeholder owns the pty instead and forwards
        // SIGINT to the debuggee, which the debugger reports as a pause.
        terminal->setShellProfile(ShellProfile(
            tr("Debuggee I/O"), QStringLiteral("/bin/sh"),
            {QStringLiteral("-c"),
             QStringLiteral(
                 "trap '' QUIT; "
                 "trap 'p=$(cat \"$2\" 2>/dev/null); "
                 "[ -n \"$p\" ] && kill -INT \"$p\" 2>/dev/null' INT; "
                 "tty > \"$1\"; "
                 "while :; do sleep 3600 & wait $!; done"),
             QStringLiteral("lightpad-debug"), ttyFile, pidFile}));
        if (!terminal->isRunning()) {
          terminal->startShell(cwd);
        }

        auto finish = [this, guarded, terminal, ttyFile,
                       sessionId](const QString &tty) {
          QFile::remove(ttyFile);
          if (guarded) {
            if (terminal && !tty.isEmpty()) {
              terminal->appendNotice(
                  tr("Program input and output. Type here and press Enter to "
                     "send input."));
            }
            guarded->provideInferiorTerminal(tty);
            if (!tty.isEmpty()) {
              showDebugTerminal(sessionId);
            }
          } else if (terminal) {
            terminal->stopShell();
          }
        };

        auto *poll = new QTimer(terminal);
        auto attempts = QSharedPointer<int>::create(0);
        connect(poll, &QTimer::timeout, poll,
                [poll, attempts, ttyFile, finish]() {
                  QFile file(ttyFile);
                  QString tty;
                  if (file.open(QIODevice::ReadOnly)) {
                    tty = QString::fromLocal8Bit(file.readAll()).trimmed();
                  }
                  const bool ready = tty.startsWith(QLatin1String("/dev/"));
                  if (ready || ++*attempts >= 80) {
                    poll->stop();
                    poll->deleteLater();
                    finish(ready ? tty : QString());
                  }
                });
        poll->start(50);

        if (guarded->client()) {
          connect(guarded->client(), &DapClient::processChanged, terminal,
                  [pidFile](int pid, const QString &) {
                    QFile file(pidFile);
                    if (pid > 0 &&
                        file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                      file.write(QByteArray::number(pid));
                    }
                  });
        }

        // Release the placeholder once the session is over, and leave the tab
        // with a normal shell profile so it stays usable.
        connect(
            guarded, &DebugSession::terminated, terminal,
            [terminal, defaultProfile, pidFile]() {
              QFile::remove(pidFile);
              if (!terminal) {
                return;
              }
              terminal->stopShell();
              terminal->setShellProfile(defaultProfile);
            },
            Qt::SingleShotConnection);
#endif
          });
  connect(session, &DebugSession::terminated, this, [this, sessionId]() {
    m_debugTerminals.remove(sessionId);
    if (m_activeDebugSessionId == sessionId && debugPanel) {
      debugPanel->setProgramIoAvailable(false);
    }
  });
}

void MainWindow::registerDebugTerminal(const QString &sessionId,
                                       Terminal *terminal) {
  m_debugTerminals.insert(sessionId, terminal);
  if (m_activeDebugSessionId == sessionId && debugPanel) {
    debugPanel->setProgramIoAvailable(true);
  }
  // Skip this callback during teardown, after MainWindow's members are gone.
  connect(
      terminal, &QObject::destroyed, this,
      [this, sessionId]() {
        if (m_debugTerminals.value(sessionId)) {
          return;
        }
        m_debugTerminals.remove(sessionId);
        if (m_activeDebugSessionId == sessionId && debugPanel) {
          debugPanel->setProgramIoAvailable(false);
        }
      },
      Qt::QueuedConnection);
}

void MainWindow::showDebugTerminal(const QString &sessionId) {
  Terminal *terminal = m_debugTerminals.value(sessionId);
  if (!terminalWidget || !terminal) {
    return;
  }
  showTerminalPanel();
  terminalWidget->setCurrentTerminal(terminal);
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
  hookInferiorTerminal(session);
  DebugSessionManager::instance().setFocusedSession(sessionId);
  debugPanel->setDapClient(session->client());
  debugPanel->setProgramIoAvailable(m_debugTerminals.value(sessionId));
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
      [this, client = session->client(),
       sessionId](int requestSeq, const QString &kind, const QString &title,
                  const QStringList &commandLine, const QString &cwd,
                  const QMap<QString, QString> &env) {
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

        QStringList args = commandLine;
        const QString program = args.takeFirst();
        const QString workingDirectory =
            cwd.isEmpty() ? (m_projectRootPath.isEmpty() ? QDir::currentPath()
                                                         : m_projectRootPath)
                          : cwd;

        QPointer<DapClient> clientPtr(client);
        QPointer<Terminal> terminal = tabs->addNewTerminal(
            workingDirectory, title.isEmpty() ? tr("Debuggee I/O") : title);
        registerDebugTerminal(sessionId, terminal);
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
        showDebugTerminal(sessionId);
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
