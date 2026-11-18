#ifndef LAUNCHPLANNER_H
#define LAUNCHPLANNER_H

#include "../dap/debugconfiguration.h"
#include "runtargetresolver.h"

#include <QList>
#include <QString>
#include <QStringList>

enum class LaunchMode { Run, Debug };

// What the Run and Debug buttons start: the file in the editor (resolved
// automatically from run templates, CMake targets, pytest, ...), a saved
// launch configuration, or a compound of saved configurations.
struct LaunchTarget {
  enum class Kind { CurrentFile, Configuration, Compound };

  Kind kind = Kind::CurrentFile;
  QString name;

  static LaunchTarget currentFile() { return {}; }
  static LaunchTarget configuration(const QString &name) {
    return {Kind::Configuration, name};
  }
  static LaunchTarget compound(const QString &name) {
    return {Kind::Compound, name};
  }

  bool isCurrentFile() const { return kind == Kind::CurrentFile; }

  // Settings representation: "" for the current file, the configuration
  // name, or "compound:<name>".
  QString toKey() const;
  static LaunchTarget fromKey(const QString &key);

  bool operator==(const LaunchTarget &other) const {
    return kind == other.kind && name == other.name;
  }
  bool operator!=(const LaunchTarget &other) const { return !(*this == other); }
};

// A command that has to succeed before the program can start, such as
// compiling a single source file with debug information.
struct LaunchBuildStep {
  QString title;
  QStringList command;
  QString workingDirectory;

  bool isValid() const { return !command.isEmpty(); }
};

// How Debug starts the file that Run would start.
struct FileDebugPlan {
  DebugConfiguration configuration;
  QList<LaunchBuildStep> buildSteps;
  // The program is the executable of a CMake target, built (and possibly
  // chosen) right before the session starts.
  bool requiresCMakeBuild = false;
  QString error;

  bool isValid() const {
    return error.isEmpty() && !configuration.type.isEmpty();
  }
};

class LaunchPlanner {
public:
  // Debugging the current file uses the same run template, arguments,
  // environment, working directory and Python interpreter as running it.
  // The debug configuration manager's workspace folder must be set.
  static FileDebugPlan debugPlanForFile(const RunTarget &target);

  // The command Run executes for a saved configuration whose variables are
  // already resolved.
  static RunCommand runCommandFor(const DebugConfiguration &resolved);

  // Name of the debugger that would handle the configuration, or an empty
  // string when no registered debug adapter supports its type.
  static QString debuggerNameFor(const DebugConfiguration &configuration);

  // Which debugger Debug would use for the file, without starting any
  // process (safe for tooltips). Empty with *reason set when the file
  // cannot be debugged.
  static QString fileDebuggerName(const RunTarget &target,
                                  QString *reason = nullptr);

  // "program arg 'two words'" for previews and the clipboard.
  static QString commandLine(const QString &program,
                             const QStringList &arguments);
};

#endif
