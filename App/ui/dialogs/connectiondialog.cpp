#include "connectiondialog.h"

#include "../../database/dbconnectionurl.h"
#include "../../database/dbtypes.h"
#include "../panels/dbglyphs.h"
#include "themedmessagebox.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int KindRole = Qt::UserRole;
constexpr int PayloadRole = Qt::UserRole + 1;
constexpr int SearchRole = Qt::UserRole + 2;

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
    return QStringLiteral("SELECT CONCAT(@@version_comment, ' ', VERSION())");
  case DbEngine::SqlServer:
    return QStringLiteral("SELECT @@VERSION");
  }
  return {};
}

QString badgeText(const DbConnectionProfile &p) {
  QString text = DbEngineInfo::displayName(p.engine);
  if (p.engine == DbEngine::Sqlite) {
    return p.filePath == QLatin1String(":memory:")
               ? text + QStringLiteral(" · in memory")
               : text + QStringLiteral(" · file");
  }
  if (p.transport == DbTransport::Docker) {
    text += QStringLiteral(" · Docker");
  }
  return text;
}

// One line describing how Lightpad will reach the database.
QString reachSummary(const DbConnectionProfile &p,
                     const QString &hostPlaceholder = QString()) {
  if (p.engine == DbEngine::Sqlite) {
    return p.filePath == QLatin1String(":memory:")
               ? QObject::tr("A new empty database in memory")
               : (p.filePath.isEmpty() ? QObject::tr("Choose a file")
                                       : p.filePath);
  }
  if (p.transport == DbTransport::Docker) {
    QString line =
        QStringLiteral("docker exec %1 %2")
            .arg(p.container.isEmpty() ? QStringLiteral("<container>")
                                       : p.container,
                 DbEngineInfo::clientName(p.engine));
    if (!p.user.isEmpty()) {
      line += QStringLiteral(" · ") + p.user;
    }
    if (!p.database.isEmpty()) {
      line += QStringLiteral(" · ") + p.database;
    }
    return line;
  }
  DbConnectionProfile shown = p;
  if (shown.host.isEmpty() && !hostPlaceholder.isEmpty()) {
    shown.host = hostPlaceholder;
  }
  QString url = DbConnectionUrl::format(shown);
  url.replace(QLatin1String("%3C"), QLatin1String("<"));
  url.replace(QLatin1String("%3E"), QLatin1String(">"));
  return url;
}

QString projectName(const QString &folder) {
  return folder.isEmpty() ? QString() : QDir(folder).dirName();
}

} // namespace

ConnectionDialog::ConnectionDialog(QWidget *parent) : StyledDialog(parent) {
  setWindowTitle(tr("Database Connection"));
  setMinimumSize(760, 540);
  resize(880, 640);

  m_discovery = new DockerDiscovery(this);
  connect(m_discovery, &DockerDiscovery::containersFound, this,
          &ConnectionDialog::onContainersFound);
  connect(m_discovery, &DockerDiscovery::failed, this,
          [this](const QString &message) {
            m_dockerFailed = true;
            m_dockerMessage = message;
            m_containers.clear();
            populateChooser();
            updateContainerChoices();
          });
  connect(m_discovery, &DockerDiscovery::credentialsFound, this,
          &ConnectionDialog::onCredentials);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(16, 14, 16, 12);
  root->setSpacing(10);
  m_pages = new QStackedWidget(this);
  root->addWidget(m_pages, 1);
  buildChooser();
  buildForm();
  m_pages->addWidget(m_chooserPage);
  m_pages->addWidget(m_formPage);

  DbConnectionTemplateCatalog::instance().reload();
  populateChooser();
  showChooser();
}

ConnectionDialog::~ConnectionDialog() {
  disconnect(m_testResultConnection);
  if (m_testConnection) {
    m_testConnection->disconnect(this);
  }
}

// --- Chooser -----------------------------------------------------------

void ConnectionDialog::buildChooser() {
  m_chooserPage = new QWidget(this);
  auto *layout = new QVBoxLayout(m_chooserPage);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  auto *title = new QLabel(tr("New Database Connection"), m_chooserPage);
  title->setObjectName("dbDialogTitle");
  layout->addWidget(title);
  auto *subtitle = new QLabel(
      tr("Start from something found in your project, a running container or "
         "a template — or paste a connection URL."),
      m_chooserPage);
  subtitle->setObjectName("dbDialogSubtitle");
  subtitle->setWordWrap(true);
  layout->addWidget(subtitle);

  auto *urlRow = new QHBoxLayout();
  m_chooserUrl = new QLineEdit(m_chooserPage);
  m_chooserUrl->setObjectName("dbChooserUrl");
  m_chooserUrl->setPlaceholderText(
      tr("Paste a connection URL: postgresql://user@host:5432/db, "
         "mysql://…, sqlserver://…, sqlite:///app.db"));
  m_chooserUrl->setClearButtonEnabled(true);
  m_chooserUrl->setAccessibleName(tr("Connection URL"));
  m_chooserUrl->installEventFilter(this);
  m_chooserUseUrl = new QPushButton(tr("Use URL"), m_chooserPage);
  m_chooserUseUrl->setEnabled(false);
  urlRow->addWidget(m_chooserUrl, 1);
  urlRow->addWidget(m_chooserUseUrl);
  layout->addLayout(urlRow);
  m_chooserUrlStatus = new QLabel(m_chooserPage);
  m_chooserUrlStatus->setWordWrap(true);
  m_chooserUrlStatus->setVisible(false);
  layout->addWidget(m_chooserUrlStatus);

  m_search = new QLineEdit(m_chooserPage);
  m_search->setObjectName("dbChooserSearch");
  m_search->setPlaceholderText(
      tr("Search (PostgreSQL, Docker, Supabase, SQLite, …)"));
  m_search->setClearButtonEnabled(true);
  m_search->setAccessibleName(tr("Search connection templates"));
  m_search->installEventFilter(this);
  layout->addWidget(m_search);

  auto *splitter = new QSplitter(Qt::Horizontal, m_chooserPage);
  splitter->setChildrenCollapsible(false);
  m_choices = new QTreeWidget(splitter);
  m_choices->setObjectName("dbChoices");
  m_choices->setHeaderHidden(true);
  m_choices->setRootIsDecorated(false);
  m_choices->setIndentation(12);
  m_choices->setUniformRowHeights(true);
  m_choices->setMinimumWidth(300);
  m_choices->setAccessibleName(tr("Connection templates"));

  auto *details = new QWidget(splitter);
  auto *detailsLayout = new QVBoxLayout(details);
  detailsLayout->setContentsMargins(14, 2, 4, 2);
  detailsLayout->setSpacing(8);
  auto *titleRow = new QHBoxLayout();
  m_choiceTitle = new QLabel(details);
  m_choiceTitle->setWordWrap(true);
  m_choiceBadge = new QLabel(details);
  titleRow->addWidget(m_choiceTitle, 1);
  titleRow->addWidget(m_choiceBadge, 0, Qt::AlignTop);
  detailsLayout->addLayout(titleRow);
  m_choiceDescription = new QLabel(details);
  m_choiceDescription->setWordWrap(true);
  detailsLayout->addWidget(m_choiceDescription);
  m_choicePreview = new QLabel(details);
  m_choicePreview->setObjectName("dbChoicePreview");
  m_choicePreview->setWordWrap(true);
  m_choicePreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
  detailsLayout->addWidget(m_choicePreview);
  m_choiceNote = new QLabel(details);
  m_choiceNote->setWordWrap(true);
  m_choiceNote->setTextFormat(Qt::RichText);
  m_choiceNote->setOpenExternalLinks(true);
  detailsLayout->addWidget(m_choiceNote);
  detailsLayout->addStretch();
  splitter->addWidget(m_choices);
  splitter->addWidget(details);
  splitter->setStretchFactor(0, 2);
  splitter->setStretchFactor(1, 3);
  layout->addWidget(splitter, 1);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  auto *cancel = new QPushButton(tr("Cancel"), m_chooserPage);
  m_continueButton = new QPushButton(tr("Continue"), m_chooserPage);
  m_continueButton->setObjectName("dbChooserContinue");
  buttons->addWidget(cancel);
  buttons->addWidget(m_continueButton);
  layout->addLayout(buttons);

  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_continueButton, &QPushButton::clicked, this,
          &ConnectionDialog::chooseCurrent);
  connect(m_search, &QLineEdit::textChanged, this,
          &ConnectionDialog::filterChooser);
  connect(m_choices, &QTreeWidget::currentItemChanged, this,
          &ConnectionDialog::updateChoiceDetails);
  connect(m_choices, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item) {
            if (item && item->data(0, KindRole).toInt() != 0) {
              chooseCurrent();
            }
          });
  connect(m_chooserUrl, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            const bool any = !text.trimmed().isEmpty();
            QString error;
            DbConnectionUrl::Parsed parsed;
            const bool ok = any && DbConnectionUrl::parse(text, &parsed, &error,
                                                          m_workspaceFolder);
            m_chooserUseUrl->setEnabled(ok);
            m_chooserUrlStatus->setVisible(any);
            if (ok) {
              m_chooserUrlStatus->setText(
                  tr("%1 — press Enter or Use URL to continue.")
                      .arg(badgeText(parsed.profile)));
              styleToneLabel(m_chooserUrlStatus, UIStyleHelper::Tone::Success);
            } else if (any) {
              m_chooserUrlStatus->setText(error);
              styleToneLabel(m_chooserUrlStatus, UIStyleHelper::Tone::Warning);
            }
          });
  connect(m_chooserUseUrl, &QPushButton::clicked, this,
          &ConnectionDialog::useChooserUrl);
}

