#include "connectiondialog.h"

#include "../../database/dbtypes.h"
#include "../panels/dbglyphs.h"
#include "themedmessagebox.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPointer>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

QWidget *wrapLayout(QLayout *layout, QWidget *parent) {
  auto *holder = new QWidget(parent);
  layout->setContentsMargins(0, 0, 0, 0);
  holder->setLayout(layout);
  return holder;
}

QString versionQuery(DbEngine engine) {
  switch (engine) {
  case DbEngine::Sqlite:
    return QStringLiteral("SELECT 'SQLite ' || sqlite_version()");
  case DbEngine::PostgreSql:
    return QStringLiteral("SELECT version()");
  case DbEngine::MySql:
    return QStringLiteral("SELECT VERSION()");
  case DbEngine::SqlServer:
    return QStringLiteral("SELECT @@VERSION");
  }
  return {};
}

QString engineBadge(DbEngine engine) {
  switch (engine) {
  case DbEngine::SqlServer:
    return QStringLiteral("SQL Server");
  case DbEngine::PostgreSql:
    return QStringLiteral("PostgreSQL");
  case DbEngine::MySql:
    return QStringLiteral("MySQL/MariaDB");
  case DbEngine::Sqlite:
    return QStringLiteral("SQLite");
  }
  return {};
}

} // namespace

ConnectionDialog::ConnectionDialog(QWidget *parent) : StyledDialog(parent) {
  setWindowTitle(tr("Database Connection"));
  m_discovery = new DockerDiscovery(this);
  connect(m_discovery, &DockerDiscovery::containersFound, this,
          &ConnectionDialog::onContainersFound);
  connect(m_discovery, &DockerDiscovery::failed, this,
          [this](const QString &msg) {
            m_dockerStatus->setText(msg);
            m_containerList->clear();
            m_containerList->setVisible(false);
          });
  connect(m_discovery, &DockerDiscovery::credentialsFound, this,
          &ConnectionDialog::onCredentials);
  buildUi();
  onEngineChanged();
  refreshContainers();

  const QScreen *screen = QGuiApplication::primaryScreen();
  const int maxHeight =
      screen ? screen->availableGeometry().height() * 90 / 100 : 800;

  const int wanted = m_body->sizeHint().height() + 230;
  resize(680, qMin(qMax(wanted, 620), maxHeight));
}

