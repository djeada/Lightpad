#include "vimmode.h"

#include <QTextBlock>
#include <algorithm>

// Manual folds (foldmethod=manual). Folds are kept as a flat list of line
// ranges anchored with QTextCursors; nesting follows from containment. A
// closed fold hides every line after its first one, so motions can treat any
// run of hidden blocks (including the editor's own syntax folds) as a single
// line, just like Vim does.

namespace {

int foldFirst(const QTextCursor &c) { return c.blockNumber(); }

} // namespace

void VimMode::normalizeFolds() {
  for (int i = m_folds.size() - 1; i >= 0; --i) {
    const VimFold &f = m_folds[i];
    if (f.start.isNull() || f.end.isNull() || f.start.document() != doc() ||
        foldFirst(f.start) > foldFirst(f.end))
      m_folds.removeAt(i);
  }
}

QList<int> VimMode::foldsContaining(int line) const {
  QList<int> result;
  for (int i = 0; i < m_folds.size(); ++i) {
    const int s = foldFirst(m_folds[i].start);
    const int e = foldFirst(m_folds[i].end);
    if (s <= line && line <= e)
      result << i;
  }
  std::sort(result.begin(), result.end(), [this](int a, int b) {
    const int sa = foldFirst(m_folds[a].start),
              sb = foldFirst(m_folds[b].start);
    const int ea = foldFirst(m_folds[a].end), eb = foldFirst(m_folds[b].end);
    if (sa != sb)
      return sa < sb;
    if (ea != eb)
      return ea > eb;
    return a < b;
  });
  return result;
}

bool VimMode::vimFoldClosedAt(int line, int *start, int *end) const {
  if (!m_foldEnable)
    return false;
  for (int i : foldsContaining(line)) {
    if (m_folds[i].closed) {
      if (start)
        *start = foldFirst(m_folds[i].start);
      if (end)
        *end = foldFirst(m_folds[i].end);
      return true;
    }
  }
  return false;
}

bool VimMode::closedFoldAt(int line, int *start, int *end) const {
  QTextBlock block = doc()->findBlockByNumber(line);
  if (!block.isValid())
    return false;
  if (block.isVisible()) {
    const QTextBlock next = block.next();
    if (!next.isValid() || next.isVisible())
      return false;
  }
  QTextBlock first = block;
  while (!first.isVisible() && first.previous().isValid())
    first = first.previous();
  QTextBlock lastBlock = first;
  while (lastBlock.next().isValid() && !lastBlock.next().isVisible())
    lastBlock = lastBlock.next();
  if (lastBlock.blockNumber() == first.blockNumber())
    return false;
  if (start)
    *start = first.blockNumber();
  if (end)
    *end = lastBlock.blockNumber();
  return true;
}

// Mirrors Vim's cursor_down(): every closed fold counts as one line. It
// only fails on the last line; from a closed fold that reaches the end of the
// buffer it moves to the last line.
int VimMode::foldLinesDown(int line, int count, bool *ok) const {
  if (ok)
    *ok = true;
  if (count <= 0)
    return line;
  const int last = lineCount() - 1;
  if (line >= last) {
    if (ok)
      *ok = false;
    return line;
  }
  int lnum = line;
  int e = 0;
  if (closedFoldAt(lnum, nullptr, &e))
    lnum = e;
  if (lnum + count >= last)
    return last;
  for (int n = 0; n < count; ++n) {
    if (closedFoldAt(lnum, nullptr, &e))
      lnum = e + 1;
    else
      ++lnum;
    if (lnum >= last)
      break;
  }
  return qMin(lnum, last);
}

// Mirrors Vim's cursor_up().
int VimMode::foldLinesUp(int line, int count, bool *ok) const {
  if (ok)
    *ok = true;
  if (count <= 0)
    return line;
  if (line <= 0) {
    if (ok)
      *ok = false;
    return line;
  }
  if (count >= line + 1)
    return 0;
  int lnum = line;
  int s = 0;
  if (closedFoldAt(lnum, &s, nullptr))
    lnum = s;
  for (int n = count; n > 0; --n) {
    --lnum;
    if (lnum <= 0)
      break;
    if (closedFoldAt(lnum, &s, nullptr))
      lnum = s;
  }
  return qMax(0, lnum);
}

