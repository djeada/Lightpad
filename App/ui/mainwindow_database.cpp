

#include "mainwindow.h"

#include "../completion/completionproviderregistry.h"
#include "../completion/providers/sqlcompletionprovider.h"
#include "../core/lightpadtabwidget.h"
#include "../core/textarea.h"
#include "../database/databasemanager.h"
#include "../theme/themeengine.h"
#include "dockutils.h"
#include "panels/databasepanel.h"
#include "panels/dbglyphs.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolButton>

namespace {

QString databaseChipStyle() {
  const ThemeColors &tc = ThemeEngine::instance().activeTheme().colors;
  return QString("QToolButton {"
                 "  color: %1;"
                 "  padding: 0 8px;"
                 "  font-size: 12px;"
                 "  border: none;"
                 "  background: transparent;"
                 "}"
                 "QToolButton:hover {"
                 "  color: %2;"
                 "}"
                 "QToolButton::menu-indicator { image: none; }")
      .arg(tc.textSecondary.name(), tc.textPrimary.name());
}

QColor chipColor(DbConnectionState state) {
  const ThemeColors &tc = ThemeEngine::instance().activeTheme().colors;
  switch (state) {
  case DbConnectionState::Connected:
    return tc.statusSuccess;
  case DbConnectionState::Connecting:
    return tc.statusWarning;
  case DbConnectionState::Failed:
    return tc.statusError;
  case DbConnectionState::Disconnected:
    break;
  }
  return tc.textMuted;
}

} // namespace

void MainWindow::setupDatabaseMenu() {
  DatabaseManager &manager = DatabaseManager::instance();

  m_databaseMenu = new QMenu(tr("Database"), this);
  m_databaseMenu->setObjectName("menuDatabase");

  m_databaseToggleAction = m_databaseMenu->addAction(tr("Database Panel"));
  m_databaseToggleAction->setCheckable(true);
  m_databaseToggleAction->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  connect(m_databaseToggleAction, &QAction::triggered, this,
          &MainWindow::toggleDatabasePanel);

  QAction *newConnection = m_databaseMenu->addAction(tr("New Connection…"));
  connect(newConnection, &QAction::triggered, this, [this]() {
    showDatabasePanel();
    databasePanel->newConnection();
  });

  m_databaseConnectionsMenu = m_databaseMenu->addMenu(tr("Connection"));
  connect(m_databaseConnectionsMenu, &QMenu::aboutToShow, this,
          &MainWindow::rebuildDatabaseConnectionsMenu);

  m_databaseMenu->addSeparator();

  QAction *runStatement =
      m_databaseMenu->addAction(tr("Run Statement or Selection"));
  runStatement->setShortcut(QKeySequence(Qt::Key_F9));
  runStatement->setShortcutContext(Qt::ApplicationShortcut);
  connect(runStatement, &QAction::triggered, this,
          [this]() { runSqlFromEditor(false, false); });

  QAction *runScript = m_databaseMenu->addAction(tr("Run Whole Script"));
  runScript->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F9));
  runScript->setShortcutContext(Qt::ApplicationShortcut);
  connect(runScript, &QAction::triggered, this,
          [this]() { runSqlFromEditor(true, false); });

  QAction *explain = m_databaseMenu->addAction(tr("Explain Statement"));
  explain->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F9));
  explain->setShortcutContext(Qt::ApplicationShortcut);
  connect(explain, &QAction::triggered, this,
          [this]() { runSqlFromEditor(false, true); });

  QAction *stop = m_databaseMenu->addAction(tr("Stop Running Query"));
  connect(stop, &QAction::triggered, this, [this]() {
    if (databasePanel) {
      databasePanel->stopRunning();
    }
  });

  m_databaseMenu->addSeparator();

  QAction *newQuery = m_databaseMenu->addAction(tr("New SQL Query"));
  newQuery->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q));
  connect(newQuery, &QAction::triggered, this, &MainWindow::newSqlQuery);

  QAction *history = m_databaseMenu->addAction(tr("Query History…"));
  connect(history, &QAction::triggered, this, [this]() {
    showDatabasePanel();
    databasePanel->showHistory();
  });

  QAction *refresh = m_databaseMenu->addAction(tr("Refresh Schema"));
  connect(refresh, &QAction::triggered, this, [this]() {
    showDatabasePanel();
    databasePanel->refreshSchema();
  });

  QAction *before = ui->menuHelp ? ui->menuHelp->menuAction() : nullptr;
  if (before) {
    menuBar()->insertMenu(before, m_databaseMenu);
  } else {
    menuBar()->addMenu(m_databaseMenu);
  }

  m_databaseStatusButton = new QToolButton(this);
  m_databaseStatusButton->setObjectName("databaseStatusButton");
  m_databaseStatusButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_databaseStatusButton->setAutoRaise(true);
  m_databaseStatusButton->setCursor(Qt::PointingHandCursor);
  m_databaseStatusButton->setPopupMode(QToolButton::InstantPopup);
  m_databaseStatusButton->setMenu(new QMenu(m_databaseStatusButton));
  connect(m_databaseStatusButton->menu(), &QMenu::aboutToShow, this, [this]() {
    QMenu *menu = m_databaseStatusButton->menu();
    menu->clear();
    DatabaseManager &m = DatabaseManager::instance();
    for (DbConnection *c : m.connections()) {
      QAction *a = menu->addAction(DbGlyphs::dot(chipColor(c->state()), 14),
                                   c->profile().name);
      a->setCheckable(true);
      a->setChecked(c->profile().id == m.activeId());
      const QString id = c->profile().id;
      connect(a, &QAction::triggered, this,
              [id]() { DatabaseManager::instance().setActive(id); });
    }
    if (!m.connections().isEmpty()) {
      menu->addSeparator();
    }
    menu->addAction(tr("New Connection…"), this, [this]() {
      showDatabasePanel();
      databasePanel->newConnection();
    });
    menu->addAction(tr("Open Database Panel"), this,
                    [this]() { showDatabasePanel(true); });
  });
  if (auto *layout =
          qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout())) {
    layout->insertWidget(qMax(0, layout->count() - 1), m_databaseStatusButton);
  } else {
    statusBar()->addPermanentWidget(m_databaseStatusButton);
  }

  auto refreshChip = [this]() { updateDatabaseStatusChip(); };
  connect(&manager, &DatabaseManager::activeChanged, this, refreshChip);
  connect(&manager, &DatabaseManager::profilesChanged, this, refreshChip);
  connect(&manager, &DatabaseManager::connectionStateChanged, this,
          refreshChip);
  connect(&manager, &DatabaseManager::schemaChanged, this, refreshChip);
  updateDatabaseStatusChip();
}

