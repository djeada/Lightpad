#include "gitconflictresolution.h"

#include <QObject>

namespace {

constexpr int kMaxUndoSteps = 200;

const QLatin1String kOursMarker("<<<<<<<");
const QLatin1String kBaseMarker("|||||||");
const QLatin1String kSeparator("=======");
const QLatin1String kTheirsMarker(">>>>>>>");

QString markerLabel(const QString &line, const QLatin1String &marker) {
  return line.mid(marker.size()).trimmed();
}

bool isSeparator(const QString &line) {
  if (!line.startsWith(kSeparator)) {
    return false;
  }

  return line.trimmed().count(QLatin1Char('=')) == line.trimmed().size();
}

} // namespace

QString conflictChoiceOutcome(ConflictChoice choice) {
  switch (choice) {
  case ConflictChoice::Unresolved:
    return QObject::tr("still undecided");
  case ConflictChoice::Ours:
    return QObject::tr("kept your version");
  case ConflictChoice::Theirs:
    return QObject::tr("kept their version");
  case ConflictChoice::BothOursFirst:
    return QObject::tr("kept both, yours first");
  case ConflictChoice::BothTheirsFirst:
    return QObject::tr("kept both, theirs first");
  case ConflictChoice::Base:
    return QObject::tr("went back to the shared original");
  case ConflictChoice::Neither:
    return QObject::tr("dropped both versions");
  case ConflictChoice::Custom:
    return QObject::tr("wrote it by hand");
  }
  return QString();
}

QString conflictChoiceAction(ConflictChoice choice) {
  switch (choice) {
  case ConflictChoice::Unresolved:
    return QObject::tr("Decide later");
  case ConflictChoice::Ours:
    return QObject::tr("Keep your version");
  case ConflictChoice::Theirs:
    return QObject::tr("Keep their version");
  case ConflictChoice::BothOursFirst:
    return QObject::tr("Keep both, yours first");
  case ConflictChoice::BothTheirsFirst:
    return QObject::tr("Keep both, theirs first");
  case ConflictChoice::Base:
    return QObject::tr("Go back to the shared original");
  case ConflictChoice::Neither:
    return QObject::tr("Drop both versions");
  case ConflictChoice::Custom:
    return QObject::tr("Write it by hand");
  }
  return QString();
}

QStringList ConflictRegion::resultLines() const {
  switch (choice) {
  case ConflictChoice::Unresolved:
    return rawLines;
  case ConflictChoice::Ours:
    return oursLines;
  case ConflictChoice::Theirs:
    return theirsLines;
  case ConflictChoice::BothOursFirst:
    return oursLines + theirsLines;
  case ConflictChoice::BothTheirsFirst:
    return theirsLines + oursLines;
  case ConflictChoice::Base:
    return baseLines;
  case ConflictChoice::Neither:
    return QStringList();
  case ConflictChoice::Custom:
    return customLines;
  }
  return rawLines;
}

int ConflictDocumentItem::lineCount() const {
  return leadingLines.size() + hiddenLines.size() + trailingLines.size();
}

void ConflictFileResolver::clear() {
  m_segments.clear();
  m_regions.clear();
  m_undo.clear();
  m_redo.clear();
  m_history.clear();
  m_oursLabel.clear();
  m_theirsLabel.clear();
  m_loaded = false;
  m_trailingNewline = true;
}

