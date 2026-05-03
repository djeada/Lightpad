#include "preferences.h"
#include "../../settings/settingsmanager.h"
#include "../../theme/themedefinition.h"
#include "../../theme/themeengine.h"
#include "../mainwindow.h"
#include "styleddialog.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtGlobal>
#include <functional>

namespace {
struct ThemeColorEntry {
  QString group;
  QString role;
  QString label;
  QString description;
};

class ThemePreviewCard : public QWidget {
public:
  explicit ThemePreviewCard(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumHeight(190);
  }

  void setDefinition(const ThemeDefinition &def) {
    m_def = def;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const ThemeColors &c = m_def.colors;
    qreal r = m_def.ui.borderRadius;
    QRectF full = rect().adjusted(1, 1, -1, -1);

    p.setPen(QPen(c.accentPrimary, 1));
    p.setBrush(c.surfaceBase);
    p.drawRoundedRect(full, r, r);

    QRectF tb(full.left() + 6, full.top() + 6, full.width() - 12, 22);
    p.setBrush(c.surfaceRaised);
    p.setPen(QPen(c.borderSubtle, 1));
    p.drawRoundedRect(tb, r / 2, r / 2);
    p.setPen(c.textPrimary);
    p.setFont(QFont("Monospace", 9, QFont::Bold));
    p.drawText(tb.adjusted(8, 0, 0, 0), Qt::AlignVCenter,
               "▶ " + m_def.name.toUpper());
    qreal dx = tb.right() - 10;
    for (const QColor &d : {c.statusError, c.statusWarning, c.statusSuccess}) {
      p.setBrush(d);
      p.setPen(Qt::NoPen);
      p.drawEllipse(QPointF(dx, tb.center().y()), 3.0, 3.0);
      dx -= 9;
    }

    QRectF body(full.left() + 6, tb.bottom() + 4, full.width() - 12,
                full.bottom() - tb.bottom() - 10);
    p.setBrush(c.editorBg);
    p.setPen(QPen(c.borderSubtle, 1));
    p.drawRoundedRect(body, r / 2, r / 2);

    p.setFont(QFont("Monospace", 9));
    qreal y = body.top() + 14;
    qreal x = body.left() + 10;
    qreal lh = 15;
    struct Tok {
      QString t;
      QColor c;
    };
    auto line = [&](std::initializer_list<Tok> toks) {
      qreal cx = x;
      for (const Tok &tok : toks) {
        p.setPen(tok.c);
        p.drawText(QPointF(cx, y), tok.t);
        cx += p.fontMetrics().horizontalAdvance(tok.t);
      }
      y += lh;
    };
    line({{"// lightpad > hacker mode", c.syntaxComment}});
    line({{"def ", c.syntaxKeyword},
          {"compile", c.syntaxFunction},
          {"(", c.textPrimary},
          {"path", c.textPrimary},
          {": ", c.textPrimary},
          {"str", c.syntaxType},
          {"):", c.textPrimary}});
    line({{"    ", c.textPrimary},
          {"return ", c.syntaxKeyword},
          {"\"lightpad/\" + ", c.syntaxString},
          {"path", c.textPrimary}});

    QRectF bar(body.left() + 10, y + 2, body.width() - 20, 2);
    p.setBrush(c.accentPrimary);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bar, 1, 1);

    y = bar.bottom() + 8;
    QList<QPair<QString, QColor>> sw = {
        {"bg", c.surfaceBase},     {"fg", c.textPrimary},
        {"acc", c.accentPrimary},  {"ok", c.statusSuccess},
        {"warn", c.statusWarning}, {"err", c.statusError}};
    qreal cellW = (body.width() - 20 - (sw.size() - 1) * 3) / sw.size();
    qreal cx = body.left() + 10;
    p.setFont(QFont("Monospace", 7));
    for (const auto &s : sw) {
      QRectF rc(cx, y, cellW, 16);
      p.setBrush(s.second);
      p.setPen(QPen(c.borderSubtle, 1));
      p.drawRoundedRect(rc, 2, 2);
      p.setPen(c.textMuted);
      p.drawText(rc, Qt::AlignCenter, s.first);
      cx += cellW + 3;
    }
  }

private:
  ThemeDefinition m_def;
};

class ColorSwatchButton : public QToolButton {
public:
  explicit ColorSwatchButton(QWidget *parent = nullptr) : QToolButton(parent) {}

  std::function<void()> onDoubleClick;

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton && onDoubleClick)
      onDoubleClick();
    QToolButton::mouseDoubleClickEvent(event);
  }
};
} // namespace

static const QString kSwatchStyle =
    "QToolButton { border: 1px solid %1; border-radius: 3px; min-width: 28px; "
    "max-width: 28px; min-height: 28px; max-height: 28px; }";

static const QString kSectionHeaderStyle =
    "font-family: Monospace; font-weight: 700; font-size: 13px; "
    "padding-top: 6px; letter-spacing: 1px;";

static void applySwatchColor(QToolButton *button, const QColor &borderColor,
                             const QColor &color) {
  if (!button || !color.isValid())
    return;
  button->setStyleSheet(
      kSwatchStyle.arg(borderColor.name()) +
      QString("QToolButton { background-color: %1; }").arg(color.name()));
  button->setToolTip(QObject::tr("Double-click to edit\nCurrent: %1")
                         .arg(color.name().toUpper()));
}

static const QList<ThemeColorEntry> &quickThemeTokenEntries() {
  static const QList<ThemeColorEntry> entries = {
      {"Core", "editor.background", "Editor bg",
       "Main background for code buffers."},
      {"Core", "editor.foreground", "Editor fg", "Primary code text color."},
      {"Core", "accent.primary", "Accent",
       "Focus rings, highlights, and UI emphasis."},
      {"Syntax", "syntax.keyword", "Keyword",
       "Language keywords and control flow."},
      {"Syntax", "syntax.string", "String",
       "String literals and quoted values."},
      {"Syntax", "syntax.function", "Function", "Function and method names."},
      {"Syntax", "syntax.comment", "Comment", "Comments and inline notes."},
      {"Status", "status.success", "Success",
       "Positive and completed state messaging."},
      {"Status", "status.warning", "Warning",
       "Warning and caution state messaging."},
      {"Status", "status.error", "Error", "Error and failure state messaging."},
      {"Status", "status.info", "Info",
       "Informational badges and neutral states."},
      {"Diagnostics", "diagnostic.error", "Diag error",
       "Editor diagnostics for errors."},
      {"Diagnostics", "diagnostic.warning", "Diag warn",
       "Editor diagnostics for warnings."},
      {"Diagnostics", "diagnostic.info", "Diag info",
       "Editor diagnostics for informational notices."},
      {"Diagnostics", "diagnostic.hint", "Diag hint",
       "Editor diagnostics for hints and suggestions."},
      {"Git", "git.added", "Git added",
       "Added files and added source regions."},
      {"Git", "git.modified", "Git modified",
       "Modified files and changed source regions."},
      {"Git", "git.deleted", "Git deleted",
       "Deleted files and removed source regions."},
      {"Git", "git.conflicted", "Git conflict",
       "Conflict states in Git workflows."},
      {"Tests", "test.passed", "Test pass", "Passing test results."},
      {"Tests", "test.failed", "Test fail", "Failing test results."},
      {"Tests", "test.skipped", "Test skip", "Skipped or ignored tests."},
      {"Tests", "test.running", "Test run", "Running or in-progress tests."},
      {"Debug", "debug.breakpoint", "Breakpoint",
       "Breakpoint markers and indicators."},
      {"Debug", "debug.currentLine", "Exec line",
       "Current execution line highlight."},
  };
  return entries;
}