void ConnectionDialog::populateChooser() {
  const QString current =
      m_choices->currentItem()
          ? m_choices->currentItem()->data(0, PayloadRole).toString()
          : QString();
  m_choices->clear();
  m_dockerGroup = nullptr;
  const QColor muted = UIStyleHelper::mutedTextColor(m_theme);
  const QColor accent =
      m_theme.accentColor.isValid() ? m_theme.accentColor : QColor("#4aa3ff");

  auto addGroup = [this](const QString &title) {
    auto *group = new QTreeWidgetItem(m_choices, {title});
    group->setFlags(Qt::ItemIsEnabled);
    QFont font = group->font(0);
    font.setBold(true);
    group->setFont(0, font);
    group->setExpanded(true);
    return group;
  };
  auto addChoice = [](QTreeWidgetItem *group, const QString &text,
                      ChoiceKind kind, const QString &payload,
                      const QString &search, const QIcon &icon) {
    auto *item = new QTreeWidgetItem(group, {text});
    item->setData(0, KindRole, int(kind));
    item->setData(0, PayloadRole, payload);
    item->setData(0, SearchRole, search + QLatin1Char(' ') + text);
    item->setIcon(0, icon);
    return item;
  };

  if (!m_detected.isEmpty()) {
    QTreeWidgetItem *group = addGroup(tr("Found in this project"));
    for (int i = 0; i < m_detected.size(); ++i) {
      const DbDetectedConnection &entry = m_detected.at(i);
      const QString text =
          entry.profile.engine == DbEngine::Sqlite
              ? entry.source
              : QStringLiteral("%1  ·  %2").arg(entry.title, entry.source);
      QTreeWidgetItem *item =
          addChoice(group, text, ChoiceKind::Detected, QString::number(i),
                    badgeText(entry.profile) + QLatin1Char(' ') + entry.source,
                    DbGlyphs::icon(DbGlyphs::Glyph::Database, accent, 14));
      const QString saved = savedNameFor(entry.profile);
      if (!saved.isEmpty()) {
        item->setForeground(0, muted);
        item->setText(0, tr("%1  (saved as %2)").arg(text, saved));
      }
    }
  }

  if (!m_dockerFailed || m_dockerSearched) {
    QList<int> databases;
    for (int i = 0; i < m_containers.size(); ++i) {
      if (m_containers.at(i).isDatabase) {
        databases << i;
      }
    }
    if (!m_dockerFailed) {
      m_dockerGroup = addGroup(tr("Running in Docker"));
      if (!m_dockerSearched) {
        auto *hint =
            new QTreeWidgetItem(m_dockerGroup, {tr("Looking for containers…")});
        hint->setFlags(Qt::NoItemFlags);
        hint->setForeground(0, muted);
      } else if (databases.isEmpty()) {
        auto *hint = new QTreeWidgetItem(
            m_dockerGroup, {tr("No database containers are running")});
        hint->setFlags(Qt::NoItemFlags);
        hint->setForeground(0, muted);
      }
      const QColor good = m_theme.successColor.isValid() ? m_theme.successColor
                                                         : QColor("#3fb950");
      for (int index : databases) {
        const DockerContainer &c = m_containers.at(index);
        addChoice(m_dockerGroup,
                  QStringLiteral("%1  ·  %2")
                      .arg(c.name, DbEngineInfo::displayName(c.engine)),
                  ChoiceKind::Container, QString::number(index),
                  c.image + QStringLiteral(" docker container"),
                  DbGlyphs::dot(c.running ? good : muted, 12));
      }
    }
  }

  const DbConnectionTemplateCatalog &catalog =
      DbConnectionTemplateCatalog::instance();
  for (const QString &category : catalog.categories()) {
    QTreeWidgetItem *group = nullptr;
    for (const DbConnectionTemplate &entry : catalog.templates()) {
      if (entry.category != category) {
        continue;
      }
      if (!group) {
        group = addGroup(category);
      }
      addChoice(group, entry.name, ChoiceKind::Template, entry.id,
                entry.searchText(),
                DbGlyphs::icon(DbGlyphs::Glyph::Database, muted, 14));
    }
  }
  m_choices->expandAll();
  filterChooser(m_search->text());
  if (!current.isEmpty()) {
    QTreeWidgetItemIterator it(m_choices);
    while (*it) {
      if ((*it)->data(0, PayloadRole).toString() == current &&
          !(*it)->isHidden()) {
        m_choices->setCurrentItem(*it);
        return;
      }
      ++it;
    }
  }
}

void ConnectionDialog::filterChooser(const QString &text) {
  const QStringList words =
      text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (int i = 0; i < m_choices->topLevelItemCount(); ++i) {
    QTreeWidgetItem *group = m_choices->topLevelItem(i);
    bool anyVisible = false;
    for (int j = 0; j < group->childCount(); ++j) {
      QTreeWidgetItem *item = group->child(j);
      if (item->data(0, KindRole).toInt() == 0) {
        item->setHidden(!words.isEmpty());
        anyVisible = anyVisible || words.isEmpty();
        continue;
      }
      const QString haystack = item->data(0, SearchRole).toString();
      bool matches = true;
      for (const QString &word : words) {
        matches = matches && haystack.contains(word, Qt::CaseInsensitive);
      }
      item->setHidden(!matches);
      anyVisible = anyVisible || matches;
    }
    group->setHidden(!anyVisible);
  }
  QTreeWidgetItem *current = m_choices->currentItem();
  if (!current || current->isHidden() ||
      current->data(0, KindRole).toInt() == 0 ||
      (current->parent() && current->parent()->isHidden())) {
    selectFirstChoice();
  }
}

