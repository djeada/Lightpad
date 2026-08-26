#include "processpickerdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#ifdef Q_OS_LINUX
#include <dirent.h>
#endif

namespace {

bool readTextFile(const QString &path, QString *out) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  *out = QString::fromUtf8(file.readAll());
  return true;
}

QList<ProcessPickerDialog::ProcessEntry> listProcessesFromProcFs() {
  QList<ProcessPickerDialog::ProcessEntry> entries;

  QDir procDir(QStringLiteral("/proc"));
  const QStringList pidNames =
      procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString &pidName : pidNames) {
    bool pidOk = false;
    const qint64 pid = pidName.toLongLong(&pidOk);
    if (!pidOk || pid <= 0) {
      continue;
    }

    QString comm;
    if (!readTextFile(QStringLiteral("/proc/%1/comm").arg(pid), &comm)) {
      continue;
    }
    comm = comm.trimmed();
    if (comm.isEmpty()) {
      continue;
    }

    QString cmdline;
    readTextFile(QStringLiteral("/proc/%1/cmdline").arg(pid), &cmdline);
    cmdline.replace(QLatin1Char('\0'), QLatin1Char(' '));
    cmdline = cmdline.trimmed();
    if (cmdline.isEmpty()) {
      cmdline = comm;
    }

    ProcessPickerDialog::ProcessEntry entry;
    entry.pid = pid;
    entry.name = comm;
    entry.commandLine = cmdline;
    entries.append(entry);
  }

  return entries;
}

QList<ProcessPickerDialog::ProcessEntry> listProcessesFromPs() {
  QList<ProcessPickerDialog::ProcessEntry> entries;

  QProcess ps;
#ifdef Q_OS_WIN
  ps.start(
      QStringLiteral("tasklist"),
      {QStringLiteral("/FO"), QStringLiteral("CSV"), QStringLiteral("/NH")});
#else
  ps.start(QStringLiteral("ps"),
           {QStringLiteral("-eo"), QStringLiteral("pid=,comm=")});
#endif
  if (!ps.waitForStarted(3000) || !ps.waitForFinished(5000)) {
    ps.kill();
    return entries;
  }

  const QString output = QString::fromLocal8Bit(ps.readAllStandardOutput());
  for (const QString &line : output.split(QLatin1Char('\n'))) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }

#ifdef Q_OS_WIN

    const QStringList fields = trimmed.split(QLatin1Char(','));
    if (fields.size() < 2) {
      continue;
    }
    ProcessPickerDialog::ProcessEntry entry;
    entry.name = fields.at(0).remove(QLatin1Char('"'));
    entry.pid = fields.at(1).toLongLong();
    if (entry.pid <= 0) {
      continue;
    }
    entry.commandLine = entry.name;
#else
    const int splitAt = trimmed.indexOf(QLatin1Char(' '));
    if (splitAt <= 0) {
      continue;
    }
    bool pidOk = false;
    ProcessPickerDialog::ProcessEntry entry;
    entry.pid = trimmed.left(splitAt).toLongLong(&pidOk);
    if (!pidOk || entry.pid <= 0) {
      continue;
    }
    entry.name = trimmed.mid(splitAt + 1).trimmed();
    entry.commandLine = entry.name;
#endif
    entries.append(entry);
  }

  return entries;
}

} // namespace

ProcessPickerDialog::ProcessPickerDialog(QWidget *parent)
    : QDialog(parent), m_selectedPid(0) {
  setWindowTitle(tr("Select a Process to Attach"));
  setModal(true);
  resize(640, 480);
  setupUi();
  refresh();
}