static const QList<ThemeColorEntry> &paletteThemeColorEntries() {
  static const QList<ThemeColorEntry> entries = {
      {"Editor", "editor.background", "Editor background",
       "Main code canvas background."},
      {"Editor", "editor.foreground", "Editor foreground",
       "Primary code text color."},
      {"Editor", "editor.gutter", "Editor gutter",
       "Line number gutter and margin background."},
      {"Editor", "editor.gutterFg", "Gutter text",
       "Line numbers, blame text, and gutter foreground."},
      {"Editor", "editor.lineHighlight", "Current line",
       "Background behind the active line."},
      {"Editor", "editor.selection", "Selection",
       "Selected text background in editors."},
      {"Editor", "editor.cursor", "Cursor", "Caret color and cursor emphasis."},
      {"Editor", "editor.findMatch", "Find match",
       "Background for normal search hits."},
      {"Editor", "editor.bracketMatch", "Bracket match",
       "Highlight for matching bracket pairs."},
      {"Surfaces", "surface.base", "Base surface",
       "Main application panels and window chrome."},
      {"Surfaces", "surface.raised", "Raised surface",
       "Cards, headers, and elevated sections."},
      {"Surfaces", "surface.overlay", "Overlay surface",
       "Hover cards, menus, and overlays."},
      {"Surfaces", "text.primary", "Primary text",
       "Main text across dialogs and panels."},
      {"Surfaces", "text.muted", "Muted text",
       "Secondary labels, metadata, and hints."},
      {"Surfaces", "accent.primary", "Primary accent",
       "Focus, selection, and brand highlight color."},
      {"Syntax", "syntax.keyword", "Keyword",
       "Control-flow keywords and declarations."},
      {"Syntax", "syntax.string", "String", "String literals and paths."},
      {"Syntax", "syntax.function", "Function",
       "Functions, methods, and callable symbols."},
      {"Syntax", "syntax.comment", "Comment",
       "Comments, docs, and low-priority notes."},
      {"Syntax", "syntax.type", "Type", "Types, classes, and interfaces."},
      {"Syntax", "syntax.number", "Number", "Numbers and numeric constants."},
      {"Syntax", "syntax.operator", "Operator",
       "Operators and expression punctuation."},
      {"Status", "status.success", "Success",
       "Positive UI messages and success badges."},
      {"Status", "status.warning", "Warning", "Warnings and attention states."},
      {"Status", "status.error", "Error",
       "Errors, failures, and destructive states."},
      {"Status", "status.info", "Info",
       "Neutral informational states and notices."},
      {"Diagnostics", "diagnostic.error", "Diagnostic error",
       "Editor diagnostics for errors."},
      {"Diagnostics", "diagnostic.warning", "Diagnostic warning",
       "Editor diagnostics for warnings."},
      {"Diagnostics", "diagnostic.info", "Diagnostic info",
       "Editor diagnostics for informational notices."},
      {"Diagnostics", "diagnostic.hint", "Diagnostic hint",
       "Editor diagnostics for suggestions and hints."},
      {"Git & Diff", "git.added", "Git added", "Added files and added lines."},
      {"Git & Diff", "git.modified", "Git modified",
       "Modified files and changed regions."},
      {"Git & Diff", "git.deleted", "Git deleted",
       "Deleted files and removed lines."},
      {"Git & Diff", "git.conflicted", "Git conflicted",
       "Conflicted files and unresolved merge states."},
      {"Git & Diff", "diff.added", "Diff added",
       "Diff gutter and tooltip color for additions."},
      {"Git & Diff", "diff.modified", "Diff modified",
       "Diff gutter and tooltip color for edits."},
      {"Git & Diff", "diff.removed", "Diff removed",
       "Diff gutter and tooltip color for removals."},
      {"Git & Diff", "diff.conflict", "Diff conflict",
       "Diff gutter and tooltip color for conflicts."},
      {"Tests & Debug", "test.passed", "Test passed",
       "Passing test items and summaries."},
      {"Tests & Debug", "test.failed", "Test failed",
       "Failing test items and summaries."},
      {"Tests & Debug", "test.skipped", "Test skipped",
       "Skipped test items and summaries."},
      {"Tests & Debug", "test.running", "Test running",
       "Running tests and in-progress status."},
      {"Tests & Debug", "test.queued", "Test queued",
       "Queued tests waiting to run."},
      {"Tests & Debug", "debug.ready", "Debug ready", "Idle debugger state."},
      {"Tests & Debug", "debug.running", "Debug running",
       "Live debugger and running session state."},
      {"Tests & Debug", "debug.paused", "Debug paused",
       "Paused debugger state and attention markers."},
      {"Tests & Debug", "debug.error", "Debug error",
       "Debugger error states and failures."},
      {"Tests & Debug", "debug.breakpoint", "Breakpoint",
       "Breakpoint markers in the gutter and panels."},
      {"Tests & Debug", "debug.currentLine", "Execution line",
       "Current debug execution line highlight."},
  };
  return entries;
}

