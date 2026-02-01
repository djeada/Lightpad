#include "imageviewer.h"
#include <QWheelEvent>
#include <QShowEvent>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>

ImageViewer::ImageViewer(QWidget* parent)
    : QWidget(parent)
    , m_graphicsView(nullptr)
    , m_scene(nullptr)
    , m_pixmapItem(nullptr)
    , m_toolbar(nullptr)
    , m_zoomLabel(nullptr)
    , m_infoLabel(nullptr)
    , m_zoomFactor(1.0)
    , m_initialFitPending(false)
{
    setupUi();
}

void ImageViewer::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Create toolbar
    setupToolbar();
    layout->addWidget(m_toolbar);
    
    // Create graphics view for image display
    m_scene = new QGraphicsScene(this);
    m_graphicsView = new QGraphicsView(m_scene, this);
    m_graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_graphicsView->setBackgroundBrush(QBrush(QColor(40, 40, 40)));
    layout->addWidget(m_graphicsView, 1);
    
    // Info bar at bottom
    auto* infoBar = new QHBoxLayout();
    m_infoLabel = new QLabel(this);
    m_zoomLabel = new QLabel("100%", this);
    m_zoomLabel->setAlignment(Qt::AlignRight);
    infoBar->addWidget(m_infoLabel, 1);
    infoBar->addWidget(m_zoomLabel);
    infoBar->setContentsMargins(5, 2, 5, 2);
    layout->addLayout(infoBar);
    
    setLayout(layout);
}

void ImageViewer::setupToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    
    QAction* zoomInAction = m_toolbar->addAction("+");
    zoomInAction->setToolTip("Zoom In (Ctrl++)");
    connect(zoomInAction, &QAction::triggered, this, &ImageViewer::zoomIn);
    
    QAction* zoomOutAction = m_toolbar->addAction("-");
    zoomOutAction->setToolTip("Zoom Out (Ctrl+-)");
    connect(zoomOutAction, &QAction::triggered, this, &ImageViewer::zoomOut);
    
    m_toolbar->addSeparator();
    
    QAction* fitAction = m_toolbar->addAction("Fit");
    fitAction->setToolTip("Fit to Window");
    connect(fitAction, &QAction::triggered, this, &ImageViewer::fitToWindow);
    
    QAction* actualSizeAction = m_toolbar->addAction("1:1");
    actualSizeAction->setToolTip("Actual Size (100%)");
    connect(actualSizeAction, &QAction::triggered, this, &ImageViewer::actualSize);
}

bool ImageViewer::loadImage(const QString& filePath)
{
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    
    QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Image Viewer"),
            tr("Cannot load image: %1").arg(reader.errorString()));
        return false;
    }
    
    m_filePath = filePath;
    m_originalPixmap = QPixmap::fromImage(image);
    
    // Clear existing items
    m_scene->clear();
    
    // Add new image
    m_pixmapItem = m_scene->addPixmap(m_originalPixmap);
    m_scene->setSceneRect(m_originalPixmap.rect());
    
    // Update info label
    QFileInfo fileInfo(filePath);
    m_infoLabel->setText(QString("%1  |  %2 x %3  |  %4")
        .arg(fileInfo.fileName())
        .arg(m_originalPixmap.width())
        .arg(m_originalPixmap.height())
        .arg(QLocale().formattedDataSize(fileInfo.size())));
    
    // Defer fit-to-window until widget is shown and has proper size
    m_initialFitPending = true;
    
    return true;
}

bool ImageViewer::isSupportedImageFormat(const QString& extension)
{
    static const QStringList supportedFormats = {
        "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "ico", "tiff", "tif"
    };
    return supportedFormats.contains(extension.toLower());
}

void ImageViewer::zoomIn()
{
    if (m_zoomFactor < MAX_ZOOM) {
        m_zoomFactor *= ZOOM_STEP;
        applyZoom();
    }
}

void ImageViewer::zoomOut()
{
    if (m_zoomFactor > MIN_ZOOM) {
        m_zoomFactor /= ZOOM_STEP;
        applyZoom();
    }
}

void ImageViewer::fitToWindow()
{
    if (!m_pixmapItem || m_originalPixmap.isNull()) {
        return;
    }
    
    m_graphicsView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    
    // Calculate the actual zoom factor after fit
    QRectF sceneRect = m_scene->sceneRect();
    QRectF viewRect = m_graphicsView->viewport()->rect();
    
    double scaleX = viewRect.width() / sceneRect.width();
    double scaleY = viewRect.height() / sceneRect.height();
    m_zoomFactor = qMin(scaleX, scaleY);
    
    updateZoomLabel();
}

void ImageViewer::actualSize()
{
    m_zoomFactor = 1.0;
    applyZoom();
}

void ImageViewer::applyZoom()
{
    if (!m_graphicsView) {
        return;
    }
    
    m_graphicsView->resetTransform();
    m_graphicsView->scale(m_zoomFactor, m_zoomFactor);
    updateZoomLabel();
}

void ImageViewer::updateZoomLabel()
{
    if (m_zoomLabel) {
        m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_zoomFactor * 100)));
    }
}

void ImageViewer::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void ImageViewer::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    
    if (m_initialFitPending && !m_originalPixmap.isNull()) {
        m_initialFitPending = false;
        fitToWindow();
    }
}
