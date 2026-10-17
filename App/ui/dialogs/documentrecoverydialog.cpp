#include "documentrecoverydialog.h"
#include "../../theme/colorcontrast.h"
#include "themedmessagebox.h"
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
QString key(const Recovery::Draft &draft) {
  return draft.sessionId + '/' + draft.id;
}
} // namespace
DocumentRecoveryDialog::DocumentRecoveryDialog(QWidget *parent)
    : StyledDialog(parent) {
  setObjectName("documentRecoveryDialog");
  setWindowTitle(tr("Recover Unsaved Documents"));
  resize(1020, 660);
  setMinimumSize(700, 460);
  setModal(false);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(22, 20, 22, 18);
  layout->setSpacing(12);
  m_title = new QLabel(tr("Recover unsaved work"), this);
  layout->addWidget(m_title);
  m_description = new QLabel(tr("Restore drafts into new unsaved tabs. Save "
                                "each recovered document when you are ready."),
                             this);
  m_description->setWordWrap(true);
  layout->addWidget(m_description);
  auto *bar = new QHBoxLayout;
  m_filter = new QLineEdit(this);
  m_filter->setObjectName("recoveryFilter");
  m_filter->setPlaceholderText(
      tr("Filter documents by name or original path…"));
  m_filter->setAccessibleName(tr("Filter recovery documents"));
  m_filter->setClearButtonEnabled(true);
  bar->addWidget(m_filter, 1);
  auto *refresh = new QPushButton(tr("Refresh"), this);
  refresh->setObjectName("refreshRecovery");
  bar->addWidget(refresh);
  layout->addLayout(bar);
  auto *split = new QSplitter(Qt::Horizontal, this);
  m_list = new QTreeWidget(split);
  m_list->setObjectName("recoveryDocuments");
  m_list->setHeaderLabels({tr("Document"), tr("Recovery saved")});
  m_list->setRootIsDecorated(false);
  m_list->setUniformRowHeights(true);
  m_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_list->setMinimumWidth(260);
  m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  auto *previewPanel = new QWidget(split);
  auto *previewLayout = new QVBoxLayout(previewPanel);
  previewLayout->setContentsMargins(10, 0, 0, 0);
  previewLayout->setSpacing(8);
  m_details = new QLabel(this);
  m_details->setObjectName("recoveryDetails");
  m_details->setTextFormat(Qt::PlainText);
  m_details->setWordWrap(true);
  m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
  previewLayout->addWidget(m_details);
  m_preview = new QPlainTextEdit(this);
  m_preview->setObjectName("recoveryPreview");
  m_preview->setReadOnly(true);
  m_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_preview->setAccessibleName(tr("Recovered text preview"));
  previewLayout->addWidget(m_preview, 1);
  split->addWidget(m_list);
  split->addWidget(previewPanel);
  split->setSizes({420, 570});
  layout->addWidget(split, 1);
  auto *bottom = new QHBoxLayout;
  m_status = new QLabel(this);
  m_status->setObjectName("recoveryStatus");
  m_status->setTextFormat(Qt::PlainText);
  m_status->setWordWrap(true);
  bottom->addWidget(m_status, 1);
  m_warningButton = new QPushButton(tr("Details"), this);
  m_warningButton->setObjectName("recoveryWarnings");
  bottom->addWidget(m_warningButton);
  auto *later = new QPushButton(tr("Later"), this);
  later->setObjectName("recoverLater");
  later->setToolTip(tr("Keep these recovery copies and close this window"));
  bottom->addWidget(later);
  m_discardButton = new QPushButton(tr("Discard selected"), this);
  m_discardButton->setObjectName("discardRecovery");
  bottom->addWidget(m_discardButton);
  m_restoreButton = new QPushButton(tr("Restore selected"), this);
  m_restoreButton->setObjectName("restoreRecovery");
  bottom->addWidget(m_restoreButton);
  layout->addLayout(bottom);
  connect(refresh, &QPushButton::clicked, this,
          &DocumentRecoveryDialog::refresh);
  connect(later, &QPushButton::clicked, this, &QDialog::reject);
  connect(
      m_filter, &QLineEdit::textChanged, this, [this](const QString &filter) {
        for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
          auto *item = m_list->topLevelItem(i);
          const auto &draft = m_drafts.at(item->data(0, Qt::UserRole).toInt());
          item->setHidden(
              !filter.isEmpty() &&
              !draft.title.contains(filter, Qt::CaseInsensitive) &&
              !draft.originalPath.contains(filter, Qt::CaseInsensitive));
        }
        updateSelection();
      });
  connect(m_list, &QTreeWidget::itemChanged, this,
          [this]() { updateSelection(); });
  connect(m_list, &QTreeWidget::currentItemChanged, this,
          [this]() { showPreview(); });
  connect(m_restoreButton, &QPushButton::clicked, this,
          [this]() { runAction(true); });
  connect(m_discardButton, &QPushButton::clicked, this,
          [this]() { runAction(false); });
  connect(m_warningButton, &QPushButton::clicked, this, [this]() {
    ThemedMessageBox box(this);
    box.setWindowTitle(tr("Recovery Details"));
    box.setIcon(ThemedMessageBox::Warning);
    box.setText(tr("Some recovery copies could not be read or processed. They "
                   "have been retained."));
    box.setDetailedText(m_warnings.join('\n'));
    box.setStandardButtons(ThemedMessageBox::Ok);
    box.exec();
  });
  setKeyboardDefault(m_restoreButton);
  updateSelection();
}
void DocumentRecoveryDialog::setActions(
    std::function<Recovery::Scan()> provider, Action restore, Action discard) {
  m_provider = std::move(provider);
  m_restore = std::move(restore);
  m_discard = std::move(discard);
}
void DocumentRecoveryDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  styleTitleLabel(m_title);
  const QString readable =
      ColorContrast::ensure(UIStyleHelper::secondaryTextColor(theme),
                            theme.surfaceColor)
          .name();
  for (auto *label : {m_description, m_details})
    label->setStyleSheet("background: transparent; color: " + readable + ";");
  stylePrimaryButton(m_restoreButton);
  styleDangerButton(m_discardButton);
}
void DocumentRecoveryDialog::refresh() {
  QSet<QString> unchecked;
  for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
    auto *item = m_list->topLevelItem(i);
    const auto &draft = m_drafts.at(item->data(0, Qt::UserRole).toInt());
    if (item->checkState(0) != Qt::Checked)
      unchecked.insert(key(draft));
  }
  const QSignalBlocker blocker(m_list);
  const auto result = m_provider ? m_provider() : Recovery::Scan{};
  m_drafts = result.drafts;
  m_warnings = result.warnings;
  m_list->clear();
  for (int i = 0; i < m_drafts.size(); ++i) {
    const auto &draft = m_drafts.at(i);
    auto *item = new QTreeWidgetItem(m_list);
    const QString name = draft.title.isEmpty() ? tr("Untitled") : draft.title;
    item->setText(0, name);
    item->setText(1, QLocale().toString(draft.updated.toLocalTime(),
                                        QLocale::ShortFormat));
    item->setToolTip(0, draft.originalPath.isEmpty() ? tr("Untitled document")
                                                     : draft.originalPath);
    item->setData(0, Qt::UserRole, i);
    item->setCheckState(0, unchecked.contains(key(draft)) ? Qt::Unchecked
                                                          : Qt::Checked);
    item->setHidden(
        !m_filter->text().isEmpty() &&
        !draft.title.contains(m_filter->text(), Qt::CaseInsensitive) &&
        !draft.originalPath.contains(m_filter->text(), Qt::CaseInsensitive));
  }
  if (m_list->topLevelItemCount())
    m_list->setCurrentItem(m_list->topLevelItem(0));
  showPreview();
  updateSelection();
  m_status->setText(
      m_warnings.isEmpty()
          ? (m_drafts.isEmpty() ? tr("No recoverable drafts. Your open windows "
                                     "keep their own drafts.")
                                : tr("%1 recovery copies available. Original "
                                     "files are left unchanged.")
                                      .arg(m_drafts.size()))
          : tr("%1 recovery copies available · %2 recovery warnings")
                .arg(m_drafts.size())
                .arg(m_warnings.size()));
  m_warningButton->setVisible(!m_warnings.isEmpty());
}
QVector<Recovery::Draft> DocumentRecoveryDialog::selected() const {
  QVector<Recovery::Draft> result;
  for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
    auto *item = m_list->topLevelItem(i);
    if (!item->isHidden() && item->checkState(0) == Qt::Checked)
      result.append(m_drafts.at(item->data(0, Qt::UserRole).toInt()));
  }
  return result;
}
void DocumentRecoveryDialog::updateSelection() {
  const int count = selected().size();
  m_restoreButton->setEnabled(count > 0 && bool(m_restore));
  m_discardButton->setEnabled(count > 0 && bool(m_discard));
  m_restoreButton->setText(count == 1 ? tr("Restore 1 draft")
                                      : tr("Restore %1 drafts").arg(count));
}
void DocumentRecoveryDialog::showPreview() {
  auto *item = m_list->currentItem();
  if (!item) {
    m_preview->clear();
    m_details->setText(tr("Select a document to preview its recovered text."));
    return;
  }
  const auto &draft = m_drafts.at(item->data(0, Qt::UserRole).toInt());
  constexpr int previewCharacters = 256 * 1024;
  m_preview->setPlainText(draft.text.left(previewCharacters));
  QString details = draft.originalPath.isEmpty()
                        ? tr("Untitled document")
                        : tr("Original: %1").arg(draft.originalPath);
  details +=
      tr("\n%1 characters · %2")
          .arg(draft.text.size())
          .arg(draft.format.encoding == FileOpenGuard::TextEncoding::Latin1
                   ? "Latin-1"
               : draft.format.hasBom ? "UTF-8 with BOM"
                                     : "UTF-8");
  if (!draft.originalPath.isEmpty() && !QFileInfo::exists(draft.originalPath))
    details += tr(" · Original file is missing");
  if (draft.text.size() > previewCharacters)
    details += tr("\nPreview shows the first %1 characters; Restore keeps the "
                  "full text.")
                   .arg(previewCharacters);
  m_details->setText(details);
}
void DocumentRecoveryDialog::runAction(bool restore) {
  const auto drafts = selected();
  if (drafts.isEmpty())
    return;
  if (!restore) {
    ThemedMessageBox box(this);
    box.setWindowTitle(tr("Discard Recovery Copies"));
    box.setIcon(ThemedMessageBox::Warning);
    box.setText(tr("Discard %1 selected recovery copies?").arg(drafts.size()));
    box.setInformativeText(
        tr("Their unsaved text will no longer be available for recovery."));
    box.setStandardButtons(ThemedMessageBox::Yes | ThemedMessageBox::No);
    box.setButtonText(ThemedMessageBox::Yes, tr("Discard"));
    box.setButtonText(ThemedMessageBox::No, tr("Cancel"));
    box.setDefaultButton(ThemedMessageBox::No);
    if (box.exec() != ThemedMessageBox::Yes)
      return;
  }
  int completed = 0;
  QStringList errors;
  for (const auto &draft : drafts) {
    QString error;
    const auto &action = restore ? m_restore : m_discard;
    if (action && action(draft, &error))
      ++completed;
    else
      errors.append(error.isEmpty()
                        ? tr("Could not process %1.").arg(draft.title)
                        : error);
  }
  refresh();
  m_warnings.append(errors);
  m_warningButton->setVisible(!m_warnings.isEmpty());
  m_status->setText((restore ? tr("Restored %1 drafts into unsaved tabs.")
                             : tr("Discarded %1 recovery copies."))
                        .arg(completed) +
                    (errors.isEmpty()
                         ? QString()
                         : tr(" %1 copies could not be processed; see Details.")
                               .arg(errors.size())));
}
