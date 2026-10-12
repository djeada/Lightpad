#include "mainwindow_internal.h"

void MainWindow::setProjectRootPath(const QString &path) {
  QString normalizedPath = QDir::cleanPath(path);
  if (!normalizedPath.isEmpty()) {
    normalizedPath = QFileInfo(normalizedPath).absoluteFilePath();
  }

  QString previousRoot = m_projectRootPath;
  if (previousRoot != normalizedPath) {
    if (!previousRoot.isEmpty()) {
      persistTreeStateToSettings();
      if (m_globalSettingsLoaded) {
        SettingsManager::instance().saveSettings();
      }
    }
  }

  m_projectRootPath = normalizedPath;

  if (previousRoot != normalizedPath) {
    m_treeExpandedPaths.clear();
    loadTreeStateFromSettings(normalizedPath);
    if (m_globalSettingsLoaded) {
      SettingsManager &globalSettings = SettingsManager::instance();
      if (globalSettings.getValue("lastProjectPath", "").toString() !=
          m_projectRootPath) {
        globalSettings.setValue("lastProjectPath", m_projectRootPath);
        globalSettings.saveSettings();
      }
    }
  }

  if (!normalizedPath.isEmpty()) {
    ensureProjectSettings(normalizedPath);
    ensureProjectWorkspaceVisible();
  }

  ensureFileTreeModel();
  if (m_fileTreeModel) {
    QString rootPath =
        normalizedPath.isEmpty() ? QDir::home().path() : normalizedPath;
    m_fileTreeModel->setRootPath(rootPath);
    m_fileTreeModel->setRootHeaderLabel(normalizedPath);
    if (m_gitIntegration) {
      m_fileTreeModel->setGitIntegration(m_gitIntegration);
    }
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (page) {
        page->setProjectRootPath(normalizedPath);
        page->setTreeViewVisible(!normalizedPath.isEmpty());
        if (!normalizedPath.isEmpty()) {
          page->setModelRootIndex(normalizedPath);
        }
        page->setTreeFilterText(m_treeFilterText);
      }
    }
  }

  if (!normalizedPath.isEmpty()) {
    updateGitIntegrationForPath(normalizedPath);
  }

  RunTemplateManager::instance().setWorkspaceFolder(normalizedPath);
  FormatTemplateManager::instance().setWorkspaceFolder(normalizedPath);
  TestConfigurationManager::instance().setWorkspaceFolder(normalizedPath);
  TestConfigurationManager::instance().loadUserConfigurations(normalizedPath);

  if (!normalizedPath.isEmpty()) {
    DebugSettings::instance().initialize(normalizedPath);
    DebugConfigurationManager::instance().setWorkspaceFolder(normalizedPath);
    DebugConfigurationManager::instance().loadFromLightpadDir();
    BreakpointManager::instance().setWorkspaceFolder(normalizedPath);
    BreakpointManager::instance().loadFromLightpadDir();
    WatchManager::instance().setWorkspaceFolder(normalizedPath);
    WatchManager::instance().loadFromLightpadDir();
  }

  refreshDebugTargetButton();
  refreshTestTargetButton();

  if (testPanel) {
    testPanel->setWorkspaceFolder(normalizedPath);
  }

  applyTreeExpandedStateToViews();
}

QString MainWindow::getProjectRootPath() const { return m_projectRootPath; }

GitIntegration *MainWindow::getGitIntegration() const {
  return m_gitIntegration;
}

GitFileSystemModel *MainWindow::getFileTreeModel() const {
  return m_fileTreeModel;
}

void MainWindow::ensureFileTreeModel() {
  if (m_fileTreeModel && m_fileTreeSelectionModel &&
      m_fileTreeSelectionModel->model() == m_fileTreeModel) {
    return;
  }

  if (!m_fileTreeModel) {
    m_fileTreeModel = new GitFileSystemModel(this);
    connect(m_fileTreeModel, &QFileSystemModel::directoryLoaded, this,
            [this](const QString &) {
              recheckOpenFilesForExternalChanges();
              applyTreeExpandedStateToViews();
            });
    QString rootPath =
        m_projectRootPath.isEmpty() ? QDir::home().path() : m_projectRootPath;
    m_fileTreeModel->setRootPath(rootPath);
    m_fileTreeModel->setRootHeaderLabel(m_projectRootPath);
    if (m_gitIntegration) {
      m_fileTreeModel->setGitIntegration(m_gitIntegration);
    }
  }

  if (!m_fileTreeSelectionModel ||
      m_fileTreeSelectionModel->model() != m_fileTreeModel) {
    m_fileTreeSelectionModel = new QItemSelectionModel(m_fileTreeModel, this);
    connect(m_fileTreeSelectionModel, &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
              trackTreeCurrentIndex(current);
            });
  }
}