void ConnectionDialog::buildUi() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(18, 16, 18, 14);
  root->setSpacing(12);

  auto *title = new QLabel(tr("Database Connection"), this);
  title->setObjectName("dbDialogTitle");
  root->addWidget(title);
  auto *subtitle =
      new QLabel(tr("Connect to SQLite files, or to SQL Server, PostgreSQL and "
                    "MySQL/MariaDB — directly or inside a Docker container."),
                 this);
  subtitle->setWordWrap(true);
  subtitle->setObjectName("dbDialogSubtitle");
  root->addWidget(subtitle);

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *body = new QWidget(scroll);
  m_body = body;
  m_scroll = scroll;
  body->setObjectName("dbDialogBody");
  auto *bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(0, 0, 8, 0);
  bodyLayout->setSpacing(12);
  scroll->setWidget(body);
  scroll->viewport()->setAutoFillBackground(false);
  body->setAutoFillBackground(false);
  root->addWidget(scroll, 1);

  auto *dockerBox = new QGroupBox(tr("Detected in Docker"), body);
  m_dockerBox = dockerBox;
  auto *dockerLayout = new QVBoxLayout(dockerBox);
  dockerLayout->setSpacing(6);
  m_containerList = new QListWidget(dockerBox);
  m_containerList->setMinimumHeight(78);
  m_containerList->setMaximumHeight(96);
  // Shown once discovery finds a database container to pick.
  m_containerList->setVisible(false);
  dockerLayout->addWidget(m_containerList);
  auto *dockerRow = new QHBoxLayout();
  m_dockerStatus = new QLabel(dockerBox);
  m_dockerStatus->setWordWrap(true);
  dockerRow->addWidget(m_dockerStatus, 1);
  m_refreshDocker = new QPushButton(tr("Refresh"), dockerBox);
  dockerRow->addWidget(m_refreshDocker);
  dockerLayout->addLayout(dockerRow);
  bodyLayout->addWidget(dockerBox);
  connect(m_refreshDocker, &QPushButton::clicked, this,
          &ConnectionDialog::refreshContainers);
  connect(m_containerList, &QListWidget::itemClicked, this,
          &ConnectionDialog::onContainerPicked);
  connect(m_containerList, &QListWidget::itemActivated, this,
          &ConnectionDialog::onContainerPicked);

  auto *formHost = new QWidget(body);
  m_form = new QFormLayout(formHost);
  m_form->setContentsMargins(0, 0, 0, 0);
  m_form->setHorizontalSpacing(12);
  m_form->setVerticalSpacing(6);
  m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  m_name = new QLineEdit(formHost);
  m_name->setPlaceholderText(tr("e.g. Local SQL Server"));
  m_color = new QComboBox(formHost);
  m_color->addItem(tr("No colour"), QString());
  const QList<QPair<QString, QString>> colors = {
      {tr("Green (dev)"), "#3fb950"},
      {tr("Blue"), "#388bfd"},
      {tr("Amber (staging)"), "#d29922"},
      {tr("Red (production)"), "#f85149"},
      {tr("Purple"), "#a371f7"}};
  for (const auto &c : colors) {
    m_color->addItem(DbGlyphs::dot(QColor(c.second), 16), c.first, c.second);
  }
  auto *nameRow = new QHBoxLayout();
  nameRow->addWidget(m_name, 1);
  nameRow->addWidget(m_color);
  m_form->addRow(tr("Name"), wrapLayout(nameRow, formHost));

  m_engine = new QComboBox(formHost);
  for (DbEngine e : DbEngineInfo::allEngines()) {
    m_engine->addItem(DbEngineInfo::displayName(e), int(e));
  }
  m_form->addRow(tr("Database type"), m_engine);

  m_direct = new QRadioButton(tr("Direct (client on this machine)"), formHost);
  m_docker = new QRadioButton(tr("Inside a Docker container"), formHost);
  m_direct->setChecked(true);
  auto *transportRow = new QVBoxLayout();
  transportRow->setSpacing(2);
  transportRow->addWidget(m_direct);
  transportRow->addWidget(m_docker);
  m_transportRow = wrapLayout(transportRow, formHost);
  m_form->addRow(tr("Connect"), m_transportRow);

  m_container = new QComboBox(formHost);
  m_container->setEditable(true);
  m_container->setInsertPolicy(QComboBox::NoInsert);
  m_container->lineEdit()->setPlaceholderText(tr("container name or id"));
  m_form->addRow(tr("Container"), m_container);

  m_host = new QLineEdit(QStringLiteral("localhost"), formHost);
  m_port = new QSpinBox(formHost);
  m_port->setRange(0, 65535);
  // Ports are typed, not stepped through; arrow keys and the wheel still work.
  m_port->setButtonSymbols(QAbstractSpinBox::NoButtons);
  m_port->setFixedWidth(96);
  auto *hostRow = new QHBoxLayout();
  hostRow->addWidget(m_host, 1);
  hostRow->addWidget(new QLabel(tr("Port"), formHost));
  hostRow->addWidget(m_port);
  m_hostRow = wrapLayout(hostRow, formHost);
  m_form->addRow(tr("Host"), m_hostRow);

  m_user = new QLineEdit(formHost);
  m_form->addRow(tr("User"), m_user);

  m_password = new QLineEdit(formHost);
  m_password->setEchoMode(QLineEdit::Password);
  m_password->setPlaceholderText(tr("asked again in the next session"));
  m_form->addRow(tr("Password"), m_password);
  m_passwordHint = new QLabel(formHost);
  m_passwordHint->setObjectName("dbDialogHint");
  m_passwordHint->setWordWrap(true);
  m_passwordHint->setText(tr("Passwords are kept in memory for this session "
                             "only and are never saved."));
  m_form->addRow(QString(), m_passwordHint);

  m_database = new QLineEdit(formHost);
  m_database->setPlaceholderText(tr("optional"));
  m_form->addRow(tr("Database"), m_database);

  m_file = new QLineEdit(formHost);
  m_file->setPlaceholderText(tr("path to a .db / .sqlite file"));
  m_browse = new QPushButton(tr("Browse…"), formHost);
  m_newFile = new QPushButton(tr("New…"), formHost);
  auto *fileRow = new QHBoxLayout();
  fileRow->addWidget(m_file, 1);
  fileRow->addWidget(m_browse);
  fileRow->addWidget(m_newFile);
  m_fileRow = wrapLayout(fileRow, formHost);
  m_form->addRow(tr("File"), m_fileRow);

  m_readOnly = new QCheckBox(
      tr("Read-only (refuse statements that change data)"), formHost);
  m_confirmDestructive = new QCheckBox(
      tr("Ask before DROP, TRUNCATE and DELETE/UPDATE without WHERE"),
      formHost);
  m_confirmDestructive->setChecked(true);
  m_trustCertificate = new QCheckBox(
      tr("Trust the server certificate (typical for local containers)"),
      formHost);
  m_trustCertificate->setChecked(true);
  auto *optionsCol = new QVBoxLayout();
  optionsCol->setSpacing(2);
  optionsCol->addWidget(m_readOnly);
  optionsCol->addWidget(m_confirmDestructive);
  optionsCol->addWidget(m_trustCertificate);
  m_form->addRow(tr("Safety"), wrapLayout(optionsCol, formHost));

  m_clientPath = new QLineEdit(formHost);
  m_clientPath->setPlaceholderText(tr("default: psql / mysql / sqlcmd"));
  m_form->addRow(tr("Client program"), m_clientPath);
  m_extraArgs = new QLineEdit(formHost);
  m_extraArgs->setPlaceholderText(
      tr("extra command line arguments (advanced)"));
  m_form->addRow(tr("Extra arguments"), m_extraArgs);
  bodyLayout->addWidget(formHost);
  bodyLayout->addStretch(1);

  auto *testRow = new QHBoxLayout();
  m_testButton = new QPushButton(tr("Test connection"), this);
  m_testStatus = new QLabel(this);
  m_testStatus->setWordWrap(true);
  m_testStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  testRow->addWidget(m_testButton);
  testRow->addWidget(m_testStatus, 1);
  root->addLayout(testRow);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  auto *cancel = new QPushButton(tr("Cancel"), this);
  m_saveButton = new QPushButton(tr("Save"), this);
  m_saveConnectButton = new QPushButton(tr("Save && Connect"), this);
  buttons->addWidget(cancel);
  buttons->addWidget(m_saveButton);
  buttons->addWidget(m_saveConnectButton);
  root->addLayout(buttons);
  setKeyboardDefault(m_saveConnectButton);

  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_saveButton, &QPushButton::clicked, this, [this]() {
    m_connectAfterSave = false;
    accept();
  });
  connect(m_saveConnectButton, &QPushButton::clicked, this, [this]() {
    m_connectAfterSave = true;
    accept();
  });
  connect(m_testButton, &QPushButton::clicked, this,
          &ConnectionDialog::testConnection);
  connect(m_browse, &QPushButton::clicked, this, &ConnectionDialog::browseFile);
  connect(m_newFile, &QPushButton::clicked, this, &ConnectionDialog::newFile);
  connect(m_engine, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ConnectionDialog::onEngineChanged);
  connect(m_direct, &QRadioButton::toggled, this,
          &ConnectionDialog::onTransportChanged);
  connect(m_name, &QLineEdit::textEdited, this,
          [this]() { m_nameEdited = true; });
  connect(m_container->lineEdit(), &QLineEdit::textChanged, this,
          [this]() { updateAutoName(); });
  connect(m_host, &QLineEdit::textChanged, this,
          [this]() { updateAutoName(); });
  connect(m_file, &QLineEdit::textChanged, this,
          [this]() { updateAutoName(); });
}

void ConnectionDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  styleTitleLabel(findChild<QLabel *>("dbDialogTitle"));
  styleSubduedLabel(findChild<QLabel *>("dbDialogSubtitle"));
  styleSubduedLabel(m_passwordHint);
  styleSubduedLabel(m_dockerStatus);
  stylePrimaryButton(m_saveConnectButton);
  m_containerList->setIconSize(QSize(14, 14));
  // Keep the port box level with the host field next to it.
  m_host->ensurePolished();
  m_port->setFixedHeight(m_host->sizeHint().height());
}

void ConnectionDialog::setProfile(const DbConnectionProfile &p) {
  m_settingProfile = true;
  m_id = p.id;
  m_name->setText(p.name);
  m_nameEdited = !p.name.isEmpty();
  const int engineIndex = m_engine->findData(int(p.engine));
  m_engine->setCurrentIndex(engineIndex >= 0 ? engineIndex : 0);
  (p.transport == DbTransport::Docker ? m_docker : m_direct)->setChecked(true);
  m_container->setEditText(p.container);
  m_host->setText(p.host);
  m_port->setValue(p.port > 0 ? p.port : DbEngineInfo::defaultPort(p.engine));
  m_user->setText(p.user);
  m_database->setText(p.database);
  m_file->setText(p.filePath);
  m_readOnly->setChecked(p.readOnly);
  m_confirmDestructive->setChecked(p.confirmDestructive);
  m_trustCertificate->setChecked(p.trustServerCertificate);
  m_clientPath->setText(p.clientPath);
  m_extraArgs->setText(p.extraArgs);
  const int colorIndex = m_color->findData(p.color);
  m_color->setCurrentIndex(colorIndex >= 0 ? colorIndex : 0);
  m_settingProfile = false;
  updateVisibility();
}