void VimMode::createFold(int first, int last) {
  first = qBound(0, first, lineCount() - 1);
  last = qBound(first, last, lineCount() - 1);
  VimFold fold;
  fold.start = QTextCursor(doc());
  fold.start.setPosition(lineStart(first));
  fold.end = QTextCursor(doc());
  fold.end.setPosition(lineStart(last));
  fold.closed = true;
  m_folds.append(fold);
  m_foldEnable = true;
  applyFoldDisplay();
}

void VimMode::applyFoldDisplay() {
  normalizeFolds();
  QSet<int> hidden;
  if (m_foldEnable) {
    for (const VimFold &f : m_folds) {
      if (!f.closed)
        continue;
      const int s = foldFirst(f.start);
      const int e = foldFirst(f.end);
      // A closed fold inside another closed fold is covered by the outer one;
      // an open fold inside a closed one stays hidden as well.
      for (int l = s + 1; l <= e; ++l)
        hidden.insert(l);
    }
  }
  bool changed = false;
  QList<QTextCursor> keep;
  for (const QTextCursor &c : m_foldHidden) {
    if (c.isNull() || c.document() != doc())
      continue;
    QTextBlock b = c.block();
    if (!hidden.contains(b.blockNumber())) {
      if (!b.isVisible()) {
        b.setVisible(true);
        b.setLineCount(qMax(1, b.layout() ? b.layout()->lineCount() : 1));
        changed = true;
      }
    }
  }
  for (int l : hidden) {
    QTextBlock b = doc()->findBlockByNumber(l);
    if (!b.isValid())
      continue;
    if (b.isVisible()) {
      b.setVisible(false);
      b.setLineCount(0);
      changed = true;
    }
    QTextCursor c(doc());
    c.setPosition(b.position());
    keep.append(c);
  }
  m_foldHidden = keep;
  if (changed) {
    doc()->markContentsDirty(0, doc()->characterCount());
    m_editor->viewport()->update();
  }
}

void VimMode::forgetFolds(int first, int last) {
  for (int i = m_folds.size() - 1; i >= 0; --i) {
    VimFold &f = m_folds[i];
    const int s = foldFirst(f.start);
    const int e = foldFirst(f.end);
    if (s >= first && e <= last) {
      m_folds.removeAt(i);
    } else if (s < first && e >= first && e <= last) {
      f.end.setPosition(lineStart(first - 1));
    } else if (s >= first && s <= last && e > last) {
      f.start.setPosition(lineStart(last + 1));
    }
  }
}

void VimMode::openFoldsAt(int line, bool recursive, int count) {
  const QList<int> folds = foldsContaining(line);
  if (recursive) {
    for (int i : folds)
      m_folds[i].closed = false;
  } else {
    for (int n = 0; n < count; ++n) {
      bool opened = false;
      for (int i : foldsContaining(line)) {
        if (m_folds[i].closed) {
          m_folds[i].closed = false;
          opened = true;
          break;
        }
      }
      if (!opened)
        break;
    }
  }
  applyFoldDisplay();
}

void VimMode::closeFoldsAt(int line, bool recursive, int count) {
  const QList<int> folds = foldsContaining(line);
  if (recursive) {
    for (int i : folds)
      m_folds[i].closed = true;
  } else {
    for (int n = 0; n < count; ++n) {
      const QList<int> current = foldsContaining(line);
      int target = -1;
      for (int k = 0; k < current.size(); ++k) {
        if (m_folds[current[k]].closed) {
          target = k - 1;
          break;
        }
        target = k;
      }
      if (target < 0)
        break;
      m_folds[current[target]].closed = true;
    }
  }
  m_foldEnable = true;
  applyFoldDisplay();
}

void VimMode::setFoldLevel(int level) {
  m_foldLevel = qMax(0, level);
  for (int i = 0; i < m_folds.size(); ++i) {
    const int s = foldFirst(m_folds[i].start);
    const int e = foldFirst(m_folds[i].end);
    int depth = 0;
    for (const VimFold &other : m_folds) {
      if (foldFirst(other.start) <= s && foldFirst(other.end) >= e)
        ++depth;
    }
    m_folds[i].closed = depth > m_foldLevel;
  }
  applyFoldDisplay();
}

