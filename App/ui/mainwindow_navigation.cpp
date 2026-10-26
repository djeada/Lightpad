#include "../core/editor/documentsymbols.h"
#include "mainwindow_internal.h"
#include <QCryptographicHash>
#include <QSaveFile>
#include <algorithm>
#include <memory>

void MainWindow::showCommandPalette() {
  if (commandPalette) {
    commandPalette->showPalette();
  }
}

void MainWindow::setupCommandPalette() {
  commandPalette = new CommandPalette(this);

  QMenuBar *menuBar = this->menuBar();
  for (QAction *action : menuBar->actions()) {
    if (action->menu()) {
      commandPalette->registerMenu(action->menu());
    }
  }
}

void MainWindow::showGoToLineDialog() {
  if (!goToLineDialog) {
    setupGoToLineDialog();
  }

  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    int maxLine = textArea->blockCount();
    goToLineDialog->setMaxLine(maxLine);
    goToLineDialog->showDialog();
  }
}

void MainWindow::setupGoToLineDialog() {
  goToLineDialog = new GoToLineDialog(this);

  connect(goToLineDialog, &GoToLineDialog::lineSelected, this,
          [this](int lineNumber) {
            TextArea *textArea = getCurrentTextArea();
            if (textArea) {
              textArea->setTextCursor(
                  cursorAtLineColumn(textArea->document(), lineNumber - 1, 0));
              textArea->centerCursor();
              textArea->setFocus();
            }
          });
}

void MainWindow::showGoToSymbolDialog() {
  if (!goToSymbolDialog) {
    setupGoToSymbolDialog();
  }

  TextArea *textArea = getCurrentTextArea();
  if (textArea) {

    QList<LspDocumentSymbol> symbols;

    QString language;
    if (LightpadTabWidget *tw = currentTabWidget()) {
      const QString path = tw->getFilePath(tw->currentIndex());
      language = LanguageCatalog::normalize(effectiveLanguageIdForFile(path));
    }
    const auto found = DocumentSymbols::scan(textArea->toPlainText(), language);
    for (const DocumentSymbols::Symbol &s : found) {
      LspDocumentSymbol sym;
      sym.name = s.name;
      switch (s.kind) {
      case DocumentSymbols::Kind::Class:
        sym.kind = LspSymbolKind::Class;
        break;
      case DocumentSymbols::Kind::Method:
        sym.kind = LspSymbolKind::Method;
        break;
      case DocumentSymbols::Kind::Heading:
        sym.kind = LspSymbolKind::Namespace;
        break;
      default:
        sym.kind = LspSymbolKind::Function;
        break;
      }
      sym.selectionRange.start.line = s.line;
      sym.selectionRange.start.character = s.column;
      sym.range = sym.selectionRange;
      symbols.append(sym);
    }

    goToSymbolDialog->setSymbols(symbols);
    goToSymbolDialog->showDialog();
  }
}

void MainWindow::setupGoToSymbolDialog() {
  goToSymbolDialog = new GoToSymbolDialog(this);

  connect(goToSymbolDialog, &GoToSymbolDialog::symbolSelected, this,
          [this](int line, int column) {
            TextArea *textArea = getCurrentTextArea();
            if (textArea) {
              textArea->setTextCursor(
                  cursorAtLineColumn(textArea->document(), line, column));
              textArea->centerCursor();
              textArea->setFocus();
            }
          });
}

void MainWindow::showFileQuickOpen() {
  if (!fileQuickOpen) {
    setupFileQuickOpen();
  }

  QString rootPath =
      m_projectRootPath.isEmpty() ? QDir::currentPath() : m_projectRootPath;

  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  auto filePath = tabWidget->getFilePath(tabIndex);
  if (m_projectRootPath.isEmpty() && !filePath.isEmpty()) {
    QFileInfo fileInfo(filePath);
    rootPath = fileInfo.absolutePath();

    QDir dir(rootPath);
    while (dir.exists()) {
      if (dir.exists(".git") || dir.exists("CMakeLists.txt") ||
          dir.exists("package.json") || dir.exists("Makefile")) {
        rootPath = dir.absolutePath();
        break;
      }
      if (!dir.cdUp()) {
        break;
      }
    }
  }

  fileQuickOpen->setRootDirectory(rootPath);
  fileQuickOpen->showDialog();
}

void MainWindow::setupFileQuickOpen() {
  fileQuickOpen = new FileQuickOpen(this);

  connect(
      fileQuickOpen, &FileQuickOpen::fileSelected, this,
      [this](const QString &filePath) { openFileAndAddToNewTab(filePath); });
}

void MainWindow::showRecentFilesDialog() {
  if (!recentFilesDialog) {
    setupRecentFilesDialog();
  }

  recentFilesDialog->showDialog();
}