DbConnectionProfile ConnectionDialog::profile() const {
  DbConnectionProfile p;
  p.id = m_id;
  p.name = m_name->text().trimmed();
  p.engine = static_cast<DbEngine>(m_engine->currentData().toInt());
  p.transport = m_docker->isChecked() && DbEngineInfo::supportsDocker(p.engine)
                    ? DbTransport::Docker
                    : DbTransport::Direct;
  p.container = m_container->currentText().trimmed();
  p.host = m_host->text().trimmed();
  p.port = m_port->value();
  p.user = m_user->text().trimmed();
  p.database = m_database->text().trimmed();
  p.filePath = m_file->text().trimmed();
  p.readOnly = m_readOnly->isChecked();
  p.confirmDestructive = m_confirmDestructive->isChecked();
  p.trustServerCertificate = m_trustCertificate->isChecked();
  p.clientPath = m_clientPath->text().trimmed();
  p.extraArgs = m_extraArgs->text().trimmed();
  p.color = m_color->currentData().toString();
  return p;
}

void ConnectionDialog::onEngineChanged() {
  if (m_engine->currentIndex() < 0) {
    return;
  }
  const DbEngine engine =
      static_cast<DbEngine>(m_engine->currentData().toInt());
  if (!m_settingProfile) {

    const bool userDefault = m_user->text().isEmpty();
    for (DbEngine e : DbEngineInfo::allEngines()) {
      if (m_user->text() == DbEngineInfo::defaultUser(e)) {
        m_user->setText(DbEngineInfo::defaultUser(engine));
      }
      if (m_database->text() == DbEngineInfo::defaultDatabase(e) &&
          !m_database->text().isEmpty()) {
        m_database->setText(DbEngineInfo::defaultDatabase(engine));
      }
    }
    if (userDefault) {
      m_user->setText(DbEngineInfo::defaultUser(engine));
    }
    if (m_database->text().isEmpty()) {
      m_database->setText(DbEngineInfo::defaultDatabase(engine));
    }
  }
  if (!m_settingProfile) {

    bool custom = m_port->value() != 0;
    for (DbEngine e : DbEngineInfo::allEngines()) {
      if (m_port->value() == DbEngineInfo::defaultPort(e)) {
        custom = false;
      }
    }
    if (!custom) {
      m_port->setValue(DbEngineInfo::defaultPort(engine));
    }
  }
  m_clientPath->setPlaceholderText(
      tr("default: %1").arg(DbEngineInfo::clientName(engine)));
  updateVisibility();
  updateAutoName();
}

void ConnectionDialog::onTransportChanged() {
  updateVisibility();
  updateAutoName();
}

