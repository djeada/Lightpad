#include "gitbisect.h"
#include "gitintegration.h"
#include <QObject>
#include <QRegularExpression>

QString gitBisectStatusName(GitBisectStatus status) {
  switch (status) {
  case GitBisectStatus::NotStarted:
    return QObject::tr("Not started");
  case GitBisectStatus::Searching:
    return QObject::tr("Searching");
  case GitBisectStatus::Found:
    return QObject::tr("Found");
  }
  return QString();
}

QString gitBisectGuidance(const GitBisectState &state) {
  switch (state.status) {
  case GitBisectStatus::NotStarted:
    return QObject::tr(
        "Pick a commit where the behaviour was right and one where it was "
        "wrong. Bisect is a binary search over the commits between them, so a "
        "test you trust matters more than a good guess about which commit is "
        "to blame.");
  case GitBisectStatus::Searching: {
    QString text =
        QObject::tr(
            "Your checkout is on %1. Build or run it, then say whether the "
            "behaviour is right or wrong here.")
            .arg(state.currentHash.left(7));
    if (state.revisionsLeft > 0) {
      text += QObject::tr(" About %1 commits left, roughly %2 more steps.")
                  .arg(state.revisionsLeft)
                  .arg(state.estimatedSteps);
    }
    if (state.skipCount > 0) {
      text += QObject::tr(
                  " %1 commit(s) were skipped; if the search ends on a range "
                  "rather than one commit, that is why.")
                  .arg(state.skipCount);
    }
    return text;
  }
  case GitBisectStatus::Found:
    return QObject::tr(
               "%1 is the first commit where the behaviour was wrong. Reset "
               "when you "
               "are done: your checkout is still where the search left it.")
        .arg(state.suspectHash.left(7));
  }
  return QString();
}

void parseBisectProgress(const QString &output, GitBisectState *state) {
  if (!state) {
    return;
  }

  static const QRegularExpression remaining(
      QStringLiteral("Bisecting:\\s+(\\d+)\\s+revisions? left to test after "
                     "this\\s*\\(roughly\\s+(\\d+)\\s+steps?\\)"));
  const QRegularExpressionMatch remainingMatch = remaining.match(output);
  if (remainingMatch.hasMatch()) {
    state->revisionsLeft = remainingMatch.captured(1).toInt();
    state->estimatedSteps = remainingMatch.captured(2).toInt();
    state->status = GitBisectStatus::Searching;
  }

  static const QRegularExpression found(
      QStringLiteral("([0-9a-f]{7,40}) is the first bad commit"));
  const QRegularExpressionMatch foundMatch = found.match(output);
  if (foundMatch.hasMatch()) {
    state->suspectHash = foundMatch.captured(1);
    state->status = GitBisectStatus::Found;
  }
}

void parseBisectLog(const QString &log, GitBisectState *state) {
  if (!state) {
    return;
  }

  state->log.clear();
  state->goodCount = 0;
  state->badCount = 0;
  state->skipCount = 0;

  for (const QString &line : log.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    state->log << line;
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1String("git bisect good"))) {
      ++state->goodCount;
    } else if (trimmed.startsWith(QLatin1String("git bisect bad"))) {
      ++state->badCount;
    } else if (trimmed.startsWith(QLatin1String("git bisect skip"))) {
      ++state->skipCount;
    }
  }

  for (const QString &line : state->log) {
    if (line.trimmed().startsWith(QLatin1String("git bisect start")) &&
        line.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() >= 5) {
      ++state->badCount;
      ++state->goodCount;
      break;
    }
  }
}

QString gitBisectExitCodeMeaning(int exitCode) {
  if (exitCode == 0) {
    return QObject::tr("0 — the command succeeded, so this commit is good.");
  }
  if (exitCode == 125) {
    return QObject::tr(
        "125 — the commit could not be tested, so it is skipped. Use this "
        "exit code when the build itself fails.");
  }
  if (exitCode >= 1 && exitCode <= 127) {
    return QObject::tr("%1 — the command failed, so this commit is bad.")
        .arg(exitCode);
  }
  return QObject::tr(
             "%1 — outside the range git bisect run understands; it aborts "
             "rather than guessing.")
      .arg(exitCode);
}

GitBisectState buildBisectState(GitIntegration *git) {
  GitBisectState state;
  if (!git || !git->isValidRepository()) {
    return state;
  }

  state.valid = true;
  if (!git->isBisecting()) {
    return state;
  }

  state.status = GitBisectStatus::Searching;
  parseBisectLog(git->bisectLog(), &state);

  const GitCommitInfo head = git->getCommitDetails(QStringLiteral("HEAD"));
  state.currentHash = head.hash;
  state.currentSubject = head.subject;

  return state;
}
