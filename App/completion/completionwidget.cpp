#include "completionwidget.h"
#include <QKeyEvent>
#include <QScrollBar>
#include <QApplication>
#include <QScreen>

CompletionWidget::CompletionWidget(QWidget* parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint)
    , m_model(new CompletionItemModel(this))
    , m_listView(new QListView(this))
    , m_docLabel(new QLabel(this))
    , m_layout(new QVBoxLayout(this))
{
    // Setup list view
    m_listView->setModel(m_model);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setFocusPolicy(Qt::NoFocus);
    m_listView->installEventFilter(this);
    
    // Setup documentation label
    m_docLabel->setWordWrap(true);
    m_docLabel->setTextFormat(Qt::RichText);
    m_docLabel->setVisible(false);
    m_docLabel->setMaximumHeight(100);
    m_docLabel->setStyleSheet("QLabel { padding: 5px; background: #f5f5f5; border-top: 1px solid #ddd; }");
    
    // Layout
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_listView);
    m_layout->addWidget(m_docLabel);
    
    // Style
    setStyleSheet(
        "CompletionWidget { background: white; border: 1px solid #ccc; }"
        "QListView { border: none; }"
        "QListView::item { padding: 3px 5px; }"
        "QListView::item:selected { background: #0078d4; color: white; }"
    );
    
    // Connections
    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &CompletionWidget::onSelectionChanged);
    connect(m_listView, &QListView::clicked, this, &CompletionWidget::onItemClicked);
    connect(m_listView, &QListView::doubleClicked, this, &CompletionWidget::onItemDoubleClicked);
    
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMinimumWidth(200);
    setMaximumWidth(500);
}

void CompletionWidget::applyTheme(const Theme& theme)
{
    QString bgColor = theme.surfaceColor.name();
    QString fgColor = theme.foregroundColor.name();
    QString borderColor = theme.borderColor.name();
    QString hoverColor = theme.hoverColor.name();
    QString selectionColor = theme.accentSoftColor.name();
    QString focusColor = theme.accentColor.name();

    setStyleSheet(
        "CompletionWidget { background: " + bgColor + "; border: 1px solid " + borderColor + "; }"
        "QListView { border: none; background: " + bgColor + "; color: " + fgColor + "; }"
        "QListView::item { padding: 3px 5px; }"
        "QListView::item:selected { background: " + selectionColor + "; color: " + fgColor + "; }"
        "QListView::item:hover { background: " + hoverColor + "; }"
        "QListView::item:focus { outline: none; border: 1px solid " + focusColor + "; }"
    );

    m_docLabel->setStyleSheet(
        "QLabel { padding: 5px; background: " + bgColor + "; color: " + fgColor + "; "
        "border-top: 1px solid " + borderColor + "; }"
    );
}

void CompletionWidget::setItems(const QList<CompletionItem>& items)
{
    m_model->setItems(items);
    
    if (!items.isEmpty()) {
        m_listView->setCurrentIndex(m_model->index(0, 0));
    }
    
    adjustSize();
}

void CompletionWidget::showAt(const QPoint& position)
{
    if (m_model->count() == 0) {
        hide();
        return;
    }
    
    // Ensure popup fits on screen
    QScreen* screen = QApplication::screenAt(position);
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    QRect screenGeom = screen->availableGeometry();
    
    QPoint adjustedPos = position;
    QSize popupSize = sizeHint();
    
    // Adjust horizontal position
    if (adjustedPos.x() + popupSize.width() > screenGeom.right()) {
        adjustedPos.setX(screenGeom.right() - popupSize.width());
    }
    
    // Adjust vertical position (show above if no room below)
    if (adjustedPos.y() + popupSize.height() > screenGeom.bottom()) {
        adjustedPos.setY(position.y() - popupSize.height() - 20);
    }
    
    move(adjustedPos);
    show();
    raise();
}

void CompletionWidget::hide()
{
    QWidget::hide();
    m_docLabel->setVisible(false);
}

