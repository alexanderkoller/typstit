#include "mainwindow.h"
#include "previewwidget.h"
#include "compiler.h"
#include "highlighter.h"
#include "preamble.h"
#include "history.h"
#include "historywindow.h"

#include <QPlainTextEdit>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QFont>
#include <QPalette>
#include <QDir>
#include <QFile>
#include <QStatusBar>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QTimer>
#include <QColorDialog>
#include <QPixmap>
#include <QPainter>
#include <QClipboard>
#include <QMimeData>
#include <QApplication>
#include <QMenuBar>
#include <QAction>
#include "clipboard_mac.h"
#include <QStandardPaths>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QAbstractItemView>
#include <QStyledItemDelegate>
#include <QMessageBox>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Typstit");
    setMinimumSize(560, 400);

    setupWorkspace();

    m_store    = new HistoryStore(this);
    m_pdfDoc   = new QPdfDocument(this);
    m_compiler = new Compiler(this);

    connect(m_compiler, &Compiler::availabilityChecked, this, &MainWindow::onAvailabilityChecked);
    connect(m_compiler, &Compiler::compileFinished,     this, &MainWindow::onCompileFinished);
    connect(m_compiler, &Compiler::compileFailed,       this, &MainWindow::onCompileFailed);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(500);
    connect(m_debounce, &QTimer::timeout, this, &MainWindow::triggerTypeset);

    // ── Central layout ──────────────────────────────────────────────────────
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *vLayout = new QVBoxLayout(central);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    vLayout->addWidget(buildToolbar());

    auto *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #d0d0d0; border: none;");
    vLayout->addWidget(sep);

    // ── Equal-height panels (no draggable divider) ───────────────────────────
    auto *panels = new QWidget();
    auto *panelLayout = new QVBoxLayout(panels);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);
    vLayout->addWidget(panels, 1);

    // ── Editor ──────────────────────────────────────────────────────────────
    m_editor = new QPlainTextEdit();
    m_editor->setPlainText("$ integral_0^infinity e^(-x^2) d x = sqrt(pi) / 2 $");
    m_editor->setMinimumHeight(60);
    m_editor->setFrameShape(QFrame::NoFrame);
    {
        QFont f;
        f.setFamily("Menlo");
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(13);
        m_editor->setFont(f);
        m_editor->document()->setDefaultFont(f);
    }
    {
        QPalette p = m_editor->palette();
        p.setColor(QPalette::Base, QColor("#ffffff"));
        p.setColor(QPalette::Text, QColor("#1d1d1f"));
        m_editor->setPalette(p);
    }
    m_highlighter = new SyntaxHighlighter(m_editor->document());
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    panelLayout->addWidget(m_editor, 1);

    // Mid-panel separator
    auto *midSep = new QFrame();
    midSep->setFrameShape(QFrame::HLine);
    midSep->setFixedHeight(1);
    midSep->setStyleSheet("background: #d0d0d0; border: none;");
    panelLayout->addWidget(midSep);

    // ── Preview ─────────────────────────────────────────────────────────────
    m_preview = new PreviewWidget(m_pdfDoc);
    m_preview->setMinimumHeight(60);
    panelLayout->addWidget(m_preview, 1);

    // ── Status bar ──────────────────────────────────────────────────────────
    m_status = new QLabel("Checking for typst…");
    m_status->setStyleSheet("color: #6e6e73; font-size: 11px;");
    statusBar()->addWidget(m_status);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(
        "QStatusBar { background: #f4f4f4; border-top: 1px solid #d0d0d0; padding: 0 6px; }"
    );

    // ── Menu bar ─────────────────────────────────────────────────────────────

    // Edit menu — standard editing actions + Paste Typst Source
    QMenu *editMenu = menuBar()->addMenu("Edit");

    QAction *undoAct = editMenu->addAction("Undo");
    undoAct->setShortcut(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, m_editor, &QPlainTextEdit::undo);

    QAction *redoAct = editMenu->addAction("Redo");
    redoAct->setShortcut(QKeySequence::Redo);
    connect(redoAct, &QAction::triggered, m_editor, &QPlainTextEdit::redo);

    editMenu->addSeparator();

    QAction *cutAct = editMenu->addAction("Cut");
    cutAct->setShortcut(QKeySequence::Cut);
    connect(cutAct, &QAction::triggered, m_editor, &QPlainTextEdit::cut);

    QAction *copyAct = editMenu->addAction("Copy");
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, m_editor, &QPlainTextEdit::copy);

    QAction *pasteAct = editMenu->addAction("Paste");
    pasteAct->setShortcut(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, m_editor, &QPlainTextEdit::paste);

    QAction *selectAllAct = editMenu->addAction("Select All");
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAct, &QAction::triggered, m_editor, &QPlainTextEdit::selectAll);

    editMenu->addSeparator();

    QAction *pasteBackAct = editMenu->addAction("Paste Typst Source",
                                                this, &MainWindow::pasteBack);
    pasteBackAct->setShortcut(QKeySequence("Shift+Ctrl+V"));

    // Typeset menu
    QMenu *typesetMenu = menuBar()->addMenu("Typeset");

    QAction *typesetAct = typesetMenu->addAction("Typeset", this, &MainWindow::triggerTypeset);
    typesetAct->setShortcut(QKeySequence("Ctrl+T"));

    QAction *autoAct = typesetMenu->addAction("Auto");
    autoAct->setCheckable(true);
    autoAct->setShortcut(QKeySequence("Shift+Ctrl+T"));
    connect(autoAct, &QAction::toggled, m_autoCheck, &QCheckBox::setChecked);
    connect(m_autoCheck, &QCheckBox::toggled, autoAct, &QAction::setChecked);

    // History menu
    QMenu *historyMenu = menuBar()->addMenu("History");
    QAction *showHistoryAct = historyMenu->addAction("Show History", this, [this] {
        auto *win = new HistoryWindow(m_store, this);
        connect(win, &HistoryWindow::restoreRequested,
                this, [this](const HistoryEntry &e) { restoreEntry(e, false); });
        connect(win, &HistoryWindow::restoreAndCompileRequested,
                this, [this](const HistoryEntry &e) { restoreEntry(e, true); });
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
    });
    showHistoryAct->setShortcut(QKeySequence("Ctrl+Y"));

    m_compiler->findTypst();
    m_editor->setFocus();
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

