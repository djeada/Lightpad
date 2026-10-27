#include "gitblamewidget.h"
#include "../uistylehelper.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

GitBlameWidget::GitBlameWidget(QWidget *parent) : QWidget(parent) {
  setObjectName("gitBlameAnnotations");
  setAccessibleName(tr("Git blame annotations"));
  setAccessibleDescription(
      tr("Use Up and Down to select a line, Enter to "
         "inspect its commit, and Escape to return to code."));
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setEditorFont(QApplication::font());
}

QString GitBlameWidget::identity(const GitBlameLineInfo &info) {
  return info.commitHash.isEmpty() ? info.shortHash : info.commitHash;
}

bool GitBlameWidget::committed(const GitBlameLineInfo &info) {
  const QString hash = identity(info);
  return !hash.isEmpty() && hash.count('0') != hash.size();
}

void GitBlameWidget::setData(const QMap<int, GitBlameLineInfo> &data) {
  if (m_popup)
    m_popup->close();
  m_data = data;
  m_outdated = false;
  m_hoveredLine = 0;
  m_keyboardLine = 0;
  setAccessibleDescription(
      tr("Use Up and Down to select a line, Enter to "
         "inspect its commit, and Escape to return to code."));
  setToolTip(QString());
  update();
}

void GitBlameWidget::markOutdated() {
  setData({});
  m_outdated = true;
  setAccessibleDescription(
      tr("Edits pending. Save the file to refresh blame."));
  update();
}

void GitBlameWidget::setVisibleLines(const QVector<VisibleLine> &lines,
                                     int currentLine) {
  if (m_lines == lines && m_currentLine == currentLine)
    return;
  m_lines = lines;
  m_currentLine = currentLine;
  update();
}

void GitBlameWidget::setEditorFont(const QFont &font) {
  m_labelFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  m_labelFont.setPointSizeF(qBound(8.0, font.pointSizeF() - 1.0, 12.0));
  m_detailFont = m_labelFont;
  m_detailFont.setPointSizeF(qMax(8.0, m_labelFont.pointSizeF() - 1.0));
  update();
}

void GitBlameWidget::applyTheme(const Theme &theme) {
  m_theme = theme;
  if (m_popup)
    m_popup->close();
  update();
}

QVector<GitBlameWidget::Group> GitBlameWidget::visibleGroups() const {
  QVector<Group> groups;
  for (const auto &line : m_lines) {
    if (!m_data.contains(line.number))
      continue;
    if (!groups.isEmpty() && groups.last().last + 1 == line.number &&
        identity(m_data.value(groups.last().last)) ==
            identity(m_data.value(line.number))) {
      groups.last().last = line.number;
      groups.last().rect.setBottom(line.top + line.height - 1);
    } else {
      groups.append(
          {line.number, line.number, QRect(0, line.top, width(), line.height)});
    }
  }
  return groups;
}

void GitBlameWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.fillRect(rect(), m_theme.surfaceColor);
  const QColor muted = UIStyleHelper::secondaryTextColor(m_theme);
  if (m_outdated) {
    painter.setFont(m_labelFont);
    painter.setPen(m_theme.foregroundColor);
    painter.drawText(rect().adjusted(10, 10, -10, 0),
                     Qt::AlignTop | Qt::TextWordWrap, tr("Edits pending"));
    painter.setFont(m_detailFont);
    painter.setPen(muted);
    painter.drawText(rect().adjusted(10, 34, -10, 0),
                     Qt::AlignTop | Qt::TextWordWrap,
                     tr("Save to refresh blame"));
    return;
  }
  const QFontMetrics labelMetrics(m_labelFont);
  const QFontMetrics detailMetrics(m_detailFont);
  const int selected = hasFocus() ? m_keyboardLine : m_currentLine;
  const auto groups = visibleGroups();
  for (const auto &group : groups) {
    const auto info = m_data.value(group.first);
    const bool local = !committed(info);
    const bool active = selected >= group.first && selected <= group.last;
    const bool hovered =
        m_hoveredLine >= group.first && m_hoveredLine <= group.last;
    QColor accent = local ? m_theme.warningColor : m_theme.accentColor;
    QColor tint = accent;
    tint.setAlpha(10);
    painter.fillRect(group.rect, tint);
    if (active || hovered)
      painter.fillRect(group.rect,
                       active ? m_theme.accentSoftColor : m_theme.hoverColor);
    painter.fillRect(0, group.rect.top() + 2, 2,
                     qMax(0, group.rect.height() - 4), accent);
    painter.setPen(m_theme.borderColor);
    painter.drawLine(8, group.rect.top(), width() - 8, group.rect.top());

    const int firstHeight =
        m_lines.isEmpty() ? labelMetrics.height() + 4 : [&]() {
          for (const auto &line : m_lines)
            if (line.number == group.first)
              return line.height;
          return labelMetrics.height() + 4;
        }();
    const QString age = local ? QString() : info.relativeDate;
    const int ageWidth =
        qMin(detailMetrics.horizontalAdvance(age), width() / 2);
    const int nameWidth =
        qMax(0, width() - 24 - (age.isEmpty() ? 0 : ageWidth + 12));
    painter.setFont(m_labelFont);
    painter.setPen(m_theme.foregroundColor);
    painter.drawText(QRect(10, group.rect.top(), nameWidth, firstHeight),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     labelMetrics.elidedText(local ? tr("Uncommitted changes")
                                             : info.author.isEmpty()
                                                 ? tr("Unknown author")
                                                 : info.author,
                                             Qt::ElideRight, nameWidth));
    painter.setFont(m_detailFont);
    painter.setPen(muted);
    painter.drawText(
        QRect(width() - ageWidth - 10, group.rect.top(), ageWidth, firstHeight),
        Qt::AlignVCenter | Qt::AlignRight,
        detailMetrics.elidedText(age, Qt::ElideRight, ageWidth));
    if (group.rect.height() >= firstHeight * 2) {
      const QString detail =
          local ? tr("Working tree · not committed yet")
                : tr("%1 · %2").arg(info.shortHash, info.summary);
      painter.drawText(QRect(10, group.rect.top() + firstHeight,
                             qMax(0, width() - 20), firstHeight),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       detailMetrics.elidedText(detail, Qt::ElideRight,
                                                qMax(0, width() - 20)));
    }
    if (hasFocus() && active) {
      painter.setPen(QPen(m_theme.accentColor, 1, Qt::DotLine));
      painter.drawRect(group.rect.adjusted(3, 2, -4, -2));
    }
  }
  painter.setPen(m_theme.borderColor);
  painter.drawLine(width() - 1, 0, width() - 1, height());
}

int GitBlameWidget::lineAt(int y) const {
  for (const auto &line : m_lines)
    if (y >= line.top && y < line.top + line.height &&
        m_data.contains(line.number))
      return line.number;
  return 0;
}

QString GitBlameWidget::attribution(int line) const {
  const auto info = m_data.value(line);
  return committed(info)
             ? tr("%1 · %2\n%3\n%4\nClick or press Enter for commit actions")
                   .arg(info.author, info.relativeDate, info.summary,
                        identity(info))
             : tr("Uncommitted changes\nThese lines are part of your working "
                  "tree.");
}

void GitBlameWidget::mouseMoveEvent(QMouseEvent *event) {
  const int line = lineAt(event->position().toPoint().y());
  if (line != m_hoveredLine) {
    m_hoveredLine = line;
    setToolTip(line ? attribution(line) : QString());
    setCursor(line ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
  }
}

void GitBlameWidget::leaveEvent(QEvent *) {
  m_hoveredLine = 0;
  update();
}

void GitBlameWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    const int line = lineAt(event->position().toPoint().y());
    if (line) {
      m_keyboardLine = line;
      emit lineSelected(line);
      showDetails(line);
      event->accept();
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

void GitBlameWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
      event->key() == Qt::Key_Space) {
    showDetails(m_keyboardLine);
  } else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
    auto it = m_data.lowerBound(m_keyboardLine);
    if (event->key() == Qt::Key_Up && it != m_data.begin())
      --it;
    else if (event->key() == Qt::Key_Down && it != m_data.end())
      ++it;
    if (it != m_data.end()) {
      m_keyboardLine = it.key();
      setAccessibleDescription(attribution(m_keyboardLine));
      emit lineSelected(m_keyboardLine);
      update();
    }
  } else if (event->key() == Qt::Key_Escape) {
    emit returnToEditorRequested();
  } else {
    QWidget::keyPressEvent(event);
    return;
  }
  event->accept();
}