static QColor themeColorForRole(const ThemeDefinition &theme,
                                const QString &role) {
  const ThemeColors &c = theme.colors;
  if (role == "editor.background")
    return c.editorBg;
  if (role == "editor.foreground")
    return c.editorFg;
  if (role == "editor.gutter")
    return c.editorGutter;
  if (role == "editor.gutterFg")
    return c.editorGutterFg;
  if (role == "editor.lineHighlight")
    return c.editorLineHighlight;
  if (role == "editor.selection")
    return c.editorSelection;
  if (role == "editor.cursor")
    return c.editorCursor;
  if (role == "editor.findMatch")
    return c.editorFindMatch;
  if (role == "editor.bracketMatch")
    return c.editorBracketMatch;
  if (role == "surface.base")
    return c.surfaceBase;
  if (role == "surface.raised")
    return c.surfaceRaised;
  if (role == "surface.overlay")
    return c.surfaceOverlay;
  if (role == "text.primary")
    return c.textPrimary;
  if (role == "text.muted")
    return c.textMuted;
  if (role == "accent.primary")
    return c.accentPrimary;
  if (role == "syntax.keyword")
    return c.syntaxKeyword;
  if (role == "syntax.string")
    return c.syntaxString;
  if (role == "syntax.function")
    return c.syntaxFunction;
  if (role == "syntax.comment")
    return c.syntaxComment;
  if (role == "syntax.type")
    return c.syntaxType;
  if (role == "syntax.number")
    return c.syntaxNumber;
  if (role == "syntax.operator")
    return c.syntaxOperator;
  if (role == "status.success")
    return c.statusSuccess;
  if (role == "status.warning")
    return c.statusWarning;
  if (role == "status.error")
    return c.statusError;
  if (role == "status.info")
    return c.statusInfo;
  if (role == "diagnostic.error")
    return c.diagnosticError;
  if (role == "diagnostic.warning")
    return c.diagnosticWarning;
  if (role == "diagnostic.info")
    return c.diagnosticInfo;
  if (role == "diagnostic.hint")
    return c.diagnosticHint;
  if (role == "git.added")
    return c.gitAdded;
  if (role == "git.modified")
    return c.gitModified;
  if (role == "git.deleted")
    return c.gitDeleted;
  if (role == "git.conflicted")
    return c.gitConflicted;
  if (role == "diff.added")
    return c.diffAdded;
  if (role == "diff.modified")
    return c.diffModified;
  if (role == "diff.removed")
    return c.diffRemoved;
  if (role == "diff.conflict")
    return c.diffConflict;
  if (role == "test.passed")
    return c.testPassed;
  if (role == "test.failed")
    return c.testFailed;
  if (role == "test.skipped")
    return c.testSkipped;
  if (role == "test.running")
    return c.testRunning;
  if (role == "test.queued")
    return c.testQueued;
  if (role == "debug.ready")
    return c.debugReady;
  if (role == "debug.running")
    return c.debugRunning;
  if (role == "debug.paused")
    return c.debugPaused;
  if (role == "debug.error")
    return c.debugError;
  if (role == "debug.breakpoint")
    return c.debugBreakpoint;
  if (role == "debug.currentLine")
    return c.debugCurrentLine;
  return QColor();
}

static void setThemeColorForRole(ThemeDefinition &theme, const QString &role,
                                 const QColor &color) {
  ThemeColors &c = theme.colors;
  if (role == "editor.background")
    c.editorBg = color;
  else if (role == "editor.foreground")
    c.editorFg = color;
  else if (role == "editor.gutter")
    c.editorGutter = color;
  else if (role == "editor.gutterFg")
    c.editorGutterFg = color;
  else if (role == "editor.lineHighlight")
    c.editorLineHighlight = color;
  else if (role == "editor.selection")
    c.editorSelection = color;
  else if (role == "editor.cursor")
    c.editorCursor = color;
  else if (role == "editor.findMatch")
    c.editorFindMatch = color;
  else if (role == "editor.bracketMatch")
    c.editorBracketMatch = color;
  else if (role == "surface.base")
    c.surfaceBase = color;
  else if (role == "surface.raised")
    c.surfaceRaised = color;
  else if (role == "surface.overlay")
    c.surfaceOverlay = color;
  else if (role == "text.primary")
    c.textPrimary = color;
  else if (role == "text.muted")
    c.textMuted = color;
  else if (role == "accent.primary")
    c.accentPrimary = color;
  else if (role == "syntax.keyword")
    c.syntaxKeyword = color;
  else if (role == "syntax.string")
    c.syntaxString = color;
  else if (role == "syntax.function")
    c.syntaxFunction = color;
  else if (role == "syntax.comment")
    c.syntaxComment = color;
  else if (role == "syntax.type")
    c.syntaxType = color;
  else if (role == "syntax.number")
    c.syntaxNumber = color;
  else if (role == "syntax.operator")
    c.syntaxOperator = color;
  else if (role == "status.success")
    c.statusSuccess = color;
  else if (role == "status.warning")
    c.statusWarning = color;
  else if (role == "status.error")
    c.statusError = color;
  else if (role == "status.info")
    c.statusInfo = color;
  else if (role == "diagnostic.error")
    c.diagnosticError = color;
  else if (role == "diagnostic.warning")
    c.diagnosticWarning = color;
  else if (role == "diagnostic.info")
    c.diagnosticInfo = color;
  else if (role == "diagnostic.hint")
    c.diagnosticHint = color;
  else if (role == "git.added")
    c.gitAdded = color;
  else if (role == "git.modified")
    c.gitModified = color;
  else if (role == "git.deleted")
    c.gitDeleted = color;
  else if (role == "git.conflicted")
    c.gitConflicted = color;
  else if (role == "diff.added")
    c.diffAdded = color;
  else if (role == "diff.modified")
    c.diffModified = color;
  else if (role == "diff.removed")
    c.diffRemoved = color;
  else if (role == "diff.conflict")
    c.diffConflict = color;
  else if (role == "test.passed")
    c.testPassed = color;
  else if (role == "test.failed")
    c.testFailed = color;
  else if (role == "test.skipped")
    c.testSkipped = color;
  else if (role == "test.running")
    c.testRunning = color;
  else if (role == "test.queued")
    c.testQueued = color;
  else if (role == "debug.ready")
    c.debugReady = color;
  else if (role == "debug.running")
    c.debugRunning = color;
  else if (role == "debug.paused")
    c.debugPaused = color;
  else if (role == "debug.error")
    c.debugError = color;
  else if (role == "debug.breakpoint")
    c.debugBreakpoint = color;
  else if (role == "debug.currentLine")
    c.debugCurrentLine = color;
}

static void synchronizeThemeAliases(ThemeDefinition &theme, const QString &role,
                                    const QColor &color) {
  if (role == "editor.background") {
    theme.colors.surfaceBase = color;
    theme.colors.termBg = color;
  } else if (role == "editor.foreground") {
    theme.colors.textPrimary = color;
    theme.colors.termFg = color;
  } else if (role == "accent.primary") {
    theme.colors.borderFocus = color;
    theme.colors.editorCursor = color;
  }
}

namespace {
class ThemePaletteEditorDialog : public StyledDialog {
public:
  ThemePaletteEditorDialog(const ThemeDefinition &theme,
                           QWidget *parent = nullptr)
      : StyledDialog(parent), m_theme(theme) {
    setWindowTitle(QObject::tr("Palette Editor"));
    resize(760, 640);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *title = new QLabel(QObject::tr("Palette Editor"), this);
    styleTitleLabel(title);
    root->addWidget(title);

    auto *subtitle = new QLabel(
        QObject::tr("Double-click any swatch to pick a new color. Saving with "
                    "the same name overwrites a custom theme; built-in themes "
                    "are protected and will be saved as a new custom copy."),
        this);
    subtitle->setWordWrap(true);
    styleSubduedLabel(subtitle);
    root->addWidget(subtitle);

    m_preview = new ThemePreviewCard(this);
    m_preview->setDefinition(m_theme);
    root->addWidget(m_preview);

    auto *form = new QFormLayout();
    m_nameEdit = new QLineEdit(m_theme.name, this);
    m_authorEdit = new QLineEdit(m_theme.author, this);
    form->addRow(QObject::tr("Name"), m_nameEdit);
    form->addRow(QObject::tr("Author"), m_authorEdit);
    root->addLayout(form);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scrollArea);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);

