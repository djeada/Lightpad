#include "databasepanel.h"

#include "../../database/dbcatalog.h"
#include "../../database/resultexporter.h"
#include "../../database/sqlstatementsplitter.h"
#include "../../settings/settingsmanager.h"
#include "../dialogs/connectiondialog.h"
#include "../dialogs/queryhistorydialog.h"
#include "../dialogs/themedmessagebox.h"
#include "../uistylehelper.h"
#include "dbglyphs.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QString formatDuration(qint64 ms) {
  if (ms < 1000) {
    return QStringLiteral("%1 ms").arg(ms);
  }
  if (ms < 60000) {
    return QStringLiteral("%1 s").arg(ms / 1000.0, 0, 'f', 2);
  }
  return QStringLiteral("%1 min %2 s").arg(ms / 60000).arg((ms % 60000) / 1000);
}

QString oneLine(const QString &sql, int max = 160) {
  QString t = sql.simplified();
  if (t.size() > max) {
    t = t.left(max - 1) + QChar(0x2026);
  }
  return t;
}

QString itemKey(const QTreeWidgetItem *item) {
  return item->data(0, Qt::UserRole + 5).toString();
}

bool isDefaultSchema(const QString &schema) {
  return schema == QLatin1String("public") || schema == QLatin1String("dbo") ||
         schema == QLatin1String("main") || schema.isEmpty();
}

constexpr int kPreviewRows = 200;
constexpr int kTypeRole = Qt::UserRole + 6;

class SchemaTreeDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  QColor muted;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    const QString note = index.data(kTypeRole).toString();
    if (note.isEmpty()) {
      QStyledItemDelegate::paint(painter, option, index);
      return;
    }
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QFontMetrics fm(opt.font);

    const int maxNote = qMax(40, option.rect.width() * 45 / 100);
    const QString shown = fm.elidedText(note, Qt::ElideRight, maxNote);
    const int noteWidth = fm.horizontalAdvance(shown) + 10;
    QStyleOptionViewItem nameOpt = opt;
    nameOpt.rect.adjust(0, 0, -noteWidth, 0);
    QStyledItemDelegate::paint(painter, nameOpt, index);
    painter->save();
    painter->setPen(opt.state & QStyle::State_Selected
                        ? opt.palette.color(QPalette::HighlightedText)
                        : muted);
    painter->setFont(opt.font);
    painter->drawText(QRect(opt.rect.right() - noteWidth, opt.rect.top(),
                            noteWidth - 6, opt.rect.height()),
                      Qt::AlignRight | Qt::AlignVCenter, shown);
    painter->restore();
  }
};

} // namespace

DatabasePanel::DatabasePanel(DatabaseManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager) {
  setObjectName("databasePanel");
  buildUi();

  connect(m_manager, &DatabaseManager::profilesChanged, this,
          &DatabasePanel::onProfilesChanged);
  connect(m_manager, &DatabaseManager::activeChanged, this,
          &DatabasePanel::onActiveChanged);
  connect(m_manager, &DatabaseManager::connectionStateChanged, this,
          &DatabasePanel::onConnectionState);
  connect(m_manager, &DatabaseManager::schemaChanged, this,
          &DatabasePanel::onSchemaChanged);

  m_runningTimer.setInterval(100);
  connect(&m_runningTimer, &QTimer::timeout, this,
          &DatabasePanel::updateRunningLabel);

  for (DbConnection *c : m_manager->connections()) {
    hookConnection(c);
  }
  refreshCombos();
  rebuildTree();
  refreshToolbar();
  onActiveChanged();
}

DatabasePanel::~DatabasePanel() = default;

void DatabasePanel::buildUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  buildToolbar();
  root->addWidget(m_toolbar);

  m_progress = new QProgressBar(this);
  m_progress->setObjectName("dbProgress");
  m_progress->setTextVisible(false);
  m_progress->setRange(0, 0);
  m_progress->setFixedHeight(2);
  m_progress->hide();
  root->addWidget(m_progress);

  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->setChildrenCollapsible(false);

  auto *left = new QWidget(m_mainSplitter);
  left->setObjectName("dbBrowser");
  auto *leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(0);
  m_filter = new QLineEdit(left);
  m_filter->setObjectName("dbFilter");
  m_filter->setPlaceholderText(tr("Filter tables and columns…"));
  m_filter->setClearButtonEnabled(true);
  leftLayout->addWidget(m_filter);

  m_tree = new QTreeWidget(left);
  m_tree->setObjectName("dbTree");
  m_tree->setColumnCount(1);
  m_tree->setHeaderHidden(true);
  m_tree->setItemDelegate(new SchemaTreeDelegate(m_tree));
  m_tree->setTextElideMode(Qt::ElideRight);
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tree->setUniformRowHeights(true);
  m_tree->setIndentation(14);
  m_tree->setAnimated(false);
  m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tree->setExpandsOnDoubleClick(false);
  leftLayout->addWidget(m_tree, 1);
  m_mainSplitter->addWidget(left);

  m_rightSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
  m_rightSplitter->setChildrenCollapsible(false);

  auto *consoleHost = new QWidget(m_rightSplitter);
  auto *consoleLayout = new QVBoxLayout(consoleHost);
  consoleLayout->setContentsMargins(0, 0, 0, 0);
  consoleLayout->setSpacing(0);
  m_colorStrip = new QWidget(consoleHost);
  m_colorStrip->setFixedHeight(3);
  consoleLayout->addWidget(m_colorStrip);
  m_console = new SqlConsoleEdit(consoleHost);
  consoleLayout->addWidget(m_console, 1);
  m_console->setSchemaProvider([this]() -> const DbSchema * {
    DbConnection *c = activeConnection();
    return c && !c->schema().isEmpty() ? &c->schema() : nullptr;
  });
  m_rightSplitter->addWidget(consoleHost);

  m_outputTabs = new QTabWidget(m_rightSplitter);
  m_outputTabs->setObjectName("dbOutputTabs");
  m_outputTabs->setDocumentMode(true);

  m_resultStack = new QStackedWidget(m_outputTabs);
  m_resultsEmpty =
      new QLabel(tr("Run a query to see its results here.\n\nCtrl+Enter runs "
                    "the statement under the caret, "
                    "Ctrl+Shift+Enter runs everything.\nIn any .sql file, F9 "
                    "runs the statement and Shift+F9 the whole script."),
                 m_resultStack);
  m_resultsEmpty->setAlignment(Qt::AlignCenter);
  m_resultsEmpty->setObjectName("dbResultsEmpty");
  m_resultTabs = new QTabWidget(m_resultStack);
  m_resultTabs->setObjectName("dbResultTabs");
  m_resultTabs->setTabsClosable(true);
  m_resultTabs->setDocumentMode(true);
  m_resultTabs->setMovable(true);
  m_resultStack->addWidget(m_resultsEmpty);
  m_resultStack->addWidget(m_resultTabs);
  m_outputTabs->addTab(m_resultStack, tr("Results"));

  m_messages = new QPlainTextEdit(m_outputTabs);
  m_messages->setObjectName("dbMessages");
  m_messages->setReadOnly(true);
  m_messages->setMaximumBlockCount(2000);
  m_messages->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  m_messages->setContextMenuPolicy(Qt::CustomContextMenu);
  m_outputTabs->addTab(m_messages, tr("Messages"));

  m_insights = new DbInsightsView(m_outputTabs);
  m_outputTabs->addTab(m_insights, tr("Insights"));
  m_rightSplitter->addWidget(m_outputTabs);

  m_rightSplitter->setStretchFactor(0, 1);
  m_rightSplitter->setStretchFactor(1, 3);
  m_rightSplitter->setSizes({130, 360});
  m_mainSplitter->addWidget(m_rightSplitter);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setSizes({280, 800});
  root->addWidget(m_mainSplitter, 1);

  auto *statusBar = new QWidget(this);
  statusBar->setObjectName("dbStatusBar");
  auto *sl = new QHBoxLayout(statusBar);
  sl->setContentsMargins(10, 3, 10, 3);
  m_status = new QLabel(tr("Ready"), statusBar);
  m_status->setObjectName("dbStatus");
  m_runningLabel = new QLabel(statusBar);
  m_runningLabel->setObjectName("dbRunning");
  sl->addWidget(m_status, 1);
  sl->addWidget(m_runningLabel);
  root->addWidget(statusBar);

  connect(m_filter, &QLineEdit::textChanged, this,
          &DatabasePanel::onFilterChanged);
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          &DatabasePanel::onTreeDoubleClicked);
  connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
          &DatabasePanel::onTreeContextMenu);
  connect(m_tree, &QTreeWidget::itemExpanded, this,
          &DatabasePanel::onTreeItemExpanded);
  connect(m_tree, &QTreeWidget::itemCollapsed, this,
          &DatabasePanel::onTreeItemCollapsed);
  connect(m_console, &SqlConsoleEdit::runStatementRequested, this,
          &DatabasePanel::runStatement);
  connect(m_console, &SqlConsoleEdit::runScriptRequested, this,
          &DatabasePanel::runScript);
  connect(m_console, &SqlConsoleEdit::historyStep, this, [this](int direction) {
    DbConnection *c = activeConnection();
    const auto entries = m_manager->history().search(
        QString(), c ? c->profile().name : QString());
    if (entries.isEmpty()) {
      return;
    }
    if (m_historyCursor == -1) {
      m_historyDraft = m_console->toPlainText();
    }
    m_historyCursor =
        qBound(-1, m_historyCursor - direction, int(entries.size()) - 1);
    m_console->setPlainText(m_historyCursor < 0 ? m_historyDraft
                                                : entries[m_historyCursor].sql);
    m_console->moveCursor(QTextCursor::End);
  });
  connect(m_console, &QPlainTextEdit::textChanged, this, [this]() {
    if (m_console->hasFocus() && m_historyCursor != -1 &&
        m_console->toPlainText().isEmpty()) {
      m_historyCursor = -1;
    }
  });
  connect(m_resultTabs, &QTabWidget::tabCloseRequested, this,
          [this](int index) {
            QWidget *w = m_resultTabs->widget(index);
            m_resultTabs->removeTab(index);
            delete w;
            if (m_resultTabs->count() == 0) {
              m_resultStack->setCurrentWidget(m_resultsEmpty);
            }
            updateTabTitles();
          });
  connect(m_messages, &QPlainTextEdit::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QMenu *menu = m_messages->createStandardContextMenu();
            menu->addSeparator();
            menu->addAction(tr("Clear messages"), this, [this]() {
              m_messages->clear();
              m_messageCount = 0;
              m_errorCount = 0;
              updateTabTitles();
            });
            menu->exec(m_messages->viewport()->mapToGlobal(pos));
            delete menu;
          });
}

