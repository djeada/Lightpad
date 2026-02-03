#include "gitdiffdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextOption>
#include <QVBoxLayout>

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include "../uistylehelper.h"

namespace {
constexpr int kToolbarSpacing = 8;
constexpr int kDiffPreviewLimit = 80000;
constexpr int kDiffToneUnified = 150;
constexpr int kDiffToneSplit = 150;
constexpr int kDiffToneWord = 170;
} // namespace

GitDiffDialog::GitDiffDialog(GitIntegration *git, const QString &targetId,
                             DiffTarget target, bool staged, const Theme &theme,
                             QWidget *parent)
    : QDialog(parent), m_git(git), m_targetId(targetId), m_target(target),
      m_staged(staged), m_diffText(), m_wordDiffText(), m_summaryText(),
      m_viewMode(DiffViewMode::Unified), m_diffView(nullptr),
      m_summaryLabel(nullptr), m_changeCounterLabel(nullptr),
      m_modeSelector(nullptr), m_wrapToggle(nullptr), m_searchField(nullptr),
      m_findPrevButton(nullptr), m_findNextButton(nullptr),
      m_prevButton(nullptr), m_nextButton(nullptr), m_copyButton(nullptr),
      m_lines(), m_changeBlocks(), m_currentChange(0), m_addedCount(0),
      m_deletedCount(0), m_totalAdded(0), m_totalDeleted(0), m_theme(theme) {
  setModal(true);
  setWindowTitle(tr("Diff Viewer"));
  if (m_target == DiffTarget::File) {
    QFileInfo info(m_targetId);
    m_summaryText =
        tr("Diff • %1")
            .arg(info.fileName().isEmpty() ? m_targetId : info.fileName());
  } else {
    m_summaryText = tr("Commit diff • %1").arg(m_targetId.left(7));
  }
  resize(960, 640);
  buildUi();
  applyTheme(theme);
}

void GitDiffDialog::setDiffText(const QString &diffText) {
  m_diffText = diffText.left(kDiffPreviewLimit);
  m_wordDiffText.clear();
  if (m_summaryText.isEmpty()) {
    m_summaryText = tr("Diff");
  }
  m_totalAdded = 0;
  m_totalDeleted = 0;
  parseDiff(m_diffText);
  m_totalAdded = m_addedCount;
  m_totalDeleted = m_deletedCount;
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText);
  }
  if (m_changeCounterLabel) {
    m_changeCounterLabel->setText(
        tr("0 changes • +%1 -%2").arg(m_totalAdded).arg(m_totalDeleted));
  }
  updateDiffPresentation();
}

void GitDiffDialog::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Find)) {
    if (m_searchField) {
      m_searchField->setFocus();
      m_searchField->selectAll();
    }
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_F3 && !(event->modifiers() & Qt::ShiftModifier)) {
    performSearch(false);
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F3 && (event->modifiers() & Qt::ShiftModifier)) {
    performSearch(true);
    event->accept();
    return;
  }

  QDialog::keyPressEvent(event);
}

void GitDiffDialog::onModeChanged(int index) {
  if (index == 0) {
    m_viewMode = DiffViewMode::Unified;
  } else if (index == 1) {
    m_viewMode = DiffViewMode::Split;
  } else {
    m_viewMode = DiffViewMode::Word;
  }
  updateDiffPresentation();
}

void GitDiffDialog::onWrapToggled(bool enabled) {
  if (m_diffView) {
    m_diffView->setLineWrapMode(enabled ? QTextEdit::WidgetWidth
                                        : QTextEdit::NoWrap);
  }
}

void GitDiffDialog::onPrevChange() {
  if (m_changeBlocks.isEmpty()) {
    return;
  }
  m_currentChange =
      (m_currentChange - 1 + m_changeBlocks.size()) % m_changeBlocks.size();
  scrollToChange(m_currentChange);
  updateChangeCounter();
}

void GitDiffDialog::onNextChange() {
  if (m_changeBlocks.isEmpty()) {
    return;
  }
  m_currentChange = (m_currentChange + 1) % m_changeBlocks.size();
  scrollToChange(m_currentChange);
  updateChangeCounter();
}

void GitDiffDialog::onCopy() {
  if (!m_diffView) {
    return;
  }
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(m_diffView->toPlainText());
}