    QString currentGroup;
    const QColor borderColor = m_theme.colors.borderDefault.isValid()
                                   ? m_theme.colors.borderDefault
                                   : m_theme.colors.accentPrimary;
    for (const ThemeColorEntry &entry : paletteThemeColorEntries()) {
      if (entry.group != currentGroup) {
        currentGroup = entry.group;
        auto *groupLabel = new QLabel(currentGroup, content);
        groupLabel->setStyleSheet(kSectionHeaderStyle);
        contentLayout->addWidget(groupLabel);
      }

      auto *rowWidget = new QWidget(content);
      auto *rowLayout = new QHBoxLayout(rowWidget);
      rowLayout->setContentsMargins(0, 0, 0, 0);
      rowLayout->setSpacing(10);

      auto *swatch = new ColorSwatchButton(rowWidget);
      swatch->setCursor(Qt::PointingHandCursor);
      swatch->onDoubleClick = [this, entry]() { editRole(entry.role); };
      m_swatches.insert(entry.role, swatch);
      applySwatchColor(swatch, borderColor,
                       themeColorForRole(m_theme, entry.role));
      rowLayout->addWidget(swatch, 0, Qt::AlignTop);

      auto *textWrap = new QWidget(rowWidget);
      auto *textLayout = new QVBoxLayout(textWrap);
      textLayout->setContentsMargins(0, 0, 0, 0);
      textLayout->setSpacing(2);

      auto *label = new QLabel(entry.label, textWrap);
      label->setStyleSheet("font-weight: 700;");
      textLayout->addWidget(label);

      auto *desc = new QLabel(entry.description, textWrap);
      desc->setWordWrap(true);
      desc->setStyleSheet("color: palette(mid);");
      textLayout->addWidget(desc);

      rowLayout->addWidget(textWrap, 1);
      contentLayout->addWidget(rowWidget);
    }
    contentLayout->addStretch(1);
    scrollArea->setWidget(content);
    root->addWidget(scrollArea, 1);

    auto *buttonRow = new QHBoxLayout();
    auto *cancelBtn = new QPushButton(QObject::tr("Cancel"), this);
    m_applyBtn = new QPushButton(QObject::tr("Apply Palette"), this);
    m_saveBtn = new QPushButton(QObject::tr("Save Custom Theme"), this);
    styleSecondaryButton(cancelBtn);
    styleSecondaryButton(m_applyBtn);
    stylePrimaryButton(m_saveBtn);
    buttonRow->addStretch(1);
    buttonRow->addWidget(cancelBtn);
    buttonRow->addWidget(m_applyBtn);
    buttonRow->addWidget(m_saveBtn);
    root->addLayout(buttonRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_applyBtn, &QPushButton::clicked, this, [this]() {
      commitMetadata();
      m_saveRequested = false;
      accept();
    });
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
      commitMetadata();
      m_saveRequested = true;
      accept();
    });
    connect(m_nameEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) {
              m_theme.name = text.trimmed();
              m_preview->setDefinition(m_theme);
            });
    connect(m_authorEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { m_theme.author = text.trimmed(); });

    applyTheme(ThemeEngine::instance().activeTheme());
  }

  ThemeDefinition editedTheme() const { return m_theme; }
  bool saveRequested() const { return m_saveRequested; }

private:
  void commitMetadata() {
    m_theme.name = m_nameEdit->text().trimmed();
    m_theme.author = m_authorEdit->text().trimmed();
    if (m_theme.name.isEmpty())
      m_theme.name = QObject::tr("Custom Theme");
    if (m_theme.author.isEmpty())
      m_theme.author = QObject::tr("User");
    m_theme.type = "custom";
    m_theme.normalize();
  }

  void editRole(const QString &role) {
    const QColor current = themeColorForRole(m_theme, role);
    const QColor color = QColorDialog::getColor(
        current, this, QObject::tr("Select %1").arg(role));
    if (!color.isValid())
      return;

    setThemeColorForRole(m_theme, role, color);
    synchronizeThemeAliases(m_theme, role, color);
    m_theme.normalize();
    refreshSwatches();
    m_preview->setDefinition(m_theme);
  }

  void refreshSwatches() {
    const QColor borderColor = m_theme.colors.borderDefault.isValid()
                                   ? m_theme.colors.borderDefault
                                   : m_theme.colors.accentPrimary;
    for (auto it = m_swatches.begin(); it != m_swatches.end(); ++it) {
      applySwatchColor(it.value(), borderColor,
                       themeColorForRole(m_theme, it.key()));
    }
  }

  ThemeDefinition m_theme;
  bool m_saveRequested = false;
  ThemePreviewCard *m_preview = nullptr;
  QLineEdit *m_nameEdit = nullptr;
  QLineEdit *m_authorEdit = nullptr;
  QPushButton *m_applyBtn = nullptr;
  QPushButton *m_saveBtn = nullptr;
  QMap<QString, QToolButton *> m_swatches;
};
} // namespace

Preferences::Preferences(MainWindow *parent)
    : QDialog(nullptr), m_mainWindow(parent) {
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle("Preferences");
  setMinimumSize(480, 560);
  resize(520, 640);

  buildUi();
  loadCurrentSettings();
  connectSignals();
  show();
}

Preferences::~Preferences() = default;

QWidget *Preferences::createSectionHeader(const QString &title) {
  auto *label = new QLabel(title, this);
  label->setStyleSheet(kSectionHeaderStyle);
  return label;
}

QWidget *Preferences::createSeparator() {
  auto *line = new QFrame(this);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  Theme theme = m_mainWindow->getTheme();
  line->setStyleSheet(QString("color: %1;").arg(theme.borderColor.name()));
  return line;
}

QToolButton *Preferences::createColorSwatch(const QColor &color) {
  auto *btn = new ColorSwatchButton(this);
  Theme theme = m_mainWindow->getTheme();
  applySwatchColor(btn, theme.borderColor, color);
  btn->setCursor(Qt::PointingHandCursor);
  return btn;
}