void DatabasePanel::buildToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("dbToolbar");
  m_toolbar->setMovable(false);
  m_toolbar->setIconSize(QSize(16, 16));
  m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  m_connectionCombo = new QComboBox(m_toolbar);
  m_connectionCombo->setObjectName("dbConnectionCombo");
  m_connectionCombo->setMinimumWidth(190);
  m_connectionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  m_connectionCombo->setToolTip(tr("Connection that queries run against"));
  m_toolbar->addWidget(m_connectionCombo);

  m_connectAction = m_toolbar->addAction(tr("Connect"));
  m_connectAction->setToolTip(
      tr("Connect / disconnect the selected connection"));
  connect(m_connectAction, &QAction::triggered, this, [this]() {
    DbConnection *c = activeConnection();
    if (!c) {
      newConnection();
    } else if (c->isConnected() ||
               c->state() == DbConnectionState::Connecting) {
      disconnectConnection(c->profile().id);
    } else {
      connectConnection(c->profile().id);
    }
  });

  m_databaseCombo = new QComboBox(m_toolbar);
  m_databaseCombo->setObjectName("dbDatabaseCombo");
  m_databaseCombo->setMinimumWidth(130);
  m_databaseCombo->setToolTip(tr("Database the session uses"));
  m_databaseComboAction = m_toolbar->addWidget(m_databaseCombo);
  m_toolbar->addSeparator();

  m_runAction = m_toolbar->addAction(tr("Run"));
  m_runAction->setToolTip(
      tr("Run the statement under the caret or the selection (Ctrl+Enter)"));
  m_runAllAction = m_toolbar->addAction(tr("Run all"));
  m_runAllAction->setToolTip(tr("Run the whole script (Ctrl+Shift+Enter)"));
  m_stopAction = m_toolbar->addAction(tr("Stop"));
  m_stopAction->setToolTip(tr("Cancel the running query"));
  m_explainAction = m_toolbar->addAction(tr("Explain"));
  m_explainAction->setToolTip(tr("Show the execution plan of the statement"));
  m_toolbar->addSeparator();
  m_refreshAction = m_toolbar->addAction(tr("Refresh"));
  m_refreshAction->setToolTip(tr("Reload the schema of the active connection"));
  m_historyAction = m_toolbar->addAction(tr("History"));
  m_historyAction->setToolTip(tr("Browse previously run statements"));

  auto *spacer = new QWidget(m_toolbar);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_toolbar->addWidget(spacer);

  m_addAction = m_toolbar->addAction(tr("New connection"));
  m_addAction->setToolTip(tr("Add a database connection"));
  m_manageAction = m_toolbar->addAction(tr("Manage"));
  m_manageAction->setToolTip(
      tr("Edit, duplicate or remove the selected connection"));

  connect(m_runAction, &QAction::triggered, this, &DatabasePanel::runStatement);
  connect(m_runAllAction, &QAction::triggered, this, &DatabasePanel::runScript);
  connect(m_stopAction, &QAction::triggered, this, &DatabasePanel::stopRunning);
  connect(m_explainAction, &QAction::triggered, this,
          &DatabasePanel::explainStatement);
  connect(m_refreshAction, &QAction::triggered, this,
          [this]() { refreshSchema(); });
  connect(m_historyAction, &QAction::triggered, this,
          &DatabasePanel::showHistory);
  connect(m_addAction, &QAction::triggered, this,
          &DatabasePanel::newConnection);
  connect(m_manageAction, &QAction::triggered, this, [this]() {
    DbConnection *c = activeConnection();
    QMenu menu(this);
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));
    QAction *edit = menu.addAction(tr("Edit connection…"));
    QAction *dup = menu.addAction(tr("Duplicate"));
    QAction *remove = menu.addAction(tr("Remove…"));
    edit->setEnabled(c);
    dup->setEnabled(c);
    remove->setEnabled(c);
    QWidget *w = m_toolbar->widgetForAction(m_manageAction);
    QAction *chosen =
        menu.exec(w ? w->mapToGlobal(QPoint(0, w->height())) : QCursor::pos());
    if (!c || !chosen) {
      return;
    }
    if (chosen == edit) {
      editConnection(c->profile().id);
    } else if (chosen == dup) {
      duplicateConnection(c->profile().id);
    } else if (chosen == remove) {
      removeConnection(c->profile().id);
    }
  });
  connect(m_connectionCombo, QOverload<int>::of(&QComboBox::activated), this,
          &DatabasePanel::onConnectionComboActivated);
  connect(m_databaseCombo, QOverload<int>::of(&QComboBox::activated), this,
          &DatabasePanel::onDatabaseComboActivated);
}

QColor DatabasePanel::stateColor(DbConnectionState state) const {
  switch (state) {
  case DbConnectionState::Connected:
    return m_theme.successColor.isValid() ? m_theme.successColor
                                          : QColor("#3fb950");
  case DbConnectionState::Connecting:
    return m_theme.warningColor.isValid() ? m_theme.warningColor
                                          : QColor("#d29922");
  case DbConnectionState::Failed:
    return m_theme.errorColor.isValid() ? m_theme.errorColor
                                        : QColor("#f85149");
  case DbConnectionState::Disconnected:
    break;
  }
  return UIStyleHelper::mutedTextColor(m_theme);
}

QIcon DatabasePanel::stateIcon(DbConnectionState state, const QString &) const {
  return DbGlyphs::dot(stateColor(state), 14);
}