void GitBlameWidget::focusInEvent(QFocusEvent *event) {
  if (!m_data.isEmpty() && !m_data.contains(m_keyboardLine))
    m_keyboardLine =
        m_data.contains(m_currentLine) ? m_currentLine : m_data.firstKey();
  if (m_keyboardLine)
    setAccessibleDescription(attribution(m_keyboardLine));
  QWidget::focusInEvent(event);
  update();
}

void GitBlameWidget::focusOutEvent(QFocusEvent *event) {
  QWidget::focusOutEvent(event);
  update();
}

void GitBlameWidget::contextMenuEvent(QContextMenuEvent *event) {
  const int line = event->reason() == QContextMenuEvent::Keyboard
                       ? m_keyboardLine
                       : lineAt(event->pos().y());
  QMenu menu(this);
  if (line) {
    const auto info = m_data.value(line);
    menu.addAction(tr("Inspect change…"),
                   [this, line]() { showDetails(line); });
    auto *copy = menu.addAction(tr("Copy Commit SHA"), [info]() {
      QGuiApplication::clipboard()->setText(identity(info));
    });
    copy->setEnabled(committed(info));
    menu.addAction(tr("File history"),
                   [this, line]() { emit fileHistoryRequested(line); });
    menu.addSeparator();
  }
  menu.addAction(tr("Hide blame annotations"), this,
                 &GitBlameWidget::hideRequested);
  menu.exec(event->globalPos());
}

