#include "breakpointeditordialog.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

QString BreakpointEditorDialog::conditionExample(const QString &filePath) {
  const QString suffix = QFileInfo(filePath).suffix().toLower();
  if (suffix == "py") {
    return tr("e.g. total > 100 and name == \"bob\"");
  }
  if (suffix == "js" || suffix == "mjs" || suffix == "cjs" || suffix == "ts" ||
      suffix == "tsx" || suffix == "jsx") {
    return tr("e.g. items.length === 0");
  }
  if (suffix == "go") {
    return tr("e.g. len(items) == 0");
  }
  if (suffix == "rs") {
    return tr("e.g. count > 10");
  }
  return tr("e.g. i == 42 && ptr != NULL");
}

BreakpointEditorDialog::BreakpointEditorDialog(const Breakpoint &breakpoint,
                                               QWidget *parent)
    : StyledDialog(parent), m_breakpoint(breakpoint) {
  setWindowTitle(tr("Breakpoint"));
  setMinimumWidth(520);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 16, 18, 14);
  layout->setSpacing(10);

  const QFileInfo file(breakpoint.filePath);
  m_title = new QLabel(
      tr("Breakpoint · %1:%2").arg(file.fileName()).arg(breakpoint.line), this);
  layout->addWidget(m_title);
  m_location = new QLabel(breakpoint.filePath, this);
  m_location->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_location->setWordWrap(true);
  layout->addWidget(m_location);

  m_enabled = new QCheckBox(tr("Enabled"), this);
  m_enabled->setObjectName("breakpointEnabled");
  m_enabled->setChecked(breakpoint.id <= 0 || breakpoint.enabled);
  layout->addWidget(m_enabled);

  auto *form = new QFormLayout();
  form->setHorizontalSpacing(12);
  form->setVerticalSpacing(6);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  auto addHint = [this, form](const QString &text) {
    auto *hint = new QLabel(text, this);
    hint->setWordWrap(true);
    m_hints << hint;
    form->addRow(QString(), hint);
  };

  m_condition = new QLineEdit(breakpoint.condition, this);
  m_condition->setObjectName("breakpointCondition");
  m_condition->setPlaceholderText(conditionExample(breakpoint.filePath));
  form->addRow(tr("Condition"), m_condition);
  addHint(tr("Only stops when this expression is true. Leave empty to stop "
             "every time."));

  m_hitCondition = new QLineEdit(breakpoint.hitCondition, this);
  m_hitCondition->setObjectName("breakpointHitCondition");
  m_hitCondition->setPlaceholderText(tr("e.g. 5, >= 10 or % 3"));
  form->addRow(tr("Hit count"), m_hitCondition);
  addHint(tr("Stops on a given hit: 5 is the fifth time, >= 10 from the "
             "tenth on, % 3 every third (debugpy and js-debug). Every "
             "debugger accepts a plain number."));

  m_pause = new QRadioButton(tr("Pause the program"), this);
  m_log = new QRadioButton(tr("Log a message and keep running"), this);
  m_log->setObjectName("breakpointLogMode");
  (breakpoint.isLogpoint ? m_log : m_pause)->setChecked(true);
  auto *modeRow = new QHBoxLayout();
  modeRow->setSpacing(18);
  modeRow->addWidget(m_pause);
  modeRow->addWidget(m_log);
  modeRow->addStretch();
  auto *modeHolder = new QWidget(this);
  modeRow->setContentsMargins(0, 0, 0, 0);
  modeHolder->setLayout(modeRow);
  form->addRow(tr("When hit"), modeHolder);

  m_logMessage = new QLineEdit(breakpoint.logMessage, this);
  m_logMessage->setObjectName("breakpointLogMessage");
  m_logMessage->setPlaceholderText(tr("e.g. total={total} items={len(items)}"));
  form->addRow(tr("Message"), m_logMessage);
  addHint(tr("Written to the Debug Console; expressions in {braces} are "
             "evaluated."));
  layout->addLayout(form);

  auto *buttons = new QHBoxLayout();
  if (breakpoint.id > 0) {
    m_removeButton = new QPushButton(tr("Remove Breakpoint"), this);
    m_removeButton->setObjectName("breakpointRemove");
    buttons->addWidget(m_removeButton);
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
      m_removeRequested = true;
      QDialog::accept();
    });
  }
  buttons->addStretch();
  auto *cancel = new QPushButton(tr("Cancel"), this);
  m_saveButton = new QPushButton(tr("Save"), this);
  m_saveButton->setObjectName("breakpointSave");
  buttons->addWidget(cancel);
  buttons->addWidget(m_saveButton);
  layout->addLayout(buttons);
  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_saveButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_log, &QRadioButton::toggled, this,
          &BreakpointEditorDialog::updateModeState);
  setKeyboardDefault(m_saveButton);
  updateModeState();
  setInitialFocus(breakpoint.isLogpoint ? Focus::LogMessage : Focus::Condition);
}

