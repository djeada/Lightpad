#include "testpanel.h"
#include "../../test_templates/pytestintegration.h"
#include "../../test_templates/testsource.h"
#include "../../theme/colorcontrast.h"
#include "../dialogs/testconfigurationdialog.h"
#include "../dialogs/themedmessagebox.h"
#include "../paneltypography.h"
#include "../uistylehelper.h"
#include "../workspaceicons.h"
#include "debugglyphs.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QUrlQuery>
#include <QWidgetAction>
#include <QtMath>
#include <algorithm>
#include <functional>

namespace {
constexpr int TestIdRole = Qt::UserRole;
constexpr int FilePathRole = Qt::UserRole + 1;
constexpr int LineNumberRole = Qt::UserRole + 2;
constexpr int IsSuiteRole = Qt::UserRole + 3;
constexpr int IsDetailItemRole = Qt::UserRole + 4;
constexpr int StatusRole = Qt::UserRole + 5;
constexpr int GenerationRole = Qt::UserRole + 6;
constexpr int DurationRole = Qt::UserRole + 7;
constexpr int SourceColumn = 3;
constexpr int IsFileGroupRole = Qt::UserRole + 8;

QString statusText(TestStatus status) {
  switch (status) {
  case TestStatus::Queued:
    return QObject::tr("Not run");
  case TestStatus::Running:
    return QObject::tr("Running");
  case TestStatus::Passed:
    return QObject::tr("Passed");
  case TestStatus::Failed:
    return QObject::tr("Failed");
  case TestStatus::Skipped:
    return QObject::tr("Skipped");
  case TestStatus::Errored:
    return QObject::tr("Error");
  }
  return {};
}

QString durationText(int milliseconds) {
  return milliseconds < 0 ? QString::fromUtf8("—")
         : milliseconds < 1000
             ? QObject::tr("%1 ms").arg(milliseconds)
             : QObject::tr("%1 s").arg(milliseconds / 1000.0, 0, 'f', 2);
}

QIcon testStatusIcon(TestStatus status, const QColor &color, int size) {
  size = qMax(18, size);
  const qreal ratio = qApp->devicePixelRatio();
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.scale(size / 20.0, size / 20.0);
  painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  if (status == TestStatus::Passed) {
    QPainterPath check;
    check.moveTo(4, 10);
    check.lineTo(8, 14);
    check.lineTo(16, 5);
    painter.drawPath(check);
  } else if (status == TestStatus::Failed) {
    painter.drawLine(QPointF(5, 5), QPointF(15, 15));
    painter.drawLine(QPointF(15, 5), QPointF(5, 15));
  } else if (status == TestStatus::Errored) {
    painter.drawEllipse(QRectF(3, 3, 14, 14));
    painter.drawLine(QPointF(10, 6), QPointF(10, 10));
    painter.drawPoint(QPointF(10, 14));
  } else if (status == TestStatus::Running) {
    painter.drawArc(QRectF(3, 3, 14, 14), 40 * 16, 280 * 16);
  } else {
    painter.drawEllipse(QRectF(4, 4, 12, 12));
    if (status == TestStatus::Skipped)
      painter.drawLine(QPointF(5, 15), QPointF(15, 5));
  }
  return QIcon(pixmap);
}

class TestTreeItem : public QTreeWidgetItem {
public:
  using QTreeWidgetItem::QTreeWidgetItem;
  bool operator<(const QTreeWidgetItem &other) const override {
    const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
    if (column == 2)
      return data(0, DurationRole).toInt() <
             other.data(0, DurationRole).toInt();
    if (column == 1)
      return data(0, StatusRole).toInt() < other.data(0, StatusRole).toInt();
    return text(column).compare(other.text(column), Qt::CaseInsensitive) < 0;
  }
};

void appendUnique(QStringList &values, const QString &value) {
  const QString trimmed = value.trimmed();
  if (!trimmed.isEmpty() && !values.contains(trimmed))
    values.append(trimmed);
}

QStringList googleTestTokenCandidates(const QString &value) {
  QStringList candidates;
  appendUnique(candidates, value);

  const int firstSlash = value.indexOf('/');
  if (firstSlash > 0)
    appendUnique(candidates, value.left(firstSlash));

  const int lastSlash = value.lastIndexOf('/');
  if (lastSlash > 0 && lastSlash + 1 < value.size())
    appendUnique(candidates, value.mid(lastSlash + 1));

  return candidates;
}

QString toRegexAlternation(const QStringList &values) {
  QStringList escaped;
  escaped.reserve(values.size());
  for (const QString &value : values)
    escaped.append(QRegularExpression::escape(value));
  return escaped.join('|');
}

QString formatCommandArgument(const QString &arg) {
  if (arg.isEmpty())
    return "\"\"";

  QString escaped = arg;
  escaped.replace('\\', "\\\\");
  escaped.replace('"', "\\\"");

  static const QRegularExpression needsQuoting(R"([\s"])");
  if (needsQuoting.match(arg).hasMatch())
    return "\"" + escaped + "\"";
  return escaped;
}

QStringList extractGoogleTestNames(const QString &content) {
  QStringList names;
  static const QRegularExpression testRegex(
      R"(\b(?:TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\s*\(\s*([^\s,()]+)\s*,\s*([^\s,()]+)\s*\))");

  QRegularExpressionMatchIterator it = testRegex.globalMatch(content);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    appendUnique(names, match.captured(1).trimmed() + "." +
                            match.captured(2).trimmed());
  }

  return names;
}

bool resolveNavigableFilePath(const QString &candidatePath,
                              const QString &workspaceFolder,
                              QString *resolvedPath) {
  if (candidatePath.isEmpty() || !resolvedPath)
    return false;

  QString absolutePath = candidatePath;
  QFileInfo info(absolutePath);
  if (info.isRelative() && !workspaceFolder.isEmpty()) {
    absolutePath = QDir(workspaceFolder).absoluteFilePath(absolutePath);
    info = QFileInfo(absolutePath);
  }

  if (!info.exists() || !info.isFile())
    return false;

  *resolvedPath = info.absoluteFilePath();
  return true;
}

bool findGoogleTestMacroLocation(const QString &content,
                                 const QStringList &suiteCandidates,
                                 const QStringList &testCandidates, int *line) {
  if (suiteCandidates.isEmpty() || testCandidates.isEmpty())
    return false;

  const QString pattern =
      QString(
          R"(\b(?:TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\s*\(\s*(?:%1)\s*,\s*(?:%2)\s*\))")
          .arg(toRegexAlternation(suiteCandidates),
               toRegexAlternation(testCandidates));
  const QRegularExpression regex(pattern);
  const QRegularExpressionMatch match = regex.match(content);
  if (!match.hasMatch())
    return false;

  if (line)
    *line = content.left(match.capturedStart()).count('\n') + 1;
  return true;
}

int autoRunModeToComboIndex(AutoRunMode mode) {
  switch (mode) {
  case AutoRunMode::AllOnSave:
    return 0;
  case AutoRunMode::CurrentFileOnSave:
    return 1;
  case AutoRunMode::LastSelection:
    return 2;
  case AutoRunMode::Off:
  default:
    return 0;
  }
}

AutoRunMode comboIndexToAutoRunMode(int index) {
  switch (index) {
  case 0:
    return AutoRunMode::AllOnSave;
  case 1:
    return AutoRunMode::CurrentFileOnSave;
  case 2:
    return AutoRunMode::LastSelection;
  default:
    return AutoRunMode::AllOnSave;
  }
}

class TestTreeDelegate : public QStyledItemDelegate {
public:
  explicit TestTreeDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void setTheme(const Theme &theme) {
    m_textColor = theme.foregroundColor;
    m_baseBackground = theme.backgroundColor;
    m_hoverBackground =
        ColorContrast::mix(theme.backgroundColor, theme.hoverColor, 0.6);
    m_selectedBackground = theme.accentSoftColor.isValid()
                               ? theme.accentSoftColor
                               : ColorContrast::mix(theme.backgroundColor,
                                                    theme.accentColor, 0.25);
    m_selectedBorder = theme.accentColor;
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter)
      return;

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool isSelected = opt.state.testFlag(QStyle::State_Selected);
    const bool isHovered = opt.state.testFlag(QStyle::State_MouseOver);

    const QColor base = m_baseBackground.isValid()
                            ? m_baseBackground
                            : option.palette.color(QPalette::Base);
    const QColor hoverBg = m_hoverBackground.isValid()
                               ? m_hoverBackground
                               : option.palette.color(QPalette::AlternateBase);
    const QColor selectedBg =
        m_selectedBackground.isValid()
            ? m_selectedBackground
            : ColorContrast::mix(
                  base, option.palette.color(QPalette::Highlight), 0.3);
    const QColor selectedBorder =
        m_selectedBorder.isValid() ? m_selectedBorder
                                   : option.palette.color(QPalette::Highlight);

    QRect rowRect = opt.rect.adjusted(0, 1, 0, -1);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    QColor fill = base;
    if (isSelected || isHovered) {
      fill = isSelected ? selectedBg : hoverBg;
      painter->setPen(Qt::NoPen);
      painter->setBrush(fill);
      painter->drawRect(rowRect);

      if (isSelected && index.column() == 0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selectedBorder);
        painter->drawRect(
            QRect(rowRect.left(), rowRect.top(), 2, rowRect.height()));
      }
    }

    QColor text = m_textColor.isValid() ? m_textColor
                                        : option.palette.color(QPalette::Text);
    const QVariant foreground = index.data(Qt::ForegroundRole);
    const bool hasItemColor = foreground.canConvert<QBrush>() &&
                              foreground.value<QBrush>().style() != Qt::NoBrush;
    if (hasItemColor)
      text = foreground.value<QBrush>().color();
    text = ColorContrast::ensure(text, ColorContrast::flatten(fill, base),
                                 hasItemColor ? ColorContrast::GlyphRatio
                                              : ColorContrast::TextRatio);

    opt.rect = rowRect.adjusted(8, 0, -6, 0);
    opt.state &= ~QStyle::State_HasFocus;
    opt.showDecorationSelected = false;
    opt.palette.setColor(QPalette::Highlight, Qt::transparent);
    opt.palette.setColor(QPalette::HighlightedText, text);
    opt.palette.setColor(QPalette::Text, text);

    if (index.column() == 0) {
      const QWidget *widget = option.widget;
      QStyle *style = widget ? widget->style() : QApplication::style();
      const QRect textRect =
          style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
      QTextLayout textLayout(opt.text, opt.font);
      const qreal height = layoutText(textLayout, textRect.width());
      opt.text.clear();
      style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
      painter->setPen(text);
      textLayout.draw(
          painter, QPointF(textRect.left(),
                           opt.rect.top() +
                               qMax(0.0, (opt.rect.height() - height) / 2.0)));
    } else {
      QStyledItemDelegate::paint(painter, opt, index);
    }
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QSize size = QStyledItemDelegate::sizeHint(opt, index);
    if (index.column() == 0) {
      if (auto *tree = qobject_cast<QTreeWidget *>(parent())) {
        int indentation = tree->indentation();
        for (auto ancestor = index.parent(); ancestor.isValid();
             ancestor = ancestor.parent())
          indentation += tree->indentation();
        opt.rect = QRect(0, 0, tree->columnWidth(0) - indentation, 1000)
                       .adjusted(10, 1, -8, -1);
        const QRect textRect = tree->style()->subElementRect(
            QStyle::SE_ItemViewItemText, &opt, tree);
        QTextLayout text(opt.text, opt.font);
        const qreal height = layoutText(text, textRect.width());
        size.setHeight(qMax(size.height(), qCeil(height) + 12));
      }
    }
    size.setHeight(qMax(size.height(), opt.fontMetrics.height() + 12));
    return size;
  }

protected:
  void initStyleOption(QStyleOptionViewItem *option,
                       const QModelIndex &index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    if (index.column() == 0) {
      option->text.replace("_", QString::fromUtf8("_\u200b"));
      option->text.replace("::", QString::fromUtf8("::\u200b"));
      option->features |= QStyleOptionViewItem::WrapText;
      option->textElideMode = Qt::ElideNone;
    } else {
      option->features &= ~QStyleOptionViewItem::WrapText;
      option->textElideMode =
          index.column() == SourceColumn ? Qt::ElideMiddle : Qt::ElideRight;
    }
  }

private:
  static qreal layoutText(QTextLayout &text, int width) {
    QTextOption wrapping;
    wrapping.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    text.setTextOption(wrapping);
    text.beginLayout();
    qreal height = 0;
    for (;;) {
      auto line = text.createLine();
      if (!line.isValid())
        break;
      line.setLineWidth(qMax(20, width));
      line.setPosition(QPointF(0, height));
      height += line.height();
    }
    text.endLayout();
    return height;
  }
  QColor m_textColor;
  QColor m_baseBackground;
  QColor m_hoverBackground;
  QColor m_selectedBackground;
  QColor m_selectedBorder;
};
} // namespace

