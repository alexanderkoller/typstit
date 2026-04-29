#include "historywindow.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QFrame>
#include <QFutureWatcher>
#include <QPdfDocument>
#include <QPainter>
#include <QtConcurrent/QtConcurrent>

static constexpr int kThumbW = 180;
static constexpr int kThumbH = 90;
static constexpr int kGridW  = 196;
static constexpr int kGridH  = 124;  // icon + date text

// Render the first page of a PDF to a thumbnail image (runs on a thread pool thread).
static QImage renderThumbnail(const QString &pdfPath)
{
    QPdfDocument doc;
    if (doc.load(pdfPath) != QPdfDocument::Error::None || doc.pageCount() == 0)
        return {};
    QSizeF sz = doc.pagePointSize(0);
    if (sz.isEmpty()) return {};
    double scale = qMin(kThumbW / sz.width(), kThumbH / sz.height());
    QImage img = doc.render(0, QSize(qRound(sz.width() * scale),
                                     qRound(sz.height() * scale)));
    // Pad to exact kThumbW×kThumbH on a white canvas so all cells are uniform.
    QImage canvas(kThumbW, kThumbH, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::white);
    QPainter p(&canvas);
    p.drawImage((kThumbW - img.width()) / 2, (kThumbH - img.height()) / 2, img);
    return canvas;
}

// ── HistoryWindow ─────────────────────────────────────────────────────────────

HistoryWindow::HistoryWindow(HistoryStore *store, QWidget *parent)
    : QDialog(parent), m_store(store)
{
    setWindowTitle("History");
    setMinimumSize(520, 360);
    resize(640, 440);
    setWindowFlags(Qt::Window);   // independent window, not a sheet

    // ── Layout ───────────────────────────────────────────────────────────────
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Stacked widget: page 0 = list, page 1 = empty state
    m_stack = new QStackedWidget();
    root->addWidget(m_stack, 1);

    // Page 0 — grid list
    m_list = new QListWidget();
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(kThumbW, kThumbH));
    m_list->setGridSize(QSize(kGridW, kGridH));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(4);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stack->addWidget(m_list);

    // Page 1 — empty state
    auto *emptyWidget = new QWidget();
    auto *ev = new QVBoxLayout(emptyWidget);
    ev->setAlignment(Qt::AlignCenter);
    auto *clockLbl = new QLabel("⏱");
    clockLbl->setAlignment(Qt::AlignCenter);
    QFont f = clockLbl->font(); f.setPointSize(36); clockLbl->setFont(f);
    auto *emptyLbl = new QLabel("No history yet");
    emptyLbl->setAlignment(Qt::AlignCenter);
    emptyLbl->setStyleSheet("color: #8e8e93; font-size: 13px;");
    ev->addWidget(clockLbl);
    ev->addWidget(emptyLbl);
    m_stack->addWidget(emptyWidget);

    // Bottom bar
    auto *bar = new QWidget();
    bar->setFixedHeight(38);
    bar->setAutoFillBackground(true);
    QPalette p = bar->palette();
    p.setColor(QPalette::Window, QColor("#f4f4f4"));
    bar->setPalette(p);
    auto *barLine = new QFrame(); barLine->setFrameShape(QFrame::HLine);
    barLine->setStyleSheet("background: #d0d0d0; border: none;"); barLine->setFixedHeight(1);
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 0, 8, 0);
    m_clearBtn = new QPushButton("Clear All");
    m_clearBtn->setStyleSheet("color: #c0392b;");
    barLayout->addStretch();
    barLayout->addWidget(m_clearBtn);

    root->addWidget(barLine);
    root->addWidget(bar);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        emit restoreAndCompileRequested(entryForItem(item));
        accept();
    });

    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = m_list->itemAt(pos);
        if (item) showContextMenu(m_list->mapToGlobal(pos), item);
    });

    connect(m_clearBtn, &QPushButton::clicked, this, [this] {
        m_store->clearAll();
    });

    connect(m_store, &HistoryStore::changed, this, &HistoryWindow::rebuild);

    rebuild();
}

void HistoryWindow::rebuild()
{
    m_list->clear();

    const auto &entries = m_store->entries();
    m_stack->setCurrentIndex(entries.isEmpty() ? 1 : 0);
    m_clearBtn->setEnabled(!entries.isEmpty());

    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        QString dateStr = QLocale().toString(e.date, QLocale::ShortFormat);

        auto *item = new QListWidgetItem(dateStr);
        item->setData(Qt::UserRole, e.id);
        // Placeholder icon (white square) until async render completes
        QPixmap placeholder(kThumbW, kThumbH);
        placeholder.fill(Qt::white);
        item->setIcon(QIcon(placeholder));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        m_list->addItem(item);

        loadThumbnailAsync(i, e.id);
    }
}

void HistoryWindow::loadThumbnailAsync(int row, const QString &id)
{
    QString path = m_store->pdfPath(id);
    auto *watcher = new QFutureWatcher<QImage>(this);

    connect(watcher, &QFutureWatcher<QImage>::finished, this, [=] {
        QImage img = watcher->result();
        if (!img.isNull() && row < m_list->count()) {
            QListWidgetItem *item = m_list->item(row);
            if (item && item->data(Qt::UserRole).toString() == id)
                item->setIcon(QIcon(QPixmap::fromImage(img)));
        }
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run(renderThumbnail, path));
}

HistoryEntry HistoryWindow::entryForItem(QListWidgetItem *item) const
{
    QString id = item->data(Qt::UserRole).toString();
    for (const auto &e : m_store->entries())
        if (e.id == id) return e;
    return {};
}

void HistoryWindow::showContextMenu(const QPoint &globalPos, QListWidgetItem *item)
{
    HistoryEntry entry = entryForItem(item);
    QMenu menu;

    menu.addAction("Restore", this, [=] {
        emit restoreRequested(entry);
    });
    menu.addAction("Restore and Compile", this, [=] {
        emit restoreAndCompileRequested(entry);
        accept();
    });
    menu.addSeparator();
    auto *del = menu.addAction("Delete", this, [=] {
        m_store->remove(entry.id);
    });
    del->setEnabled(!entry.id.isEmpty());

    menu.exec(globalPos);
}
