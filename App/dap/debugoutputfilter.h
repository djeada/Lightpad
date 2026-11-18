#ifndef DEBUGOUTPUTFILTER_H
#define DEBUGOUTPUTFILTER_H

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// Debuggers print a banner and start-up chatter (GDB's licence text,
// "No source file named …" while breakpoints wait for symbols, debuginfod
// prompts) that say nothing about the program. The debug console drops those
// lines from the adapter's own "console" output; program output is untouched.
namespace DebugOutputFilter {

inline bool isAdapterChatter(const QString &line) {
  static const QRegularExpression chatter(QStringLiteral(
      "^(GNU gdb .*"
      "|Copyright \\(C\\) \\d+ Free Software Foundation.*"
      "|License GPLv3\\+.*"
      "|This is free software: you are free to change and redistribute.*"
      "|There is NO WARRANTY, to the extent permitted by law\\.?"
      "|Type \"show copying\" and \"show warranty\" for details\\.?"
      "|This GDB was configured as .*"
      "|Type \"show configuration\" for configuration details\\.?"
      "|For bug reporting instructions, please see:"
      "|<https?://www\\.gnu\\.org/software/gdb/[a-z]+/>\\.?"
      "|Find the GDB manual and other documentation resources online at:"
      "|For help, type \"help\"\\.?"
      "|Type \"apropos word\" to search for commands related to "
      "\"word\"\\.?"
      "|No source file named .*\\."
      "|Breakpoint \\d+ \\(.*\\) pending\\."
      "|No symbol table is loaded\\.\\s+Use the \"file\" command\\."
      "|Make breakpoint pending on future shared library load\\?.*"
      "|This GDB supports auto-downloading debuginfo from the following "
      "URLs:"
      "|\\s*<https?://debuginfod\\.[^>]*>"
      "|Enable debuginfod for this session\\? .*"
      "|Debuginfod has been disabled\\."
      "|To make this setting permanent, add 'set debuginfod enabled "
      "off' to \\.gdbinit\\."
      "|\\[Thread debugging using libthread_db enabled\\]"
      "|Using host libthread_db library .*)\\s*$"));
  return chatter.match(line).hasMatch();
}

// The output with chatter lines removed; empty when nothing is left.
inline QString withoutAdapterChatter(const QString &output) {
  if (output.isEmpty()) {
    return output;
  }
  const QStringList lines = output.split(QLatin1Char('\n'));
  QStringList kept;
  bool dropped = false;
  for (const QString &line : lines) {
    if (isAdapterChatter(line)) {
      dropped = true;
    } else {
      kept << line;
    }
  }
  if (!dropped) {
    return output;
  }
  QString text = kept.join(QLatin1Char('\n'));
  return text.trimmed().isEmpty() ? QString() : text;
}

} // namespace DebugOutputFilter

#endif