int VimMode::maxFoldDepth() const {
  int best = 0;
  for (const VimFold &f : m_folds) {
    int depth = 0;
    for (const VimFold &other : m_folds) {
      if (foldFirst(other.start) <= foldFirst(f.start) &&
          foldFirst(other.end) >= foldFirst(f.end))
        ++depth;
    }
    best = qMax(best, depth);
  }
  return best;
}

// Handles the z-commands that deal with folds. Returns false for keys that
// are not fold commands.
bool VimMode::executeFoldCommand(const QString &key, int count) {
  normalizeFolds();
  const int c1 = qMax(1, count);
  const int pos = cursorPos();
  const int line = lineOf(pos);
  const bool haveFold = !foldsContaining(line).isEmpty();

  auto moveTo = [&](int target) {
    if (target != line) {
      pushJump(pos);
      setCursorPos(lineStart(target));
    }
  };

  if (key == "zo" || key == "zO" || key == "zv") {
    if (!haveFold) {
      if (key != "zv")
        emit commandExecuted("unfold");
      return true;
    }
    openFoldsAt(line, key != "zo", c1);
  } else if (key == "zc" || key == "zC") {
    if (!haveFold) {
      emit commandExecuted("fold");
      return true;
    }
    closeFoldsAt(line, key == "zC", c1);
  } else if (key == "za" || key == "zA") {
    if (!haveFold) {
      emit commandExecuted("toggleFold");
      return true;
    }
    if (vimFoldClosedAt(line, nullptr, nullptr))
      openFoldsAt(line, key == "zA", c1);
    else
      closeFoldsAt(line, key == "zA", c1);
  } else if (key == "zR") {
    setFoldLevel(maxFoldDepth());
    emit commandExecuted("unfoldAll");
  } else if (key == "zM") {
    setFoldLevel(0);
    m_foldEnable = true;
    applyFoldDisplay();
    emit commandExecuted("foldAll");
  } else if (key == "zr") {
    setFoldLevel(qMin(maxFoldDepth(), m_foldLevel + c1));
  } else if (key == "zm") {
    setFoldLevel(qMax(0, m_foldLevel - c1));
  } else if (key == "zx" || key == "zX") {
    setFoldLevel(m_foldLevel);
    if (key == "zx" && haveFold)
      openFoldsAt(line, true, 1);
  } else if (key == "zn") {
    m_foldEnable = false;
    applyFoldDisplay();
  } else if (key == "zN") {
    m_foldEnable = true;
    applyFoldDisplay();
  } else if (key == "zi") {
    m_foldEnable = !m_foldEnable;
    applyFoldDisplay();
  } else if (key == "zd" || key == "zD") {
    const QList<int> folds = foldsContaining(line);
    if (folds.isEmpty()) {
      emit statusMessage("E490: No fold found");
      return true;
    }
    QList<int> remove;
    if (key == "zD") {
      int s = 0, e = 0;
      if (!vimFoldClosedAt(line, &s, &e)) {
        s = foldFirst(m_folds[folds.last()].start);
        e = foldFirst(m_folds[folds.last()].end);
      }
      for (int i = 0; i < m_folds.size(); ++i) {
        if (foldFirst(m_folds[i].start) >= s && foldFirst(m_folds[i].end) <= e)
          remove << i;
      }
    } else {
      int target = folds.last();
      for (int i : folds) {
        if (m_folds[i].closed && m_foldEnable) {
          target = i;
          break;
        }
      }
      remove << target;
    }
    std::sort(remove.begin(), remove.end());
    for (int k = remove.size() - 1; k >= 0; --k)
      m_folds.removeAt(remove[k]);
    applyFoldDisplay();
  } else if (key == "zE") {
    m_folds.clear();
    applyFoldDisplay();
  } else if (key == "zj") {
    int from = line;
    int e = 0;
    if (closedFoldAt(line, nullptr, &e))
      from = e;
    int target = line;
    for (int n = 0; n < c1; ++n) {
      int best = -1;
      for (const VimFold &f : m_folds) {
        const int s = foldFirst(f.start);
        if (s > from && (best < 0 || s < best))
          best = s;
      }
      if (best < 0)
        break;
      target = best;
      from = best;
      if (closedFoldAt(best, nullptr, &e))
        from = e;
    }
    if (target == line) {
      failCommand();
      return true;
    }
    moveTo(target);
  } else if (key == "zk") {
    int from = line;
    int s = 0;
    if (closedFoldAt(line, &s, nullptr))
      from = s;
    int target = line;
    for (int n = 0; n < c1; ++n) {
      int best = -1;
      for (const VimFold &f : m_folds) {
        const int e = foldFirst(f.end);
        if (e < from && e > best)
          best = e;
      }
      if (best < 0)
        break;
      target = best;
      from = best;
      if (closedFoldAt(best, &s, nullptr))
        from = s;
    }
    if (target == line) {
      failCommand();
      return true;
    }
    moveTo(target);
  } else if (key == "[z" || key == "]z") {
    const QList<int> folds = foldsContaining(line);
    if (folds.isEmpty()) {
      failCommand();
      return true;
    }
    const bool toStart = key == "[z";
    int target = line;
    for (int k = folds.size() - 1, n = 0; k >= 0 && n < c1; --k) {
      const int edge = toStart ? foldFirst(m_folds[folds[k]].start)
                               : foldFirst(m_folds[folds[k]].end);
      if (edge == target)
        continue;
      target = edge;
      ++n;
    }
    moveTo(target);
  } else {
    return false;
  }
  return true;
}