void Preferences::buildUi() {
  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 12);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  auto *scrollWidget = new QWidget();
  auto *mainLayout = new QVBoxLayout(scrollWidget);
  mainLayout->setContentsMargins(20, 16, 20, 16);
  mainLayout->setSpacing(8);

  mainLayout->addWidget(createSectionHeader("◆ THEME"));
  mainLayout->addWidget(createSeparator());
  mainLayout->addWidget(buildThemeSection());
  {
    auto *fxGrid = new QGridLayout();
    fxGrid->setHorizontalSpacing(12);
    fxGrid->setVerticalSpacing(8);

    auto *glowLabel = new QLabel("Glow", this);
    m_glowCombo = new QComboBox(this);
    m_glowCombo->addItem("Off", 0.0);
    m_glowCombo->addItem("Low", 0.12);
    m_glowCombo->addItem("Medium", 0.28);
    m_glowCombo->addItem("High", 0.5);
    connect(m_glowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Preferences::onGlowPresetChanged);
    fxGrid->addWidget(glowLabel, 0, 0);
    fxGrid->addWidget(m_glowCombo, 0, 1);

    auto *transparencyLabel = new QLabel("Transparency", this);
    m_transparencyCombo = new QComboBox(this);
    m_transparencyCombo->addItem("Off", 1.0);
    m_transparencyCombo->addItem("Low", 0.94);
    m_transparencyCombo->addItem("Medium", 0.86);
    connect(m_transparencyCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Preferences::onTransparencyPresetChanged);
    fxGrid->addWidget(transparencyLabel, 0, 2);
    fxGrid->addWidget(m_transparencyCombo, 0, 3);

    m_scanlinesCheck = new QCheckBox("CRT scanline overlay", this);
    m_scanlinesCheck->setChecked(
        ThemeEngine::instance().activeTheme().ui.scanlineEffect);
    connect(m_scanlinesCheck, &QCheckBox::toggled, this,
            &Preferences::onScanlinesToggled);
    fxGrid->addWidget(m_scanlinesCheck, 1, 0, 1, 2);

    m_panelBordersCheck = new QCheckBox("Subtle panel borders", this);
    m_panelBordersCheck->setChecked(
        ThemeEngine::instance().activeTheme().ui.panelBorders);
    connect(m_panelBordersCheck, &QCheckBox::toggled, this,
            &Preferences::onPanelBordersToggled);
    fxGrid->addWidget(m_panelBordersCheck, 1, 2, 1, 2);
    fxGrid->setColumnStretch(4, 1);
    mainLayout->addLayout(fxGrid);
  }
  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ FONT"));
  mainLayout->addWidget(createSeparator());

  auto *fontRow = new QHBoxLayout();
  fontRow->setSpacing(10);

  auto *familyLabel = new QLabel("Family", this);
  m_fontFamilyCombo = new QFontComboBox(this);
  m_fontFamilyCombo->setFontFilters(QFontComboBox::MonospacedFonts);
  m_fontFamilyCombo->setMinimumWidth(200);

  auto *sizeLabel = new QLabel("Size", this);
  m_fontSizeSpinner = new QSpinBox(this);
  m_fontSizeSpinner->setRange(6, 72);
  m_fontSizeSpinner->setSuffix(" pt");

  fontRow->addWidget(familyLabel);
  fontRow->addWidget(m_fontFamilyCombo, 1);
  fontRow->addSpacing(12);
  fontRow->addWidget(sizeLabel);
  fontRow->addWidget(m_fontSizeSpinner);
  mainLayout->addLayout(fontRow);

  m_fontPreview = new QLabel(this);
  m_fontPreview->setWordWrap(true);
  Theme theme = m_mainWindow->getTheme();
  m_fontPreview->setStyleSheet(
      QString(
          "QLabel { border: 1px solid %1; border-radius: 4px; padding: 10px; "
          "background-color: %2; }")
          .arg(theme.borderColor.name(), theme.backgroundColor.name()));
  m_fontPreview->setMinimumHeight(56);
  m_fontPreview->setText("The quick brown fox jumps over the lazy dog.\n"
                         "0123456789 (){}[] <> :: -> => != == += -= */");
  mainLayout->addWidget(m_fontPreview);

  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ EDITOR"));
  mainLayout->addWidget(createSeparator());

  auto *tabRow = new QHBoxLayout();
  auto *tabLabel = new QLabel("Tab width", this);
  m_tabWidthCombo = new QComboBox(this);
  m_tabWidthCombo->addItems({"2", "4", "8"});
  m_tabWidthCombo->setFixedWidth(70);
  tabRow->addWidget(tabLabel);
  tabRow->addWidget(m_tabWidthCombo);
  tabRow->addStretch();
  mainLayout->addLayout(tabRow);

  m_autoIndentCheck = new QCheckBox("Enable automatic indentation", this);
  mainLayout->addWidget(m_autoIndentCheck);

  m_autoSaveCheck = new QCheckBox("Auto-save modified files", this);
  mainLayout->addWidget(m_autoSaveCheck);

  m_trimWhitespaceCheck =
      new QCheckBox("Trim trailing whitespace on save", this);
  mainLayout->addWidget(m_trimWhitespaceCheck);

  m_finalNewlineCheck = new QCheckBox("Insert final newline on save", this);
  mainLayout->addWidget(m_finalNewlineCheck);

  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ DISPLAY"));
  mainLayout->addWidget(createSeparator());

  m_lineNumbersCheck = new QCheckBox("Show line numbers", this);
  mainLayout->addWidget(m_lineNumbersCheck);

  m_currentLineCheck = new QCheckBox("Highlight current line", this);
  mainLayout->addWidget(m_currentLineCheck);

  m_bracketMatchCheck = new QCheckBox("Highlight matching brackets", this);
  mainLayout->addWidget(m_bracketMatchCheck);

  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ THEME TOKENS"));
  mainLayout->addWidget(createSeparator());

  auto *colorGrid = new QGridLayout();
  colorGrid->setHorizontalSpacing(16);
  colorGrid->setVerticalSpacing(8);

  int row = 0, col = 0;
  for (const ThemeColorEntry &entry : quickThemeTokenEntries()) {
    auto *swatchBtn = createColorSwatch(QColor("#000"));
    m_colorSwatches[entry.role] = swatchBtn;

    auto *lbl = new QLabel(entry.label, this);
    lbl->setFixedWidth(80);

    auto *cellLayout = new QHBoxLayout();
    cellLayout->setSpacing(6);
    cellLayout->addWidget(lbl);
    cellLayout->addWidget(swatchBtn);
    cellLayout->addStretch();

    colorGrid->addLayout(cellLayout, row, col);
    col++;
    if (col >= 2) {
      col = 0;
      row++;
    }
  }
  mainLayout->addLayout(colorGrid);

  mainLayout->addStretch();

  scrollArea->setWidget(scrollWidget);
  outerLayout->addWidget(scrollArea);

  auto *buttonRow = new QHBoxLayout();
  buttonRow->setContentsMargins(20, 0, 20, 0);
  buttonRow->addStretch();
  auto *closeBtn = new QToolButton(this);
  closeBtn->setText("Close");
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setStyleSheet(
      QString("QToolButton { border: 1px solid %1; border-radius: 4px; "
              "padding: 4px 16px; }"
              "QToolButton:hover { background-color: %2; }")
          .arg(theme.borderColor.name(), theme.surfaceAltColor.name()));
  connect(closeBtn, &QToolButton::clicked, this, &QDialog::close);
  buttonRow->addWidget(closeBtn);
  outerLayout->addLayout(buttonRow);
}

