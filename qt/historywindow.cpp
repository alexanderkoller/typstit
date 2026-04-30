#include "historywindow.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QMenu>
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QFutureWatcher>
#include <QPdfDocument>
#include <QApplication>
#include <QtConcurrent/QtConcurrent>

// ── Card dimensions (match Swift HistoryCell) ──────────────────────────────
static constexpr int kCardW    = 185;   // total card width
static constexpr int kThumbH   = 88;    // thumbnail area height
static constexpr int kCardPad  = 8;     // padding inside card
static constexpr int kSpacing  = 12;    // gap between cards
static constexpr int kCols     = 3;     // fixed column count
static constexpr int kGridPad  = 16;    // scroll-area content padding

// ── Async thumbnail render ────────────────────────────────────────────────
static QImage renderThumbnail(const QString &pdfPath, int targetW, int targetH)
{
    QPdfDocument doc;
    if (doc.load(pdfPath) != QPdfDocument::Error::None || doc.pageCount() == 0)
        return {};
    QSizeF sz = doc.pagePointSize(0);
    if (sz.isEmpty()) return {};
    double scale = qMin(targetW / sz.width(), targetH / sz.height());
    QImage img = doc.render(0, QSize(qRound(sz.width() * scale),
                                     qRound(sz.height() * scale)));
    QImage canvas(targetW, targetH, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::white);
    QPainter p(&canvas);
    p.drawImage((targetW - img.width()) / 2, (targetH - img.height()) / 2, img);
    return canvas;
}

// ── HistoryCard ───────────────────────────────────────────────────────────

class HistoryCard : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryCard(const HistoryEntry &entry, QWidget *parent = nullptr)
        : QWidget(parent), m_entry(entry)
    {
        setFixedWidth(kCardW);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);

        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(kCardPad, kCardPad, kCardPad, kCardPad);
        outer->setSpacing(6);

        // White thumbnail rectangle (clips its content with rounded corners via stylesheet)
        m_thumbFrame = new QFrame();
        m_thumbFrame->setFixedHeight(kThumbH);
        m_thumbFrame->setStyleSheet(
            "QFrame { background: white; border-radius: 4px; }"
        );
        auto *thumbLayout = new QVBoxLayout(m_thumbFrame);
        thumbLayout->setContentsMargins(6, 6, 6, 6);
        m_thumbLabel = new QLabel();
        m_thumbLabel->setAlignment(Qt::AlignCenter);
        thumbLayout->addWidget(m_thumbLabel);
        outer->addWidget(m_thumbFrame);

        // Date text
        QString dateStr = QLocale().toString(entry.date, QLocale::ShortFormat);
        auto *dateLabel = new QLabel(dateStr);
        dateLabel->setStyleSheet("font-size: 11px; color: #1c1c1e;");
        outer->addWidget(dateLabel);
    }

    void setThumbnail(const QImage &img)
    {
        if (img.isNull()) return;
        qreal dpr = devicePixelRatioF();
        int tw = m_thumbFrame->width()  - 12;
        int th = m_thumbFrame->height() - 12;
        QImage scaled = img.scaled(QSize(tw, th) * qRound(dpr),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        m_thumbLabel->setPixmap(QPixmap::fromImage(scaled));
    }

    const HistoryEntry &entry() const { return m_entry; }

signals:
    void restoreClicked(HistoryEntry);
    void restoreAndCompileClicked(HistoryEntry);
    void deleteClicked(QString id);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = m_hovered
            ? QColor(0, 122, 255, 28)
            : palette().color(QPalette::Base);
        p.setBrush(bg);
        if (m_hovered)
            p.setPen(QPen(QColor(0, 122, 255, 90), 1));
        else
            p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) m_pressPos = e->pos();
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton &&
            (e->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            emit restoreClicked(m_entry);
    }

    void mouseDoubleClickEvent(QMouseEvent *) override
    {
        emit restoreAndCompileClicked(m_entry);
    }

    void contextMenuEvent(QContextMenuEvent *e) override
    {
        QMenu menu;
        menu.addAction("Restore",          this, [this]{ emit restoreClicked(m_entry); });
        menu.addAction("Open and Compile", this, [this]{ emit restoreAndCompileClicked(m_entry); });
        menu.addSeparator();
        auto *del = menu.addAction("Delete", this, [this]{ emit deleteClicked(m_entry.id); });
        del->setEnabled(!m_entry.id.isEmpty());
        menu.exec(e->globalPos());
    }

    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::HoverEnter)  { m_hovered = true;  update(); }
        if (e->type() == QEvent::HoverLeave)  { m_hovered = false; update(); }
        return QWidget::event(e);
    }

private:
    HistoryEntry m_entry;
    QFrame      *m_thumbFrame  = nullptr;
    QLabel      *m_thumbLabel  = nullptr;
    bool         m_hovered     = false;
    QPoint       m_pressPos;
};

// ── HistoryWindow ─────────────────────────────────────────────────────────