TestPanel::TestPanel(QWidget *parent) : QWidget(parent) {
  setObjectName("TestPanel");
  m_runManager = new TestRunManager(this);
  m_autoTestRunner = new AutoTestRunner(m_runManager, this);
  m_runDetailsRefreshTimer = new QTimer(this);
  m_runDetailsRefreshTimer->setSingleShot(true);
  m_runDetailsRefreshTimer->setInterval(100);
  connect(m_runDetailsRefreshTimer, &QTimer::timeout, this,
          &TestPanel::refreshRunDetails);
  setupUI();

  auto &configManager = TestConfigurationManager::instance();
  connect(&configManager, &TestConfigurationManager::templatesLoaded, this,
          &TestPanel::refreshConfigurations);
  connect(&configManager, &TestConfigurationManager::configurationsChanged,
          this, &TestPanel::refreshConfigurations);

  connect(m_runManager, &TestRunManager::testStarted, this,
          &TestPanel::onTestStarted);
  connect(m_runManager, &TestRunManager::testFinished, this,
          &TestPanel::onTestFinished);
  connect(m_runManager, &TestRunManager::runStarted, this,
          &TestPanel::onRunStarted);
  connect(m_runManager, &TestRunManager::runFinished, this,
          &TestPanel::onRunFinished);
  connect(m_runManager, &TestRunManager::outputLine, this,
          &TestPanel::onOutputLine);
  connect(m_runManager, &TestRunManager::processStarted, this,
          &TestPanel::onProcessStarted);
  connect(m_runManager, &TestRunManager::processFinished, this,
          &TestPanel::onProcessFinished);
  connect(m_runManager, &TestRunManager::totalTestsChanged, this,
          [this](int total) {
            m_expectedTests = total;
            m_progressBar->setRange(0, qMax(1, total));
            m_progressBar->setValue(m_completedRunIds.size());
          });

  refreshConfigurations();
}

TestPanel::~TestPanel() {
  disconnect(m_runManager, nullptr, this, nullptr);
  m_runManager->stop();
  if (m_discoveryAdapter)
    disconnect(m_discoveryAdapter, nullptr, this, nullptr);
  if (m_ownsDiscoveryAdapter)
    delete m_discoveryAdapter;
}

void TestPanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_headerShell = new QWidget(this);
  m_headerShell->setObjectName("testHeaderShell");
  auto *headerLayout = new QVBoxLayout(m_headerShell);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(0);

  setupToolbar();
  auto *actions = new QHBoxLayout;
  actions->setContentsMargins(6, 4, 6, 0);
  actions->setSpacing(6);
  actions->addWidget(m_toolbar, 1);
  actions->addWidget(m_configCombo);
  actions->addWidget(m_optionsButton);
  headerLayout->addLayout(actions);

  auto *filters = new QHBoxLayout;
  filters->setContentsMargins(10, 6, 10, 6);
  filters->setSpacing(8);
  m_searchEdit = new QLineEdit(m_headerShell);
  m_searchEdit->setObjectName("testSearchEdit");
  m_searchEdit->setPlaceholderText(tr("Search test names, files, or IDs"));
  m_searchEdit->setClearButtonEnabled(true);
  m_searchEdit->setAccessibleName(tr("Filter tests by name"));
  m_searchEdit->setMinimumWidth(80);
  m_searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &TestPanel::onSearchTextChanged);
  filters->addWidget(m_searchEdit, 1);
  filters->addWidget(m_filterCombo);
  headerLayout->addLayout(filters);

  layout->addWidget(m_headerShell);

  m_progressBar = new QProgressBar(this);
  m_progressBar->setObjectName("testProgressBar");
  m_progressBar->setMaximumHeight(3);
  m_progressBar->setTextVisible(false);
  m_progressBar->setRange(0, 0);
  m_progressBar->setVisible(false);
  layout->addWidget(m_progressBar);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setObjectName("testDetailsSplitter");
  m_splitter->setChildrenCollapsible(false);

  auto *treeContainer = new QWidget(this);
  auto *treeLayout = new QVBoxLayout(treeContainer);
  treeLayout->setContentsMargins(0, 0, 0, 0);
  treeLayout->setSpacing(0);

  m_tree = new QTreeWidget(treeContainer);
  m_tree->setObjectName("testTree");
  m_tree->setAccessibleName(tr("Tests and results"));
  m_tree->setHeaderLabels(
      {tr("Test"), tr("Result"), tr("Time"), tr("Definition")});
  m_tree->setRootIsDecorated(true);
  m_tree->setAlternatingRowColors(false);
  m_tree->setUniformRowHeights(false);
  m_tree->setWordWrap(true);
  m_tree->setTextElideMode(Qt::ElideNone);
  m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_tree->setAllColumnsShowFocus(true);
  m_tree->setMouseTracking(true);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
  m_tree->header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_tree->header(), &QHeaderView::customContextMenuRequested, this,
          [this](const QPoint &point) {
            QMenu menu(this);
            menu.addAction(tr("Fit Columns"), this, [this]() {
              m_userSizedColumns = false;
              fitTestColumns();
            });
            for (int column = 1; column < m_tree->columnCount(); ++column) {
              auto *action = menu.addAction(m_tree->headerItem()->text(column));
              action->setCheckable(true);
              action->setChecked(!m_tree->isColumnHidden(column));
              connect(action, &QAction::toggled, this,
                      [this, column](bool visible) {
                        m_userSizedColumns = true;
                        m_tree->setColumnHidden(column, !visible);
                      });
            }
            menu.exec(m_tree->header()->mapToGlobal(point));
          });
  m_tree->header()->setSectionsClickable(true);
  connect(
      m_tree->header(), &QHeaderView::sectionClicked, this, [this](int column) {
        const auto order =
            m_tree->header()->sortIndicatorSection() == column &&
                    m_tree->header()->sortIndicatorOrder() == Qt::AscendingOrder
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
        m_tree->setSortingEnabled(true);
        m_tree->sortItems(column, order);
      });
  m_tree->setColumnHidden(2, true);
  connect(m_splitter, &QSplitter::splitterMoved, this,
          [this]() { fitTestColumns(); });
  connect(m_tree->header(), &QHeaderView::sectionResized, this, [this]() {
    if (!m_fittingColumns &&
        QApplication::mouseButtons().testFlag(Qt::LeftButton))
      m_userSizedColumns = true;
    m_tree->doItemsLayout();
  });
  m_tree->installEventFilter(this);
  m_tree->viewport()->installEventFilter(this);
  connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
          &TestPanel::updateSelectionActions);
  connect(m_tree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *item) { showItemDetails(item); });
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tree->setItemDelegate(new TestTreeDelegate(m_tree));
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          &TestPanel::onItemDoubleClicked);
  connect(m_tree, &QTreeWidget::itemClicked, this, &TestPanel::onItemClicked);
  connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
          &TestPanel::onContextMenu);

  m_emptyState = new QWidget(treeContainer);
  m_emptyState->setObjectName("testEmptyStatePanel");
  auto *emptyLayout = new QVBoxLayout(m_emptyState);
  emptyLayout->setContentsMargins(16, 8, 16, 8);
  emptyLayout->setSpacing(6);
  emptyLayout->addStretch();
  m_emptyStateLabel = new QLabel(m_emptyState);
  m_emptyStateLabel->setObjectName("testEmptyState");
  m_emptyStateLabel->setAlignment(Qt::AlignCenter);
  m_emptyStateLabel->setText(
      tr("No tests discovered yet.\nUse \"Discover\" to find tests or "
         "\"Run All\" to execute them."));
  m_emptyStateLabel->setWordWrap(true);
  emptyLayout->addWidget(m_emptyStateLabel);
  m_emptyAction = new QPushButton(tr("Discover tests"), m_emptyState);
  m_emptyAction->setObjectName("testEmptyAction");
  emptyLayout->addWidget(m_emptyAction, 0, Qt::AlignHCenter);
  emptyLayout->addStretch();
  connect(m_emptyAction, &QPushButton::clicked, this, [this]() {
    if (m_tree->topLevelItemCount() > 0) {
      m_filterCombo->setCurrentIndex(0);
      m_searchEdit->clear();
    } else {
      discoverTests();
    }
  });

  treeLayout->addWidget(m_tree, 1);
  treeLayout->addWidget(m_emptyState, 1);

  m_detailSection = new QWidget(this);
  m_detailSection->setObjectName("testDetailSection");
  auto *detailLayout = new QVBoxLayout(m_detailSection);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailLayout->setSpacing(0);

  m_detailToggleButton = new QToolButton(this);
  m_detailToggleButton->setObjectName("testDetailToggle");
  m_detailToggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_detailToggleButton->setText(tr("Run Details"));
  m_detailToggleButton->setCheckable(true);
  m_detailToggleButton->setChecked(false);
  m_detailToggleButton->setArrowType(Qt::RightArrow);

  m_detailTabs = new QTabWidget(m_detailSection);
  m_detailTabs->setObjectName("testDetailTabs");
  auto *browser = new QTextBrowser(m_detailTabs);
  browser->setOpenLinks(false);
  connect(browser, &QTextBrowser::anchorClicked, this,
          &TestPanel::openSourceLink);
  m_detailPane = browser;
  m_detailPane->setObjectName("testDetailPane");
  m_detailPane->setReadOnly(true);
  m_detailPane->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  m_detailPane->setMinimumHeight(100);
  m_detailPane->setPlaceholderText(
      tr("Select a test to view its output and details"));
  m_detailTabs->addTab(m_detailPane, tr("Selected test"));
  m_runOutputPane = new QTextEdit(m_detailTabs);
  m_runOutputPane->setObjectName("testRunOutput");
  m_runOutputPane->setReadOnly(true);
  m_detailTabs->addTab(m_runOutputPane, tr("Run output"));
  detailLayout->addWidget(m_detailTabs);

  connect(m_detailToggleButton, &QToolButton::toggled, this,
          [this](bool checked) { setDetailsExpanded(checked); });

  m_splitter->addWidget(treeContainer);
  m_splitter->addWidget(m_detailSection);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 1);
  setDetailsExpanded(false);

  layout->addWidget(m_splitter, 1);

  auto *footer = new QWidget(this);
  footer->setObjectName("testFooter");
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(10, 0, 6, 0);
  footerLayout->setSpacing(8);
  m_statusLabel = new QLabel(this);
  m_statusLabel->setObjectName("statusLabel");
  m_statusLabel->setMinimumWidth(0);
  m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  footerLayout->addWidget(m_statusLabel, 1);
  footerLayout->addWidget(m_detailToggleButton);
  layout->addWidget(footer);

  updateStatusLabel();
  updateEmptyState();
}

void TestPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("testToolbar");
  m_toolbar->setIconSize(QSize(16, 16));
  m_toolbar->setMovable(false);
  m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  m_runAllAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay),
                           tr("Run All"), this, &TestPanel::runAll);
  m_runAllAction->setObjectName("runAllAction");
  m_runAllAction->setToolTip(tr("Run All Tests"));
  m_toolbar->widgetForAction(m_runAllAction)
      ->setObjectName("runAllTestsButton");

  m_runSelectedAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_MediaPlay), tr("Run Selected"), this,
      &TestPanel::runSelectedTests);
  m_runSelectedAction->setObjectName("runSelectedTestsAction");
  m_runSelectedAction->setToolTip(
      tr("Run selected tests or all tests in selected groups (Ctrl+Enter)"));
  m_runSelectedAction->setEnabled(false);

  m_runFailedAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload),
                           tr("Run Failed"), this, &TestPanel::runFailed);
  m_runFailedAction->setToolTip(tr("Re-run Failed Tests"));
  m_runFailedAction->setEnabled(false);

  m_debugAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_ComputerIcon),
                           tr("Debug"), this, &TestPanel::debugSelectedTests);
  m_debugAction->setObjectName("debugTestsAction");
  m_debugAction->setToolTip(
      tr("Debug selected pytest tests, or all tests if none are selected"));

  m_stopAction =
      m_toolbar->addAction(style()->standardIcon(QStyle::SP_MediaStop),
                           tr("Stop"), this, &TestPanel::stopTests);
  m_stopAction->setToolTip(tr("Stop"));
  m_stopAction->setEnabled(false);

  m_discoverAction = m_toolbar->addAction(
      style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("Discover"),
      this, &TestPanel::discoverTests);
  m_discoverAction->setToolTip(tr("Discover Tests"));

  m_optionsButton = new QToolButton(this);
  m_optionsButton->setObjectName("testOptions");
  m_optionsButton->setText(tr("Options"));
  m_optionsButton->setToolTip(
      tr("Test configuration, automatic runs, and clear results"));
  m_optionsButton->setAccessibleName(tr("Test panel options"));
  m_optionsButton->setPopupMode(QToolButton::InstantPopup);
  auto *options = new QMenu(m_optionsButton);
  m_optionsButton->setMenu(options);

  m_openSourceAction = options->addAction(tr("Go to Test Definition"), this,
                                          &TestPanel::openSelectedTest);
  m_openSourceAction->setObjectName("goToTestDefinition");
  m_openSourceAction->setToolTip(
      tr("Open the selected test's definition (Enter)"));
  m_openSourceAction->setEnabled(false);
  options->addAction(tr("Expand All"), this, [this]() {
    for (auto *group : m_suiteItems)
      group->setExpanded(true);
  });
  options->addAction(tr("Collapse All"), this,
                     [this]() { m_tree->collapseAll(); });
  options->addAction(tr("Fit Columns"), this, [this]() {
    m_userSizedColumns = false;
    fitTestColumns();
  });
  options->addSeparator();

  m_clearAction = options->addAction(
      style()->standardIcon(QStyle::SP_DialogResetButton), tr("Clear Results"),
      this, [this]() {
        m_detailPane->clear();
        m_runOutputPane->clear();
        m_testResults.clear();
        m_runManager->clearResults();
        m_completedRunIds.clear();
        m_hasDurations = false;
        for (auto *item : m_testItems) {
          item->setText(1, tr("Not run"));
          item->setText(2, QString::fromUtf8("—"));
          item->setData(0, StatusRole, static_cast<int>(TestStatus::Queued));
          item->setData(0, DurationRole, -1);
          while (item->childCount())
            delete item->takeChild(0);
          updateTreeItemIcon(item, TestStatus::Queued);
        }
        m_passedCount = m_failedCount = m_skippedCount = m_erroredCount = 0;
        resetRunContext();
        updateSuiteSummaries();
        updateStatusLabel();
        updateEmptyState();
        applyFilter();
        fitTestColumns();
        updateSelectionActions();
        emit countsChanged(0, 0, 0, 0);
      });
  m_clearAction->setToolTip(tr("Clear Results"));

  options->addSeparator();

  m_autoRunAction = options->addAction(
      style()->standardIcon(QStyle::SP_DialogApplyButton), tr("Auto"));
  m_autoRunAction->setObjectName("autoRunAction");
  m_autoRunAction->setCheckable(true);
  m_autoRunAction->setChecked(false);
  m_autoRunAction->setToolTip(tr("Toggle Auto-run Tests on Save"));
  connect(m_autoRunAction, &QAction::toggled, this,
          &TestPanel::onAutoRunToggled);
  connect(m_autoRunAction, &QAction::toggled, this, [this](bool enabled) {
    m_optionsButton->setText(enabled ? tr("Auto on save") : tr("Options"));
  });

  auto *autoRunRow = new QWidget(options);
  auto *autoRunLayout = new QHBoxLayout(autoRunRow);
  autoRunLayout->setContentsMargins(12, 6, 12, 6);
  autoRunLayout->addWidget(new QLabel(tr("On save"), autoRunRow));
  m_autoRunModeCombo = new QComboBox(autoRunRow);
  m_autoRunModeCombo->setObjectName("autoRunModeCombo");
  m_autoRunModeCombo->addItem(tr("All on Save"));
  m_autoRunModeCombo->addItem(tr("File on Save"));
  m_autoRunModeCombo->addItem(tr("Last Selection"));
  m_autoRunModeCombo->setMinimumContentsLength(8);
  m_autoRunModeCombo->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  m_autoRunModeCombo->setEnabled(false);
  m_autoRunModeCombo->setToolTip(tr("Auto-run scope"));
  connect(m_autoRunModeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &TestPanel::onAutoRunModeChanged);
  autoRunLayout->addWidget(m_autoRunModeCombo);
  auto *autoRunWidget = new QWidgetAction(options);
  autoRunWidget->setDefaultWidget(autoRunRow);
  options->addAction(autoRunWidget);

  m_filterCombo = new QComboBox(this);
  m_filterCombo->setObjectName("filterCombo");
  m_filterCombo->addItem(tr("All results"));
  m_filterCombo->addItem(tr("Failed"));
  m_filterCombo->addItem(tr("Passed"));
  m_filterCombo->addItem(tr("Skipped"));
  m_filterCombo->setAccessibleName(tr("Filter tests by status"));
  connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &TestPanel::onFilterChanged);
  m_filterCombo->setMinimumContentsLength(6);
  m_filterCombo->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  m_filterCombo->setMaximumWidth(140);

  m_configCombo = new QComboBox(this);
  m_configCombo->setObjectName("configCombo");
  m_configCombo->setAccessibleName(tr("Test configuration"));
  m_configCombo->setToolTip(
      tr("Choose the configuration used to discover and run tests"));
  m_configCombo->setMinimumContentsLength(10);
  m_configCombo->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &TestPanel::onConfigChanged);
  m_configCombo->setMinimumWidth(120);
  m_configCombo->setMaximumWidth(220);

  options->addSeparator();

  m_configureAction = options->addAction(
      style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Configure"),
      this, &TestPanel::openConfigurationDialog);
  m_configureAction->setToolTip(tr("Edit Test Configurations"));
  for (auto *action : {m_stopAction})
    if (auto *button =
            qobject_cast<QToolButton *>(m_toolbar->widgetForAction(action)))
      button->setToolButtonStyle(Qt::ToolButtonIconOnly);
}

void TestPanel::applyTheme(const Theme &theme) {
  m_theme = theme;
  setObjectName(QStringLiteral("testPanel"));

  const QColor panelSurface = theme.backgroundColor;
  const QColor headerSurface = theme.surfaceColor;
  const QColor textColor =
      UIStyleHelper::readableText(theme, panelSurface, theme.foregroundColor);
  const QColor secondaryText = UIStyleHelper::secondaryTextColor(theme);
  const QColor borderColor = ColorContrast::mix(headerSurface, textColor, 0.14);
  const QColor hoverBg = theme.hoverColor;
  const QColor accentColor =
      theme.accentColor.isValid() ? theme.accentColor : theme.testRunningColor;
  const QColor progressTrack = theme.surfaceAltColor.isValid()
                                   ? theme.surfaceAltColor
                                   : theme.surfaceColor;
  const QString sideBorders;

  if (auto *delegate =
          dynamic_cast<TestTreeDelegate *>(m_tree->itemDelegate())) {
    delegate->setTheme(theme);
  }
  m_runAllAction->setIcon(
      DebugGlyphs::icon(DebugGlyphs::Glyph::Play, textColor));
  m_runFailedAction->setIcon(
      DebugGlyphs::icon(DebugGlyphs::Glyph::Restart, secondaryText));
  m_stopAction->setIcon(
      DebugGlyphs::icon(DebugGlyphs::Glyph::Stop, secondaryText));
  m_discoverAction->setIcon(
      WorkspaceIcons::icon(WorkspaceIcons::Glyph::Search, secondaryText));

  setStyleSheet(
      UIStyleHelper::panelStyle(theme, objectName()) +
      UIStyleHelper::panelHeaderStyle(theme, m_headerShell->objectName()) +
      QString(
          "QWidget#testPanel { %1 }"
          "QToolBar#testToolbar {"
          "  background: transparent;"
          "  border: 0;"
          "  padding: 0;"
          "  spacing: 4px;"
          "}"
          "QToolBar#testToolbar QToolButton { padding: 4px 8px; min-height: "
          "20px; font-size: 12px; }"
          "QToolButton#runAllTestsButton { background: %10; color: %3; "
          "font-weight: 600; }"
          "QToolButton#testOptions { padding: 4px 8px; font-size: 12px; }"
          "QLineEdit#testSearchEdit {"
          "  background: %2;"
          "  color: %3;"
          "  border: 1px solid %4;"
          "  border-radius: 4px;"
          "  padding: 4px 8px;"
          "  font-size: 13px;"
          "}"
          "QLineEdit#testSearchEdit:focus {"
          "  border-color: %5;"
          "}"
          "QProgressBar#testProgressBar {"
          "  background: %6;"
          "  border: none;"
          "  max-height: 3px;"
          "}"
          "QProgressBar#testProgressBar::chunk {"
          "  background: %5;"
          "}"
          "QTreeWidget#testTree {"
          "  background: %2;"
          "  color: %3;"
          "  border: none;"
          "  outline: none;"
          "  padding: 2px;"
          "}"
          "QTreeWidget#testTree::item { padding: 4px 2px; }"
          "QTreeWidget#testTree::item:hover,"
          "QTreeWidget#testTree::item:selected {"
          "  background: transparent;"
          "}"
          "QHeaderView::section {"
          "  %7"
          "  background: %8;"
          "  border: 0;"
          "  border-bottom: 1px solid %4;"
          "  padding: 6px 8px;"
          "}"
          "QWidget#testDetailSection {"
          "  background: %2;"
          "  border-top: 1px solid %4;"
          "}"
          "QToolButton#testDetailToggle {"
          "  background: %8;"
          "  color: %9;"
          "  border: none;"
          "  border-radius: 0;"
          "  padding: 4px 6px;"
          "  text-align: left;"
          "  font-size: 12px;"
          "  font-weight: 600;"
          "}"
          "QToolButton#testDetailToggle:hover {"
          "  background: %10;"
          "  color: %3;"
          "}"
          "QToolButton#testDetailToggle:checked {"
          "  background: %8;"
          "}"
          "QTextEdit#testDetailPane, QTextEdit#testRunOutput {"
          "  background: %2;"
          "  color: %3;"
          "  border: none;"
          "  padding: 8px;"
          "  font-family: monospace;"
          "  font-size: 12px;"
          "}"
          "QLabel#statusLabel {"
          "  color: %9;"
          "  background: transparent;"
          "  padding: 4px 0;"
          "  font-size: 12px;"
          "}"
          "QWidget#testFooter { background: %8; border-top: 1px solid %4; }"
          "QWidget#testEmptyStatePanel { background: %2; }"
          "QPushButton#testEmptyAction { padding: 4px 12px; font-size: 12px; }"
          "QSplitter::handle {"
          "  background: transparent;"
          "  border-top: 1px solid %4;"
          "  height: 5px;"
          "}"
          "QSplitter::handle:hover {"
          "  background: %10;"
          "}")
          .arg(sideBorders, panelSurface.name(), textColor.name(),
               borderColor.name(), accentColor.name(), progressTrack.name(),
               UIStyleHelper::sectionLabelStyle(theme), headerSurface.name(),
               secondaryText.name())
          .arg(hoverBg.name()));

  m_emptyStateLabel->setStyleSheet(
      QString(
          "color: %1; background: transparent; font-size: 13px; padding: 0;")
          .arg(ColorContrast::ensure(secondaryText, panelSurface).name()));

  QString comboStyle = UIStyleHelper::comboBoxStyle(theme);
  m_filterCombo->setStyleSheet(comboStyle);
  m_configCombo->setStyleSheet(comboStyle);
  m_autoRunModeCombo->setStyleSheet(comboStyle);
  m_detailTabs->setStyleSheet(UIStyleHelper::tabWidgetStyle(theme));
  const QColor link = ColorContrast::ensure(accentColor, panelSurface);
  m_detailPane->document()->setDefaultStyleSheet("a { color: " + link.name() +
                                                 "; }");

  for (auto it = m_testItems.constBegin(); it != m_testItems.constEnd(); ++it) {
    const auto result = m_testResults.constFind(it.key());
    if (it.value() && result != m_testResults.constEnd())
      updateTreeItemIcon(it.value(), result->status);
  }
  if (!m_runManager || !m_runManager->isRunning())
    updateStatusLabel();
  m_tree->viewport()->update();
  setContentFont(PanelTypography::contentFont(this));
}

void TestPanel::setContentFont(const QFont &font) {
  const qreal oldSize = m_tree->font().pointSizeF();
  PanelTypography::apply(this, font);
  if (m_userSizedColumns && oldSize > 0 &&
      m_tree->font().pointSizeF() != oldSize) {
    m_fittingColumns = true;
    const qreal scale = m_tree->font().pointSizeF() / oldSize;
    for (int column = 0; column < m_tree->columnCount(); ++column)
      m_tree->setColumnWidth(column,
                             qRound(m_tree->columnWidth(column) * scale));
    m_fittingColumns = false;
  }
  m_configCombo->setMaximumWidth(
      qMax(220, fontMetrics().horizontalAdvance("Workspace pytest") + 48));
  m_filterCombo->setMaximumWidth(
      qMax(180, fontMetrics().horizontalAdvance("All results") + 48));
  const int iconSize = m_toolbar->iconSize().width();
  const QColor text = m_theme.foregroundColor.isValid()
                          ? m_theme.foregroundColor
                          : palette().color(QPalette::Text);
  const QColor secondary = m_theme.surfaceColor.isValid()
                               ? UIStyleHelper::secondaryTextColor(m_theme)
                               : text;
  m_runAllAction->setIcon(
      DebugGlyphs::icon(DebugGlyphs::Glyph::Play, text, iconSize));
  m_runSelectedAction->setIcon(
      DebugGlyphs::icon(DebugGlyphs::Glyph::Play, secondary, iconSize));
  m_runFailedAction->setIcon(
      DebugGlyphs::icon(DebugGlyphs::Glyph::Restart, secondary, iconSize));
  m_stopAction->setIcon(
      DebugGlyphs::icon(DebugGlyphs::Glyph::Stop, secondary, iconSize));
  m_discoverAction->setIcon(
      WorkspaceIcons::icon(WorkspaceIcons::Glyph::Search, secondary, iconSize));
  m_debugAction->setIcon(
      WorkspaceIcons::icon(WorkspaceIcons::Glyph::Debug, secondary, iconSize));
  for (auto *item : m_testItems)
    updateTreeItemIcon(
        item, static_cast<TestStatus>(item->data(0, StatusRole).toInt()));
  updateSuiteSummaries();
  fitTestColumns();
  if (auto *item = m_tree->currentItem())
    m_tree->scrollToItem(item);
}

void TestPanel::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  const auto orientation = width() >= 1100 ? Qt::Horizontal : Qt::Vertical;
  if (m_splitter->orientation() != orientation)
    m_splitter->setOrientation(orientation);
  if (!m_userSizedColumns)
    fitTestColumns();
}

