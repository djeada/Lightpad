#include "debugpanel.h"

#include "../../core/logging/logger.h"
#include "../../dap/debugadapterregistry.h"
#include "debugglyphs.h"

#include "../uimetrics.h"
#include "../uistylehelper.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>

namespace {
constexpr int MAX_DEBUG_CONSOLE_BLOCKS = 2000;
constexpr int MAX_DEBUG_CONSOLE_ENTRY_CHARS = 8192;
constexpr int MAX_EAGER_SCOPE_LOADS = 1;
constexpr int MAX_STACK_FRAMES_PER_REFRESH = 64;

QString fillStyleTemplate(
    QString style,
    std::initializer_list<std::pair<const char *, QString>> replacements) {
  for (const auto &[key, value] : replacements) {
    style.replace(QString("{%1}").arg(QString::fromLatin1(key)), value);
  }
  return style;
}

class DebugTreeDelegate : public QStyledItemDelegate {
public:
  // Horizontal room paint() takes away from the item rect for the row
  // background and its inner text inset. sizeHint() has to add it back or
  // ResizeToContents columns come out too narrow and elide their content.
  static constexpr int kRowInset = 3;
  static constexpr int kTextInsetLeft = 10;
  static constexpr int kTextInsetRight = 8;
  static constexpr int kHorizontalPadding =
      2 * kRowInset + kTextInsetLeft + kTextInsetRight;

  explicit DebugTreeDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void setTheme(const Theme &theme) {
    m_textColor = theme.foregroundColor;
    m_hoverBackground =
        QColor::fromRgbF(theme.hoverColor.redF(), theme.hoverColor.greenF(),
                         theme.hoverColor.blueF(), 0.2);
    m_selectedBackground = QColor::fromRgbF(
        theme.accentSoftColor.redF(), theme.accentSoftColor.greenF(),
        theme.accentSoftColor.blueF(), 0.18);
    m_selectedBorder = theme.accentColor.lighter(102);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter) {
      return;
    }

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool isSelected = opt.state.testFlag(QStyle::State_Selected);
    const bool isHovered = opt.state.testFlag(QStyle::State_MouseOver);

    QRect rowRect = opt.rect.adjusted(kRowInset, 1, -kRowInset, -1);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    if (isSelected || isHovered) {
      const QColor bg = isSelected ? m_selectedBackground : m_hoverBackground;
      const QColor border = isSelected ? m_selectedBorder : bg.lighter(110);
      painter->setPen(Qt::NoPen);
      painter->setBrush(bg);
      painter->drawRect(rowRect);

      if (isSelected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_selectedBorder);
        painter->drawRect(
            QRect(rowRect.left(), rowRect.top(), 2, rowRect.height()));
      }
    }

    opt.rect = rowRect.adjusted(kTextInsetLeft, 0, -kTextInsetRight, 0);
    opt.state &= ~QStyle::State_HasFocus;
    opt.showDecorationSelected = false;
    opt.palette.setColor(QPalette::Highlight, Qt::transparent);
    opt.palette.setColor(QPalette::HighlightedText, m_textColor);
    opt.palette.setColor(QPalette::Text, m_textColor);

    QStyledItemDelegate::paint(painter, opt, index);
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), 20));
    return size + QSize(kHorizontalPadding, 1);
  }

private:
  QColor m_textColor = QColor("#dce4ee");
  QColor m_hoverBackground = QColor("#171e27");
  QColor m_selectedBackground = QColor("#17283d");
  QColor m_selectedBorder = QColor("#5da7ff");
};

void applyTreePalette(QTreeWidget *tree, const Theme &theme) {
  if (!tree) {
    return;
  }

  QPalette palette = tree->palette();
  palette.setColor(QPalette::Base, theme.backgroundColor);
  palette.setColor(QPalette::AlternateBase, theme.surfaceAltColor);
  palette.setColor(QPalette::Text, theme.foregroundColor);
  palette.setColor(QPalette::Highlight, theme.accentSoftColor);
  palette.setColor(QPalette::HighlightedText, theme.foregroundColor);
  tree->setPalette(palette);
}

QList<QPair<QString, QString>> parseInfoLocalsOutput(const QString &raw) {
  QList<QPair<QString, QString>> entries;
  QString currentName;
  QString currentValue;

  const auto flush = [&]() {
    if (!currentName.isEmpty()) {
      entries.append({currentName, currentValue.trimmed()});
      currentName.clear();
      currentValue.clear();
    }
  };

  const QStringList lines = raw.split('\n');
  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }

    const int eqPos = line.indexOf('=');
    if (eqPos > 0) {
      const QString candidateName = line.left(eqPos).trimmed();
      if (!candidateName.isEmpty() && !candidateName.contains(' ')) {
        flush();
        currentName = candidateName;
        currentValue = line.mid(eqPos + 1).trimmed();
        continue;
      }
    }

    if (!currentName.isEmpty()) {
      if (!currentValue.isEmpty()) {
        currentValue += " ";
      }
      currentValue += trimmed;
    }
  }

  flush();
  return entries;
}
} // namespace

DebugPanel::DebugPanel(QWidget *parent)
    : QWidget(parent), m_dapClient(nullptr), m_currentThreadId(0),
      m_currentFrameId(0), m_programmaticVariablesExpand(false),
      m_variablesNameColumnUserSized(false),
      m_variablesNameColumnAutofitting(false), m_stepInProgress(false),
      m_expectStopEvent(true), m_hasLastStopEvent(false),
      m_lastStoppedThreadId(0), m_lastStoppedReason(DapStoppedReason::Unknown),
      m_localsFallbackPending(false), m_localsFallbackFrameId(-1),
      m_localsFallbackScopeRef(0), m_localsFallbackRequestSeq(0),
      m_themeInitialized(false) {
  setupUI();
  updateToolbarState();

  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointAdded,
          this, &DebugPanel::refreshBreakpointList);
  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointRemoved,
          this, [this](int, const QString &, int) { refreshBreakpointList(); });
  connect(&BreakpointManager::instance(), &BreakpointManager::breakpointChanged,
          this, &DebugPanel::refreshBreakpointList);
  connect(&BreakpointManager::instance(),
          &BreakpointManager::functionBreakpointsChanged, this,
          &DebugPanel::refreshBreakpointList);
  connect(&BreakpointManager::instance(),
          &BreakpointManager::allBreakpointsCleared, this,
          &DebugPanel::refreshBreakpointList);
  connect(&BreakpointManager::instance(),
          &BreakpointManager::dataBreakpointsChanged, this,
          &DebugPanel::refreshBreakpointList);
  connect(&BreakpointManager::instance(),
          &BreakpointManager::exceptionBreakpointsChanged, this,
          &DebugPanel::refreshBreakpointList);

  refreshBreakpointList();

  connect(&WatchManager::instance(), &WatchManager::watchAdded, this,
          &DebugPanel::onWatchAdded);
  connect(&WatchManager::instance(), &WatchManager::watchRemoved, this,
          &DebugPanel::onWatchRemoved);
  connect(&WatchManager::instance(), &WatchManager::watchUpdated, this,
          &DebugPanel::onWatchUpdated);
  connect(&WatchManager::instance(), &WatchManager::watchChildrenReceived, this,
          &DebugPanel::onWatchChildrenReceived);

  for (const WatchExpression &w : WatchManager::instance().allWatches()) {
    onWatchAdded(w);
  }
}

DebugPanel::~DebugPanel() {}

void DebugPanel::applyTheme(const Theme &theme) {
  m_theme = theme;
  m_themeInitialized = true;

  // The transport glyphs are painted in palette colours, so they have to be
  // redrawn whenever the palette changes.
  applyTransportIcons();

  const auto blend = [](const QColor &base, const QColor &overlay,
                        qreal ratio) {
    const qreal clamped = qBound(0.0, ratio, 1.0);
    return QColor::fromRgbF(
        base.redF() * (1.0 - clamped) + overlay.redF() * clamped,
        base.greenF() * (1.0 - clamped) + overlay.greenF() * clamped,
        base.blueF() * (1.0 - clamped) + overlay.blueF() * clamped, 1.0);
  };
  const auto withAlpha = [](QColor color, int alpha) {
    color.setAlpha(alpha);
    return color;
  };

  QColor panelSurface = blend(theme.backgroundColor, theme.surfaceColor, 0.08);
  QColor toolbarShell = blend(theme.backgroundColor, theme.surfaceColor, 0.14);
  QColor shellSurface = blend(theme.backgroundColor, theme.surfaceColor, 0.12);
  QColor cardSurface = blend(theme.backgroundColor, theme.surfaceColor, 0.08);
  QColor recessedSurface =
      blend(theme.backgroundColor, theme.surfaceColor, 0.1);
  QColor inputSurface = blend(theme.backgroundColor, theme.surfaceColor, 0.08);
  QColor focusSurface =
      blend(theme.backgroundColor, theme.accentSoftColor, 0.06);
  QColor mutedText = blend(theme.foregroundColor, theme.backgroundColor, 0.24);
  QColor subtleText = blend(theme.foregroundColor, theme.backgroundColor, 0.34);
  QColor consoleSurface =
      blend(theme.backgroundColor, theme.surfaceColor, 0.08);
  QColor readyBg = withAlpha(theme.debugReadyColor, 22);
  QColor startingBg = withAlpha(theme.debugStartingColor, 30);
  QColor runningBg = withAlpha(theme.debugRunningColor, 30);
  QColor pausedBg = withAlpha(theme.debugPausedColor, 34);
  QColor errorBg = withAlpha(theme.debugErrorColor, 36);
  QColor toolbarButtonBg =
      blend(theme.backgroundColor, theme.surfaceColor, 0.04);
  QColor toolbarButtonHover =
      blend(theme.backgroundColor, theme.accentSoftColor, 0.04);
  QColor toolbarButtonPressed =
      blend(theme.backgroundColor, theme.accentSoftColor, 0.08);
  QColor treeBorder = withAlpha(theme.borderColor, 110);
  QColor shellBorder = withAlpha(theme.borderColor, 115);
  QColor tabBg = Qt::transparent;
  QColor tabHover = withAlpha(theme.foregroundColor, 8);
  QColor tabSelected = withAlpha(theme.foregroundColor, 6);
  QColor tabSelectedBorder = withAlpha(theme.accentColor, 80);

  setStyleSheet(fillStyleTemplate(
      QStringLiteral(
          "QWidget#debugPanel {"
          "  background: {panelBg};"
          "  color: {fg};"
          "}"
          "QWidget#debugToolbarShell {"
          "  background: {toolbarBg};"
          "  border: 1px solid {shellBorder};"
          "  border-radius: 4px;"
          "}"
          "QWidget#debugToolbar {"
          "  background: transparent;"
          "  border: none;"
          "}"
          "QWidget#debugToolbarGroup {"
          "  background: transparent;"
          "  border: none;"
          "}"
          "QFrame#debugToolbarDivider {"
          "  background: {shellBorder};"
          "  min-width: 1px;"
          "  max-width: 1px;"
          "}"
          // Ghost controls: no border and no fill by default, so a row of
          // actions reads as one strip instead of a line of outlined boxes.
          // Weight is carried by the surface on hover and by the accent on the
          // single primary action.
          "QToolButton#debugToolbarButton {"
          "  color: {fg};"
          "  background: transparent;"
          "  border: 1px solid transparent;"
          "  border-radius: 3px;"
          "  padding: 5px 10px;"
          "  font-size: 12px;"
          "  font-weight: 500;"
          "  text-align: left;"
          "}"
          "QToolButton#debugToolbarButton:hover {"
          "  background: {focusSurface};"
          "}"
          "QToolButton#debugToolbarButton:pressed {"
          "  background: {accentSoft};"
          "}"
          "QToolButton#debugToolbarButton:disabled {"
          "  color: {subtleText};"
          "  background: transparent;"
          "  border-color: transparent;"
          "}"
          "QToolButton#debugToolbarButton[role=\"primary\"] {"
          "  background: {accentSoft};"
          "  color: {fg};"
          "  font-weight: 600;"
          "}"
          "QToolButton#debugToolbarButton[role=\"primary\"]:hover {"
          "  background: {tabSelected};"
          "  border-color: {tabSelectedBorder};"
          "}"
          "QToolButton#debugToolbarButton[role=\"primary\"]:disabled {"
          "  background: transparent;"
          "}"
          "QToolButton#debugToolbarButton[role=\"danger\"]:hover {"
          "  background: {errorBg};"
          "  border-color: {errorColor};"
          "}"
          "QWidget#debugInspectorShell {"
          "  background: {shellSurface};"
          "  border: none;"
          "  border-radius: 4px;"
          "}"
          "QWidget#debugPanel QLabel {"
          "  color: {fg};"
          "}"
          "QWidget#debugInspectorTabBar {"
          "  background: transparent;"
          "  border: none;"
          "  border-bottom: 1px solid {shellBorder};"
          "}"
          "QStackedWidget#debugInspectorStack {"
          "  background: transparent;"
          "  border: none;"
          "}"
          // One indicator marks the active tab: an accent underline plus
          // primary text. A filled box as well would make the tab bar compete
          // with the content it labels.
          "QToolButton#debugInspectorTab {"
          "  color: {subtleText};"
          "  background: transparent;"
          "  border: none;"
          "  border-bottom: {tabIndicator}px solid transparent;"
          "  padding: 7px {tabPadH}px 6px {tabPadH}px;"
          "  margin: 0 {space}px 0 0;"
          "  font-size: 11px;"
          "  font-weight: 500;"
          "  text-align: left;"
          "}"
          "QToolButton#debugInspectorTab:hover {"
          "  color: {fg};"
          "  background: {tabHover};"
          "}"
          "QToolButton#debugInspectorTab:checked {"
          "  color: {fg};"
          "  background: transparent;"
          "  font-weight: 600;"
          "  border-bottom-color: {tabSelectedBorder};"
          "}"
          "QWidget#debugInspectorPage {"
          "  background: transparent;"
          "}"
          "QFrame#debugSectionCard {"
          "  background: transparent;"
          "  border: none;"
          "  border-radius: 0px;"
          "}"
          "QWidget#debugWatchContainer {"
          "  background: transparent;"
          "  border: none;"
          "}"
          "QToolButton#debugSectionAction {"
          "  color: {fg};"
          "  background: {inputSurface};"
          "  border: 1px solid {shellBorder};"
          "  border-radius: 3px;"
          "  padding: 3px 8px;"
          "  font-weight: 600;"
          "}"
          "QToolButton#debugSectionAction:hover {"
          "  background: {focusSurface};"
          "  border-color: {tabSelectedBorder};"
          "}"
          "QToolButton#debugSectionAction:pressed {"
          "  background: {accentSoft};"
          "}"
          "QLineEdit#debugWatchInput, QLineEdit#debugConsoleInput {"
          "  background: {inputSurface};"
          "  color: {fg};"
          "  border: 1px solid {shellBorder};"
          "  border-radius: 3px;"
          "  padding: 7px 10px;"
          "}"
          "QLineEdit#debugWatchInput:focus, QLineEdit#debugConsoleInput:focus {"
          "  background: {focusSurface};"
          "  border-color: {tabSelectedBorder};"
          "}"
          "QMenu {"
          "  background: {cardSurface};"
          "  color: {fg};"
          "  border: 1px solid {shellBorder};"
          "  padding: 6px;"
          "}"
          "QMenu::item {"
          "  padding: 6px 10px;"
          "  border-radius: 2px;"
          "}"
          "QMenu::item:selected {"
          "  background: {focusSurface};"
          "}"
          "QComboBox#debugThreadSelector {"
          "  min-height: 30px;"
          "  padding: 2px 10px;"
          "  border: 1px solid {shellBorder};"
          "  border-radius: 3px;"
          "  background: {inputSurface};"
          "  color: {fg};"
          "}"
          "QComboBox#debugThreadSelector:hover {"
          "  border-color: {tabSelectedBorder};"
          "}"
          "QComboBox#debugThreadSelector QAbstractItemView {"
          "  background: {shellSurface};"
          "  color: {fg};"
          "  border: 1px solid {shellBorder};"
          "  selection-background-color: {accentSoft};"
          "  selection-color: {fg};"
          "}"
          "QComboBox#debugThreadSelector::drop-down {"
          "  border: none;"
          "}"
          "QLabel#debugStatusLabel {"
          "  padding: 6px 12px;"
          "  font-weight: 600;"
          "  border: 1px solid {tabSelectedBorder};"
          "  border-radius: 3px;"
          "  background: {tabHover};"
          "  color: {fg};"
          "}"
          "QLabel#debugStatusLabel[statusKind=\"ready\"] {"
          "  border-color: {shellBorder};"
          "  background: {readyBg};"
          "}"
          "QLabel#debugStatusLabel[statusKind=\"starting\"] {"
          "  border-color: {warningColor};"
          "  background: {startingBg};"
          "  color: {warningColor};"
          "}"
          "QLabel#debugStatusLabel[statusKind=\"running\"] {"
          "  border-color: {tabSelectedBorder};"
          "  background: {runningBg};"
          "  color: {tabSelectedBorder};"
          "}"
          "QLabel#debugStatusLabel[statusKind=\"paused\"] {"
          "  border-color: {successColor};"
          "  background: {pausedBg};"
          "  color: {successColor};"
          "}"
          "QLabel#debugStatusLabel[statusKind=\"error\"] {"
          "  border-color: {errorColor};"
          "  background: {errorBg};"
          "  color: {errorColor};"
          "}"
          "QPlainTextEdit#debugConsoleOutput {"
          "  background: {consoleSurface};"
          "  color: {fg};"
          "  border: 1px solid {shellBorder};"
          "  border-radius: 0px;"
          "  selection-background-color: {accentSoft};"
          "  selection-color: {fg};"
          "  padding: 6px;"
          "}"),
      {{"panelBg", panelSurface.name()},
       {"fg", theme.foregroundColor.name()},
       {"toolbarBg", toolbarShell.name()},
       {"shellBorder", shellBorder.name(QColor::HexArgb)},
       {"shellSurface", shellSurface.name()},
       {"subtleText", subtleText.name()},
       {"tabBg", tabBg.name(QColor::HexArgb)},
       {"tabHover", tabHover.name(QColor::HexArgb)},
       {"tabSelected", tabSelected.name(QColor::HexArgb)},
       {"tabSelectedBorder", tabSelectedBorder.name(QColor::HexArgb)},
       {"cardSurface", cardSurface.name()},
       {"inputSurface", inputSurface.name()},
       {"focusSurface", focusSurface.name()},
       {"accentSoft", theme.accentSoftColor.name()},
       {"consoleSurface", consoleSurface.name()},
       {"warningColor", theme.warningColor.name()},
       {"startingBg", startingBg.name(QColor::HexArgb)},
       {"runningBg", runningBg.name(QColor::HexArgb)},
       {"successColor", theme.successColor.name()},
       {"pausedBg", pausedBg.name(QColor::HexArgb)},
       {"errorColor", theme.errorColor.name()},
       {"errorBg", errorBg.name(QColor::HexArgb)},
       {"readyBg", readyBg.name(QColor::HexArgb)},
       {"tabPadH", QString::number(UiMetrics::TabPaddingH)},
       {"tabIndicator", QString::number(UiMetrics::TabIndicatorThickness)},
       {"space", QString::number(UiMetrics::SpaceMd)}}));

  const QString treeStyle = fillStyleTemplate(
      QStringLiteral(
          // No outline: the tree already sits on its own surface inside the
          // inspector, and a box here would be the third nested frame.
          "QTreeWidget {"
          "  background: {bg};"
          "  alternate-background-color: {altBg};"
          "  color: {fg};"
          "  border: none;"
          "  border-radius: 0px;"
          "  outline: none;"
          "  selection-background-color: {selectionBg};"
          "  selection-color: {fg};"
          "}"
          "QTreeWidget::item {"
          "  color: {fg};"
          "  padding: 4px 8px;"
          "  margin: 0;"
          "  border-radius: 0px;"
          "}"
          "QTreeWidget::item:focus {"
          "  outline: none;"
          "}"
          "QTreeWidget::item:selected {"
          "  background: transparent;"
          "  color: {fg};"
          "}"
          "QTreeWidget::item:hover:!selected {"
          "  background: transparent;"
          "}"
          "QTreeWidget::branch {"
          "  background: transparent;"
          "}"
          "QTreeWidget::branch:has-children:closed,"
          "QTreeWidget::branch:closed:has-children:has-siblings {"
          "  image: url(:/resources/icons/branch_closed.png);"
          "}"
          "QTreeWidget::branch:has-children:open,"
          "QTreeWidget::branch:open:has-children:has-siblings {"
          "  image: url(:/resources/icons/branch_open.png);"
          "}"
          "QTreeWidget::branch:selected {"
          "  background: transparent;"
          "}"
          "QHeaderView::section {"
          "  background: transparent;"
          "  color: {subtleText};"
          "  border: none;"
          "  padding: 5px 8px 4px 8px;"
          "  font-size: 10px;"
          "  font-weight: 600;"
          "}"
          "QScrollBar:vertical {"
          "  background: transparent;"
          "  width: 8px;"
          "  margin: 2px 0 2px 0;"
          "}"
          "QScrollBar::handle:vertical {"
          "  background: {border};"
          "  min-height: 18px;"
          "  border-radius: 2px;"
          "}"
          "QScrollBar::handle:vertical:hover {"
          "  background: {subtleText};"
          "}"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
          "  height: 0px;"
          "}"
          "QScrollBar:horizontal {"
          "  background: transparent;"
          "  height: 8px;"
          "  margin: 0 2px 0 2px;"
          "}"
          "QScrollBar::handle:horizontal {"
          "  background: {border};"
          "  min-width: 18px;"
          "  border-radius: 2px;"
          "}"
          "QScrollBar::handle:horizontal:hover {"
          "  background: {subtleText};"
          "}"
          "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
          "  width: 0px;"
          "}"),
      {{"bg", recessedSurface.name()},
       {"altBg", cardSurface.name()},
       {"fg", theme.foregroundColor.name()},
       {"border", treeBorder.name(QColor::HexArgb)},
       {"selectionBg", theme.accentSoftColor.name()},
       {"subtleText", subtleText.name()}});

  for (QTreeWidget *tree :
       {m_callStackTree, m_variablesTree, m_watchTree, m_breakpointsTree}) {
    if (!tree) {
      continue;
    }
    tree->setStyleSheet(treeStyle);
    if (tree->header()) {
      tree->header()->setDefaultSectionSize(24);
      tree->header()->setMinimumSectionSize(20);
      tree->header()->setFixedHeight(22);
    }
    if (auto *delegate =
            dynamic_cast<DebugTreeDelegate *>(tree->itemDelegate())) {
      delegate->setTheme(theme);
    }
    applyTreePalette(tree, theme);
  }

  if (m_consoleOutput) {
    QPalette consolePalette = m_consoleOutput->palette();
    consolePalette.setColor(QPalette::Base, consoleSurface);
    consolePalette.setColor(QPalette::Text, theme.foregroundColor);
    consolePalette.setColor(QPalette::Highlight, focusSurface);
    consolePalette.setColor(QPalette::HighlightedText, theme.foregroundColor);
    m_consoleOutput->setPalette(consolePalette);
  }
}