void ConnectionDialog::updateVisibility() {
  const DbEngine engine =
      static_cast<DbEngine>(m_engine->currentData().toInt());
  const bool file = DbEngineInfo::usesFile(engine);
  const bool docker = !file && m_docker->isChecked();
  const bool server = !file;
  auto show = [this](QWidget *field, bool visible) {
    m_form->setRowVisible(field, visible);
  };
  show(m_transportRow, server);
  show(m_container, docker);
  show(m_hostRow, server && !docker);
  show(m_user, server);
  show(m_password, server);
  show(m_passwordHint, server);
  show(m_database, server);
  show(m_fileRow, file);
  m_trustCertificate->setVisible(engine == DbEngine::SqlServer);
  show(m_clientPath, server);
  show(m_extraArgs, server);
  m_dockerBox->setVisible(server);
  if (file && m_docker->isChecked()) {
    m_direct->setChecked(true);
  }
}

QString ConnectionDialog::suggestedName() const {
  const DbEngine engine =
      static_cast<DbEngine>(m_engine->currentData().toInt());
  QString where;
  if (DbEngineInfo::usesFile(engine)) {
    where = QFileInfo(m_file->text()).completeBaseName();
  } else if (m_docker->isChecked()) {
    where = m_container->currentText().trimmed();
  } else {
    where = m_host->text().trimmed();
  }
  const QString base = DbEngineInfo::displayName(engine);
  return where.isEmpty() ? base : QStringLiteral("%1 · %2").arg(base, where);
}

void ConnectionDialog::updateAutoName() {
  if (m_settingProfile || m_nameEdited) {
    return;
  }
  m_name->setText(suggestedName());
}

void ConnectionDialog::refreshContainers() {
  m_dockerStatus->setText(tr("Looking for database containers…"));
  m_discovery->refresh();
}

void ConnectionDialog::onContainersFound(
    const QVector<DockerContainer> &containers) {
  m_containers = containers;
  m_containerList->clear();
  m_container->clear();
  int databases = 0;
  const QColor good =
      m_theme.successColor.isValid() ? m_theme.successColor : QColor("#3fb950");
  const QColor idle =
      m_theme.borderColor.isValid() ? m_theme.borderColor : QColor("#888888");
  for (int i = 0; i < containers.size(); ++i) {
    const DockerContainer &c = containers[i];
    m_container->addItem(c.name);
    if (!c.isDatabase) {
      continue;
    }
    ++databases;
    auto *item =
        new QListWidgetItem(DbGlyphs::dot(c.running ? good : idle, 14),
                            QStringLiteral("%1   ·   %2   ·   %3")
                                .arg(c.name, engineBadge(c.engine), c.status),
                            m_containerList);
    item->setData(Qt::UserRole, i);
    item->setToolTip(c.image);
    if (!c.running) {
      item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
  }
  m_containerList->setVisible(databases > 0);
  m_dockerStatus->setText(
      databases > 0
          ? tr("%n database container(s) found. Click one to fill in the form.",
               nullptr, databases)
          : tr("No running database containers found (SQL Server, PostgreSQL, "
               "MySQL/MariaDB)."));
}

void ConnectionDialog::onContainerPicked() {
  QListWidgetItem *item = m_containerList->currentItem();
  if (!item) {
    return;
  }
  const int index = item->data(Qt::UserRole).toInt();
  if (index < 0 || index >= m_containers.size()) {
    return;
  }
  const DockerContainer c = m_containers[index];
  const DbConnectionProfile suggested = DockerDiscoveryParsing::profileFor(c);
  m_settingProfile = true;
  m_engine->setCurrentIndex(m_engine->findData(int(c.engine)));
  m_docker->setChecked(true);
  m_container->setEditText(c.name);
  m_user->setText(suggested.user);
  m_database->setText(suggested.database);
  m_port->setValue(c.hostPort > 0 ? c.hostPort
                                  : DbEngineInfo::defaultPort(c.engine));
  m_settingProfile = false;
  m_nameEdited = false;
  updateVisibility();
  updateAutoName();
  if (m_name->text().isEmpty() || !m_nameEdited) {
    m_name->setText(c.name);
  }
  m_testStatus->clear();
  m_scroll->ensureWidgetVisible(m_password, 0, 60);
  m_discovery->inspect(c);
}

void ConnectionDialog::onCredentials(const QString &container,
                                     const DockerCredentialHints &hints) {
  if (m_container->currentText().trimmed() != container) {
    return;
  }
  if (!hints.user.isEmpty()) {
    m_user->setText(hints.user);
  }
  if (!hints.database.isEmpty()) {
    m_database->setText(hints.database);
  }
  if (hints.hasPassword() && m_password->text().isEmpty()) {
    m_password->setText(hints.password);
    m_passwordHint->setText(tr("Filled in from the container's environment. It "
                               "stays in memory for this session only."));
  }
}

void ConnectionDialog::browseFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open SQLite database"), m_file->text(),
      tr("SQLite databases (*.db *.sqlite *.sqlite3 *.db3 *.s3db);;All files "
         "(*)"));
  if (!path.isEmpty()) {
    m_file->setText(path);
  }
}

