#include "mainwindow_internal.h"

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

    QTextBlock block = textArea->document()->begin();
    while (block.isValid()) {
      QString text = block.text().trimmed();

      if (text.contains('(') && !text.startsWith("//") &&
          !text.startsWith("/*")) {

        int parenPos = text.indexOf('(');
        QString beforeParen = text.left(parenPos).trimmed();

        QStringList parts = beforeParen.split(QRegularExpression("\\s+"));
        if (!parts.isEmpty()) {
          QString name = parts.last();

          while (!name.isEmpty() && (name[0] == '*' || name[0] == '&')) {
            name = name.mid(1);
          }

          if (!name.isEmpty() && name[0].isLetter()) {
            LspDocumentSymbol sym;
            sym.name = name;
            sym.kind = LspSymbolKind::Function;
            sym.selectionRange.start.line = block.blockNumber();
            sym.selectionRange.start.character = 0;
            sym.range = sym.selectionRange;

            if (beforeParen.startsWith("class ") ||
                beforeParen.startsWith("struct ")) {
              sym.kind = LspSymbolKind::Class;
            }

            symbols.append(sym);
          }
        }
      }

      else if (text.startsWith("class ") || text.startsWith("struct ")) {
        QStringList parts = text.split(QRegularExpression("\\s+"));
        if (parts.size() >= 2) {
          QString name = parts[1];

          name = name.split(QRegularExpression("[:{]")).first();

          if (!name.isEmpty()) {
            LspDocumentSymbol sym;
            sym.name = name;
            sym.kind = LspSymbolKind::Class;
            sym.selectionRange.start.line = block.blockNumber();
            sym.selectionRange.start.character = 0;
            sym.range = sym.selectionRange;
            symbols.append(sym);
          }
        }
      }

      else if (text.startsWith("def ") || text.startsWith("class ")) {
        QString keyword = text.startsWith("def ") ? "def " : "class ";
        QString rest = text.mid(keyword.length());
        int endPos = rest.indexOf(QRegularExpression("[:(]"));
        if (endPos > 0) {
          QString name = rest.left(endPos).trimmed();

          LspDocumentSymbol sym;
          sym.name = name;
          sym.kind = text.startsWith("def ") ? LspSymbolKind::Function
                                             : LspSymbolKind::Class;
          sym.selectionRange.start.line = block.blockNumber();
          sym.selectionRange.start.character = 0;
          sym.range = sym.selectionRange;
          symbols.append(sym);
        }
      }

      else if (text.startsWith("function ")) {
        QString rest = text.mid(9);
        int endPos = rest.indexOf('(');
        if (endPos > 0) {
          QString name = rest.left(endPos).trimmed();

          LspDocumentSymbol sym;
          sym.name = name;
          sym.kind = LspSymbolKind::Function;
          sym.selectionRange.start.line = block.blockNumber();
          sym.selectionRange.start.character = 0;
          sym.range = sym.selectionRange;
          symbols.append(sym);
        }
      }

      block = block.next();
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

  const auto configs = LanguageLspDefinitionProvider::defaultConfigs();
  for (const LanguageServerConfig &config : configs) {
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

  m_symbolNavService->goToDefinition(req);
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