void DebugPanel::setupUI() {
  setObjectName("debugPanel");
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(8);

  setupToolbar();
  QWidget *toolbarShell = new QWidget(this);
  toolbarShell->setObjectName("debugToolbarShell");
  QHBoxLayout *toolbarShellLayout = new QHBoxLayout(toolbarShell);
  toolbarShellLayout->setContentsMargins(10, 8, 10, 8);
  toolbarShellLayout->setSpacing(0);
  toolbarShellLayout->addWidget(m_toolbar);
  mainLayout->addWidget(toolbarShell);

  m_inspectorShell = new QWidget(this);
  m_inspectorShell->setObjectName("debugInspectorShell");
  QVBoxLayout *inspectorLayout = new QVBoxLayout(m_inspectorShell);
  inspectorLayout->setContentsMargins(0, 0, 0, 0);
  inspectorLayout->setSpacing(0);

  m_inspectorTabBar = new QWidget(m_inspectorShell);
  m_inspectorTabBar->setObjectName("debugInspectorTabBar");
  QHBoxLayout *tabBarLayout = new QHBoxLayout(m_inspectorTabBar);
  tabBarLayout->setContentsMargins(12, 8, 12, 0);
  tabBarLayout->setSpacing(0);

  m_inspectorTabGroup = new QButtonGroup(this);
  m_inspectorTabGroup->setExclusive(true);

  setupVariables();
  setupWatches();
  setupCallStack();
  setupBreakpoints();

  QWidget *stackPage = createInspectorPage(m_callStackTree);
  QWidget *variablesPage = createInspectorPage(m_variablesTree);
  QWidget *watchesPage =
      createInspectorPage(m_watchTree->parentWidget(), nullptr, nullptr);

  QWidget *breakpointActions = new QWidget(this);
  QHBoxLayout *breakpointActionsLayout = new QHBoxLayout(breakpointActions);
  breakpointActionsLayout->setContentsMargins(0, 0, 0, 0);
  breakpointActionsLayout->setSpacing(6);

  m_addFunctionBreakpointButton = new QToolButton(breakpointActions);
  m_addFunctionBreakpointButton->setObjectName("debugSectionAction");
  m_addFunctionBreakpointButton->setText(tr("Function"));
  m_addFunctionBreakpointButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(m_addFunctionBreakpointButton, &QToolButton::clicked, this, [this]() {
    bool ok = false;
    const QString functionName = QInputDialog::getText(
        this, tr("Function Breakpoint"), tr("Function name or symbol:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || functionName.trimmed().isEmpty()) {
      return;
    }
    BreakpointManager::instance().addFunctionBreakpoint(functionName.trimmed());
  });
  breakpointActionsLayout->addWidget(m_addFunctionBreakpointButton);

  m_exceptionBreakpointsButton = new QToolButton(breakpointActions);
  m_exceptionBreakpointsButton->setObjectName("debugSectionAction");
  m_exceptionBreakpointsButton->setText(tr("Exceptions"));
  m_exceptionBreakpointsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_exceptionBreakpointsButton->setPopupMode(QToolButton::InstantPopup);
  m_exceptionBreakpointsMenu = new QMenu(m_exceptionBreakpointsButton);
  connect(m_exceptionBreakpointsMenu, &QMenu::aboutToShow, this,
          &DebugPanel::rebuildExceptionBreakpointMenu);
  m_exceptionBreakpointsButton->setMenu(m_exceptionBreakpointsMenu);
  breakpointActionsLayout->addWidget(m_exceptionBreakpointsButton);
  breakpointActionsLayout->addStretch();

  QWidget *breakpointsPage =
      createInspectorPage(m_breakpointsTree, breakpointActions);

  setupConsole();
  m_consolePage =
      createInspectorPage(m_consoleOutput, m_consoleToolbar, m_consoleInput);
  QWidget *consolePage = m_consolePage;

  for (QTreeWidget *tree :
       {m_callStackTree, m_variablesTree, m_watchTree, m_breakpointsTree}) {
    if (tree) {
      tree->viewport()->installEventFilter(this);
    }
  }

  m_inspectorStack = new QStackedWidget(m_inspectorShell);
  m_inspectorStack->setObjectName("debugInspectorStack");
  m_inspectorStack->addWidget(stackPage);
  m_inspectorStack->addWidget(variablesPage);
  m_inspectorStack->addWidget(watchesPage);
  m_inspectorStack->addWidget(breakpointsPage);
  m_inspectorStack->addWidget(consolePage);

  const QStringList tabLabels = {tr("Stack"), tr("Variables"), tr("Watches"),
                                 tr("Breakpoints"), tr("Console")};
  for (int i = 0; i < tabLabels.size(); ++i) {
    tabBarLayout->addWidget(createInspectorTabButton(tabLabels.at(i), i));
  }
  tabBarLayout->addStretch(1);

  inspectorLayout->addWidget(m_inspectorTabBar);
  inspectorLayout->addWidget(m_inspectorStack, 1);
  mainLayout->addWidget(m_inspectorShell, 1);

  setCurrentInspectorTab(0);
  updateSectionSummaries();
}

QToolButton *DebugPanel::createInspectorTabButton(const QString &label,
                                                  int index) {
  QToolButton *button = new QToolButton(m_inspectorTabBar);
  button->setObjectName("debugInspectorTab");
  button->setText(label);
  button->setCheckable(true);
  button->setAutoRaise(false);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  button->setCursor(Qt::PointingHandCursor);
  m_inspectorTabGroup->addButton(button, index);
  m_inspectorTabButtons.append(button);
  connect(button, &QToolButton::clicked, this,
          [this, index]() { setCurrentInspectorTab(index); });
  return button;
}

QWidget *DebugPanel::createInspectorPage(QWidget *content,
                                         QWidget *headerActions,
                                         QWidget *footer) {
  QWidget *pageHost = new QWidget(this);
  pageHost->setObjectName("debugInspectorPage");
  pageHost->setAttribute(Qt::WA_StyledBackground, true);

  QVBoxLayout *hostLayout = new QVBoxLayout(pageHost);
  hostLayout->setContentsMargins(12, 10, 12, 12);
  hostLayout->setSpacing(0);

  QFrame *card = new QFrame(pageHost);
  card->setObjectName("debugSectionCard");
  card->setFrameShape(QFrame::NoFrame);
  card->setAttribute(Qt::WA_StyledBackground, true);

  QVBoxLayout *layout = new QVBoxLayout(card);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  if (headerActions) {
    layout->addWidget(headerActions);
  }

  if (content) {
    layout->addWidget(content, 1);
  }

  if (footer) {
    layout->addWidget(footer);
  }

  hostLayout->addWidget(card);
  return pageHost;
}

void DebugPanel::setCurrentInspectorTab(int index) {
  if (!m_inspectorStack || index < 0 || index >= m_inspectorStack->count()) {
    return;
  }

  m_inspectorStack->setCurrentIndex(index);
  for (int i = 0; i < m_inspectorTabButtons.size(); ++i) {
    if (QToolButton *button = m_inspectorTabButtons.at(i)) {
      const QSignalBlocker blocker(button);
      button->setChecked(i == index);
    }
  }

  // Refit once the tree is actually on screen and knows its real width.
  if (m_variablesTree && m_inspectorStack->currentWidget() &&
      m_inspectorStack->currentWidget()->isAncestorOf(m_variablesTree)) {
    QTimer::singleShot(0, this, [this]() { fitVariablesNameColumn(); });
  }
}

void DebugPanel::setInspectorTabLabel(int index, const QString &label) {
  if (index < 0 || index >= m_inspectorTabButtons.size()) {
    return;
  }
  if (QToolButton *button = m_inspectorTabButtons.at(index)) {
    button->setText(label);
  }
}

QList<QAction *> DebugPanel::transportActions() const {
  QList<QAction *> actions;
  for (QAction *action :
       {m_continueAction, m_pauseAction, m_stepOverAction, m_stepIntoAction,
        m_stepOutAction, m_restartAction, m_stopAction}) {
    if (action) {
      actions.append(action);
    }
  }
  return actions;
}

void DebugPanel::setupToolbar() {
  m_toolbar = new QWidget(this);
  m_toolbar->setObjectName("debugToolbar");

  QHBoxLayout *toolbarLayout = new QHBoxLayout(m_toolbar);
  toolbarLayout->setContentsMargins(0, 0, 0, 0);
  toolbarLayout->setSpacing(UiMetrics::SpaceLg);

  const auto configureAction = [](QAction *action, const QString &toolTip,
                                  const QString &helpText) {
    if (!action) {
      return;
    }
    action->setToolTip(toolTip);
    action->setStatusTip(toolTip);
    action->setWhatsThis(helpText);
  };

  m_continueAction = new QAction(tr("Start"), this);
  m_continueAction->setShortcut(QKeySequence(Qt::Key_F5));
  configureAction(m_continueAction, tr("Start or continue debugging (F5)"),
                  tr("Starts a debug session when idle, or continues execution "
                     "when paused."));
  connect(m_continueAction, &QAction::triggered, this, &DebugPanel::onContinue);

  m_pauseAction = new QAction(tr("Pause"), this);
  m_pauseAction->setShortcut(QKeySequence(Qt::Key_F6));
  configureAction(
      m_pauseAction, tr("Pause execution (F6)"),
      tr("Interrupts a running debug session at the next safe point."));
  connect(m_pauseAction, &QAction::triggered, this, &DebugPanel::onPause);

  m_stepOverAction = new QAction(tr("Over"), this);
  m_stepOverAction->setShortcut(QKeySequence(Qt::Key_F10));
  configureAction(
      m_stepOverAction, tr("Step over current line (F10)"),
      tr("Executes the current line without entering called functions."));
  connect(m_stepOverAction, &QAction::triggered, this, &DebugPanel::onStepOver);

  m_stepIntoAction = new QAction(tr("Into"), this);
  m_stepIntoAction->setShortcut(QKeySequence(Qt::Key_F11));
  configureAction(
      m_stepIntoAction, tr("Step into function call (F11)"),
      tr("Advances into the function being called on the current line."));
  connect(m_stepIntoAction, &QAction::triggered, this, &DebugPanel::onStepInto);

  m_stepOutAction = new QAction(tr("Out"), this);
  m_stepOutAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));
  configureAction(m_stepOutAction,
                  tr("Step out of current function (Shift+F11)"),
                  tr("Runs until the current function returns to its caller."));
  connect(m_stepOutAction, &QAction::triggered, this, &DebugPanel::onStepOut);

  m_restartAction = new QAction(tr("Restart"), this);
  m_restartAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
  configureAction(m_restartAction, tr("Restart debugging (Ctrl+Shift+F5)"),
                  tr("Stops and relaunches the current debug session."));
  connect(m_restartAction, &QAction::triggered, this, &DebugPanel::onRestart);

  m_stopAction = new QAction(tr("Stop"), this);
  m_stopAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
  configureAction(
      m_stopAction, tr("Stop debugging (Shift+F5)"),
      tr("Terminates debugging and clears the current debug context."));
  connect(m_stopAction, &QAction::triggered, this, &DebugPanel::onStop);

  QWidget *transportGroup = new QWidget(m_toolbar);
  transportGroup->setObjectName("debugToolbarGroup");
  QHBoxLayout *transportLayout = new QHBoxLayout(transportGroup);
  transportLayout->setContentsMargins(0, 0, 0, 0);
  transportLayout->setSpacing(UiMetrics::ToolbarGroupSpacing);
  transportLayout->addWidget(
      createToolbarButton(m_continueAction, QStringLiteral("primary")));
  transportLayout->addWidget(createToolbarButton(m_pauseAction));

  QWidget *stepGroup = new QWidget(m_toolbar);
  stepGroup->setObjectName("debugToolbarGroup");
  QHBoxLayout *stepLayout = new QHBoxLayout(stepGroup);
  stepLayout->setContentsMargins(0, 0, 0, 0);
  stepLayout->setSpacing(UiMetrics::ToolbarGroupSpacing);
  stepLayout->addWidget(createToolbarButton(m_stepOverAction));
  stepLayout->addWidget(createToolbarButton(m_stepIntoAction));
  stepLayout->addWidget(createToolbarButton(m_stepOutAction));

  QWidget *sessionGroup = new QWidget(m_toolbar);
  sessionGroup->setObjectName("debugToolbarGroup");
  QHBoxLayout *sessionLayout = new QHBoxLayout(sessionGroup);
  sessionLayout->setContentsMargins(0, 0, 0, 0);
  sessionLayout->setSpacing(UiMetrics::ToolbarGroupSpacing);
  sessionLayout->addWidget(createToolbarButton(m_restartAction));
  sessionLayout->addWidget(
      createToolbarButton(m_stopAction, QStringLiteral("danger")));

  m_threadSelector = new QComboBox(this);
  m_threadSelector->setObjectName("debugThreadSelector");
  m_threadSelector->setToolTip(tr("Select active thread"));
  m_threadSelector->setStatusTip(tr("Select active thread"));
  m_threadSelector->setMinimumWidth(160);
  m_threadSelector->setEnabled(false);
  m_threadSelector->setCursor(Qt::PointingHandCursor);
  connect(m_threadSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DebugPanel::onThreadSelected);

  m_debugStatusLabel = new QLabel(tr("Ready"), this);
  m_debugStatusLabel->setObjectName("debugStatusLabel");
  m_debugStatusLabel->setMinimumWidth(110);
  m_debugStatusText = m_debugStatusLabel->text();
  // Re-elide whenever the pill is given a new width.
  m_debugStatusLabel->installEventFilter(this);
  m_debugStatusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

  QWidget *metaGroup = new QWidget(m_toolbar);
  metaGroup->setObjectName("debugToolbarGroup");
  QHBoxLayout *metaLayout = new QHBoxLayout(metaGroup);
  metaLayout->setContentsMargins(0, 0, 0, 0);
  metaLayout->setSpacing(10);
  metaLayout->addWidget(m_threadSelector);
  metaLayout->addWidget(m_debugStatusLabel);

  applyTransportIcons();

  toolbarLayout->addWidget(transportGroup);
  toolbarLayout->addWidget(createToolbarDivider());
  toolbarLayout->addWidget(stepGroup);
  toolbarLayout->addWidget(createToolbarDivider());
  toolbarLayout->addWidget(sessionGroup);
  toolbarLayout->addStretch(1);
  toolbarLayout->addWidget(metaGroup);
}