void GitDiffDialog::buildUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto *headerLayout = new QHBoxLayout();
  headerLayout->setSpacing(kToolbarSpacing);

  m_summaryLabel = new QLabel(tr("Diff"), this);
  m_summaryLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  headerLayout->addWidget(m_summaryLabel, 1);

  m_changeCounterLabel = new QLabel(this);
  headerLayout->addWidget(m_changeCounterLabel);

  m_prevButton = new QPushButton("◀", this);
  m_prevButton->setToolTip(tr("Previous change (Shift+F3)"));
  headerLayout->addWidget(m_prevButton);

  m_nextButton = new QPushButton("▶", this);
  m_nextButton->setToolTip(tr("Next change (F3)"));
  headerLayout->addWidget(m_nextButton);

  m_modeSelector = new QComboBox(this);
  m_modeSelector->addItems({tr("Unified"), tr("Split"), tr("Word")});
  headerLayout->addWidget(m_modeSelector);

  m_wrapToggle = new QCheckBox(tr("Wrap"), this);
  m_wrapToggle->setChecked(true);
  headerLayout->addWidget(m_wrapToggle);

  m_searchField = new QLineEdit(this);
  m_searchField->setPlaceholderText(tr("Search diff"));
  m_searchField->setMinimumWidth(160);
  headerLayout->addWidget(m_searchField);

  m_findPrevButton = new QPushButton("◀", this);
  m_findPrevButton->setToolTip(tr("Find previous"));
  headerLayout->addWidget(m_findPrevButton);

  m_findNextButton = new QPushButton("▶", this);
  m_findNextButton->setToolTip(tr("Find next"));
  headerLayout->addWidget(m_findNextButton);

  m_copyButton = new QPushButton(tr("Copy"), this);
  headerLayout->addWidget(m_copyButton);

  layout->addLayout(headerLayout);

  m_diffView = new QTextEdit(this);
  m_diffView->setReadOnly(true);
  m_diffView->setLineWrapMode(QTextEdit::WidgetWidth);
  m_diffView->setUndoRedoEnabled(false);
  m_diffView->setAcceptRichText(true);
  m_diffView->setFontFamily("monospace");
  m_diffView->setTabStopDistance(
      QFontMetrics(m_diffView->font()).horizontalAdvance(' ') * 4);
  m_diffView->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  layout->addWidget(m_diffView, 1);

  connect(m_modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GitDiffDialog::onModeChanged);
  connect(m_wrapToggle, &QCheckBox::toggled, this,
          &GitDiffDialog::onWrapToggled);
  connect(m_prevButton, &QPushButton::clicked, this,
          &GitDiffDialog::onPrevChange);
  connect(m_nextButton, &QPushButton::clicked, this,
          &GitDiffDialog::onNextChange);
  connect(m_searchField, &QLineEdit::returnPressed,
          [this]() { performSearch(false); });
  connect(m_searchField, &QLineEdit::textChanged,
          [this](const QString &queryText) {
            if (queryText.size() < 2) {
              return;
            }
            performSearch(false);
          });
  connect(m_findPrevButton, &QPushButton::clicked,
          [this]() { performSearch(true); });
  connect(m_findNextButton, &QPushButton::clicked,
          [this]() { performSearch(false); });
  connect(m_copyButton, &QPushButton::clicked, this, &GitDiffDialog::onCopy);
}

void GitDiffDialog::applyTheme(const Theme &theme) {
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  if (m_summaryLabel) {
    m_summaryLabel->setStyleSheet(UIStyleHelper::titleLabelStyle(theme));
  }
  if (m_changeCounterLabel) {
    m_changeCounterLabel->setStyleSheet(UIStyleHelper::infoLabelStyle(theme));
  }
  if (m_modeSelector) {
    m_modeSelector->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }
  if (m_wrapToggle) {
    m_wrapToggle->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  }
  if (m_searchField) {
    m_searchField->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }
  for (QPushButton *button : {m_prevButton, m_nextButton, m_findPrevButton,
                              m_findNextButton, m_copyButton}) {
    if (button) {
      button->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
    }
  }
  if (m_diffView) {
    QString textStyle = QString("QTextEdit {"
                                "  background: %1;"
                                "  color: %2;"
                                "  border: 1px solid %3;"
                                "  border-radius: 6px;"
                                "}")
                            .arg(theme.backgroundColor.name())
                            .arg(theme.foregroundColor.name())
                            .arg(theme.borderColor.name());
    m_diffView->setStyleSheet(textStyle);
  }
}

