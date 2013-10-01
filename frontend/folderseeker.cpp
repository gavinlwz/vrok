#include "folderseeker.h"

#include <QDirIterator>
#include <QTextStream>

FolderSeeker *FolderSeeker::getSingleton()
{
    static FolderSeeker fs;
    return &fs;
}

FolderSeeker::FolderSeeker()
{
}

void FolderSeeker::setExtensionList(QStringList _extensions)
{
    extensions = _extensions;
}

void FolderSeeker::setSeekPath(QString path)
{
    currentPath = path;
    QFile f(currentPath + QDir::separator() + ".vrok.cache");
    if (f.exists()) {
        f.open(QFile::Text | QFile::ReadOnly);
        f.setTextModeEnabled(true);
        QTextStream ts(&f);
        int i=0;
        ts.setCodec("UTF-8");
        dirs.clear();
        while (!ts.atEnd()) {
            dirs.append(ts.readLine());
        }
        f.close();
    } else {
        folderSeekSweep();
    }
}

QString FolderSeeker::getQueue(QStandardItemModel *m, int idx)
{
    if (!dirs.empty()) {

        QDirIterator iterator(dirs[idx],extensions,QDir::Files);

        int i=0;
        //m->removeRows(0,model->rowCount());
        m->clear();
        while (iterator.hasNext()){
            iterator.next();
            m->setItem(i,0, new QStandardItem(iterator.fileName()));
            m->setItem(i,1, new QStandardItem(iterator.filePath()));
            i++;
        }
        m->sort(0);
        return dirs[idx];
    } else {
        return QString("");
    }
}

FolderSeeker::~FolderSeeker()
{
    QFile f(currentPath + QDir::separator() + ".vrok.cache");
    f.open(QFile::Text | QFile::WriteOnly);
    f.setTextModeEnabled(true);
    QTextStream ts(&f);
    int i=0;
    ts.setCodec("UTF-8");
    while (dirs.size() > i) {
        ts << dirs[i] <<"\n" ;
        i++;
    }
    f.close();
}

void FolderSeeker::folderSeekSweep()
{
    QDirIterator iterator(currentPath, QDirIterator::Subdirectories );
    QDirIterator iteratorSubRoot(currentPath,extensions,QDir::Files);
    if (iteratorSubRoot.hasNext())
        dirs.append(currentPath);

    while (iterator.hasNext()) {
       iterator.next();
       if (iterator.fileInfo().isDir() && (iterator.fileName()!="..") && (iterator.fileName() != ".")) {
           QDirIterator iteratorSub(iterator.filePath(),extensions,QDir::Files);
           if (iteratorSub.hasNext())
               dirs.append(iterator.filePath());
       }
    }
    dirs.sort();
}