void DebugPanel::applyTransportIcons() {
  // Stepping and session control are neutral chrome; only the primary action
  // and the destructive one earn a colour of their own.
  const QColor neutral = m_themeInitialized ? m_theme.foregroundColor
                                            : palette().color(QPalette::Text);
  const QColor accent = m_themeInitialized ? m_theme.accentColor : neutral;
  const QColor danger = m_themeInitialized ? m_theme.errorColor : neutral;

  const struct {
    QAction *action;
    DebugGlyphs::Glyph glyph;
    QColor color;
  } glyphs[] = {
      {m_continueAction, DebugGlyphs::Glyph::Play, accent},
      {m_pauseAction, DebugGlyphs::Glyph::Pause, neutral},
      {m_stepOverAction, DebugGlyphs::Glyph::StepOver, neutral},
      {m_stepIntoAction, DebugGlyphs::Glyph::StepInto, neutral},
      {m_stepOutAction, DebugGlyphs::Glyph::StepOut, neutral},
      {m_restartAction, DebugGlyphs::Glyph::Restart, neutral},
      {m_stopAction, DebugGlyphs::Glyph::Stop, danger},
  };

  for (const auto &entry : glyphs) {
    if (entry.action) {
      entry.action->setIcon(DebugGlyphs::icon(entry.glyph, entry.color));
    }
  }
}

QToolButton *DebugPanel::createToolbarButton(QAction *action,
                                             const QString &role) {
  QToolButton *button = new QToolButton(m_toolbar);
  button->setObjectName("debugToolbarButton");
  button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  button->setDefaultAction(action);
  button->setCursor(Qt::PointingHandCursor);
  if (!role.isEmpty()) {
    button->setProperty("role", role);
  }
  return button;
}

QWidget *DebugPanel::createToolbarDivider() {
  QFrame *divider = new QFrame(m_toolbar);
  divider->setObjectName("debugToolbarDivider");
  divider->setFrameShape(QFrame::NoFrame);
  divider->setMinimumHeight(22);
  return divider;
}

void DebugPanel::setupCallStack() {
  m_callStackTree = new QTreeWidget(this);
  m_callStackTree->setObjectName("debugCallStackTree");
  m_callStackTree->setHeaderLabels({tr("Function"), tr("File"), tr("Line")});
  m_callStackTree->setRootIsDecorated(false);
  m_callStackTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_callStackTree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_callStackTree->setAlternatingRowColors(false);
  m_callStackTree->setUniformRowHeights(true);
  m_callStackTree->setAllColumnsShowFocus(false);
  m_callStackTree->setFrameShape(QFrame::NoFrame);
  m_callStackTree->setMouseTracking(true);
  m_callStackTree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_callStackTree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_callStackTree->setItemDelegate(new DebugTreeDelegate(m_callStackTree));

  m_callStackTree->header()->setStretchLastSection(false);
  m_callStackTree->header()->setHighlightSections(false);
  m_callStackTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_callStackTree->header()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_callStackTree->header()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);

  connect(m_callStackTree, &QTreeWidget::itemClicked, this,
          &DebugPanel::onCallStackItemClicked);
  connect(m_callStackTree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
            if (current) {
              onCallStackItemClicked(current, 0);
            }
          });
}

void DebugPanel::setupVariables() {
  m_variablesTree = new QTreeWidget(this);
  m_variablesTree->setObjectName("debugVariablesTree");
  m_variablesTree->setHeaderLabels({tr("Name"), tr("Value"), tr("Type")});
  m_variablesTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_variablesTree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_variablesTree->setAlternatingRowColors(false);
  m_variablesTree->setUniformRowHeights(true);
  m_variablesTree->setAllColumnsShowFocus(false);
  m_variablesTree->setFrameShape(QFrame::NoFrame);
  m_variablesTree->setMouseTracking(true);
  m_variablesTree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_variablesTree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_variablesTree->setIndentation(14);
  m_variablesTree->setItemDelegate(new DebugTreeDelegate(m_variablesTree));

  m_variablesTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_variablesTree, &QTreeWidget::customContextMenuRequested, this,
          &DebugPanel::showVariablesContextMenu);

  m_variablesTree->header()->setStretchLastSection(false);
  m_variablesTree->header()->setHighlightSections(false);
  m_variablesTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  connect(m_variablesTree->header(), &QHeaderView::sectionResized, this,
          [this](int section, int, int) {
            // Layout passes resize sections too; only a live drag (left button
            // held over the header) counts as the user choosing a width.
            if (section == 0 && !m_variablesNameColumnAutofitting &&
                (QApplication::mouseButtons() & Qt::LeftButton)) {
              m_variablesNameColumnUserSized = true;
            }
          });
  m_variablesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_variablesTree->header()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);

  connect(m_variablesTree, &QTreeWidget::itemExpanded, this,
          &DebugPanel::onVariableItemExpanded);
}

void DebugPanel::setupWatches() {

  QWidget *watchContainer = new QWidget(this);
  watchContainer->setObjectName("debugWatchContainer");
  QVBoxLayout *watchLayout = new QVBoxLayout(watchContainer);
  watchLayout->setContentsMargins(0, 0, 0, 0);
  watchLayout->setSpacing(6);

  m_watchTree = new QTreeWidget(watchContainer);
  m_watchTree->setObjectName("debugWatchTree");
  m_watchTree->setHeaderLabels({tr("Expression"), tr("Value"), tr("Type")});
  m_watchTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_watchTree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_watchTree->setAlternatingRowColors(false);
  m_watchTree->setRootIsDecorated(true);
  m_watchTree->setUniformRowHeights(true);
  m_watchTree->setAllColumnsShowFocus(false);
  m_watchTree->setFrameShape(QFrame::NoFrame);
  m_watchTree->setMouseTracking(true);
  m_watchTree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_watchTree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_watchTree->setIndentation(14);
  m_watchTree->setItemDelegate(new DebugTreeDelegate(m_watchTree));

  m_watchTree->header()->setStretchLastSection(false);
  m_watchTree->header()->setHighlightSections(false);
  m_watchTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_watchTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_watchTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  connect(m_watchTree, &QTreeWidget::itemExpanded, this,
          &DebugPanel::onWatchItemExpanded);

  m_watchTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_watchTree, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QTreeWidgetItem *item = m_watchTree->itemAt(pos);
            if (!item || item->parent())
              return;

            QMenu menu;
            QAction *removeAction = menu.addAction(tr("Remove Watch"));
            QAction *chosen = menu.exec(m_watchTree->mapToGlobal(pos));
            if (chosen == removeAction) {
              int watchId = item->data(0, Qt::UserRole).toInt();
              WatchManager::instance().removeWatch(watchId);
            }
          });

  watchLayout->addWidget(m_watchTree);

  QHBoxLayout *inputLayout = new QHBoxLayout();
  inputLayout->setContentsMargins(0, 0, 0, 0);

  m_watchInput = new QLineEdit(watchContainer);
  m_watchInput->setObjectName("debugWatchInput");
  m_watchInput->setPlaceholderText(tr("Add watch expression..."));
  m_watchInput->setClearButtonEnabled(true);
  m_watchInput->setMinimumHeight(34);
  connect(m_watchInput, &QLineEdit::returnPressed, this,
          &DebugPanel::onAddWatch);

  inputLayout->addWidget(m_watchInput);
  watchLayout->addLayout(inputLayout);
}

void DebugPanel::setupBreakpoints() {
  m_breakpointsTree = new QTreeWidget(this);
  m_breakpointsTree->setObjectName("debugBreakpointsTree");
  m_breakpointsTree->setHeaderLabels({tr(""), tr("Location"), tr("Condition")});
  m_breakpointsTree->setRootIsDecorated(false);
  m_breakpointsTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_breakpointsTree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_breakpointsTree->setAlternatingRowColors(false);
  m_breakpointsTree->setUniformRowHeights(true);
  m_breakpointsTree->setAllColumnsShowFocus(false);
  m_breakpointsTree->setFrameShape(QFrame::NoFrame);
  m_breakpointsTree->setMouseTracking(true);
  m_breakpointsTree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_breakpointsTree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_breakpointsTree->setItemDelegate(new DebugTreeDelegate(m_breakpointsTree));
  m_breakpointsTree->setContextMenuPolicy(Qt::CustomContextMenu);

  m_breakpointsTree->header()->setStretchLastSection(true);
  m_breakpointsTree->header()->setHighlightSections(false);
  m_breakpointsTree->header()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_breakpointsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

  connect(m_breakpointsTree, &QTreeWidget::itemDoubleClicked, this,
          &DebugPanel::onBreakpointItemDoubleClicked);
  connect(m_breakpointsTree, &QTreeWidget::itemChanged, this,
          &DebugPanel::onBreakpointItemChanged);
  connect(m_breakpointsTree, &QTreeWidget::customContextMenuRequested, this,
          &DebugPanel::onBreakpointsContextMenuRequested);
}

void DebugPanel::setupConsole() {
  m_consoleOutput = new QPlainTextEdit(this);
  m_consoleOutput->setObjectName("debugConsoleOutput");
  m_consoleOutput->setReadOnly(true);
  m_consoleOutput->setUndoRedoEnabled(false);
  m_consoleOutput->document()->setMaximumBlockCount(MAX_DEBUG_CONSOLE_BLOCKS);
  QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  fixedFont.setPointSize(9);
  m_consoleOutput->setFont(fixedFont);
  m_consoleOutput->setFrameShape(QFrame::NoFrame);
  m_consoleOutput->setWordWrapMode(QTextOption::NoWrap);
  m_consoleOutput->setPlaceholderText(tr("Debug console output..."));
  m_consoleOutput->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_consoleOutput, &QPlainTextEdit::customContextMenuRequested, this,
          &DebugPanel::showConsoleContextMenu);
  // A location printed in a traceback or adapter message is the fastest way
  // back to the code, so make the line it sits on openable.
  m_consoleOutput->viewport()->installEventFilter(this);

  m_consoleInput = new QLineEdit(this);
  m_consoleInput->setObjectName("debugConsoleInput");
  m_consoleInput->setFont(fixedFont);
  m_consoleInput->setClearButtonEnabled(true);
  m_consoleInput->setMinimumHeight(UiMetrics::ControlHeight + 4);
  // Up/Down recall previous expressions instead of moving the caret.
  m_consoleInput->installEventFilter(this);

  connect(m_consoleInput, &QLineEdit::returnPressed, this,
          &DebugPanel::onConsoleInput);

  setupConsoleToolbar();
  updateConsoleInputAffordances();
}