void CompletionWidget::selectNext()
{
    int current = selectedIndex();
    int next = (current + 1) % m_model->count();
    m_listView->setCurrentIndex(m_model->index(next, 0));
}

void CompletionWidget::selectPrevious()
{
    int current = selectedIndex();
    int prev = (current - 1 + m_model->count()) % m_model->count();
    m_listView->setCurrentIndex(m_model->index(prev, 0));
}

void CompletionWidget::selectFirst()
{
    if (m_model->count() > 0) {
        m_listView->setCurrentIndex(m_model->index(0, 0));
    }
}

void CompletionWidget::selectLast()
{
    if (m_model->count() > 0) {
        m_listView->setCurrentIndex(m_model->index(m_model->count() - 1, 0));
    }
}

void CompletionWidget::selectPageDown()
{
    int current = selectedIndex();
    int next = qMin(current + m_maxVisibleItems, m_model->count() - 1);
    m_listView->setCurrentIndex(m_model->index(next, 0));
}

void CompletionWidget::selectPageUp()
{
    int current = selectedIndex();
    int prev = qMax(current - m_maxVisibleItems, 0);
    m_listView->setCurrentIndex(m_model->index(prev, 0));
}

CompletionItem CompletionWidget::selectedItem() const
{
    return m_model->itemAt(selectedIndex());
}

int CompletionWidget::selectedIndex() const
{
    QModelIndex idx = m_listView->currentIndex();
    return idx.isValid() ? idx.row() : -1;
}

void CompletionWidget::setMaxVisibleItems(int count)
{
    m_maxVisibleItems = count;
    adjustSize();
}

void CompletionWidget::setShowDocumentation(bool show)
{
    m_showDocumentation = show;
    updateDocumentation();
}

void CompletionWidget::setShowIcons(bool show)
{
    m_showIcons = show;
    // Would need to update the delegate to respect this
}

void CompletionWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        selectPrevious();
        event->accept();
        break;
    case Qt::Key_Down:
        selectNext();
        event->accept();
        break;
    case Qt::Key_PageUp:
        selectPageUp();
        event->accept();
        break;
    case Qt::Key_PageDown:
        selectPageDown();
        event->accept();
        break;
    case Qt::Key_Home:
        selectFirst();
        event->accept();
        break;
    case Qt::Key_End:
        selectLast();
        event->accept();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab:
        emit itemAccepted(selectedItem());
        hide();
        event->accept();
        break;
    case Qt::Key_Escape:
        emit cancelled();
        hide();
        event->accept();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

bool CompletionWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_listView && event->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void CompletionWidget::onSelectionChanged()
{
    CompletionItem item = selectedItem();
    emit itemSelected(item);
    updateDocumentation();
}

void CompletionWidget::onItemClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        emit itemSelected(m_model->itemAt(index.row()));
    }
}

void CompletionWidget::onItemDoubleClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        emit itemAccepted(m_model->itemAt(index.row()));
        hide();
    }
}

void CompletionWidget::updateDocumentation()
{
    if (!m_showDocumentation) {
        m_docLabel->setVisible(false);
        return;
    }
    
    CompletionItem item = selectedItem();
    if (!item.documentation.isEmpty()) {
        m_docLabel->setText(item.documentation);
        m_docLabel->setVisible(true);
    } else if (!item.detail.isEmpty()) {
        m_docLabel->setText(item.detail);
        m_docLabel->setVisible(true);
    } else {
        m_docLabel->setVisible(false);
    }
}

void CompletionWidget::adjustSize()
{
    int itemCount = qMin(m_model->count(), m_maxVisibleItems);
    int itemHeight = m_listView->sizeHintForRow(0);
    if (itemHeight <= 0) {
        itemHeight = 20;  // Default if no items
    }
    
    int listHeight = itemCount * itemHeight + 4;  // +4 for borders
    m_listView->setFixedHeight(listHeight);
    
    QWidget::adjustSize();
}