void VimMode::exFoldCommand(const QString &name, const QString &args, int line1,
                            int line2, bool hasRange, bool bang) {
  normalizeFolds();
  auto is = [&](const QString &full, int minLen) {
    return name.size() >= minLen && full.startsWith(name);
  };
  if (is("folddoopen", 5) || is("folddoclosed", 7)) {
    const bool closed = is("folddoclosed", 7);
    if (!hasRange) {
      line1 = 0;
      line2 = lineCount() - 1;
    }
    QList<QTextCursor> marks;
    for (int l = line1; l <= line2; ++l) {
      if (vimFoldClosedAt(l, nullptr, nullptr) == closed) {
        QTextCursor c(doc());
        c.setPosition(lineStart(l));
        marks.append(c);
      }
    }
    QTextCursor block(doc());
    block.beginEditBlock();
    const QList<QTextCursor> outerMarks = m_globalMarks;
    m_globalMarks = marks;
    for (int k = 0; k < m_globalMarks.size(); ++k) {
      const QTextCursor mark = m_globalMarks[k];
      if (mark.isNull())
        continue;
      setCursorPos(lineStart(mark.blockNumber()));
      executeEx(args);
      if (m_mode != VimEditMode::Normal)
        handleKey("<Esc>", nullptr);
    }
    m_globalMarks = outerMarks;
    block.endEditBlock();
    return;
  }
  if (is("foldopen", 5)) {
    for (int l = line1; l <= line2; ++l) {
      for (int i : foldsContaining(l)) {
        if (m_folds[i].closed) {
          m_folds[i].closed = false;
          if (!bang)
            break;
        }
      }
    }
    applyFoldDisplay();
    return;
  }
  if (is("foldclose", 5)) {
    QSet<int> toClose;
    for (int l = line1; l <= line2; ++l) {
      const QList<int> folds = foldsContaining(l);
      if (bang) {
        for (int i : folds)
          toClose.insert(i);
        continue;
      }
      for (int k = folds.size() - 1; k >= 0; --k) {
        if (!m_folds[folds[k]].closed) {
          toClose.insert(folds[k]);
          break;
        }
      }
    }
    for (int i : toClose)
      m_folds[i].closed = true;
    m_foldEnable = true;
    applyFoldDisplay();
    return;
  }
  // :fold
  createFold(line1, line2);
}

// 'foldopen': horizontal moves, searches, mark jumps and undo open the
// closed folds the cursor ends up in.
void VimMode::openFoldsForCursor() {
  if (m_folds.isEmpty())
    return;
  const int line = lineOf(cursorPos());
  if (vimFoldClosedAt(line, nullptr, nullptr))
    openFoldsAt(line, true, 1);
}