bool TestPanel::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_tree->viewport() && event->type() == QEvent::Resize &&
      !m_userSizedColumns)
    QTimer::singleShot(0, this, &TestPanel::fitTestColumns);
  if (watched == m_tree && event->type() == QEvent::KeyPress) {
    const auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
      if (key->modifiers() & Qt::ControlModifier)
        runSelectedTests();
      else
        openSelectedTest();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void TestPanel::fitTestColumns() {
  if (!m_tree || m_userSizedColumns)
    return;
  m_fittingColumns = true;
  const QFontMetrics metrics(m_tree->font());
  const int available = qMax(360, m_tree->viewport()->width());
  const int status = qMax(metrics.horizontalAdvance(tr("Running")),
                          qMax(metrics.horizontalAdvance(
                                   tr("%1 failed").arg(m_testItems.size())),
                               metrics.horizontalAdvance(
                                   tr("%1 tests").arg(m_testItems.size())))) +
                     48;
  const int time =
      m_hasDurations ? metrics.horizontalAdvance("120.00 s") + 24 : 0;
  const int sourceWidth = qMin(420, qMax(260, available / 4));
  const int readableName =
      metrics.horizontalAdvance("test_endpoint_with_multiple_conditions");
  const bool showSource =
      available >= qMax(1000, readableName + status + time + sourceWidth);
  const int source = showSource ? sourceWidth : 0;
  m_tree->setColumnHidden(2, !m_hasDurations);
  m_tree->setColumnHidden(SourceColumn, !showSource);
  m_tree->setColumnWidth(0, qMax(320, available - status - time - source - 8));
  m_tree->setColumnWidth(1, status);
  m_tree->setColumnWidth(2, qMax(80, time));
  m_tree->setColumnWidth(SourceColumn, qMax(180, source));
  m_fittingColumns = false;
  m_tree->doItemsLayout();
}

void TestPanel::updateSelectionActions() {
  const bool idle = !m_running && !m_discovering;
  const auto config = currentConfiguration();
  m_runAllAction->setEnabled(idle && config.isValid());
  m_runSelectedAction->setEnabled(idle && config.isValid() &&
                                  !m_tree->selectedItems().isEmpty());
  bool hasFailures = false;
  for (const auto &result : m_testResults)
    hasFailures |= result.status == TestStatus::Failed ||
                   result.status == TestStatus::Errored;
  m_runFailedAction->setEnabled(idle && hasFailures);
  m_debugAction->setEnabled(idle && config.isValid() &&
                            config.outputFormat == "pytest");
  m_stopAction->setEnabled(!idle);
  m_configCombo->setEnabled(idle);
  m_configureAction->setEnabled(idle);
  m_discoverAction->setEnabled(idle && m_discoveryAdapter &&
                               !m_workspaceFolder.isEmpty());
  m_clearAction->setEnabled(idle);
  if (m_openSourceAction)
    m_openSourceAction->setEnabled(m_tree->currentItem() != nullptr);
}

void TestPanel::openSelectedTest() {
  onItemDoubleClicked(m_tree->currentItem(), 0);
}

void TestPanel::runSelectedTests() {
  runItems(m_tree->selectedItems(), tr("Selected tests"));
}

void TestPanel::runItems(const QList<QTreeWidgetItem *> &selection,
                         const QString &scope) {
  auto config = currentConfiguration();
  if (!config.isValid() || m_running || m_discovering)
    return;
  QList<QTreeWidgetItem *> leaves;
  QStringList files;
  QSet<QString> ids;
  std::function<void(QTreeWidgetItem *)> collect = [&](QTreeWidgetItem *item) {
    if (!item)
      return;
    if (item->data(0, IsDetailItemRole).toBool())
      item = item->parent();
    if (!item)
      return;
    if (item->data(0, IsSuiteRole).toBool()) {
      if (config.outputFormat == "jest_json" &&
          item->data(0, IsFileGroupRole).toBool()) {
        files << item->data(0, FilePathRole).toString();
        return;
      }
      if (item->childCount() == 0 && item->data(0, IsFileGroupRole).toBool())
        files << item->data(0, FilePathRole).toString();
      for (int i = 0; i < item->childCount(); ++i)
        collect(item->child(i));
    } else {
      const QString id = item->data(0, TestIdRole).toString();
      if (!ids.contains(id)) {
        ids.insert(id);
        leaves << item;
      }
    }
  };
  for (auto *item : selection)
    collect(item);
  files.removeDuplicates();
  if (config.outputFormat == "jest_json" && !files.isEmpty()) {
    leaves.erase(std::remove_if(leaves.begin(), leaves.end(),
                                [&](QTreeWidgetItem *item) {
                                  const bool included = files.contains(
                                      item->data(0, FilePathRole).toString());
                                  if (included)
                                    ids.remove(
                                        item->data(0, TestIdRole).toString());
                                  return included;
                                }),
                 leaves.end());
    if (!leaves.isEmpty()) {
      m_statusLabel->setText(tr("Run Jest file groups separately from "
                                "individual cases in other files."));
      return;
    }
  }
  if (leaves.isEmpty() && files.isEmpty())
    return;
  recordRunRequest(config, scope);
  m_runTargets = ids.values();
  m_runTargets << files;
  if (config.outputFormat == "pytest") {
    QStringList targets;
    for (auto *item : leaves)
      targets << pytestTargets(item);
    if (targets.isEmpty())
      return;
    m_runTargets = targets;
    m_runManager->runAll(PytestIntegration::withTargets(config, targets),
                         m_workspaceFolder);
  } else if (config.outputFormat == "jest_json") {
    config.env["LIGHTPAD_JEST_SELECTION"] =
        PytestIntegration::selectionValue(ids.values());
    QStringList names;
    for (auto *item : leaves) {
      names << item->text(0);
      files << item->data(0, FilePathRole).toString();
    }
    files.removeAll(QString());
    files.removeDuplicates();
    if (names.isEmpty()) {
      config.args << "--runTestsByPath" << files;
      m_runManager->runAll(config, m_workspaceFolder);
    } else {
      config.runFailed.args << "--runTestsByPath" << files;
      m_runManager->runPattern(config, m_workspaceFolder,
                               TestRunManager::failedTestFilter(config, names));
    }
  } else if (config.outputFormat == "ctest") {
    QStringList names;
    for (auto *item : leaves)
      names << item->text(0);
    m_runManager->runPattern(config, m_workspaceFolder,
                             TestRunManager::failedTestFilter(config, names));
  } else if ((config.outputFormat == "cargo" ||
              config.outputFormat == "cargo_json") &&
             leaves.size() > 1) {
    if (QFileInfo(config.command).baseName() != "cargo") {
      m_statusLabel->setText(
          tr("This configuration supports one selected Rust test at a time."));
      return;
    }
    if (!config.args.contains("--"))
      config.args << "--";
    config.args << "--exact" << ids.values();
    m_runManager->runAll(config, m_workspaceFolder);
  } else if (!leaves.isEmpty()) {
    auto *item = leaves.first();
    const bool cargo =
        config.outputFormat == "cargo" || config.outputFormat == "cargo_json";
    const QString name =
        cargo ? item->data(0, TestIdRole).toString() : item->text(0);
    m_runManager->runSingleTest(config, m_workspaceFolder, name,
                                item->data(0, FilePathRole).toString());
  }
}

static QString workspaceConfigurationKey(const QString &folder) {
  return QStringLiteral("workspaceConfigurationId_%1")
      .arg(qHash(QDir::cleanPath(folder)));
}

static QString workspaceConfigurationId(const QString &folder) {
  if (folder.isEmpty()) {
    return {};
  }
  QSettings settings;
  settings.beginGroup("TestPanel");
  QString configId =
      settings.value(workspaceConfigurationKey(folder)).toString();
  settings.endGroup();
  if (configId.isEmpty()) {
    configId = TestConfigurationManager::instance()
                   .preferredConfigurationForPath(folder)
                   .id;
  }
  return configId;
}

void TestPanel::setWorkspaceFolder(const QString &folder) {
  if (QDir::cleanPath(m_workspaceFolder) != QDir::cleanPath(folder)) {
    if (m_discovering || m_running)
      stopTests();
    m_tree->clear();
    m_suiteItems.clear();
    m_testItems.clear();
    m_testResults.clear();
    m_discoveredTests.clear();
    m_runManager->clearResults();
    m_completedRunIds.clear();
    m_runTargets.clear();
    m_passedCount = m_failedCount = m_skippedCount = m_erroredCount = 0;
    m_hasDurations = false;
    m_runGeneration = 0;
    m_searchEdit->clear();
    m_detailPane->clear();
    m_runOutputPane->clear();
    resetRunContext();
    emit countsChanged(0, 0, 0, 0);
  }
  m_workspaceFolder = folder;
  TestConfigurationManager::instance().setWorkspaceFolder(folder);
  TestConfigurationManager::instance().loadUserConfigurations(folder);
  refreshConfigurations();
  const int workspaceIndex =
      m_configCombo->findData(workspaceConfigurationId(folder));
  if (workspaceIndex >= 0) {
    m_configCombo->setCurrentIndex(workspaceIndex);
  }
  m_autoTestRunner->setWorkspaceFolder(folder);
  m_autoTestRunner->loadSettings(folder);
  m_autoRunAction->setChecked(m_autoTestRunner->isEnabled());
  m_autoRunModeCombo->setEnabled(m_autoTestRunner->isEnabled());
  if (m_autoTestRunner->mode() != AutoRunMode::Off) {
    int modeIdx = autoRunModeToComboIndex(m_autoTestRunner->mode());
    if (modeIdx >= 0 && modeIdx < m_autoRunModeCombo->count())
      m_autoRunModeCombo->setCurrentIndex(modeIdx);
  }
  syncAutoRunConfiguration();
  updateStatusLabel();
  updateSelectionActions();
  updateEmptyState();
}

void TestPanel::runAll() {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid() || m_running || m_discovering)
    return;
  recordRunRequest(config, tr("Workspace"));
  m_runManager->runAll(config, m_workspaceFolder);
}

void TestPanel::runFailed() {
  QList<QTreeWidgetItem *> failures;
  for (auto it = m_testResults.cbegin(); it != m_testResults.cend(); ++it)
    if (it->status == TestStatus::Failed || it->status == TestStatus::Errored)
      if (auto *item = findTestItem(it.key()))
        failures << item;
  runItems(failures, tr("Failed tests"));
}

QStringList TestPanel::pytestTargets(QTreeWidgetItem *item) const {
  QStringList targets;
  if (item && item->data(0, IsDetailItemRole).toBool())
    item = item->parent();
  if (!item)
    return targets;
  if (item->data(0, IsSuiteRole).toBool()) {
    for (int i = 0; i < item->childCount(); ++i)
      targets << pytestTargets(item->child(i));
  } else {
    const QString id = item->data(0, TestIdRole).toString();
    if (id.contains("::")) {
      // Definition metadata can point to an inherited method in another module.
      // Pytest must run the collecting module from the node ID.
      const QString collectingFile = TestSource::existingFile(
          id.section("::", 0, 0), m_workspaceFolder, m_lastRunWorkingDirectory);
      targets << (collectingFile.isEmpty()
                      ? id
                      : collectingFile + id.mid(id.indexOf("::")));
    }
  }
  targets.removeDuplicates();
  return targets;
}

void TestPanel::debugAllTests() {
  const auto config = currentConfiguration();
  if (config.outputFormat != "pytest") {
    m_statusLabel->setText(
        tr("Select a pytest configuration to debug Python tests."));
    return;
  }
  emit debugTestsRequested({}, config);
}

void TestPanel::debugSelectedTests() {
  const auto config = currentConfiguration();
  if (config.outputFormat != "pytest") {
    m_statusLabel->setText(
        tr("Select a pytest configuration to debug Python tests."));
    return;
  }
  QStringList targets;
  for (auto *item : m_tree->selectedItems())
    targets << pytestTargets(item);
  targets.removeDuplicates();
  if (!m_tree->selectedItems().isEmpty() && targets.isEmpty())
    return;
  emit debugTestsRequested(targets, config);
}

void TestPanel::debugTestFile(const QString &filePath) {
  const auto config = currentConfiguration();
  if (config.outputFormat == "pytest" && !filePath.isEmpty())
    emit debugTestsRequested({filePath}, config);
}

void TestPanel::runCurrentFile(const QString &filePath) {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid())
    return;
  if (runGoogleTestScopedPath(config, filePath))
    return;
  recordRunRequest(config, tr("File"), filePath);
  m_runManager->runFile(config, m_workspaceFolder, filePath);
}

void TestPanel::runTestsForPath(const QString &path) {
  QFileInfo fi(path);
  const TestConfiguration preferredConfig =
      TestConfigurationManager::instance().preferredConfigurationForPath(path);
  if (preferredConfig.isValid()) {
    const int idx = m_configCombo->findData(preferredConfig.id);
    if (idx >= 0)
      m_configCombo->setCurrentIndex(idx);
  }

  if (fi.isFile()) {
    runCurrentFile(path);
  } else if (fi.isDir()) {

    TestConfiguration config = currentConfiguration();
    if (!config.isValid())
      return;
    if (runGoogleTestScopedPath(config, path))
      return;
    recordRunRequest(config, tr("Directory"), path);
    m_runManager->runAll(config, m_workspaceFolder, path);
  }
}

bool TestPanel::runWithConfigurationId(const QString &configId,
                                       const QString &filePath) {
  const int configIndex = m_configCombo->findData(configId);
  if (configIndex < 0) {
    return false;
  }

  if (m_configCombo->currentIndex() != configIndex) {
    m_configCombo->setCurrentIndex(configIndex);
  }

  if (filePath.isEmpty()) {
    runAll();
  } else {
    QFileInfo fileInfo(filePath);
    TestConfiguration config = currentConfiguration();
    if (!config.isValid())
      return false;
    if (runGoogleTestScopedPath(config, filePath)) {
      return true;
    } else if (fileInfo.isDir()) {
      recordRunRequest(config, tr("Directory"), filePath);
      m_runManager->runAll(config, m_workspaceFolder, filePath);
    } else {
      runCurrentFile(filePath);
    }
  }
  return true;
}

void TestPanel::stopTests() {
  if (m_discovering) {
    m_discovering = false;
    m_discoveryAdapter->cancel();
    m_discoverAction->setEnabled(true);
    m_stopAction->setEnabled(false);
    m_runAllAction->setEnabled(true);
    m_progressBar->hide();
    m_statusLabel->setText(tr("Discovery cancelled"));
    updateSelectionActions();
    return;
  }
  if (!m_running && !m_runManager->isRunning())
    return;
  m_cancelled = true;
  m_runManager->stop();
  m_statusLabel->setText(
      tr("Cancelled · %1 test(s) completed").arg(m_completedRunIds.size()));
}

