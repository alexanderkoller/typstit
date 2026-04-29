#pragma once
#include <QObject>
#include <QList>
#include <QDateTime>
#include <QString>
#include <QByteArray>

struct HistoryEntry {
    QString   id;
    QDateTime date;
    QString   source;
    QString   fontName;
    int       fontSize   = 36;
    QString   colorHex;     // 6 lowercase hex digits, no '#'
};

class HistoryStore : public QObject
{
    Q_OBJECT
public:
    explicit HistoryStore(QObject *parent = nullptr);

    const QList<HistoryEntry> &entries() const { return m_entries; }

    void add(const QString &source, const QByteArray &pdfData,
             const QString &fontName, int fontSize, const QString &colorHex);
    void remove(const QString &id);
    void clearAll();

    // Full path to the stored PDF for a given entry id.
    QString pdfPath(const QString &id) const;

signals:
    void changed();

private:
    QList<HistoryEntry> m_entries;
    QString             m_dir;

    QString entryDir(const QString &id) const;
    void    writeMeta(const HistoryEntry &e) const;
    void    load();
};