void DebugPanel::setupConsoleToolbar() {
  m_consoleToolbar = new QWidget(this);
  m_consoleToolbar->setObjectName("debugConsoleToolbar");

  QHBoxLayout *layout = new QHBoxLayout(m_consoleToolbar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(UiMetrics::ToolbarGroupSpacing);

  m_consoleFindInput = new QLineEdit(m_consoleToolbar);
  m_consoleFindInput->setObjectName("debugConsoleFind");
  m_consoleFindInput->setPlaceholderText(tr("Find in console…"));
  m_consoleFindInput->setClearButtonEnabled(true);
  m_consoleFindInput->setFixedHeight(UiMetrics::ControlHeight);
  m_consoleFindInput->setFixedWidth(240);
  connect(m_consoleFindInput, &QLineEdit::returnPressed, this,
          [this]() { findInConsole(false); });
  layout->addWidget(m_consoleFindInput);
  layout->addStretch(1);

  m_consoleWrapAction = new QAction(tr("Wrap"), this);
  m_consoleWrapAction->setCheckable(true);
  m_consoleWrapAction->setToolTip(tr("Wrap long lines instead of scrolling "
                                     "horizontally"));
  connect(m_consoleWrapAction, &QAction::toggled, this, [this](bool wrap) {
    m_consoleOutput->setWordWrapMode(
        wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
  });

  m_consoleClearAction = new QAction(tr("Clear"), this);
  m_consoleClearAction->setToolTip(tr("Clear the console output"));
  connect(m_consoleClearAction, &QAction::triggered, this,
          [this]() { m_consoleOutput->clear(); });

  m_consoleDetachAction = new QAction(tr("Detach"), this);
  m_consoleDetachAction->setToolTip(
      tr("Open the console in its own window, where long output is readable"));
  connect(m_consoleDetachAction, &QAction::triggered, this,
          &DebugPanel::toggleConsoleDetached);

  for (QAction *action :
       {m_consoleWrapAction, m_consoleDetachAction, m_consoleClearAction}) {
    QToolButton *button = new QToolButton(m_consoleToolbar);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setFixedHeight(UiMetrics::ControlHeight);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setObjectName("debugConsoleToolButton");
    layout->addWidget(button);
  }
}

void DebugPanel::toggleConsoleDetached() {
  if (m_consoleWindow) {
    reattachConsole();
    return;
  }
  if (!m_consolePage || !m_inspectorStack) {
    return;
  }

  // A bottom dock leaves the console a few lines tall, which is unusable for
  // reading a traceback. Move the very same page into a window the user can
  // size freely; reattaching puts it back where it came from.
  m_consoleWindow = new QWidget(window(), Qt::Window);
  m_consoleWindow->setWindowTitle(tr("Debug Console"));
  m_consoleWindow->setAttribute(Qt::WA_DeleteOnClose, false);
  m_consoleWindow->resize(900, 520);

  auto *layout = new QVBoxLayout(m_consoleWindow);
  layout->setContentsMargins(UiMetrics::PanelMargin, UiMetrics::PanelMargin,
                             UiMetrics::PanelMargin, UiMetrics::PanelMargin);
  layout->setSpacing(0);

  m_inspectorStack->removeWidget(m_consolePage);
  layout->addWidget(m_consolePage);
  m_consolePage->show();

  m_consoleWindow->installEventFilter(this);
  m_consoleWindow->show();
  m_consoleWindow->raise();

  m_consoleDetachAction->setText(tr("Attach"));
  m_consoleDetachAction->setToolTip(tr("Put the console back in the panel"));

  // The tab it used to occupy would now switch to an empty page.
  setInspectorTabLabel(4, tr("Console ↗"));
  setCurrentInspectorTab(0);
}

void DebugPanel::reattachConsole() {
  if (!m_consoleWindow || !m_consolePage || !m_inspectorStack) {
    return;
  }

  m_consoleWindow->removeEventFilter(this);
  m_consolePage->setParent(nullptr);
  m_inspectorStack->insertWidget(4, m_consolePage);

  m_consoleWindow->deleteLater();
  m_consoleWindow = nullptr;

  m_consoleDetachAction->setText(tr("Detach"));
  m_consoleDetachAction->setToolTip(
      tr("Open the console in its own window, where long output is readable"));
  setInspectorTabLabel(4, tr("Console"));
  setCurrentInspectorTab(4);
}

void DebugPanel::recallConsoleHistory(int direction) {
  if (m_consoleHistory.isEmpty()) {
    return;
  }

  // Leaving the newest entry keeps whatever the user had half-typed, so that
  // browsing history and coming back does not lose it.
  if (m_consoleHistoryIndex == m_consoleHistory.size()) {
    m_consoleHistoryDraft = m_consoleInput->text();
  }

  const int target =
      qBound(0, m_consoleHistoryIndex + direction, m_consoleHistory.size());
  if (target == m_consoleHistoryIndex) {
    return;
  }
  m_consoleHistoryIndex = target;

  m_consoleInput->setText(target == m_consoleHistory.size()
                              ? m_consoleHistoryDraft
                              : m_consoleHistory.at(target));
  m_consoleInput->setCursorPosition(m_consoleInput->text().size());
}

void DebugPanel::findInConsole(bool backwards) {
  const QString needle = m_consoleFindInput->text();
  if (needle.isEmpty()) {
    return;
  }

  QTextDocument::FindFlags flags;
  if (backwards) {
    flags |= QTextDocument::FindBackward;
  }
  if (m_consoleOutput->find(needle, flags)) {
    return;
  }

  // Wrap around rather than silently doing nothing at the last match.
  QTextCursor cursor = m_consoleOutput->textCursor();
  cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
  m_consoleOutput->setTextCursor(cursor);
  if (!m_consoleOutput->find(needle, flags)) {
    appendConsoleLine(tr("No match for \"%1\"").arg(needle),
                      consoleMutedColor());
  }
}

QString
DebugPanel::expressionForVariableItem(const QTreeWidgetItem *item) const {
  if (!item) {
    return QString();
  }
  // Adapters supply evaluateName for rows that can be re-evaluated; a scope
  // header or a synthetic row has none and cannot become a watch.
  const QString evaluateName = item->data(0, Qt::UserRole + 1).toString();
  if (!evaluateName.isEmpty()) {
    return evaluateName;
  }
  return item->parent() ? item->text(0) : QString();
}

void DebugPanel::showVariablesContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_variablesTree->itemAt(pos);
  if (!item) {
    return;
  }

  const QString name = item->text(0);
  const QString value = item->text(1);
  const QString expression = expressionForVariableItem(item);

  QMenu menu(m_variablesTree);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));

  QAction *copyName = menu.addAction(tr("Copy Name"));
  QAction *copyValue = menu.addAction(tr("Copy Value"));
  copyValue->setEnabled(!value.isEmpty());
  QAction *copyExpression = menu.addAction(tr("Copy Expression"));
  copyExpression->setEnabled(!expression.isEmpty());

  menu.addSeparator();
  QAction *addWatch = menu.addAction(tr("Add to Watch"));
  addWatch->setEnabled(!expression.isEmpty());

  menu.addSeparator();
  QAction *expand = menu.addAction(tr("Expand"));
  QAction *collapse = menu.addAction(tr("Collapse"));
  const bool expandable =
      item->childCount() > 0 || item->data(0, Qt::UserRole).toInt() > 0;
  expand->setEnabled(expandable && !item->isExpanded());
  collapse->setEnabled(expandable && item->isExpanded());
  QAction *refresh = menu.addAction(tr("Refresh"));

  QAction *chosen = menu.exec(m_variablesTree->viewport()->mapToGlobal(pos));
  if (!chosen) {
    return;
  }

  if (chosen == copyName) {
    QGuiApplication::clipboard()->setText(name);
  } else if (chosen == copyValue) {
    QGuiApplication::clipboard()->setText(value);
  } else if (chosen == copyExpression) {
    QGuiApplication::clipboard()->setText(expression);
  } else if (chosen == addWatch) {
    WatchManager::instance().addWatch(expression);
  } else if (chosen == expand) {
    item->setExpanded(true);
  } else if (chosen == collapse) {
    item->setExpanded(false);
  } else if (chosen == refresh) {
    // Re-request the frame's scopes, which repopulates the whole tree.
    setCurrentFrame(m_currentFrameId);
  }
}

void DebugPanel::showConsoleContextMenu(const QPoint &pos) {
  QMenu menu(m_consoleOutput);
  menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));

  QAction *copy = menu.addAction(tr("Copy"));
  copy->setEnabled(m_consoleOutput->textCursor().hasSelection());
  QAction *copyAll = menu.addAction(tr("Copy All"));
  QAction *selectAll = menu.addAction(tr("Select All"));
  menu.addSeparator();
  QAction *wrap = menu.addAction(tr("Word Wrap"));
  wrap->setCheckable(true);
  wrap->setChecked(m_consoleOutput->wordWrapMode() != QTextOption::NoWrap);
  menu.addSeparator();
  QAction *clear = menu.addAction(tr("Clear"));

  QAction *chosen = menu.exec(m_consoleOutput->viewport()->mapToGlobal(pos));
  if (chosen == copy) {
    m_consoleOutput->copy();
  } else if (chosen == copyAll) {
    QGuiApplication::clipboard()->setText(m_consoleOutput->toPlainText());
  } else if (chosen == selectAll) {
    m_consoleOutput->selectAll();
  } else if (chosen == wrap) {
    m_consoleWrapAction->setChecked(wrap->isChecked());
  } else if (chosen == clear) {
    m_consoleOutput->clear();
  }
}

void DebugPanel::navigateToConsoleLocation(const QString &lineText) {
  // Matches "/abs/path.cpp:42" and "file.py", line 8 - the two shapes gdb and
  // debugpy actually emit. Anything else is left alone rather than guessed at.
  static const QRegularExpression pathLine(
      QStringLiteral("([^\\s\"'<>|]+\\.[A-Za-z0-9_+]+):(\\d+)"));
  static const QRegularExpression pythonFrame(
      QStringLiteral("File \"([^\"]+)\", line (\\d+)"));

  QString path;
  int line = 0;

  QRegularExpressionMatch match = pythonFrame.match(lineText);
  if (match.hasMatch()) {
    path = match.captured(1);
    line = match.captured(2).toInt();
  } else {
    match = pathLine.match(lineText);
    if (match.hasMatch()) {
      path = match.captured(1);
      line = match.captured(2).toInt();
    }
  }

  if (path.isEmpty() || line <= 0) {
    return;
  }
  emit locationClicked(path, line, 0);
}

void DebugPanel::updateConsoleInputAffordances() {
  if (!m_consoleInput) {
    return;
  }

  const DapClient::State state =
      m_dapClient ? m_dapClient->state() : DapClient::State::Disconnected;

  // The input stays enabled in every state: a disabled box that swallows the
  // click explains nothing, whereas submitting prints why it cannot evaluate.
  if (state == DapClient::State::Stopped) {
    QString frameLabel;
    for (const DapStackFrame &frame : m_stackFrames) {
      if (frame.id == m_currentFrameId) {
        frameLabel = frame.name;
        break;
      }
    }
    m_consoleInput->setPlaceholderText(
        frameLabel.isEmpty() ? tr("Evaluate expression…")
                             : tr("Evaluate in %1…").arg(frameLabel));
  } else if (state == DapClient::State::Running) {
    m_consoleInput->setPlaceholderText(
        tr("Pause execution to evaluate expressions"));
  } else {
    m_consoleInput->setPlaceholderText(
        tr("Start debugging to evaluate expressions"));
  }
}

QJsonArray DebugPanel::currentExceptionBreakpointFilters() const {
  QString adapterId = m_dapClient ? m_dapClient->adapterId() : QString();
  if (!adapterId.isEmpty()) {
    const auto adapter = DebugAdapterRegistry::instance().adapter(adapterId);
    if (adapter) {
      return adapter->config().exceptionBreakpointFilters;
    }
  }

  QJsonArray merged;
  QSet<QString> seen;
  const auto adapters = DebugAdapterRegistry::instance().allAdapters();
  for (const auto &adapter : adapters) {
    if (!adapter) {
      continue;
    }
    const QJsonArray filters = adapter->config().exceptionBreakpointFilters;
    for (const QJsonValue &filterValue : filters) {
      const QJsonObject filter = filterValue.toObject();
      const QString filterId = filter["filter"].toString();
      if (filterId.isEmpty() || seen.contains(filterId)) {
        continue;
      }
      seen.insert(filterId);
      merged.append(filter);
    }
  }
  return merged;
}

void DebugPanel::rebuildExceptionBreakpointMenu() {
  if (!m_exceptionBreakpointsMenu) {
    return;
  }

  m_exceptionBreakpointsMenu->clear();
  const QJsonArray filters = currentExceptionBreakpointFilters();
  const QStringList enabledFilters =
      BreakpointManager::instance().enabledExceptionFilters();

  if (filters.isEmpty()) {
    QAction *emptyAction =
        m_exceptionBreakpointsMenu->addAction(tr("No exception filters"));
    emptyAction->setEnabled(false);
    return;
  }

  for (const QJsonValue &filterValue : filters) {
    const QJsonObject filter = filterValue.toObject();
    const QString filterId = filter["filter"].toString();
    if (filterId.isEmpty()) {
      continue;
    }
    const QString label = filter["label"].toString(filterId);
    QAction *action = m_exceptionBreakpointsMenu->addAction(label);
    action->setCheckable(true);
    action->setChecked(enabledFilters.contains(filterId));
    connect(action, &QAction::toggled, this, [filterId](bool checked) {
      QStringList filters =
          BreakpointManager::instance().enabledExceptionFilters();
      if (checked) {
        if (!filters.contains(filterId)) {
          filters.append(filterId);
        }
      } else {
        filters.removeAll(filterId);
      }
      BreakpointManager::instance().setExceptionBreakpoints(filters);
    });
  }
}

void DebugPanel::updateSectionSummaries() {
  if (m_inspectorStack) {
    const int scopeCount =
        m_variablesTree ? m_variablesTree->topLevelItemCount() : 0;
    const int watchCount = m_watchTree ? m_watchTree->topLevelItemCount() : 0;
    const int sourceCount =
        BreakpointManager::instance().allBreakpoints().size();
    const int functionCount =
        BreakpointManager::instance().allFunctionBreakpoints().size();
    const int dataCount =
        BreakpointManager::instance().allDataBreakpoints().size();
    const int exceptionCount =
        BreakpointManager::instance().enabledExceptionFilters().size();

    QString stackLabel = tr("Stack");
    if (!m_threads.isEmpty() || !m_stackFrames.isEmpty()) {
      stackLabel =
          tr("Stack (%1/%2)").arg(m_threads.size()).arg(m_stackFrames.size());
    }
    setInspectorTabLabel(0, stackLabel);
    setInspectorTabLabel(1, scopeCount > 0
                                ? tr("Variables (%1)").arg(scopeCount)
                                : tr("Variables"));
    setInspectorTabLabel(2, watchCount > 0 ? tr("Watches (%1)").arg(watchCount)
                                           : tr("Watches"));

    const int breakpointTotal =
        sourceCount + functionCount + dataCount + exceptionCount;
    setInspectorTabLabel(3, breakpointTotal > 0
                                ? tr("Breakpoints (%1)").arg(breakpointTotal)
                                : tr("Breakpoints"));
    setInspectorTabLabel(4, tr("Console"));
  }

  if (m_exceptionBreakpointsButton) {
    m_exceptionBreakpointsButton->setEnabled(
        !currentExceptionBreakpointFilters().isEmpty());
  }
}