void ConnectionDialog::newFile() {
  QString path = QFileDialog::getSaveFileName(
      this, tr("Create SQLite database"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
          "/database.db",
      tr("SQLite databases (*.db *.sqlite *.sqlite3);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  if (!QFileInfo::exists(path)) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
      ThemedMessageBox::warning(
          this, tr("Create database"),
          tr("Could not create %1: %2").arg(path, f.errorString()));
      return;
    }
  }
  m_file->setText(path);
}

void ConnectionDialog::setTestStatus(const QString &text, bool ok, bool busy) {
  const QColor color = busy ? UIStyleHelper::mutedTextColor(m_theme)
                            : (ok ? m_theme.successColor : m_theme.errorColor);
  m_testStatus->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
  m_testStatus->setText(text);
}

void ConnectionDialog::testConnection() {
  QString why;
  DbConnectionProfile p = profile();
  if (p.name.isEmpty()) {
    p.name = QStringLiteral("test");
  }
  if (!p.isValid(&why)) {
    setTestStatus(why, false);
    return;
  }
  m_testConnection = std::make_unique<DbConnection>(p);
  DbConnection *conn = m_testConnection.get();
  m_testButton->setEnabled(false);
  setTestStatus(tr("Connecting…"), true, true);

  QPointer<ConnectionDialog> self(this);
  connect(
      conn, &DbConnection::stateChanged, this,
      [this, conn, p](DbConnectionState s) {
        if (s == DbConnectionState::Failed) {
          setTestStatus(conn->lastError(), false);
          m_testButton->setEnabled(true);
        } else if (s == DbConnectionState::Connected) {
          const quint64 id = conn->execute({versionQuery(p.engine)});
          auto *conn1 = new QMetaObject::Connection;
          *conn1 = connect(
              conn, &DbConnection::statementFinished, this,
              [this, id, conn1](quint64 rid, int, const DbStatementResult &r) {
                if (rid != id) {
                  return;
                }
                disconnect(*conn1);
                delete conn1;
                QString version;
                if (r.ok && !r.resultSets.isEmpty() &&
                    !r.resultSets[0].rows.isEmpty()) {
                  version = r.resultSets[0]
                                .rows[0]
                                .value(0)
                                .toString()
                                .section('\n', 0, 0)
                                .simplified();
                }
                setTestStatus(version.isEmpty()
                                  ? tr("Connected.")
                                  : tr("Connected — %1").arg(version.left(110)),
                              true);
                m_testButton->setEnabled(true);
                if (m_testConnection) {
                  m_testConnection->disconnectFromServer();
                }
              });
        }
      });
  conn->connectToServer(m_password->text());
}

void ConnectionDialog::accept() {
  QString why;
  DbConnectionProfile p = profile();
  if (!p.isValid(&why)) {
    setTestStatus(why, false);
    return;
  }
  if (m_uniqueName) {

    const QString unique = m_uniqueName(p.name);
    if (unique != p.name) {
      m_name->setText(unique);
    }
  }
  if (m_testConnection) {
    m_testConnection->disconnectFromServer();
    m_testConnection.reset();
  }
  QDialog::accept();
}