void MainWindow::rebuildDatabaseConnectionsMenu() {
  QMenu *menu = m_databaseConnectionsMenu;
  menu->clear();
  DatabaseManager &manager = DatabaseManager::instance();
  const auto conns = manager.connections();
  if (conns.isEmpty()) {
    QAction *none = menu->addAction(tr("No connections"));
    none->setEnabled(false);
    return;
  }
  for (DbConnection *c : conns) {
    QAction *a = menu->addAction(DbGlyphs::dot(chipColor(c->state()), 14),
                                 c->profile().name);
    a->setCheckable(true);
    a->setChecked(c->profile().id == manager.activeId());
    const QString id = c->profile().id;
    connect(a, &QAction::triggered, this,
            [id]() { DatabaseManager::instance().setActive(id); });
  }
  menu->addSeparator();
  DbConnection *active = manager.activeConnection();
  if (active) {
    const QString id = active->profile().id;
    if (active->isConnected() ||
        active->state() == DbConnectionState::Connecting) {
      menu->addAction(tr("Disconnect \"%1\"").arg(active->profile().name), this,
                      [this, id]() {
                        ensureDatabasePanel();
                        databasePanel->disconnectConnection(id);
                      });
    } else {
      menu->addAction(tr("Connect \"%1\"").arg(active->profile().name), this,
                      [this, id]() {
                        ensureDatabasePanel();
                        databasePanel->connectConnection(id);
                      });
    }
  }
}

void MainWindow::updateDatabaseStatusChip() {
  if (!m_databaseStatusButton) {
    return;
  }
  DbConnection *c = DatabaseManager::instance().activeConnection();
  m_databaseStatusButton->setStyleSheet(databaseChipStyle());
  if (!c) {
    m_databaseStatusButton->setIcon(DbGlyphs::icon(
        DbGlyphs::Glyph::Database,
        ThemeEngine::instance().activeTheme().colors.textMuted, 14));
    m_databaseStatusButton->setText(tr("No database"));
    m_databaseStatusButton->setToolTip(
        tr("No database connection. Click to add one."));
    return;
  }
  QString text = c->profile().name;
  if (c->profile().engine != DbEngine::Sqlite &&
      !c->currentDatabase().isEmpty()) {
    text += QStringLiteral(" · ") + c->currentDatabase();
  }
  m_databaseStatusButton->setIcon(DbGlyphs::dot(chipColor(c->state()), 14));
  m_databaseStatusButton->setText(text);
  static const QStringList states = {tr("Disconnected"), tr("Connecting…"),
                                     tr("Connected"), tr("Failed")};
  m_databaseStatusButton->setToolTip(
      QStringLiteral("%1\n%2 — %3\n%4")
          .arg(c->profile().name, c->profile().summary(),
               states.value(int(c->state())),
               tr("Queries from SQL files and the console run here.")));
}