HistoryWindow::HistoryWindow(HistoryStore *store, QWidget *parent)
    : QDialog(parent), m_store(store)
{
    setWindowTitle("History");
    setMinimumSize(520, 360);
    resize(640, 480);
    setWindowFlags(Qt::Window);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Stacked widget: page 0 = grid, page 1 = empty state ─────────────────
    m_stack = new QStackedWidget();
    root->addWidget(m_stack, 1);

    // Page 0 — scrollable card grid
    auto *scrollArea = new QScrollArea();
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    {
        QPalette p = scrollArea->palette();
        p.setColor(QPalette::Window, QColor("#ececec"));
        scrollArea->setPalette(p);
    }

    m_gridWidget = new QWidget();
    m_gridWidget->setAutoFillBackground(true);
    {
        QPalette p = m_gridWidget->palette();
        p.setColor(QPalette::Window, QColor("#ececec"));
        m_gridWidget->setPalette(p);
    }

    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setContentsMargins(kGridPad, kGridPad, kGridPad, kGridPad);
    m_gridLayout->setHorizontalSpacing(kSpacing);
    m_gridLayout->setVerticalSpacing(kSpacing);
    // Stretch last column so cards stay left-aligned, not stretched
    m_gridLayout->setColumnStretch(kCols, 1);

    scrollArea->setWidget(m_gridWidget);
    m_stack->addWidget(scrollArea);

    // Page 1 — empty state
    auto *emptyWidget = new QWidget();
    emptyWidget->setAutoFillBackground(true);
    {
        QPalette p = emptyWidget->palette();
        p.setColor(QPalette::Window, QColor("#ececec"));
        emptyWidget->setPalette(p);
    }
    auto *ev = new QVBoxLayout(emptyWidget);
    ev->setAlignment(Qt::AlignCenter);
    auto *clockLbl = new QLabel("⏰");
    clockLbl->setAlignment(Qt::AlignCenter);
    QFont f = clockLbl->font(); f.setPointSize(36); clockLbl->setFont(f);
    auto *emptyLbl = new QLabel("No history yet");
    emptyLbl->setAlignment(Qt::AlignCenter);
    emptyLbl->setStyleSheet("color: #8e8e93; font-size: 13px;");
    ev->addWidget(clockLbl);
    ev->addWidget(emptyLbl);
    m_stack->addWidget(emptyWidget);

    // ── Bottom bar ────────────────────────────────────────────────────────────
    auto *barLine = new QFrame();
    barLine->setFrameShape(QFrame::HLine);
    barLine->setStyleSheet("background: #d0d0d0; border: none;");
    barLine->setFixedHeight(1);
    root->addWidget(barLine);

    auto *bar = new QWidget();
    bar->setFixedHeight(38);
    bar->setAutoFillBackground(true);
    {
        QPalette p = bar->palette();
        p.setColor(QPalette::Window, QColor("#f4f4f4"));
        bar->setPalette(p);
    }
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 0, 8, 0);
    m_clearBtn = new QPushButton("Clear All");
    m_clearBtn->setAutoDefault(false);
    m_clearBtn->setDefault(false);
    barLayout->addStretch();
    barLayout->addWidget(m_clearBtn);
    root->addWidget(bar);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_clearBtn, &QPushButton::clicked, m_store, &HistoryStore::clearAll);
    connect(m_store, &HistoryStore::changed, this, &HistoryWindow::rebuild);

    rebuild();
}

void HistoryWindow::rebuild()
{
    // Remove all existing cards
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const auto &entries = m_store->entries();
    m_stack->setCurrentIndex(entries.isEmpty() ? 1 : 0);
    m_clearBtn->setEnabled(!entries.isEmpty());

    int row = 0, col = 0;
    for (const auto &e : entries) {
        auto *card = new HistoryCard(e, m_gridWidget);

        connect(card, &HistoryCard::restoreClicked,
                this, &HistoryWindow::restoreRequested);
        connect(card, &HistoryCard::restoreAndCompileClicked, this,
                [this](const HistoryEntry &entry) {
                    emit restoreAndCompileRequested(entry);
                    accept();
                });
        connect(card, &HistoryCard::deleteClicked,
                m_store, &HistoryStore::remove);

        m_gridLayout->addWidget(card, row, col);

        // Load thumbnail asynchronously
        QString path = m_store->pdfPath(e.id);
        auto *watcher = new QFutureWatcher<QImage>(card);
        connect(watcher, &QFutureWatcher<QImage>::finished, card,
                [card, watcher] {
                    card->setThumbnail(watcher->result());
                    watcher->deleteLater();
                });
        int tw = kCardW - kCardPad * 2 - 12;   // available thumb pixel width
        int th = kThumbH - 12;                   // available thumb pixel height
        watcher->setFuture(QtConcurrent::run(renderThumbnail, path, tw * 2, th * 2));

        if (++col >= kCols) { col = 0; ++row; }
    }

    // Fill remaining cells in last row with spacers so cards stay left-aligned
    while (col > 0 && col < kCols) {
        auto *sp = new QWidget();
        sp->setFixedWidth(kCardW);
        m_gridLayout->addWidget(sp, row, col++);
    }
}

#include "historywindow.moc"