void GitDiffDialog::updateDiffPresentation() {
  if (!m_diffView) {
    return;
  }

  if (m_diffText.isEmpty()) {
    m_lines.clear();
    m_changeBlocks.clear();
    updateChangeCounter();
    m_diffView->setPlainText(tr("No diff content available."));
    if (m_summaryLabel && !m_summaryText.isEmpty()) {
      m_summaryLabel->setText(m_summaryText);
    }
    return;
  }

  if (m_viewMode == DiffViewMode::Word && m_wordDiffText.isEmpty()) {
    m_wordDiffText = resolveWordDiff();
  }
  const QString diffSource =
      (m_viewMode == DiffViewMode::Word && !m_wordDiffText.isEmpty())
          ? m_wordDiffText
          : m_diffText;
  parseDiff(diffSource);
  m_addedCount = m_totalAdded;
  m_deletedCount = m_totalDeleted;
  collectChangeBlocks();
  int verticalScroll = m_diffView->verticalScrollBar()->value();
  int horizontalScroll = m_diffView->horizontalScrollBar()->value();
  switch (m_viewMode) {
  case DiffViewMode::Unified:
    rebuildUnified();
    break;
  case DiffViewMode::Split:
    rebuildSplit();
    break;
  case DiffViewMode::Word:
    rebuildWord();
    break;
  }
  m_diffView->verticalScrollBar()->setValue(verticalScroll);
  m_diffView->horizontalScrollBar()->setValue(horizontalScroll);
  updateChangeCounter();
}

void GitDiffDialog::rebuildUnified() {
  QString html;
  html += "<html><head><style>";
  html += "body { background:" + m_theme.backgroundColor.name() +
          "; color:" + m_theme.foregroundColor.name() + "; }";
  html +=
      ".meta { color:" + m_theme.foregroundColor.lighter(140).name() + "; }";
  html += ".add { background:" +
          m_theme.successColor.lighter(kDiffToneUnified).name() +
          "; color:" + m_theme.foregroundColor.name() + "; }";
  html += ".del { background:" +
          m_theme.errorColor.lighter(kDiffToneUnified).name() +
          "; color:" + m_theme.foregroundColor.name() + "; }";
  html += ".ctx { color:" + m_theme.foregroundColor.name() + "; }";
  html += "pre { font-family: monospace; font-size: 12px; margin: 0; "
          "white-space: pre-wrap; }";
  html += "</style></head><body><pre>";

  for (const auto &line : m_lines) {
    QString escaped = htmlEscape(line.content);
    if (line.prefix == '+') {
      html += "<span class=\"add\">+" + escaped + "</span>\n";
    } else if (line.prefix == '-') {
      html += "<span class=\"del\">-" + escaped + "</span>\n";
    } else if (line.prefix == '@' || line.prefix == 'd' || line.prefix == 'i' ||
               line.prefix == 'h') {
      html += "<span class=\"meta\">" + escaped + "</span>\n";
    } else {
      html += "<span class=\"ctx\">" + escaped + "</span>\n";
    }
  }

  html += "</pre></body></html>";
  m_diffView->setHtml(html);
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText.isEmpty() ? tr("Diff")
                                                    : m_summaryText);
  }
}

void GitDiffDialog::rebuildSplit() {
  QString html;
  html += "<html><head><style>";
  html += "body { background:" + m_theme.backgroundColor.name() +
          "; color:" + m_theme.foregroundColor.name() + "; }";
  html += "table { width: 100%; border-collapse: collapse; font-family: "
          "monospace; font-size: 12px; }";
  html +=
      "td { vertical-align: top; padding: 2px 6px; white-space: pre-wrap; }";
  html +=
      ".meta { color:" + m_theme.foregroundColor.lighter(140).name() + "; }";
  html += ".add { background:" +
          m_theme.successColor.lighter(kDiffToneSplit).name() + "; }";
  html +=
      ".del { background:" + m_theme.errorColor.lighter(kDiffToneSplit).name() +
      "; }";
  html += ".ctx { color:" + m_theme.foregroundColor.name() + "; }";
  html += ".gutter { width: 50%; }";
  html += "</style></head><body><table>";

  for (const auto &line : m_lines) {
    QString escaped = htmlEscape(line.content);
    if (line.prefix == '+') {
      html += "<tr><td class=\"gutter\"></td><td class=\"add\">+" + escaped +
              "</td></tr>";
    } else if (line.prefix == '-') {
      html += "<tr><td class=\"del\">-" + escaped +
              "</td><td class=\"gutter\"></td></tr>";
    } else if (line.prefix == '@' || line.prefix == 'd' || line.prefix == 'i' ||
               line.prefix == 'h') {
      html += "<tr><td class=\"meta\" colspan=\"2\">" + escaped + "</td></tr>";
    } else {
      html += "<tr><td class=\"ctx\">" + escaped + "</td><td class=\"ctx\">" +
              escaped + "</td></tr>";
    }
  }

  html += "</table></body></html>";
  m_diffView->setHtml(html);
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText.isEmpty() ? tr("Diff")
                                                    : m_summaryText);
  }
}

