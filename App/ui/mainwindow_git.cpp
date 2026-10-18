#include "mainwindow_internal.h"
#include "dialogs/gitobjectexplorerdialog.h"

void MainWindow::on_actionGit_Log_triggered() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    ThemedMessageBox::information(this, tr("Git Log"),
                                  tr("No valid Git repository found."));
    return;
  }

  GitLogDialog dialog(m_gitIntegration, settings.theme, this);
  connect(&dialog, &GitLogDialog::inspectObjectRequested, this,
          &MainWindow::showGitObjects);
  connect(&dialog, &GitLogDialog::compareRequested, this,
          [this](const QString &from, const QString &to) {
            showCompareAnything(from, to);
          });
  connect(&dialog, &GitLogDialog::viewCommitDiff, this,
          [this](const QString &hash) {
            GitDiffDialog diffDialog(m_gitIntegration, hash,
                                     GitDiffDialog::DiffTarget::Commit, false,
                                     settings.theme, this);
            diffDialog.setDiffText(m_gitIntegration->getCommitDiff(hash));
            diffDialog.exec();
          });
  connect(&dialog, &GitLogDialog::repositoryChanged, this, [this]() {
    if (sourceControlPanel) {
      sourceControlPanel->refresh();
    }
  });

  dialog.exec();
}

void MainWindow::on_actionGit_Objects_triggered() { showGitObjects(); }

void MainWindow::showGitObjects(const QString &hash) {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    ThemedMessageBox::information(this, tr("Git Objects & Packfiles"),
                                  tr("Open a Git repository first."));
    return;
  }
  GitObjectExplorerDialog dialog(m_gitIntegration->repositoryPath(),
                                 settings.theme, this);
  if (!hash.isEmpty())
    dialog.searchHash(hash);
  connect(&dialog, &GitObjectExplorerDialog::viewCommitRequested, this,
          [this, &dialog](const QString &commit) {
            GitLogDialog graph(m_gitIntegration, settings.theme, &dialog);
            connect(&graph, &GitLogDialog::inspectObjectRequested, this,
                    &MainWindow::showGitObjects);
            connect(&graph, &GitLogDialog::compareRequested, this,
                    [this](const QString &from, const QString &to) {
                      showCompareAnything(from, to);
                    });
            connect(&graph, &GitLogDialog::viewCommitDiff, this,
                    [this](const QString &target) {
                      GitDiffDialog diff(m_gitIntegration, target,
                                         GitDiffDialog::DiffTarget::Commit,
                                         false, settings.theme, this);
                      diff.setDiffText(m_gitIntegration->getCommitDiff(target));
                      diff.exec();
                    });
            graph.selectCommit(commit);
            graph.exec();
          });
  dialog.exec();
}

void MainWindow::on_actionGit_Compare_triggered() { showCompareAnything(); }

void MainWindow::showCompareAnything(const QString &baseRef,
                                     const QString &compareRef,
                                     const QString &filePath) {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    ThemedMessageBox::information(this, tr("Compare Anything"),
                                  tr("No valid Git repository found."));
    return;
  }

  CompareAnythingDialog dialog(m_gitIntegration, settings.theme, this);
  connect(&dialog, &CompareAnythingDialog::fileOpenRequested, this,
          [this](const QString &path) {
            openFileAndAddToNewTab(
                QDir(m_gitIntegration->repositoryPath()).filePath(path));
          });

  if (!baseRef.isEmpty() || !compareRef.isEmpty()) {
    const GitCompareEndpoint base =
        baseRef.isEmpty() ? GitCompareEndpoint::head()
                          : GitCompareEndpoint::commit(baseRef, baseRef);
    const GitCompareEndpoint compare =
        compareRef.isEmpty()
            ? GitCompareEndpoint::workingTree()
            : GitCompareEndpoint::commit(compareRef, compareRef);
    dialog.setEndpoints(base, compare);
  }
  if (!filePath.isEmpty()) {
    dialog.selectFile(filePath);
  }

  dialog.exec();
}

void MainWindow::on_actionGit_File_History_triggered() { showFileHistory(); }