// Filled right-pointing triangle matching SF Symbol play.fill.
// rightPad pixels of transparent space to the right push the button text further away.
// Adds vertical padding to each item in a QComboBox popup.
// QMacStyle ignores view stylesheets for combo popups, so a delegate is required.
class PaddedComboDelegate : public QStyledItemDelegate {
public:
    explicit PaddedComboDelegate(int extraV, int leftPad, QComboBox *combo)
        : QStyledItemDelegate(combo), m_extraV(extraV), m_leftPad(leftPad), m_combo(combo) {}

    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        return QStyledItemDelegate::sizeHint(opt, idx) + QSize(0, m_extraV);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();

        // Background/highlight across the full item width
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

        // Checkmark for the currently-selected item (replicates NSPopUpButton ✓)
        bool isCurrent = m_combo && (index.row() == m_combo->currentIndex());
        QPalette::ColorRole textRole = (opt.state & QStyle::State_Selected)
            ? QPalette::HighlightedText : QPalette::Text;
        if (isCurrent) {
            QRect checkRect(opt.rect.left() + 2, opt.rect.top(), m_leftPad - 4, opt.rect.height());
            painter->save();
            painter->setFont(opt.font);
            painter->setPen(opt.palette.color(QPalette::Active, textRole));
            painter->drawText(checkRect, Qt::AlignVCenter | Qt::AlignRight, "✓");
            painter->restore();
        }