void DebugPanel::setDapClient(DapClient *client) {
  if (m_dapClient) {
    disconnect(m_dapClient, nullptr, this, nullptr);
  }

  m_dapClient = client;
  clearAll();

  if (m_dapClient) {
    connect(m_dapClient, &DapClient::stateChanged, this,
            &DebugPanel::updateToolbarState);
    connect(m_dapClient, &DapClient::stopped, this, &DebugPanel::onStopped);
    connect(m_dapClient, &DapClient::continued, this,
            [this](int, bool) { onContinued(); });
    connect(m_dapClient, &DapClient::terminated, this,
            &DebugPanel::onTerminated);
    connect(m_dapClient, &DapClient::threadsReceived, this,
            &DebugPanel::onThreadsReceived);
    connect(m_dapClient, &DapClient::stackTraceReceived, this,
            &DebugPanel::onStackTraceReceived);
    connect(m_dapClient, &DapClient::scopesReceived, this,
            &DebugPanel::onScopesReceived);
    connect(m_dapClient, &DapClient::variablesReceived, this,
            &DebugPanel::onVariablesReceived);
    connect(m_dapClient, &DapClient::output, this,
            &DebugPanel::onOutputReceived);
    connect(m_dapClient, &DapClient::evaluateResponse, this,
            &DebugPanel::onEvaluateResult);
    connect(m_dapClient, &DapClient::evaluateResponseError, this,
            &DebugPanel::onEvaluateError);
    connect(m_dapClient, &DapClient::exceptionInfoReceived, this,
            &DebugPanel::onExceptionInfoReceived);
    connect(m_dapClient, &DapClient::exited, this, [this](int exitCode) {
      appendConsoleLine(tr("Program exited with code %1.").arg(exitCode),
                        consoleMutedColor());
    });

    WatchManager::instance().setDapClient(m_dapClient);

    QTimer::singleShot(0, this, [this]() {
      if (!m_dapClient || m_dapClient->state() != DapClient::State::Stopped) {
        return;
      }

      m_expectStopEvent = false;
      m_currentThreadId = m_dapClient->currentThreadId();
      if (m_currentThreadId > 0) {
        m_dapClient->getStackTrace(m_currentThreadId, 0,
                                   MAX_STACK_FRAMES_PER_REFRESH);
      } else {
        m_dapClient->getThreads();
      }
    });

    if (m_dapClient->state() == DapClient::State::Running) {
      appendConsoleLine(
          tr("Program is running. Variables and watches appear after a "
             "breakpoint or pause. If the program is waiting for input, type "
             "in the terminal panel."),
          consoleMutedColor());
    }
  }

  updateToolbarState();
  updateSectionSummaries();
}

void DebugPanel::clearAll() {
  m_callStackTree->clear();
  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingTreeSummaries.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();
  m_programmaticVariablesExpand = false;
  m_variablesNameColumnUserSized = false;
  m_stepInProgress = false;
  m_expectStopEvent = true;
  m_hasLastStopEvent = false;
  m_lastStoppedThreadId = 0;
  m_lastStoppedReason = DapStoppedReason::Unknown;
  m_pendingConsoleEvaluations.clear();
  m_consoleExpansions.clear();
  m_consoleExpansionNodeByRef.clear();
  m_previousVariableValues.clear();
  m_consoleOutput->clear();
  m_threads.clear();
  m_stackFrames.clear();
  m_currentThreadId = 0;
  m_currentFrameId = 0;
  m_threadSelector->clear();
  m_threadSelector->setEnabled(false);
  updateSectionSummaries();
}

void DebugPanel::setCurrentFrame(int frameId) {
  m_currentFrameId = frameId;

  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_dapClient->getScopes(frameId);
  }
}

void DebugPanel::onStopped(const DapStoppedEvent &event) {
  if (!m_expectStopEvent && event.allThreadsStopped) {
    LOG_DEBUG("DebugPanel: Ignoring redundant allThreadsStopped event");
    return;
  }

  const int eventThreadId =
      event.threadId > 0 ? event.threadId : m_currentThreadId;
  const bool duplicateStop = !m_expectStopEvent && m_hasLastStopEvent &&
                             eventThreadId == m_lastStoppedThreadId &&
                             event.reason == m_lastStoppedReason;
  if (duplicateStop) {
    LOG_DEBUG("DebugPanel: Ignoring duplicate stopped event");
    return;
  }

  m_expectStopEvent = false;
  m_hasLastStopEvent = true;
  m_lastStoppedThreadId = eventThreadId;
  m_lastStoppedReason = event.reason;
  m_stepInProgress = false;

  if (event.threadId > 0) {
    m_currentThreadId = event.threadId;
  }

  QString reasonText;
  switch (event.reason) {
  case DapStoppedReason::Breakpoint:
    reasonText = tr("Breakpoint hit");
    break;
  case DapStoppedReason::Step:
    reasonText = tr("Step completed");
    break;
  case DapStoppedReason::Exception:
    if (!event.description.isEmpty()) {
      reasonText = tr("Exception: %1").arg(event.description);
    } else if (!event.text.isEmpty()) {
      reasonText = tr("Exception: %1").arg(event.text);
    } else if (!event.rawReason.isEmpty() &&
               event.rawReason.compare("exception", Qt::CaseInsensitive) != 0) {

      reasonText =
          tr("Program halted by signal (%1)").arg(event.rawReason.toUpper());
    } else {
      reasonText = tr("Exception");
    }
    break;
  case DapStoppedReason::Pause:
    reasonText = tr("Paused");
    break;
  case DapStoppedReason::Entry:
    reasonText = tr("Entry point");
    break;
  default:
    reasonText = event.description.isEmpty()
                     ? tr("Stopped")
                     : tr("Stopped: %1").arg(event.description);
  }

  appendConsoleLine(reasonText, consoleInfoColor());

  if (event.reason == DapStoppedReason::Exception && m_dapClient &&
      !m_dapClient->supportsExceptionInfoRequest()) {
    appendConsoleLine(
        tr("The debug adapter does not provide exception details; check the "
           "adapter output above for the signal or error report."),
        consoleMutedColor());
  }

  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingTreeSummaries.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();

  if (m_dapClient) {
    const bool canFastRefreshOnStep =
        event.reason == DapStoppedReason::Step && m_currentThreadId > 0;
    if (canFastRefreshOnStep) {
      m_dapClient->getStackTrace(m_currentThreadId, 0,
                                 MAX_STACK_FRAMES_PER_REFRESH);
    } else {
      m_dapClient->getThreads();
    }
  }

  updateSectionSummaries();
  updateToolbarState();
}

void DebugPanel::onContinued() {
  m_stepInProgress = false;
  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingTreeSummaries.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();
  updateSectionSummaries();
  updateToolbarState();
}

void DebugPanel::onTerminated() {
  clearAll();
  appendConsoleLine(tr("Debug session ended."), consoleMutedColor());
  updateToolbarState();
}

void DebugPanel::onContinue() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    m_expectStopEvent = true;
    m_stepInProgress = true;
    updateToolbarState();
    m_dapClient->continueExecution(activeThreadId());
  } else {
    emit startDebugRequested();
  }
}

void DebugPanel::onPause() {
  if (m_dapClient && m_dapClient->state() == DapClient::State::Running) {
    m_expectStopEvent = true;
    m_dapClient->pause(activeThreadId());
  }
}

void DebugPanel::onStepOver() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const int threadId = activeThreadId();
    if (threadId > 0) {
      m_expectStopEvent = true;
      m_stepInProgress = true;
      updateToolbarState();
      m_dapClient->stepOver(threadId);
    }
  }
}

void DebugPanel::onStepInto() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const int threadId = activeThreadId();
    if (threadId > 0) {
      m_expectStopEvent = true;
      m_stepInProgress = true;
      updateToolbarState();
      m_dapClient->stepInto(threadId);
    }
  }
}

void DebugPanel::onStepOut() {
  if (m_stepInProgress)
    return;
  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const int threadId = activeThreadId();
    if (threadId > 0) {
      m_expectStopEvent = true;
      m_stepInProgress = true;
      updateToolbarState();
      m_dapClient->stepOut(threadId);
    }
  }
}

void DebugPanel::onRestart() { emit restartDebugRequested(); }

void DebugPanel::onStop() {
  if (m_dapClient && m_dapClient->isDebugging()) {
    appendConsoleLine(tr("Stopping debug session..."), consoleMutedColor());
  }
  emit stopDebugRequested();
}

void DebugPanel::onThreadsReceived(const QList<DapThread> &threads) {
  m_threads = threads;

  bool hasCurrentThread = false;

  m_threadSelector->blockSignals(true);
  m_threadSelector->clear();
  for (const DapThread &thread : threads) {
    m_threadSelector->addItem(
        QString("Thread %1: %2").arg(thread.id).arg(thread.name), thread.id);
    if (thread.id == m_currentThreadId) {
      m_threadSelector->setCurrentIndex(m_threadSelector->count() - 1);
      hasCurrentThread = true;
    }
  }

  if (!threads.isEmpty() && !hasCurrentThread) {
    m_currentThreadId = threads.first().id;
    m_threadSelector->setCurrentIndex(0);
    hasCurrentThread = true;
  }

  m_threadSelector->setEnabled(!threads.isEmpty());
  m_threadSelector->blockSignals(false);

  if (m_dapClient && hasCurrentThread) {
    m_dapClient->getStackTrace(m_currentThreadId, 0,
                               MAX_STACK_FRAMES_PER_REFRESH);
  }
  updateSectionSummaries();
  updateToolbarState();
}

void DebugPanel::onStackTraceReceived(int threadId,
                                      const QList<DapStackFrame> &frames,
                                      int totalFrames) {
  Q_UNUSED(totalFrames);

  if (threadId != m_currentThreadId) {
    return;
  }

  m_stackFrames = frames;
  m_callStackTree->clear();

  for (const DapStackFrame &frame : frames) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, frame.name);
    item->setText(1, frame.source.name);
    // Frames without source information report line 0; a bare "0" reads as a
    // real location, so leave the cell empty instead.
    item->setText(2, frame.line > 0 ? QString::number(frame.line) : QString());
    item->setData(0, Qt::UserRole, frame.id);
    item->setData(0, Qt::UserRole + 1, frame.source.path);
    item->setData(0, Qt::UserRole + 2, frame.line);
    item->setData(0, Qt::UserRole + 3, frame.column);

    if (frame.presentationHint == "subtle") {
      item->setForeground(0, Qt::gray);
    }

    m_callStackTree->addTopLevelItem(item);
  }

  if (!frames.isEmpty()) {
    int activeIndex = 0;
    for (int i = 0; i < frames.size(); ++i) {
      const DapStackFrame &frame = frames.at(i);
      if (frame.presentationHint != "subtle" && !frame.source.path.isEmpty()) {
        activeIndex = i;
        break;
      }
    }

    const DapStackFrame &activeFrame = frames.at(activeIndex);

    m_callStackTree->blockSignals(true);
    m_callStackTree->setCurrentItem(m_callStackTree->topLevelItem(activeIndex));
    m_callStackTree->blockSignals(false);
    setCurrentFrame(activeFrame.id);

    if (!activeFrame.source.path.isEmpty()) {
      emit locationClicked(activeFrame.source.path, activeFrame.line,
                           activeFrame.column);
    }

    WatchManager::instance().evaluateAll(activeFrame.id);
  }

  // The status pill names the paused location, which is only known once the
  // frames arrive - the state change alone came too early to show it.
  updateToolbarState();
  updateSectionSummaries();
}

void DebugPanel::onScopesReceived(int frameId, const QList<DapScope> &scopes) {

  if (frameId != m_currentFrameId) {
    return;
  }

  m_variablesTree->clear();
  m_variableRefToItem.clear();
  m_pendingTreeSummaries.clear();
  m_pendingScopeVariableLoads.clear();
  m_pendingVariableRequests.clear();
  clearLocalsFallbackState();

  QList<int> eagerScopeRefs;
  for (const DapScope &scope : scopes) {
    if (scope.variablesReference <= 0 || scope.expensive) {
      continue;
    }
    const QString lowered = scope.name.trimmed().toLower();
    if (lowered.contains("register") || lowered.contains("local")) {
      continue;
    }

    if (lowered.contains("argument") || lowered == QLatin1String("args")) {
      eagerScopeRefs.append(scope.variablesReference);
      break;
    }
  }
  if (eagerScopeRefs.isEmpty()) {
    for (const DapScope &scope : scopes) {
      if (scope.variablesReference <= 0 || scope.expensive) {
        continue;
      }
      const QString lowered = scope.name.trimmed().toLower();
      if (lowered.contains("register") || lowered.contains("local")) {
        continue;
      }
      eagerScopeRefs.append(scope.variablesReference);
      break;
    }
  }
  int eagerLoadsRemaining = MAX_EAGER_SCOPE_LOADS;
  const bool preferLocalsFallback = hasLocalsFallbackCommand();

  for (const DapScope &scope : scopes) {
    const QString lowered = scope.name.trimmed().toLower();
    const bool registerScope = lowered.contains("register");
    const bool localScope = lowered.contains("local");
    const bool autoLoadScope =
        eagerScopeRefs.contains(scope.variablesReference) &&
        eagerLoadsRemaining > 0;
    const bool shouldLoadLocals = localScope && !scope.expensive &&
                                  !registerScope && !preferLocalsFallback;
    const bool shouldLoadScopeVariables =
        shouldLoadLocals ||
        (!scope.expensive && !registerScope && !localScope && autoLoadScope);
    const bool deferScope = !shouldLoadScopeVariables;

    QTreeWidgetItem *scopeItem = new QTreeWidgetItem();
    scopeItem->setText(0, scope.name);
    scopeItem->setData(0, Qt::UserRole, scope.variablesReference);
    scopeItem->setFirstColumnSpanned(true);

    QFont font = scopeItem->font(0);
    font.setBold(true);
    scopeItem->setFont(0, font);

    if (scope.variablesReference > 0 &&
        (localScope || shouldLoadScopeVariables)) {
      scopeItem->setExpanded(true);
    }

    m_variablesTree->addTopLevelItem(scopeItem);

    if (scope.variablesReference > 0) {
      if (localScope && preferLocalsFallback && !scope.expensive &&
          !registerScope) {
        m_variableRefToItem[scope.variablesReference] = scopeItem;
        scopeItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        requestLocalsFallback(scope.variablesReference);
      } else if (shouldLoadScopeVariables) {
        m_variableRefToItem[scope.variablesReference] = scopeItem;
        m_pendingScopeVariableLoads.insert(scope.variablesReference);
        m_pendingVariableRequests.insert(scope.variablesReference);
        m_dapClient->getVariables(scope.variablesReference);
        if (!localScope) {
          --eagerLoadsRemaining;
        }
      } else if (deferScope) {
        scopeItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
      }
    }
  }

  if (m_pendingScopeVariableLoads.isEmpty()) {
    fitVariablesNameColumn();
  }
  updateSectionSummaries();
}