void MainWindow::ensureDatabasePanel() {
  if (databaseDock) {
    return;
  }
  databasePanel = new DatabasePanel(&DatabaseManager::instance(), this);
  databasePanel->setObjectName("databasePanelWidget");
  databasePanel->applyTheme(getTheme());
  databasePanel->restoreState();
  connect(databasePanel, &DatabasePanel::openInEditorRequested, this,
          &MainWindow::openSqlInEditor);
  connect(
      databasePanel, &DatabasePanel::statusMessage, this,
      [this](const QString &text) { statusBar()->showMessage(text, 6000); });

  databaseDock = new QDockWidget(tr("Database"), this);
  databaseDock->setObjectName("databaseDock");
  DockUtils::configureToolPanelDock(databaseDock);
  databaseDock->setWidget(databasePanel);
  addDockWidget(Qt::BottomDockWidgetArea, databaseDock);
  tabifyBottomDock(databaseDock);
  trackDockLayoutChanges(databaseDock);
  databaseDock->hide();
  connect(databaseDock, &QDockWidget::visibilityChanged, this,
          [this](bool) { syncViewToggleActionStates(); });
}

void MainWindow::showDatabasePanel(bool focusConsole) {
  ensureDatabasePanel();
  const bool wasVisible = databaseDock->isVisible();
  databaseDock->show();
  databaseDock->raise();

  if (!wasVisible && databaseDock->height() < 320) {
    resizeDocks({databaseDock}, {qMax(380, height() * 45 / 100)}, Qt::Vertical);
  }
  if (focusConsole) {
    databasePanel->focusConsole();
  }
}

void MainWindow::toggleDatabasePanel() {
  ensureDatabasePanel();
  if (databaseDock->isVisible()) {
    databaseDock->hide();
  } else {
    showDatabasePanel(true);
  }
  syncViewToggleActionStates();
}

void MainWindow::runSqlFromEditor(bool runAll, bool explain) {
  ensureDatabasePanel();

  if (databasePanel->console()->hasFocus()) {
    if (explain) {
      databasePanel->explainStatement();
    } else if (runAll) {
      databasePanel->runScript();
    } else {
      databasePanel->runStatement();
    }
    return;
  }
  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }
  const QTextCursor cursor = textArea->textCursor();
  const QString selection =
      cursor.hasSelection()
          ? cursor.selectedText().replace(QChar::ParagraphSeparator, '\n')
          : QString();
  if (!databaseDock->isVisible()) {
    showDatabasePanel();
  }
  databasePanel->runFromEditor(textArea->toPlainText(), cursor.position(),
                               selection, runAll, explain);
}

void MainWindow::openSqlInEditor(const QString &title, const QString &sql) {
  Q_UNUSED(title)
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return;
  }
  tabWidget->addNewTab();
  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }
  textArea->setPlainText(sql);
  applyLanguageOverride(QStringLiteral("sql"));
  textArea->moveCursor(QTextCursor::End);
  textArea->setFocus();
}

void MainWindow::newSqlQuery() {
  DatabaseManager &manager = DatabaseManager::instance();
  QString header;

  if (manager.connections().size() > 1 && manager.activeConnection()) {
    header = QStringLiteral("-- connection: %1\n\n")
                 .arg(manager.activeConnection()->profile().name);
  }
  openSqlInEditor(QString(), header);
  ensureDatabasePanel();
  if (manager.connections().isEmpty()) {
    showDatabasePanel();
    databasePanel->newConnection();
  }
}

void MainWindow::registerSqlCompletion() {
  auto provider = std::make_shared<SqlCompletionProvider>();
  provider->setDocumentTextProvider([this](const QString &) -> QString {
    TextArea *textArea = getCurrentTextArea();
    return textArea ? textArea->toPlainText() : QString();
  });
  CompletionProviderRegistry::instance().registerProvider(provider);
}