void MainWindow::on_actionGit_Provenance_triggered() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    ThemedMessageBox::information(this, tr("Code provenance"),
                                  tr("No valid Git repository found."));
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  TextArea *textArea = getCurrentTextArea();
  if (!tabWidget || !textArea) {
    return;
  }

  const QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty()) {
    ThemedMessageBox::information(this, tr("Code provenance"),
                                  tr("Save the file first — Git can only "
                                     "trace a file it knows about."));
    return;
  }

  QString relativePath = filePath;
  const QString root = m_gitIntegration->repositoryPath();
  if (!root.isEmpty() && filePath.startsWith(root)) {
    relativePath = QDir(root).relativeFilePath(filePath);
  }

  const QTextCursor cursor = textArea->textCursor();
  int startLine = cursor.blockNumber() + 1;
  int endLine = startLine;
  if (cursor.hasSelection()) {
    QTextCursor start(cursor);
    start.setPosition(cursor.selectionStart());
    QTextCursor end(cursor);
    end.setPosition(cursor.selectionEnd());
    startLine = start.blockNumber() + 1;
    endLine = end.blockNumber() + 1;
  }

  ProvenanceLensDialog dialog(m_gitIntegration, relativePath, startLine,
                              endLine, settings.theme, this);
  connect(&dialog, &ProvenanceLensDialog::openCommitRequested, this,
          [this](const QString &hash) {
            GitDiffDialog diffDialog(m_gitIntegration, hash,
                                     GitDiffDialog::DiffTarget::Commit, false,
                                     settings.theme, this);
            diffDialog.setDiffText(m_gitIntegration->getCommitDiff(hash));
            diffDialog.exec();
          });
  connect(
      &dialog, &ProvenanceLensDialog::openParentDiffRequested, this,
      [this](const QString &hash) { showCompareAnything(hash + "^", hash); });
  connect(&dialog, &ProvenanceLensDialog::openFileHistoryRequested, this,
          [this](const QString &, int, int) { showFileHistory(); });
  connect(&dialog, &ProvenanceLensDialog::showInGraphRequested, this,
          [this](const QString &hash) {
            GitLogDialog logDialog(m_gitIntegration, settings.theme, this);
            logDialog.selectCommit(hash);
            logDialog.exec();
          });
  dialog.exec();
}

void MainWindow::showFileHistory() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    ThemedMessageBox::information(this, tr("File History"),
                                  tr("No valid Git repository found."));
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty()) {
    ThemedMessageBox::information(this, tr("File History"),
                                  tr("No file is currently open."));
    return;
  }

  QString relativePath = filePath;
  const QString root = m_gitIntegration->repositoryPath();
  if (!root.isEmpty() && filePath.startsWith(root)) {
    relativePath = QDir(root).relativeFilePath(filePath);
  }

  FileTimelineDialog dialog(m_gitIntegration, relativePath, settings.theme,
                            this);

  if (TextArea *area = getCurrentTextArea()) {
    const QTextCursor cursor = area->textCursor();
    if (cursor.hasSelection()) {
      QTextCursor start(cursor);
      start.setPosition(cursor.selectionStart());
      QTextCursor end(cursor);
      end.setPosition(cursor.selectionEnd());
      dialog.setLineRange(start.blockNumber() + 1, end.blockNumber() + 1);
    }
  }

  connect(&dialog, &FileTimelineDialog::openCommitRequested, this,
          [this](const QString &hash) {
            GitDiffDialog diffDialog(m_gitIntegration, hash,
                                     GitDiffDialog::DiffTarget::Commit, false,
                                     settings.theme, this);
            diffDialog.setDiffText(m_gitIntegration->getCommitDiff(hash));
            diffDialog.exec();
          });
  connect(&dialog, &FileTimelineDialog::compareRequested, this,
          [this](const QString &from, const QString &to, const QString &path) {
            showCompareAnything(from, to, path);
          });
  connect(&dialog, &FileTimelineDialog::showInGraphRequested, this,
          [this](const QString &hash, const QString &path) {
            GitLogDialog logDialog(m_gitIntegration, settings.theme, this);
            logDialog.setFilePath(path);
            logDialog.selectCommit(hash);
            logDialog.exec();
          });
  dialog.exec();
}

void MainWindow::openReadOnlyTab(const QString &content, const QString &title,
                                 const QString &originalFilePath) {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  int newIndex = tabWidget->addTab(new LightpadPage(tabWidget), title);
  tabWidget->setCurrentIndex(newIndex);

  auto *page = tabWidget->getPage(newIndex);
  if (page) {
    auto *textArea = page->getTextArea();
    if (textArea) {
      textArea->setMainWindow(this);
      textArea->setPlainText(content);
      textArea->setReadOnly(true);

      if (!originalFilePath.isEmpty()) {
        applyHighlightForFile(originalFilePath);
      }
    }
  }
}

void MainWindow::on_actionGit_Rebase_triggered() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    ThemedMessageBox::information(this, tr("Rebase timeline"),
                                  tr("No valid Git repository found."));
    return;
  }

  RebaseTimelineDialog dialog(m_gitIntegration, settings.theme, this);
  connect(&dialog, &RebaseTimelineDialog::repositoryChanged, this, [this]() {
    if (sourceControlPanel) {
      sourceControlPanel->refresh();
    }
  });
  dialog.exec();
}

void MainWindow::on_actionGit_Workbench_triggered() { showGitWorkbench(); }

