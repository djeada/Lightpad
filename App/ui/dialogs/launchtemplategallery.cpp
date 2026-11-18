#include "launchtemplategallery.h"

#include "../../run_templates/launchplanner.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int TemplateIdRole = Qt::UserRole;

QString configurationSummary(const LaunchTemplate &entry) {
  const DebugConfiguration config = entry.instantiate(entry.name);
  QStringList lines;
  if (config.request == QLatin1String("attach")) {
    if (config.port > 0) {
      lines << QObject::tr("Connects to %1:%2")
                   .arg(config.host.isEmpty() ? QStringLiteral("localhost")
                                              : config.host)
                   .arg(config.port);
    } else if (config.adapterConfig.contains("miDebuggerServerAddress")) {
      lines << QObject::tr("Connects to gdbserver at %1")
                   .arg(config.adapterConfig.value("miDebuggerServerAddress")
                            .toString());
    } else {
      lines << QObject::tr("Attaches to a process you pick at start");
    }
  } else {
    const RunCommand command = LaunchPlanner::runCommandFor(config);
    const QString line =
        LaunchPlanner::commandLine(command.program, command.arguments);
    if (!line.isEmpty()) {
      lines << QStringLiteral("$ ") + line;
    }
    if (!config.preLaunchTask.isEmpty()) {
      lines << QObject::tr("Before launch: %1").arg(config.preLaunchTask);
    }
  }
  return lines.join(QLatin1Char('\n'));
}
} // namespace

LaunchTemplateGallery::LaunchTemplateGallery(const QString &languageId,
                                             const QString &workspaceFolder,
                                             QWidget *parent)
    : StyledDialog(parent), m_languageId(languageId),
      m_workspaceFolder(workspaceFolder) {
  setWindowTitle(tr("Add Configuration"));
  setMinimumSize(720, 480);
  resize(820, 540);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 12);
  layout->setSpacing(10);

  m_search = new QLineEdit(this);
  m_search->setPlaceholderText(tr("Search templates (Python, npm, CMake, "
                                  "attach, …)"));
  m_search->setClearButtonEnabled(true);
  m_search->setAccessibleName(tr("Search templates"));
  m_search->installEventFilter(this);
  layout->addWidget(m_search);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);

  m_tree = new QTreeWidget(splitter);
  m_tree->setHeaderHidden(true);
  m_tree->setRootIsDecorated(false);
  m_tree->setIndentation(12);
  m_tree->setUniformRowHeights(true);
  m_tree->setAccessibleName(tr("Templates"));
  m_tree->setMinimumWidth(280);

  auto *details = new QWidget(splitter);
  auto *detailsLayout = new QVBoxLayout(details);
  detailsLayout->setContentsMargins(12, 4, 4, 4);
  detailsLayout->setSpacing(8);
  auto *titleRow = new QHBoxLayout();
  m_title = new QLabel(details);
  m_title->setWordWrap(true);
  m_badge = new QLabel(details);
  titleRow->addWidget(m_title, 1);
  titleRow->addWidget(m_badge, 0, Qt::AlignTop);
  detailsLayout->addLayout(titleRow);
  m_description = new QLabel(details);
  m_description->setWordWrap(true);
  m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);
  detailsLayout->addWidget(m_description);
  m_preview = new QLabel(details);
  m_preview->setWordWrap(true);
  m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_preview->setObjectName(QStringLiteral("templatePreview"));
  detailsLayout->addWidget(m_preview);
  auto *hint = new QLabel(
      tr("Everything can be changed after the configuration is created."),
      details);
  hint->setWordWrap(true);
  hint->setObjectName(QStringLiteral("templateHint"));
  detailsLayout->addWidget(hint);
  detailsLayout->addStretch();

  splitter->addWidget(m_tree);
  splitter->addWidget(details);
  splitter->setStretchFactor(0, 2);
  splitter->setStretchFactor(1, 3);
  layout->addWidget(splitter, 1);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch();
  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_createButton = new QPushButton(tr("Add Configuration"), this);
  buttons->addWidget(m_cancelButton);
  buttons->addWidget(m_createButton);
  layout->addLayout(buttons);

  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_createButton, &QPushButton::clicked, this, [this]() {
    if (!selectedTemplateId().isEmpty()) {
      accept();
    }
  });
  connect(m_search, &QLineEdit::textChanged, this,
          &LaunchTemplateGallery::applyFilter);
  connect(m_tree, &QTreeWidget::currentItemChanged, this,
          &LaunchTemplateGallery::updateDetails);
  connect(m_tree, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item) {
            if (item && !item->data(0, TemplateIdRole).toString().isEmpty()) {
              accept();
            }
          });

  populate();
  setKeyboardDefault(m_createButton);
  m_search->setFocus();
}