void DatabasePanel::applyTheme(const Theme &theme) {
  m_theme = theme;
  const QColor text = UIStyleHelper::readableText(theme, theme.backgroundColor,
                                                  theme.foregroundColor);
  const QColor muted = UIStyleHelper::mutedTextColor(theme);
  const QColor selection = theme.accentSoftColor.isValid()
                               ? theme.accentSoftColor
                               : theme.highlightColor;

  using G = DbGlyphs::Glyph;
  m_runAction->setIcon(DbGlyphs::icon(
      G::Run, theme.successColor.isValid() ? theme.successColor : text));
  m_runAllAction->setIcon(DbGlyphs::icon(
      G::RunAll, theme.successColor.isValid() ? theme.successColor : text));
  m_stopAction->setIcon(DbGlyphs::icon(
      G::Stop, theme.errorColor.isValid() ? theme.errorColor : text));
  m_explainAction->setIcon(DbGlyphs::icon(G::Explain, text));
  m_refreshAction->setIcon(DbGlyphs::icon(G::Refresh, text));
  m_historyAction->setIcon(DbGlyphs::icon(G::History, text));
  m_addAction->setIcon(DbGlyphs::icon(G::Add, text));
  m_manageAction->setIcon(DbGlyphs::icon(G::Gear, text));

  setStyleSheet(
      QString(
          "QWidget#databasePanel { background: %1; }"
          "QToolBar#dbToolbar { background: %2; border: 0; border-bottom: 1px "
          "solid %3;"
          "  padding: 3px 6px; spacing: 4px; }"
          "QToolBar#dbToolbar QToolButton { background: transparent; border: "
          "1px solid transparent;"
          "  border-radius: 4px; padding: 4px; }"
          "QToolBar#dbToolbar QToolButton:hover { background: %4; "
          "border-color: %3; }"
          "QToolBar#dbToolbar QToolButton:disabled { opacity: 0.4; }"
          "QToolBar#dbToolbar QToolButton:pressed { background: %5; }"
          "QComboBox#dbConnectionCombo, QComboBox#dbDatabaseCombo { "
          "background: %1; color: %6;"
          "  border: 1px solid %3; border-radius: 4px; padding: 3px 8px; }"
          "QComboBox#dbConnectionCombo:hover, QComboBox#dbDatabaseCombo:hover "
          "{ border-color: %7; }"
          "QComboBox QAbstractItemView { background: %2; color: %6; border: "
          "1px solid %3;"
          "  selection-background-color: %8; selection-color: %6; outline: "
          "none; }"
          "QWidget#dbBrowser { background: %1; border-right: 1px solid %3; }"
          "QLineEdit#dbFilter { background: %1; color: %6; border: none; "
          "border-bottom: 1px solid %3;"
          "  padding: 6px 10px; }"
          "QLineEdit#dbFilter:focus { border-bottom: 1px solid %7; }"
          "QTreeWidget#dbTree { background: %1; color: %6; border: none; "
          "outline: none; }"
          "QTreeWidget#dbTree::item { height: 24px; }"
          "QTreeWidget#dbTree::item:hover { background: %4; }"
          "QTreeWidget#dbTree::item:selected { background: %8; color: %6; }"
          "QSplitter::handle { background: %3; }"
          "QSplitter::handle:horizontal { width: 1px; }"
          "QSplitter::handle:vertical { height: 1px; }"
          "QTabWidget#dbOutputTabs::pane, QTabWidget#dbResultTabs::pane { "
          "border: none;"
          "  border-top: 1px solid %3; background: %1; }"
          "QTabBar { background: %2; }"
          "QTabBar::tab { background: transparent; color: %9; padding: 6px "
          "14px; border: none;"
          "  border-bottom: 2px solid transparent; }"
          "QTabBar::tab:selected { color: %6; border-bottom: 2px solid %7; }"
          "QTabBar::tab:hover:!selected { color: %6; background: %4; }"
          "QPlainTextEdit#dbMessages { background: %1; color: %6; border: "
          "none; padding: 6px 10px; }"
          "QLabel#dbResultsEmpty { color: %9; background: %1; }"
          "QWidget#dbStatusBar { background: %2; border-top: 1px solid %3; }"
          "QLabel#dbStatus, QLabel#dbRunning { color: %9; }"
          "QProgressBar#dbProgress { background: %2; border: none; }"
          "QProgressBar#dbProgress::chunk { background: %7; }"
          "QScrollBar:vertical { background: transparent; width: 10px; }"
          "QScrollBar::handle:vertical { background: %3; border-radius: 4px; "
          "min-height: 24px; }"
          "QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }"
          "QScrollBar:horizontal { background: transparent; height: 10px; }"
          "QScrollBar::handle:horizontal { background: %3; border-radius: 4px; "
          "min-width: 24px; }")
          .arg(theme.backgroundColor.name(), theme.surfaceColor.name(),
               theme.borderColor.name(), theme.hoverColor.name(),
               theme.pressedColor.isValid() ? theme.pressedColor.name()
                                            : theme.hoverColor.name(),
               text.name(), theme.accentColor.name(), selection.name(),
               muted.name()));

  if (auto *d = dynamic_cast<SchemaTreeDelegate *>(m_tree->itemDelegate())) {
    d->muted = muted;
  }
  m_console->applyTheme(theme);
  m_insights->applyTheme(theme);
  for (int i = 0; i < m_resultTabs->count(); ++i) {
    if (auto *v = qobject_cast<DbResultView *>(m_resultTabs->widget(i))) {
      v->applyTheme(theme);
    }
  }
  if (m_historyDialog) {
    m_historyDialog->applyTheme(theme);
  }
  refreshCombos();
  rebuildTree();
  refreshToolbar();
  onActiveChanged();
}

void DatabasePanel::saveState() const {
  SettingsManager &s = SettingsManager::instance();
  s.setValue("databaseMainSplitter",
             QString::fromLatin1(m_mainSplitter->saveState().toBase64()));
  s.setValue("databaseRightSplitter",
             QString::fromLatin1(m_rightSplitter->saveState().toBase64()));
  s.setValue("databaseConsoleText", m_console->toPlainText());
  s.setValue("databaseExpandedNodes",
             QStringList(m_expanded.begin(), m_expanded.end()));
  m_manager->flush();
}

void DatabasePanel::restoreState() {
  SettingsManager &s = SettingsManager::instance();
  const QByteArray main = QByteArray::fromBase64(
      s.getValue("databaseMainSplitter", QString()).toString().toLatin1());
  if (!main.isEmpty()) {
    m_mainSplitter->restoreState(main);
  }
  const QByteArray right = QByteArray::fromBase64(
      s.getValue("databaseRightSplitter", QString()).toString().toLatin1());
  if (!right.isEmpty()) {
    m_rightSplitter->restoreState(right);
  }
  const QString text = s.getValue("databaseConsoleText", QString()).toString();
  if (!text.isEmpty() && m_console->toPlainText().isEmpty()) {
    m_console->setPlainText(text);
  }
  const QStringList expanded =
      s.getValue("databaseExpandedNodes", QStringList()).toStringList();
  m_expanded = QSet<QString>(expanded.begin(), expanded.end());
  rebuildTree();
}

DbConnection *DatabasePanel::activeConnection() const {
  return m_manager->activeConnection();
}

void DatabasePanel::hookConnection(DbConnection *conn) {
  if (!conn || m_hooked.contains(conn->profile().id)) {
    return;
  }
  m_hooked.insert(conn->profile().id);
  connect(conn, &DbConnection::requestStarted, this, [this, conn](quint64 id) {
    if (m_pendingRun.conn == conn) {
      m_pendingRun.id = id;
      m_runs.insert(id, m_pendingRun);
      m_activeRunByConnection.insert(conn->profile().id, id);
      m_pendingRun = Run();
    }
  });
  connect(conn, &DbConnection::statementFinished, this,
          [this, conn](quint64 id, int index, const DbStatementResult &r) {
            onStatementFinished(conn, id, index, r);
          });
  connect(conn, &DbConnection::requestFinished, this,
          [this, conn](quint64 id, bool cancelled) {
            onRequestFinished(conn, id, cancelled);
          });
  connect(conn, &DbConnection::databasesLoaded, this, [this, conn]() {
    if (conn == activeConnection()) {
      refreshCombos();
    }
  });
  connect(conn, &DbConnection::databaseChanged, this, [this, conn]() {
    if (conn == activeConnection()) {
      refreshCombos();
    }
  });
}

void DatabasePanel::onProfilesChanged() {
  for (DbConnection *c : m_manager->connections()) {
    hookConnection(c);
  }
  refreshCombos();
  rebuildTree();
  refreshToolbar();
}

void DatabasePanel::onActiveChanged() {
  DbConnection *c = activeConnection();
  m_console->setEngine(c ? c->profile().engine : DbEngine::PostgreSql);
  m_historyCursor = -1;
  const QString color = c ? c->profile().color : QString();
  m_colorStrip->setStyleSheet(
      QStringLiteral("background: %1;")
          .arg(color.isEmpty() ? QStringLiteral("transparent") : color));
  refreshCombos();
  rebuildTree();
  refreshToolbar();
}

void DatabasePanel::onConnectionState(const QString &id,
                                      DbConnectionState state) {
  DbConnection *c = m_manager->connection(id);
  if (state == DbConnectionState::Failed && c) {
    logMessage(tr("Could not connect to \"%1\"").arg(c->profile().name), true,
               c->lastError());
    if (m_userConnect) {
      m_userConnect = false;
      ThemedMessageBox::warning(this, tr("Connection failed"),
                                tr("Could not connect to \"%1\".\n\n%2")
                                    .arg(c->profile().name, c->lastError()));
    }
    m_outputTabs->setCurrentWidget(m_messages);
  } else if (state == DbConnectionState::Connected && c) {
    m_userConnect = false;
    logMessage(tr("Connected to \"%1\" (%2)")
                   .arg(c->profile().name, c->profile().summary()),
               false);
    setStatus(tr("Connected to %1").arg(c->profile().name));
  } else if (state == DbConnectionState::Disconnected && c &&
             !c->lastError().isEmpty() &&
             !c->lastError().contains(QLatin1String("cancelled"))) {
    logMessage(tr("Disconnected from \"%1\"").arg(c->profile().name), true,
               c->lastError());
  }
  refreshCombos();
  rebuildTree();
  refreshToolbar();
}