void MainWindow::showGitWorkbench() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    ThemedMessageBox::information(this, tr("Git Workbench"),
                                  tr("No valid Git repository found."));
    return;
  }

  GitWorkbenchDialog dialog(m_gitIntegration, settings.theme, this);
  connect(&dialog, &GitWorkbenchDialog::inspectObjectRequested, this,
          &MainWindow::showGitObjects);
  connect(&dialog, &GitWorkbenchDialog::commitGraphRequested, this,
          &MainWindow::on_actionGit_Log_triggered);
  connect(&dialog, &GitWorkbenchDialog::viewCommitDiff,
          [this](const QString &hash) {
            QString diff = m_gitIntegration->getCommitDiff(hash);
            GitCommitInfo info = m_gitIntegration->getCommitDetails(hash);
            GitDiffDialog *diffDialog = new GitDiffDialog(
                m_gitIntegration, hash, GitDiffDialog::DiffTarget::Commit,
                false, settings.theme, this);
            diffDialog->setDiffText(diff);
            diffDialog->setCommitInfo(info.author, info.date, info.subject);
            diffDialog->setAttribute(Qt::WA_DeleteOnClose);
            diffDialog->show();
          });
  dialog.exec();
}

void MainWindow::on_actionToggle_Heatmap_triggered(bool checked) {
  m_heatmapEnabled = checked;
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setHeatmapEnabled(checked);
    if (checked) {
      updateHeatmapForCurrentFile();
    }
  }
}

void MainWindow::on_actionToggle_CodeLens_triggered(bool checked) {
  m_codeLensEnabled = checked;
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setCodeLensEnabled(checked);
    if (checked) {
      updateCodeLensForCurrentFile();
    } else {
      textArea->clearCodeLensEntries();
    }
  }
}

void MainWindow::updateHeatmapForCurrentFile() {
  if (!m_heatmapEnabled || !m_gitIntegration ||
      !m_gitIntegration->isValidRepository())
    return;

  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty())
    return;

  QMap<int, qint64> timestamps = m_gitIntegration->getBlameTimestamps(filePath);
  textArea->setHeatmapData(timestamps);
  textArea->setHeatmapEnabled(true);
}

void MainWindow::updateCodeLensForCurrentFile() {
  if (!m_codeLensEnabled || !m_gitIntegration ||
      !m_gitIntegration->isValidRepository())
    return;

  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty())
    return;

  QList<GitBlameLineInfo> blameLines = m_gitIntegration->getBlameInfo(filePath);
  if (blameLines.isEmpty())
    return;

  QMap<int, GitBlameLineInfo> blameMap;
  for (const auto &info : blameLines) {
    blameMap[info.lineNumber] = info;
  }

  QList<TextArea::CodeLensEntry> entries;
  QTextDocument *doc = textArea->document();
  if (!doc)
    return;

  for (int i = 0; i < doc->blockCount(); ++i) {
    QTextBlock block = doc->findBlockByNumber(i);
    QString line = block.text().trimmed();

    bool looksLikeFunction = false;
    if (line.contains('(') && !line.startsWith("//") &&
        !line.startsWith("/*") && !line.startsWith("*") &&
        !line.startsWith("#")) {

      if (line.endsWith('{') || line.endsWith(") {")) {
        looksLikeFunction = true;
      } else if (i + 1 < doc->blockCount()) {
        QString nextLine = doc->findBlockByNumber(i + 1).text().trimmed();
        if (nextLine == "{")
          looksLikeFunction = true;
      }
    }

    if (line.startsWith("class ") || line.startsWith("struct ")) {
      looksLikeFunction = true;
    }

    if (!looksLikeFunction)
      continue;

    int startLine = i + 1;
    int endLine = startLine;
    int braceDepth = 0;
    bool foundOpen = false;
    for (int j = i; j < doc->blockCount(); ++j) {
      QString bLine = doc->findBlockByNumber(j).text();
      for (QChar c : bLine) {
        if (c == '{') {
          braceDepth++;
          foundOpen = true;
        } else if (c == '}') {
          braceDepth--;
        }
      }
      if (foundOpen && braceDepth <= 0) {
        endLine = j + 1;
        break;
      }
    }

    QSet<QString> authors;
    int changeCount = 0;
    QString latestAuthor;
    QString latestDate;
    qint64 latestEpoch = 0;

    for (int ln = startLine; ln <= endLine; ++ln) {
      auto it = blameMap.find(ln);
      if (it != blameMap.end()) {
        authors.insert(it->author);
        changeCount++;

        if (latestAuthor.isEmpty() || it->relativeDate < latestDate) {
          latestAuthor = it->author;
          latestDate = it->relativeDate;
        }
      }
    }

    if (authors.isEmpty())
      continue;

    QString authorsText;
    if (authors.size() <= 3) {
      authorsText = QString("%1 author%2 (%3)")
                        .arg(authors.size())
                        .arg(authors.size() > 1 ? "s" : "")
                        .arg(QStringList(authors.values()).join(", "));
    } else {
      QStringList authorsList = authors.values();
      authorsText = QString("%1 authors (%2, %3, ...)")
                        .arg(authors.size())
                        .arg(authorsList[0], authorsList[1]);
    }

    TextArea::CodeLensEntry entry;
    entry.line = i;
    entry.text = QString("%1 | %2").arg(authorsText, latestDate);
    entry.symbolName = line.left(60);
    entries.append(entry);
  }

  textArea->setCodeLensEntries(entries);
}
