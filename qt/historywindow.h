#pragma once
#include <QDialog>
#include "history.h"

class QScrollArea;
class QWidget;
class QPushButton;
class QStackedWidget;
class QGridLayout;

class HistoryWindow : public QDialog
{
    Q_OBJECT
public:
    explicit HistoryWindow(HistoryStore *store, QWidget *parent = nullptr);

signals:
    void restoreRequested(HistoryEntry entry);
    void restoreAndCompileRequested(HistoryEntry entry);

private:
    HistoryStore   *m_store;
    QStackedWidget *m_stack;
    QWidget        *m_gridWidget;
    QGridLayout    *m_gridLayout;
    QPushButton    *m_clearBtn;

    void rebuild();
};
