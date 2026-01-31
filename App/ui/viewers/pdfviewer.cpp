#include "pdfviewer.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QPdfPageNavigator>

PdfViewer::PdfViewer(QWidget* parent)
    : QWidget(parent)
    , m_document(nullptr)
    , m_pdfView(nullptr)
    , m_toolbar(nullptr)
    , m_pageLabel(nullptr)
    , m_zoomLabel(nullptr)
    , m_pageSpinBox(nullptr)
    , m_zoomFactor(1.0)
{
    setupUi();
}

void PdfViewer::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Create toolbar
    setupToolbar();
    layout->addWidget(m_toolbar);
    
    // Create PDF document and view
    m_document = new QPdfDocument(this);
    m_pdfView = new QPdfView(this);
    m_pdfView->setDocument(m_document);
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    
    layout->addWidget(m_pdfView, 1);
    
    // Info bar at bottom
    auto* infoBar = new QHBoxLayout();
    m_pageLabel = new QLabel(this);
    m_zoomLabel = new QLabel("100%", this);
    m_zoomLabel->setAlignment(Qt::AlignRight);
    infoBar->addWidget(m_pageLabel, 1);
    infoBar->addWidget(m_zoomLabel);
    infoBar->setContentsMargins(5, 2, 5, 2);
    layout->addLayout(infoBar);
    
    setLayout(layout);
    
    // Connect document status change
    connect(m_document, &QPdfDocument::statusChanged, this, [this](QPdfDocument::Status status) {
        if (status == QPdfDocument::Status::Ready) {
            updatePageLabel();
            if (m_pageSpinBox) {
                m_pageSpinBox->setRange(1, m_document->pageCount());
                m_pageSpinBox->setValue(1);
            }
        }
    });
}

void PdfViewer::setupToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    
    // Navigation controls
    QAction* prevAction = m_toolbar->addAction("◀");
    prevAction->setToolTip("Previous Page");
    connect(prevAction, &QAction::triggered, this, &PdfViewer::previousPage);
    
    m_pageSpinBox = new QSpinBox(this);
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(1);
    m_pageSpinBox->setToolTip("Go to page");
    connect(m_pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PdfViewer::goToPage);
    m_toolbar->addWidget(m_pageSpinBox);
    
    QAction* nextAction = m_toolbar->addAction("▶");
    nextAction->setToolTip("Next Page");
    connect(nextAction, &QAction::triggered, this, &PdfViewer::nextPage);
    
    m_toolbar->addSeparator();
    
    // Zoom controls
    QAction* zoomOutAction = m_toolbar->addAction("-");
    zoomOutAction->setToolTip("Zoom Out");
    connect(zoomOutAction, &QAction::triggered, this, &PdfViewer::zoomOut);
    
    QAction* zoomInAction = m_toolbar->addAction("+");
    zoomInAction->setToolTip("Zoom In");
    connect(zoomInAction, &QAction::triggered, this, &PdfViewer::zoomIn);
    
    m_toolbar->addSeparator();
    
    // View mode controls
    QAction* fitWidthAction = m_toolbar->addAction("Fit Width");
    fitWidthAction->setToolTip("Fit to Width");
    connect(fitWidthAction, &QAction::triggered, this, &PdfViewer::fitWidth);
    
    QAction* fitPageAction = m_toolbar->addAction("Fit Page");
    fitPageAction->setToolTip("Fit Whole Page");
    connect(fitPageAction, &QAction::triggered, this, &PdfViewer::fitPage);
}

bool PdfViewer::loadPdf(const QString& filePath)
{
    if (!m_document) {
        return false;
    }
    
    QPdfDocument::Error error = m_document->load(filePath);
    if (error != QPdfDocument::Error::None) {
        QString errorMsg;
        switch (error) {
            case QPdfDocument::Error::FileNotFound:
                errorMsg = tr("File not found");
                break;
            case QPdfDocument::Error::InvalidFileFormat:
                errorMsg = tr("Invalid PDF format");
                break;
            case QPdfDocument::Error::IncorrectPassword:
                errorMsg = tr("Password protected PDF");
                break;
            case QPdfDocument::Error::UnsupportedSecurityScheme:
                errorMsg = tr("Unsupported security scheme");
                break;
            default:
                errorMsg = tr("Unknown error");
                break;
        }
        QMessageBox::warning(this, tr("PDF Viewer"),
            tr("Cannot load PDF: %1").arg(errorMsg));
        return false;
    }
    
    m_filePath = filePath;
    updatePageLabel();
    updateZoomLabel();
    
    return true;
}

bool PdfViewer::isSupportedPdfFormat(const QString& extension)
{
    return extension.toLower() == "pdf";
}

void PdfViewer::zoomIn()
{
    if (m_zoomFactor < MAX_ZOOM) {
        m_zoomFactor *= ZOOM_STEP;
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(m_zoomFactor);
        updateZoomLabel();
    }
}

void PdfViewer::zoomOut()
{
    if (m_zoomFactor > MIN_ZOOM) {
        m_zoomFactor /= ZOOM_STEP;
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(m_zoomFactor);
        updateZoomLabel();
    }
}

void PdfViewer::goToPage(int page)
{
    if (!m_pdfView || !m_document) {
        return;
    }
    
    // Page numbers are 0-indexed internally
    int pageIndex = page - 1;
    if (pageIndex >= 0 && pageIndex < m_document->pageCount()) {
        auto* navigator = m_pdfView->pageNavigator();
        if (navigator) {
            navigator->jump(pageIndex, QPointF());
        }
    }
    updatePageLabel();
}

void PdfViewer::previousPage()
{
    if (m_pageSpinBox && m_pageSpinBox->value() > 1) {
        m_pageSpinBox->setValue(m_pageSpinBox->value() - 1);
    }
}

void PdfViewer::nextPage()
{
    if (m_pageSpinBox && m_document && m_pageSpinBox->value() < m_document->pageCount()) {
        m_pageSpinBox->setValue(m_pageSpinBox->value() + 1);
    }
}

void PdfViewer::fitWidth()
{
    if (m_pdfView) {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        m_zoomFactor = m_pdfView->zoomFactor();
        updateZoomLabel();
    }
}

void PdfViewer::fitPage()
{
    if (m_pdfView) {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
        m_zoomFactor = m_pdfView->zoomFactor();
        updateZoomLabel();
    }
}

void PdfViewer::updatePageLabel()
{
    if (!m_document || !m_pageLabel) {
        return;
    }
    
    int currentPage = 1;
    if (m_pageSpinBox) {
        currentPage = m_pageSpinBox->value();
    }
    
    m_pageLabel->setText(QString("Page %1 of %2")
        .arg(currentPage)
        .arg(m_document->pageCount()));
}

void PdfViewer::updateZoomLabel()
{
    if (m_zoomLabel) {
        m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_zoomFactor * 100)));
    }
}