void GitBlameWidget::showDetails(int line) {
  if (!m_data.contains(line))
    return;
  if (m_popup)
    m_popup->close();
  const auto info = m_data.value(line);
  const bool local = !committed(info);
  int first = line, last = line;
  while (m_data.contains(first - 1) &&
         identity(m_data.value(first - 1)) == identity(info))
    --first;
  while (m_data.contains(last + 1) &&
         identity(m_data.value(last + 1)) == identity(info))
    ++last;

  auto *popup = new QFrame(this, Qt::Popup);
  m_popup = popup;
  popup->setObjectName("gitBlameDetails");
  popup->setAccessibleName(tr("Git change details"));
  popup->setAttribute(Qt::WA_DeleteOnClose);
  popup->setFixedWidth(390);
  popup->setFont(m_labelFont);
  popup->setStyleSheet(
      QString("QFrame#gitBlameDetails { background: %1; border: 1px solid %2; "
              "border-radius: 8px; } QLabel { color: %3; background: "
              "transparent; border: none; }")
          .arg(m_theme.surfaceColor.name(), m_theme.borderColor.name(),
               m_theme.foregroundColor.name()));
  auto *layout = new QVBoxLayout(popup);
  layout->setContentsMargins(18, 14, 18, 16);
  layout->setSpacing(12);
  auto *heading = new QHBoxLayout;
  auto *region = new QLabel(
      first == last ? tr("LAST CHANGE · LINE %1").arg(first)
                    : tr("LAST CHANGE · LINES %1–%2").arg(first).arg(last),
      popup);
  region->setObjectName("gitBlameRegion");
  region->setStyleSheet(
      QString("color: %1; font-size: 10px; font-weight: 600;")
          .arg(UIStyleHelper::secondaryTextColor(m_theme).name()));
  heading->addWidget(region, 1);
  auto *close = new QToolButton(popup);
  close->setText(QStringLiteral("×"));
  close->setAccessibleName(tr("Close change details"));
  close->setAutoRaise(true);
  close->setStyleSheet(
      QString("QToolButton { color: %1; background: transparent; "
              "border: none; font-size: 18px; padding: 0 4px; }")
          .arg(m_theme.foregroundColor.name()));
  connect(close, &QToolButton::clicked, popup, &QWidget::close);
  heading->addWidget(close);
  layout->addLayout(heading);
  auto addLabel = [&](const QString &text, const QString &name) {
    auto *label = new QLabel(text, popup);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(label);
    return label;
  };
  auto *summary = addLabel(local ? tr("Uncommitted changes") : info.summary,
                           "gitBlameSummary");
  QFont title = m_labelFont;
  title.setPointSizeF(qMax(11.0, m_labelFont.pointSizeF() + 2.0));
  title.setWeight(QFont::DemiBold);
  summary->setFont(title);
  if (local) {
    addLabel(tr("These lines have not been committed yet. Their history will "
                "be available after a commit."),
             "gitBlameAuthor");
  } else {
    addLabel(info.author + (info.authorEmail.isEmpty()
                                ? QString()
                                : QStringLiteral("\n") + info.authorEmail),
             "gitBlameAuthor");
    const QDateTime date = QDateTime::fromString(info.date, Qt::ISODate);
    addLabel(date.isValid()
                 ? QLocale().toString(date.toLocalTime(), QLocale::LongFormat)
                 : info.date,
             "gitBlameDate");
  }
  auto *hashRow = new QHBoxLayout;
  auto *hash = new QLabel(local ? tr("Working tree") : info.shortHash, popup);
  hash->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  hash->setToolTip(identity(info));
  hashRow->addWidget(hash, 1);
  auto *copy = new QPushButton(tr("Copy SHA"), popup);
  copy->setObjectName("gitBlameCopySha");
  copy->setEnabled(!local);
  copy->setToolTip(tr("Copy the full commit SHA"));
  copy->setStyleSheet(UIStyleHelper::secondaryButtonStyle(m_theme));
  connect(copy, &QPushButton::clicked, popup, [info, copy]() {
    QGuiApplication::clipboard()->setText(identity(info));
    copy->setText(tr("Copied"));
  });
  hashRow->addWidget(copy);
  layout->addLayout(hashRow);
  auto *actions = new QHBoxLayout;
  auto *open = new QPushButton(tr("Open commit"), popup);
  open->setObjectName("gitBlameOpenCommit");
  open->setEnabled(!local);
  open->setStyleSheet(UIStyleHelper::primaryButtonStyle(m_theme));
  connect(open, &QPushButton::clicked, popup, [this, popup, info]() {
    popup->close();
    emit openCommitRequested(identity(info));
  });
  actions->addWidget(open);
  auto *history = new QPushButton(tr("File history"), popup);
  history->setObjectName("gitBlameFileHistory");
  history->setStyleSheet(UIStyleHelper::secondaryButtonStyle(m_theme));
  connect(history, &QPushButton::clicked, popup, [this, popup, line]() {
    popup->close();
    emit fileHistoryRequested(line);
  });
  actions->addWidget(history);
  layout->addLayout(actions);
  auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), popup);
  connect(escape, &QShortcut::activated, popup, &QWidget::close);
  popup->adjustSize();
  QPoint anchor = mapToGlobal(QPoint(width() + 6, 0));
  for (const auto &visible : m_lines)
    if (visible.number == line)
      anchor = mapToGlobal(QPoint(width() + 6, qMax(0, visible.top)));
  if (auto *screen = QGuiApplication::screenAt(anchor)) {
    const QRect available = screen->availableGeometry().adjusted(8, 8, -8, -8);
    anchor.setX(
        qBound(available.left(), anchor.x(),
               qMax(available.left(), available.right() - popup->width())));
    anchor.setY(
        qBound(available.top(), anchor.y(),
               qMax(available.top(), available.bottom() - popup->height())));
  }
  popup->move(anchor);
  popup->show();
  (local ? history : open)->setFocus();
}
