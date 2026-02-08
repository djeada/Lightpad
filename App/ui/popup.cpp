#include "popup.h"
#include "mainwindow.h"
#include <QStringListModel>
#include <QVBoxLayout>

ListView::ListView(QWidget *parent) : QListView(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QSize ListView::sizeHint() const {
  if (model()->rowCount() == 0)
    return QSize(width(), 0);

  int nToShow = 10 < model()->rowCount() ? 10 : model()->rowCount();
  return QSize(width(), nToShow * sizeHintForRow(0));
}

Popup::Popup(QStringList list, QWidget *parent) : QDialog(parent), list(list) {
  setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
  auto model = new QStringListModel(this);
  listView = new ListView(this);

  model->setStringList(list);

  listView->setModel(model);
  listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(listView);
  layout->setContentsMargins(0, 0, 0, 0);

  show();
}

PopupTabWidth::PopupTabWidth(QStringList list, QWidget *parent)
    : Popup(list, parent) {

  QObject::connect(
      listView, &QListView::clicked, this, [&](const QModelIndex &index) {
        auto width = index.data().toString();
        auto mainWindow = qobject_cast<MainWindow *>(parentWidget());

        if (mainWindow != 0) {
          mainWindow->setTabWidthLabel("Tab Width: " + width);
          mainWindow->setTabWidth(width.toInt());
        }

        close();
      });
}
