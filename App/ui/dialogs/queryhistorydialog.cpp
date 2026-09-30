#include "queryhistorydialog.h"

#include "../panels/dbglyphs.h"
#include "themedmessagebox.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QSplitter>
#include <QVBoxLayout>

QueryHistoryDialog::QueryHistoryDialog(QueryHistory *history, QWidget *parent)
    : StyledDialog(parent), m_history(history) {
  setWindowTitle(tr("Query History"));
  resize(820, 560);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(16, 14, 16, 14);
  root->setSpacing(10);

  auto *filters = new QHBoxLayout();
  m_search = new QLineEdit(this);
  m_search->setPlaceholderText(tr("Search statements and errors…"));
  m_search->setClearButtonEnabled(true);
  m_connection = new QComboBox(this);
  m_connection->addItem(tr("All connections"), QString());
  filters->addWidget(m_search, 1);
  filters->addWidget(m_connection);
  root->addLayout(filters);

  auto *split = new QSplitter(Qt::Vertical, this);
  m_list = new QListWidget(split);
  m_list->setUniformItemSizes(false);
  auto *bottom = new QWidget(split);
  auto *bl = new QVBoxLayout(bottom);
  bl->setContentsMargins(0, 6, 0, 0);
  m_meta = new QLabel(bottom);
  m_preview = new QPlainTextEdit(bottom);
  m_preview->setReadOnly(true);
  m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  bl->addWidget(m_meta);
  bl->addWidget(m_preview, 1);
  split->addWidget(m_list);
  split->addWidget(bottom);
  split->setStretchFactor(0, 3);
  split->setStretchFactor(1, 2);
  root->addWidget(split, 1);

  auto *buttons = new QHBoxLayout();
  m_clear = new QPushButton(tr("Clear history"), this);
  m_copy = new QPushButton(tr("Copy"), this);
  m_insert = new QPushButton(tr("Insert in editor"), this);
  m_run = new QPushButton(tr("Run again"), this);
  auto *close = new QPushButton(tr("Close"), this);
  buttons->addWidget(m_clear);
  buttons->addStretch();
  buttons->addWidget(m_copy);
  buttons->addWidget(m_insert);
  buttons->addWidget(m_run);
  buttons->addWidget(close);
  root->addLayout(buttons);
  setKeyboardDefault(m_insert);

  connect(close, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_search, &QLineEdit::textChanged, this, &QueryHistoryDialog::reload);
  connect(m_connection, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &QueryHistoryDialog::reload);
  connect(m_list, &QListWidget::currentRowChanged, this,
          &QueryHistoryDialog::onSelectionChanged);
  connect(m_list, &QListWidget::itemDoubleClicked, this, [this]() {
    if (const auto *e = selectedEntry()) {
      emit insertRequested(e->sql);
      accept();
    }
  });
  connect(m_insert, &QPushButton::clicked, this, [this]() {
    if (const auto *e = selectedEntry()) {
      emit insertRequested(e->sql);
      accept();
    }
  });
  connect(m_run, &QPushButton::clicked, this, [this]() {
    if (const auto *e = selectedEntry()) {
      emit runRequested(e->sql, e->connection);
      accept();
    }
  });
  connect(m_copy, &QPushButton::clicked, this, [this]() {
    if (const auto *e = selectedEntry()) {
      QApplication::clipboard()->setText(e->sql);
    }
  });
  connect(m_clear, &QPushButton::clicked, this, &QueryHistoryDialog::clearAll);
  reload();
}

void QueryHistoryDialog::setConnectionNames(const QStringList &names) {
  const QString keep = m_connection->currentData().toString();
  m_connection->blockSignals(true);
  m_connection->clear();
  m_connection->addItem(tr("All connections"), QString());
  for (const QString &n : names) {
    m_connection->addItem(n, n);
  }
  const int idx = m_connection->findData(keep);
  m_connection->setCurrentIndex(idx >= 0 ? idx : 0);
  m_connection->blockSignals(false);
  reload();
}

void QueryHistoryDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  stylePrimaryButton(m_insert);
  styleSubduedLabel(m_meta);
  styleDangerButton(m_clear);
  reload();
}

void QueryHistoryDialog::reload() {
  m_shown = m_history->search(m_search->text().trimmed(),
                              m_connection->currentData().toString());
  m_list->clear();
  const QColor ok =
      m_theme.successColor.isValid() ? m_theme.successColor : QColor("#3fb950");
  const QColor bad =
      m_theme.errorColor.isValid() ? m_theme.errorColor : QColor("#f85149");
  for (const QueryHistoryEntry &e : m_shown) {
    QString firstLine = e.sql.section('\n', 0, 0).simplified();
    if (firstLine.size() > 110) {
      firstLine = firstLine.left(109) + QChar(0x2026);
    }
    auto *item = new QListWidgetItem(
        DbGlyphs::dot(e.ok ? ok : bad, 14),
        QStringLiteral("%1\n%2 · %3 · %4")
            .arg(firstLine,
                 e.when.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 e.connection.isEmpty() ? tr("no connection") : e.connection,
                 e.elapsedMs >= 1000
                     ? tr("%1 s").arg(e.elapsedMs / 1000.0, 0, 'f', 2)
                     : tr("%1 ms").arg(e.elapsedMs)),
        m_list);
    item->setToolTip(e.sql.left(1500));
  }
  if (!m_shown.isEmpty()) {
    m_list->setCurrentRow(0);
  } else {
    onSelectionChanged();
  }
}

const QueryHistoryEntry *QueryHistoryDialog::selectedEntry() const {
  const int row = m_list->currentRow();
  return row >= 0 && row < m_shown.size() ? &m_shown[row] : nullptr;
}

void QueryHistoryDialog::onSelectionChanged() {
  const QueryHistoryEntry *e = selectedEntry();
  m_insert->setEnabled(e);
  m_run->setEnabled(e);
  m_copy->setEnabled(e);
  if (!e) {
    m_preview->clear();
    m_meta->setText(m_history->entries().isEmpty()
                        ? tr("Nothing has been run yet.")
                        : tr("No statement matches."));
    return;
  }
  m_preview->setPlainText(e->error.isEmpty()
                              ? e->sql
                              : e->sql + QStringLiteral("\n\n-- ") + e->error);
  QString meta = e->ok ? tr("Succeeded") : tr("Failed");
  if (e->rows >= 0) {
    meta += tr(" · %1 row(s)").arg(e->rows);
  }
  m_meta->setText(meta);
}

void QueryHistoryDialog::clearAll() {
  if (ThemedMessageBox::question(this, tr("Clear history"),
                                 tr("Delete all %1 saved statements?")
                                     .arg(m_history->entries().size())) ==
      ThemedMessageBox::Yes) {
    m_history->clear();
    m_history->save();
    reload();
  }
}