void ConnectionDialog::selectFirstChoice() {
  QTreeWidgetItemIterator it(m_choices);
  while (*it) {
    QTreeWidgetItem *item = *it;
    if (item->data(0, KindRole).toInt() != 0 && !item->isHidden() &&
        !(item->parent() && item->parent()->isHidden())) {
      m_choices->setCurrentItem(item);
      return;
    }
    ++it;
  }
  m_choices->setCurrentItem(nullptr);
  updateChoiceDetails();
}

void ConnectionDialog::updateChoiceDetails() {
  QTreeWidgetItem *item = m_choices->currentItem();
  const auto kind =
      static_cast<ChoiceKind>(item ? item->data(0, KindRole).toInt() : 0);
  const QString payload = item ? item->data(0, PayloadRole).toString() : "";
  m_continueButton->setEnabled(item && int(kind) != 0);
  QString title;
  QString description;
  QString preview;
  QString note;
  DbConnectionProfile profile;
  switch (kind) {
  case ChoiceKind::Template: {
    const DbConnectionTemplate entry =
        DbConnectionTemplateCatalog::instance().templateById(payload);
    profile = entry.profile;
    title = entry.name;
    description = entry.description;
    preview = reachSummary(entry.profile, entry.hostPlaceholder);
    note = entry.help.toHtmlEscaped();
    if (!entry.docsUrl.isEmpty()) {
      note += (note.isEmpty() ? QString() : QStringLiteral("<br/>")) +
              linkHtml(entry.docsUrl, tr("Open the provider's guide"));
    }
    break;
  }
  case ChoiceKind::Detected: {
    const DbDetectedConnection entry = m_detected.value(payload.toInt());
    profile = entry.profile;
    title = entry.title;
    description = tr("Read from %1 in this project.").arg(entry.source);
    preview = reachSummary(entry.profile);
    QStringList notes;
    if (entry.hasPassword) {
      notes << tr("Its password is filled in for this session only; Lightpad "
                  "never saves passwords.");
    }
    const QString saved = savedNameFor(entry.profile);
    if (!saved.isEmpty()) {
      notes << tr("Already saved as \"%1\".").arg(saved);
    }
    note = notes.join(QStringLiteral("<br/>")).toHtmlEscaped();
    note.replace(QLatin1String("&lt;br/&gt;"), QLatin1String("<br/>"));
    break;
  }
  case ChoiceKind::Container: {
    const DockerContainer c = m_containers.value(payload.toInt());
    profile = DockerDiscoveryParsing::profileFor(c);
    title = c.name;
    description = tr("%1 — %2").arg(c.image, c.status);
    preview = reachSummary(profile);
    note = tr("Runs %1 inside the container, so nothing has to be installed "
              "here. The user, database and password come from the "
              "container's environment when it defines them.")
               .arg(DbEngineInfo::clientName(c.engine))
               .toHtmlEscaped();
    break;
  }
  }
  if (int(kind) == 0) {
    m_choiceTitle->setText(tr("Nothing matches the search."));
    m_choiceBadge->setVisible(false);
    m_choiceDescription->clear();
    m_choicePreview->setVisible(false);
    m_choiceNote->clear();
    return;
  }
  m_choiceTitle->setText(
      QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped()));
  m_choiceBadge->setText(badgeText(profile));
  m_choiceBadge->setVisible(true);
  styleBadge(m_choiceBadge, UIStyleHelper::Tone::Accent);
  m_choiceDescription->setText(description);
  m_choicePreview->setText(preview);
  m_choicePreview->setVisible(!preview.isEmpty());
  m_choiceNote->setText(note);
}

void ConnectionDialog::chooseCurrent() {
  QTreeWidgetItem *item = m_choices->currentItem();
  if (!item) {
    return;
  }
  const auto kind = static_cast<ChoiceKind>(item->data(0, KindRole).toInt());
  const QString payload = item->data(0, PayloadRole).toString();
  switch (kind) {
  case ChoiceKind::Template:
    startFromTemplate(payload);
    break;
  case ChoiceKind::Detected: {
    const DbDetectedConnection entry = m_detected.value(payload.toInt());
    clearTemplateHints();
    m_templateName.clear();
    DbConnectionProfile profile = entry.profile;
    const QString project = projectName(m_workspaceFolder);
    profile.name =
        entry.profile.engine == DbEngine::Sqlite
            ? QFileInfo(entry.profile.filePath).fileName()
            : (project.isEmpty()
                   ? entry.title
                   : QStringLiteral("%1 · %2").arg(project, entry.title));
    loadProfile(profile, entry.password, entry.hasPassword);
    m_nameEdited = true;
    m_help->setText(tr("Filled in from %1.").arg(entry.source));
    m_help->setVisible(true);
    showForm();
    break;
  }
  case ChoiceKind::Container: {
    const DockerContainer c = m_containers.value(payload.toInt());
    clearTemplateHints();
    m_templateName.clear();
    DbConnectionProfile profile = DockerDiscoveryParsing::profileFor(c);
    loadProfile(profile);
    m_nameEdited = true;
    m_help->setText(
        tr("Reading the credentials from %1's environment…").arg(c.name));
    m_help->setVisible(true);
    showForm();
    m_discovery->inspect(c);
    break;
  }
  default:
    break;
  }
}

void ConnectionDialog::useChooserUrl() {
  QString error;
  if (applyConnectionString(m_chooserUrl->text(), &error)) {
    showForm();
  } else {
    m_chooserUrlStatus->setText(error);
    m_chooserUrlStatus->setVisible(true);
  }
}

void ConnectionDialog::showChooser() {
  m_pages->setCurrentWidget(m_chooserPage);
  setKeyboardDefault(m_continueButton);
  if (!m_dockerSearched && !m_dockerFailed) {
    refreshContainers();
  }
  m_search->setFocus();
}

void ConnectionDialog::showForm() {
  m_pages->setCurrentWidget(m_formPage);
  m_backButton->setVisible(!m_editing);
  setKeyboardDefault(m_saveConnectButton);
  updateVisibility();
  updateHeader();
  if (m_name->text().isEmpty()) {
    m_name->setFocus();
  } else if (m_engine->currentData().toInt() == int(DbEngine::Sqlite) &&
             m_file->text().isEmpty()) {
    m_file->setFocus();
  } else if (m_host->isVisible() && m_host->text().isEmpty()) {
    m_host->setFocus();
  } else if (m_password->isVisible() && m_password->isEnabled() &&
             m_password->text().isEmpty()) {
    m_password->setFocus();
  }
}

QString ConnectionDialog::linkHtml(const QString &url,
                                   const QString &text) const {
  const QColor color =
      m_theme.accentColor.isValid() ? m_theme.accentColor : QColor("#4aa3ff");
  return QStringLiteral("<a href=\"%1\" style=\"color:%2;\">%3</a>")
      .arg(url.toHtmlEscaped(), color.name(), text.toHtmlEscaped());
}

bool ConnectionDialog::isChoosing() const {
  return m_pages->currentWidget() == m_chooserPage;
}

// --- Form --------------------------------------------------------------