        // Text at indented position
        if (!opt.text.isEmpty()) {
            QRect textRect = opt.rect.adjusted(m_leftPad, 0, -4, 0);
            painter->save();
            painter->setFont(opt.font);
            style->drawItemText(painter, textRect,
                                Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                                opt.palette, true, opt.text, textRole);
            painter->restore();
        }
    }

private:
    int       m_extraV;
    int       m_leftPad;
    QComboBox *m_combo;
};

static QIcon makePlayIcon(int sz, int rightPad = 5)
{
    QPixmap pm((sz + rightPad) * 2, sz * 2);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(2.0);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    // Equilateral triangle, vertically centred in sz×sz
    const qreal h = sz * 0.87;
    const qreal w = h * 0.866;
    const qreal y0 = (sz - h) / 2.0;
    QPolygonF tri;
    tri << QPointF(0, y0) << QPointF(0, y0 + h) << QPointF(w, sz / 2.0);
    p.setBrush(QColor("#3c3c43"));
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
    return QIcon(pm);
}

static QIcon makeClockIcon(int sz)
{
    QPixmap pm(sz * 2, sz * 2);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(2.0);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal cx = sz / 2.0, cy = sz / 2.0, r = sz / 2.0 - 1.0;
    p.setPen(QPen(QColor("#3c3c43"), 1.3));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), r, r);
    // Hour hand (~10 o'clock)
    const double ha = -M_PI / 2.0 - M_PI / 3.0;
    p.drawLine(QPointF(cx, cy),
               QPointF(cx + r * 0.50 * std::cos(ha),
                       cy + r * 0.50 * std::sin(ha)));
    // Minute hand (~12 o'clock)
    const double ma = -M_PI / 2.0;
    p.drawLine(QPointF(cx, cy),
               QPointF(cx + r * 0.72 * std::cos(ma),
                       cy + r * 0.72 * std::sin(ma)));
    return QIcon(pm);
}

