#include "gitui.h"
#include "uimetrics.h"
#include "uistylehelper.h"
#include "widgets/flowlayout.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTableWidget>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVariant>

namespace {
// Nested QBoxLayouts can retain a one-line size hint after a flow row wraps.
// Keep its minimum height tied to the actual available width, including after
// theme/font changes, so the next row never paints over an action.
class ActionGroup : public QWidget {
public:
  explicit ActionGroup(QWidget *parent) : QWidget(parent) {
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
  }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override {
    return layout() ? layout()->heightForWidth(width) : 0;
  }

protected:
  bool event(QEvent *event) override {
    const bool result = QWidget::event(event);
    if (layout() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show ||
         event->type() == QEvent::LayoutRequest)) {
      const int needed = qMax(0, heightForWidth(width()));
      if (minimumHeight() != needed)
        setMinimumHeight(needed);
    }
    return result;
  }
};

class EmptyState : public QObject {
public:
  explicit EmptyState(QAbstractItemView *view)
      : QObject(view->viewport()), m_view(view) {
    setObjectName("gitEmptyStateOwner");
    m_label = new QLabel(view->viewport());
    m_label->setObjectName("gitEmptyState");
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    m_label->setTextFormat(Qt::PlainText);
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    view->viewport()->installEventFilter(this);
    auto *model = view->model();
    connect(model, &QAbstractItemModel::rowsInserted, this,
            [this]() { update(); });
    connect(model, &QAbstractItemModel::rowsRemoved, this,
            [this]() { update(); });
    connect(model, &QAbstractItemModel::modelReset, this,
            [this]() { update(); });
    connect(model, &QAbstractItemModel::layoutChanged, this,
            [this]() { update(); });
  }

  void setText(const QString &text, const Theme &theme) {
    m_label->setText(text);
    m_label->setStyleSheet(UIStyleHelper::emptyStateStyle(theme));
    update();
  }

protected:
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
      update();
    return false;
  }

private:
  void update() {
    m_label->setGeometry(m_view->viewport()->rect().adjusted(16, 12, -16, -12));
    m_label->setVisible(m_view->model()->rowCount(m_view->rootIndex()) == 0);
    m_label->raise();
  }
  QAbstractItemView *m_view;
  QLabel *m_label;
};

} // namespace

void GitUi::emptyState(QAbstractItemView *view, const QString &message,
                       const Theme &theme) {
  auto *state =
      dynamic_cast<EmptyState *>(view->viewport()->findChild<QObject *>(
          "gitEmptyStateOwner", Qt::FindDirectChildrenOnly));
  if (!state) {
    state = new EmptyState(view);
  }
  state->setText(message, theme);
}

void GitUi::wrapActions(QBoxLayout *row) {
  if (!row || row->property("gitActionsWrapped").toBool() ||
      row->direction() != QBoxLayout::LeftToRight)
    return;
  int buttons = 0;
  for (int i = 0; i < row->count(); ++i) {
    if (row->itemAt(i)->layout())
      return;
    if (qobject_cast<QPushButton *>(row->itemAt(i)->widget()))
      ++buttons;
  }
  if (buttons < 3)
    return;

  QList<QWidget *> widgets;
  while (auto *item = row->takeAt(0)) {
    if (item->widget())
      widgets << item->widget();
    delete item;
  }
  auto *host = new ActionGroup(row->parentWidget());
  host->setObjectName("gitActionGroup");
  auto *flow = new FlowLayout(host, 0, UiMetrics::SpaceMd, UiMetrics::SpaceMd);
  QList<QWidget *> trailing;
  bool afterDismiss = false;
  for (auto *widget : widgets) {
    if (widget->property("gitDismiss").toBool())
      afterDismiss = true;
    if (afterDismiss)
      trailing << widget;
    else
      flow->addWidget(widget);
  }
  row->addWidget(host, 1);
  for (auto *widget : trailing)
    row->addWidget(widget);
  row->setAlignment(Qt::AlignVCenter);
  row->setProperty("gitActionsWrapped", true);
}

