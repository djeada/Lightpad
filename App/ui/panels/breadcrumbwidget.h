#ifndef BREADCRUMBWIDGET_H
#define BREADCRUMBWIDGET_H

#include "../../settings/theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QWidget>

class BreadcrumbWidget : public QWidget {
  Q_OBJECT

public:
  explicit BreadcrumbWidget(QWidget *parent = nullptr);
  ~BreadcrumbWidget();

  void setFilePath(const QString &filePath);

  void setProjectRoot(const QString &rootPath);

  void clear();

  void applyTheme(const Theme &theme);

signals:

  void pathSegmentClicked(const QString &fullPath);

  void siblingRequested(const QString &siblingPath);

private slots:
  void onSegmentClicked();
  void onDropdownClicked();

private:
  void setupUI();
  void rebuildBreadcrumbs();
  QStringList getPathSegments(const QString &path) const;
  QString buildPathUpTo(int segmentIndex) const;

  QHBoxLayout *m_layout;
  QString m_filePath;
  QString m_projectRoot;
  QList<QPushButton *> m_segments;
  QList<QLabel *> m_separators;
  Theme m_theme;
};

#endif