bool ConflictFileResolver::load(const QString &content) {
  clear();
  m_loaded = true;

  m_trailingNewline = content.isEmpty() || content.endsWith(QLatin1Char('\n'));

  QStringList lines = content.split(QLatin1Char('\n'));
  if (m_trailingNewline && !lines.isEmpty()) {
    lines.removeLast();
  }

  QStringList pending;
  const auto flushPending = [&]() {
    if (pending.isEmpty()) {
      return;
    }
    Segment segment;
    segment.plain = pending;
    m_segments.append(segment);
    pending.clear();
  };

  int index = 0;
  while (index < lines.size()) {
    const QString &line = lines.at(index);
    if (!line.startsWith(kOursMarker)) {
      pending.append(line);
      ++index;
      continue;
    }

    ConflictRegion region;
    region.oursLabel = markerLabel(line, kOursMarker);
    region.rawLines.append(line);

    int cursor = index + 1;
    int section = 0;
    bool complete = false;

    while (cursor < lines.size()) {
      const QString &current = lines.at(cursor);
      region.rawLines.append(current);

      if (current.startsWith(kOursMarker)) {

        break;
      }
      if (current.startsWith(kBaseMarker) && section == 0) {
        region.hasBase = true;
        region.baseLabel = markerLabel(current, kBaseMarker);
        section = 1;
        ++cursor;
        continue;
      }
      if (isSeparator(current) && section < 2) {
        section = 2;
        ++cursor;
        continue;
      }
      if (current.startsWith(kTheirsMarker) && section == 2) {
        region.theirsLabel = markerLabel(current, kTheirsMarker);
        complete = true;
        ++cursor;
        break;
      }

      switch (section) {
      case 0:
        region.oursLines.append(current);
        break;
      case 1:
        region.baseLines.append(current);
        break;
      default:
        region.theirsLines.append(current);
        break;
      }
      ++cursor;
    }

    if (!complete) {
      pending.append(line);
      ++index;
      continue;
    }

    flushPending();

    region.id = m_regions.size() + 1;
    m_regions.append(region);

    Segment segment;
    segment.isConflict = true;
    segment.regionIndex = m_regions.size() - 1;
    m_segments.append(segment);

    if (m_oursLabel.isEmpty()) {
      m_oursLabel = region.oursLabel;
    }
    if (m_theirsLabel.isEmpty()) {
      m_theirsLabel = region.theirsLabel;
    }

    index = cursor;
  }

  flushPending();

  return hasConflicts();
}

int ConflictFileResolver::resolvedCount() const {
  int count = 0;
  for (const ConflictRegion &region : m_regions) {
    if (region.resolved()) {
      ++count;
    }
  }
  return count;
}

int ConflictFileResolver::indexOf(int id) const {
  for (int i = 0; i < m_regions.size(); ++i) {
    if (m_regions.at(i).id == id) {
      return i;
    }
  }
  return -1;
}

const ConflictRegion *ConflictFileResolver::region(int id) const {
  const int index = indexOf(id);
  return index < 0 ? nullptr : &m_regions.at(index);
}

void ConflictFileResolver::pushUndo(const QString &description) {
  Snapshot snapshot;
  snapshot.regions = m_regions;
  snapshot.description = description;
  m_undo.append(snapshot);
  while (m_undo.size() > kMaxUndoSteps) {
    m_undo.removeFirst();
  }
  m_redo.clear();
}

bool ConflictFileResolver::resolve(int id, ConflictChoice choice,
                                   const QStringList &customLines) {
  const int index = indexOf(id);
  if (index < 0) {
    return false;
  }

  if (choice == ConflictChoice::Base && !m_regions.at(index).hasBase) {
    return false;
  }
  if (m_regions.at(index).choice == choice &&
      choice != ConflictChoice::Custom) {
    return true;
  }

  const QString description = QObject::tr("Conflict %1 - %2")
                                  .arg(id)
                                  .arg(conflictChoiceOutcome(choice));

  pushUndo(description);

  ConflictRegion &region = m_regions[index];
  region.choice = choice;
  region.customLines =
      choice == ConflictChoice::Custom ? customLines : QStringList();
  m_history.append(description);
  return true;
}

bool ConflictFileResolver::resolveAll(ConflictChoice choice) {
  if (m_regions.isEmpty() || choice == ConflictChoice::Unresolved) {
    return false;
  }

  bool anyApplicable = false;
  for (const ConflictRegion &region : m_regions) {
    if (choice == ConflictChoice::Base && !region.hasBase) {
      continue;
    }
    if (region.choice != choice) {
      anyApplicable = true;
      break;
    }
  }
  if (!anyApplicable) {
    return false;
  }

  const QString description = QObject::tr("All %1 conflicts - %2")
                                  .arg(m_regions.size())
                                  .arg(conflictChoiceOutcome(choice));
  pushUndo(description);

  for (ConflictRegion &region : m_regions) {
    if (choice == ConflictChoice::Base && !region.hasBase) {
      continue;
    }
    region.choice = choice;
    region.customLines.clear();
  }
  m_history.append(description);
  return true;
}

bool ConflictFileResolver::reopen(int id) {
  const int index = indexOf(id);
  if (index < 0 || !m_regions.at(index).resolved()) {
    return false;
  }

  const QString description = QObject::tr("Conflict %1 - reopened").arg(id);
  pushUndo(description);

  m_regions[index].choice = ConflictChoice::Unresolved;
  m_regions[index].customLines.clear();
  m_history.append(description);
  return true;
}

