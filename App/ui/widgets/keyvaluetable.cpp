#include "keyvaluetable.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

KeyValueTable::KeyValueTable(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  m_table = new QTableWidget(0, 2, this);
  m_table->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
  m_table->horizontalHeader()->setSectionResizeMode(0,
                                                    QHeaderView::Interactive);
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->horizontalHeader()->resizeSection(0, 180);
  m_table->verticalHeader()->setVisible(false);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::AllEditTriggers);
  m_table->setAccessibleName(tr("Environment variables"));
  layout->addWidget(m_table);

  auto *buttons = new QHBoxLayout();
  buttons->setSpacing(6);
  m_addButton = new QPushButton(tr("Add Variable"), this);
  m_removeButton = new QPushButton(tr("Remove"), this);
  m_removeButton->setEnabled(false);
  buttons->addWidget(m_addButton);
  buttons->addWidget(m_removeButton);
  buttons->addStretch();
  layout->addLayout(buttons);

  connect(m_addButton, &QPushButton::clicked, this, [this]() {
    addRow();
    const int row = m_table->rowCount() - 1;
    m_table->setCurrentCell(row, 0);
    m_table->editItem(m_table->item(row, 0));
    emit changed();
  });
  connect(m_removeButton, &QPushButton::clicked, this,
          &KeyValueTable::removeSelectedRows);
  connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
    m_removeButton->setEnabled(!m_table->selectedItems().isEmpty());
  });
  connect(m_table, &QTableWidget::itemChanged, this, &KeyValueTable::changed);
  updateHeight();
}

void KeyValueTable::setHeaderLabels(const QString &key, const QString &value) {
  m_table->setHorizontalHeaderLabels({key, value});
}

void KeyValueTable::setValues(const QMap<QString, QString> &values) {
  const QSignalBlocker blocker(m_table);
  m_table->setRowCount(0);
  for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
    addRow(it.key(), it.value());
  }
  updateHeight();
}

QMap<QString, QString> KeyValueTable::values() const {
  QMap<QString, QString> result;
  for (int row = 0; row < m_table->rowCount(); ++row) {
    const QTableWidgetItem *key = m_table->item(row, 0);
    const QTableWidgetItem *value = m_table->item(row, 1);
    const QString name = key ? key->text().trimmed() : QString();
    if (!name.isEmpty()) {
      result.insert(name, value ? value->text() : QString());
    }
  }
  return result;
}

void KeyValueTable::addRow(const QString &key, const QString &value) {
  const QSignalBlocker blocker(m_table);
  const int row = m_table->rowCount();
  m_table->insertRow(row);
  m_table->setItem(row, 0, new QTableWidgetItem(key));
  m_table->setItem(row, 1, new QTableWidgetItem(value));
  updateHeight();
}

void KeyValueTable::removeSelectedRows() {
  QSet<int> rows;
  for (const QTableWidgetItem *item : m_table->selectedItems()) {
    rows.insert(item->row());
  }
  QList<int> sorted = rows.values();
  std::sort(sorted.begin(), sorted.end(), std::greater<int>());
  for (int row : sorted) {
    m_table->removeRow(row);
  }
  updateHeight();
  emit changed();
}

void KeyValueTable::updateHeight() {
  // Grow with the rows (up to six) so short lists do not waste space.
  const int rows = qBound(2, m_table->rowCount() + 1, 6);
  const int rowHeight = m_table->verticalHeader()->defaultSectionSize();
  m_table->setFixedHeight(m_table->horizontalHeader()->sizeHint().height() +
                          rows * rowHeight + 2 * m_table->frameWidth());
}
