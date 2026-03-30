#include "popup.h"
#include "mainwindow.h"
#include <QStringListModel>
#include <QStyle>
#include <QVBoxLayout>

ListView::ListView(QWidget *parent) : QListView(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
}

QSize ListView::sizeHint() const {
  if (!model() || model()->rowCount() == 0)
    return QListView::sizeHint();

  const int rowCount = model()->rowCount();
  const int rowsToShow = qMin(10, rowCount);
  const int rowHeight =
      qMax(sizeHintForRow(0), fontMetrics().height() + 10);
  const int contentWidth = qMax(sizeHintForColumn(0), 0);
  const int scrollBarWidth =
      rowCount > rowsToShow ? style()->pixelMetric(QStyle::PM_ScrollBarExtent)
                            : 0;
  const int frameWidthPx = frameWidth() * 2;
  const int width = qMax(120, contentWidth + scrollBarWidth + frameWidthPx + 24);
  const int height = rowsToShow * rowHeight + frameWidthPx;

  return QSize(width, height);
}

Popup::Popup(QStringList list, QWidget *parent) : QDialog(parent), list(list) {
  setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
  auto model = new QStringListModel(this);
  listView = new ListView(this);

  model->setStringList(list);

  listView->setModel(model);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(listView);
  layout->setContentsMargins(0, 0, 0, 0);

  resize(sizeHint());
  show();
}

QSize Popup::sizeHint() const {
  if (layout())
    return layout()->sizeHint();
  return QDialog::sizeHint();
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