void BreakpointEditorDialog::updateModeState() {
  m_logMessage->setEnabled(m_log->isChecked());
  if (m_log->isChecked() && m_logMessage->text().isEmpty()) {
    m_logMessage->setFocus();
  }
}

void BreakpointEditorDialog::setInitialFocus(Focus focus) {
  switch (focus) {
  case Focus::Condition:
    m_condition->setFocus();
    break;
  case Focus::HitCount:
    m_hitCondition->setFocus();
    break;
  case Focus::LogMessage:
    m_log->setChecked(true);
    m_logMessage->setFocus();
    break;
  }
}

Breakpoint BreakpointEditorDialog::breakpoint() const {
  Breakpoint edited = m_breakpoint;
  edited.enabled = m_enabled->isChecked();
  edited.condition = m_condition->text().trimmed();
  edited.hitCondition = m_hitCondition->text().trimmed();
  edited.logMessage =
      m_log->isChecked() ? m_logMessage->text().trimmed() : QString();
  edited.isLogpoint = !edited.logMessage.isEmpty();
  return edited;
}

void BreakpointEditorDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  styleTitleLabel(m_title);
  styleSubduedLabel(m_location);
  for (QLabel *hint : m_hints) {
    styleSubduedLabel(hint);
  }
  stylePrimaryButton(m_saveButton);
  if (m_removeButton) {
    styleDangerButton(m_removeButton);
  }
}

void BreakpointEditorDialog::apply(const Breakpoint &edited) {
  BreakpointManager &manager = BreakpointManager::instance();
  int id = edited.id;
  if (id <= 0 || manager.breakpoint(id).id <= 0) {
    Breakpoint added;
    added.filePath = edited.filePath;
    added.line = edited.line;
    added.enabled = edited.enabled;
    id = manager.addBreakpoint(added);
  }
  const Breakpoint current = manager.breakpoint(id);
  if (current.condition != edited.condition) {
    manager.setCondition(id, edited.condition);
  }
  if (current.hitCondition != edited.hitCondition) {
    manager.setHitCondition(id, edited.hitCondition);
  }
  if (current.logMessage != edited.logMessage) {
    manager.setLogMessage(id, edited.logMessage);
  }
  if (current.enabled != edited.enabled) {
    manager.setEnabled(id, edited.enabled);
  }
}

bool BreakpointEditorDialog::editAt(const QString &filePath, int line,
                                    Focus focus, const Theme &theme,
                                    QWidget *parent) {
  BreakpointManager &manager = BreakpointManager::instance();
  Breakpoint breakpoint = manager.breakpointAt(filePath, line);
  if (breakpoint.id <= 0) {
    breakpoint = Breakpoint();
    breakpoint.filePath = filePath;
    breakpoint.line = line;
  }
  BreakpointEditorDialog dialog(breakpoint, parent);
  dialog.applyTheme(theme);
  dialog.setInitialFocus(focus);
  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }
  if (dialog.removeRequested()) {
    manager.removeBreakpoint(breakpoint.id);
    return true;
  }
  const Breakpoint edited = dialog.breakpoint();
  // A new breakpoint is only created when the dialog says something;
  // saving an empty form for a line without one changes nothing.
  if (breakpoint.id <= 0 && edited.condition.isEmpty() &&
      edited.hitCondition.isEmpty() && edited.logMessage.isEmpty()) {
    return false;
  }
  apply(edited);
  return true;
}
