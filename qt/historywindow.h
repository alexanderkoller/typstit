#pragma once
#include <QDialog>
#include "history.h"

class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QPushButton;

class HistoryWindow : public QDialog
{
    Q_OBJECT
public:
    explicit HistoryWindow(HistoryStore *store, QWidget *parent = nullptr);

signals:
    void restoreRequested(HistoryEntry entry);
    void restoreAndCompileRequested(HistoryEntry entry);

private:
    HistoryStore  *m_store;
    QListWidget   *m_list;
    QStackedWidget *m_stack;
    QPushButton   *m_clearBtn;

    void rebuild();
    void loadThumbnailAsync(int row, const QString &id);
    void showContextMenu(const QPoint &globalPos, QListWidgetItem *item);
    HistoryEntry entryForItem(QListWidgetItem *item) const;
};