void MainWindow::setupRecentFilesDialog() {
  recentFilesDialog = new RecentFilesDialog(recentFilesManager, this);

  connect(
      recentFilesDialog, &RecentFilesDialog::fileSelected, this,
      [this](const QString &filePath) { openFileAndAddToNewTab(filePath); });
}

void MainWindow::setupBreadcrumb() {
  breadcrumbWidget = new BreadcrumbWidget(this);

  auto layout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
  if (layout) {

    layout->insertWidget(0, breadcrumbWidget);
  }

  connect(breadcrumbWidget, &BreadcrumbWidget::pathSegmentClicked, this,
          [this](const QString &path) {
            QFileInfo fileInfo(path);
            if (fileInfo.isDir()) {

            } else if (fileInfo.isFile()) {
              openFileAndAddToNewTab(path);
            }
          });
}

void MainWindow::updateBreadcrumb(const QString &filePath) {
  if (breadcrumbWidget) {
    breadcrumbWidget->setFilePath(filePath);
    if (!m_projectRootPath.isEmpty()) {
      breadcrumbWidget->setProjectRoot(m_projectRootPath);
    }
  }
}

void MainWindow::toggleShowWhitespace() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setShowWhitespace(!textArea->showWhitespace());
  }
}

void MainWindow::toggleShowIndentGuides() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setShowIndentGuides(!textArea->showIndentGuides());
  }
}

void MainWindow::navigateBack() {
  if (!navigationHistory || !navigationHistory->canGoBack()) {
    return;
  }

  NavigationLocation loc = navigationHistory->goBack();
  if (loc.isValid()) {

    openFileAndAddToNewTab(loc.filePath);

    TextArea *textArea = getCurrentTextArea();
    if (textArea) {
      textArea->setTextCursor(
          cursorAtLineColumn(textArea->document(), loc.line - 1, loc.column));
      textArea->centerCursor();
    }
  }
}

void MainWindow::navigateForward() {
  if (!navigationHistory || !navigationHistory->canGoForward()) {
    return;
  }

  NavigationLocation loc = navigationHistory->goForward();
  if (loc.isValid()) {

    openFileAndAddToNewTab(loc.filePath);

    TextArea *textArea = getCurrentTextArea();
    if (textArea) {
      textArea->setTextCursor(
          cursorAtLineColumn(textArea->document(), loc.line - 1, loc.column));
      textArea->centerCursor();
    }
  }
}

void MainWindow::recordNavigationLocation() {
  if (!navigationHistory) {
    return;
  }

  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  QString filePath = tabWidget->getFilePath(tabIndex);
  if (filePath.isEmpty()) {
    return;
  }

  QTextCursor cursor = textArea->textCursor();
  NavigationLocation loc;
  loc.filePath = filePath;
  loc.line = cursor.blockNumber() + 1;
  loc.column = cursor.positionInBlock();

  navigationHistory->recordLocationIfSignificant(loc);
}

void MainWindow::setupNavigationHistory() {
  navigationHistory = new NavigationHistory(this);
}

void MainWindow::setupSymbolNavigation() {
  m_symbolNavService = new SymbolNavigationService(this);

  const QStringList languages =
      m_languageFeatureManager ? m_languageFeatureManager->supportedLanguages()
                               : QStringList{};
  for (const QString &language : languages) {
    const auto server = m_languageFeatureManager->serverConfig(language);
    const LanguageServerConfig config{"lsp-" + language,
                                      LanguageCatalog::displayName(language),
                                      {language},
                                      server.command,
                                      server.arguments};
    auto *provider = new LanguageLspDefinitionProvider(config, this);
    provider->setClientResolver([this](const QString &filePath) -> LspClient * {
      if (!m_languageFeatureManager) {
        return nullptr;
      }
      flushPendingLanguageServerChanges(filePath);
      return m_languageFeatureManager->clientForFile(filePath);
    });
    m_symbolNavService->registerProvider(provider);
  }

  connect(m_symbolNavService, &SymbolNavigationService::definitionFound, this,
          &MainWindow::handleDefinitionResults);

  connect(m_symbolNavService, &SymbolNavigationService::noDefinitionFound, this,
          [this](const QString &message) {
            statusBar()->showMessage(message, 5000);
          });

  connect(m_symbolNavService,
          &SymbolNavigationService::definitionRequestStarted, this, [this]() {
            statusBar()->showMessage(QCoreApplication::translate(
                "MainWindow", "Searching for definition..."));
          });

  connect(m_symbolNavService,
          &SymbolNavigationService::definitionRequestFinished, this, [this]() {
            QString expected = QCoreApplication::translate(
                "MainWindow", "Searching for definition...");
            if (statusBar()->currentMessage() == expected) {
              statusBar()->clearMessage();
            }
          });
}