void Preferences::loadCurrentSettings() {
  if (!m_mainWindow)
    return;

  auto textSettings = m_mainWindow->getSettings();
  auto theme = m_mainWindow->getTheme();
  auto &sm = SettingsManager::instance();

  m_fontFamilyCombo->setCurrentFont(textSettings.mainFont);
  m_fontSizeSpinner->setValue(textSettings.mainFont.pointSize());
  QFont previewFont = textSettings.mainFont;
  previewFont.setPointSize(textSettings.mainFont.pointSize());
  m_fontPreview->setFont(previewFont);

  int tabWidth = textSettings.tabWidth;
  int idx = m_tabWidthCombo->findText(QString::number(tabWidth));
  if (idx >= 0)
    m_tabWidthCombo->setCurrentIndex(idx);

  m_autoIndentCheck->setChecked(textSettings.autoIndent);
  m_autoSaveCheck->setChecked(sm.getValue("autoSaveFiles", true).toBool());
  m_trimWhitespaceCheck->setChecked(
      sm.getValue("trimTrailingWhitespace", false).toBool());
  m_finalNewlineCheck->setChecked(
      sm.getValue("insertFinalNewline", false).toBool());

  m_lineNumbersCheck->setChecked(textSettings.showLineNumberArea);
  m_currentLineCheck->setChecked(textSettings.lineHighlighted);
  m_bracketMatchCheck->setChecked(textSettings.matchingBracketsHighlighted);

  const ThemeDefinition &activeTheme = ThemeEngine::instance().activeTheme();
  if (m_glowCombo) {
    const qreal glow = activeTheme.ui.glowIntensity;
    int best = 0;
    qreal bestDistance = 10.0;
    for (int i = 0; i < m_glowCombo->count(); ++i) {
      qreal distance = qAbs(m_glowCombo->itemData(i).toDouble() - glow);
      if (distance < bestDistance) {
        best = i;
        bestDistance = distance;
      }
    }
    m_glowCombo->setCurrentIndex(best);
  }
  if (m_transparencyCombo) {
    const qreal opacity = activeTheme.ui.chromeOpacity;
    int best = 0;
    qreal bestDistance = 10.0;
    for (int i = 0; i < m_transparencyCombo->count(); ++i) {
      qreal distance =
          qAbs(m_transparencyCombo->itemData(i).toDouble() - opacity);
      if (distance < bestDistance) {
        best = i;
        bestDistance = distance;
      }
    }
    m_transparencyCombo->setCurrentIndex(best);
  }
  if (m_scanlinesCheck)
    m_scanlinesCheck->setChecked(activeTheme.ui.scanlineEffect);
  if (m_panelBordersCheck)
    m_panelBordersCheck->setChecked(activeTheme.ui.panelBorders);
  Q_UNUSED(theme);
  m_editTheme = activeTheme;
  syncThemeControls(m_editTheme);
}

void Preferences::connectSignals() {
  if (!m_mainWindow)
    return;

  connect(m_fontFamilyCombo, &QFontComboBox::currentFontChanged, this,
          &Preferences::onFontFamilyChanged);

  connect(m_fontSizeSpinner, &QSpinBox::valueChanged, this,
          &Preferences::onFontSizeChanged);

  connect(m_tabWidthCombo, &QComboBox::currentTextChanged, this,
          [this](const QString &text) {
            bool ok;
            int width = text.toInt(&ok);
            if (ok && m_mainWindow) {
              m_mainWindow->setTabWidth(width);
              persistAll();
            }
          });

  connect(m_autoIndentCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      SettingsManager::instance().setValue("autoIndent", checked);
      persistAll();
    }
  });

  connect(m_autoSaveCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow)
      m_mainWindow->setAutoSaveEnabled(checked);
  });

  connect(
      m_trimWhitespaceCheck, &QCheckBox::toggled, this, [this](bool checked) {
        SettingsManager::instance().setValue("trimTrailingWhitespace", checked);
        SettingsManager::instance().saveSettings();
      });

  connect(m_finalNewlineCheck, &QCheckBox::toggled, this, [this](bool checked) {
    SettingsManager::instance().setValue("insertFinalNewline", checked);
    SettingsManager::instance().saveSettings();
  });

  connect(m_lineNumbersCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      m_mainWindow->showLineNumbers(checked);
      persistAll();
    }
  });

  connect(m_currentLineCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      m_mainWindow->highlihtCurrentLine(checked);
      persistAll();
    }
  });

  connect(m_bracketMatchCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      m_mainWindow->highlihtMatchingBracket(checked);
      persistAll();
    }
  });

  for (auto it = m_colorSwatches.begin(); it != m_colorSwatches.end(); ++it) {
    const QString role = it.key();
    QToolButton *btn = it.value();
    connect(btn, &QToolButton::clicked, this,
            [this, btn, role]() { onColorSwatchClicked(btn, role); });
  }

  if (m_themeNameEdit) {
    connect(m_themeNameEdit, &QLineEdit::textChanged, this,
            &Preferences::onThemeMetadataChanged);
  }
  if (m_themeAuthorEdit) {
    connect(m_themeAuthorEdit, &QLineEdit::textChanged, this,
            &Preferences::onThemeMetadataChanged);
  }
  if (m_generateThemeButton) {
    connect(m_generateThemeButton, &QPushButton::clicked, this,
            &Preferences::onGenerateThemeClicked);
  }
  if (m_saveThemeButton) {
    connect(m_saveThemeButton, &QPushButton::clicked, this,
            &Preferences::onSaveThemeClicked);
  }
  if (m_deleteThemeButton) {
    connect(m_deleteThemeButton, &QPushButton::clicked, this,
            &Preferences::onDeleteThemeClicked);
  }
}

void Preferences::onFontFamilyChanged(const QFont &font) {
  if (!m_mainWindow)
    return;

  QFont newFont = m_mainWindow->getFont();
  newFont.setFamily(font.family());
  m_mainWindow->setFont(newFont);
  m_fontPreview->setFont(newFont);
  persistAll();
}

void Preferences::onFontSizeChanged(int size) {
  if (!m_mainWindow)
    return;

  QFont newFont = m_mainWindow->getFont();
  newFont.setPointSize(size);
  m_mainWindow->setFont(newFont);
  m_fontPreview->setFont(newFont);
  persistAll();
}

void Preferences::onColorSwatchClicked(QToolButton *button,
                                       const QString &role) {
  QColor current;
  current.setNamedColor(themeColorForRole(m_editTheme, role).name());
  QColor color = QColorDialog::getColor(current, this, "Select Color");
  if (!color.isValid() || !m_mainWindow)
    return;

  applySwatchColor(button, m_mainWindow->getTheme().borderColor, color);

  setThemeColorForRole(m_editTheme, role, color);
  synchronizeThemeAliases(m_editTheme, role, color);
  m_editTheme.normalize();

  applyEditedTheme(QString());
  persistAll();
}

