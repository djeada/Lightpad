#include "gitsubmodulemodel.h"

QList<GitSubmoduleInfo> parseSubmoduleStatus(const QString &output) {
  QList<GitSubmoduleInfo> result;
  const QStringList lines = output.split('\n');
  for (const QString &rawLine : lines) {
    if (rawLine.trimmed().isEmpty()) {
      continue;
    }
    GitSubmoduleInfo info;
    QString line = rawLine;
    const QChar prefix = line.at(0);
    if (prefix == QLatin1Char('-')) {
      info.state = GitSubmoduleState::Uninitialized;
      line.remove(0, 1);
    } else if (prefix == QLatin1Char('+')) {
      info.state = GitSubmoduleState::OutOfDate;
      line.remove(0, 1);
    } else if (prefix == QLatin1Char('U')) {
      info.state = GitSubmoduleState::Conflicted;
      line.remove(0, 1);
    } else if (prefix == QLatin1Char(' ')) {
      line.remove(0, 1);
    }

    const int space = line.indexOf(QLatin1Char(' '));
    if (space <= 0) {
      continue;
    }
    info.hash = line.left(space);
    QString rest = line.mid(space + 1);

    if (rest.endsWith(QLatin1Char(')'))) {
      const int open = rest.lastIndexOf(QStringLiteral(" ("));
      if (open > 0) {
        info.describe = rest.mid(open + 2, rest.size() - open - 3);
        rest = rest.left(open);
      }
    }
    info.path = rest;
    if (!info.path.isEmpty()) {
      result.append(info);
    }
  }
  return result;
}

QString gitSubmoduleStateName(GitSubmoduleState state) {
  switch (state) {
  case GitSubmoduleState::Uninitialized:
    return QStringLiteral("not initialized");
  case GitSubmoduleState::Current:
    return QStringLiteral("up to date");
  case GitSubmoduleState::OutOfDate:
    return QStringLiteral("checked out at a different commit");
  case GitSubmoduleState::Conflicted:
    return QStringLiteral("merge conflict");
  }
  return QString();
}

QString gitSubmoduleSummary(const QList<GitSubmoduleInfo> &modules) {
  if (modules.isEmpty()) {
    return QStringLiteral("No submodules");
  }
  int uninit = 0, outdated = 0, conflicted = 0;
  for (const GitSubmoduleInfo &m : modules) {
    if (m.state == GitSubmoduleState::Uninitialized)
      ++uninit;
    else if (m.state == GitSubmoduleState::OutOfDate)
      ++outdated;
    else if (m.state == GitSubmoduleState::Conflicted)
      ++conflicted;
  }
  QStringList parts;
  parts << QStringLiteral("%1 submodule(s)").arg(modules.size());
  if (uninit)
    parts << QStringLiteral("%1 not initialized").arg(uninit);
  if (outdated)
    parts << QStringLiteral("%1 out of date").arg(outdated);
  if (conflicted)
    parts << QStringLiteral("%1 conflicted").arg(conflicted);
  return parts.join(QStringLiteral(", "));
}