void ConnectionDialog::buildForm() {
  m_formPage = new QWidget(this);
  auto *layout = new QVBoxLayout(m_formPage);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto *header = new QHBoxLayout();
  m_backButton = new QPushButton(tr("← All templates"), m_formPage);
  m_backButton->setObjectName("dbBackToChooser");
  m_backButton->setToolTip(tr("Start from a different template"));
  m_backButton->setCursor(Qt::PointingHandCursor);
  m_formTitle = new QLabel(m_formPage);
  m_formTitle->setObjectName("dbDialogTitle");
  m_formBadge = new QLabel(m_formPage);
  header->addWidget(m_backButton);
  header->addWidget(m_formTitle);
  header->addWidget(m_formBadge);
  header->addStretch();
  layout->addLayout(header);

  m_help = new QLabel(m_formPage);
  m_help->setObjectName("dbDialogHelp");
  m_help->setWordWrap(true);
  m_help->setTextFormat(Qt::RichText);
  m_help->setOpenExternalLinks(true);
  m_help->setVisible(false);
  layout->addWidget(m_help);

  auto *scroll = new QScrollArea(m_formPage);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *body = new QWidget(scroll);
  scroll->setWidget(body);
  // setWidget turns background filling on; the form sits on the dialog.
  body->setAutoFillBackground(false);
  scroll->viewport()->setAutoFillBackground(false);
  layout->addWidget(scroll, 1);

  m_form = new QFormLayout(body);
  m_form->setContentsMargins(0, 4, 10, 4);
  m_form->setHorizontalSpacing(14);
  m_form->setVerticalSpacing(8);
  m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

  m_url = new QLineEdit(body);
  m_url->setObjectName("dbConnectionUrl");
  m_url->setPlaceholderText(tr("postgresql://user@host:5432/database"));
  m_url->setAccessibleName(tr("Connection URL"));
  m_copyUrl = new QPushButton(tr("Copy"), body);
  m_copyUrl->setToolTip(tr("Copy the URL (without the password)"));
  auto *urlRow = new QHBoxLayout();
  urlRow->addWidget(m_url, 1);
  urlRow->addWidget(m_copyUrl);
  m_form->addRow(tr("Connection URL"), wrapLayout(urlRow, body));
  m_urlHint = new QLabel(
      tr("Paste a URL or connection string here to fill in the fields below."),
      body);
  m_urlHint->setObjectName("dbDialogHint");
  m_urlHint->setWordWrap(true);
  m_form->addRow(QString(), m_urlHint);

  m_name = new QLineEdit(body);
  m_name->setObjectName("dbConnectionName");
  m_name->setPlaceholderText(tr("A name for the connection"));
  m_color = new QComboBox(body);
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
  m_color->setToolTip(tr("Colours the connection everywhere it is shown, so "
                         "production stands out"));
  auto *nameRow = new QHBoxLayout();
  nameRow->addWidget(m_name, 1);
  nameRow->addWidget(m_color);
  m_form->addRow(tr("Name"), wrapLayout(nameRow, body));

  m_engine = new QComboBox(body);
  m_engine->setObjectName("dbEngine");
  for (DbEngine e : {DbEngine::PostgreSql, DbEngine::MySql, DbEngine::SqlServer,
                     DbEngine::Sqlite}) {
    m_engine->addItem(DbEngineInfo::displayName(e), int(e));
  }
  m_form->addRow(tr("Type"), m_engine);

  m_direct = new QRadioButton(tr("This machine"), body);
  m_direct->setToolTip(tr("Connect with the client program installed here"));
  m_docker = new QRadioButton(tr("Inside a Docker container"), body);
  m_docker->setToolTip(tr("Run the client inside the container with docker "
                          "exec; nothing has to be installed or published"));
  m_direct->setChecked(true);
  auto *transportRow = new QHBoxLayout();
  transportRow->setSpacing(18);
  transportRow->addWidget(m_direct);
  transportRow->addWidget(m_docker);
  transportRow->addStretch();
  m_transportRow = wrapLayout(transportRow, body);
  m_form->addRow(tr("Connect from"), m_transportRow);

  m_container = new QComboBox(body);
  m_container->setObjectName("dbContainer");
  m_container->setEditable(true);
  m_container->setInsertPolicy(QComboBox::NoInsert);
  m_container->lineEdit()->setPlaceholderText(tr("container name or id"));
  m_refreshContainers = new QPushButton(tr("Refresh"), body);
  m_refreshContainers->setToolTip(tr("Look for running containers again"));
  auto *containerRow = new QHBoxLayout();
  containerRow->addWidget(m_container, 1);
  containerRow->addWidget(m_refreshContainers);
  m_containerRow = wrapLayout(containerRow, body);
  m_form->addRow(tr("Container"), m_containerRow);

  m_host = new QLineEdit(body);
  m_host->setObjectName("dbHost");
  m_port = new QSpinBox(body);
  m_port->setObjectName("dbPort");
  m_port->setRange(0, 65535);
  m_port->setSpecialValueText(tr("default"));
  m_port->setButtonSymbols(QAbstractSpinBox::NoButtons);
  m_port->setFixedWidth(90);
  auto *hostRow = new QHBoxLayout();
  hostRow->addWidget(m_host, 1);
  hostRow->addWidget(new QLabel(tr("Port"), body));
  hostRow->addWidget(m_port);
  m_hostRow = wrapLayout(hostRow, body);
  m_form->addRow(tr("Host"), m_hostRow);

  m_file = new QLineEdit(body);
  m_file->setObjectName("dbSqliteFile");
  m_file->setPlaceholderText(tr("path to a .db / .sqlite file"));
  m_browse = new QPushButton(tr("Browse…"), body);
  m_browse->setObjectName("dbBrowseSqliteFile");
  m_browse->setToolTip(tr("Open an existing SQLite database"));
  m_newFile = new QPushButton(tr("New…"), body);
  m_newFile->setObjectName("dbNewSqliteFile");
  m_newFile->setToolTip(tr("Create a new SQLite database file"));
  auto *fileRow = new QHBoxLayout();
  fileRow->addWidget(m_file, 1);
  fileRow->addWidget(m_browse);
  fileRow->addWidget(m_newFile);
  m_fileRow = wrapLayout(fileRow, body);
  m_form->addRow(tr("File"), m_fileRow);

  m_database = new QLineEdit(body);
  m_database->setObjectName("dbDatabase");
  m_form->addRow(tr("Database"), m_database);

  m_user = new QLineEdit(body);
  m_user->setObjectName("dbUser");
  m_form->addRow(tr("User"), m_user);

  m_password = new QLineEdit(body);
  m_password->setObjectName("dbPassword");
  m_password->setEchoMode(QLineEdit::Password);
  m_passwordSource = new QComboBox(body);
  m_passwordSource->setObjectName("dbPasswordSource");
  m_passwordSource->addItem(tr("Ask once per session"),
                            int(DbPasswordSource::Ask));
  m_passwordSource->addItem(tr("No password"), int(DbPasswordSource::None));
  m_passwordSource->addItem(tr("From an environment variable"),
                            int(DbPasswordSource::Environment));
  m_passwordEnv = new QLineEdit(body);
  m_passwordEnv->setObjectName("dbPasswordEnv");
  m_passwordEnv->setPlaceholderText(tr("e.g. PGPASSWORD"));
  m_passwordEnv->setMaximumWidth(180);
  auto *passwordRow = new QHBoxLayout();
  passwordRow->addWidget(m_password, 1);
  passwordRow->addWidget(m_passwordEnv, 1);
  passwordRow->addWidget(m_passwordSource);
  m_passwordRow = wrapLayout(passwordRow, body);
  m_form->addRow(tr("Password"), m_passwordRow);
  m_passwordHint = new QLabel(body);
  m_passwordHint->setObjectName("dbDialogHint");
  m_passwordHint->setWordWrap(true);
  m_form->addRow(QString(), m_passwordHint);

  m_sslMode = new QComboBox(body);
  m_sslMode->setObjectName("dbSslMode");
  m_form->addRow(tr("Encryption"), m_sslMode);
  m_trustCertificate =
      new QCheckBox(tr("Trust the server certificate (self-signed, local "
                       "servers and containers)"),
                    body);
  m_trustCertificate->setChecked(true);
  m_form->addRow(tr("Certificate"), m_trustCertificate);

  m_readOnly =
      new QCheckBox(tr("Read-only — refuse statements that change data"), body);
  m_confirmDestructive = new QCheckBox(
      tr("Ask before DROP, TRUNCATE and DELETE/UPDATE without WHERE"), body);
  m_confirmDestructive->setChecked(true);
  auto *safety = new QVBoxLayout();
  safety->setSpacing(4);
  safety->addWidget(m_readOnly);
  safety->addWidget(m_confirmDestructive);
  m_safetyRow = wrapLayout(safety, body);
  m_form->addRow(tr("Safety"), m_safetyRow);

  m_advancedToggle = new QToolButton(body);
  m_advancedToggle->setText(tr("Advanced"));
  m_advancedToggle->setCheckable(true);
  m_advancedToggle->setAutoRaise(true);
  m_advancedToggle->setArrowType(Qt::RightArrow);
  m_advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_form->addRow(QString(), m_advancedToggle);
  m_clientPath = new QLineEdit(body);
  m_form->addRow(tr("Client program"), m_clientPath);
  m_extraArgs = new QLineEdit(body);
  m_extraArgs->setPlaceholderText(tr("extra command line arguments"));
  m_form->addRow(tr("Extra arguments"), m_extraArgs);

  auto *testRow = new QHBoxLayout();
  m_testButton = new QPushButton(tr("Test Connection"), m_formPage);
  m_testButton->setObjectName("dbTestConnection");
  m_testStatus = new QLabel(m_formPage);
  m_testStatus->setObjectName("dbTestStatus");
  m_testStatus->setWordWrap(true);
  m_testStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  testRow->addWidget(m_testButton, 0, Qt::AlignTop);
  testRow->addWidget(m_testStatus, 1);
  layout->addLayout(testRow);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  m_cancelButton = new QPushButton(tr("Cancel"), m_formPage);
  m_saveButton = new QPushButton(tr("Save"), m_formPage);
  m_saveButton->setObjectName("dbSave");
  m_saveConnectButton = new QPushButton(tr("Save && Connect"), m_formPage);
  m_saveConnectButton->setObjectName("dbSaveConnect");
  buttons->addWidget(m_cancelButton);
  buttons->addWidget(m_saveButton);
  buttons->addWidget(m_saveConnectButton);
  layout->addLayout(buttons);

  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_saveButton, &QPushButton::clicked, this, [this]() {
    m_connectAfterSave = false;
    accept();
  });
  connect(m_saveConnectButton, &QPushButton::clicked, this, [this]() {
    m_connectAfterSave = true;
    accept();
  });
  connect(m_backButton, &QPushButton::clicked, this,
          &ConnectionDialog::showChooser);
  connect(m_testButton, &QPushButton::clicked, this,
          &ConnectionDialog::testConnection);
  connect(m_browse, &QPushButton::clicked, this, &ConnectionDialog::browseFile);
  connect(m_newFile, &QPushButton::clicked, this, &ConnectionDialog::newFile);
  connect(m_refreshContainers, &QPushButton::clicked, this,
          &ConnectionDialog::refreshContainers);
  connect(m_copyUrl, &QPushButton::clicked, this, [this]() {
    QApplication::clipboard()->setText(DbConnectionUrl::format(profile()));
    m_urlHint->setText(tr("Copied. The password is never part of the copy."));
  });
  connect(m_engine, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ConnectionDialog::onEngineChanged);
  connect(m_direct, &QRadioButton::toggled, this,
          &ConnectionDialog::onTransportChanged);
  connect(m_passwordSource, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ConnectionDialog::onPasswordSourceChanged);
  connect(m_passwordEnv, &QLineEdit::textChanged, this,
          &ConnectionDialog::onPasswordSourceChanged);
  connect(m_advancedToggle, &QToolButton::toggled, this, [this](bool open) {
    m_advancedToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    updateVisibility();
  });
  connect(m_name, &QLineEdit::textEdited, this,
          [this]() { m_nameEdited = !m_name->text().trimmed().isEmpty(); });
  connect(m_url, &QLineEdit::textEdited, this, &ConnectionDialog::onUrlEdited);
  for (QLineEdit *edit : {m_host, m_user, m_database, m_file}) {
    connect(edit, &QLineEdit::textChanged, this,
            &ConnectionDialog::onFieldEdited);
  }
  connect(m_container->lineEdit(), &QLineEdit::textChanged, this,
          &ConnectionDialog::onFieldEdited);
  connect(m_port, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &ConnectionDialog::onFieldEdited);
  connect(m_sslMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ConnectionDialog::onFieldEdited);
  connect(m_trustCertificate, &QCheckBox::toggled, this,
          &ConnectionDialog::onFieldEdited);
  connect(m_password, &QLineEdit::textEdited, this, [this]() {
    m_passwordHint->setText(tr("Kept in memory for this session only; "
                               "Lightpad never saves passwords."));
  });

  onEngineChanged();
}