void DatabasePanel::onSchemaChanged(const QString &id) {
  Q_UNUSED(id)
  rebuildTree();
  refreshCombos();
}

void DatabasePanel::newConnection() {
  ConnectionDialog dlg(this);
  dlg.applyTheme(m_theme);
  dlg.setNameSuggestionProvider(
      [this](const QString &n) { return m_manager->uniqueName(n); });
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const QString id = m_manager->saveProfile(dlg.profile());
  m_manager->setActive(id);
  DbConnection *c = m_manager->connection(id);
  if (c && !dlg.password().isEmpty()) {
    c->rememberPassword(dlg.password());
  }
  if (dlg.connectAfterSave()) {
    connectConnection(id);
  }
}

void DatabasePanel::editConnection(const QString &id) {
  DbConnection *c = m_manager->connection(id);
  if (!c) {
    return;
  }
  ConnectionDialog dlg(this);
  dlg.applyTheme(m_theme);
  dlg.setNameSuggestionProvider(
      [this, id](const QString &n) { return m_manager->uniqueName(n, id); });
  dlg.setProfile(c->profile());
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  DbConnectionProfile p = dlg.profile();
  p.id = id;
  m_manager->saveProfile(p);
  if (!dlg.password().isEmpty()) {
    c->rememberPassword(dlg.password());
  }
  if (dlg.connectAfterSave() && !c->isConnected()) {
    connectConnection(id);
  }
  onActiveChanged();
}

void DatabasePanel::duplicateConnection(const QString &id) {
  DbConnection *c = m_manager->connection(id);
  if (!c) {
    return;
  }
  DbConnectionProfile p = c->profile();
  p.id.clear();
  p.name = m_manager->uniqueName(p.name + tr(" copy"));
  m_manager->setActive(m_manager->saveProfile(p));
}

void DatabasePanel::removeConnection(const QString &id) {
  DbConnection *c = m_manager->connection(id);
  if (!c) {
    return;
  }
  if (ThemedMessageBox::question(this, tr("Remove connection"),
                                 tr("Remove the connection \"%1\"?\nThe "
                                    "database itself is not touched.")
                                     .arg(c->profile().name)) !=
      ThemedMessageBox::Yes) {
    return;
  }
  m_manager->removeProfile(id);
}

bool DatabasePanel::ensurePassword(DbConnection *conn) {
  if (!conn) {
    return false;
  }
  if (conn->isConnected() || conn->state() == DbConnectionState::Connecting ||
      conn->profile().engine == DbEngine::Sqlite ||
      conn->hasSessionPassword()) {
    return true;
  }
  bool ok = false;
  const QString who =
      conn->profile().user.isEmpty()
          ? conn->profile().name
          : conn->profile().user + QLatin1Char('@') + conn->profile().name;
  const QString pw = QInputDialog::getText(
      this, tr("Connect to %1").arg(conn->profile().name),
      tr("Password for %1\n(kept in memory for this session only):").arg(who),
      QLineEdit::Password, QString(), &ok);
  if (!ok) {
    return false;
  }
  conn->rememberPassword(pw);
  return true;
}

void DatabasePanel::connectConnection(const QString &id, bool userInitiated) {
  DbConnection *c = m_manager->connection(id);
  if (!c || c->isConnected() || c->state() == DbConnectionState::Connecting) {
    return;
  }
  if (!ensurePassword(c)) {
    return;
  }
  m_userConnect = userInitiated;
  hookConnection(c);
  c->reconnect();
}

void DatabasePanel::disconnectConnection(const QString &id) {
  if (DbConnection *c = m_manager->connection(id)) {
    c->disconnectFromServer();
    logMessage(tr("Disconnected from \"%1\"").arg(c->profile().name), false);
  }
}

void DatabasePanel::refreshSchema(const QString &id) {
  DbConnection *c =
      id.isEmpty() ? activeConnection() : m_manager->connection(id);
  if (c && c->isConnected()) {
    c->refreshSchema();
    setStatus(tr("Refreshing schema of %1…").arg(c->profile().name));
  } else if (c) {
    connectConnection(c->profile().id);
  }
}

void DatabasePanel::refreshCombos() {
  const QSignalBlocker b1(m_connectionCombo);
  const QSignalBlocker b2(m_databaseCombo);
  m_connectionCombo->clear();
  int activeIndex = -1;
  const auto conns = m_manager->connections();
  for (int i = 0; i < conns.size(); ++i) {
    DbConnection *c = conns[i];
    m_connectionCombo->addItem(stateIcon(c->state(), c->profile().color),
                               c->profile().name, c->profile().id);
    m_connectionCombo->setItemData(i, c->profile().summary(), Qt::ToolTipRole);
    if (c->profile().id == m_manager->activeId()) {
      activeIndex = i;
    }
  }
  if (conns.isEmpty()) {
    m_connectionCombo->addItem(tr("No connections"));
    m_connectionCombo->setEnabled(false);
  } else {
    m_connectionCombo->setEnabled(true);
    m_connectionCombo->setCurrentIndex(activeIndex);
  }

  m_databaseCombo->clear();
  DbConnection *c = activeConnection();
  const bool hasDatabases = c && c->isConnected() &&
                            c->profile().engine != DbEngine::Sqlite &&
                            !c->databases().isEmpty();
  m_databaseComboAction->setVisible(
      c && c->profile().engine != DbEngine::Sqlite &&
      (hasDatabases || !c->currentDatabase().isEmpty()));
  if (hasDatabases) {
    m_databaseCombo->addItems(c->databases());
    const int idx = m_databaseCombo->findText(c->currentDatabase());
    if (idx >= 0) {
      m_databaseCombo->setCurrentIndex(idx);
    } else if (!c->currentDatabase().isEmpty()) {
      m_databaseCombo->insertItem(0, c->currentDatabase());
      m_databaseCombo->setCurrentIndex(0);
    }
    m_databaseCombo->setEnabled(true);
  } else {
    if (c && !c->currentDatabase().isEmpty()) {
      m_databaseCombo->addItem(c->currentDatabase());
    }
    m_databaseCombo->setEnabled(false);
  }
}

void DatabasePanel::onConnectionComboActivated(int index) {
  const QString id = m_connectionCombo->itemData(index).toString();
  if (!id.isEmpty()) {
    m_manager->setActive(id);
  }
}

void DatabasePanel::onDatabaseComboActivated(int index) {
  DbConnection *c = activeConnection();
  if (!c || !c->isConnected() || index < 0) {
    return;
  }
  const QString db = m_databaseCombo->itemText(index);
  if (db != c->currentDatabase()) {
    c->switchDatabase(db);
    setStatus(tr("Switching to database %1…").arg(db));
  }
}

void DatabasePanel::refreshToolbar() {
  DbConnection *c = activeConnection();
  const bool connected = c && c->isConnected();
  const bool connecting = c && c->state() == DbConnectionState::Connecting;
  const bool running = isRunning();
  const QColor text = UIStyleHelper::readableText(
      m_theme, m_theme.backgroundColor, m_theme.foregroundColor);
  m_connectAction->setIcon(DbGlyphs::icon(
      connected || connecting ? DbGlyphs::Glyph::Unplug : DbGlyphs::Glyph::Plug,
      text));
  m_connectAction->setText(connected || connecting ? tr("Disconnect")
                                                   : tr("Connect"));
  m_connectAction->setToolTip(
      connected || connecting ? tr("Disconnect from %1").arg(c->profile().name)
                              : (c ? tr("Connect to %1").arg(c->profile().name)
                                   : tr("Add a connection")));
  m_runAction->setEnabled(c && !running);
  m_runAllAction->setEnabled(c && !running);
  m_stopAction->setEnabled(running);
  m_explainAction->setEnabled(c && !running);
  m_refreshAction->setEnabled(c);
  m_manageAction->setEnabled(true);
}

bool DatabasePanel::isRunning() const {
  return !m_runs.isEmpty() || m_pendingRun.conn;
}

DbConnection *DatabasePanel::connectionFromItem(QTreeWidgetItem *item) const {
  return item ? m_manager->connection(item->data(0, RoleConnection).toString())
              : nullptr;
}

