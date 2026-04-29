#include "history.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>

HistoryStore::HistoryStore(QObject *parent) : QObject(parent)
{
    m_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/history";
    QDir().mkpath(m_dir);
    load();
}

QString HistoryStore::entryDir(const QString &id) const
{
    return m_dir + "/" + id;
}

QString HistoryStore::pdfPath(const QString &id) const
{
    return entryDir(id) + "/output.pdf";
}

void HistoryStore::add(const QString &source, const QByteArray &pdfData,
                       const QString &fontName, int fontSize, const QString &colorHex)
{
    // Deduplicate by trimmed source text.
    QString trimmed = source.trimmed();
    for (const auto &e : m_entries)
        if (e.source.trimmed() == trimmed) return;

    HistoryEntry entry;
    entry.id       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.date     = QDateTime::currentDateTime();
    entry.source   = source;
    entry.fontName = fontName;
    entry.fontSize = fontSize;
    entry.colorHex = colorHex;

    QDir().mkpath(entryDir(entry.id));
    {
        QFile f(entryDir(entry.id) + "/output.pdf");
        if (f.open(QIODevice::WriteOnly)) f.write(pdfData);
    }
    {
        QFile f(entryDir(entry.id) + "/source.typ");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(source.toUtf8());
    }
    writeMeta(entry);

    m_entries.prepend(entry);
    emit changed();
}

void HistoryStore::remove(const QString &id)
{
    QDir(entryDir(id)).removeRecursively();
    m_entries.removeIf([&](const HistoryEntry &e) { return e.id == id; });
    emit changed();
}

void HistoryStore::clearAll()
{
    for (const auto &e : m_entries)
        QDir(entryDir(e.id)).removeRecursively();
    m_entries.clear();
    emit changed();
}

void HistoryStore::writeMeta(const HistoryEntry &e) const
{
    QJsonObject obj;
    obj["id"]       = e.id;
    obj["date"]     = e.date.toString(Qt::ISODate);
    obj["source"]   = e.source;
    obj["fontName"] = e.fontName;
    obj["fontSize"] = e.fontSize;
    obj["colorHex"] = e.colorHex;

    QFile f(entryDir(e.id) + "/meta.json");
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

void HistoryStore::load()
{
    const auto dirs = QDir(m_dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &info : dirs) {
        QFile f(info.filePath() + "/meta.json");
        if (!f.open(QIODevice::ReadOnly)) continue;
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        if (obj.isEmpty()) continue;

        HistoryEntry e;
        e.id       = obj["id"].toString();
        e.date     = QDateTime::fromString(obj["date"].toString(), Qt::ISODate);
        e.source   = obj["source"].toString();
        e.fontName = obj["fontName"].toString();
        e.fontSize = obj["fontSize"].toInt(36);
        e.colorHex = obj["colorHex"].toString();
        if (!e.id.isEmpty()) m_entries.append(e);
    }
    std::sort(m_entries.begin(), m_entries.end(),
              [](const HistoryEntry &a, const HistoryEntry &b){ return a.date > b.date; });
}