void GitDiffDialog::rebuildWord() {
  QString html;
  html += "<html><head><style>";
  html += "body { background:" + m_theme.backgroundColor.name() +
          "; color:" + m_theme.foregroundColor.name() + "; }";
  html +=
      ".meta { color:" + m_theme.foregroundColor.lighter(140).name() + "; }";
  html += ".add { background:" +
          m_theme.successColor.lighter(kDiffToneWord).name() +
          "; color:" + m_theme.foregroundColor.name() + "; }";
  html +=
      ".del { background:" + m_theme.errorColor.lighter(kDiffToneWord).name() +
      "; color:" + m_theme.foregroundColor.name() + "; }";
  html += ".ctx { color:" + m_theme.foregroundColor.name() + "; }";
  html += "pre { font-family: monospace; font-size: 12px; margin: 0; "
          "white-space: pre-wrap; }";
  html += "</style></head><body><pre>";

  for (const auto &line : m_lines) {
    const bool hasWordMarkers =
        line.content.contains("{+") || line.content.contains("[-");
    if (line.prefix == '@' || line.prefix == 'd' || line.prefix == 'i' ||
        line.prefix == 'h') {
      html += "<span class=\"meta\">" + htmlEscape(line.content) + "</span>\n";
    } else if (line.prefix == '+' || line.prefix == '-') {
      if (hasWordMarkers) {
        html += buildWordDiffLine(line.content) + "\n";
      } else {
        html +=
            styleToken(line.content, line.prefix == '+' ? "add" : "del") + "\n";
      }
    } else if (hasWordMarkers) {
      html += buildWordDiffLine(line.content) + "\n";
    } else {
      html += "<span class=\"ctx\">" + htmlEscape(line.content) + "</span>\n";
    }
  }

  html += "</pre></body></html>";
  m_diffView->setHtml(html);
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText.isEmpty() ? tr("Diff")
                                                    : m_summaryText);
  }
}

void GitDiffDialog::parseDiff(const QString &diffText) {
  m_lines.clear();
  m_addedCount = 0;
  m_deletedCount = 0;
  if (diffText.isEmpty()) {
    return;
  }
  const QStringList rawLines = diffText.split('\n');
  for (const QString &rawLine : rawLines) {
    DiffLine line;
    if (rawLine.startsWith("diff --git")) {
      line.prefix = 'd';
      line.content = rawLine;
    } else if (rawLine.startsWith("index ") || rawLine.startsWith("new file") ||
               rawLine.startsWith("deleted file") ||
               rawLine.startsWith("--- ") || rawLine.startsWith("+++ ") ||
               rawLine.startsWith("rename ") ||
               rawLine.startsWith("similarity ")) {
      line.prefix = 'i';
      line.content = rawLine;
    } else if (rawLine.startsWith("@@")) {
      line.prefix = '@';
      line.content = rawLine;
    } else if (rawLine.startsWith("+") && !rawLine.startsWith("+++")) {
      line.prefix = '+';
      line.content = rawLine.mid(1);
      m_addedCount++;
    } else if (rawLine.startsWith("-") && !rawLine.startsWith("---")) {
      line.prefix = '-';
      line.content = rawLine.mid(1);
      m_deletedCount++;
    } else {
      line.prefix = ' ';
      line.content = rawLine.startsWith(" ") ? rawLine.mid(1) : rawLine;
    }
    m_lines.append(line);
  }
}

