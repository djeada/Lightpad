#include "debugconcurrencypanel.h"

#include "../../dap/debugsession.h"
#include "../uistylehelper.h"
#include "debugvalueinspector.h"
#include <QCheckBox>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QString sessionState(DebugSession::State state) {
  switch (state) {
  case DebugSession::State::Starting:
    return QObject::tr("Starting");
  case DebugSession::State::Running:
    return QObject::tr("Running");
  case DebugSession::State::Stopped:
    return QObject::tr("Paused");
  case DebugSession::State::Terminated:
    return QObject::tr("Finished");
  default:
    return QObject::tr("Ready");
  }
}
} // namespace

DebugConcurrencyPanel::DebugConcurrencyPanel(QWidget *parent)
    : QWidget(parent) {
  setObjectName("debugConcurrencyPanel");
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  auto *controls = new QHBoxLayout;
  auto &manager = DebugSessionManager::instance();
  auto *refreshButton = new QPushButton(tr("Refresh"), this);
  connect(refreshButton, &QPushButton::clicked, this,
          &DebugConcurrencyPanel::refresh);
  controls->addWidget(refreshButton);
  auto *pause = new QPushButton(tr("Pause all processes"), this);
  connect(pause, &QPushButton::clicked, &manager,
          &DebugSessionManager::pauseAllSessions);
  controls->addWidget(pause);
  auto *resume = new QPushButton(tr("Continue all processes"), this);
  connect(resume, &QPushButton::clicked, &manager,
          &DebugSessionManager::continueAllSessions);
  controls->addWidget(resume);
  auto *stop = new QPushButton(tr("Stop all"), this);
  connect(stop, &QPushButton::clicked, &manager,
          [&manager]() { manager.stopAllSessions(true); });
  controls->addWidget(stop);
  controls->addStretch();
  m_singleThread = new QCheckBox(tr("Run selected thread only"), this);
  m_singleThread->setObjectName("debugSingleThreadExecution");
  m_singleThread->setToolTip(
      tr("When supported by the adapter, Continue and Step run only the "
         "selected thread. Other paused threads remain paused."));
  controls->addWidget(m_singleThread);
  layout->addLayout(controls);

  auto *tabs = new QTabWidget(this);
  auto *splitter = new QSplitter(this);
  m_sessions = new QTreeWidget(splitter);
  m_sessions->setObjectName("debugProcessesTree");
  m_sessions->setHeaderLabels({tr("Process / thread"), tr("ID"), tr("State")});
  m_sessions->setUniformRowHeights(true);
  m_sessions->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_sessions->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_sessions->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_output = new QPlainTextEdit(splitter);
  m_output->setObjectName("debugProcessOutput");
  m_output->setReadOnly(true);
  m_output->setMaximumBlockCount(2000);
  m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_output->setPlaceholderText(
      tr("Select a process to read its output. Finished processes retain their "
         "recent output."));
  splitter->addWidget(m_sessions);
  splitter->addWidget(m_output);
  splitter->setSizes({500, 500});
  tabs->addTab(splitter, tr("Processes & threads"));
  connect(m_sessions, &QTreeWidget::currentItemChanged, this,
          &DebugConcurrencyPanel::showSelectedOutput);
  connect(m_sessions, &QTreeWidget::itemActivated, this,
          [this, &manager](QTreeWidgetItem *item, int) {
            DebugSession *session =
                manager.session(item->data(0, Qt::UserRole).toString());
            if (!session)
              return;
            // Focusing a process rebuilds the tree and deletes this item.
            const int thread = item->data(0, Qt::UserRole + 1).toInt();
            manager.setFocusedSession(session->id());
            emit clientSelected(session->client());
            if (thread > 0) {
              if (session->client()->isThreadStopped(thread))
                emit threadSelected(session->client(), thread);
              else {
                session->client()->pause(thread);
                m_status->setText(
                    tr("Pause requested for thread %1.").arg(thread));
              }
            }
          });

  auto *asyncPage = new QWidget(this);
  auto *asyncLayout = new QVBoxLayout(asyncPage);
  m_tasksButton = new QPushButton(tr("Inspect asyncio tasks"), asyncPage);
  m_tasksButton->setObjectName("debugInspectAsyncTasks");
  connect(m_tasksButton, &QPushButton::clicked, this,
          &DebugConcurrencyPanel::inspectAsyncTasks);
  asyncLayout->addWidget(m_tasksButton, 0, Qt::AlignLeft);
  m_tasks = new DebugValueInspector(asyncPage);
  m_tasks->setObjectName("debugAsyncTaskInspector");
  connect(m_tasks, &DebugValueInspector::addWatchRequested, this,
          &DebugConcurrencyPanel::watchRequested);
  asyncLayout->addWidget(m_tasks, 1);
  tabs->addTab(asyncPage, tr("Async tasks"));
  layout->addWidget(tabs, 1);
  m_status = new QLabel(tr("Select a process and paused thread. For asyncio, "
                           "select a Python frame with an active event loop."),
                        this);
  m_status->setWordWrap(true);
  m_status->setTextFormat(Qt::PlainText);
  layout->addWidget(m_status);

  connect(&manager, &DebugSessionManager::sessionsChanged, this,
          &DebugConcurrencyPanel::rebuildSessions);
  connect(&manager, &DebugSessionManager::focusedSessionChanged, this,
          &DebugConcurrencyPanel::rebuildSessions);
  connect(&manager, &DebugSessionManager::sessionOutputChanged, this,
          [this](const QString &id) {
            if (m_sessions->currentItem() &&
                m_sessions->currentItem()->data(0, Qt::UserRole).toString() ==
                    id)
              showSelectedOutput();
          });
  auto *timer = new QTimer(this);
  timer->setInterval(1000);
  connect(timer, &QTimer::timeout, this, [this]() {
    if (isVisible())
      refresh();
  });
  timer->start();
  rebuildSessions();
  updateActions();
}