void Preferences::persistAll() {
  if (!m_mainWindow)
    return;

  auto &sm = SettingsManager::instance();
  QFont currentFont = m_mainWindow->getFont();

  sm.setValue("fontFamily", currentFont.family());
  sm.setValue("fontSize", currentFont.pointSize());
  sm.setValue("fontWeight", currentFont.weight());
  sm.setValue("fontItalic", currentFont.italic());

  sm.saveSettings();
}

void Preferences::persistCurrentTheme(const QString &activeThemeName) {
  if (!m_mainWindow)
    return;

  auto &sm = SettingsManager::instance();
  QJsonObject themeJson;
  m_mainWindow->getTheme().write(themeJson);
  QJsonObject themeDefinitionJson;
  ThemeEngine::instance().activeTheme().write(themeDefinitionJson);
  sm.setValue("theme", themeJson);
  sm.setValue("activeThemeDefinition", themeDefinitionJson);
  sm.setValue("activeThemeName", activeThemeName);
  sm.saveSettings();
}

void Preferences::refreshThemeList(const QString &selectedName) {
  if (!m_themeList || !m_themeBaseCombo)
    return;

  const QString currentName =
      selectedName.isEmpty() ? m_editTheme.name : selectedName;
  const QSignalBlocker listBlocker(m_themeList);
  const QSignalBlocker comboBlocker(m_themeBaseCombo);

  m_themeList->clear();
  m_themeBaseCombo->clear();

  QStringList names = ThemeEngine::instance().availableThemes();
  names.sort();
  for (const QString &name : names) {
    ThemeDefinition d = ThemeEngine::instance().themeByName(name);
    QPixmap px(14, 14);
    px.fill(d.colors.accentPrimary);
    auto *item = new QListWidgetItem(QIcon(px), name);
    m_themeList->addItem(item);
    m_themeBaseCombo->addItem(name);
    if (name == currentName)
      m_themeList->setCurrentItem(item);
  }

  if (!currentName.isEmpty()) {
    const int idx = m_themeBaseCombo->findText(currentName);
    if (idx >= 0)
      m_themeBaseCombo->setCurrentIndex(idx);
  }
}

void Preferences::syncThemeControls(const ThemeDefinition &theme) {
  m_updatingThemeUi = true;
  m_editTheme = theme;
  refreshThemeSwatches();
  if (m_themePreview)
    static_cast<ThemePreviewCard *>(m_themePreview)->setDefinition(theme);
  if (m_themeNameEdit)
    m_themeNameEdit->setText(theme.name);
  if (m_themeAuthorEdit)
    m_themeAuthorEdit->setText(theme.author);
  if (m_themeBaseCombo) {
    int idx = m_themeBaseCombo->findText(theme.name);
    if (idx < 0)
      idx = 0;
    if (idx >= 0)
      m_themeBaseCombo->setCurrentIndex(idx);
  }
  if (m_deleteThemeButton)
    m_deleteThemeButton->setVisible(
        !theme.name.isEmpty() && ThemeEngine::instance().hasTheme(theme.name) &&
        !ThemeEngine::instance().isBuiltinTheme(theme.name));
  m_updatingThemeUi = false;
}

void Preferences::refreshThemeSwatches() {
  const Theme borderTheme = m_mainWindow ? m_mainWindow->getTheme() : Theme();
  for (auto it = m_colorSwatches.begin(); it != m_colorSwatches.end(); ++it) {
    const QColor color = themeColorForRole(m_editTheme, it.key());
    if (!color.isValid())
      continue;
    applySwatchColor(it.value(), borderTheme.borderColor, color);
  }
}

void Preferences::applyEditedTheme(const QString &activeThemeName) {
  if (!m_mainWindow)
    return;

  ThemeDefinition applied = m_editTheme;
  applied.author =
      m_themeAuthorEdit ? m_themeAuthorEdit->text().trimmed() : applied.author;
  applied.name =
      m_themeNameEdit ? m_themeNameEdit->text().trimmed() : applied.name;
  if (applied.author.isEmpty())
    applied.author = tr("User");
  if (applied.name.isEmpty())
    applied.name = QStringLiteral("Custom");
  applied.normalize();

  m_mainWindow->setTheme(applied);
  persistCurrentTheme(activeThemeName);
  if (m_themePreview)
    static_cast<ThemePreviewCard *>(m_themePreview)->setDefinition(m_editTheme);
  refreshThemeSwatches();
}

QWidget *Preferences::buildThemeSection() {
  auto *wrap = new QWidget(this);
  auto *row = new QHBoxLayout(wrap);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(10);

  m_themeList = new QListWidget(wrap);
  m_themeList->setMinimumWidth(180);
  m_themeList->setMaximumWidth(220);
  m_themeList->setMinimumHeight(190);

  row->addWidget(m_themeList);

  auto *rightWrap = new QWidget(wrap);
  auto *rightLayout = new QVBoxLayout(rightWrap);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(8);

  m_themePreview = new ThemePreviewCard(rightWrap);
  rightLayout->addWidget(m_themePreview);

  auto *form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setHorizontalSpacing(8);
  form->setVerticalSpacing(6);
  m_themeNameEdit = new QLineEdit(rightWrap);
  m_themeAuthorEdit = new QLineEdit(rightWrap);
  m_themeBaseCombo = new QComboBox(rightWrap);
  form->addRow(tr("Name"), m_themeNameEdit);
  form->addRow(tr("Author"), m_themeAuthorEdit);
  form->addRow(tr("Generate from"), m_themeBaseCombo);
  rightLayout->addLayout(form);

  auto *buttonRow = new QHBoxLayout();
  buttonRow->setContentsMargins(0, 0, 0, 0);
  m_generateThemeButton = new QPushButton(tr("Generate Palette"), rightWrap);
  m_saveThemeButton = new QPushButton(tr("Save Custom Theme"), rightWrap);
  m_deleteThemeButton = new QPushButton(tr("Delete Custom Theme"), rightWrap);
  buttonRow->addWidget(m_generateThemeButton);
  buttonRow->addWidget(m_saveThemeButton);
  buttonRow->addWidget(m_deleteThemeButton);
  m_deleteThemeButton->hide();
  rightLayout->addLayout(buttonRow);

  auto *saveHint = new QLabel(
      tr("Generate Palette opens the full palette editor. Saving with the same "
         "name overwrites a custom theme; built-in themes are always kept and "
         "saved as new custom copies instead. Only custom themes can be "
         "deleted."),
      rightWrap);
  saveHint->setWordWrap(true);
  saveHint->setStyleSheet("color: palette(mid);");
  rightLayout->addWidget(saveHint);

  row->addWidget(rightWrap, 1);
  refreshThemeList(ThemeEngine::instance().activeTheme().name);

  connect(m_themeList, &QListWidget::currentRowChanged, this,
          &Preferences::onThemePresetChanged);

  if (auto *cur = m_themeList->currentItem()) {
    ThemeDefinition d = ThemeEngine::instance().themeByName(cur->text());
    syncThemeControls(d);
  }
  return wrap;
}