void TestPanel::openConfigurationDialog() {
  if (m_workspaceFolder.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Test Configurations"),
        tr("Open a project or workspace first to configure test settings."));
    return;
  }

  TestConfigurationManager::instance().setWorkspaceFolder(m_workspaceFolder);
  TestConfigurationManager::instance().loadUserConfigurations(
      m_workspaceFolder);

  if (auto *existing = window()->findChild<TestConfigurationDialog *>()) {
    if (existing->isVisible()) {
      existing->raise();
      existing->activateWindow();
      return;
    }
  }

  auto *dialog = new TestConfigurationDialog(
      this, m_configCombo->currentData().toString());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->applyTheme(m_theme);
  dialog->show();
}

QTreeWidgetItem *TestPanel::updateTestItem(const TestResult &result) {
  QString suite = result.suite;
  if (result.id.section("::", 0, 0).endsWith(".py"))
    suite = result.id.section("::", 0, 0);
  QTreeWidgetItem *group = findOrCreateSuiteItem(suite);
  QTreeWidgetItem *item = findTestItem(result.id);
  if (!item) {
    item = new TestTreeItem();
    if (group)
      group->addChild(item);
    else
      m_tree->addTopLevelItem(item);
    m_testItems[result.id] = item;
  }
  item->setText(0, result.name);
  item->setData(0, TestIdRole, result.id);
  item->setData(0, StatusRole, static_cast<int>(result.status));
  item->setData(0, GenerationRole, m_runGeneration);
  item->setData(0, DurationRole, result.durationMs);
  if (!result.filePath.isEmpty())
    item->setData(0, FilePathRole, result.filePath);
  if (result.line > 0)
    item->setData(0, LineNumberRole, result.line);
  if (const auto it = m_discoveredTests.constFind(result.id);
      it != m_discoveredTests.constEnd()) {
    if (item->data(0, FilePathRole).toString().isEmpty())
      item->setData(0, FilePathRole, it->filePath);
    if (item->data(0, LineNumberRole).toInt() <= 0 && it->line > 0)
      item->setData(0, LineNumberRole, it->line);
  }
  item->setText(1, statusText(result.status));
  item->setText(2, durationText(result.durationMs));
  item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
  if (result.durationMs >= 0 && !m_hasDurations) {
    m_hasDurations = true;
    if (m_userSizedColumns) {
      m_fittingColumns = true;
      m_tree->setColumnHidden(2, false);
      m_tree->setColumnWidth(
          2, QFontMetrics(m_tree->font()).horizontalAdvance("120.00 s") + 24);
      m_fittingColumns = false;
    }
    fitTestColumns();
  }
  QString path;
  int line = 0;
  if (resolveSourceLocation(item, &path, &line)) {
    const QString shown = m_workspaceFolder.isEmpty()
                              ? path
                              : QDir(m_workspaceFolder).relativeFilePath(path);
    const QString filename = QFileInfo(path).fileName();
    item->setText(SourceColumn,
                  line > 0 ? filename + ':' + QString::number(line) : filename);
    item->setToolTip(SourceColumn,
                     shown + '\n' + path + ':' + QString::number(line));
  }
  const QString tooltip =
      result.id + '\n' + statusText(result.status) +
      (result.durationMs >= 0 ? " · " + durationText(result.durationMs)
                              : QString()) +
      (result.message.isEmpty() ? QString() : '\n' + result.message);
  item->setToolTip(0, tooltip);
  item->setToolTip(1, statusText(result.status));
  item->setToolTip(2,
                   result.durationMs >= 0
                       ? durationText(result.durationMs)
                       : tr("The runner did not report timing for this test."));
  updateTreeItemIcon(item, result.status);
  return item;
}

void TestPanel::onTestStarted(const TestResult &result) {
  auto *item = updateTestItem(result);
  while (item->childCount())
    delete item->takeChild(0);
  m_testResults[result.id] = result;
  if (m_tree->currentItem() == item)
    showItemDetails(item);
  updateSuiteSummaries();
  applySearchFilter();
}

void TestPanel::onTestFinished(const TestResult &result) {
  m_lastRunProducedResults = true;
  auto *item = updateTestItem(result);
  m_testResults[result.id] = result;
  m_completedRunIds.insert(result.id);
  while (item->childCount())
    delete item->takeChild(0);
  auto addDetail = [&](const QString &title, const QString &text) {
    if (text.isEmpty())
      return;
    auto *child = new QTreeWidgetItem(item);
    child->setText(0, title + text.section('\n', 0, 0).trimmed());
    child->setData(0, TestIdRole, result.id);
    child->setData(0, IsDetailItemRole, true);
    child->setToolTip(0, text);
  };
  addDetail(tr("Message: "), result.message);
  addDetail(tr("Stack Trace: "), result.stackTrace);
  addDetail(tr("stdout: "), result.stdoutOutput);
  addDetail(tr("stderr: "), result.stderrOutput);
  m_passedCount = m_failedCount = m_skippedCount = m_erroredCount = 0;
  for (const QString &id : m_completedRunIds) {
    switch (m_testResults.value(id).status) {
    case TestStatus::Passed:
      ++m_passedCount;
      break;
    case TestStatus::Failed:
      ++m_failedCount;
      break;
    case TestStatus::Skipped:
      ++m_skippedCount;
      break;
    case TestStatus::Errored:
      ++m_erroredCount;
      break;
    default:
      break;
    }
  }
  if (m_expectedTests > 0)
    m_progressBar->setValue(m_completedRunIds.size());
  updateSuiteSummaries();
  applyFilter();
  if (m_tree->currentItem() == item)
    showItemDetails(item);
  if (m_runManager->isRunning())
    updateRunningStatusLabel();
}

void TestPanel::updateSuiteSummaries() {
  for (auto *group : m_suiteItems) {
    int total = 0, passed = 0, failed = 0, running = 0, elapsed = 0;
    bool timed = false;
    for (int i = 0; i < group->childCount(); ++i) {
      auto *item = group->child(i);
      if (item->data(0, IsDetailItemRole).toBool())
        continue;
      ++total;
      const auto result =
          m_testResults.constFind(item->data(0, TestIdRole).toString());
      if (result == m_testResults.constEnd())
        continue;
      passed += result->status == TestStatus::Passed;
      failed += result->status == TestStatus::Failed ||
                result->status == TestStatus::Errored;
      running += result->status == TestStatus::Running;
      if (result->durationMs >= 0) {
        elapsed += result->durationMs;
        timed = true;
      }
    }
    group->setText(1, total == 0                 ? tr("Test file")
                      : running                  ? tr("Running")
                      : failed                   ? tr("%1 failed").arg(failed)
                      : passed == total && total ? tr("Passed")
                                                 : tr("%1 tests").arg(total));
    group->setText(2, timed ? durationText(elapsed) : QString());
    group->setData(0, DurationRole, timed ? elapsed : -1);
    group->setData(0, StatusRole,
                   static_cast<int>(running  ? TestStatus::Running
                                    : failed ? TestStatus::Failed
                                    : passed == total && total
                                        ? TestStatus::Passed
                                        : TestStatus::Queued));
    group->setToolTip(0,
                      group->text(0) + tr("\n%1 tests · %2 passed · %3 failed")
                                           .arg(total)
                                           .arg(passed)
                                           .arg(failed));
    updateTreeItemIcon(group, running                    ? TestStatus::Running
                              : failed                   ? TestStatus::Failed
                              : passed == total && total ? TestStatus::Passed
                                                         : TestStatus::Queued);
  }
}

void TestPanel::onRunStarted() {
  m_running = true;
  ++m_runGeneration;
  m_cancelled = false;
  m_expectedTests = 0;
  m_completedRunIds.clear();
  m_passedCount = m_failedCount = m_skippedCount = m_erroredCount = 0;
  m_latestStdoutLine.clear();
  m_lastRunProducedResults = false;
  for (auto it = m_testItems.begin(); it != m_testItems.end(); ++it) {
    bool scheduled = m_runTargets.isEmpty();
    for (QString target : m_runTargets) {
      if (QFileInfo(target.section("::", 0, 0)).isAbsolute()) {
        const int separator = target.indexOf("::");
        const QString file = separator >= 0 ? target.left(separator) : target;
        target = QDir(m_workspaceFolder).relativeFilePath(file) +
                 (separator >= 0 ? target.mid(separator) : QString());
      }
      scheduled |= it.key() == target || it.key().startsWith(target + "::") ||
                   it.key().startsWith(target + '/');
    }
    if (!scheduled)
      continue;
    m_testResults.remove(it.key());
    auto *item = it.value();
    item->setText(1, tr("Queued"));
    item->setText(2, QString::fromUtf8("—"));
    item->setData(0, StatusRole, static_cast<int>(TestStatus::Queued));
    item->setData(0, DurationRole, -1);
    item->setData(0, GenerationRole, m_runGeneration);
    while (item->childCount())
      delete item->takeChild(0);
    updateTreeItemIcon(item, TestStatus::Queued);
  }
  m_stopAction->setEnabled(true);
  m_runAllAction->setEnabled(false);
  m_runSelectedAction->setEnabled(false);
  m_runFailedAction->setEnabled(false);
  m_discoverAction->setEnabled(false);
  m_clearAction->setEnabled(false);
  m_progressBar->setRange(0, 0);
  m_progressBar->setVisible(true);
  updateSuiteSummaries();
  updateRunningStatusLabel();
  refreshRunDetails();
  updateEmptyState();
  updateSelectionActions();
}

void TestPanel::onRunFinished(int passed, int failed, int skipped,
                              int errored) {
  m_running = false;
  m_passedCount = passed;
  m_failedCount = failed;
  m_skippedCount = skipped;
  m_erroredCount = errored;

  m_stopAction->setEnabled(false);
  m_runAllAction->setEnabled(true);
  m_runFailedAction->setEnabled(failed > 0 || errored > 0);
  m_discoverAction->setEnabled(true);
  m_clearAction->setEnabled(true);
  m_progressBar->setVisible(false);

  for (auto *item : m_testItems) {
    if (item->data(0, StatusRole).toInt() ==
            static_cast<int>(TestStatus::Running) ||
        item->text(1) == tr("Queued")) {
      item->setText(1, m_cancelled ? tr("Cancelled") : tr("Not run"));
      item->setData(0, StatusRole, static_cast<int>(TestStatus::Queued));
      const QString id = item->data(0, TestIdRole).toString();
      if (m_testResults.contains(id))
        m_testResults[id].status = TestStatus::Queued;
    }
  }

  updateStatusLabel();
  updateSuiteSummaries();
  refreshRunDetails();
  updateEmptyState();
  emit countsChanged(passed, failed, skipped, errored);
  updateSelectionActions();
  showItemDetails(m_tree->currentItem());
  if (m_lastRunExitCode != 0 && !m_cancelled && failed + errored == 0) {
    setDetailsExpanded(true);
    m_detailTabs->setCurrentWidget(m_runOutputPane);
  }
}

void TestPanel::onOutputLine(const QString &line, bool isError) {
  if (!line.isEmpty()) {
    if (!m_lastRunOutput.isEmpty() && !m_lastRunOutput.endsWith('\n') &&
        !line.startsWith('\n')) {
      m_lastRunOutput += '\n';
    }
    m_lastRunOutput += line;
    if (!m_lastRunOutput.endsWith('\n'))
      m_lastRunOutput += '\n';
    constexpr int maxRetainedOutput = 1024 * 1024;
    if (m_lastRunOutput.size() > maxRetainedOutput) {
      int cut = m_lastRunOutput.size() - maxRetainedOutput;
      const int newline = m_lastRunOutput.indexOf('\n', cut);
      if (newline >= 0)
        cut = newline + 1;
      m_lastRunOutput.remove(0, cut);
    }
  }

  if (!m_runDetailsRefreshTimer->isActive())
    m_runDetailsRefreshTimer->start();

  if (isError)
    return;
  const QString trimmed = line.trimmed();
  if (trimmed.isEmpty())
    return;
  m_latestStdoutLine = trimmed;
  updateRunningStatusLabel();
}

void TestPanel::onProcessStarted(const QString &command,
                                 const QStringList &args,
                                 const QString &workingDirectory) {
  if (!m_hasRunContext || !m_lastRunCommand.isEmpty()) {
    const QString file = m_autoTestRunner->mode() == AutoRunMode::AllOnSave
                             ? QString()
                             : m_autoTestRunner->lastFilePath();
    recordRunRequest(currentConfiguration(), tr("Automatic run"), file);
  }
  m_lastRunCommand = command;
  m_lastRunArgs = args;
  m_lastRunWorkingDirectory = workingDirectory;
  refreshRunDetails();
}

void TestPanel::onProcessFinished(int exitCode, bool normalExit) {
  m_lastRunExitCode = exitCode;
  m_lastRunExitedNormally = normalExit;
  refreshRunDetails();
}

void TestPanel::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  QString filePath;
  int line = 0;
  if (resolveSourceLocation(item, &filePath, &line))
    emit locationClicked(filePath, line, 0);
}