DbEngine ConnectionDialog::currentEngine() const {
  return static_cast<DbEngine>(m_engine->currentData().toInt());
}

void ConnectionDialog::loadProfile(const DbConnectionProfile &p,
                                   const QString &password,
                                   bool passwordFound) {
  // While loading, engine changes must not rewrite the loaded values as if
  // the user had switched engines.
  m_loading = true;
  m_id = p.id;
  m_name->setText(p.name);
  m_nameEdited = !p.name.isEmpty();
  const int engineIndex = m_engine->findData(int(p.engine));
  m_engine->setCurrentIndex(engineIndex >= 0 ? engineIndex : 0);
  onEngineChanged();
  (p.transport == DbTransport::Docker ? m_docker : m_direct)->setChecked(true);
  m_container->setEditText(p.container);
  m_host->setText(p.host);
  m_port->setValue(p.port);
  m_user->setText(p.user);
  m_database->setText(p.database);
  m_file->setText(p.filePath);
  m_readOnly->setChecked(p.readOnly);
  m_confirmDestructive->setChecked(p.confirmDestructive);
  m_trustCertificate->setChecked(p.trustServerCertificate);
  m_clientPath->setText(p.clientPath);
  m_extraArgs->setText(p.extraArgs);
  m_advancedToggle->setChecked(!p.clientPath.isEmpty() ||
                               !p.extraArgs.isEmpty());
  const int colorIndex = m_color->findData(p.color);
  m_color->setCurrentIndex(colorIndex >= 0 ? colorIndex : 0);
  const int sslIndex = m_sslMode->findData(p.sslMode);
  m_sslMode->setCurrentIndex(sslIndex >= 0 ? sslIndex : 0);
  m_passwordSource->setCurrentIndex(
      m_passwordSource->findData(int(p.passwordSource)));
  m_passwordEnv->setText(p.passwordEnv);
  m_password->setText(password);
  m_loading = false;

  onPasswordSourceChanged();
  if (passwordFound) {
    m_passwordHint->setText(tr("Filled in from the project; kept for this "
                               "session only and never saved."));
  }
  m_testStatus->clear();
  m_urlHint->setText(
      tr("Paste a URL or connection string here to fill in the fields below."));
  styleSubduedLabel(m_urlHint);
  updateContainerChoices();
  updateVisibility();
  updateUrlField(true);
  updateHeader();
}

void ConnectionDialog::setProfile(const DbConnectionProfile &profile) {
  m_editing = !profile.id.isEmpty();
  clearTemplateHints();
  m_templateName.clear();
  loadProfile(profile);
  m_help->setVisible(false);
  showForm();
}