void MainWindow::goToDefinitionAtCursor() {
  ++m_externalDefinitionGeneration;
  if (!m_symbolNavService) {
    return;
  }

  if (m_symbolNavService->isRequestInFlight()) {
    statusBar()->showMessage(tr("Definition lookup already in progress..."),
                             3000);
    return;
  }

  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return;
  }

  int tabIndex = tabWidget->currentIndex();
  QString filePath = tabWidget->getFilePath(tabIndex);
  if (filePath.isEmpty()) {
    statusBar()->showMessage(tr("No file open"), 3000);
    return;
  }

  QString languageId = effectiveLanguageIdForFile(filePath);

  QTextCursor cursor = textArea->textCursor();

  recordNavigationLocation();

  DefinitionRequest req;
  req.filePath = filePath;
  req.line = cursor.blockNumber() + 1;
  req.column = cursor.positionInBlock();
  req.languageId = languageId;
  req.documentUri = textArea->getDocumentUri();

  m_symbolNavService->goToDefinition(req);
}

void MainWindow::renameSymbolAtCursor() {
  TextArea *textArea = getCurrentTextArea();
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!textArea || !tabWidget || !m_languageFeatureManager) {
    return;
  }

  const QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty()) {
    statusBar()->showMessage(tr("Save the file before renaming symbols"), 4000);
    return;
  }

  LspClient *client = m_languageFeatureManager->clientForFile(filePath);
  if (!client) {
    statusBar()->showMessage(
        tr("Rename needs a running language server for this file"), 5000);
    return;
  }

  QTextCursor cursor = textArea->textCursor();
  QTextCursor word = cursor;
  word.select(QTextCursor::WordUnderCursor);
  const QString current = word.selectedText();
  if (current.isEmpty()) {
    statusBar()->showMessage(tr("Place the caret on a symbol to rename it"),
                             4000);
    return;
  }

  bool ok = false;
  const QString newName = QInputDialog::getText(
      this, tr("Rename Symbol"), tr("New name for '%1':").arg(current),
      QLineEdit::Normal, current, &ok);
  if (!ok || newName.isEmpty() || newName == current) {
    return;
  }

  flushPendingLanguageServerChanges(filePath);

  const QString uri = textArea->getDocumentUri();
  const LspPosition position{cursor.blockNumber(), cursor.positionInBlock()};
  const int requestId = client->requestRename(uri, position, newName);

  auto connection = std::make_shared<QMetaObject::Connection>();
  *connection = connect(
      client, &LspClient::renameReceived, this,
      [this, requestId, connection](int id, const LspWorkspaceEdit &edit) {
        if (id != requestId) {
          return;
        }
        QObject::disconnect(*connection);

        if (edit.changes.isEmpty()) {
          statusBar()->showMessage(
              tr("The language server could not rename this symbol"), 5000);
          return;
        }

        int editCount = 0;
        for (auto it = edit.changes.begin(); it != edit.changes.end(); ++it) {
          const QString path = QUrl(it.key()).toLocalFile();
          if (path.isEmpty()) {
            continue;
          }
          if (!isFileOpenInEditor(path)) {
            openFileAndAddToNewTab(path);
          }
          TextArea *target = nullptr;
          for (LightpadTabWidget *tw : allTabWidgets()) {
            for (int i = 0; tw && i < tw->count() && !target; ++i) {
              if (QDir::cleanPath(tw->getFilePath(i)) ==
                  QDir::cleanPath(path)) {
                if (auto *page = tw->getPage(i)) {
                  target = page->getTextArea();
                }
              }
            }
          }
          if (!target) {
            continue;
          }

          QList<LspTextEdit> edits = it.value();
          std::sort(edits.begin(), edits.end(),
                    [](const LspTextEdit &a, const LspTextEdit &b) {
                      if (a.range.start.line != b.range.start.line) {
                        return a.range.start.line > b.range.start.line;
                      }
                      return a.range.start.character > b.range.start.character;
                    });
          QTextDocument *doc = target->document();
          QTextCursor tc(doc);
          tc.beginEditBlock();
          for (const LspTextEdit &e : edits) {
            const QTextBlock startBlock =
                doc->findBlockByNumber(e.range.start.line);
            const QTextBlock endBlock =
                doc->findBlockByNumber(e.range.end.line);
            if (!startBlock.isValid() || !endBlock.isValid()) {
              continue;
            }
            tc.setPosition(
                startBlock.position() +
                qMin(e.range.start.character, startBlock.length() - 1));
            tc.setPosition(endBlock.position() + qMin(e.range.end.character,
                                                      endBlock.length() - 1),
                           QTextCursor::KeepAnchor);
            tc.insertText(e.newText);
            ++editCount;
          }
          tc.endEditBlock();
        }
        statusBar()->showMessage(
            tr("Renamed: %n occurrence(s) changed", nullptr, editCount), 5000);
      });
}