void TestPanel::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;
  if (item->data(0, IsDetailItemRole).toBool())
    item = item->parent();
  m_tree->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
  showItemDetails(item);
  setDetailsExpanded(true);
  m_detailTabs->setCurrentWidget(m_detailPane);
  const QString id = item->data(0, TestIdRole).toString();
  QTimer::singleShot(0, this, [this, id]() {
    auto *selected = m_testItems.value(id, m_suiteItems.value(id));
    if (selected)
      m_tree->scrollToItem(selected);
  });
  if (!(QApplication::keyboardModifiers() &
        (Qt::ControlModifier | Qt::ShiftModifier)))
    onItemDoubleClicked(item, 0);
}

void TestPanel::openSourceLink(const QUrl &url) {
  if (url.scheme() != "lightpad-source")
    return;
  const QUrlQuery query(url);
  const QString path =
      TestSource::existingFile(query.queryItemValue("file"), m_workspaceFolder,
                               m_lastRunWorkingDirectory);
  if (!path.isEmpty())
    emit locationClicked(path, qMax(1, query.queryItemValue("line").toInt()),
                         0);
}

void TestPanel::showItemDetails(QTreeWidgetItem *item) {
  if (!item)
    return;
  if (item->data(0, IsDetailItemRole).toBool())
    item = item->parent();
  if (!item)
    return;
  const QString id = item->data(0, TestIdRole).toString();
  QString title = item->text(0).toHtmlEscaped();
  title.replace('_', QString::fromUtf8("_\u200b"));
  title.replace("::", QString::fromUtf8("::\u200b"));
  QString html = "<p><b>" + title + "</b></p>";
  if (item->data(0, IsSuiteRole).toBool()) {
    html +=
        "<p>" + item->toolTip(0).toHtmlEscaped().replace('\n', "<br>") + "</p>";
  } else {
    html += "<p><b>" + item->text(1).toHtmlEscaped() + "</b>";
    if (m_testResults.contains(id)) {
      const auto &result = m_testResults[id];
      if (result.durationMs >= 0)
        html += " · " + durationText(result.durationMs);
    }
    html += "</p>";
    if (m_testResults.contains(id) &&
        item->data(0, GenerationRole).toInt() < m_runGeneration)
      html += "<p>" + tr("This result is from an earlier run.") + "</p>";
    QString path;
    int line = 0;
    if (resolveSourceLocation(item, &path, &line)) {
      QUrl link("lightpad-source://definition");
      QUrlQuery query;
      query.addQueryItem("file", path);
      query.addQueryItem("line", QString::number(qMax(1, line)));
      link.setQuery(query);
      html += "<p><a href=\"" + link.toString().toHtmlEscaped() + "\">" +
              tr("Go to test definition: %1:%2")
                  .arg(QDir(m_workspaceFolder).relativeFilePath(path))
                  .arg(qMax(1, line))
                  .toHtmlEscaped() +
              "</a></p>";
    }
    html += "<p><b>" + tr("Test ID") + "</b><br>" + id.toHtmlEscaped() + "</p>";
    const auto result = m_testResults.constFind(id);
    if (result != m_testResults.constEnd()) {
      auto section = [&](const QString &title, const QString &text) {
        if (text.isEmpty())
          return;
        QString rendered;
        const QRegularExpression frame(
            R"frame((?:File "([^"]+)", line (\d+)|([^\n"'()]+?\.(?:py|cpp|cc|cxx|c|h|js|jsx|mjs|cjs|ts|tsx|go|rs)):(\d+)(?::|$)))frame");
        for (const QString &row : text.split('\n')) {
          const auto match = frame.match(row);
          const QString file = match.captured(1).isEmpty() ? match.captured(3)
                                                           : match.captured(1);
          QString path = TestSource::existingFile(
              file.trimmed(), m_workspaceFolder, m_lastRunWorkingDirectory);
          // Stack frames can prefix a path with "at" or "panicked at".
          for (int i = 0; path.isEmpty() && i < file.size(); ++i)
            if (file[i].isSpace())
              path = TestSource::existingFile(file.mid(i + 1).trimmed(),
                                              m_workspaceFolder,
                                              m_lastRunWorkingDirectory);
          if (!path.isEmpty()) {
            QUrl link("lightpad-source://failure");
            QUrlQuery query;
            query.addQueryItem("file", path);
            query.addQueryItem("line", match.captured(2).isEmpty()
                                           ? match.captured(4)
                                           : match.captured(2));
            link.setQuery(query);
            rendered += "<a href=\"" + link.toString().toHtmlEscaped() + "\">" +
                        row.toHtmlEscaped() + "</a>\n";
          } else
            rendered += row.toHtmlEscaped() + '\n';
        }
        html += "<h4>" + title.toHtmlEscaped() +
                "</h4><pre style='white-space: pre-wrap'>" + rendered +
                "</pre>";
      };
      section(tr("Message"), result->message);
      section(tr("Traceback"), result->stackTrace);
      section(tr("Captured stdout"), result->stdoutOutput);
      section(tr("Captured stderr"), result->stderrOutput);
      if (result->message.isEmpty() && result->stackTrace.isEmpty() &&
          result->stdoutOutput.isEmpty() && result->stderrOutput.isEmpty())
        html += "<p>" + tr("No captured output for this test.") + "</p>";
    }
  }
  m_detailPane->setHtml(html);
}

void TestPanel::onFilterChanged(int index) {
  Q_UNUSED(index)
  applyFilter();
}

void TestPanel::onConfigChanged(int index) {
  Q_UNUSED(index)
  syncAutoRunConfiguration();
  updateDiscoveryAdapterForConfig();
  updateSelectionActions();
}

void TestPanel::onContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_tree->itemAt(pos);
  if (!item)
    return;

  QMenu menu(this);
  if (m_theme.surfaceColor.isValid()) {
    menu.setStyleSheet(UIStyleHelper::contextMenuStyle(m_theme));
  }

  if (item->data(0, IsDetailItemRole).toBool())
    item = item->parent();
  if (!item)
    return;
  m_tree->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);

  QString testId = item->data(0, TestIdRole).toString();
  QString testName = item->text(0);
  QString filePath = item->data(0, FilePathRole).toString();

  bool isSuite = item->data(0, IsSuiteRole).toBool();

  const bool idle = !m_running && !m_discovering;
  if (idle && currentConfiguration().outputFormat == "pytest") {
    const QStringList targets = pytestTargets(item);
    if (!targets.isEmpty()) {
      menu.addAction(isSuite ? tr("Debug Suite") : tr("Debug This Test"),
                     [this, targets]() {
                       emit debugTestsRequested(targets,
                                                currentConfiguration());
                     });
    }
  }

  if (idle) {
    menu.addAction(isSuite ? tr("Run Suite") : tr("Run This Test"),
                   [this, item, isSuite]() {
                     runItems({item}, isSuite ? tr("Suite") : tr("Test"));
                   });
  }

  QString sourceFilePath;
  int sourceLine = 0;
  if (resolveSourceLocation(item, &sourceFilePath, &sourceLine)) {
    menu.addAction(tr("Go to Source"), [this, sourceFilePath, sourceLine]() {
      emit locationClicked(sourceFilePath, sourceLine, 0);
    });
  }

  menu.addAction(tr("Copy Name"), [testName]() {
    QApplication::clipboard()->setText(testName);
  });
  menu.addAction(tr("Copy Test ID"),
                 [testId]() { QApplication::clipboard()->setText(testId); });

  menu.addSeparator();

  if (m_testResults.contains(testId)) {
    menu.addAction(tr("Show Output"), [this, item]() {
      showItemDetails(item);
      setDetailsExpanded(true);
      m_detailTabs->setCurrentWidget(m_detailPane);
    });
  }

  menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void TestPanel::updateStatusLabel() {
  int total = m_passedCount + m_failedCount + m_skippedCount + m_erroredCount;

  if (total == 0 && m_hasRunContext) {
    QString label = tr("No tests were reported");
    if (!m_runManager || !m_runManager->isRunning())
      label += tr(" (exit %1)").arg(m_lastRunExitCode);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setText(label);
    return;
  }
  if (total == 0) {
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setText(tr("Ready"));
    return;
  }

  QColor successColor = m_theme.testPassedColor;
  QColor errorColor = m_theme.testFailedColor;
  QColor warningColor = m_theme.testSkippedColor;
  QColor mutedColor = UIStyleHelper::secondaryTextColor(m_theme);

  QString text =
      QString("<span style='color:%6'>Last run: "
              "<span style='color:%1'>%2 passed</span>"
              " &nbsp; "
              "<span style='color:%3'>%4 failed</span>"
              " &nbsp; "
              "<span style='color:%5'>%7 skipped</span>"
              " &nbsp; "
              "<span style='color:%8'>%9 errors</span>"
              "</span>")
          .arg(successColor.name())
          .arg(m_passedCount)
          .arg(errorColor.name())
          .arg(m_failedCount)
          .arg(warningColor.name())
          .arg(mutedColor.name())
          .arg(m_skippedCount)
          .arg(UIStyleHelper::toneColor(m_theme, UIStyleHelper::Tone::Warning)
                   .name())
          .arg(m_erroredCount);
  m_statusLabel->setTextFormat(Qt::RichText);
  m_statusLabel->setText(text);
  m_statusLabel->setToolTip(
      tr("Last run: %1 tests completed. %2 tests in the explorer.")
          .arg(total)
          .arg(m_testItems.size()));
}

void TestPanel::updateRunningStatusLabel() {
  constexpr int kMaxLen = 120;
  QString label = m_expectedTests > 0 ? tr("Running tests… %1 / %2 completed")
                                            .arg(m_completedRunIds.size())
                                            .arg(m_expectedTests)
                                      : tr("Running tests...");
  if (!m_latestStdoutLine.isEmpty()) {
    QString display = m_latestStdoutLine;
    if (display.length() > kMaxLen)
      display = display.left(kMaxLen) + QChar(0x2026);
    label += QString(QChar(0x20)) + QChar(0x2502) + QChar(0x20) + display;
  }
  m_statusLabel->setTextFormat(Qt::PlainText);
  m_statusLabel->setText(label);
}

void TestPanel::updateTreeItemIcon(QTreeWidgetItem *item, TestStatus status) {
  QColor color;
  switch (status) {
  case TestStatus::Passed:
    color = m_theme.testPassedColor;
    break;
  case TestStatus::Failed:
    color = m_theme.testFailedColor;
    break;
  case TestStatus::Skipped:
    color = m_theme.testSkippedColor;
    break;
  case TestStatus::Errored:
    color = m_theme.testFailedColor;
    break;
  case TestStatus::Running:
    color = m_theme.testRunningColor;
    break;
  case TestStatus::Queued:
    color = m_theme.testQueuedColor;
    break;
  }
  if (!color.isValid())
    color = palette().color(QPalette::Text);
  item->setIcon(0, testStatusIcon(status, color, m_tree->iconSize().width()));
  item->setForeground(1, QBrush(color));
}

QTreeWidgetItem *TestPanel::findOrCreateSuiteItem(const QString &suite) {
  if (suite.isEmpty())
    return nullptr;

  if (m_suiteItems.contains(suite))
    return m_suiteItems[suite];

  auto *item = new TestTreeItem();
  item->setText(0, QFileInfo(suite).isAbsolute() && QFileInfo(suite).isFile()
                       ? QDir(m_workspaceFolder).relativeFilePath(suite)
                       : suite);
  item->setData(0, TestIdRole, suite);
  item->setData(0, IsSuiteRole, true);
  m_tree->addTopLevelItem(item);
  item->setExpanded(true);
  m_suiteItems[suite] = item;
  return item;
}

QTreeWidgetItem *TestPanel::findTestItem(const QString &id) {
  if (m_testItems.contains(id))
    return m_testItems[id];
  return nullptr;
}

const DiscoveredTest *
TestPanel::findDiscoveredTestByName(const QString &name) const {
  const DiscoveredTest *match = nullptr;
  for (auto it = m_discoveredTests.cbegin(); it != m_discoveredTests.cend();
       ++it) {
    if (it.value().name == name && !it.value().filePath.isEmpty()) {
      if (match && match->filePath != it->filePath)
        return nullptr;
      match = &it.value();
    }
  }
  return match;
}

bool TestPanel::supportsGoogleTestPathScoping(
    const TestConfiguration &config) const {
  if (config.outputFormat != "ctest")
    return false;

  const QString templateKey =
      config.templateId.isEmpty() ? config.id : config.templateId;
  return config.language.compare("C++", Qt::CaseInsensitive) == 0 ||
         templateKey == "gtest_cmake" || templateKey == "gtest_make";
}

bool TestPanel::runGoogleTestScopedPath(const TestConfiguration &config,
                                        const QString &path) {
  if (!supportsGoogleTestPathScoping(config))
    return false;

  QFileInfo info(path);
  if (!info.exists())
    return false;

  const QString scopeLabel = info.isDir() ? tr("Directory") : tr("File");
  recordRunRequest(config, scopeLabel, path);

  const QStringList testNames = googleTestNamesForPath(config, path);
  if (testNames.isEmpty()) {
    m_lastRunCommand = tr("No command executed");
    m_lastRunOutput =
        tr("No matching Google Test cases were found under:\n%1").arg(path);
    refreshRunDetails();
    updateStatusLabel();
    updateEmptyState();
    return true;
  }

  const QString pattern = QString("^(%1)$").arg(toRegexAlternation(testNames));
  m_runManager->runPattern(config, m_workspaceFolder, pattern, path);
  return true;
}

QStringList TestPanel::googleTestNamesForPath(const TestConfiguration &config,
                                              const QString &path) const {
  QStringList names;
  QFileInfo pathInfo(path);
  if (!pathInfo.exists())
    return names;

  QStringList extensions = config.extensions;
  const QStringList commonCppExtensions = {"cpp", "cc",  "cxx", "c++", "h",
                                           "hh",  "hpp", "hxx", "ipp", "inl"};
  for (const QString &extension : commonCppExtensions)
    appendUnique(extensions, extension);

  auto processFile = [&names](const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      return;
    const QString content = QString::fromUtf8(file.readAll());
    const QStringList fileNames = extractGoogleTestNames(content);
    for (const QString &name : fileNames)
      appendUnique(names, name);
  };

  if (pathInfo.isFile()) {
    processFile(pathInfo.absoluteFilePath());
    return names;
  }

  QStringList nameFilters;
  for (const QString &extension : extensions)
    nameFilters.append(QString("*.%1").arg(extension));

  QDirIterator it(pathInfo.absoluteFilePath(), nameFilters, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext())
    processFile(it.next());

  return names;
}

void TestPanel::resetRunContext() {
  m_lastRunConfigurationName.clear();
  m_lastRunScope.clear();
  m_lastRunTargetPath.clear();
  m_lastRunCommand.clear();
  m_lastRunArgs.clear();
  m_lastRunWorkingDirectory.clear();
  m_lastRunOutput.clear();
  m_lastRunExitCode = 0;
  m_lastRunExitedNormally = true;
  m_hasRunContext = false;
  m_lastRunProducedResults = false;
}

void TestPanel::recordRunRequest(const TestConfiguration &config,
                                 const QString &scope,
                                 const QString &targetPath) {
  m_lastRunConfigurationName = config.name;
  m_runTargets.clear();
  if (!targetPath.isEmpty() && QFileInfo(targetPath).exists())
    m_runTargets << targetPath;
  m_lastRunScope = scope;
  m_lastRunTargetPath = targetPath;
  m_lastRunCommand.clear();
  m_lastRunArgs.clear();
  m_lastRunWorkingDirectory.clear();
  m_lastRunOutput.clear();
  m_lastRunExitCode = 0;
  m_lastRunExitedNormally = true;
  m_hasRunContext = true;
  m_lastRunProducedResults = false;
  refreshRunDetails();
  updateEmptyState();
}

void TestPanel::refreshRunDetails() {
  if (!m_hasRunContext)
    return;

  auto *scroll = m_runOutputPane->verticalScrollBar();
  const int position = scroll->value();
  const bool follow = position >= scroll->maximum();
  m_runOutputPane->setPlainText(buildRunDetailsText());
  scroll->setValue(follow ? scroll->maximum() : position);
}

QString TestPanel::buildRunDetailsText() const {
  QStringList lines;
  lines << tr("Run Details");
  if (!m_lastRunConfigurationName.isEmpty())
    lines << tr("Configuration: %1").arg(m_lastRunConfigurationName);
  if (!m_lastRunScope.isEmpty())
    lines << tr("Scope: %1").arg(m_lastRunScope);
  if (!m_lastRunTargetPath.isEmpty())
    lines << tr("Target: %1").arg(m_lastRunTargetPath);
  if (!m_workspaceFolder.isEmpty())
    lines << tr("Workspace: %1").arg(m_workspaceFolder);
  if (!m_lastRunWorkingDirectory.isEmpty())
    lines << tr("Working directory: %1").arg(m_lastRunWorkingDirectory);

  QString commandLine = m_lastRunCommand;
  for (const QString &arg : m_lastRunArgs) {
    if (!commandLine.isEmpty())
      commandLine += ' ';
    commandLine += formatCommandArgument(arg);
  }
  lines << tr("Command: %1")
               .arg(commandLine.isEmpty() ? tr("Preparing...") : commandLine);

  QString state = m_running ? tr("Running") : tr("Finished");
  if (m_cancelled)
    state = tr("Cancelled");
  else if (!m_running) {
    state += tr(" (exit code %1%2)")
                 .arg(m_lastRunExitCode)
                 .arg(m_lastRunExitedNormally ? QString() : tr(", abnormal"));
  }
  lines << tr("State: %1").arg(state);

  if (!m_running) {
    if (m_lastRunProducedResults) {
      lines << tr("Result: %1 test item(s) reported.")
                   .arg(m_completedRunIds.size());
    } else {
      lines << tr("Result: No tests were reported for this run.");
    }
  }

  if (!m_lastRunOutput.trimmed().isEmpty()) {
    lines << QString();
    lines << tr("Raw output:");
    lines << m_lastRunOutput.trimmed();
  }

  return lines.join('\n');
}

void TestPanel::setDetailsExpanded(bool expanded) {
  if (m_detailToggleButton->isChecked() != expanded)
    m_detailToggleButton->setChecked(expanded);

  m_detailToggleButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
  m_detailSection->setVisible(expanded);
  if (!expanded)
    return;

  QList<int> sizes = m_splitter->sizes();
  if (sizes.size() != 2)
    return;

  {
    const int total =
        qMax(m_splitter->sizes().value(0) + m_splitter->sizes().value(1), 1);
    const int desiredDetails = qMax(180, total * 2 / 5);
    if (sizes[1] < desiredDetails) {
      sizes[1] = desiredDetails;
      sizes[0] = qMax(0, total - desiredDetails);
      m_splitter->setSizes(sizes);
    }
  }
}

bool TestPanel::resolveSourceLocation(QTreeWidgetItem *item, QString *filePath,
                                      int *line) {
  if (!item || !filePath || !line)
    return false;

  if (item->data(0, IsDetailItemRole).toBool()) {
    item = item->parent();
    if (!item)
      return false;
  }

  if (item->data(0, IsSuiteRole).toBool()) {
    const QString path =
        TestSource::existingFile(item->data(0, TestIdRole).toString(),
                                 m_workspaceFolder, m_lastRunWorkingDirectory);
    if (path.isEmpty())
      return false;
    *filePath = path;
    *line = 1;
    return true;
  }

  QString resolvedPath;
  const QString itemPath = item->data(0, FilePathRole).toString();
  const int itemLine = item->data(0, LineNumberRole).toInt();
  resolvedPath = TestSource::existingFile(itemPath, m_workspaceFolder,
                                          m_lastRunWorkingDirectory);
  if (currentConfiguration().outputFormat == "ctest" &&
      (resolvedPath.isEmpty() || resolvedPath.endsWith(".cmake") ||
       resolvedPath.endsWith("CMakeLists.txt"))) {
    if (resolveWorkspaceSourceLocation(item, filePath, line)) {
      item->setData(0, FilePathRole, *filePath);
      item->setData(0, LineNumberRole, *line);
      return true;
    }
  }
  if (!resolvedPath.isEmpty()) {
    *filePath = resolvedPath;
    *line = itemLine >= 0 ? itemLine : 0;
    if (*line <= 0 && resolvedPath.endsWith(".py")) {
      const QString id = item->data(0, TestIdRole).toString();
      const QString name =
          id.contains("::") ? id.mid(id.indexOf("::") + 2) : item->text(0);
      *line = TestSource::pythonDefinitionLine(resolvedPath, name);
      item->setData(0, LineNumberRole, *line);
    }
    if (resolvedPath != itemPath)
      item->setData(0, FilePathRole, resolvedPath);
    return true;
  }

  const QString id = item->data(0, TestIdRole).toString();
  const QString fromId = TestSource::existingFile(
      id.section("::", 0, 0), m_workspaceFolder, m_lastRunWorkingDirectory);
  if (!fromId.isEmpty()) {
    item->setData(0, FilePathRole, fromId);
    return resolveSourceLocation(item, filePath, line);
  }

  if (const DiscoveredTest *dt = findDiscoveredTestByName(item->text(0))) {
    if (resolveNavigableFilePath(dt->filePath, m_workspaceFolder,
                                 &resolvedPath)) {
      *filePath = resolvedPath;
      *line = dt->line >= 0 ? dt->line : 0;
      item->setData(0, FilePathRole, resolvedPath);
      item->setData(0, LineNumberRole, *line);
      return true;
    }
  }

  if (!resolveWorkspaceSourceLocation(item, &resolvedPath, line))
    return false;

  *filePath = resolvedPath;
  item->setData(0, FilePathRole, resolvedPath);
  item->setData(0, LineNumberRole, *line);
  return true;
}

bool TestPanel::resolveWorkspaceSourceLocation(QTreeWidgetItem *item,
                                               QString *filePath,
                                               int *line) const {
  if (!item || !filePath || !line || m_workspaceFolder.isEmpty())
    return false;

  const TestConfiguration config = currentConfiguration();
  if (config.outputFormat == "cargo" || config.outputFormat == "cargo_json")
    return TestSource::rustDefinition(m_workspaceFolder,
                                      item->data(0, TestIdRole).toString(),
                                      filePath, line);
  const bool shouldUseCppLookup =
      config.outputFormat == "ctest" ||
      config.language.compare("C++", Qt::CaseInsensitive) == 0;
  if (!shouldUseCppLookup)
    return false;
  if ((config.language == "C" || item->text(0).startsWith("test_")) &&
      TestSource::namedCDefinition(m_workspaceFolder, item->text(0), filePath,
                                   line))
    return true;

  QString suiteName;
  QString testName = item->text(0).trimmed();
  if (QTreeWidgetItem *parent = item->parent();
      parent && parent->data(0, IsSuiteRole).toBool()) {
    suiteName = parent->text(0).trimmed();
  }

  const QString testId = item->data(0, TestIdRole).toString();
  if (suiteName.isEmpty() && m_testResults.contains(testId))
    suiteName = m_testResults.value(testId).suite.trimmed();
  if (suiteName.isEmpty() && m_discoveredTests.contains(testId))
    suiteName = m_discoveredTests.value(testId).suite.trimmed();

  if (suiteName.isEmpty()) {
    const int dotIndex = testName.indexOf('.');
    if (dotIndex > 0 && dotIndex + 1 < testName.size()) {
      suiteName = testName.left(dotIndex).trimmed();
      testName = testName.mid(dotIndex + 1).trimmed();
    }
  }

  QStringList suiteCandidates = googleTestTokenCandidates(suiteName);
  QStringList testCandidates = googleTestTokenCandidates(testName);
  if (suiteCandidates.isEmpty() || testCandidates.isEmpty())
    return false;

  QStringList extensions = config.extensions;
  const QStringList commonCppExtensions = {"cpp", "cc",  "cxx", "c++", "h",
                                           "hh",  "hpp", "hxx", "ipp", "inl"};
  for (const QString &extension : commonCppExtensions)
    appendUnique(extensions, extension);

  QStringList nameFilters;
  nameFilters.reserve(extensions.size());
  for (const QString &extension : extensions)
    nameFilters.append(QString("*.%1").arg(extension));

  const QString workspaceRoot = QDir(m_workspaceFolder).absolutePath();
  const QString buildRoot = QDir(workspaceRoot).absoluteFilePath("build") + '/';
  const QString gitRoot = QDir(workspaceRoot).absoluteFilePath(".git") + '/';
  const QString lightpadRoot =
      QDir(workspaceRoot).absoluteFilePath(".lightpad") + '/';

  for (const QString &candidatePath :
       TestSource::sourceFiles(workspaceRoot, nameFilters)) {
    if (candidatePath.startsWith(buildRoot) ||
        candidatePath.startsWith(gitRoot) ||
        candidatePath.startsWith(lightpadRoot)) {
      continue;
    }

    const QFileInfo info(candidatePath);
    if (info.size() > 1024 * 1024)
      continue;

    QFile file(candidatePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    const QString content = QString::fromUtf8(file.readAll());
    int matchedLine = 0;
    if (!findGoogleTestMacroLocation(content, suiteCandidates, testCandidates,
                                     &matchedLine)) {
      continue;
    }

    *filePath = info.absoluteFilePath();
    *line = matchedLine;
    return true;
  }

  return false;
}

void TestPanel::applyFilter() { applySearchFilter(); }

void TestPanel::refreshConfigurations() {
  const QString previousId = m_configCombo->currentData().toString();
  const QString previousName = m_configCombo->currentText();
  const QString defaultId =
      TestConfigurationManager::instance().defaultConfigurationId();
  const QString defaultName =
      TestConfigurationManager::instance().defaultConfigurationName();

  QSignalBlocker blocker(m_configCombo);
  m_configCombo->clear();
  auto configs = TestConfigurationManager::instance().allConfigurations();
  for (const TestConfiguration &cfg : configs)
    m_configCombo->addItem(cfg.name, cfg.id);

  int idx = -1;
  if (!previousId.isEmpty())
    idx = m_configCombo->findData(previousId);
  if (idx < 0 && !previousName.isEmpty())
    idx = m_configCombo->findText(previousName);
  if (idx < 0 && !defaultId.isEmpty())
    idx = m_configCombo->findData(defaultId);
  if (idx < 0 && !defaultName.isEmpty())
    idx = m_configCombo->findText(defaultName);
  if (idx < 0 && m_configCombo->count() > 0)
    idx = 0;
  if (idx >= 0)
    m_configCombo->setCurrentIndex(idx);

  syncAutoRunConfiguration();
  updateDiscoveryAdapterForConfig();
}

TestConfiguration TestPanel::currentConfiguration() const {
  const QString selectedId = m_configCombo->currentData().toString();
  if (!selectedId.isEmpty()) {
    const TestConfiguration cfg =
        TestConfigurationManager::instance().configurationById(selectedId);
    if (cfg.isValid())
      return cfg;
  }

  return TestConfigurationManager::instance().configurationByName(
      m_configCombo->currentText());
}

void TestPanel::setDiscoveryAdapter(ITestDiscoveryAdapter *adapter) {
  if (m_discoveryAdapter) {
    disconnect(m_discoveryAdapter, nullptr, this, nullptr);
    if (m_ownsDiscoveryAdapter)
      delete m_discoveryAdapter;
  }
  m_discoveryAdapter = adapter;
  m_ownsDiscoveryAdapter = false;
  if (m_discoveryAdapter)
    connectDiscoveryAdapter();
}

void TestPanel::connectDiscoveryAdapter() {
  if (!m_discoveryAdapter)
    return;
  connect(m_discoveryAdapter, &ITestDiscoveryAdapter::discoveryFinished, this,
          &TestPanel::onDiscoveryFinished);
  connect(m_discoveryAdapter, &ITestDiscoveryAdapter::discoveryError, this,
          &TestPanel::onDiscoveryError);
}

void TestPanel::updateDiscoveryAdapterForConfig() {
  TestConfiguration config = currentConfiguration();
  if (!config.isValid()) {
    if (m_discoveryAdapter && m_ownsDiscoveryAdapter) {
      disconnect(m_discoveryAdapter, nullptr, this, nullptr);
      delete m_discoveryAdapter;
      m_discoveryAdapter = nullptr;
      m_ownsDiscoveryAdapter = false;
    }
    return;
  }

  if (m_discoveryAdapter && !m_ownsDiscoveryAdapter)
    return;

  QString templateKey =
      config.templateId.isEmpty() ? config.id : config.templateId;

  if (m_discoveryAdapter && m_ownsDiscoveryAdapter) {
    disconnect(m_discoveryAdapter, nullptr, this, nullptr);
    delete m_discoveryAdapter;
    m_discoveryAdapter = nullptr;
    m_ownsDiscoveryAdapter = false;
  }

  ITestDiscoveryAdapter *adapter =
      TestDiscoveryAdapterFactory::createForConfiguration(templateKey, this);
  if (adapter) {
    m_discoveryAdapter = adapter;
    m_ownsDiscoveryAdapter = true;
    connectDiscoveryAdapter();
  }
}

QString TestPanel::discoveryRootForConfiguration(
    const TestConfiguration &config) const {
  QString discoveryRoot;
  if (!config.discoveryDirectory.isEmpty()) {
    discoveryRoot = TestConfigurationManager::substituteVariables(
        config.discoveryDirectory, QString(), m_workspaceFolder);
  } else if (config.outputFormat == "pytest" &&
             !config.workingDirectory.isEmpty()) {
    discoveryRoot = TestConfigurationManager::substituteVariables(
        config.workingDirectory, QString(), m_workspaceFolder);
  } else if (m_discoveryAdapter && m_discoveryAdapter->adapterId() == "ctest" &&
             !m_workspaceFolder.isEmpty()) {
    discoveryRoot = QDir(m_workspaceFolder).absoluteFilePath("build");
  } else {
    discoveryRoot = m_workspaceFolder;
  }

  if (!discoveryRoot.isEmpty() && QFileInfo(discoveryRoot).isRelative() &&
      !m_workspaceFolder.isEmpty()) {
    discoveryRoot = QDir(m_workspaceFolder).absoluteFilePath(discoveryRoot);
  }

  return discoveryRoot;
}

void TestPanel::discoverTests() {
  if (m_workspaceFolder.isEmpty() || m_running || m_discovering)
    return;

  if (!m_discoveryAdapter) {
    m_statusLabel->setText(tr("No discovery adapter configured"));
    return;
  }

  m_statusLabel->setText(tr("Discovering tests..."));
  m_discoverAction->setEnabled(false);
  m_discovering = true;
  m_stopAction->setEnabled(true);
  m_runAllAction->setEnabled(false);
  m_progressBar->setRange(0, 0);
  m_progressBar->show();
  updateSelectionActions();
  updateEmptyState();

  if (auto *pytest = qobject_cast<PytestDiscoveryAdapter *>(m_discoveryAdapter))
    pytest->setTestConfiguration(currentConfiguration(), m_workspaceFolder);

  m_discoveryAdapter->discover(
      discoveryRootForConfiguration(currentConfiguration()));
}

void TestPanel::onDiscoveryFinished(const QList<DiscoveredTest> &tests) {
  m_discovering = false;
  m_stopAction->setEnabled(false);
  m_runAllAction->setEnabled(true);
  m_progressBar->hide();
  m_discoverAction->setEnabled(true);
  populateTreeFromDiscovery(tests);
  int files = 0;
  for (const auto &test : tests)
    files += test.isFile;
  QString summary = tr("Discovered %1 tests").arg(tests.size() - files);
  if (files)
    summary += " · " + (files == 1 ? tr("1 test file")
                                   : tr("%1 test files").arg(files));
  m_statusLabel->setText(summary);
  updateSelectionActions();
}

void TestPanel::onDiscoveryError(const QString &message) {
  m_discovering = false;
  m_stopAction->setEnabled(false);
  m_runAllAction->setEnabled(true);
  m_progressBar->hide();
  m_discoverAction->setEnabled(true);
  m_statusLabel->setText(tr("Discovery error: %1").arg(message));
  m_statusLabel->setToolTip(message);
  m_runOutputPane->setPlainText(tr("Test discovery failed\n\n%1").arg(message));
  setDetailsExpanded(true);
  m_detailTabs->setCurrentWidget(m_runOutputPane);
  updateSelectionActions();
  updateEmptyState();
}

void TestPanel::populateTreeFromDiscovery(const QList<DiscoveredTest> &tests) {
  if (m_runManager->isRunning())
    return;
  const QString selectedId =
      m_tree->currentItem()
          ? m_tree->currentItem()->data(0, TestIdRole).toString()
          : QString();
  m_tree->clear();
  m_suiteItems.clear();
  m_testItems.clear();
  m_discoveredTests.clear();
  QSet<QString> discoveredIds;
  for (const auto &test : tests) {
    m_discoveredTests[test.id] = test;
    discoveredIds.insert(test.id);
    if (test.isFile) {
      auto *group = findOrCreateSuiteItem(test.filePath);
      group->setData(0, IsFileGroupRole, true);
      group->setData(0, FilePathRole, test.filePath);
      group->setData(0, LineNumberRole, 1);
      group->setToolTip(
          0,
          test.filePath + '\n' +
              tr("Run this file to include dynamically generated test cases."));
      continue;
    }
    TestResult result = m_testResults.value(test.id);
    result.id = test.id;
    result.name = test.name;
    result.suite = test.suite;
    result.filePath = test.filePath;
    if (test.line > 0)
      result.line = test.line;
    updateTestItem(result);
  }
  for (auto it = m_testResults.begin(); it != m_testResults.end();) {
    if (!discoveredIds.contains(it.key()) && !m_suiteItems.contains(it->suite))
      it = m_testResults.erase(it);
    else {
      if (!m_testItems.contains(it.key()))
        updateTestItem(it.value());
      ++it;
    }
  }
  if (auto *selected =
          m_testItems.value(selectedId, m_suiteItems.value(selectedId)))
    m_tree->setCurrentItem(selected);
  updateSuiteSummaries();
  applySearchFilter();
  fitTestColumns();
  updateSelectionActions();
}

void TestPanel::notifyFileSaved(const QString &filePath) {
  if (m_discovering)
    return;
  syncAutoRunConfiguration();
  m_autoTestRunner->notifyFileSaved(filePath);
}

void TestPanel::onAutoRunToggled(bool checked) {
  m_autoTestRunner->setEnabled(checked);
  m_autoRunModeCombo->setEnabled(checked);
  if (checked) {
    m_autoTestRunner->setMode(
        comboIndexToAutoRunMode(m_autoRunModeCombo->currentIndex()));
  } else {
    m_autoTestRunner->setMode(AutoRunMode::Off);
  }
  m_autoTestRunner->saveSettings(m_workspaceFolder);
}

void TestPanel::onAutoRunModeChanged(int index) {
  if (m_autoTestRunner->isEnabled()) {
    m_autoTestRunner->setMode(comboIndexToAutoRunMode(index));
    m_autoTestRunner->saveSettings(m_workspaceFolder);
  }
}

void TestPanel::syncAutoRunConfiguration() {
  TestConfiguration config = currentConfiguration();
  m_autoTestRunner->setConfiguration(config);
}

void TestPanel::saveState() const {
  QSettings settings;
  settings.beginGroup("TestPanel");
  settings.setValue("lastConfigurationId", m_configCombo->currentData());
  settings.setValue("lastConfiguration", m_configCombo->currentText());
  if (!m_workspaceFolder.isEmpty()) {
    settings.setValue(workspaceConfigurationKey(m_workspaceFolder),
                      m_configCombo->currentData());
  }
  settings.setValue("lastFilter", m_filterCombo->currentIndex());
  if (m_userSizedColumns) {
    settings.setValue("headerState", m_tree->header()->saveState());
    settings.setValue("headerFontSize", m_tree->font().pointSizeF());
  } else
    settings.remove("headerState");
  settings.endGroup();
}

void TestPanel::restoreState() {
  QSettings settings;
  settings.beginGroup("TestPanel");
  QString lastConfigId = settings.value("lastConfigurationId").toString();
  QString lastConfig = settings.value("lastConfiguration").toString();
  int lastFilter = settings.value("lastFilter", 0).toInt();
  const QByteArray headerState = settings.value("headerState").toByteArray();
  const qreal headerFontSize =
      settings.value("headerFontSize", m_tree->font().pointSizeF()).toDouble();
  settings.endGroup();

  int idx =
      m_configCombo->findData(workspaceConfigurationId(m_workspaceFolder));
  if (idx < 0 && !lastConfigId.isEmpty())
    idx = m_configCombo->findData(lastConfigId);
  if (idx < 0 && !lastConfig.isEmpty())
    idx = m_configCombo->findText(lastConfig);
  if (idx >= 0)
    m_configCombo->setCurrentIndex(idx);
  if (lastFilter >= 0 && lastFilter < m_filterCombo->count())
    m_filterCombo->setCurrentIndex(lastFilter);
  if (!headerState.isEmpty()) {
    m_fittingColumns = true;
    m_userSizedColumns = m_tree->header()->restoreState(headerState);
    m_tree->setColumnHidden(2, !m_hasDurations);
    if (m_userSizedColumns && headerFontSize > 0) {
      const qreal scale = m_tree->font().pointSizeF() / headerFontSize;
      for (int column = 0; column < m_tree->columnCount(); ++column)
        m_tree->setColumnWidth(column,
                               qRound(m_tree->columnWidth(column) * scale));
    }
    m_fittingColumns = false;
  }
}

void TestPanel::onSearchTextChanged(const QString &text) {
  Q_UNUSED(text)
  applySearchFilter();
}

void TestPanel::applySearchFilter() {
  const QString search = m_searchEdit->text().trimmed();
  const int status = m_filterCombo->currentIndex();
  for (auto it = m_testItems.begin(); it != m_testItems.end(); ++it) {
    auto *item = it.value();
    const auto result = m_testResults.constFind(it.key());
    bool visible = status == 0;
    if (result != m_testResults.constEnd()) {
      visible |= status == 1 && (result->status == TestStatus::Failed ||
                                 result->status == TestStatus::Errored);
      visible |= status == 2 && result->status == TestStatus::Passed;
      visible |= status == 3 && result->status == TestStatus::Skipped;
    }
    if (visible && !search.isEmpty()) {
      visible = item->text(0).contains(search, Qt::CaseInsensitive) ||
                item->data(0, FilePathRole)
                    .toString()
                    .contains(search, Qt::CaseInsensitive) ||
                item->data(0, TestIdRole)
                    .toString()
                    .contains(search, Qt::CaseInsensitive) ||
                (item->parent() &&
                 item->parent()->text(0).contains(search, Qt::CaseInsensitive));
    }
    item->setHidden(!visible);
  }
  for (auto *suite : m_suiteItems) {
    bool visible = suite->childCount() == 0 &&
                   suite->data(0, IsFileGroupRole).toBool() && status == 0 &&
                   (search.isEmpty() ||
                    suite->text(0).contains(search, Qt::CaseInsensitive));
    for (int i = 0; i < suite->childCount(); ++i)
      visible |= !suite->child(i)->isHidden();
    suite->setHidden(!visible);
  }
  updateEmptyState();
}

void TestPanel::updateEmptyState() {
  const bool hasItems = m_tree->topLevelItemCount() > 0;
  bool hasVisibleItems = false;
  for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    hasVisibleItems |= !m_tree->topLevelItem(i)->isHidden();
  const bool running = m_running || m_discovering;
  if (!hasVisibleItems) {
    if (hasItems) {
      m_emptyStateLabel->setText(tr("No tests match these filters."));
      m_emptyAction->setText(tr("Clear filters"));
    } else if (m_discovering) {
      m_emptyStateLabel->setText(tr("Discovering tests…"));
    } else if (running) {
      m_emptyStateLabel->setText(
          tr("Running tests…\nLive output is available in Run Details."));
    } else if (m_hasRunContext) {
      m_emptyStateLabel->setText(tr("No tests were reported.\nOpen Run Details "
                                    "to inspect the command and output."));
    } else {
      m_emptyStateLabel->setText(
          m_workspaceFolder.isEmpty()
              ? tr("Open a workspace to discover tests.")
              : tr("No tests discovered yet. Choose a configuration, then "
                   "discover tests."));
      m_emptyAction->setText(tr("Discover tests"));
    }
  }
  m_emptyAction->setVisible(hasItems || (!running && !m_hasRunContext));
  m_emptyAction->setEnabled(hasItems || !m_workspaceFolder.isEmpty());
  m_emptyState->setVisible(!hasVisibleItems);
  m_emptyStateLabel->setVisible(!hasVisibleItems);
  m_tree->setVisible(hasVisibleItems);
  m_detailToggleButton->setEnabled(hasItems || m_hasRunContext);
}