bool ConnectionDialog::startFromTemplate(const QString &templateId) {
  const DbConnectionTemplate entry =
      DbConnectionTemplateCatalog::instance().templateById(templateId);
  if (!entry.isValid()) {
    return false;
  }
  m_templateId = entry.id;
  m_templateName = entry.name;
  DbConnectionProfile profile = entry.instantiate(QString());
  profile.name.clear();
  loadProfile(profile);
  m_nameEdited = false;
  applyTemplateHints(entry);
  updateAutoName();
  showForm();
  return true;
}

void ConnectionDialog::applyTemplateHints(const DbConnectionTemplate &entry) {
  m_host->setPlaceholderText(entry.hostPlaceholder.isEmpty()
                                 ? QStringLiteral("localhost")
                                 : entry.hostPlaceholder);
  m_user->setPlaceholderText(entry.userPlaceholder.isEmpty()
                                 ? DbEngineInfo::defaultUser(currentEngine())
                                 : entry.userPlaceholder);
  m_database->setPlaceholderText(entry.databasePlaceholder.isEmpty()
                                     ? tr("optional — the server's default")
                                     : entry.databasePlaceholder);
  QString help = entry.help.toHtmlEscaped();
  if (!entry.docsUrl.isEmpty()) {
    help += (help.isEmpty() ? QString() : QStringLiteral(" ")) +
            linkHtml(entry.docsUrl, tr("Guide"));
  }
  m_help->setText(help);
  m_help->setVisible(!help.isEmpty());
}

void ConnectionDialog::clearTemplateHints() {
  m_templateId.clear();
  m_host->setPlaceholderText(QStringLiteral("localhost"));
  m_user->setPlaceholderText(DbEngineInfo::defaultUser(currentEngine()));
  m_database->setPlaceholderText(tr("optional — the server's default"));
  m_help->clear();
  m_help->setVisible(false);
}

bool ConnectionDialog::applyConnectionString(const QString &text,
                                             QString *error) {
  DbConnectionUrl::Parsed parsed;
  if (!DbConnectionUrl::parse(text, &parsed, error, m_workspaceFolder)) {
    return false;
  }
  DbConnectionProfile p = parsed.profile;
  if (p.engine != currentEngine()) {
    m_templateName.clear();
  }
  // Keep what the URL cannot express.
  const DbConnectionProfile current = profile();
  p.id = m_id;
  p.name = m_nameEdited ? current.name : QString();
  p.color = current.color;
  p.readOnly = current.readOnly;
  p.confirmDestructive = current.confirmDestructive;
  p.clientPath = current.clientPath;
  p.extraArgs = current.extraArgs;
  if (p.passwordSource == DbPasswordSource::Ask &&
      current.passwordSource != DbPasswordSource::Ask && !parsed.hasPassword) {
    p.passwordSource = current.passwordSource;
    p.passwordEnv = current.passwordEnv;
  }
  const bool nameEdited = m_nameEdited;
  loadProfile(p, parsed.hasPassword ? parsed.password : m_password->text(),
              parsed.hasPassword);
  m_nameEdited = nameEdited;
  updateAutoName();
  if (parsed.hasPassword) {
    m_passwordHint->setText(tr("Taken from the URL; kept for this session "
                               "only and never saved."));
  }
  return true;
}

DbConnectionProfile ConnectionDialog::profile() const {
  DbConnectionProfile p;
  p.id = m_id;
  p.name = m_name->text().trimmed();
  p.engine = currentEngine();
  p.transport = m_docker->isChecked() && DbEngineInfo::supportsDocker(p.engine)
                    ? DbTransport::Docker
                    : DbTransport::Direct;
  p.container = m_container->currentText().trimmed();
  p.host = m_host->text().trimmed();
  if (p.host.isEmpty() && p.engine != DbEngine::Sqlite &&
      m_host->placeholderText() == QLatin1String("localhost")) {
    p.host = QStringLiteral("localhost");
  }
  p.port = m_port->value();
  p.user = m_user->text().trimmed();
  p.database = m_database->text().trimmed();
  p.filePath = m_file->text().trimmed();
  p.readOnly = m_readOnly->isChecked();
  p.confirmDestructive = m_confirmDestructive->isChecked();
  p.trustServerCertificate = m_trustCertificate->isChecked();
  p.sslMode = DbEngineInfo::supportsSslMode(p.engine)
                  ? m_sslMode->currentData().toString()
                  : QString();
  p.passwordSource =
      static_cast<DbPasswordSource>(m_passwordSource->currentData().toInt());
  p.passwordEnv = p.passwordSource == DbPasswordSource::Environment
                      ? m_passwordEnv->text().trimmed()
                      : QString();
  p.clientPath = m_clientPath->text().trimmed();
  p.extraArgs = m_extraArgs->text().trimmed();
  p.color = m_color->currentData().toString();
  return p;
}

QString ConnectionDialog::password() const {
  const auto source =
      static_cast<DbPasswordSource>(m_passwordSource->currentData().toInt());
  return source == DbPasswordSource::Ask ? m_password->text() : QString();
}

void ConnectionDialog::onEngineChanged() {
  if (m_engine->currentIndex() < 0) {
    return;
  }
  const DbEngine engine = currentEngine();
  if (!m_loading) {
    // Replace values that were the previous engine's defaults.
    for (DbEngine e : DbEngineInfo::allEngines()) {
      if (e == engine) {
        continue;
      }
      if (!m_user->text().isEmpty() &&
          m_user->text() == DbEngineInfo::defaultUser(e)) {
        m_user->setText(DbEngineInfo::defaultUser(engine));
      }
      if (!m_database->text().isEmpty() &&
          m_database->text() == DbEngineInfo::defaultDatabase(e)) {
        m_database->setText(DbEngineInfo::defaultDatabase(engine));
      }
      if (m_port->value() == DbEngineInfo::defaultPort(e)) {
        m_port->setValue(0);
      }
    }
  }
  m_user->setPlaceholderText(DbEngineInfo::defaultUser(engine));
  const QString defaultPort =
      DbEngineInfo::defaultPort(engine) > 0
          ? QString::number(DbEngineInfo::defaultPort(engine))
          : tr("default");
  m_port->setSpecialValueText(defaultPort);
  m_port->setToolTip(
      tr("Leave empty for the default port (%1)").arg(defaultPort));
  m_clientPath->setPlaceholderText(
      tr("default: %1").arg(DbEngineInfo::clientName(engine)));

  const QString ssl = m_sslMode->currentData().toString();
  {
    const QSignalBlocker blocker(m_sslMode);
    m_sslMode->clear();
    for (const auto &mode : DbEngineInfo::sslModes(engine)) {
      m_sslMode->addItem(mode.second, mode.first);
    }
    const int index = m_sslMode->findData(ssl);
    m_sslMode->setCurrentIndex(index >= 0 ? index : 0);
  }
  if (!m_loading) {
    updateVisibility();
    updateAutoName();
    updateUrlField();
    updateHeader();
  }
}

void ConnectionDialog::onTransportChanged() {
  if (m_loading) {
    return;
  }
  if (m_docker->isChecked() && !m_dockerSearched) {
    refreshContainers();
  }
  updateVisibility();
  updateAutoName();
  updateUrlField();
  updateHeader();
}

void ConnectionDialog::onFieldEdited() {
  if (m_loading) {
    return;
  }
  updateAutoName();
  updateUrlField();
  updateHeader();
  m_testStatus->clear();
}