void GitDiffDialog::collectChangeBlocks() {
  m_changeBlocks.clear();
  int start = -1;
  for (int i = 0; i < m_lines.size(); ++i) {
    const QChar prefix = m_lines[i].prefix;
    const bool isChange = (prefix == '+' || prefix == '-');
    if (isChange && start < 0) {
      start = i;
    } else if (!isChange && start >= 0) {
      m_changeBlocks.append({start, i - 1});
      start = -1;
    }
  }
  if (start >= 0) {
    m_changeBlocks.append({start, m_lines.size() - 1});
  }
  m_currentChange = m_changeBlocks.isEmpty()
                        ? 0
                        : qMin(m_currentChange, m_changeBlocks.size() - 1);
}

void GitDiffDialog::scrollToChange(int index) {
  if (!m_diffView || m_changeBlocks.isEmpty() || index < 0 ||
      index >= m_changeBlocks.size()) {
    return;
  }
  int lineIndex = m_changeBlocks[index].first;
  QTextCursor cursor = m_diffView->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineIndex);
  m_diffView->setTextCursor(cursor);
  m_diffView->ensureCursorVisible();
}

void GitDiffDialog::updateChangeCounter() {
  if (!m_changeCounterLabel) {
    return;
  }
  if (m_changeBlocks.isEmpty()) {
    m_changeCounterLabel->setText(
        tr("0 changes • +%1 -%2").arg(m_addedCount).arg(m_deletedCount));
  } else {
    m_changeCounterLabel->setText(tr("%1 of %2 • +%3 -%4")
                                      .arg(m_currentChange + 1)
                                      .arg(m_changeBlocks.size())
                                      .arg(m_addedCount)
                                      .arg(m_deletedCount));
  }
}

void GitDiffDialog::performSearch(bool backwards) {
  if (!m_diffView || !m_searchField) {
    return;
  }
  const QString query = m_searchField->text();
  if (query.isEmpty()) {
    return;
  }
  if (!m_diffView->hasFocus()) {
    m_diffView->setFocus();
  }
  QTextDocument::FindFlags flags;
  if (backwards) {
    flags |= QTextDocument::FindBackward;
  }
  if (!m_diffView->find(query, flags)) {
    QTextCursor cursor = m_diffView->textCursor();
    cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
    m_diffView->setTextCursor(cursor);
    m_diffView->find(query, flags);
  }
}

QString GitDiffDialog::resolveWordDiff() {
  if (!m_git) {
    return QString();
  }
  if (m_target == DiffTarget::Commit) {
    return m_git->executeWordDiff({"show", "--word-diff", "--color=never",
                                   "--pretty=format:", m_targetId});
  }
  QString relativePath = m_targetId;
  if (!m_git->repositoryPath().isEmpty() &&
      relativePath.startsWith(m_git->repositoryPath())) {
    relativePath = relativePath.mid(m_git->repositoryPath().length() + 1);
  }
  if (m_staged) {
    return m_git->executeWordDiff({"diff", "--cached", "--word-diff",
                                   "--color=never", "--", relativePath});
  }
  return m_git->executeWordDiff(
      {"diff", "--word-diff", "--color=never", "--", relativePath});
}

QString GitDiffDialog::htmlEscape(const QString &text) const {
  QString escaped = text;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  return escaped;
}

QString GitDiffDialog::buildWordDiffLine(const QString &line) const {
  QString output;
  bool inDelete = false;
  bool inAdd = false;
  QString token;

  auto flushToken = [&]() {
    if (token.isEmpty()) {
      return;
    }
    if (inDelete) {
      output += styleToken(token, "del");
    } else if (inAdd) {
      output += styleToken(token, "add");
    } else {
      output += htmlEscape(token);
    }
    token.clear();
  };

  for (int i = 0; i < line.size(); ++i) {
    const QChar ch = line[i];
    if (ch == '{' && i + 1 < line.size() && line[i + 1] == '+') {
      flushToken();
      inAdd = true;
      i++;
      continue;
    }
    if (ch == '+' && inAdd && i + 1 < line.size() && line[i + 1] == '}') {
      flushToken();
      inAdd = false;
      i++;
      continue;
    }
    if (ch == '[' && i + 1 < line.size() && line[i + 1] == '-') {
      flushToken();
      inDelete = true;
      i++;
      continue;
    }
    if (ch == '-' && inDelete && i + 1 < line.size() && line[i + 1] == ']') {
      flushToken();
      inDelete = false;
      i++;
      continue;
    }
    token.append(ch);
  }
  flushToken();
  return output;
}

QString GitDiffDialog::styleToken(const QString &token,
                                  const QString &cssClass) const {
  return QString("<span class=\"%1\">%2</span>")
      .arg(cssClass, htmlEscape(token));
}