const DbTableInfo *DatabasePanel::tableFromItem(QTreeWidgetItem *item,
                                                DbConnection **connOut) const {
  if (!item || item->data(0, RoleKind).toInt() != KindTable) {
    return nullptr;
  }
  DbConnection *c = connectionFromItem(item);
  if (connOut) {
    *connOut = c;
  }
  return c ? c->schema().find(item->data(0, RoleSchema).toString(),
                              item->data(0, RoleName).toString())
           : nullptr;
}

QTreeWidgetItem *DatabasePanel::addTableItem(QTreeWidgetItem *parent,
                                             DbConnection *conn,
                                             const DbTableInfo &table) {
  const QColor text = UIStyleHelper::readableText(
      m_theme, m_theme.backgroundColor, m_theme.foregroundColor);
  auto *item = new QTreeWidgetItem(parent);
  item->setText(0, table.name);
  item->setData(0, kTypeRole, table.isView ? tr("view") : QString());
  item->setIcon(0, DbGlyphs::icon(table.isView ? DbGlyphs::Glyph::View
                                               : DbGlyphs::Glyph::Table,
                                  text));
  item->setData(0, RoleKind, KindTable);
  item->setData(0, RoleConnection, conn->profile().id);
  item->setData(0, RoleSchema, table.schema);
  item->setData(0, RoleName, table.name);
  item->setData(0, Qt::UserRole + 5,
                QStringLiteral("t:%1:%2:%3")
                    .arg(conn->profile().id, table.schema, table.name));
  item->setToolTip(0,
                   tr("%1.%2 — %n column(s)", nullptr, table.columns.size())
                       .arg(table.schema.isEmpty() ? QStringLiteral("(default)")
                                                   : table.schema,
                            table.name));
  if (table.isView) {
    QFont f = item->font(0);
    f.setItalic(true);
    item->setFont(0, f);
  }
  auto *placeholder = new QTreeWidgetItem(item);
  placeholder->setData(0, RoleKind, KindPlaceholder);
  return item;
}

void DatabasePanel::populateColumns(QTreeWidgetItem *tableItem) {
  if (tableItem->childCount() != 1 ||
      tableItem->child(0)->data(0, RoleKind).toInt() != KindPlaceholder) {
    return;
  }
  DbConnection *conn = nullptr;
  const DbTableInfo *table = tableFromItem(tableItem, &conn);
  qDeleteAll(tableItem->takeChildren());
  if (!table) {
    return;
  }
  const QColor text = UIStyleHelper::readableText(
      m_theme, m_theme.backgroundColor, m_theme.foregroundColor);
  const QColor muted = UIStyleHelper::mutedTextColor(m_theme);
  const QColor accent =
      m_theme.warningColor.isValid() ? m_theme.warningColor : text;
  for (const DbColumnInfo &col : table->columns) {
    auto *item = new QTreeWidgetItem(tableItem);
    item->setText(0, col.name);
    item->setData(
        0, kTypeRole,
        col.type + (col.nullable ? QString() : QStringLiteral("  NOT NULL")));
    item->setIcon(0, DbGlyphs::icon(col.primaryKey ? DbGlyphs::Glyph::Key
                                                   : DbGlyphs::Glyph::Column,
                                    col.primaryKey ? accent : muted));
    item->setData(0, RoleKind, KindColumn);
    item->setData(0, RoleConnection, conn->profile().id);
    item->setData(0, RoleSchema, table->schema);
    item->setData(0, RoleName, col.name);
    item->setToolTip(
        0, QStringLiteral("%1 %2%3%4")
               .arg(col.name, col.type,
                    col.nullable ? QString() : QStringLiteral(" NOT NULL"),
                    col.primaryKey ? QStringLiteral(" · primary key")
                                   : QString()));
  }
}

void DatabasePanel::populateConnectionItem(QTreeWidgetItem *item,
                                           DbConnection *conn) {
  const QColor text = UIStyleHelper::readableText(
      m_theme, m_theme.backgroundColor, m_theme.foregroundColor);
  const QColor muted = UIStyleHelper::mutedTextColor(m_theme);
  auto hint = [&](const QString &message, const QColor &color) {
    auto *h = new QTreeWidgetItem(item);
    h->setText(0, message);
    h->setForeground(0, color);
    h->setData(0, RoleKind, KindHint);
    h->setData(0, RoleConnection, conn->profile().id);
    h->setFlags(h->flags() & ~Qt::ItemIsSelectable);
    return h;
  };

  switch (conn->state()) {
  case DbConnectionState::Disconnected:
    hint(tr("Double-click to connect"), muted);
    return;
  case DbConnectionState::Connecting:
    hint(tr("Connecting…"), muted);
    return;
  case DbConnectionState::Failed: {
    QTreeWidgetItem *h = hint(conn->lastError().section('\n', 0, 0),
                              stateColor(DbConnectionState::Failed));
    h->setToolTip(0, conn->lastError());
    hint(tr("Double-click to retry"), muted);
    return;
  }
  case DbConnectionState::Connected:
    break;
  }

  const DbSchema &schema = conn->schema();
  if (schema.isEmpty()) {
    hint(conn->schemaLoading() ? tr("Loading schema…") : tr("No tables"),
         muted);
    return;
  }

  const QString filter = m_filter->text().trimmed();
  auto matches = [&](const DbTableInfo &t) {
    if (filter.isEmpty() || t.name.contains(filter, Qt::CaseInsensitive)) {
      return true;
    }
    for (const DbColumnInfo &c : t.columns) {
      if (c.name.contains(filter, Qt::CaseInsensitive)) {
        return true;
      }
    }
    return false;
  };

  const QStringList schemas = schema.schemaNames();
  const bool flatten = schemas.size() == 1 &&
                       isDefaultSchema(schemas.first()) &&
                       conn->profile().engine != DbEngine::MySql;
  int shown = 0;
  for (const QString &schemaName : schemas) {
    QTreeWidgetItem *parent = item;
    QVector<const DbTableInfo *> tables = schema.tablesIn(schemaName);
    std::stable_sort(tables.begin(), tables.end(),
                     [](const DbTableInfo *a, const DbTableInfo *b) {
                       if (a->isView != b->isView) {
                         return !a->isView;
                       }
                       return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
                     });
    QVector<const DbTableInfo *> visible;
    for (const DbTableInfo *t : tables) {
      if (matches(*t)) {
        visible.append(t);
      }
    }
    if (visible.isEmpty()) {
      continue;
    }
    if (!flatten) {
      auto *schemaItem = new QTreeWidgetItem(item);
      schemaItem->setText(0,
                          schemaName.isEmpty() ? tr("(default)") : schemaName);
      schemaItem->setData(0, kTypeRole, QString::number(visible.size()));
      schemaItem->setIcon(
          0, DbGlyphs::icon(conn->profile().engine == DbEngine::MySql
                                ? DbGlyphs::Glyph::Database
                                : DbGlyphs::Glyph::Schema,
                            muted));
      schemaItem->setData(0, RoleKind, KindSchema);
      schemaItem->setData(0, RoleConnection, conn->profile().id);
      schemaItem->setData(0, RoleSchema, schemaName);
      schemaItem->setData(
          0, Qt::UserRole + 5,
          QStringLiteral("s:%1:%2").arg(conn->profile().id, schemaName));
      parent = schemaItem;
    }
    for (const DbTableInfo *t : visible) {
      QTreeWidgetItem *ti = addTableItem(parent, conn, *t);
      ++shown;
      if (!filter.isEmpty()) {
        Q_UNUSED(ti)
      } else if (m_expanded.contains(itemKey(ti))) {
        ti->setExpanded(true);
        populateColumns(ti);
      }
    }
    if (parent != item) {
      parent->setExpanded(!filter.isEmpty() ||
                          m_expanded.contains(itemKey(parent)) ||
                          schemas.size() == 1);
    }
  }
  if (shown == 0) {
    hint(tr("Nothing matches \"%1\"").arg(filter), muted);
  }
  Q_UNUSED(text)
}