bool ConflictFileResolver::undo() {
  if (m_undo.isEmpty()) {
    return false;
  }

  Snapshot redoPoint;
  redoPoint.regions = m_regions;
  redoPoint.description = m_undo.last().description;
  m_redo.append(redoPoint);

  m_regions = m_undo.last().regions;
  m_undo.removeLast();
  if (!m_history.isEmpty()) {
    m_history.removeLast();
  }
  return true;
}

bool ConflictFileResolver::redo() {
  if (m_redo.isEmpty()) {
    return false;
  }

  Snapshot undoPoint;
  undoPoint.regions = m_regions;
  undoPoint.description = m_redo.last().description;
  m_undo.append(undoPoint);

  m_regions = m_redo.last().regions;
  m_history.append(m_redo.last().description);
  m_redo.removeLast();
  return true;
}

QString ConflictFileResolver::undoDescription() const {
  return m_undo.isEmpty() ? QString() : m_undo.last().description;
}

QString ConflictFileResolver::redoDescription() const {
  return m_redo.isEmpty() ? QString() : m_redo.last().description;
}

QStringList ConflictFileResolver::history() const { return m_history; }

QString ConflictFileResolver::text() const {
  QStringList out;
  for (const Segment &segment : m_segments) {
    if (segment.isConflict) {
      out += m_regions.at(segment.regionIndex).resultLines();
    } else {
      out += segment.plain;
    }
  }

  if (out.isEmpty()) {
    return QString();
  }

  QString joined = out.join(QLatin1Char('\n'));
  if (m_trailingNewline) {
    joined.append(QLatin1Char('\n'));
  }
  return joined;
}

int ConflictFileResolver::lineOf(int id) const {
  int line = 1;
  for (const Segment &segment : m_segments) {
    if (segment.isConflict) {
      const ConflictRegion &region = m_regions.at(segment.regionIndex);
      if (region.id == id) {
        return line;
      }
      line += region.resultLines().size();
    } else {
      line += segment.plain.size();
    }
  }
  return 0;
}

QList<ConflictDocumentItem>
ConflictFileResolver::documentItems(int contextLines) const {
  QList<ConflictDocumentItem> items;
  const int context = contextLines < 0 ? 0 : contextLines;

  int line = 1;
  for (int i = 0; i < m_segments.size(); ++i) {
    const Segment &segment = m_segments.at(i);
    if (segment.isConflict) {
      ConflictDocumentItem item;
      item.kind = ConflictDocumentItem::Kind::Conflict;
      item.regionId = m_regions.at(segment.regionIndex).id;
      item.startLine = line;
      items.append(item);
      line += m_regions.at(segment.regionIndex).resultLines().size();
      continue;
    }

    ConflictDocumentItem item;
    item.kind = ConflictDocumentItem::Kind::Context;
    item.startLine = line;

    const QStringList &plain = segment.plain;
    const bool firstSegment = i == 0;
    const bool lastSegment = i == m_segments.size() - 1;

    const int head = firstSegment ? 0 : context;
    const int tail = lastSegment ? 0 : context;

    if (plain.size() <= head + tail) {
      item.leadingLines = plain;
    } else {
      item.leadingLines = plain.mid(0, head);
      item.hiddenLines = plain.mid(head, plain.size() - head - tail);
      item.trailingLines = plain.mid(plain.size() - tail);
    }

    items.append(item);
    line += plain.size();
  }

  return items;
}

QStringList ConflictFileResolver::sideLines(bool ours) const {
  QStringList out;
  for (const Segment &segment : m_segments) {
    if (segment.isConflict) {
      const ConflictRegion &region = m_regions.at(segment.regionIndex);
      out += ours ? region.oursLines : region.theirsLines;
    } else {
      out += segment.plain;
    }
  }
  return out;
}

QStringList ConflictFileResolver::allOursLines() const {
  return sideLines(true);
}

QStringList ConflictFileResolver::allTheirsLines() const {
  return sideLines(false);
}

bool textHasConflictMarkers(const QString &content) {
  return countConflictMarkers(content) > 0;
}

int countConflictMarkers(const QString &content) {
  ConflictFileResolver resolver;
  resolver.load(content);
  return resolver.totalConflicts();
}