bool DebugConcurrencyPanel::singleThreadExecution() const {
  return m_singleThread->isEnabled() && m_singleThread->isChecked();
}

void DebugConcurrencyPanel::setFrameContext(DapClient *client, int frameId) {
  if (client != m_client) {
    for (const auto &connection : std::as_const(m_contextConnections))
      disconnect(connection);
    m_contextConnections.clear();
    m_client = client;
    m_tasksRequest = 0;
    m_frameId = -1;
    m_tasks->setDapClient(client);
    if (client) {
      m_contextConnections << connect(
          client, &DapClient::evaluateValueReceived, this,
          [this](int seq, DapVariable value) {
            if (seq != m_tasksRequest || m_tasksRequest == 0)
              return;
            m_tasksRequest = 0;
            value.name = tr("Asyncio tasks");
            value.evaluateName.clear();
            m_tasks->setValue(value);
            m_status->setText(tr("Up to 50 tasks with names, state, coroutine, "
                                 "await object and stack-frame locals. Expand "
                                 "Frames or Object in Tree for details."));
            updateActions();
          });
      m_contextConnections << connect(
          client, &DapClient::evaluateResponseError, this,
          [this](int seq, const QString &, const QString &error) {
            if (seq != m_tasksRequest)
              return;
            m_tasksRequest = 0;
            m_status->setText(
                tr("Cannot inspect tasks here: %1. Select a paused frame "
                   "inside an active asyncio event loop.")
                    .arg(error));
            updateActions();
          });
      m_contextConnections << connect(client, &DapClient::stateChanged, this,
                                      [this](DapClient::State state) {
                                        if (state !=
                                            DapClient::State::Stopped) {
                                          m_frameId = -1;
                                          m_tasksRequest = 0;
                                        }
                                        updateActions();
                                      });
      m_contextConnections << connect(client, &DapClient::continued, this,
                                      [this]() {
                                        m_tasksRequest = 0;
                                        m_tasks->invalidate();
                                        updateActions();
                                      });
    }
  }
  if (frameId != m_frameId) {
    m_tasksRequest = 0;
    m_tasks->invalidate();
  }
  m_frameId = frameId;
  updateActions();
}