QWidget *MainWindow::buildToolbar()
{
    auto *bar = new QWidget();
    bar->setFixedHeight(44);
    bar->setAutoFillBackground(true);
    {
        QPalette p = bar->palette();
        p.setColor(QPalette::Window, QColor("#f4f4f4"));
        bar->setPalette(p);
    }

    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 0, 10, 0);
    h->setSpacing(8);

    auto addC = [&](QWidget *w) { h->addWidget(w, 0, Qt::AlignVCenter); };

    // Play icon: sz=14 matches SF Symbol play.fill size; rightPad=3 gives a
    // tight gap between the triangle and the "Typeset" label.
    m_typesetBtn = new QPushButton("Typeset");
    m_typesetBtn->setIcon(makePlayIcon(14, 0));
    m_typesetBtn->setIconSize(QSize(17, 14));   // logical: (sz + rightPad) × sz
    m_typesetBtn->setEnabled(false);
    connect(m_typesetBtn, &QPushButton::clicked, this, &MainWindow::triggerTypeset);
    addC(m_typesetBtn);

    h->addSpacing(2);   // tighten gap between button and "Auto" checkbox

    m_autoCheck = new QCheckBox("Auto");
    connect(m_autoCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (on) m_debounce->start();
    });
    addC(m_autoCheck);

    // Separator
    auto *vsep = new QFrame();
    vsep->setFrameShape(QFrame::VLine);
    vsep->setFixedSize(1, 16);
    vsep->setStyleSheet("background: #d0d0d0; border: none;");
    h->addSpacing(6);
    addC(vsep);
    h->addSpacing(2);

    m_fontCombo = new QComboBox();
    {
        // Typst-bundled fonts that may not be installed system-wide
        static const QStringList kBundled = {
            "Libertinus Serif", "Libertinus Sans", "Libertinus Mono",
            "New Computer Modern", "New Computer Modern Math", "DejaVu Sans Mono",
        };
        QStringList system = QFontDatabase::families();
        QSet<QString> systemSet(system.begin(), system.end());
        QStringList fonts;
        for (const QString &f : kBundled)
            if (!systemSet.contains(f)) fonts << f;
        fonts << system;
        m_fontCombo->addItems(fonts);
        m_fontCombo->setCurrentText("Libertinus Serif");
        m_fontName = m_fontCombo->currentText();
    }
    m_fontCombo->setMinimumWidth(155);
    m_fontCombo->setItemDelegate(new PaddedComboDelegate(4, 22, m_fontCombo));
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, [this](const QString &f) {
        m_fontName = f;
        if (m_autoCheck->isChecked()) m_debounce->start();
    });
    addC(m_fontCombo);

    m_sizeBox = new QSpinBox();
    m_sizeBox->setRange(6, 288);
    m_sizeBox->setValue(36);
    m_sizeBox->setFixedWidth(52);
    m_sizeBox->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sizeBox->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    connect(m_sizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_fontSize = v;
        if (m_autoCheck->isChecked()) m_debounce->start();
    });
    addC(m_sizeBox);

    m_colorBtn = new QToolButton();
    m_colorBtn->setFixedSize(26, 26);
    m_colorBtn->setAutoRaise(true);
    m_colorBtn->setToolTip("Text color");
    connect(m_colorBtn, &QToolButton::clicked, this, &MainWindow::pickColor);
    updateColorBtn();
    addC(m_colorBtn);

    h->addStretch();

    m_historyBtn = new QToolButton();
    m_historyBtn->setIcon(makeClockIcon(16));
    m_historyBtn->setIconSize(QSize(16, 16));
    m_historyBtn->setFixedSize(30, 26);
    m_historyBtn->setAutoRaise(true);
    m_historyBtn->setToolTip("History (⌘Y)");
    m_historyBtn->setStyleSheet(
        "QToolButton { border: none; background: transparent; padding: 2px; }"
        "QToolButton:hover { background: rgba(0,0,0,18); border-radius: 4px; }"
        "QToolButton:pressed { background: rgba(0,0,0,30); border-radius: 4px; }"
    );
    connect(m_historyBtn, &QToolButton::clicked, this, [this] {
        auto *win = new HistoryWindow(m_store, this);
        connect(win, &HistoryWindow::restoreRequested,
                this, [this](const HistoryEntry &e) { restoreEntry(e, false); });
        connect(win, &HistoryWindow::restoreAndCompileRequested,
                this, [this](const HistoryEntry &e) { restoreEntry(e, true); });
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
    });
    addC(m_historyBtn);

    return bar;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void MainWindow::setupWorkspace()
{
    m_workspace  = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/workspace";
    m_outputPath = m_workspace + "/output.pdf";
    QDir().mkpath(m_workspace);
}

void MainWindow::setStatus(const QString &msg, bool error)
{
    m_status->setText(msg);
    m_status->setStyleSheet(error
        ? "color: #c0392b; font-size: 11px;"
        : "color: #6e6e73; font-size: 11px;");
}

void MainWindow::updateColorBtn()
{
    QPixmap pm(16, 16);
    pm.fill(m_textColor);
    {
        QPainter p(&pm);
        p.setPen(QColor(0, 0, 0, 64));
        p.drawRect(0, 0, 15, 15);
    }
    m_colorBtn->setIcon(QIcon(pm));
    m_colorBtn->setText({});
}

// ── Compilation ───────────────────────────────────────────────────────────────

void MainWindow::onTextChanged()
{
    if (m_autoCheck->isChecked()) m_debounce->start();
}

void MainWindow::triggerTypeset()
{
    if (m_isCompiling || !m_compiler->isAvailable()) return;
    m_isCompiling = true;
    m_typesetBtn->setEnabled(false);
    setStatus("Compiling…");

    QString wrapped = Preamble::wrap(
        m_editor->toPlainText(), m_fontName, m_fontSize, m_textColor);
    m_compiler->compile(wrapped, m_workspace, m_outputPath);
}

