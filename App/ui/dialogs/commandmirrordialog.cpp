#include "commandmirrordialog.h"
#include "../../git/gitintegration.h"
#include "../uimetrics.h"
#include "../uistylehelper.h"
#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int RECORD_INDEX_ROLE = Qt::UserRole + 1;
}

CommandMirrorDialog::CommandMirrorDialog(GitIntegration *git,
                                         const Theme &theme, QWidget *parent)
    : StyledDialog(parent), m_git(git), m_modeCombo(nullptr),
      m_searchEdit(nullptr), m_commandTree(nullptr),
      m_explanationLabel(nullptr), m_riskLabel(nullptr), m_outputView(nullptr),
      m_copyButton(nullptr), m_clearButton(nullptr), m_closeButton(nullptr) {
  setWindowTitle(tr("Command Mirror"));
  setMinimumSize(860, 560);
  resize(1000, 660);

  buildUi();
  setKeyboardDefault(m_closeButton);
  applyTheme(theme);

  if (m_git) {
    for (const GitCommandRecord &record : m_git->commandHistory()) {
      addRecord(record);
    }
    connect(m_git, &GitIntegration::commandExecuted, this,
            &CommandMirrorDialog::onCommandExecuted);
  }
  onSelectionChanged();
}

void CommandMirrorDialog::buildUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(UiMetrics::SpaceLg, UiMetrics::SpaceLg,
                             UiMetrics::SpaceLg, UiMetrics::SpaceLg);
  layout->setSpacing(UiMetrics::SpaceMd);

  QLabel *header = new QLabel(
      tr("Every Git command Lightpad ran, exactly as it ran it. Credentials "
         "are redacted."),
      this);
  header->setObjectName(QStringLiteral("mirrorHeaderLabel"));
  header->setWordWrap(true);
  layout->addWidget(header);

  QHBoxLayout *bar = new QHBoxLayout();
  QLabel *modeLabel = new QLabel(tr("Mode:"), this);
  modeLabel->setObjectName(QStringLiteral("mirrorModeLabel"));
  bar->addWidget(modeLabel);

  m_modeCombo = new QComboBox(this);
  m_modeCombo->setObjectName(QStringLiteral("mirrorModeCombo"));
  for (GitCommandMirrorMode value :
       {GitCommandMirrorMode::Hidden, GitCommandMirrorMode::Learn,
        GitCommandMirrorMode::Preview, GitCommandMirrorMode::Expert}) {
    m_modeCombo->addItem(gitCommandMirrorModeName(value),
                         static_cast<int>(value));
  }
  m_modeCombo->setCurrentIndex(static_cast<int>(GitIntegration::mirrorMode()));
  connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CommandMirrorDialog::onModeChanged);
  bar->addWidget(m_modeCombo);

  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setObjectName(QStringLiteral("mirrorSearchEdit"));
  m_searchEdit->setPlaceholderText(tr("Search commands and output…"));
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &CommandMirrorDialog::onSearchChanged);
  bar->addWidget(m_searchEdit, 1);
  layout->addLayout(bar);

  m_commandTree = new QTreeWidget(this);
  m_commandTree->setObjectName(QStringLiteral("mirrorCommandTree"));
  m_commandTree->setColumnCount(3);
  m_commandTree->setHeaderLabels({tr("When"), tr("Command"), tr("Result")});
  m_commandTree->setRootIsDecorated(false);
  m_commandTree->header()->setSectionResizeMode(0,
                                                QHeaderView::ResizeToContents);
  m_commandTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_commandTree->header()->setSectionResizeMode(2,
                                                QHeaderView::ResizeToContents);
  connect(m_commandTree, &QTreeWidget::itemSelectionChanged, this,
          &CommandMirrorDialog::onSelectionChanged);
  layout->addWidget(m_commandTree, 2);

  m_explanationLabel = new QLabel(this);
  m_explanationLabel->setObjectName(QStringLiteral("mirrorExplanationLabel"));
  m_explanationLabel->setWordWrap(true);
  layout->addWidget(m_explanationLabel);

  m_riskLabel = new QLabel(this);
  m_riskLabel->setObjectName(QStringLiteral("mirrorRiskLabel"));
  m_riskLabel->setWordWrap(true);
  layout->addWidget(m_riskLabel);

  m_outputView = new QPlainTextEdit(this);
  m_outputView->setObjectName(QStringLiteral("mirrorOutputView"));
  m_outputView->setReadOnly(true);
  m_outputView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  layout->addWidget(m_outputView, 1);

  QHBoxLayout *footer = new QHBoxLayout();
  m_copyButton = new QPushButton(tr("Copy command"), this);
  m_copyButton->setObjectName(QStringLiteral("mirrorCopyButton"));
  connect(m_copyButton, &QPushButton::clicked, this,
          &CommandMirrorDialog::onCopy);
  footer->addWidget(m_copyButton);

  m_clearButton = new QPushButton(tr("Clear"), this);
  m_clearButton->setObjectName(QStringLiteral("mirrorClearButton"));
  connect(m_clearButton, &QPushButton::clicked, this,
          &CommandMirrorDialog::onClear);
  footer->addWidget(m_clearButton);

  footer->addStretch();
  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setObjectName(QStringLiteral("mirrorCloseButton"));
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
  footer->addWidget(m_closeButton);
  layout->addLayout(footer);
}

