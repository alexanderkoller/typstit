#pragma once
#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QString>

class QPdfDocument;

class PreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewWidget(QPdfDocument *doc, QWidget *parent = nullptr);
    void setTempFilePath(const QString &path);
    void refresh();   // invalidate cached page image and repaint

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPdfDocument *m_doc;
    QString       m_tempFilePath;
    QImage        m_pageImage;
    QPoint        m_dragStart;
    bool          m_armed = false;

    void renderPage();
    void startDrag();
};