void DatabasePanel::rebuildTree() {
  if (m_rebuildingTree) {
    return;
  }
  m_rebuildingTree = true;
  const QString selectedKey =
      m_tree->currentItem() ? itemKey(m_tree->currentItem()) : QString();
  const int scroll = m_tree->verticalScrollBar()->value();
  m_tree->setUpdatesEnabled(false);
  m_tree->clear();

  const QColor muted = UIStyleHelper::mutedTextColor(m_theme);
  const QColor text = UIStyleHelper::readableText(
      m_theme, m_theme.backgroundColor, m_theme.foregroundColor);
  const auto conns = m_manager->connections();
  if (conns.isEmpty()) {
    auto *empty = new QTreeWidgetItem(m_tree);
    empty->setText(0, tr("No connections yet"));
    empty->setForeground(0, muted);
    empty->setData(0, RoleKind, KindHint);
    auto *add = new QTreeWidgetItem(m_tree);
    add->setText(0,
                 tr("Click + to add SQLite, SQL Server, PostgreSQL or MySQL"));
    add->setForeground(0, muted);
    add->setData(0, RoleKind, KindHint);
  }
  for (DbConnection *conn : conns) {
    auto *item = new QTreeWidgetItem(m_tree);
    item->setText(0, conn->profile().name);
    item->setIcon(0, stateIcon(conn->state(), conn->profile().color));
    QFont f = item->font(0);
    f.setBold(conn->profile().id == m_manager->activeId());
    item->setFont(0, f);
    item->setData(0, RoleKind, KindConnection);
    item->setData(0, RoleConnection, conn->profile().id);
    item->setData(0, Qt::UserRole + 5,
                  QStringLiteral("c:%1").arg(conn->profile().id));
    item->setToolTip(0, QStringLiteral("%1\n%2").arg(
                            DbEngineInfo::displayName(conn->profile().engine),
                            conn->profile().summary()));
    if (!conn->profile().color.isEmpty()) {
      item->setForeground(
          0,
          QColor(conn->profile().color)
              .lighter(m_theme.backgroundColor.lightness() < 128 ? 130 : 100));
    } else {
      item->setForeground(0, text);
    }
    populateConnectionItem(item, conn);
    item->setExpanded(conn->isConnected() ||
                      conn->state() == DbConnectionState::Failed ||
                      m_expanded.contains(itemKey(item)));
  }

  if (!selectedKey.isEmpty()) {
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
      if (itemKey(*it) == selectedKey) {
        m_tree->setCurrentItem(*it);
        break;
      }
      ++it;
    }
  }
  m_tree->setUpdatesEnabled(true);
  m_tree->verticalScrollBar()->setValue(scroll);
  m_rebuildingTree = false;
}

void DatabasePanel::onFilterChanged(const QString &) { rebuildTree(); }

void DatabasePanel::onTreeItemExpanded(QTreeWidgetItem *item) {
  if (m_rebuildingTree) {
    return;
  }
  const QString key = itemKey(item);
  if (!key.isEmpty()) {
    m_expanded.insert(key);
  }
  if (item->data(0, RoleKind).toInt() == KindTable) {
    populateColumns(item);
  }
}

void DatabasePanel::onTreeItemCollapsed(QTreeWidgetItem *item) {
  if (m_rebuildingTree) {
    return;
  }
  m_expanded.remove(itemKey(item));
}

void DatabasePanel::onTreeDoubleClicked(QTreeWidgetItem *item, int) {
  if (!item) {
    return;
  }
  DbConnection *conn = connectionFromItem(item);
  switch (item->data(0, RoleKind).toInt()) {
  case KindConnection:
    m_manager->setActive(conn->profile().id);
    if (conn->isConnected()) {
      item->setExpanded(!item->isExpanded());
    } else {
      connectConnection(conn->profile().id);
    }
    break;
  case KindHint:
    if (conn && !conn->isConnected()) {
      m_manager->setActive(conn->profile().id);
      connectConnection(conn->profile().id);
    }
    break;
  case KindTable:
    if (const DbTableInfo *t = tableFromItem(item)) {
      m_manager->setActive(conn->profile().id);
      previewTable(conn, *t);
    }
    break;
  case KindColumn: {
    const QString name = DbCatalog::quoteIdentifier(
        conn->profile().engine, item->data(0, RoleName).toString());
    m_console->insertPlainText(name);
    m_console->setFocus();
    break;
  }
  default:
    break;
  }
}

void DatabasePanel::onTreeContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_tree->itemAt(pos);
  if (!item) {
    QMenu menu(this);
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));
    menu.addAction(tr("New connection…"), this, &DatabasePanel::newConnection);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
    return;
  }
  DbConnection *conn = connectionFromItem(item);
  QMenu menu(this);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));

  switch (item->data(0, RoleKind).toInt()) {
  case KindConnection: {
    const QString id = conn->profile().id;
    if (conn->isConnected() || conn->state() == DbConnectionState::Connecting) {
      menu.addAction(tr("Disconnect"), this,
                     [this, id]() { disconnectConnection(id); });
      menu.addAction(tr("Refresh schema"), this,
                     [this, id]() { refreshSchema(id); });
    } else {
      menu.addAction(tr("Connect"), this,
                     [this, id]() { connectConnection(id); });
    }
    menu.addAction(tr("Use for queries"), this,
                   [this, id]() { m_manager->setActive(id); });
    menu.addAction(tr("New SQL file"), this, [this, conn]() {
      emit openInEditorRequested(
          tr("query.sql"),
          QStringLiteral("-- connection: %1\n\n").arg(conn->profile().name));
    });
    menu.addSeparator();
    menu.addAction(tr("Edit…"), this, [this, id]() { editConnection(id); });
    menu.addAction(tr("Duplicate"), this,
                   [this, id]() { duplicateConnection(id); });
    menu.addAction(tr("Copy connection summary"), this, [conn]() {
      QApplication::clipboard()->setText(conn->profile().summary());
    });
    menu.addSeparator();
    menu.addAction(tr("Remove…"), this, [this, id]() { removeConnection(id); });
    break;
  }
  case KindTable: {
    DbConnection *c = nullptr;
    const DbTableInfo *t = tableFromItem(item, &c);
    if (!t) {
      return;
    }
    const DbTableInfo table = *t;
    menu.addAction(tr("Preview data (%1 rows)").arg(kPreviewRows), this,
                   [this, c, table]() { previewTable(c, table); });
    menu.addAction(tr("Count rows"), this, [this, c, table]() {
      execute(c, {DbCatalog::countSql(c->profile().engine, table)}, true);
    });
    menu.addAction(tr("Show columns"), this,
                   [this, c, table]() { showTableColumns(c, table); });
    menu.addAction(tr("Profile columns…"), this, [this, c, table]() {
      execute(c, {DbCatalog::selectTopSql(c->profile().engine, table, 10000)},
              true, table.name, true);
    });
    QMenu *gen = menu.addMenu(tr("Generate SQL"));
    for (const QString &what :
         {QStringLiteral("select"), QStringLiteral("insert"),
          QStringLiteral("update"), QStringLiteral("create")}) {
      const QString label = what == "select"   ? tr("SELECT columns")
                            : what == "insert" ? tr("INSERT template")
                            : what == "update"
                                ? tr("UPDATE template")
                                : tr("CREATE TABLE (approximation)");
      gen->addAction(label, this,
                     [this, c, table, what]() { generateSql(c, table, what); });
    }
    menu.addSeparator();
    menu.addAction(tr("Copy name"), this, [table]() {
      QApplication::clipboard()->setText(table.name);
    });
    menu.addAction(tr("Copy qualified name"), this, [c, table]() {
      QApplication::clipboard()->setText(
          DbCatalog::qualifiedName(c->profile().engine, table));
    });
    break;
  }
  case KindColumn: {
    const QString name = item->data(0, RoleName).toString();
    menu.addAction(tr("Copy name"), this,
                   [name]() { QApplication::clipboard()->setText(name); });
    menu.addAction(tr("Insert into console"), this, [this, conn, name]() {
      m_console->insertPlainText(
          DbCatalog::quoteIdentifier(conn->profile().engine, name));
      m_console->setFocus();
    });
    break;
  }
  case KindSchema:
    menu.addAction(tr("Refresh schema"), this,
                   [this, conn]() { refreshSchema(conn->profile().id); });
    break;
  default:
    return;
  }
  menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void DatabasePanel::previewTable(DbConnection *conn, const DbTableInfo &table) {
  execute(
      conn,
      {DbCatalog::selectTopSql(conn->profile().engine, table, kPreviewRows)},
      true, DbCatalog::qualifiedName(conn->profile().engine, table));
}

void DatabasePanel::showTableColumns(DbConnection *conn,
                                     const DbTableInfo &table) {
  DbResultSet rs;
  rs.columns = {{QStringLiteral("column"), {}},
                {QStringLiteral("type"), {}},
                {QStringLiteral("nullable"), {}},
                {QStringLiteral("key"), {}}};
  for (const DbColumnInfo &c : table.columns) {
    rs.rows.append({c.name, c.type,
                    c.nullable ? QStringLiteral("YES") : QStringLiteral("NO"),
                    c.primaryKey ? QStringLiteral("PRIMARY KEY") : QString()});
  }
  rs.totalRows = rs.rows.size();
  clearResults();
  Run run;
  run.engine = conn->profile().engine;
  addResultTab(rs, run, tr("Columns: %1").arg(table.name), -1);
  m_outputTabs->setCurrentIndex(0);
  setStatus(tr("%1 has %2 columns").arg(table.name).arg(table.columns.size()));
}