void MainWindow::onAvailabilityChecked(bool available)
{
    m_typesetBtn->setEnabled(available);
    if (available) {
        setStatus("Ready");
    } else {
        setStatus("typst not found — install with: brew install typst", true);
    }
}

void MainWindow::onCompileFinished(const QByteArray &pdfData, qint64 elapsedMs)
{
    m_isCompiling = false;
    m_typesetBtn->setEnabled(true);

    // Write to temp path for drag-out, reload QPdfDocument, refresh preview
    QFile f(m_outputPath);
    if (f.open(QIODevice::WriteOnly)) { f.write(pdfData); f.close(); }

    m_pdfDoc->close();
    m_pdfDoc->load(m_outputPath);
    m_preview->setTempFilePath(m_outputPath);
    m_preview->refresh();

    // Persist to history
    QString colorHex = m_textColor.name(QColor::HexRgb).mid(1); // strip '#'
    m_store->add(m_editor->toPlainText(), pdfData, m_fontName, m_fontSize, colorHex);

    setStatus(QString("Compiled in %1s").arg(elapsedMs / 1000.0, 0, 'f', 2));
}

void MainWindow::onCompileFailed(const QString &error)
{
    m_isCompiling = false;
    m_typesetBtn->setEnabled(true);
    setStatus(error, true);
}

// ── Color picker ──────────────────────────────────────────────────────────────

void MainWindow::pickColor()
{
    QColor c = QColorDialog::getColor(m_textColor, this, "Text Color");
    if (!c.isValid()) return;
    m_textColor = c;
    updateColorBtn();
    if (m_autoCheck->isChecked()) m_debounce->start();
}

// ── Paste-back ────────────────────────────────────────────────────────────────

void MainWindow::pasteBack()
{
    // Use the native NSPasteboard bridge — Qt's MIME layer misses some macOS PDF types.
    QByteArray pdfBytes = macClipboardPdfData();

    if (pdfBytes.isEmpty()) {
        setStatus("No PDF found in clipboard", true);
        return;
    }

    QString tmpPdf = QDir::tempPath() + "/typstit_pasteback.pdf";
    { QFile f(tmpPdf); if (f.open(QIODevice::WriteOnly)) f.write(pdfBytes); }

    QPdfDocument doc;
    if (doc.load(tmpPdf) != QPdfDocument::Error::None || doc.pageCount() == 0) {
        setStatus("Could not open clipboard PDF", true);
        return;
    }

    QPdfSelection sel = doc.getSelectionAtIndex(0, 0, 1'000'000);
    QString text = sel.text();

    const QString marker = Preamble::sourceKeyword;
    int idx = text.indexOf(marker);
    if (idx < 0) {
        setStatus("No Typstit source found in clipboard PDF", true);
        return;
    }

    QString tail   = text.mid(idx + marker.size());
    int     endIdx = tail.indexOf('!');
    QString b64    = tail.left(endIdx < 0 ? tail.size() : endIdx)
                        .remove(QRegularExpression(R"(\s)"));

    QByteArray decoded = QByteArray::fromBase64(b64.toUtf8());
    if (decoded.isEmpty()) {
        setStatus("Source decode failed", true);
        return;
    }

    m_editor->setPlainText(QString::fromUtf8(decoded));
    setStatus("Source restored from PDF");
}

// ── History restore ───────────────────────────────────────────────────────────

void MainWindow::restoreEntry(const HistoryEntry &entry, bool compile)
{
    // Restore text
    m_editor->setPlainText(entry.source);

    // Restore font
    if (!entry.fontName.isEmpty()) {
        m_fontName = entry.fontName;
        m_fontCombo->setCurrentText(entry.fontName);
    }

    // Restore size
    m_fontSize = entry.fontSize;
    m_sizeBox->setValue(entry.fontSize);

    // Restore color
    if (!entry.colorHex.isEmpty()) {
        m_textColor = QColor("#" + entry.colorHex);
        updateColorBtn();
    }

    if (compile) triggerTypeset();
}