QString DebugConcurrencyPanel::asyncTasksExpression() {
  return QStringLiteral(
      "(lambda a: [{'Task': t.get_name(), "
      "'State': 'cancelled' if t.cancelled() else ('done' if t.done() else "
      "('cancelling' if getattr(t, 'cancelling', lambda: 0)() else "
      "('current' if t is a.current_task() else ('waiting' if "
      "getattr(t.get_coro(), 'cr_await', None) is not None else 'ready')))), "
      "'Coroutine': getattr(t.get_coro(), '__qualname__', "
      "type(t.get_coro()).__name__), "
      "'Awaiting': getattr(t.get_coro(), 'cr_await', None), "
      "'Frames': [{'Function': f.f_code.co_name, 'File': f.f_code.co_filename, "
      "'Line': f.f_lineno, 'Locals': f.f_locals} for f in "
      "t.get_stack(limit=12)], "
      "'Object': t} for t in sorted(a.all_tasks(), key=lambda t: "
      "(t.get_name(), id(t)))[:50]])(__import__('asyncio'))");
}

void DebugConcurrencyPanel::inspectAsyncTasks() {
  if (!m_client || !m_tasksButton->isEnabled())
    return;
  m_tasks->clear();
  m_status->setText(tr("Inspecting tasks in the selected event loop…"));
  m_tasksRequest =
      m_client->evaluate(asyncTasksExpression(), m_frameId, "watch");
  updateActions();
}

void DebugConcurrencyPanel::updateActions() {
  const bool paused =
      m_client && m_client->state() == DapClient::State::Stopped;
  m_singleThread->setEnabled(paused &&
                             m_client->supportsSingleThreadExecution());
  const bool python =
      m_client &&
      (m_client->adapterId().contains("python", Qt::CaseInsensitive) ||
       m_client->adapterType() == "debugpy");
  m_tasksButton->setEnabled(paused && python && m_frameId >= 0 &&
                            m_tasksRequest == 0);
}

void DebugConcurrencyPanel::refresh() {
  rebuildSessions();
  for (DebugSession *session : DebugSessionManager::instance().allSessions())
    if (session->client()->state() == DapClient::State::Stopped)
      session->client()->getThreads();
}