void ConnectionDialog::onUrlEdited(const QString &text) {
  if (text.trimmed().isEmpty()) {
    m_urlHint->setText(
        tr("Paste a URL or connection string here to fill in the fields "
           "below."));
    styleSubduedLabel(m_urlHint);
    return;
  }
  QString error;
  DbConnectionUrl::Parsed parsed;
  if (!DbConnectionUrl::parse(text, &parsed, &error, m_workspaceFolder)) {
    m_urlHint->setText(error);
    styleToneLabel(m_urlHint, UIStyleHelper::Tone::Warning);
    return;
  }
  const int cursor = m_url->cursorPosition();
  applyConnectionString(text);
  // Keep exactly what the user typed while they are typing it.
  m_url->setText(text);
  m_url->setCursorPosition(cursor);
  m_urlHint->setText(tr("Fields filled in from the URL."));
  styleToneLabel(m_urlHint, UIStyleHelper::Tone::Success);
}

void ConnectionDialog::onPasswordSourceChanged() {
  const auto source =
      static_cast<DbPasswordSource>(m_passwordSource->currentData().toInt());
  m_password->setVisible(source == DbPasswordSource::Ask);
  m_passwordEnv->setVisible(source == DbPasswordSource::Environment);
  switch (source) {
  case DbPasswordSource::Ask:
    m_password->setPlaceholderText(tr("asked when connecting if left empty"));
    m_passwordHint->setText(tr("Kept in memory for this session only; "
                               "Lightpad never saves passwords."));
    break;
  case DbPasswordSource::None:
    m_passwordHint->setText(
        tr("No password is sent (trust, peer or certificate authentication)."));
    break;
  case DbPasswordSource::Environment: {
    const QString name = m_passwordEnv->text().trimmed();
    if (name.isEmpty()) {
      m_passwordHint->setText(
          tr("Read from this variable each time Lightpad connects."));
      break;
    }
    bool found = false;
    DbProjectDiscovery::environmentValue(name, &found);
    m_passwordHint->setText(
        found ? tr("%1 is set; its value is read each time Lightpad connects.")
                    .arg(name)
              : tr("%1 is not set in Lightpad's environment or the project's "
                   ".env; you will be asked for the password.")
                    .arg(name));
    break;
  }
  }
  if (!m_loading) {
    m_testStatus->clear();
  }
}

void ConnectionDialog::updateVisibility() {
  const DbEngine engine = currentEngine();
  const bool file = DbEngineInfo::usesFile(engine);
  const bool server = !file;
  const bool docker = server && m_docker->isChecked();
  const bool advanced = m_advancedToggle->isChecked();
  auto show = [this](QWidget *field, bool visible) {
    m_form->setRowVisible(field, visible);
  };
  show(m_transportRow, server);
  show(m_containerRow, docker);
  show(m_hostRow, server && !docker);
  show(m_user, server);
  show(m_passwordRow, server);
  show(m_passwordHint, server);
  show(m_database, server);
  show(m_fileRow, file);
  show(m_sslMode, DbEngineInfo::supportsSslMode(engine));
  show(m_trustCertificate, engine == DbEngine::SqlServer);
  show(m_advancedToggle, server);
  show(m_clientPath, server && advanced);
  show(m_extraArgs, server && advanced);
  m_file->setEnabled(m_file->text() != QLatin1String(":memory:"));
}

QString ConnectionDialog::suggestedName() const {
  const DbConnectionProfile p = profile();
  QString where;
  if (p.engine == DbEngine::Sqlite) {
    where = p.filePath == QLatin1String(":memory:")
                ? QString()
                : QFileInfo(p.filePath).completeBaseName();
  } else if (p.transport == DbTransport::Docker) {
    where = p.container;
  } else if (!m_host->text().trimmed().isEmpty()) {
    where = m_host->text().trimmed().section('.', 0, 0);
  }
  if (!p.database.isEmpty() && p.engine != DbEngine::Sqlite &&
      p.database != DbEngineInfo::defaultDatabase(p.engine)) {
    where = where.isEmpty() ? p.database
                            : QStringLiteral("%1/%2").arg(where, p.database);
  }
  const QString base = m_templateName.isEmpty()
                           ? DbEngineInfo::displayName(p.engine)
                           : m_templateName;
  return where.isEmpty() ? base : QStringLiteral("%1 · %2").arg(base, where);
}

void ConnectionDialog::updateAutoName() {
  if (m_loading || m_nameEdited) {
    return;
  }
  m_name->setText(suggestedName());
}

void ConnectionDialog::updateUrlField(bool force) {
  if (m_loading || (m_url->hasFocus() && !force)) {
    return;
  }
  const DbConnectionProfile p = profile();
  const bool viaDocker =
      p.engine != DbEngine::Sqlite && p.transport == DbTransport::Docker;
  const bool complete =
      p.engine == DbEngine::Sqlite ? !p.filePath.isEmpty() : !p.host.isEmpty();
  m_url->setText(!viaDocker && complete ? DbConnectionUrl::format(p)
                                        : QString());
  QString placeholder;
  if (viaDocker) {
    placeholder = tr("Paste a URL to connect directly instead of through "
                     "Docker");
  } else if (p.engine == DbEngine::Sqlite) {
    placeholder = tr("sqlite:///path/to/database.db");
  } else {
    // Show the shape the template expects, e.g. db.<ref>.supabase.co.
    placeholder = reachSummary(p, m_host->placeholderText());
  }
  m_url->setPlaceholderText(placeholder);
  m_copyUrl->setEnabled(!m_url->text().isEmpty());
}

void ConnectionDialog::updateHeader() {
  const DbConnectionProfile p = profile();
  m_formTitle->setText(m_editing ? tr("Edit Connection")
                                 : tr("New Connection"));
  m_formBadge->setText(badgeText(p));
  styleBadge(m_formBadge, UIStyleHelper::Tone::Accent);
}

void ConnectionDialog::refreshContainers() {
  m_dockerSearched = false;
  m_dockerFailed = false;
  m_refreshContainers->setEnabled(false);
  m_discovery->refresh();
  if (isChoosing()) {
    populateChooser();
  }
}

void ConnectionDialog::onContainersFound(
    const QVector<DockerContainer> &containers) {
  m_containers = containers;
  m_dockerSearched = true;
  m_refreshContainers->setEnabled(true);
  populateChooser();
  updateContainerChoices();
}

void ConnectionDialog::updateContainerChoices() {
  m_refreshContainers->setEnabled(true);
  const QString current = m_container->currentText();
  const QSignalBlocker blocker(m_container);
  m_container->clear();
  const DbEngine engine = currentEngine();
  // Containers of the chosen engine first, then other database containers.
  for (int pass = 0; pass < 2; ++pass) {
    for (const DockerContainer &c : m_containers) {
      if (!c.isDatabase || (pass == 0) != (c.engine == engine)) {
        continue;
      }
      m_container->addItem(QStringLiteral("%1").arg(c.name), c.name);
      m_container->setItemData(m_container->count() - 1,
                               QStringLiteral("%1 · %2").arg(c.image, c.status),
                               Qt::ToolTipRole);
    }
  }
  m_container->setEditText(current);
  if (m_dockerFailed) {
    m_container->lineEdit()->setPlaceholderText(m_dockerMessage);
  }
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
    m_passwordSource->setCurrentIndex(
        m_passwordSource->findData(int(DbPasswordSource::Ask)));
    m_password->setText(hints.password);
    m_passwordHint->setText(tr("Filled in from the container's environment; "
                               "kept for this session only."));
  }
  m_help->setText(tr("Credentials read from %1's environment.").arg(container));
  m_help->setVisible(true);
}

void ConnectionDialog::browseFile() {
  const QString start =
      m_file->text().isEmpty() ? m_workspaceFolder : m_file->text();
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open SQLite database"), start,
      tr("SQLite databases (*.db *.sqlite *.sqlite3 *.db3 *.s3db);;All files "
         "(*)"));
  if (!path.isEmpty()) {
    m_file->setText(path);
  }
}