void LaunchTemplateGallery::populate() {
  m_tree->clear();
  const LaunchTemplateCatalog &catalog = LaunchTemplateCatalog::instance();

  auto addGroup = [this](const QString &title) {
    auto *group = new QTreeWidgetItem(m_tree, {title});
    group->setFlags(Qt::ItemIsEnabled);
    QFont font = group->font(0);
    font.setBold(true);
    group->setFont(0, font);
    group->setExpanded(true);
    return group;
  };
  auto addTemplate = [](QTreeWidgetItem *group, const LaunchTemplate &entry) {
    auto *item = new QTreeWidgetItem(group, {entry.name});
    item->setData(0, TemplateIdRole, entry.id);
    item->setToolTip(0, entry.description);
    // Searchable text beyond the visible name.
    item->setData(0, Qt::UserRole + 1,
                  QStringList{entry.name, entry.category, entry.description,
                              entry.languages.join(' ')}
                      .join(' '));
  };

  const QList<LaunchTemplate> suggestions =
      catalog.suggestionsFor(m_languageId, m_workspaceFolder);
  if (!suggestions.isEmpty()) {
    QTreeWidgetItem *group = addGroup(tr("Suggested for this project"));
    for (int i = 0; i < suggestions.size() && i < 6; ++i) {
      addTemplate(group, suggestions.at(i));
    }
  }
  for (const QString &category : catalog.categories()) {
    QTreeWidgetItem *group = nullptr;
    for (const LaunchTemplate &entry : catalog.templates()) {
      if (entry.category != category) {
        continue;
      }
      if (!group) {
        group = addGroup(category);
      }
      addTemplate(group, entry);
    }
  }
  m_tree->expandAll();
  selectFirstVisibleTemplate();
}

void LaunchTemplateGallery::applyFilter(const QString &text) {
  const QStringList words =
      text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *group = m_tree->topLevelItem(i);
    bool anyVisible = false;
    for (int j = 0; j < group->childCount(); ++j) {
      QTreeWidgetItem *item = group->child(j);
      const QString haystack = item->data(0, Qt::UserRole + 1).toString();
      bool matches = true;
      for (const QString &word : words) {
        if (!haystack.contains(word, Qt::CaseInsensitive)) {
          matches = false;
          break;
        }
      }
      item->setHidden(!matches);
      anyVisible = anyVisible || matches;
    }
    group->setHidden(!anyVisible);
  }
  QTreeWidgetItem *current = m_tree->currentItem();
  if (!current || current->isHidden() ||
      (current->parent() && current->parent()->isHidden())) {
    selectFirstVisibleTemplate();
  }
}

void LaunchTemplateGallery::selectFirstVisibleTemplate() {
  for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *group = m_tree->topLevelItem(i);
    if (group->isHidden()) {
      continue;
    }
    for (int j = 0; j < group->childCount(); ++j) {
      if (!group->child(j)->isHidden()) {
        m_tree->setCurrentItem(group->child(j));
        return;
      }
    }
  }
  m_tree->setCurrentItem(nullptr);
  updateDetails();
}

QString LaunchTemplateGallery::selectedTemplateId() const {
  const QTreeWidgetItem *item = m_tree->currentItem();
  return item ? item->data(0, TemplateIdRole).toString() : QString();
}

void LaunchTemplateGallery::updateDetails() {
  const LaunchTemplate entry =
      LaunchTemplateCatalog::instance().templateById(selectedTemplateId());
  m_createButton->setEnabled(entry.isValid());
  if (!entry.isValid()) {
    m_title->setText(tr("No template matches the search."));
    m_description->clear();
    m_preview->clear();
    m_preview->setVisible(false);
    m_badge->clear();
    m_badge->setVisible(false);
    return;
  }
  m_title->setText(QStringLiteral("<b>%1</b>").arg(entry.name.toHtmlEscaped()));
  m_description->setText(entry.description);
  const QString summary = configurationSummary(entry);
  m_preview->setText(summary);
  m_preview->setVisible(!summary.isEmpty());
  const DebugConfiguration config = entry.instantiate(entry.name);
  const QString debugger = LaunchPlanner::debuggerNameFor(config);
  m_badge->setText(entry.isDebuggable()
                       ? (debugger.isEmpty()
                              ? tr("Run & Debug")
                              : tr("Run & Debug · %1").arg(debugger))
                       : tr("Run only"));
  m_badge->setVisible(true);
  styleBadge(m_badge, entry.isDebuggable() ? UIStyleHelper::Tone::Accent
                                           : UIStyleHelper::Tone::Neutral);
}

bool LaunchTemplateGallery::eventFilter(QObject *watched, QEvent *event) {
  // Arrow keys in the search field move through the templates, so the list
  // can be filtered and picked without leaving the keyboard.
  if (watched == m_search && event->type() == QEvent::KeyPress) {
    const auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
      QTreeWidgetItem *current = m_tree->currentItem();
      QTreeWidgetItem *next = current;
      do {
        next = next ? (key->key() == Qt::Key_Down ? m_tree->itemBelow(next)
                                                  : m_tree->itemAbove(next))
                    : nullptr;
      } while (next && next->data(0, TemplateIdRole).toString().isEmpty());
      if (next) {
        m_tree->setCurrentItem(next);
      }
      return true;
    }
  }
  return StyledDialog::eventFilter(watched, event);
}

void LaunchTemplateGallery::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  m_search->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  styleSubduedLabel(m_description);
  if (auto *hint = findChild<QLabel *>(QStringLiteral("templateHint"))) {
    styleSubduedLabel(hint);
  }
  const QColor fill = theme.surfaceAltColor.isValid() ? theme.surfaceAltColor
                                                      : theme.surfaceColor;
  m_preview->setStyleSheet(
      QStringLiteral("QLabel { font-family: monospace; padding: 8px; "
                     "border-radius: 6px; background: %1; color: %2; "
                     "border: 1px solid %3; }")
          .arg(fill.name(),
               UIStyleHelper::readableText(theme, fill, theme.foregroundColor)
                   .name(),
               theme.borderColor.name()));
  stylePrimaryButton(m_createButton);
  updateDetails();
}
