#ifndef COMPLETIONWIDGET_H
#define COMPLETIONWIDGET_H

#include "../settings/theme.h"
#include "completionitem.h"
#include "completionitemmodel.h"
#include <QLabel>
#include <QListView>
#include <QVBoxLayout>
#include <QWidget>

class CompletionWidget : public QWidget {
  Q_OBJECT

public:
  explicit CompletionWidget(QWidget *parent = nullptr);
  void applyTheme(const Theme &theme);

  void setItems(const QList<CompletionItem> &items);

  void showAt(const QPoint &position);

  void hide();

  bool isVisible() const { return QWidget::isVisible(); }

  void selectNext();
  void selectPrevious();
  void selectFirst();
  void selectLast();
  void selectPageDown();
  void selectPageUp();

  CompletionItem selectedItem() const;

  int selectedIndex() const;

  int count() const { return m_model->count(); }

  void setMaxVisibleItems(int count);
  void setShowDocumentation(bool show);
  void setShowIcons(bool show);

signals:

  void itemSelected(const CompletionItem &item);

  void itemAccepted(const CompletionItem &item);

  void cancelled();

protected:
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onSelectionChanged();
  void onItemClicked(const QModelIndex &index);
  void onItemDoubleClicked(const QModelIndex &index);

private:
  void updateDocumentation();
  void adjustSize();

  CompletionItemModel *m_model;
  QListView *m_listView;
  QLabel *m_docLabel;
  QVBoxLayout *m_layout;

  int m_maxVisibleItems = 10;
  bool m_showDocumentation = true;
  bool m_showIcons = true;
};

#endif