void MainWindow::handleDefinitionResults(
    const QList<DefinitionTarget> &targets) {
  if (targets.size() == 1) {
    jumpToTarget(targets.first());
  } else if (targets.size() > 1) {
    QStringList items;
    for (const DefinitionTarget &t : targets) {
      QString label = t.label.isEmpty()
                          ? QStringLiteral("%1:%2")
                                .arg(QFileInfo(t.filePath).fileName())
                                .arg(t.line)
                          : t.label;
      items << label;
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(this, tr("Go to Definition"),
                                             tr("Multiple definitions found:"),
                                             items, 0, false, &ok);
    if (ok && !selected.isEmpty()) {
      int idx = items.indexOf(selected);
      if (idx >= 0 && idx < targets.size()) {
        jumpToTarget(targets.at(idx));
      }
    }
  }
}

void MainWindow::jumpToTarget(const DefinitionTarget &target) {
  if (!target.isValid()) {
    return;
  }

  const QUrl uri(target.filePath);
  if (!uri.scheme().isEmpty() && uri.scheme() != "file" &&
      !QFileInfo(target.filePath).isAbsolute()) {
    auto *client =
        m_languageFeatureManager
            ? m_languageFeatureManager->clientForFile(target.sourceFilePath)
            : nullptr;
    if (!client || !client->isReady()) {
      statusBar()->showMessage(
          tr("The language server is not available for this library source"),
          5000);
      return;
    }
    const int requestId = client->requestDocumentContent(target.filePath);
    if (requestId < 0) {
      statusBar()->showMessage(
          tr("The language server cannot open this library source"), 5000);
      return;
    }
    const int generation = ++m_externalDefinitionGeneration;
    auto *context = new QObject(this);
    connect(
        client, &LspClient::documentContentReceived, context,
        [this, context, target, uri, requestId,
         generation](int id, const QString &, const QString &text) {
          if (id != requestId)
            return;
          context->deleteLater();
          if (generation != m_externalDefinitionGeneration)
            return;
          if (text.isEmpty()) {
            statusBar()->showMessage(
                tr("No source content was provided for this library"), 5000);
            return;
          }
          const QString cache =
              QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
          const QString directory = QDir(cache).filePath("lsp-sources");
          QString name = QFileInfo(uri.path()).fileName();
          if (name.endsWith(".class"))
            name = name.left(name.size() - 6) + ".java";
          if (name.isEmpty())
            name = "library-source.txt";
          const QString hash = QString::fromLatin1(
              QCryptographicHash::hash(target.filePath.toUtf8(),
                                       QCryptographicHash::Sha256)
                  .toHex());
          const QString path = QDir(directory).filePath(hash + "-" + name);
          QSaveFile file(path);
          const QByteArray bytes = text.toUtf8();
          if (cache.isEmpty() || !QDir().mkpath(directory) ||
              !file.open(QIODevice::WriteOnly) ||
              file.write(bytes) != bytes.size() || !file.commit()) {
            statusBar()->showMessage(tr("Could not open the library source"),
                                     5000);
            return;
          }
          m_languageFeatureManager->openExternalDocument(
              path, target.filePath, target.sourceFilePath, text);
          auto localTarget = target;
          localTarget.filePath = path;
          jumpToTarget(localTarget);
          if (auto *area = getCurrentTextArea())
            area->setReadOnly(true);
          if (auto *tabs = currentTabWidget())
            tabs->setTabText(tabs->currentIndex(), name);
          statusBar()->showMessage(tr("External library source (read-only)"),
                                   3000);
        });
    connect(client, &LspClient::requestFailed, context,
            [this, context, requestId, generation](int id, const QString &,
                                                   const QString &message) {
              if (id != requestId)
                return;
              context->deleteLater();
              if (generation == m_externalDefinitionGeneration)
                statusBar()->showMessage(message, 5000);
            });
    QTimer::singleShot(15000, context, [this, context, generation]() {
      if (generation == m_externalDefinitionGeneration)
        statusBar()->showMessage(tr("Library source request timed out"), 5000);
      context->deleteLater();
    });
    return;
  }
  ++m_externalDefinitionGeneration;

  openFileAndAddToNewTab(target.filePath);

  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setTextCursor(cursorAtLineColumn(textArea->document(),
                                               target.line - 1, target.column));
    textArea->centerCursor();
  }

  if (navigationHistory) {
    NavigationLocation loc;
    loc.filePath = target.filePath;
    loc.line = target.line;
    loc.column = target.column;
    navigationHistory->recordLocation(loc);
  }
}
