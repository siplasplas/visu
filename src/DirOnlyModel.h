#ifndef DIRONLYMODEL_H
#define DIRONLYMODEL_H

#include <QFileSystemModel>
#include <QDir>

class DirOnlyModel : public QFileSystemModel
{
    Q_OBJECT
public:
    explicit DirOnlyModel(QObject* parent = nullptr)
        : QFileSystemModel(parent)
    {
        // interesują nas tylko katalogi, bez . i ..
        setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    }

    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override
    {
        // root (empty index) – default
        if (!parent.isValid())
            return QFileSystemModel::hasChildren(parent);

        const QString path = filePath(parent);
        QDir dir(path);
        dir.setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
        // if not subdirs  → false → no triangle
        return !dir.entryList().isEmpty();
    }
};

#endif // DIRONLYMODEL_H
