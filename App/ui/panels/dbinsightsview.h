#ifndef DBINSIGHTSVIEW_H
#define DBINSIGHTSVIEW_H

#include "../../database/columnprofiler.h"
#include "../../settings/theme.h"

#include <QLabel>
#include <QTableWidget>
#include <QWidget>

class DbInsightsView : public QWidget {
  Q_OBJECT

public:
  explicit DbInsightsView(QWidget *parent = nullptr);

  void setResult(const DbResultSet &result, const QString &title = QString());
  void clear();
  void applyTheme(const Theme &theme);

  QVector<ColumnProfile> profiles() const { return m_profiles; }
  QTableWidget *table() const { return m_table; }

private:
  QLabel *m_header;
  QLabel *m_empty;
  QTableWidget *m_table;
  QVector<ColumnProfile> m_profiles;
  Theme m_theme;
};

#endif