int CommandMirrorDialog::recordCount() const { return m_records.size(); }

GitCommandMirrorMode CommandMirrorDialog::mode() const {
  return static_cast<GitCommandMirrorMode>(m_modeCombo->currentData().toInt());
}

void CommandMirrorDialog::addRecord(const GitCommandRecord &record) {
  m_records.append(record);

  QTreeWidgetItem *item = new QTreeWidgetItem(m_commandTree);
  item->setText(0, record.when.toString(QStringLiteral("HH:mm:ss")));
  item->setText(1, record.commandLine());
  item->setText(2, record.succeeded ? tr("ok")
                                    : tr("exit %1").arg(record.exitCode));
  item->setData(0, RECORD_INDEX_ROLE, m_records.size() - 1);
  m_commandTree->scrollToItem(item);
}

void CommandMirrorDialog::onCommandExecuted(const GitCommandRecord &record) {
  addRecord(record);
  onSearchChanged(m_searchEdit->text());
}

void CommandMirrorDialog::onSearchChanged(const QString &text) {
  const QString needle = text.trimmed();
  for (int i = 0; i < m_commandTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_commandTree->topLevelItem(i);
    if (needle.isEmpty()) {
      item->setHidden(false);
      continue;
    }
    const GitCommandRecord &record =
        m_records.at(item->data(0, RECORD_INDEX_ROLE).toInt());
    const bool matches =
        record.commandLine().contains(needle, Qt::CaseInsensitive) ||
        record.output.contains(needle, Qt::CaseInsensitive) ||
        record.error.contains(needle, Qt::CaseInsensitive);
    item->setHidden(!matches);
  }
}

const GitCommandRecord *CommandMirrorDialog::currentRecord() const {
  QTreeWidgetItem *item = m_commandTree->currentItem();
  if (!item) {
    return nullptr;
  }
  const int index = item->data(0, RECORD_INDEX_ROLE).toInt();
  if (index < 0 || index >= m_records.size()) {
    return nullptr;
  }
  return &m_records.at(index);
}

void CommandMirrorDialog::onSelectionChanged() {
  const GitCommandRecord *record = currentRecord();
  m_copyButton->setEnabled(record != nullptr);

  if (!record) {
    m_explanationLabel->setText(tr("Select a command to see what it changed."));
    m_riskLabel->clear();
    m_outputView->clear();
    return;
  }

  m_explanationLabel->setText(
      tr("<b>%1</b><br>in <code>%2</code>")
          .arg(gitCommandExplanation(record->args),
               record->workingDirectory.toHtmlEscaped()));

  const GitOperationRisk risk = gitCommandRisk(record->args);
  m_riskLabel->setText(
      risk == GitOperationRisk::Safe
          ? tr("Classified: %1").arg(gitOperationRiskName(risk))
          : tr("⚠ Classified: %1 — the same classification the operation "
               "preview uses.")
                .arg(gitOperationRiskName(risk)));

  QStringList text;
  if (!record->output.trimmed().isEmpty()) {
    text << record->output.trimmed();
  }
  if (!record->error.trimmed().isEmpty()) {
    text << tr("[stderr]") << record->error.trimmed();
  }
  if (text.isEmpty()) {
    text << tr("(no output)");
  }
  m_outputView->setPlainText(text.join(QStringLiteral("\n")));

  applyTheme(m_theme);
}

void CommandMirrorDialog::onModeChanged(int index) {
  Q_UNUSED(index);
  GitIntegration::setMirrorMode(mode());
}

void CommandMirrorDialog::onCopy() {
  if (const GitCommandRecord *record = currentRecord()) {
    QGuiApplication::clipboard()->setText(record->commandLine());
  }
}

void CommandMirrorDialog::onClear() {
  m_records.clear();
  m_commandTree->clear();
  if (m_git) {
    m_git->clearCommandHistory();
  }
  onSelectionChanged();
}

void CommandMirrorDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_commandTree) {
    m_commandTree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
  if (m_modeCombo) {
    m_modeCombo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }
  if (m_searchEdit) {
    m_searchEdit->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  }
  if (m_outputView) {
    m_outputView->setStyleSheet(
        QString("QPlainTextEdit { background: %1; color: %2; border: 1px "
                "solid %3; }")
            .arg(theme.surfaceColor.name(), theme.foregroundColor.name(),
                 theme.borderColor.name()));
  }
  for (const char *name :
       {"mirrorHeaderLabel", "mirrorModeLabel", "mirrorExplanationLabel"}) {
    if (QLabel *label = findChild<QLabel *>(QString::fromLatin1(name))) {
      styleSubduedLabel(label);
    }
  }
  if (m_riskLabel) {
    const GitCommandRecord *record = currentRecord();
    const GitOperationRisk risk =
        record ? gitCommandRisk(record->args) : GitOperationRisk::Safe;
    m_riskLabel->setStyleSheet(
        QString("color: %1;")
            .arg((risk == GitOperationRisk::Safe ? theme.successColor
                  : risk == GitOperationRisk::RewritesLocalHistory
                      ? theme.warningColor
                      : theme.errorColor)
                     .name()));
  }
  for (QPushButton *button : {m_copyButton, m_clearButton, m_closeButton}) {
    if (button) {
      styleSecondaryButton(button);
    }
  }
}