void DebugPanel::onVariablesReceived(int variablesReference,
                                     const QList<DapVariable> &variables) {
  m_pendingVariableRequests.remove(variablesReference);

  applyConsoleExpansion(variablesReference, variables);

  const auto summaryFor = m_pendingTreeSummaries.find(variablesReference);
  if (summaryFor != m_pendingTreeSummaries.end()) {
    QTreeWidgetItem *aggregateItem = summaryFor.value();
    m_pendingTreeSummaries.erase(summaryFor);
    aggregateItem->setText(1, previewForVariables(variables));
  }

  QTreeWidgetItem *parentItem = m_variableRefToItem.value(variablesReference);
  if (!parentItem) {
    return;
  }

  // Any preview still in flight may point at a row about to be freed.
  // QTreeWidgetItem is not a QObject, so there is no guarded pointer to lean
  // on - drop the pending previews rather than risk a dangling write.
  m_pendingTreeSummaries.clear();

  while (parentItem->childCount() > 0) {
    delete parentItem->takeChild(0);
  }

  for (const DapVariable &var : variables) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, var.name);
    item->setText(1, var.value);
    item->setText(2, var.type);
    item->setData(0, Qt::UserRole, var.variablesReference);
    item->setData(0, Qt::UserRole + 1, var.evaluateName);
    item->setIcon(0, variableIcon(var));

    // A value that moved since the previous stop is what the user stepped to
    // find, so mark it - on the value cell only, and only until the next stop.
    QString path = var.name;
    for (QTreeWidgetItem *ancestor = parentItem; ancestor;
         ancestor = ancestor->parent()) {
      path.prepend(ancestor->text(0) + QLatin1Char('/'));
    }
    const auto previous = m_previousVariableValues.constFind(path);
    if (previous != m_previousVariableValues.constEnd() &&
        previous.value() != var.value) {
      item->setForeground(1, m_themeInitialized ? m_theme.warningColor
                                                : item->foreground(1));
    }
    m_previousVariableValues.insert(path, var.value);

    // Columns elide; the tooltip keeps the whole value and type reachable.
    QStringList tip;
    tip << var.name;
    if (!var.value.isEmpty()) {
      tip << var.value;
    }
    if (!var.type.isEmpty()) {
      tip << QStringLiteral("(%1)").arg(var.type);
    }
    const QString tipText = tip.join(QLatin1Char('\n'));
    for (int column = 0; column < 3; ++column) {
      item->setToolTip(column, tipText);
    }

    if (var.variablesReference > 0) {
      item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    parentItem->addChild(item);
  }

  requestAggregatePreviews(variables, parentItem);

  bool requestedLocalFallback = false;
  if (variables.isEmpty() && !parentItem->parent()) {
    const QString scopeName = parentItem->text(0).trimmed().toLower();
    if (scopeName.contains(QLatin1String("local")) &&
        hasLocalsFallbackCommand()) {
      requestLocalsFallback(variablesReference);
      requestedLocalFallback = true;
    }
  }

  if (!requestedLocalFallback) {
    parentItem->setExpanded(true);
  }

  m_pendingScopeVariableLoads.remove(variablesReference);
  // Names get longer as the user drills into a value, so refit after every
  // batch instead of only once per stop - until the column is sized by hand.
  QTimer::singleShot(0, this, [this]() { fitVariablesNameColumn(); });
  updateSectionSummaries();
}

void DebugPanel::requestLocalsFallback(int scopeVariablesReference) {
  if (scopeVariablesReference <= 0) {
    return;
  }

  QTreeWidgetItem *scopeItem =
      m_variableRefToItem.value(scopeVariablesReference);
  if (!scopeItem) {
    return;
  }

  if (!m_dapClient || m_dapClient->state() != DapClient::State::Stopped ||
      m_currentFrameId < 0) {
    showLocalsFallbackMessage(scopeVariablesReference,
                              tr("<locals unavailable in current state>"));
    return;
  }

  if (m_localsFallbackPending) {
    return;
  }

  while (scopeItem->childCount() > 0) {
    delete scopeItem->takeChild(0);
  }

  QTreeWidgetItem *loadingItem = new QTreeWidgetItem();
  loadingItem->setText(0, tr("<loading locals...>"));
  loadingItem->setFirstColumnSpanned(true);
  loadingItem->setForeground(0, consoleMutedColor());
  scopeItem->addChild(loadingItem);
  scopeItem->setExpanded(true);

  const DebugEvaluateRequest localsRequest =
      DebugExpressionTranslator::localsFallbackRequest(
          m_dapClient ? m_dapClient->adapterId() : QString(),
          m_dapClient ? m_dapClient->adapterType() : QString());
  const QString baseExpr = localsRequest.expression.trimmed();
  if (baseExpr.isEmpty()) {
    showLocalsFallbackMessage(scopeVariablesReference,
                              tr("<locals fallback unavailable for debugger>"));
    return;
  }

  const QString evalContext = localsRequest.context.isEmpty()
                                  ? QStringLiteral("repl")
                                  : localsRequest.context;

  m_localsFallbackPending = true;
  m_localsFallbackFrameId = m_currentFrameId;
  m_localsFallbackScopeRef = scopeVariablesReference;
  m_localsFallbackRequestSeq =
      m_dapClient->evaluate(baseExpr, m_currentFrameId, evalContext);
}

bool DebugPanel::hasLocalsFallbackCommand() const {
  if (!m_dapClient) {
    return false;
  }

  const DebugEvaluateRequest localsRequest =
      DebugExpressionTranslator::localsFallbackRequest(
          m_dapClient->adapterId(), m_dapClient->adapterType());
  return !localsRequest.expression.trimmed().isEmpty();
}

void DebugPanel::populateLocalsFromGdbEvaluate(int scopeVariablesReference,
                                               const QString &rawResult) {
  QTreeWidgetItem *scopeItem =
      m_variableRefToItem.value(scopeVariablesReference);
  if (!scopeItem) {
    return;
  }

  while (scopeItem->childCount() > 0) {
    delete scopeItem->takeChild(0);
  }

  const QList<QPair<QString, QString>> entries =
      parseInfoLocalsOutput(rawResult);
  if (entries.isEmpty()) {
    showLocalsFallbackMessage(scopeVariablesReference,
                              tr("<no locals available at this location>"));
    return;
  }

  for (const auto &entry : entries) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, entry.first);
    item->setText(1, entry.second);
    item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    scopeItem->addChild(item);
  }

  scopeItem->setExpanded(true);
  QTimer::singleShot(0, this, [this]() { fitVariablesNameColumn(); });
}

void DebugPanel::showLocalsFallbackMessage(int scopeVariablesReference,
                                           const QString &message,
                                           bool isError) {
  QTreeWidgetItem *scopeItem =
      m_variableRefToItem.value(scopeVariablesReference);
  if (!scopeItem) {
    return;
  }

  while (scopeItem->childCount() > 0) {
    delete scopeItem->takeChild(0);
  }

  QTreeWidgetItem *hintItem = new QTreeWidgetItem();
  hintItem->setText(0, message);
  hintItem->setFirstColumnSpanned(true);
  hintItem->setForeground(0,
                          isError ? consoleErrorColor() : consoleMutedColor());
  scopeItem->addChild(hintItem);
  scopeItem->setExpanded(true);
}

void DebugPanel::clearLocalsFallbackState() {
  m_localsFallbackPending = false;
  m_localsFallbackFrameId = -1;
  m_localsFallbackScopeRef = 0;
  m_localsFallbackRequestSeq = 0;
}

void DebugPanel::onOutputReceived(const DapOutputEvent &event) {
  QColor color = palette().color(QPalette::Text);
  if (event.category == "stderr") {
    color = consoleErrorColor();
  } else if (event.category == "important") {
    color = consoleInfoColor();
  }
  appendConsoleLine(event.output, color);
}

void DebugPanel::onExceptionInfoReceived(int threadId,
                                         const DapExceptionInfo &info) {
  if (m_currentThreadId > 0 && threadId > 0 && threadId != m_currentThreadId) {
    return;
  }

  QString title = info.exceptionId;

  const int noteIndex = title.indexOf(QStringLiteral(" (note:"));
  if (noteIndex > 0) {
    title.truncate(noteIndex);
  }
  appendConsoleLine(QString("%1: %2").arg(title, info.details.message.isEmpty()
                                                     ? info.description
                                                     : info.details.message),
                    consoleErrorColor(), true);

  const QString stackTrace = info.details.stackTrace.trimmed();
  if (!stackTrace.isEmpty()) {
    const QStringList lines = stackTrace.split('\n');
    const int maxLines = 30;
    for (int i = 0; i < lines.size() && i < maxLines; ++i) {
      appendConsoleLine(lines[i], consoleErrorColor());
    }
    if (lines.size() > maxLines) {
      appendConsoleLine(tr("… %1 more frames").arg(lines.size() - maxLines),
                        consoleMutedColor());
    }
  }

  for (const DapExceptionDetails &inner : info.details.innerExceptions) {
    appendConsoleLine(
        tr("Caused by %1: %2")
            .arg(inner.typeName.isEmpty() ? tr("exception") : inner.typeName,
                 inner.message),
        consoleErrorColor());
  }

  if (!info.details.typeName.isEmpty() && !info.exceptionId.isEmpty() &&
      info.details.typeName != info.exceptionId) {
    appendConsoleLine(tr("Type: %1").arg(info.details.typeName),
                      consoleMutedColor());
  }
}

void DebugPanel::onCallStackItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item) {
    return;
  }

  int frameId = item->data(0, Qt::UserRole).toInt();
  QString filePath = item->data(0, Qt::UserRole + 1).toString();
  int line = item->data(0, Qt::UserRole + 2).toInt();
  int col = item->data(0, Qt::UserRole + 3).toInt();

  setCurrentFrame(frameId);
  WatchManager::instance().evaluateAll(frameId);

  if (!filePath.isEmpty()) {
    emit locationClicked(filePath, line, col);
  }
}

void DebugPanel::onVariableItemExpanded(QTreeWidgetItem *item) {
  if (!item || m_programmaticVariablesExpand) {
    return;
  }

  if (!item->parent()) {
    const QString scopeName = item->text(0).trimmed().toLower();
    if (scopeName.contains(QLatin1String("local"))) {
      const int localScopeRef = item->data(0, Qt::UserRole).toInt();
      if (localScopeRef > 0 && item->childCount() == 0) {
        if (hasLocalsFallbackCommand()) {
          requestLocalsFallback(localScopeRef);
        } else if (m_dapClient &&
                   !m_pendingVariableRequests.contains(localScopeRef)) {
          m_variableRefToItem[localScopeRef] = item;
          m_pendingVariableRequests.insert(localScopeRef);
          m_dapClient->getVariables(localScopeRef);
        }
      }
      return;
    }
  }

  int varRef = item->data(0, Qt::UserRole).toInt();

  if (varRef > 0 && item->childCount() == 0 && m_dapClient &&
      !m_pendingVariableRequests.contains(varRef)) {
    m_variableRefToItem[varRef] = item;
    m_pendingVariableRequests.insert(varRef);
    m_dapClient->getVariables(varRef);
  }
}

void DebugPanel::onBreakpointItemDoubleClicked(QTreeWidgetItem *item,
                                               int column) {
  Q_UNUSED(column);

  if (!item) {
    return;
  }

  const QString kind = item->data(0, Qt::UserRole + 4).toString();
  if (kind == QLatin1String("function") || kind == QLatin1String("exception") ||
      kind == QLatin1String("data")) {
    return;
  }

  QString filePath = item->data(0, Qt::UserRole).toString();
  int line = item->data(0, Qt::UserRole + 1).toInt();

  if (!filePath.isEmpty()) {
    emit locationClicked(filePath, line, 0);
  }
}

void DebugPanel::onBreakpointItemChanged(QTreeWidgetItem *item, int column) {
  if (!item || column != 0) {
    return;
  }

  const QString kind = item->data(0, Qt::UserRole + 4).toString();
  if (kind == QLatin1String("function")) {
    const int functionBreakpointId = item->data(0, Qt::UserRole + 2).toInt();
    if (functionBreakpointId <= 0) {
      return;
    }
    BreakpointManager::instance().setFunctionBreakpointEnabled(
        functionBreakpointId, item->checkState(0) == Qt::Checked);
    return;
  }

  if (kind == QLatin1String("exception")) {
    const QString filterId = item->data(0, Qt::UserRole + 5).toString();
    if (filterId.isEmpty()) {
      return;
    }
    QStringList filters =
        BreakpointManager::instance().enabledExceptionFilters();
    if (item->checkState(0) == Qt::Checked) {
      if (!filters.contains(filterId)) {
        filters.append(filterId);
      }
    } else {
      filters.removeAll(filterId);
    }
    BreakpointManager::instance().setExceptionBreakpoints(filters);
    return;
  }

  const int breakpointId = item->data(0, Qt::UserRole + 2).toInt();
  if (breakpointId <= 0) {
    return;
  }

  const bool enabled = item->checkState(0) == Qt::Checked;
  Breakpoint breakpoint =
      BreakpointManager::instance().breakpoint(breakpointId);
  if (breakpoint.id <= 0 || breakpoint.enabled == enabled) {
    return;
  }

  BreakpointManager::instance().setEnabled(breakpointId, enabled);
}

void DebugPanel::onBreakpointsContextMenuRequested(const QPoint &pos) {
  QTreeWidgetItem *item = m_breakpointsTree->itemAt(pos);
  QMenu menu(this);

  QAction *clearAllAction = nullptr;
  if (!item) {
    QAction *addFunctionAction =
        menu.addAction(tr("Add Function Breakpoint..."));
    clearAllAction = menu.addAction(tr("Clear All Breakpoints"));
    QAction *chosen = menu.exec(m_breakpointsTree->mapToGlobal(pos));
    if (chosen == addFunctionAction && m_addFunctionBreakpointButton) {
      m_addFunctionBreakpointButton->click();
    } else if (chosen == clearAllAction) {
      BreakpointManager::instance().clearAll();
    }
    return;
  } else {
    const QString kind = item->data(0, Qt::UserRole + 4).toString();
    const int breakpointId = item->data(0, Qt::UserRole + 2).toInt();
    if (kind == QLatin1String("function")) {
      const QList<FunctionBreakpoint> functionBreakpoints =
          BreakpointManager::instance().allFunctionBreakpoints();
      FunctionBreakpoint breakpoint;
      for (const FunctionBreakpoint &candidate : functionBreakpoints) {
        if (candidate.id == breakpointId) {
          breakpoint = candidate;
          break;
        }
      }
      if (breakpoint.id <= 0) {
        return;
      }

      QAction *toggleEnabledAction =
          menu.addAction(breakpoint.enabled ? tr("Disable Function Breakpoint")
                                            : tr("Enable Function Breakpoint"));
      QAction *removeAction = menu.addAction(tr("Remove Function Breakpoint"));
      QAction *chosen = menu.exec(m_breakpointsTree->mapToGlobal(pos));
      if (chosen == toggleEnabledAction) {
        BreakpointManager::instance().setFunctionBreakpointEnabled(
            breakpointId, !breakpoint.enabled);
      } else if (chosen == removeAction) {
        BreakpointManager::instance().removeFunctionBreakpoint(breakpointId);
      }
      return;
    }

    if (kind == QLatin1String("exception")) {
      const QString filterId = item->data(0, Qt::UserRole + 5).toString();
      if (filterId.isEmpty()) {
        return;
      }
      const bool enabled = item->checkState(0) == Qt::Checked;
      QAction *toggleEnabledAction =
          menu.addAction(enabled ? tr("Disable Exception Breakpoint")
                                 : tr("Enable Exception Breakpoint"));
      QAction *chosen = menu.exec(m_breakpointsTree->mapToGlobal(pos));
      if (chosen == toggleEnabledAction) {
        item->setCheckState(0, enabled ? Qt::Unchecked : Qt::Checked);
      }
      return;
    }

    Breakpoint breakpoint =
        BreakpointManager::instance().breakpoint(breakpointId);
    if (breakpoint.id <= 0) {
      return;
    }

    QAction *goToAction = menu.addAction(tr("Go to Source"));
    QAction *toggleEnabledAction =
        menu.addAction(breakpoint.enabled ? tr("Disable Breakpoint")
                                          : tr("Enable Breakpoint"));
    menu.addSeparator();
    QAction *editConditionAction = menu.addAction(tr("Edit Condition..."));
    QAction *editHitConditionAction = menu.addAction(tr("Edit Hit Count..."));
    QAction *editLogpointAction = menu.addAction(tr("Edit Logpoint..."));
    QAction *clearMetadataAction =
        menu.addAction(tr("Clear Condition and Logpoint"));
    menu.addSeparator();
    QAction *removeAction = menu.addAction(tr("Remove Breakpoint"));

    QAction *chosen = menu.exec(m_breakpointsTree->mapToGlobal(pos));
    if (!chosen) {
      return;
    }

    if (chosen == goToAction) {
      emit locationClicked(breakpoint.filePath, breakpoint.line,
                           breakpoint.column);
      return;
    }

    if (chosen == toggleEnabledAction) {
      BreakpointManager::instance().setEnabled(breakpointId,
                                               !breakpoint.enabled);
      return;
    }

    if (chosen == editConditionAction) {
      bool ok = false;
      const QString condition =
          QInputDialog::getText(this, tr("Breakpoint Condition"),
                                tr("Pause when expression is true:"),
                                QLineEdit::Normal, breakpoint.condition, &ok);
      if (ok) {
        BreakpointManager::instance().setCondition(breakpointId,
                                                   condition.trimmed());
      }
      return;
    }

    if (chosen == editHitConditionAction) {
      bool ok = false;
      const QString hitCondition = QInputDialog::getText(
          this, tr("Breakpoint Hit Count"),
          tr("Break after this hit count or expression:"), QLineEdit::Normal,
          breakpoint.hitCondition, &ok);
      if (ok) {
        BreakpointManager::instance().setHitCondition(breakpointId,
                                                      hitCondition.trimmed());
      }
      return;
    }

    if (chosen == editLogpointAction) {
      bool ok = false;
      const QString logMessage =
          QInputDialog::getText(this, tr("Breakpoint Logpoint"),
                                tr("Log message (use braces for expressions):"),
                                QLineEdit::Normal, breakpoint.logMessage, &ok);
      if (ok) {
        BreakpointManager::instance().setLogMessage(breakpointId,
                                                    logMessage.trimmed());
      }
      return;
    }

    if (chosen == clearMetadataAction) {
      BreakpointManager::instance().setCondition(breakpointId, QString());
      BreakpointManager::instance().setHitCondition(breakpointId, QString());
      BreakpointManager::instance().setLogMessage(breakpointId, QString());
      return;
    }

    if (chosen == removeAction) {
      BreakpointManager::instance().removeBreakpoint(breakpointId);
      return;
    }

    return;
  }
}