void GitUi::polish(QWidget *surface, const Theme &theme) {
  QPalette palette = surface->palette();
  palette.setColor(QPalette::Window, theme.backgroundColor);
  palette.setColor(QPalette::Base, theme.backgroundColor);
  palette.setColor(QPalette::WindowText, theme.foregroundColor);
  palette.setColor(QPalette::Text, theme.foregroundColor);
  surface->setPalette(palette);
  const QString scrollStyle = UIStyleHelper::scrollBarStyle(theme);
  for (auto *bar : surface->findChildren<QScrollBar *>())
    bar->setStyleSheet(scrollStyle);
  for (auto *menu : surface->findChildren<QMenu *>())
    menu->setStyleSheet(UIStyleHelper::contextMenuStyle(theme));
  for (auto *area : surface->findChildren<QScrollArea *>()) {
    area->viewport()->setPalette(palette);
    if (area->widget())
      area->widget()->setPalette(palette);
  }
  for (auto *button : surface->findChildren<QPushButton *>()) {
    if (!button->isDefault())
      button->setAutoDefault(false);
    if (button->maximumHeight() < UiMetrics::ControlHeight)
      button->setFixedHeight(UiMetrics::ControlHeight);
    else
      button->setMinimumHeight(UiMetrics::ControlHeight);
    if (button->maximumWidth() < UiMetrics::ControlHeight)
      button->setFixedWidth(UiMetrics::ControlHeight);
    if (button->accessibleName().isEmpty())
      button->setAccessibleName(button->text().size() <= 2 &&
                                        !button->toolTip().isEmpty()
                                    ? button->toolTip()
                                    : button->text().remove('&'));
    const QString role = button->property("uiRole").toString();
    button->setStyleSheet(
        role == "danger"    ? UIStyleHelper::dangerButtonStyle(theme)
        : role == "primary" ? UIStyleHelper::primaryButtonStyle(theme)
                            : UIStyleHelper::secondaryButtonStyle(theme));
    const QString caption = button->text().trimmed();
    if (!caption.isEmpty() && caption.size() <= 2 &&
        !caption.front().isLetterOrNumber())
      button->setStyleSheet(button->styleSheet() +
                            "QPushButton { padding: 0; }");
    button->setStyleSheet(
        button->styleSheet() +
        QStringLiteral("QPushButton:disabled { background: transparent; "
                       "border: 1px solid %1; color: %2; }")
            .arg(theme.borderColor.name(),
                 UIStyleHelper::mutedTextColor(theme).name()));
  }
  for (auto *button : surface->findChildren<QToolButton *>()) {
    if (qobject_cast<QLineEdit *>(button->parentWidget()))
      continue;
    if (qobject_cast<QTabBar *>(button->parentWidget())) {
      button->setStyleSheet(UIStyleHelper::iconButtonStyle(theme) +
                            "QToolButton { padding: 0; border-radius: 0; }");
      continue;
    }
    button->setMinimumHeight(UiMetrics::ControlHeight);
    button->setStyleSheet(
        button->icon().isNull() && button->arrowType() == Qt::NoArrow
            ? UIStyleHelper::secondaryButtonStyle(theme).replace("QPushButton",
                                                                 "QToolButton")
            : UIStyleHelper::iconButtonStyle(theme));
  }
  for (auto *edit : surface->findChildren<QLineEdit *>()) {
    if (qobject_cast<QAbstractSpinBox *>(edit->parentWidget()) ||
        qobject_cast<QComboBox *>(edit->parentWidget()))
      continue;
    edit->setClearButtonEnabled(true);
    edit->setMinimumHeight(UiMetrics::ControlHeight);
    edit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }
  for (auto *combo : surface->findChildren<QComboBox *>()) {
    combo->setMinimumHeight(UiMetrics::ControlHeight);
    combo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }
  for (auto *check : surface->findChildren<QCheckBox *>())
    check->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  for (auto *tree : surface->findChildren<QTreeWidget *>()) {
    tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
    tree->setUniformRowHeights(true);
    tree->setAllColumnsShowFocus(true);
    tree->header()->setMinimumSectionSize(48);
    emptyState(tree,
               tree->property("gitEmptyText").isValid()
                   ? tree->property("gitEmptyText").toString()
                   : QObject::tr("No entries to display."),
               theme);
  }
  for (auto *list : surface->findChildren<QListWidget *>()) {
    list->setStyleSheet(UIStyleHelper::listWidgetStyle(theme));
    emptyState(list,
               list->property("gitEmptyText").isValid()
                   ? list->property("gitEmptyText").toString()
                   : QObject::tr("No entries to display."),
               theme);
  }
  for (auto *table : surface->findChildren<QTableWidget *>()) {
    table->setStyleSheet(UIStyleHelper::tableWidgetStyle(theme));
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    emptyState(table,
               table->property("gitEmptyText").isValid()
                   ? table->property("gitEmptyText").toString()
                   : QObject::tr("No entries to display."),
               theme);
  }
  for (auto *splitter : surface->findChildren<QSplitter *>()) {
    splitter->setHandleWidth(4);
    splitter->setChildrenCollapsible(false);
    splitter->setStyleSheet(
        QStringLiteral("QSplitter::handle { background: %1; }")
            .arg(theme.borderColor.name()));
  }
  for (auto *label : surface->findChildren<QLabel *>()) {
    if (label->property("gitHeading").toBool())
      label->setStyleSheet(UIStyleHelper::headingStyle(theme, 17));
    if (label->property("gitDescription").toBool())
      label->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  }
}
