#include "gitfiletimeline.h"
#include <QHash>
#include <QObject>
#include <QStringList>

namespace {

const QLatin1Char RECORD_SEPARATOR('\x01');
const QLatin1Char FIELD_SEPARATOR('\x00');

struct ParsedRecord {
  GitCommitInfo commit;
  QStringList bodyLines;
};

QList<ParsedRecord> splitRecords(const QString &output) {
  QList<ParsedRecord> records;
  const QStringList chunks = output.split(RECORD_SEPARATOR, Qt::SkipEmptyParts);

  for (const QString &chunk : chunks) {
    const int newline = chunk.indexOf(QLatin1Char('\n'));
    const QString header = newline < 0 ? chunk : chunk.left(newline);
    const QStringList fields = header.split(FIELD_SEPARATOR);
    if (fields.size() < 7) {
      continue;
    }

    ParsedRecord record;
    record.commit.hash = fields.at(0);
    record.commit.shortHash = fields.at(1);
    record.commit.author = fields.at(2);
    record.commit.authorEmail = fields.at(3);
    record.commit.date = fields.at(4);
    record.commit.relativeDate = fields.at(5);
    record.commit.subject = fields.at(6);
    if (fields.size() > 7) {
      record.commit.parents =
          fields.at(7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    }

    if (newline >= 0) {
      record.bodyLines =
          chunk.mid(newline + 1).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }
    records.append(record);
  }

  return records;
}

void splitRenamePath(const QString &raw, QString *from, QString *to) {
  const int brace = raw.indexOf(QLatin1Char('{'));
  if (brace >= 0) {
    const int close = raw.indexOf(QLatin1Char('}'), brace);
    const int arrow = raw.indexOf(QStringLiteral(" => "), brace);
    if (close > brace && arrow > brace && arrow < close) {
      const QString prefix = raw.left(brace);
      const QString suffix = raw.mid(close + 1);
      *from = prefix + raw.mid(brace + 1, arrow - brace - 1) + suffix;
      *to = prefix + raw.mid(arrow + 4, close - arrow - 4) + suffix;
      return;
    }
  }

  const int arrow = raw.indexOf(QStringLiteral(" => "));
  if (arrow > 0) {
    *from = raw.left(arrow);
    *to = raw.mid(arrow + 4);
    return;
  }

  *from = QString();
  *to = raw;
}

} // namespace

QList<GitFileRevision> parseFileTimeline(const QString &nameStatusOutput,
                                         const QString &numstatOutput) {
  QHash<QString, QPair<int, int>> statsByHash;
  QHash<QString, QString> renameFromByHash;

  for (const ParsedRecord &record : splitRecords(numstatOutput)) {
    for (const QString &line : record.bodyLines) {
      const QStringList parts = line.split(QLatin1Char('\t'));
      if (parts.size() < 3) {
        continue;
      }
      const bool binary = parts.at(0) == QLatin1String("-");
      statsByHash.insert(
          record.commit.hash,
          {binary ? 0 : parts.at(0).toInt(), binary ? 0 : parts.at(1).toInt()});

      QString from;
      QString to;
      if (parts.size() >= 5) {

        from = parts.at(2);
      } else {
        splitRenamePath(parts.at(2), &from, &to);
      }
      if (!from.isEmpty()) {
        renameFromByHash.insert(record.commit.hash, from);
      }
      break;
    }
  }

  QList<GitFileRevision> revisions;
  for (const ParsedRecord &record : splitRecords(nameStatusOutput)) {
    GitFileRevision revision;
    revision.commit = record.commit;

    for (const QString &line : record.bodyLines) {
      const QStringList parts = line.split(QLatin1Char('\t'));
      if (parts.size() < 2 || parts.at(0).isEmpty()) {
        continue;
      }
      revision.status = parts.at(0).at(0);
      if (parts.size() >= 3) {
        revision.previousPath = parts.at(1);
        revision.pathAtRevision = parts.at(2);
      } else {
        revision.pathAtRevision = parts.at(1);
      }
      break;
    }

    const auto stats = statsByHash.constFind(record.commit.hash);
    if (stats != statsByHash.constEnd()) {
      revision.additions = stats->first;
      revision.deletions = stats->second;
    }
    if (revision.previousPath.isEmpty()) {
      const auto renamed = renameFromByHash.constFind(record.commit.hash);
      if (renamed != renameFromByHash.constEnd()) {
        revision.previousPath = renamed.value();
      }
    }

    revisions.append(revision);
  }

  return revisions;
}

QString gitFileRevisionSummary(const GitFileRevision &revision) {
  if (revision.isWorkingTree) {
    return QObject::tr("Uncommitted — the version currently on disk");
  }

  QStringList parts;
  switch (revision.status.toLatin1()) {
  case 'A':
    parts << QObject::tr("added");
    break;
  case 'D':
    parts << QObject::tr("deleted");
    break;
  case 'R':
    parts << (revision.previousPath.isEmpty()
                  ? QObject::tr("renamed")
                  : QObject::tr("renamed from %1").arg(revision.previousPath));
    break;
  case 'C':
    parts << (revision.previousPath.isEmpty()
                  ? QObject::tr("copied")
                  : QObject::tr("copied from %1").arg(revision.previousPath));
    break;
  default:
    parts << QObject::tr("modified");
    break;
  }

  parts << QStringLiteral("+%1 −%2")
               .arg(revision.additions)
               .arg(revision.deletions);
  return parts.join(QStringLiteral(" · "));
}