void DatabasePanel::generateSql(DbConnection *conn, const DbTableInfo &table,
                                const QString &what) {
  const DbEngine e = conn->profile().engine;
  QString sql;
  if (what == "select") {
    sql = DbCatalog::selectColumnsSql(e, table);
  } else if (what == "insert") {
    sql = DbCatalog::insertTemplateSql(e, table);
  } else if (what == "update") {
    sql = DbCatalog::updateTemplateSql(e, table);
  } else {
    sql = DbCatalog::createTableSql(e, table);
  }
  setConsoleText(sql);
}

void DatabasePanel::setStatus(const QString &text) {
  m_status->setText(text);
  emit statusMessage(text);
}

void DatabasePanel::logMessage(const QString &text, bool isError,
                               const QString &detail) {
  const QColor muted = UIStyleHelper::mutedTextColor(m_theme);
  const QColor tone =
      isError ? (m_theme.errorColor.isValid() ? m_theme.errorColor
                                              : QColor("#f85149"))
              : (m_theme.successColor.isValid() ? m_theme.successColor
                                                : QColor("#3fb950"));
  QString html =
      QStringLiteral(
          "<span style='color:%1'>%2</span> <b style='color:%3'>%4</b> %5")
          .arg(muted.name(),
               QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
               tone.name(), isError ? QStringLiteral("✗") : QStringLiteral("✓"),
               text.toHtmlEscaped());
  if (!detail.isEmpty()) {
    html += QStringLiteral(
                "<br><span "
                "style='color:%1'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%2</span>")
                .arg(isError ? tone.name() : muted.name(),
                     detail.toHtmlEscaped().replace(
                         '\n',
                         QLatin1String("<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")));
  }
  m_messages->appendHtml(html);
  ++m_messageCount;
  if (isError) {
    ++m_errorCount;
  }
  updateTabTitles();
}

void DatabasePanel::updateTabTitles() {
  m_outputTabs->setTabText(0,
                           m_resultTabs->count() > 0
                               ? tr("Results (%1)").arg(m_resultTabs->count())
                               : tr("Results"));
  QString msg = tr("Messages");
  if (m_errorCount > 0) {
    msg += tr(" (%1 error(s))").arg(m_errorCount);
  } else if (m_messageCount > 0) {
    msg += QStringLiteral(" (%1)").arg(m_messageCount);
  }
  m_outputTabs->setTabText(1, msg);
}

void DatabasePanel::clearResults() {
  while (m_resultTabs->count() > 0) {
    QWidget *w = m_resultTabs->widget(0);
    m_resultTabs->removeTab(0);
    delete w;
  }
  m_resultStack->setCurrentWidget(m_resultsEmpty);
  updateTabTitles();
}

bool DatabasePanel::confirmStatements(DbConnection *conn,
                                      const QStringList &statements) {
  if (!conn->profile().confirmDestructive) {
    return true;
  }
  QStringList warnings;
  for (const QString &s : statements) {
    const QString w =
        SqlStatementSplitter::destructiveWarning(s, conn->profile().engine);
    if (!w.isEmpty()) {
      warnings << QStringLiteral("• %1\n   %2").arg(w, oneLine(s, 90));
    }
  }
  if (warnings.isEmpty()) {
    return true;
  }
  if (warnings.size() > 5) {
    const int more = warnings.size() - 5;
    warnings = warnings.mid(0, 5);
    warnings << tr("… and %n more", nullptr, more);
  }
  return ThemedMessageBox::question(
             this, tr("Run on \"%1\"?").arg(conn->profile().name),
             tr("This cannot be undone:\n\n%1\n\nRun anyway?")
                 .arg(warnings.join("\n")),
             ThemedMessageBox::Yes | ThemedMessageBox::No,
             ThemedMessageBox::No) == ThemedMessageBox::Yes;
}

quint64 DatabasePanel::execute(DbConnection *conn,
                               const QStringList &statementsIn,
                               bool stopOnError, const QString &tableName,
                               bool profileAfter, bool explain) {
  QStringList statements;
  for (const QString &s : statementsIn) {
    if (!SqlStatementSplitter::stripComments(s).trimmed().isEmpty()) {
      statements << s;
    }
  }
  if (!conn) {
    newConnection();
    return 0;
  }
  if (statements.isEmpty()) {
    setStatus(tr("Nothing to run"));
    return 0;
  }
  if (m_activeRunByConnection.contains(conn->profile().id) ||
      m_pendingRun.conn) {
    setStatus(tr("\"%1\" is still busy — stop the running query first.")
                  .arg(conn->profile().name));
    return 0;
  }
  if (!ensurePassword(conn) || !confirmStatements(conn, statements)) {
    return 0;
  }
  hookConnection(conn);
  clearResults();

  m_pendingRun = Run();
  m_pendingRun.conn = conn;
  m_pendingRun.connectionName = conn->profile().name;
  m_pendingRun.engine = conn->profile().engine;
  m_pendingRun.tableName = tableName;
  m_pendingRun.profileAfter = profileAfter;
  m_pendingRun.explain = explain;
  m_pendingRun.statements = statements.size();
  m_pendingRun.timer.start();

  const QString header =
      statements.size() == 1
          ? oneLine(statements.first(), 120)
          : tr("%n statement(s)", nullptr, statements.size());
  m_messages->appendHtml(
      QStringLiteral("<span style='color:%1'>▶ %2 · %3</span>")
          .arg(UIStyleHelper::mutedTextColor(m_theme).name(),
               conn->profile().name.toHtmlEscaped(), header.toHtmlEscaped()));
  m_progress->show();
  m_runningTimer.start();
  updateRunningLabel();
  setStatus(tr("Running on %1…").arg(conn->profile().name));

  const quint64 id = conn->execute(statements, stopOnError);
  m_pendingRun = Run();
  refreshToolbar();
  return id;
}

void DatabasePanel::addResultTab(const DbResultSet &rs, const Run &run,
                                 const QString &title, qint64 elapsedMs) {
  auto *view = new DbResultView(m_resultTabs);
  view->applyTheme(m_theme);
  view->setResult(rs, run.engine, run.tableName, elapsedMs);
  connect(view, &DbResultView::statusMessage, this, &DatabasePanel::setStatus);
  connect(view, &DbResultView::insightsRequested, this, [this, view]() {
    m_insights->setResult(view->result(),
                          m_resultTabs->tabText(m_resultTabs->indexOf(view)));
    m_outputTabs->setCurrentWidget(m_insights);
  });
  QString label = title;
  if (title.isEmpty()) {
    label = tr("Result %1").arg(m_resultTabs->count() + 1);
  }
  label += rs.truncated ? tr("  ·  %1+ rows").arg(rs.rows.size())
                        : tr("  ·  %1 row(s)").arg(rs.rows.size());
  const int index = m_resultTabs->addTab(view, label);
  m_resultStack->setCurrentWidget(m_resultTabs);
  if (m_resultTabs->count() == 1) {
    m_resultTabs->setCurrentIndex(index);
  }
  updateTabTitles();
}

void DatabasePanel::onStatementFinished(DbConnection *conn, quint64 id,
                                        int index, const DbStatementResult &r) {
  auto it = m_runs.find(id);
  if (it == m_runs.end()) {
    return;
  }
  Run &run = it.value();
  ++run.finished;
  run.elapsedMs += r.elapsedMs;
  m_manager->recordHistory(conn, r);

  if (!r.ok) {
    ++run.errors;
    logMessage(tr("Failed after %1 · %2")
                   .arg(formatDuration(r.elapsedMs), oneLine(r.sql, 100)),
               true, r.error);
    return;
  }
  if (r.hasRows()) {
    int part = 0;
    for (const DbResultSet &rs : r.resultSets) {
      ++part;
      ++run.results;
      run.rows += rs.totalRows;
      QString title = run.explain ? tr("Plan") : QString();
      if (title.isEmpty()) {
        title = run.tableName.isEmpty() || run.statements != 1
                    ? tr("Result %1").arg(m_resultTabs->count() + 1)
                    : run.tableName;
        if (r.resultSets.size() > 1) {
          title = tr("Statement %1.%2").arg(index + 1).arg(part);
        }
      }
      addResultTab(rs, run, title, r.elapsedMs);
    }
    logMessage(tr("%1 row(s) in %2 · %3")
                   .arg(r.resultSets.first().totalRows)
                   .arg(formatDuration(r.elapsedMs), oneLine(r.sql, 100)),
               false, r.messages.join('\n'));
  } else {
    QString what = r.rowsAffected >= 0
                       ? tr("%1 row(s) affected").arg(r.rowsAffected)
                       : tr("Command completed");
    run.rows += qMax<qint64>(0, r.rowsAffected);
    logMessage(tr("%1 in %2 · %3")
                   .arg(what, formatDuration(r.elapsedMs), oneLine(r.sql, 100)),
               false, r.messages.join('\n'));
  }
}

