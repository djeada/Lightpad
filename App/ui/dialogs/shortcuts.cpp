#include "shortcuts.h"
#include "../mainwindow.h"
#include "ui_shortcuts.h"
#include <QDebug>
#include <QKeyEvent>

const int NUM_ROWS = 10;
const int NUM_COLS = 2;

QList<QList<QString>> shortcuts{
    {"Close Tab", "Ctrl + W"},
    {"Save", "Ctrl + S"},
    {"SaveAs", "Ctrl + Shift + S"},
    {"Toggle Source Control", "Ctrl + Shift + G"},
    {"Delete", "Del"},
    {"Cut", "Ctrl + X"},
    {"Copy", "Ctrl + C"},
    {"Paste", "Ctrl + V"},
    {"Undo", "Ctrl + Z"},
    {"Redo", "Ctrl + Shift + Z"},
    {"Increase Font Size", "Ctrl + Plus"},
    {"Decrease Font Size", "Ctrl + Minus"},
    {"AddTab", "Ctrl + T"},
    {"Find", "Ctrl + F"},
    {"Find Previous", "Shift + F3"},
    {"Replace", "Ctrl + H"},
    {"Move To Next Char", "Right"},
    {"Move To Previous Char", "Left"},
    {"Move To Next Word", "Ctrl + Right"},
    {"Move To Previous Word", "Ctrl + Left"},
    {"Move To Next Line", "Down"},
    {"Move To Previous Line", "Up"},
    {"Move To Start Of Line", "PgDown"},
    {"Move To End Of Line", "PgUp"},
    {"Move To Start Of Document", "Ctrl + Home"},
    {"Move To End Of Document", "Ctrl + End"},
    {"Select Next Char", "Shift + Right"},
    {"Select Previous Char", "Shift + Left"},
    {"Select Next Word", "Ctrl + Shift + Right"},
    {"Select Previous Word", "Ctrl + Shift + Left"},
    {"Select Next Line", "Shift + Down"},
    {"Select Previous Line", "Shift + Up"},
    {"Select Start Of Line", "Shift + PgDown"},
    {"Select End Of Line", "Shift + PgUp"},
};

static int strToKey(QString str) {

  int i = str.lastIndexOf(" ");
  QKeySequence seq = QKeySequence(str.mid(i + 1, str.size()));

  if (seq.count() != 1)
    return -1;

  return seq[0].key();
}

static Qt::KeyboardModifiers strToModifiers(QString str) {
  Qt::KeyboardModifiers modifiers;

  if (str.contains("Alt"))
    modifiers |= Qt::AltModifier;

  if (str.contains("Shift"))
    modifiers |= Qt::ShiftModifier;

  if (str.contains("Ctrl"))
    modifiers |= Qt::ControlModifier;

  return modifiers;
}

ShortcutsModel::ShortcutsModel(QObject *parent)
    : QAbstractTableModel(parent), parentWindow(nullptr) {}

int ShortcutsModel::rowCount(const QModelIndex &idx) const {
  Q_UNUSED(idx);
  return NUM_ROWS;
}

int ShortcutsModel::columnCount(const QModelIndex &idx) const {
  Q_UNUSED(idx);
  return NUM_COLS;
}

QVariant ShortcutsModel::data(const QModelIndex &index, int role) const {
  if (role == Qt::DisplayRole) {
    int i = 2 * parentWindow->getSelectedButton() * (NUM_ROWS - 1) +
            index.row() + (NUM_ROWS - 1) * index.column();

    if (i < shortcuts.size())
      return QString(shortcuts[i][0] + " : " + shortcuts[i][1]);
  }

  return QVariant();
}

void ShortcutsModel::setParentWindow(ShortcutsDialog *window) {
  parentWindow = window;
}

ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ShortcutsDialog) {
  ui->setupUi(this);
  setWindowTitle("Shortcuts");
  myModel.setParentWindow(this);
  ui->tableView->setModel(&myModel);
  ui->tableView->horizontalHeader()->hide();
  ui->tableView->verticalHeader()->hide();
  ui->tableView->horizontalHeader()->setStretchLastSection(true);
  ui->tableView->setFocusPolicy(Qt::NoFocus);
  ui->radioButton->setChecked(true);

  auto allButtons = findChildren<QRadioButton *>();
  for (int i = 0; i < allButtons.size(); i++) {
    radioButtonsGroup.addButton(allButtons[i], i);
    connect(allButtons[i], &QAbstractButton::clicked, this,
            [this] { ui->tableView->update(); });
  }

  show();

  connect(ui->tableView, &QAbstractItemView::clicked, this,
          [this](const QModelIndex &index) {
            auto data = index.data().toString();
            auto modifiers = strToModifiers(data);
            auto key = strToKey(data);
            auto event = new QKeyEvent(QEvent::KeyPress, key, modifiers);
            auto window = qobject_cast<MainWindow *>(parentWidget());
            window->setFocus();
            QCoreApplication::postEvent(window, event, Qt::HighEventPriority);
          });

  setAttribute(Qt::WA_DeleteOnClose);
}

ShortcutsDialog::~ShortcutsDialog() { delete ui; }

int ShortcutsDialog::getSelectedButton() {
  return radioButtonsGroup.checkedId();
}

void ShortcutsDialog::resizeEvent(QResizeEvent *event) {
  ui->tableView->setColumnWidth(0, width() / 2);
  ui->tableView->setColumnWidth(1, width() / 2);
  QDialog::resizeEvent(event);
}