void ProcessPickerDialog::setupUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);

  m_filterInput = new QLineEdit(this);
  m_filterInput->setPlaceholderText(tr("Filter by name, PID or command line…"));
  m_filterInput->setClearButtonEnabled(true);
  connect(m_filterInput, &QLineEdit::textChanged, this,
          &ProcessPickerDialog::applyFilter);
  layout->addWidget(m_filterInput);

  m_processTree = new QTreeWidget(this);
  m_processTree->setRootIsDecorated(false);
  m_processTree->setAlternatingRowColors(true);
  m_processTree->setSortingEnabled(true);
  m_processTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_processTree->setColumnCount(3);
  m_processTree->setHeaderLabels({tr("PID"), tr("Name"), tr("Command Line")});
  m_processTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_processTree->sortByColumn(0, Qt::DescendingOrder);
  connect(m_processTree, &QTreeWidget::itemSelectionChanged, this,
          &ProcessPickerDialog::onSelectionChanged);
  connect(m_processTree, &QTreeWidget::itemDoubleClicked, this,
          &ProcessPickerDialog::onProcessDoubleClicked);
  layout->addWidget(m_processTree);

  m_countLabel = new QLabel(this);
  m_countLabel->setStyleSheet(
      QStringLiteral("color: palette(mid); font-size: 11px;"));
  layout->addWidget(m_countLabel);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *refreshButton = new QPushButton(tr("Refresh"), this);
  connect(refreshButton, &QPushButton::clicked, this,
          &ProcessPickerDialog::refresh);
  buttonLayout->addWidget(refreshButton);
  buttonLayout->addStretch();

  QDialogButtonBox *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &ProcessPickerDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this,
          &ProcessPickerDialog::reject);
  buttonLayout->addWidget(buttons);

  layout->addLayout(buttonLayout);
}

void ProcessPickerDialog::refresh() {
  const QString currentFilter = m_filterInput->text();
  m_processes = listRunningProcesses();

  m_processTree->setSortingEnabled(false);
  m_processTree->clear();

  for (const ProcessEntry &entry : std::as_const(m_processes)) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_processTree);
    item->setData(0, Qt::DisplayRole, entry.pid);
    item->setText(1, entry.name);
    item->setText(2, entry.commandLine);
    item->setData(0, Qt::UserRole, entry.pid);
    item->setToolTip(2, entry.commandLine);
  }

  m_processTree->setSortingEnabled(true);
  applyFilter(currentFilter);
}

void ProcessPickerDialog::applyFilter(const QString &text) {
  const QString needle = text.trimmed();
  int visibleCount = 0;

  for (int i = 0; i < m_processTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_processTree->topLevelItem(i);
    const bool matches = needle.isEmpty() ||
                         item->text(0).contains(needle, Qt::CaseInsensitive) ||
                         item->text(1).contains(needle, Qt::CaseInsensitive) ||
                         item->text(2).contains(needle, Qt::CaseInsensitive);
    item->setHidden(!matches);
    if (matches) {
      ++visibleCount;
    }
  }

  m_countLabel->setText(tr("%1 of %2 processes shown")
                            .arg(visibleCount)
                            .arg(m_processTree->topLevelItemCount()));
}

void ProcessPickerDialog::onSelectionChanged() {
  QTreeWidgetItem *selected = m_processTree->selectedItems().value(0, nullptr);

  QDialogButtonBox *buttons = this->findChild<QDialogButtonBox *>();
  if (buttons) {
    buttons->button(QDialogButtonBox::Ok)->setEnabled(selected != nullptr);
  }

  m_selectedPid = selected ? selected->data(0, Qt::UserRole).toLongLong() : 0;
}

void ProcessPickerDialog::onProcessDoubleClicked(QTreeWidgetItem *item,
                                                 int column) {
  Q_UNUSED(column);
  if (!item) {
    return;
  }
  m_selectedPid = item->data(0, Qt::UserRole).toLongLong();
  accept();
}

QList<ProcessPickerDialog::ProcessEntry>
ProcessPickerDialog::listRunningProcesses() {
#ifdef Q_OS_LINUX
  {
    DIR *dir = opendir("/proc");
    if (dir) {
      closedir(dir);
      const QList<ProcessEntry> entries = listProcessesFromProcFs();
      if (!entries.isEmpty()) {
        return entries;
      }
    }
  }
#endif
  return listProcessesFromPs();
}