void ConnectionDialog::newFile() {
  const QString folder =
      m_workspaceFolder.isEmpty()
          ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
          : m_workspaceFolder;
  const QString path = QFileDialog::getSaveFileName(
      this, tr("Create SQLite database"), folder + "/database.db",
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

void ConnectionDialog::setTestStatus(const QString &text,
                                     UIStyleHelper::Tone tone) {
  m_testStatus->setText(text);
  styleToneLabel(m_testStatus, tone);
}

void ConnectionDialog::testConnection() {
  QString why;
  DbConnectionProfile p = profile();
  if (p.name.isEmpty()) {
    p.name = QStringLiteral("test");
  }
  if (!p.isValid(&why)) {
    setTestStatus(why, UIStyleHelper::Tone::Error);
    return;
  }
  QString pw = password();
  if (p.passwordSource == DbPasswordSource::Environment) {
    bool found = false;
    pw = DbProjectDiscovery::environmentValue(p.passwordEnv, &found);
    if (!found) {
      setTestStatus(tr("%1 is not set.").arg(p.passwordEnv),
                    UIStyleHelper::Tone::Error);
      return;
    }
  }
  disconnect(m_testResultConnection);
  if (m_testConnection) {
    m_testConnection->disconnect(this);
    m_testConnection->disconnectFromServer();
  }
  m_testConnection = std::make_unique<DbConnection>(p);
  DbConnection *conn = m_testConnection.get();
  m_testButton->setEnabled(false);
  setTestStatus(tr("Connecting…"), UIStyleHelper::Tone::Neutral);

  auto timer = std::make_shared<QElapsedTimer>();
  timer->start();
  const DbEngine engine = p.engine;
  connect(conn, &DbConnection::stateChanged, this,
          [this, conn, engine](DbConnectionState state) {
            if (state == DbConnectionState::Failed) {
              finishTest(conn->lastError(), false);
            } else if (state == DbConnectionState::Connected) {
              m_testRequest = conn->execute({versionQuery(engine)});
            }
          });
  m_testResultConnection = connect(
      conn, &DbConnection::statementFinished, this,
      [this, timer](quint64 id, int, const DbStatementResult &r) {
        if (id != m_testRequest) {
          return;
        }
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
        const QString elapsed =
            tr("%1 ms").arg(QString::number(timer->elapsed()));
        finishTest(
            version.isEmpty()
                ? tr("Connected (%1).").arg(elapsed)
                : tr("Connected in %1 — %2").arg(elapsed, version.left(110)),
            true);
      });
  conn->connectToServer(pw);
}

void ConnectionDialog::finishTest(const QString &text, bool ok) {
  setTestStatus(text.isEmpty() ? tr("Could not connect.") : text,
                ok ? UIStyleHelper::Tone::Success : UIStyleHelper::Tone::Error);
  m_testButton->setEnabled(true);
  disconnect(m_testResultConnection);
  if (m_testConnection) {
    m_testConnection->disconnect(this);
    m_testConnection->disconnectFromServer();
  }
}

void ConnectionDialog::accept() {
  if (isChoosing()) {
    chooseCurrent();
    return;
  }
  QString why;
  const DbConnectionProfile p = profile();
  if (!p.isValid(&why)) {
    setTestStatus(why, UIStyleHelper::Tone::Error);
    return;
  }
  if (m_uniqueName) {
    const QString unique = m_uniqueName(p.name);
    if (unique != p.name) {
      m_name->setText(unique);
    }
  }
  disconnect(m_testResultConnection);
  if (m_testConnection) {
    m_testConnection->disconnect(this);
    m_testConnection->disconnectFromServer();
    m_testConnection.reset();
  }
  QDialog::accept();
}

// --- Context -------------------------------------------------------------

void ConnectionDialog::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
  m_detected = DbProjectDiscovery::scan(folder);
  populateChooser();
  // Something the project describes is the most likely choice.
  if (!m_detected.isEmpty() && isChoosing()) {
    selectFirstChoice();
  }
}

void ConnectionDialog::setSavedProfiles(
    const QVector<DbConnectionProfile> &profiles) {
  m_saved = profiles;
  populateChooser();
}

QString
ConnectionDialog::savedNameFor(const DbConnectionProfile &candidate) const {
  for (const DbConnectionProfile &p : m_saved) {
    const bool same =
        p.engine == candidate.engine &&
        (p.engine == DbEngine::Sqlite
             ? QDir::cleanPath(p.filePath) ==
                   QDir::cleanPath(candidate.filePath)
             : p.transport == candidate.transport &&
                   (p.transport == DbTransport::Docker
                        ? p.container == candidate.container
                        : p.host == candidate.host &&
                              p.effectivePort() == candidate.effectivePort()) &&
                   p.user == candidate.user &&
                   p.database == candidate.database);
    if (same) {
      return p.name;
    }
  }
  return {};
}

bool ConnectionDialog::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    const auto *key = static_cast<QKeyEvent *>(event);
    if (watched == m_search &&
        (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up)) {
      QTreeWidgetItem *next = m_choices->currentItem();
      do {
        next = next ? (key->key() == Qt::Key_Down ? m_choices->itemBelow(next)
                                                  : m_choices->itemAbove(next))
                    : nullptr;
      } while (next && next->data(0, KindRole).toInt() == 0);
      if (next) {
        m_choices->setCurrentItem(next);
      }
      return true;
    }
    if (watched == m_chooserUrl &&
        (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
      if (m_chooserUseUrl->isEnabled()) {
        useChooserUrl();
      }
      return true;
    }
  }
  return StyledDialog::eventFilter(watched, event);
}

void ConnectionDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  for (auto *title : findChildren<QLabel *>("dbDialogTitle")) {
    styleTitleLabel(title);
  }
  styleSubduedLabel(findChild<QLabel *>("dbDialogSubtitle"));
  for (auto *hint : findChildren<QLabel *>("dbDialogHint")) {
    styleSubduedLabel(hint);
  }
  styleSubduedLabel(m_choiceDescription);
  styleSubduedLabel(m_choiceNote);
  styleToneLabel(m_help, UIStyleHelper::Tone::Info);
  m_search->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  const QColor fill = theme.surfaceAltColor.isValid() ? theme.surfaceAltColor
                                                      : theme.surfaceColor;
  m_choicePreview->setStyleSheet(
      QStringLiteral("QLabel { font-family: monospace; padding: 8px; "
                     "border-radius: 6px; background: %1; color: %2; "
                     "border: 1px solid %3; }")
          .arg(fill.name(),
               UIStyleHelper::readableText(theme, fill, theme.foregroundColor)
                   .name(),
               theme.borderColor.name()));
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono.setPointSizeF(m_url->font().pointSizeF());
  m_url->setFont(mono);
  stylePrimaryButton(m_continueButton);
  stylePrimaryButton(m_saveConnectButton);
  const QColor accent =
      theme.accentColor.isValid() ? theme.accentColor : theme.foregroundColor;
  m_backButton->setStyleSheet(
      QStringLiteral("QPushButton { color: %1; background: transparent; "
                     "border: none; padding: 2px 6px 2px 0; }"
                     "QPushButton:hover { text-decoration: underline; }")
          .arg(accent.name()));
  m_advancedToggle->setStyleSheet(
      QStringLiteral("QToolButton { color: %1; background: transparent; "
                     "border: none; padding: 2px 0; }")
          .arg(UIStyleHelper::mutedTextColor(theme).name()));
  m_host->ensurePolished();
  m_port->setFixedHeight(m_host->sizeHint().height());
  populateChooser();
  updateChoiceDetails();
  updateHeader();
}
