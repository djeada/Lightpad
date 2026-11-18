#ifndef KEYVALUETABLE_H
#define KEYVALUETABLE_H

#include <QMap>
#include <QWidget>

class QPushButton;
class QTableWidget;

// Editable NAME = value list (environment variables) with Add/Remove buttons.
class KeyValueTable : public QWidget {
  Q_OBJECT

public:
  explicit KeyValueTable(QWidget *parent = nullptr);

  void setValues(const QMap<QString, QString> &values);
  // Rows with an empty name are ignored.
  QMap<QString, QString> values() const;

  void setHeaderLabels(const QString &key, const QString &value);
  QTableWidget *table() const { return m_table; }

signals:
  void changed();

private:
  void addRow(const QString &key = {}, const QString &value = {});
  void removeSelectedRows();
  void updateHeight();

  QTableWidget *m_table;
  QPushButton *m_addButton;
  QPushButton *m_removeButton;
};

#endif
