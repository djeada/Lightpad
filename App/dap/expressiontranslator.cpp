#include "expressiontranslator.h"

#include <QStringList>

namespace {

bool isGdbAdapter(const QString &adapterId, const QString &adapterType) {
  const QString loweredId = adapterId.toLower();
  const QString loweredType = adapterType.toLower();
  return loweredId.contains("gdb") || loweredType == QLatin1String("gdb");
}

bool looksLikeDebuggerCommand(const QString &input) {
  const QString trimmed = input.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  if (trimmed.startsWith(QLatin1Char('-')) ||
      trimmed.startsWith(QLatin1String("interpreter-exec "),
                         Qt::CaseInsensitive)) {
    return true;
  }

  const int firstSpace = trimmed.indexOf(QLatin1Char(' '));
  const QString firstToken =
      (firstSpace > 0 ? trimmed.left(firstSpace) : trimmed).toLower();

  static const QStringList commandTokens = {
      "alias",   "apropos",     "backtrace", "break",    "bt",     "catch",
      "clear",   "commands",    "condition", "continue", "delete", "detach",
      "disable", "disassemble", "display",   "down",     "enable", "finish",
      "frame",   "help",        "ignore",    "info",     "jump",   "list",
      "next",    "print",       "ptype",     "quit",     "run",    "set",
      "show",    "start",       "step",      "tbreak",   "thread", "until",
      "up",      "watch",       "whatis",    "where",    "x"};
  return commandTokens.contains(firstToken);
}

QList<DebugEvaluateRequest> buildDefaultPlan(const QString &trimmedInput,
                                             bool preferRepl) {
  QList<DebugEvaluateRequest> plan;
  if (trimmedInput.isEmpty()) {
    return plan;
  }

  if (preferRepl) {
    plan.append({trimmedInput, "repl"});
  } else {
    plan.append({trimmedInput, "watch"});
    plan.append({trimmedInput, "repl"});
  }

  return plan;
}

} // namespace

QList<DebugEvaluateRequest>
DebugExpressionTranslator::buildConsoleEvaluationPlan(
    const QString &userInput, const QString &adapterId,
    const QString &adapterType) {
  const QString trimmedInput = userInput.trimmed();
  if (trimmedInput.isEmpty()) {
    return {};
  }

  if (isGdbAdapter(adapterId, adapterType)) {
    const bool preferRepl = looksLikeDebuggerCommand(trimmedInput);
    if (preferRepl) {
      return {{trimmedInput, "repl"}};
    }

    return {{trimmedInput, "watch"},
            {QString("print %1").arg(trimmedInput), "repl"}};
  }

  const bool preferRepl = looksLikeDebuggerCommand(trimmedInput);
  return buildDefaultPlan(trimmedInput, preferRepl);
}

DebugEvaluateRequest
DebugExpressionTranslator::localsFallbackRequest(const QString &adapterId,
                                                 const QString &adapterType) {
  if (isGdbAdapter(adapterId, adapterType)) {
    return {"interpreter-exec console \"info locals\"", "repl"};
  }

  return {};
}