void DebugConcurrencyPanel::rebuildSessions() {
  auto &manager = DebugSessionManager::instance();
  QSet<DapClient *> active;
  for (DebugSession *session : manager.allSessions()) {
    DapClient *client = session->client();
    active.insert(client);
    if (!m_connections.contains(client)) {
      QList<QMetaObject::Connection> connections;
      connections << connect(client, &DapClient::threadsReceived, this,
                             [this, client](const QList<DapThread> &threads) {
                               m_threads[client] = threads;
                               rebuildSessions();
                             });
      connections << connect(
          client, &DapClient::threadEvent, this,
          [this, client](int id, const QString &reason) {
            auto &threads = m_threads[client];
            if (reason == QLatin1String("exited")) {
              threads.removeIf(
                  [id](const DapThread &thread) { return thread.id == id; });
            } else if (std::none_of(threads.cbegin(), threads.cend(),
                                    [id](const DapThread &thread) {
                                      return thread.id == id;
                                    })) {
              threads << DapThread{id, tr("Thread %1").arg(id)};
            }
            rebuildSessions();
            QTimer::singleShot(
                0, this, [guard = QPointer<DapClient>(client)]() {
                  if (guard && guard->state() == DapClient::State::Stopped)
                    guard->getThreads();
                });
          });
      m_connections.insert(client, connections);
    }
  }
  for (auto it = m_connections.begin(); it != m_connections.end();) {
    if (!active.contains(it.key())) {
      for (const auto &connection : it.value())
        disconnect(connection);
      m_threads.remove(it.key());
      it = m_connections.erase(it);
    } else
      ++it;
  }
  QString selectedId;
  int selectedThread = 0;
  if (auto *selected = m_sessions->currentItem()) {
    selectedId = selected->data(0, Qt::UserRole).toString();
    selectedThread = selected->data(0, Qt::UserRole + 1).toInt();
  }
  const QSignalBlocker blocker(m_sessions);
  m_sessions->clear();
  QHash<QString, QTreeWidgetItem *> items;
  for (DebugSession *session : manager.allSessions()) {
    auto *item = new QTreeWidgetItem;
    item->setText(0, session->name());
    item->setText(1, session->client()->processId() > 0
                         ? QString::number(session->client()->processId())
                         : QString());
    item->setText(2, session->state() == DebugSession::State::Stopped &&
                             !session->client()->allThreadsStopped()
                         ? tr("Some threads paused")
                         : sessionState(session->state()));
    item->setData(0, Qt::UserRole, session->id());
    item->setToolTip(0, tr("Double-click to focus this process. Its variables, "
                           "watches and console follow the selection."));
    items.insert(session->id(), item);
  }
  for (DebugSession *session : manager.allSessions()) {
    auto *item = items.value(session->id());
    if (auto *parent = items.value(session->parentSessionId()))
      parent->addChild(item);
    else
      m_sessions->addTopLevelItem(item);
    item->setExpanded(true);
    for (const DapThread &thread : m_threads.value(session->client())) {
      auto *child = new QTreeWidgetItem(item);
      child->setText(0, thread.name);
      child->setText(1, QString::number(thread.id));
      child->setText(2, session->client()->isThreadStopped(thread.id)
                            ? tr("Paused")
                            : tr("Running"));
      child->setData(0, Qt::UserRole, session->id());
      child->setData(0, Qt::UserRole + 1, thread.id);
      child->setToolTip(
          0, tr("Double-click a paused thread to inspect its stack. "
                "Double-click a running thread to request a pause."));
      if (session->id() == selectedId && thread.id == selectedThread)
        m_sessions->setCurrentItem(child);
    }
    if (session->id() == selectedId && selectedThread == 0)
      m_sessions->setCurrentItem(item);
  }
  for (const auto &session : manager.recentSessions()) {
    auto *item = new QTreeWidgetItem(m_sessions);
    item->setText(0, session.name);
    item->setText(1, session.processId > 0 ? QString::number(session.processId)
                                           : QString());
    item->setText(2, tr("Finished"));
    item->setData(0, Qt::UserRole, session.id);
    if (session.id == selectedId)
      m_sessions->setCurrentItem(item);
  }
  if (!m_sessions->currentItem() && manager.focusedSession())
    m_sessions->setCurrentItem(items.value(manager.focusedSession()->id()));
  showSelectedOutput();
}

void DebugConcurrencyPanel::showSelectedOutput() {
  const auto *item = m_sessions->currentItem();
  if (!item)
    return;
  const QString id = item->data(0, Qt::UserRole).toString();
  QString output;
  auto &manager = DebugSessionManager::instance();
  if (auto *session = manager.session(id))
    output = session->outputHistory();
  else
    for (const auto &session : manager.recentSessions())
      if (session.id == id)
        output = session.output;
  if (m_output->toPlainText() != output)
    m_output->setPlainText(output);
}

void DebugConcurrencyPanel::applyTheme(const Theme &theme) {
  QPalette colors = palette();
  colors.setColor(QPalette::Window, theme.backgroundColor);
  colors.setColor(QPalette::WindowText, theme.foregroundColor);
  colors.setColor(QPalette::Base, theme.surfaceColor);
  colors.setColor(QPalette::Text, theme.foregroundColor);
  setAutoFillBackground(true);
  setPalette(colors);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme) +
                UIStyleHelper::treeWidgetStyle(theme) +
                UIStyleHelper::plainTextEditStyle(theme) +
                UIStyleHelper::secondaryButtonStyle(theme) +
                UIStyleHelper::tabWidgetStyle(theme) +
                UIStyleHelper::checkBoxStyle(theme));
  m_tasks->applyTheme(theme);
}