void DatabasePanel::onRequestFinished(DbConnection *conn, quint64 id,
                                      bool cancelled) {
  auto it = m_runs.find(id);
  if (it == m_runs.end()) {
    return;
  }
  const Run run = it.value();
  m_runs.erase(it);
  m_activeRunByConnection.remove(conn->profile().id);
  if (m_runs.isEmpty()) {
    m_progress->hide();
    m_runningTimer.stop();
    m_runningLabel->clear();
  }

  const qint64 total = run.timer.elapsed();
  QString summary;
  if (cancelled) {
    summary = tr("Cancelled after %1").arg(formatDuration(total));
    logMessage(summary + tr(" — the session will reconnect on the next query."),
               true);
  } else if (run.errors > 0) {
    summary = tr("Finished with %n error(s) in %1", nullptr, run.errors)
                  .arg(formatDuration(total));
  } else {
    summary = tr("Done in %1").arg(formatDuration(total));
    if (run.results > 0) {
      summary += tr(" · %n result set(s)", nullptr, run.results);
    }
    if (run.rows > 0) {
      summary += tr(" · %1 row(s)").arg(run.rows);
    }
  }
  setStatus(QStringLiteral("%1 — %2").arg(conn->profile().name, summary));

  if (run.results > 0) {
    m_outputTabs->setCurrentIndex(0);
    if (run.profileAfter) {
      if (auto *v = qobject_cast<DbResultView *>(m_resultTabs->widget(0))) {
        m_insights->setResult(v->result(), run.tableName);
        m_outputTabs->setCurrentWidget(m_insights);
      }
    }
  } else if (run.errors > 0) {
    m_outputTabs->setCurrentWidget(m_messages);
  } else if (!cancelled) {
    m_outputTabs->setCurrentWidget(m_messages);
  }
  refreshToolbar();
}

void DatabasePanel::updateRunningLabel() {
  qint64 longest = 0;
  for (const Run &r : m_runs) {
    longest = qMax<qint64>(longest, r.timer.elapsed());
  }
  if (m_pendingRun.conn) {
    longest = qMax<qint64>(longest, m_pendingRun.timer.elapsed());
  }
  m_runningLabel->setText(tr("Running… %1").arg(formatDuration(longest)));
}

void DatabasePanel::stopRunning() {
  for (DbConnection *c : m_manager->connections()) {
    if (m_activeRunByConnection.contains(c->profile().id)) {
      c->cancel();
    }
  }
}

void DatabasePanel::runStatement() {
  DbConnection *c = activeConnection();
  if (!c) {
    newConnection();
    return;
  }
  const QString sel = m_console->textCursor().hasSelection()
                          ? m_console->textCursor().selectedText().replace(
                                QChar::ParagraphSeparator, '\n')
                          : QString();
  QStringList statements;
  if (!sel.isEmpty()) {
    for (const SqlStatement &s :
         SqlStatementSplitter::split(sel, c->profile().engine)) {
      statements << s.text;
    }
  } else {
    const SqlStatement st = SqlStatementSplitter::statementAt(
        m_console->toPlainText(), m_console->textCursor().position(),
        c->profile().engine);
    if (!st.text.isEmpty()) {
      statements << st.text;
    }
  }
  m_historyCursor = -1;
  execute(c, statements, true);
}

void DatabasePanel::runScript() {
  DbConnection *c = activeConnection();
  if (!c) {
    newConnection();
    return;
  }
  QStringList statements;
  for (const SqlStatement &s : SqlStatementSplitter::split(
           m_console->toPlainText(), c->profile().engine)) {
    statements << s.text;
  }
  m_historyCursor = -1;
  execute(c, statements, true);
}

void DatabasePanel::runSql(const QString &sql) {
  DbConnection *c = activeConnection();
  QStringList statements;
  const DbEngine engine = c ? c->profile().engine : DbEngine::PostgreSql;
  for (const SqlStatement &s : SqlStatementSplitter::split(sql, engine)) {
    statements << s.text;
  }
  execute(c, statements, true);
}

void DatabasePanel::explainStatement() {
  DbConnection *c = activeConnection();
  if (!c) {
    newConnection();
    return;
  }
  const QString sel = m_console->textCursor().hasSelection()
                          ? m_console->textCursor().selectedText().replace(
                                QChar::ParagraphSeparator, '\n')
                          : QString();
  const QString stmt = !sel.isEmpty() ? sel.trimmed()
                                      : SqlStatementSplitter::statementAt(
                                            m_console->toPlainText(),
                                            m_console->textCursor().position(),
                                            c->profile().engine)
                                            .text;
  if (stmt.isEmpty()) {
    setStatus(tr("Nothing to explain"));
    return;
  }
  if (c->profile().engine == DbEngine::SqlServer) {
    execute(c,
            {QStringLiteral("SET SHOWPLAN_ALL ON"), stmt,
             QStringLiteral("SET SHOWPLAN_ALL OFF")},
            false, QString(), false, true);
  } else {
    execute(c, {DbCatalog::explainPrefix(c->profile().engine) + stmt}, true,
            QString(), false, true);
  }
}

void DatabasePanel::runFromEditor(const QString &script, int caretOffset,
                                  const QString &selection, bool runAll,
                                  bool explain) {
  DbConnection *c = connectionForScript(script);
  if (!c) {
    if (m_manager->profiles().isEmpty()) {
      if (ThemedMessageBox::question(
              this, tr("No database connection"),
              tr("Add a database connection to run this script?"),
              ThemedMessageBox::Yes | ThemedMessageBox::No,
              ThemedMessageBox::Yes) == ThemedMessageBox::Yes) {
        newConnection();
      }
    } else {
      setStatus(tr("Connection \"%1\" from the script header does not exist.")
                    .arg(SqlStatementSplitter::connectionDirective(script)));
    }
    return;
  }
  const DbEngine engine = c->profile().engine;
  QStringList statements;
  if (!selection.trimmed().isEmpty()) {
    for (const SqlStatement &s :
         SqlStatementSplitter::split(selection, engine)) {
      statements << s.text;
    }
  } else if (runAll) {
    for (const SqlStatement &s : SqlStatementSplitter::split(script, engine)) {
      statements << s.text;
    }
  } else {
    const SqlStatement st =
        SqlStatementSplitter::statementAt(script, caretOffset, engine);
    if (!st.text.isEmpty()) {
      statements << st.text;
    }
  }
  if (explain) {
    if (statements.isEmpty()) {
      return;
    }
    if (engine == DbEngine::SqlServer) {
      execute(c,
              {QStringLiteral("SET SHOWPLAN_ALL ON"), statements.first(),
               QStringLiteral("SET SHOWPLAN_ALL OFF")},
              false, QString(), false, true);
    } else {
      execute(c, {DbCatalog::explainPrefix(engine) + statements.first()}, true,
              QString(), false, true);
    }
    return;
  }
  execute(c, statements, true);
}

DbConnection *DatabasePanel::connectionForScript(const QString &script) const {
  const QString directive = SqlStatementSplitter::connectionDirective(script);
  if (!directive.isEmpty()) {
    return m_manager->connectionByName(directive);
  }
  return activeConnection();
}

void DatabasePanel::setConsoleText(const QString &sql, bool focus) {
  m_console->setPlainText(sql);
  m_console->moveCursor(QTextCursor::End);
  if (focus) {
    m_console->setFocus();
  }
}

void DatabasePanel::focusConsole() { m_console->setFocus(); }

void DatabasePanel::showHistory() {
  if (!m_historyDialog) {
    m_historyDialog = new QueryHistoryDialog(&m_manager->history(), this);
    m_historyDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_historyDialog, &QueryHistoryDialog::insertRequested, this,
            [this](const QString &sql) { setConsoleText(sql); });
    connect(m_historyDialog, &QueryHistoryDialog::runRequested, this,
            [this](const QString &sql, const QString &connectionName) {
              DbConnection *c = m_manager->connectionByName(connectionName);
              if (!c) {
                c = activeConnection();
              }
              if (c) {
                m_manager->setActive(c->profile().id);
              }
              QStringList statements;
              for (const SqlStatement &s : SqlStatementSplitter::split(
                       sql, c ? c->profile().engine : DbEngine::PostgreSql)) {
                statements << s.text;
              }
              setConsoleText(sql, false);
              execute(c, statements, true);
            });
  }
  QStringList names;
  for (DbConnection *c : m_manager->connections()) {
    names << c->profile().name;
  }
  m_historyDialog->applyTheme(m_theme);
  m_historyDialog->setConnectionNames(names);
  m_historyDialog->show();
  m_historyDialog->raise();
  m_historyDialog->activateWindow();
}