void Preferences::onScanlinesToggled(bool enabled) {
  m_editTheme.ui.scanlineEffect = enabled;
  applyThemeEffects();
}

void Preferences::onGlowPresetChanged(int index) {
  if (!m_glowCombo || index < 0)
    return;
  m_editTheme.ui.glowIntensity = m_glowCombo->itemData(index).toDouble();
  applyThemeEffects();
}

void Preferences::onPanelBordersToggled(bool enabled) {
  m_editTheme.ui.panelBorders = enabled;
  applyThemeEffects();
}

void Preferences::onTransparencyPresetChanged(int index) {
  if (!m_transparencyCombo || index < 0)
    return;
  m_editTheme.ui.chromeOpacity =
      m_transparencyCombo->itemData(index).toDouble();
  applyThemeEffects();
}

void Preferences::onGenerateThemeClicked() {
  ThemeDefinition initial = m_editTheme;
  if (m_themeBaseCombo) {
    const QString baseName = m_themeBaseCombo->currentText();
    if (!baseName.isEmpty() && ThemeEngine::instance().hasTheme(baseName) &&
        baseName != m_editTheme.name) {
      initial = ThemeEngine::instance().themeByName(baseName);
    }
  }

  if (m_themeNameEdit && !m_themeNameEdit->text().trimmed().isEmpty())
    initial.name = m_themeNameEdit->text().trimmed();
  if (m_themeAuthorEdit && !m_themeAuthorEdit->text().trimmed().isEmpty())
    initial.author = m_themeAuthorEdit->text().trimmed();
  if (ThemeEngine::instance().isBuiltinTheme(initial.name))
    initial.name = tr("%1 Custom").arg(initial.name);

  ThemePaletteEditorDialog dialog(initial, this);
  if (dialog.exec() != QDialog::Accepted)
    return;

  m_editTheme = dialog.editedTheme();
  syncThemeControls(m_editTheme);
  if (dialog.saveRequested()) {
    onSaveThemeClicked();
  } else {
    applyEditedTheme(QString());
  }
}

void Preferences::onSaveThemeClicked() {
  ThemeDefinition saved = m_editTheme;
  const QString requestedName =
      m_themeNameEdit ? m_themeNameEdit->text().trimmed() : saved.name;
  saved.name = m_themeNameEdit ? m_themeNameEdit->text().trimmed() : saved.name;
  saved.author =
      m_themeAuthorEdit ? m_themeAuthorEdit->text().trimmed() : saved.author;
  if (saved.name.isEmpty())
    saved.name = tr("Custom Theme");
  if (saved.author.isEmpty())
    saved.author = tr("User");
  saved.type = "custom";
  saved.normalize();

  saved = ThemeEngine::instance().saveUserTheme(saved);
  m_editTheme = saved;
  m_mainWindow->setTheme(saved);
  persistCurrentTheme(saved.name);
  refreshThemeList(saved.name);
  syncThemeControls(saved);
  if (!requestedName.isEmpty() && requestedName != saved.name) {
    QMessageBox::information(
        this, tr("Saved as custom copy"),
        tr("Built-in themes are protected. Your palette was saved as \"%1\".")
            .arg(saved.name));
  }
}

void Preferences::onDeleteThemeClicked() {
  if (!m_themeList)
    return;

  auto *item = m_themeList->currentItem();
  if (!item)
    return;

  const QString name = item->text().trimmed();
  if (name.isEmpty() || ThemeEngine::instance().isBuiltinTheme(name)) {
    QMessageBox::information(this, tr("Built-in theme"),
                             tr("Built-in themes cannot be deleted. Only "
                                "custom themes can be removed."));
    return;
  }

  const auto answer = QMessageBox::warning(
      this, tr("Delete custom theme"),
      tr("Delete the custom theme \"%1\"? This cannot be undone.").arg(name),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
  if (answer != QMessageBox::Yes)
    return;

  const bool wasActive = ThemeEngine::instance().activeTheme().name == name;
  if (!ThemeEngine::instance().deleteUserTheme(name)) {
    QMessageBox::warning(this, tr("Delete failed"),
                         tr("Could not delete the selected custom theme."));
    return;
  }

  const ThemeDefinition fallback = ThemeEngine::instance().activeTheme();
  if (wasActive && m_mainWindow)
    m_mainWindow->setTheme(fallback);
  persistCurrentTheme(fallback.name);
  refreshThemeList(fallback.name);
  syncThemeControls(fallback);
}

void Preferences::onThemeMetadataChanged() {
  if (m_updatingThemeUi)
    return;
  if (m_themeNameEdit)
    m_editTheme.name = m_themeNameEdit->text().trimmed();
  if (m_themeAuthorEdit)
    m_editTheme.author = m_themeAuthorEdit->text().trimmed();
  if (m_themePreview)
    static_cast<ThemePreviewCard *>(m_themePreview)->setDefinition(m_editTheme);
}

void Preferences::applyThemeEffects() {
  if (!m_mainWindow)
    return;

  applyEditedTheme(QString());
}

void Preferences::onThemePresetChanged(int row) {
  auto *item = m_themeList->item(row);
  if (!item || !m_mainWindow)
    return;
  QString name = item->text();
  ThemeDefinition d = ThemeEngine::instance().themeByName(name);
  if (m_glowCombo) {
    int idx = m_glowCombo->findData(d.ui.glowIntensity);
    if (idx < 0)
      idx = d.ui.glowIntensity <= 0.01 ? 0 : (d.ui.glowIntensity < 0.2 ? 1 : 2);
    m_glowCombo->blockSignals(true);
    m_glowCombo->setCurrentIndex(idx);
    m_glowCombo->blockSignals(false);
  }
  if (m_transparencyCombo) {
    int idx = m_transparencyCombo->findData(d.ui.chromeOpacity);
    if (idx < 0)
      idx = d.ui.chromeOpacity > 0.98 ? 0 : (d.ui.chromeOpacity > 0.9 ? 1 : 2);
    m_transparencyCombo->blockSignals(true);
    m_transparencyCombo->setCurrentIndex(idx);
    m_transparencyCombo->blockSignals(false);
  }
  if (m_scanlinesCheck) {
    m_scanlinesCheck->blockSignals(true);
    m_scanlinesCheck->setChecked(d.ui.scanlineEffect);
    m_scanlinesCheck->blockSignals(false);
  }
  if (m_panelBordersCheck) {
    m_panelBordersCheck->blockSignals(true);
    m_panelBordersCheck->setChecked(d.ui.panelBorders);
    m_panelBordersCheck->blockSignals(false);
  }
  m_editTheme = d;
  syncThemeControls(d);
  m_mainWindow->setTheme(d);
  persistCurrentTheme(name);
}
