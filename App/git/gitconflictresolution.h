#ifndef GITCONFLICTRESOLUTION_H
#define GITCONFLICTRESOLUTION_H

#include <QList>
#include <QString>
#include <QStringList>

enum class ConflictChoice {
  Unresolved,
  Ours,
  Theirs,
  BothOursFirst,
  BothTheirsFirst,
  Base,
  Neither,
  Custom,
};

QString conflictChoiceOutcome(ConflictChoice choice);

QString conflictChoiceAction(ConflictChoice choice);

struct ConflictRegion {
  int id = 0;

  QStringList oursLines;
  QStringList theirsLines;
  QStringList baseLines;
  bool hasBase = false;

  QString oursLabel;
  QString theirsLabel;
  QString baseLabel;

  QStringList rawLines;

  ConflictChoice choice = ConflictChoice::Unresolved;
  QStringList customLines;

  bool resolved() const { return choice != ConflictChoice::Unresolved; }

  QStringList resultLines() const;
};

struct ConflictDocumentItem {
  enum class Kind { Context, Conflict };

  Kind kind = Kind::Context;

  QStringList leadingLines;
  QStringList hiddenLines;
  QStringList trailingLines;

  int regionId = 0;

  int startLine = 0;

  int hiddenCount() const { return hiddenLines.size(); }
  int lineCount() const;
};

class ConflictFileResolver {
public:
  ConflictFileResolver() = default;

  bool load(const QString &content);
  void clear();

  bool isLoaded() const { return m_loaded; }
  bool hasConflicts() const { return !m_regions.isEmpty(); }

  int totalConflicts() const { return m_regions.size(); }
  int resolvedCount() const;
  int remainingCount() const { return totalConflicts() - resolvedCount(); }

  const QList<ConflictRegion> &regions() const { return m_regions; }
  const ConflictRegion *region(int id) const;

  bool resolve(int id, ConflictChoice choice,
               const QStringList &customLines = QStringList());
  bool resolveAll(ConflictChoice choice);
  bool reopen(int id);

  bool canUndo() const { return !m_undo.isEmpty(); }
  bool canRedo() const { return !m_redo.isEmpty(); }
  bool undo();
  bool redo();

  QString undoDescription() const;
  QString redoDescription() const;

  QStringList history() const;

  QString text() const;

  int lineOf(int id) const;

  QList<ConflictDocumentItem> documentItems(int contextLines = 3) const;

  QStringList allOursLines() const;
  QStringList allTheirsLines() const;

  QString oursLabel() const { return m_oursLabel; }
  QString theirsLabel() const { return m_theirsLabel; }

private:
  struct Segment {
    bool isConflict = false;
    QStringList plain;
    int regionIndex = -1;
  };

  struct Snapshot {
    QList<ConflictRegion> regions;
    QString description;
  };

  QStringList sideLines(bool ours) const;
  void pushUndo(const QString &description);
  int indexOf(int id) const;

  QList<Segment> m_segments;
  QList<ConflictRegion> m_regions;
  QList<Snapshot> m_undo;
  QList<Snapshot> m_redo;
  QStringList m_history;

  QString m_oursLabel;
  QString m_theirsLabel;

  bool m_loaded = false;
  bool m_trailingNewline = true;
};

bool textHasConflictMarkers(const QString &content);

int countConflictMarkers(const QString &content);

#endif