QList<LightpadTreeView *> MainWindow::allTreeViews() const {
  QList<LightpadTreeView *> views;
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (!page) {
        continue;
      }
      auto *view = qobject_cast<LightpadTreeView *>(page->getTreeView());
      if (view) {
        views.append(view);
      }
    }
  }
  return views;
}

void MainWindow::registerTreeView(LightpadTreeView *treeView) {
  if (!treeView) {
    return;
  }
  ensureFileTreeModel();
  if (!m_fileTreeModel) {
    return;
  }

  if (treeView->model() != m_fileTreeModel) {
    treeView->setModel(m_fileTreeModel);
  }
  if (m_fileTreeSelectionModel &&
      treeView->selectionModel() != m_fileTreeSelectionModel) {
    QItemSelectionModel *oldSelectionModel = treeView->selectionModel();
    treeView->setSelectionModel(m_fileTreeSelectionModel);
    if (oldSelectionModel && oldSelectionModel != m_fileTreeSelectionModel &&
        oldSelectionModel->parent() == treeView) {
      oldSelectionModel->deleteLater();
    }
  }

  QObject::disconnect(treeView, &QTreeView::expanded, this, nullptr);
  QObject::disconnect(treeView, &QTreeView::collapsed, this, nullptr);
  QObject::disconnect(treeView->verticalScrollBar(), &QScrollBar::valueChanged,
                      this, nullptr);
  connect(treeView, &QTreeView::expanded, this,
          [this, treeView](const QModelIndex &index) {
            trackTreeExpandedState(index, true);
            for (LightpadTreeView *view : allTreeViews()) {
              if (!view || view == treeView || view->isExpanded(index)) {
                continue;
              }
              expandIndexInView(view, index);
            }
          });
  connect(treeView, &QTreeView::collapsed, this,
          [this, treeView](const QModelIndex &index) {
            trackTreeExpandedState(index, false);
            for (LightpadTreeView *view : allTreeViews()) {
              if (!view || view == treeView || !view->isExpanded(index)) {
                continue;
              }
              view->collapse(index);
            }
          });
  connect(treeView->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int value) {
            if (m_treeScrollSyncing) {
              return;
            }
            m_treeScrollValue = value;
            m_treeScrollValueInitialized = true;
            m_treeScrollSyncing = true;
            for (LightpadTreeView *view : allTreeViews()) {
              if (!view) {
                continue;
              }
              QScrollBar *scrollBar = view->verticalScrollBar();
              if (!scrollBar || scrollBar->value() == value) {
                continue;
              }
              scrollBar->setValue(value);
            }
            m_treeScrollSyncing = false;
          });

  applyTreeStateToView(treeView);
  applyTreeCurrentIndexToView(treeView);
  if (m_treeScrollValueInitialized) {
    treeView->verticalScrollBar()->setValue(m_treeScrollValue);
  }
  if (auto *page = qobject_cast<LightpadPage *>(treeView->parentWidget())) {
    page->setTreeFilterText(m_treeFilterText);
  }

  QObject::disconnect(treeView, &LightpadTreeView::runTestsRequested, this,
                      nullptr);
  QObject::disconnect(treeView, &LightpadTreeView::runFileRequested, this,
                      nullptr);
  QObject::disconnect(treeView, &LightpadTreeView::debugFileRequested, this,
                      nullptr);
  QObject::disconnect(treeView, &LightpadTreeView::toggleTestMarkerRequested,
                      this, nullptr);

  connect(treeView, &LightpadTreeView::runTestsRequested, this,
          &MainWindow::runTestsForPath);

  connect(treeView, &LightpadTreeView::runFileRequested, this,
          &MainWindow::runFileByPath);

  connect(treeView, &LightpadTreeView::debugFileRequested, this,
          &MainWindow::debugFileByPath);

  connect(treeView, &LightpadTreeView::toggleTestMarkerRequested, this,
          [](const QString &path, bool markAsTest) {
            TestFileClassifier::instance().setTestOverride(path, markAsTest);
          });

  QObject::disconnect(treeView, &LightpadTreeView::revealInFileManagerRequested,
                      this, nullptr);
  QObject::disconnect(treeView, &LightpadTreeView::openInTerminalRequested,
                      this, nullptr);
  connect(treeView, &LightpadTreeView::revealInFileManagerRequested, this,
          &MainWindow::revealPathInFileManager);
  connect(treeView, &LightpadTreeView::openInTerminalRequested, this,
          &MainWindow::openTerminalAtPath);
}