void DebugPanel::onConsoleInput() {
  QString expr = m_consoleInput->text().trimmed();
  if (expr.isEmpty()) {
    return;
  }

  if (m_consoleHistory.isEmpty() || m_consoleHistory.last() != expr) {
    m_consoleHistory.append(expr);
  }
  m_consoleHistoryIndex = m_consoleHistory.size();
  m_consoleHistoryDraft.clear();

  m_consoleInput->clear();
  // Evaluating usually means evaluating again with a small edit, so the input
  // keeps focus rather than making the user click back into it every time.
  m_consoleInput->setFocus();

  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped) {
    const QList<DebugEvaluateRequest> attempts =
        DebugExpressionTranslator::buildConsoleEvaluationPlan(
            expr, m_dapClient->adapterId(), m_dapClient->adapterType());
    if (attempts.isEmpty()) {
      appendConsoleLine(tr("Cannot evaluate: expression is empty"),
                        consoleErrorColor());
      return;
    }

    appendConsoleLine(QString("> %1").arg(expr), consoleMutedColor());
    PendingConsoleEvaluation pending;
    pending.userExpression = expr;
    pending.attempts = attempts;
    pending.activeAttemptIndex = 0;
    pending.activeRequestSeq = 0;
    m_pendingConsoleEvaluations.append(pending);
    dispatchPendingConsoleEvaluation(m_pendingConsoleEvaluations.size() - 1);
  } else {
    appendConsoleLine(QString("> %1").arg(expr), consoleMutedColor());
    appendConsoleLine(tr("Cannot evaluate: not stopped at breakpoint"),
                      consoleErrorColor());
  }
}

void DebugPanel::updateToolbarState() {
  DapClient::State state =
      m_dapClient ? m_dapClient->state() : DapClient::State::Disconnected;

  const bool isDebugging = (state == DapClient::State::Running ||
                            state == DapClient::State::Stopped);
  const bool isStopped = (state == DapClient::State::Stopped &&
                          activeThreadId() > 0 && !m_stepInProgress);
  const bool isRunning =
      (state == DapClient::State::Running || m_stepInProgress);
  const bool isStarting = state == DapClient::State::Connecting ||
                          state == DapClient::State::Initializing;
  const bool canStart = !isRunning && !isStarting;
  const bool canStop = isDebugging || isStarting;

  m_continueAction->setEnabled(canStart);
  m_pauseAction->setEnabled(isRunning && isDebugging);
  m_stepOverAction->setEnabled(isStopped);
  m_stepIntoAction->setEnabled(isStopped);
  m_stepOutAction->setEnabled(isStopped);
  m_restartAction->setEnabled(isDebugging);
  m_stopAction->setEnabled(canStop);

  if (isDebugging) {
    m_continueAction->setText(tr("Continue"));
    m_continueAction->setToolTip(tr("Continue execution (F5)"));
    m_continueAction->setStatusTip(tr("Continue execution (F5)"));
  } else {
    m_continueAction->setText(tr("Start"));
    m_continueAction->setToolTip(tr("Start debugging current file (F5)"));
    m_continueAction->setStatusTip(tr("Start debugging current file (F5)"));
  }

  if (m_debugStatusLabel) {
    QString statusText;
    QString statusDetail;
    QString statusKind = QStringLiteral("ready");
    switch (state) {
    case DapClient::State::Disconnected:
    case DapClient::State::Ready:
    case DapClient::State::Terminated:
      statusText = tr("Ready");
      statusKind = QStringLiteral("ready");
      break;
    case DapClient::State::Connecting:
    case DapClient::State::Initializing:
      statusText = tr("Starting debugger");
      statusKind = QStringLiteral("starting");
      break;
    case DapClient::State::Running:
      statusText = m_stepInProgress ? tr("Stepping") : tr("Running");
      statusKind = QStringLiteral("running");
      break;
    case DapClient::State::Stopped: {

      // The pill is narrow, so lead with the location: it is what the user
      // needs at a glance, and the function name lives in the tooltip.
      QString location;
      for (const DapStackFrame &frame : m_stackFrames) {
        if (!frame.source.name.isEmpty()) {
          location =
              tr("Paused · %1:%2").arg(frame.source.name).arg(frame.line);
          statusDetail = tr("Paused at %1 · %2:%3")
                             .arg(frame.name, frame.source.name)
                             .arg(frame.line);
          break;
        }
      }
      statusText = location.isEmpty() ? tr("Paused") : location;
      statusKind = QStringLiteral("paused");
      break;
    }
    case DapClient::State::Error:
      statusText = tr("Debugger error");
      statusKind = QStringLiteral("error");
      break;
    }
    m_debugStatusText = statusText;
    applyDebugStatusText();
    m_debugStatusLabel->setToolTip(statusDetail.isEmpty() ? statusText
                                                          : statusDetail);
    if (m_debugStatusLabel->property("statusKind").toString() != statusKind) {
      m_debugStatusLabel->setProperty("statusKind", statusKind);
      m_debugStatusLabel->style()->unpolish(m_debugStatusLabel);
      m_debugStatusLabel->style()->polish(m_debugStatusLabel);
      m_debugStatusLabel->update();
    }
  }

  updateConsoleInputAffordances();
}

int DebugPanel::activeThreadId() const {
  if (m_currentThreadId > 0) {
    return m_currentThreadId;
  }
  if (!m_threads.isEmpty()) {
    return m_threads.first().id;
  }
  return 0;
}

void DebugPanel::refreshBreakpointList() {
  const QSignalBlocker blocker(m_breakpointsTree);
  m_breakpointsTree->clear();

  QList<Breakpoint> breakpoints =
      BreakpointManager::instance().allBreakpoints();

  for (const Breakpoint &bp : breakpoints) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable |
                   Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    item->setCheckState(0, bp.enabled ? Qt::Checked : Qt::Unchecked);

    QFileInfo fi(bp.filePath);
    QString location = QString("%1:%2").arg(fi.fileName()).arg(bp.line);
    item->setText(1, location);
    item->setToolTip(1, bp.filePath);

    QStringList details;
    if (bp.isLogpoint && !bp.logMessage.isEmpty()) {
      details.append(QString("log: %1").arg(bp.logMessage));
    }
    if (!bp.condition.isEmpty()) {
      details.append(QString("if %1").arg(bp.condition));
    }
    if (!bp.hitCondition.isEmpty()) {
      details.append(QString("hit: %1").arg(bp.hitCondition));
    }
    item->setText(2, details.join(" | "));

    item->setData(0, Qt::UserRole, bp.filePath);
    item->setData(0, Qt::UserRole + 1, bp.line);
    item->setData(0, Qt::UserRole + 2, bp.id);
    item->setData(0, Qt::UserRole + 4, QStringLiteral("source"));

    if (bp.verified) {
      item->setIcon(0, style()->standardIcon(QStyle::SP_DialogApplyButton));
    } else if (!bp.verificationMessage.isEmpty()) {
      item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
      item->setToolTip(0, bp.verificationMessage);
    }

    if (!bp.verificationMessage.isEmpty()) {
      item->setToolTip(2, bp.verificationMessage);
    }

    m_breakpointsTree->addTopLevelItem(item);
  }

  const QList<FunctionBreakpoint> functionBreakpoints =
      BreakpointManager::instance().allFunctionBreakpoints();
  for (const FunctionBreakpoint &bp : functionBreakpoints) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable |
                   Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item->setCheckState(0, bp.enabled ? Qt::Checked : Qt::Unchecked);
    item->setText(1, QString("ƒ %1").arg(bp.functionName));
    if (!bp.condition.isEmpty() || !bp.hitCondition.isEmpty()) {
      QStringList details;
      if (!bp.condition.isEmpty()) {
        details.append(QString("if %1").arg(bp.condition));
      }
      if (!bp.hitCondition.isEmpty()) {
        details.append(QString("hit: %1").arg(bp.hitCondition));
      }
      item->setText(2, details.join(" | "));
    }
    item->setData(0, Qt::UserRole + 2, bp.id);
    item->setData(0, Qt::UserRole + 4, QStringLiteral("function"));
    item->setIcon(0, style()->standardIcon(QStyle::SP_CommandLink));
    m_breakpointsTree->addTopLevelItem(item);
  }

  const QJsonArray exceptionFilters = currentExceptionBreakpointFilters();
  const QStringList enabledExceptionFilters =
      BreakpointManager::instance().enabledExceptionFilters();
  for (const QJsonValue &filterValue : exceptionFilters) {
    const QJsonObject filter = filterValue.toObject();
    const QString filterId = filter["filter"].toString();
    if (filterId.isEmpty()) {
      continue;
    }

    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable |
                   Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item->setCheckState(0, enabledExceptionFilters.contains(filterId)
                               ? Qt::Checked
                               : Qt::Unchecked);
    item->setText(1, filter["label"].toString(filterId));
    if (filter["supportsCondition"].toBool()) {
      item->setText(2, tr("exception filter"));
    }
    item->setData(0, Qt::UserRole + 4, QStringLiteral("exception"));
    item->setData(0, Qt::UserRole + 5, filterId);
    item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    m_breakpointsTree->addTopLevelItem(item);
  }

  updateSectionSummaries();
}

void DebugPanel::onThreadSelected(int index) {
  if (index < 0 || !m_dapClient)
    return;

  int threadId = m_threadSelector->itemData(index).toInt();
  if (threadId > 0 && threadId != m_currentThreadId) {
    m_currentThreadId = threadId;
    m_dapClient->getStackTrace(m_currentThreadId, 0,
                               MAX_STACK_FRAMES_PER_REFRESH);
  }
}

void DebugPanel::onAddWatch() {
  QString expr = m_watchInput->text().trimmed();
  if (expr.isEmpty())
    return;

  m_watchInput->clear();
  int id = WatchManager::instance().addWatch(expr);

  if (m_dapClient && m_dapClient->state() == DapClient::State::Stopped &&
      m_currentFrameId > 0) {
    WatchManager::instance().evaluateWatch(id, m_currentFrameId);
  }
}

void DebugPanel::onRemoveWatch() {
  QTreeWidgetItem *item = m_watchTree->currentItem();
  if (!item || item->parent())
    return;

  int watchId = item->data(0, Qt::UserRole).toInt();
  WatchManager::instance().removeWatch(watchId);
}

void DebugPanel::onWatchAdded(const WatchExpression &watch) {
  QTreeWidgetItem *item = new QTreeWidgetItem();
  item->setText(0, watch.expression);
  item->setText(1, watch.value.isEmpty() ? tr("<not evaluated>") : watch.value);
  item->setText(2, watch.type);
  item->setData(0, Qt::UserRole, watch.id);
  item->setData(0, Qt::UserRole + 1, watch.variablesReference);

  if (watch.variablesReference > 0) {
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    item->setExpanded(true);
  }

  m_watchTree->addTopLevelItem(item);
  m_watchIdToItem[watch.id] = item;
  updateSectionSummaries();
}

void DebugPanel::onWatchRemoved(int id) {
  QTreeWidgetItem *item = m_watchIdToItem.take(id);
  if (item) {
    delete item;
  }
  updateSectionSummaries();
}

void DebugPanel::onWatchUpdated(const WatchExpression &watch) {
  QTreeWidgetItem *item = m_watchIdToItem.value(watch.id);
  if (!item)
    return;

  const bool wasExpanded = item->isExpanded();
  if (watch.isError) {
    item->setText(1, watch.errorMessage.isEmpty() ? tr("Evaluation failed")
                                                  : watch.errorMessage);
    item->setForeground(1, m_themeInitialized ? m_theme.errorColor
                                              : QColor(Qt::red));
  } else {
    item->setText(1, watch.value);
    item->setForeground(1, palette().color(QPalette::Text));
  }
  item->setText(2, watch.type);
  item->setData(0, Qt::UserRole + 1, watch.variablesReference);

  if (watch.variablesReference > 0) {
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    item->setExpanded(wasExpanded || item->childCount() > 0);
  } else {
    item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

    while (item->childCount() > 0) {
      delete item->takeChild(0);
    }
  }
  updateSectionSummaries();
}

void DebugPanel::onWatchItemExpanded(QTreeWidgetItem *item) {

  if (!item || item->parent())
    return;

  int watchId = item->data(0, Qt::UserRole).toInt();
  int varRef = item->data(0, Qt::UserRole + 1).toInt();

  if (varRef > 0 && item->childCount() == 0) {
    WatchManager::instance().getWatchChildren(watchId, varRef);
  }
}

