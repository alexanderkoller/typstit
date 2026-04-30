#include "previewwidget.h"

#include <algorithm>
#include <QPdfDocument>
#include <QRectF>
#include <QPainter>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QFile>
#include <QApplication>
#include <QPixmap>

PreviewWidget::PreviewWidget(QPdfDocument *doc, QWidget *parent)
    : QWidget(parent), m_doc(doc)
{
    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window, QColor("#ececec"));
    setPalette(p);
    setCursor(Qt::OpenHandCursor);
    setMouseTracking(false);
}

void PreviewWidget::setTempFilePath(const QString &path)
{
    m_tempFilePath = path;
}

void PreviewWidget::refresh()
{
    m_pageImage = {};
    update();
}

// Scan a rendered PDF image for the bounding box of non-white (formula) pixels.
// Returns a rect in PHYSICAL pixel coordinates, with `pad` pixels of slack.
static QRect findContentBounds(const QImage &img, int pad = 10)
{
    int W = img.width(), H = img.height();
    int x0 = W, x1 = -1, y0 = H, y1 = -1;
    for (int y = 0; y < H; ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < W; ++x) {
            QRgb c = row[x];
            // Treat fully-transparent or near-white pixels as background
            if (qAlpha(c) < 10) continue;
            if (qRed(c) >= 248 && qGreen(c) >= 248 && qBlue(c) >= 248) continue;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    }
    if (x1 < 0) return {};   // no formula pixels found — fall back
    return QRect(qMax(0, x0 - pad), qMax(0, y0 - pad),
                 qMin(W, x1 + 1 + 2*pad) - qMax(0, x0 - pad),
                 qMin(H, y1 + 1 + 2*pad) - qMax(0, y0 - pad));
}

void PreviewWidget::renderPage()
{
    m_pageImage   = {};
    m_contentRect = {};
    if (!m_doc || m_doc->pageCount() == 0) return;

    const int padding = 20;
    int availW = width()  - padding * 2;
    int availH = height() - padding * 2;
    if (availW < 10 || availH < 10) return;

    QSizeF pageSize = m_doc->pagePointSize(0);
    if (pageSize.isEmpty()) return;

    // Match Swift DraggablePDFView exactly: scale to fit with 20pt padding,
    // capped at 1.0 so the snippet never inflates beyond its natural PDF size.
    double scale = std::min({(double)availW / pageSize.width(),
                              (double)availH / pageSize.height(),
                              1.0});

    // Render at device pixel ratio for crisp Retina output
    qreal dpr = devicePixelRatioF();
    QSize renderSize(qRound(pageSize.width()  * scale * dpr),
                     qRound(pageSize.height() * scale * dpr));

    m_pageImage = m_doc->render(0, renderSize);
    m_pageImage.setDevicePixelRatio(dpr);

    // Compute tight content bounds to correct for preamble whitespace.
    // Store as logical-pixel rect so paintEvent can center just the formula.
    QRect phys = findContentBounds(m_pageImage);
    if (phys.isValid()) {
        m_contentRect = QRectF(phys.x() / dpr, phys.y() / dpr,
                               phys.width() / dpr, phys.height() / dpr);
    }
}

void PreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#ececec"));

    if (m_pageImage.isNull()) renderPage();
    if (m_pageImage.isNull()) return;

    qreal dpr = devicePixelRatioF();

    if (m_contentRect.isValid()) {
        // Center just the formula content (ignoring preamble whitespace margins).
        int cx = qRound((width()  - m_contentRect.width())  / 2.0);
        int cy = qRound((height() - m_contentRect.height()) / 2.0);
        // Source rect in physical pixels, destination in logical pixels
        QRectF src(m_contentRect.x() * dpr, m_contentRect.y() * dpr,
                   m_contentRect.width() * dpr, m_contentRect.height() * dpr);
        painter.drawImage(QRectF(cx, cy, m_contentRect.width(), m_contentRect.height()),
                          m_pageImage, src);
    } else {
        // Fallback: center the whole page image
        int imgW = qRound(m_pageImage.width()  / dpr);
        int imgH = qRound(m_pageImage.height() / dpr);
        painter.drawImage(QRectF((width()-imgW)/2.0, (height()-imgH)/2.0, imgW, imgH),
                          m_pageImage, QRectF(m_pageImage.rect()));
    }
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_pageImage = {};   // invalidate; repaint will re-render at new size
    update();
}

void PreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_tempFilePath.isEmpty()) {
        m_dragStart = event->pos();
        m_armed = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void PreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_armed) return;
    if (!(event->buttons() & Qt::LeftButton)) { m_armed = false; return; }

    if ((event->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
        m_armed = false;
        startDrag();
    }
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_armed = false;
    setCursor(Qt::OpenHandCursor);
}

void PreviewWidget::startDrag()
{
    setCursor(Qt::OpenHandCursor);

    auto *mime = new QMimeData();

    // File URL for Finder / file manager drops
    mime->setUrls({QUrl::fromLocalFile(m_tempFilePath)});

    // Raw PDF bytes for inline embedding in Pages, Keynote, Word, etc.
    QFile f(m_tempFilePath);
    if (f.open(QIODevice::ReadOnly)) {
        mime->setData("application/pdf", f.readAll());
        f.close();
    }

    // Thumbnail for drag cursor
    QPixmap px;
    if (!m_pageImage.isNull()) {
        QImage scaled = m_pageImage.scaled(
            QSize(128, 128) * qRound(devicePixelRatioF()),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(devicePixelRatioF());
        px = QPixmap::fromImage(scaled);
    }

    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    if (!px.isNull()) drag->setPixmap(px);
    drag->exec(Qt::CopyAction);
}