void MainWindow::setTreeFilterText(const QString &text) {
  m_treeFilterText = text;

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (page) {
        page->setTreeFilterText(m_treeFilterText);
      }
    }
  }
}

QString MainWindow::getTreeFilterText() const { return m_treeFilterText; }

void MainWindow::trackTreeExpandedState(const QModelIndex &index,
                                        bool expanded) {
  if (!m_fileTreeModel || !index.isValid()) {
    return;
  }

  QString path = QDir::cleanPath(m_fileTreeModel->filePath(index));
  if (path.isEmpty()) {
    return;
  }

  if (expanded) {
    QString rootPath = QDir::cleanPath(m_projectRootPath);
    QString currentPath = path;
    while (!currentPath.isEmpty()) {
      m_treeExpandedPaths.insert(currentPath);
      if (!rootPath.isEmpty() && currentPath == rootPath) {
        break;
      }
      QString parentPath = QFileInfo(currentPath).absolutePath();
      if (parentPath == currentPath) {
        break;
      }
      currentPath = parentPath;
    }
  } else {
    QString prefix = path + "/";
    for (auto it = m_treeExpandedPaths.begin();
         it != m_treeExpandedPaths.end();) {
      const QString &existing = *it;
      if (existing == path || existing.startsWith(prefix)) {
        it = m_treeExpandedPaths.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void MainWindow::trackTreeCurrentIndex(const QModelIndex &index) {
  if (!m_fileTreeModel || !index.isValid()) {
    return;
  }

  QString path = QDir::cleanPath(m_fileTreeModel->filePath(index));
  if (path.isEmpty()) {
    return;
  }

  m_treeCurrentPath = path;
}

void MainWindow::applyTreeStateToView(QTreeView *treeView) {
  if (!treeView || !m_fileTreeModel) {
    return;
  }

  QList<QModelIndex> expandedIndexes;
  std::function<void(const QModelIndex &)> collectExpanded =
      [&](const QModelIndex &parent) {
        int rowCount = m_fileTreeModel->rowCount(parent);
        for (int row = 0; row < rowCount; ++row) {
          QModelIndex child = m_fileTreeModel->index(row, 0, parent);
          if (!child.isValid()) {
            continue;
          }
          if (treeView->isExpanded(child)) {
            expandedIndexes.append(child);
            collectExpanded(child);
          }
        }
      };

  QModelIndex root = treeView->rootIndex();
  collectExpanded(root);

  std::sort(expandedIndexes.begin(), expandedIndexes.end(),
            [this](const QModelIndex &a, const QModelIndex &b) {
              QString pathA = QDir::cleanPath(m_fileTreeModel->filePath(a));
              QString pathB = QDir::cleanPath(m_fileTreeModel->filePath(b));
              return pathA.count('/') > pathB.count('/');
            });

  for (const QModelIndex &idx : expandedIndexes) {
    QString path = QDir::cleanPath(m_fileTreeModel->filePath(idx));
    if (!m_treeExpandedPaths.contains(path)) {
      treeView->collapse(idx);
    }
  }

  QString normalizedRoot = QDir::cleanPath(m_projectRootPath);
  for (const QString &path : m_treeExpandedPaths) {
    if (!normalizedRoot.isEmpty() && !path.startsWith(normalizedRoot)) {
      continue;
    }
    QModelIndex idx = m_fileTreeModel->index(path);
    if (idx.isValid()) {
      expandIndexInView(treeView, idx);
    }
  }
}

void MainWindow::applyTreeCurrentIndexToView(QTreeView *treeView) {
  if (!treeView || !m_fileTreeModel || m_treeCurrentPath.isEmpty()) {
    return;
  }

  QModelIndex idx = m_fileTreeModel->index(m_treeCurrentPath);
  if (!idx.isValid()) {
    return;
  }

  m_treeSelectionSyncing = true;
  if (treeView->currentIndex() != idx) {
    treeView->setCurrentIndex(idx);
  }
  m_treeSelectionSyncing = false;
}

void MainWindow::setTreeCurrentPath(const QString &path) {
  if (!m_fileTreeModel) {
    return;
  }

  QString normalizedPath = QDir::cleanPath(path);
  if (normalizedPath.isEmpty()) {
    return;
  }

  QModelIndex idx = m_fileTreeModel->index(normalizedPath);
  if (!idx.isValid()) {
    return;
  }

  trackTreeCurrentIndex(idx);
}

void MainWindow::applyTreeExpandedStateToViews() {
  if (!m_fileTreeModel) {
    return;
  }

  QString rootPath = m_projectRootPath.isEmpty() ? m_fileTreeModel->rootPath()
                                                 : m_projectRootPath;
  QString normalizedRoot = QDir::cleanPath(rootPath);

  for (LightpadTreeView *view : allTreeViews()) {
    if (!view) {
      continue;
    }
    for (const QString &path : m_treeExpandedPaths) {
      QString normalizedPath = QDir::cleanPath(path);
      if (!normalizedRoot.isEmpty() &&
          !normalizedPath.startsWith(normalizedRoot)) {
        continue;
      }

      QModelIndex idx = m_fileTreeModel->index(normalizedPath);
      if (idx.isValid()) {
        expandIndexInView(view, idx);
      }
    }
    applyTreeCurrentIndexToView(view);
  }
}

void MainWindow::expandIndexInView(QTreeView *treeView,
                                   const QModelIndex &index) {
  if (!treeView || !index.isValid()) {
    return;
  }

  QList<QModelIndex> chain;
  QModelIndex current = index;
  while (current.isValid()) {
    chain.prepend(current);
    if (current == treeView->rootIndex()) {
      break;
    }
    current = current.parent();
  }

  for (const QModelIndex &item : chain) {
    if (m_fileTreeModel && m_fileTreeModel->canFetchMore(item)) {
      m_fileTreeModel->fetchMore(item);
    }
    treeView->expand(item);
  }
}

void MainWindow::loadTreeStateFromSettings(const QString &rootPath) {
  m_treeExpandedPaths.clear();
  m_treeCurrentPath.clear();
  m_treeScrollValue = 0;
  m_treeScrollValueInitialized = false;
  m_treeFilterText.clear();

  if (rootPath.isEmpty()) {
    return;
  }

  SettingsManager &globalSettings = SettingsManager::instance();
  QJsonObject treeStates =
      globalSettings.getSettingsObject().value("treeStateByRoot").toObject();
  QString normalizedRoot = QDir::cleanPath(rootPath);
  QJsonObject state = treeStates.value(normalizedRoot).toObject();
  if (state.isEmpty() && normalizedRoot != rootPath) {
    state = treeStates.value(rootPath).toObject();
  }
  QJsonArray expanded = state.value("expanded").toArray();

  for (const QJsonValue &value : expanded) {
    QString path = QDir::cleanPath(value.toString());
    if (!path.isEmpty() &&
        (normalizedRoot.isEmpty() || path.startsWith(normalizedRoot))) {
      m_treeExpandedPaths.insert(path);
    }
  }

  QString currentPath = QDir::cleanPath(state.value("currentPath").toString());
  if (!currentPath.isEmpty() &&
      (normalizedRoot.isEmpty() || currentPath.startsWith(normalizedRoot))) {
    m_treeCurrentPath = currentPath;
  }

  const QJsonValue scrollValue = state.value("scrollValue");
  if (scrollValue.isDouble()) {
    m_treeScrollValue = scrollValue.toInt();
    m_treeScrollValueInitialized = true;
  }

  m_treeFilterText = state.value("filterText").toString();
}

void MainWindow::persistTreeStateToSettings() {
  if (m_projectRootPath.isEmpty()) {
    return;
  }

  SettingsManager &globalSettings = SettingsManager::instance();
  QJsonObject treeStates =
      globalSettings.getSettingsObject().value("treeStateByRoot").toObject();
  QJsonObject state;

  QJsonArray expanded;
  for (const QString &path : m_treeExpandedPaths) {
    expanded.append(QDir::cleanPath(path));
  }
  state["expanded"] = expanded;

  QString normalizedRoot = QDir::cleanPath(m_projectRootPath);
  if (!m_treeCurrentPath.isEmpty() &&
      isPathWithinRoot(m_treeCurrentPath, normalizedRoot)) {
    state["currentPath"] = QDir::cleanPath(m_treeCurrentPath);
  }
  if (m_treeScrollValueInitialized) {
    state["scrollValue"] = m_treeScrollValue;
  }
  if (!m_treeFilterText.isEmpty()) {
    state["filterText"] = m_treeFilterText;
  }
  treeStates[normalizedRoot.isEmpty() ? m_projectRootPath : normalizedRoot] =
      state;
  globalSettings.setValue("treeStateByRoot", treeStates);
}

