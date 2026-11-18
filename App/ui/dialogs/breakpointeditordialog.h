#ifndef BREAKPOINTEDITORDIALOG_H
#define BREAKPOINTEDITORDIALOG_H

#include "../../dap/breakpointmanager.h"
#include "styleddialog.h"

class QCheckBox;
class QLineEdit;
class QRadioButton;

// Edits everything about a line breakpoint in one place: whether it is
// enabled, the condition and hit count that make it pause, and whether it
// pauses at all or logs a message instead (a logpoint). Used from the editor
// gutter and the Debug panel's breakpoint list.
class BreakpointEditorDialog : public StyledDialog {
  Q_OBJECT
  friend class TestLaunchConfigurations;

public:
  enum class Focus { Condition, HitCount, LogMessage };

  explicit BreakpointEditorDialog(const Breakpoint &breakpoint,
                                  QWidget *parent = nullptr);

  void setInitialFocus(Focus focus);
  // The breakpoint as edited (id, file and line unchanged).
  Breakpoint breakpoint() const;
  bool removeRequested() const { return m_removeRequested; }

  void applyTheme(const Theme &theme) override;

  // Opens the editor for the breakpoint at filePath:line, creating it when
  // the dialog is saved with something set. Returns true when it changed.
  static bool editAt(const QString &filePath, int line, Focus focus,
                     const Theme &theme, QWidget *parent);
  // Writes `edited` into the BreakpointManager (adding it when it has no id).
  static void apply(const Breakpoint &edited);

  // An example condition in the language of the file.
  static QString conditionExample(const QString &filePath);

private:
  void updateModeState();

  Breakpoint m_breakpoint;
  bool m_removeRequested = false;
  QLabel *m_title = nullptr;
  QLabel *m_location = nullptr;
  QCheckBox *m_enabled = nullptr;
  QLineEdit *m_condition = nullptr;
  QLineEdit *m_hitCondition = nullptr;
  QRadioButton *m_pause = nullptr;
  QRadioButton *m_log = nullptr;
  QLineEdit *m_logMessage = nullptr;
  QList<QLabel *> m_hints;
  QPushButton *m_removeButton = nullptr;
  QPushButton *m_saveButton = nullptr;
};

#endif