void DebugPanel::onWatchChildrenReceived(int watchId,
                                         const QList<DapVariable> &children) {
  QTreeWidgetItem *parentItem = m_watchIdToItem.value(watchId);
  if (!parentItem)
    return;

  while (parentItem->childCount() > 0) {
    delete parentItem->takeChild(0);
  }

  for (const DapVariable &var : children) {
    QTreeWidgetItem *childItem = new QTreeWidgetItem();
    childItem->setText(0, var.name);
    childItem->setText(1, var.value);
    childItem->setText(2, var.type);
    childItem->setData(0, Qt::UserRole + 1, var.variablesReference);
    childItem->setIcon(0, variableIcon(var));

    if (var.variablesReference > 0) {
      childItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    parentItem->addChild(childItem);
  }

  parentItem->setExpanded(true);
}

void DebugPanel::onEvaluateResult(int requestSeq, const QString &expression,
                                  const QString &result, const QString &type,
                                  int variablesReference) {
  if (m_localsFallbackPending && requestSeq == m_localsFallbackRequestSeq) {
    const int scopeRef = m_localsFallbackScopeRef;
    const bool staleFrame = m_localsFallbackFrameId != m_currentFrameId;
    clearLocalsFallbackState();
    if (!staleFrame) {
      populateLocalsFromGdbEvaluate(scopeRef, result);
    }
    return;
  }

  const int pendingIndex = findPendingConsoleEvaluationIndex(requestSeq);
  QString displayExpression = expression;
  if (pendingIndex >= 0) {
    displayExpression =
        m_pendingConsoleEvaluations.at(pendingIndex).userExpression;
    m_pendingConsoleEvaluations.removeAt(pendingIndex);
  }

  // Structured values (a struct, a container, a QString) come back with an
  // empty result and a reference to their children. Printing "expr = (Type)"
  // tells the user nothing, so fetch the children and summarise them.
  if (result.trimmed().isEmpty() && variablesReference > 0 && m_dapClient) {
    startConsoleExpansion(displayExpression, type, variablesReference);
    return;
  }

  QString line = QString("%1 = %2").arg(displayExpression, result);
  if (!type.isEmpty()) {
    line += QString(" (%1)").arg(type);
  }
  appendConsoleLine(line, palette().color(QPalette::Text), true);
}

void DebugPanel::startConsoleExpansion(const QString &expression,
                                       const QString &type,
                                       int variablesReference) {
  const int id = m_nextConsoleExpansionId++;

  ConsoleExpansion expansion;
  expansion.expression = expression;
  expansion.type = type;

  ConsoleExpansionNode root;
  root.name = expression;
  root.reference = variablesReference;
  expansion.nodes.append(root);
  expansion.pendingRequests = 1;

  m_consoleExpansions.insert(id, expansion);
  m_consoleExpansionNodeByRef.insert(variablesReference, {id, 0});
  m_dapClient->getVariables(variablesReference);
}

bool DebugPanel::applyConsoleExpansion(int variablesReference,
                                       const QList<DapVariable> &variables) {
  // Deep enough to render nested wrappers such as QVector3D's float array,
  // capped so a large container cannot flood the adapter with requests.
  constexpr int kMaxDepth = 3;
  constexpr int kMaxNodes = 64;

  const auto located = m_consoleExpansionNodeByRef.find(variablesReference);
  if (located == m_consoleExpansionNodeByRef.end()) {
    return false;
  }
  const int expansionId = located.value().first;
  const int nodeIndex = located.value().second;
  m_consoleExpansionNodeByRef.erase(located);

  const auto found = m_consoleExpansions.find(expansionId);
  if (found == m_consoleExpansions.end()) {
    return false;
  }
  ConsoleExpansion &expansion = found.value();
  expansion.pendingRequests = qMax(0, expansion.pendingRequests - 1);

  const int childDepth = expansion.nodes.at(nodeIndex).depth + 1;
  for (const DapVariable &var : variables) {
    if (expansion.nodes.size() >= kMaxNodes) {
      break;
    }
    ConsoleExpansionNode child;
    child.name = var.name;
    child.value = var.value.trimmed();
    child.reference = var.variablesReference;
    child.depth = childDepth;
    expansion.nodes.append(child);
    const int childIndex = expansion.nodes.size() - 1;
    expansion.nodes[nodeIndex].children.append(childIndex);

    if (child.value.isEmpty() && child.reference > 0 &&
        childDepth < kMaxDepth && m_dapClient &&
        !m_consoleExpansionNodeByRef.contains(child.reference)) {
      m_consoleExpansionNodeByRef.insert(child.reference,
                                         {expansionId, childIndex});
      expansion.pendingRequests++;
      m_dapClient->getVariables(child.reference);
    }
  }

  if (expansion.pendingRequests > 0) {
    return true;
  }

  const ConsoleExpansion finished = expansion;
  m_consoleExpansions.erase(found);

  QString line = QString("%1 = %2").arg(finished.expression,
                                        renderConsoleExpansion(finished, 0));
  if (!finished.type.isEmpty()) {
    line += QString(" (%1)").arg(finished.type);
  }
  appendConsoleLine(line, palette().color(QPalette::Text), true);
  return true;
}

QString DebugPanel::renderConsoleExpansion(const ConsoleExpansion &expansion,
                                           int nodeIndex) const {
  constexpr int kMaxChildrenShown = 12;

  if (nodeIndex < 0 || nodeIndex >= expansion.nodes.size()) {
    return QStringLiteral("{...}");
  }
  const ConsoleExpansionNode &node = expansion.nodes.at(nodeIndex);
  if (node.children.isEmpty()) {
    if (!node.value.isEmpty()) {
      return node.value;
    }
    return node.reference > 0 ? QStringLiteral("{...}") : QStringLiteral("{}");
  }

  QStringList parts;
  for (int childIndex : node.children) {
    if (parts.size() >= kMaxChildrenShown) {
      parts << QStringLiteral("...");
      break;
    }
    const ConsoleExpansionNode &child = expansion.nodes.at(childIndex);
    parts << QString("%1 = %2").arg(
        child.name, renderConsoleExpansion(expansion, childIndex));
  }
  return QString("{%1}").arg(parts.join(QStringLiteral(", ")));
}

void DebugPanel::onEvaluateError(int requestSeq, const QString &expression,
                                 const QString &errorMessage) {
  if (m_localsFallbackPending && requestSeq == m_localsFallbackRequestSeq) {
    const int scopeRef = m_localsFallbackScopeRef;
    const bool staleFrame = m_localsFallbackFrameId != m_currentFrameId;
    clearLocalsFallbackState();
    if (!staleFrame) {
      showLocalsFallbackMessage(
          scopeRef, tr("<locals unavailable; use Watches/REPL>"), true);
    }
    return;
  }

  const int pendingIndex = findPendingConsoleEvaluationIndex(requestSeq);
  if (pendingIndex >= 0) {
    PendingConsoleEvaluation &pending =
        m_pendingConsoleEvaluations[pendingIndex];
    const int nextAttempt = pending.activeAttemptIndex + 1;
    if (nextAttempt < pending.attempts.size()) {
      pending.activeAttemptIndex = nextAttempt;
      pending.activeRequestSeq = 0;
      dispatchPendingConsoleEvaluation(pendingIndex);
      return;
    }

    const QString displayExpression = pending.userExpression;
    m_pendingConsoleEvaluations.removeAt(pendingIndex);
    appendConsoleLine(QString("%1: %2").arg(displayExpression, errorMessage),
                      consoleErrorColor(), true);
    return;
  }

  appendConsoleLine(QString("%1: %2").arg(expression, errorMessage),
                    consoleErrorColor(), true);
}

void DebugPanel::appendConsoleLine(const QString &text, const QColor &color,
                                   bool bold) {
  if (!m_consoleOutput) {
    return;
  }

  QString output = text;
  output.replace("\r\n", "\n");
  output.replace('\r', '\n');
  if (output.size() > MAX_DEBUG_CONSOLE_ENTRY_CHARS) {
    const int truncated = output.size() - MAX_DEBUG_CONSOLE_ENTRY_CHARS;
    output = output.left(MAX_DEBUG_CONSOLE_ENTRY_CHARS);
    output += tr(" ... [truncated %1 chars]").arg(truncated);
  }

  // Follow new output only when the view is already at the bottom; scrolling
  // back to read something must not be undone by the next line arriving.
  QScrollBar *scrollBar = m_consoleOutput->verticalScrollBar();
  const bool wasAtBottom =
      !scrollBar || scrollBar->value() >= scrollBar->maximum() - 4;
  const int previousScroll = scrollBar ? scrollBar->value() : 0;

  QTextCursor cursor(m_consoleOutput->document());
  cursor.movePosition(QTextCursor::End);

  QTextCharFormat format;
  format.setForeground(color);
  format.setBackground(QBrush(Qt::NoBrush));
  format.setFontWeight(bold ? QFont::DemiBold : QFont::Normal);
  cursor.insertText(output, format);
  if (!output.endsWith('\n')) {
    cursor.insertBlock();
  }

  if (wasAtBottom) {
    m_consoleOutput->setTextCursor(cursor);
    m_consoleOutput->ensureCursorVisible();
  } else if (scrollBar) {
    scrollBar->setValue(previousScroll);
  }
}

int DebugPanel::findPendingConsoleEvaluationIndex(int requestSeq) const {
  if (requestSeq <= 0) {
    return -1;
  }
  for (int i = 0; i < m_pendingConsoleEvaluations.size(); ++i) {
    const PendingConsoleEvaluation &pending = m_pendingConsoleEvaluations.at(i);
    if (pending.activeRequestSeq == requestSeq) {
      return i;
    }
  }
  return -1;
}

void DebugPanel::dispatchPendingConsoleEvaluation(int pendingIndex) {
  if (!m_dapClient || pendingIndex < 0 ||
      pendingIndex >= m_pendingConsoleEvaluations.size()) {
    return;
  }

  PendingConsoleEvaluation &pending = m_pendingConsoleEvaluations[pendingIndex];
  if (pending.activeAttemptIndex < 0 ||
      pending.activeAttemptIndex >= pending.attempts.size()) {
    return;
  }

  const DebugEvaluateRequest &attempt =
      pending.attempts.at(pending.activeAttemptIndex);
  if (attempt.expression.trimmed().isEmpty()) {
    return;
  }

  const QString context =
      attempt.context.isEmpty() ? QStringLiteral("repl") : attempt.context;
  pending.activeRequestSeq =
      m_dapClient->evaluate(attempt.expression, m_currentFrameId, context);
}

void DebugPanel::requestAggregatePreviews(const QList<DapVariable> &variables,
                                          QTreeWidgetItem *parentItem) {
  // gdb reports structs and containers with an empty value, leaving the row
  // blank until it is expanded. Fetch one level of children so the row can
  // preview them - bounded, so a large container does not fan out.
  constexpr int kMaxPreviewedChildren = 24;

  if (!m_dapClient || !parentItem || variables.size() > kMaxPreviewedChildren) {
    return;
  }

  for (int i = 0; i < variables.size() && i < parentItem->childCount(); ++i) {
    const DapVariable &var = variables.at(i);
    if (!var.value.trimmed().isEmpty() || var.variablesReference <= 0) {
      continue;
    }
    if (m_pendingTreeSummaries.contains(var.variablesReference) ||
        m_pendingVariableRequests.contains(var.variablesReference)) {
      continue;
    }
    m_pendingTreeSummaries.insert(var.variablesReference, parentItem->child(i));
    m_dapClient->getVariables(var.variablesReference);
  }
}

QString DebugPanel::previewForVariables(const QList<DapVariable> &variables) {
  constexpr int kMaxPreviewParts = 6;

  QStringList parts;
  for (const DapVariable &var : variables) {
    if (parts.size() >= kMaxPreviewParts) {
      parts << QStringLiteral("...");
      break;
    }
    const QString value = var.value.trimmed();
    parts << (value.isEmpty() ? QString("%1 = {...}").arg(var.name)
                              : QString("%1 = %2").arg(var.name, value));
  }
  if (parts.isEmpty()) {
    return QStringLiteral("{}");
  }
  return QString("{%1}").arg(parts.join(QStringLiteral(", ")));
}

QString DebugPanel::emptyStateTextFor(const QTreeWidget *tree) const {
  const DapClient::State state =
      m_dapClient ? m_dapClient->state() : DapClient::State::Disconnected;
  const bool paused = state == DapClient::State::Stopped;
  const bool running = state == DapClient::State::Running;

  if (tree == m_breakpointsTree) {
    return tr("No breakpoints yet.\n"
              "Click in the gutter beside a line number to add one.");
  }
  if (tree == m_watchTree) {
    return tr("No watches yet.\n"
              "Add an expression below to follow it while you step.");
  }
  if (tree == m_callStackTree) {
    if (running) {
      return tr("Running.\nPause or hit a breakpoint to see the call stack.");
    }
    return paused ? QString()
                  : tr("Not debugging.\nPress F5 to start a debug session.");
  }
  if (tree == m_variablesTree) {
    if (running) {
      return tr("Running.\nPause or hit a breakpoint to inspect variables.");
    }
    return paused ? QString()
                  : tr("Not debugging.\nPress F5 to start a debug session.");
  }
  return QString();
}

bool DebugPanel::paintTreeEmptyState(QTreeWidget *tree) {
  if (!tree || tree->topLevelItemCount() > 0) {
    return false;
  }
  const QString text = emptyStateTextFor(tree);
  if (text.isEmpty()) {
    return false;
  }

  QPainter painter(tree->viewport());
  painter.fillRect(tree->viewport()->rect(), tree->palette().base());

  QColor textColor = m_themeInitialized ? m_theme.foregroundColor
                                        : tree->palette().color(QPalette::Text);
  textColor.setAlpha(140);
  painter.setPen(textColor);
  painter.drawText(tree->viewport()->rect().adjusted(24, 0, -24, 0),
                   Qt::AlignCenter | Qt::TextWordWrap, text);
  return true;
}

bool DebugPanel::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_debugStatusLabel && event->type() == QEvent::Resize) {
    applyDebugStatusText();
  }

  // An empty tree is a blank rectangle that explains nothing; say what the
  // view needs instead.
  if (event->type() == QEvent::Paint) {
    for (QTreeWidget *tree :
         {m_callStackTree, m_variablesTree, m_watchTree, m_breakpointsTree}) {
      if (tree && watched == tree->viewport()) {
        if (paintTreeEmptyState(tree)) {
          return true;
        }
        break;
      }
    }
  }

  if (watched == m_consoleWindow && event->type() == QEvent::Close) {
    reattachConsole();
    return true;
  }

  if (watched == m_consoleInput && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Up) {
      recallConsoleHistory(-1);
      return true;
    }
    if (keyEvent->key() == Qt::Key_Down) {
      recallConsoleHistory(1);
      return true;
    }
  }

  if (m_consoleOutput && watched == m_consoleOutput->viewport() &&
      event->type() == QEvent::MouseButtonDblClick) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    const QTextCursor cursor =
        m_consoleOutput->cursorForPosition(mouseEvent->pos());
    navigateToConsoleLocation(cursor.block().text());
  }

  return QWidget::eventFilter(watched, event);
}

void DebugPanel::applyDebugStatusText() {
  if (!m_debugStatusLabel) {
    return;
  }
  // The pill competes for width with the thread selector, so a long location
  // has to end in an ellipsis rather than be sliced mid-word - a clipped
  // "meshview.cpp:7" would read as a real, wrong line number. The subtracted
  // room matches the stylesheet's 12px horizontal padding plus its border.
  const int available = m_debugStatusLabel->width() - 2 * (12 + 1);
  if (available <= 0) {
    m_debugStatusLabel->setText(m_debugStatusText);
    return;
  }
  m_debugStatusLabel->setText(m_debugStatusLabel->fontMetrics().elidedText(
      m_debugStatusText, Qt::ElideRight, available));
}

void DebugPanel::fitVariablesNameColumn() {
  if (!m_variablesTree || m_variablesNameColumnUserSized) {
    return;
  }

  m_variablesNameColumnAutofitting = true;
  m_variablesTree->resizeColumnToContents(0);
  const int measured = m_variablesTree->columnWidth(0);
  // Variables usually arrive while another inspector tab is showing; a hidden
  // tree reports a stale viewport width, so fall back to the panel's width.
  const int available = qMax(m_variablesTree->viewport()->width(), width());
  const int minWidth = 180;
  const int maxWidth = qMax(minWidth, static_cast<int>(available * 0.55));
  m_variablesTree->setColumnWidth(0, qBound(minWidth, measured, maxWidth));
  m_variablesNameColumnAutofitting = false;
}

QColor DebugPanel::consoleErrorColor() const {
  if (m_themeInitialized) {
    return m_theme.debugErrorColor;
  }
  const bool darkBackground = palette().color(QPalette::Base).lightness() < 128;
  return darkBackground ? m_theme.debugErrorColor.lighter(120)
                        : m_theme.debugErrorColor.darker(120);
}

QColor DebugPanel::consoleMutedColor() const {
  if (m_themeInitialized) {
    return m_theme.debugReadyColor;
  }
  QColor muted = palette().color(QPalette::PlaceholderText);
  if (!muted.isValid()) {
    muted = palette().color(QPalette::Mid);
  }
  return muted;
}

QColor DebugPanel::consoleInfoColor() const {
  if (m_themeInitialized) {
    return m_theme.debugRunningColor;
  }
  QColor info = palette().color(QPalette::Link);
  if (!info.isValid()) {
    info = palette().color(QPalette::Highlight);
  }
  return info;
}

QString DebugPanel::formatVariable(const DapVariable &var) const {
  if (var.type.isEmpty()) {
    return var.value;
  }
  return QString("%1 (%2)").arg(var.value).arg(var.type);
}

QIcon DebugPanel::variableIcon(const DapVariable &var) const {
  if (var.variablesReference > 0) {

    return style()->standardIcon(QStyle::SP_DirIcon);
  } else {

    return style()->standardIcon(QStyle::SP_FileIcon);
  }
}
