#include "previewwidget.h"

#include <QPdfDocument>
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

void PreviewWidget::renderPage()
{
    m_pageImage = {};
    if (!m_doc || m_doc->pageCount() == 0) return;

    const int padding = 48;
    int availW = width()  - padding * 2;
    int availH = height() - padding * 2;
    if (availW < 10 || availH < 10) return;

    QSizeF pageSize = m_doc->pagePointSize(0);
    if (pageSize.isEmpty()) return;

    double scale = qMin((double)availW / pageSize.width(),
                        (double)availH / pageSize.height());
    scale = qMin(scale, 3.0);

    // Render at device pixel ratio for crisp Retina output
    qreal dpr = devicePixelRatioF();
    QSize renderSize(qRound(pageSize.width()  * scale * dpr),
                     qRound(pageSize.height() * scale * dpr));

    m_pageImage = m_doc->render(0, renderSize);
    m_pageImage.setDevicePixelRatio(dpr);
}

void PreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#ececec"));

    if (m_pageImage.isNull()) renderPage();
    if (m_pageImage.isNull()) return;

    // Center the page image (logical pixels)
    int imgW = qRound(m_pageImage.width()  / devicePixelRatioF());
    int imgH = qRound(m_pageImage.height() / devicePixelRatioF());
    int x = (width()  - imgW) / 2;
    int y = (height() - imgH) / 2;

    painter.drawImage(QPoint(x, y), m_pageImage);

    // Drag hint
    QFont hintFont = font();
    hintFont.setPointSize(9);
    painter.setFont(hintFont);
    painter.setPen(QColor(0, 0, 0, 70));
    painter.drawText(rect().adjusted(0, 0, 0, -7),
                     Qt::AlignBottom | Qt::AlignHCenter,
                     "drag to paste into another app");
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
