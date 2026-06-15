#include "fileworker.h"
#include "mainwindow.h"

#include <QStatusBar>
#include <QRegularExpression>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>

FileWorker::FileWorker(MainWindow *mw, QMutex *pauseMutex,QWaitCondition *pauseCondition, QObject *parent) : QObject(parent),
    pauseMutex{pauseMutex},
    pauseCondition{pauseCondition},
    mw{mw}
{}

bool FileWorker::start(QString &sourcePath, QString &resultPath, QString &mask, QString &function, bool needToDelete, WorkModes::FilterModes &filterMode, WorkModes::CollisionResolveModes &resolveMode) {

    disconnect(&futureWatcher, &QFutureWatcher<void>::finished, this, &FileWorker::end);
    futures.clear();

    QDir sourceDir(sourcePath);
    QDir resultDir(resultPath);

    if ((!resultDir.exists()) || (!sourceDir.exists())) {
        QMessageBox::information(nullptr, "Information", "Directory was deleted!");
        mw->statusBar()->showMessage("Directory was deleted!");
        return false;
    }

    sourceDir.setFilter(QDir::Files);

    QStringList sourceFiles;

    if (filterMode == WorkModes::FilterModes::FileName) {
        if (sourceDir.exists(mask))
            sourceFiles.append(mask);
        else {
            mw->statusBar()->showMessage("No source files found");
            emit end();
            return true;
        }
    }
    if (filterMode == WorkModes::FilterModes::FileType)  {
        QStringList allFiles;
        allFiles = sourceDir.entryList();
        QRegularExpression re(QString("^.*%1$").arg(QRegularExpression::escape(mask)));
        sourceFiles = allFiles.filter(re);
    }
    if (filterMode == WorkModes::FilterModes::NoFilter)  {
        sourceFiles = sourceDir.entryList();
    }
    mw->statusBar()->showMessage(QString("Found %1 source files").arg(sourceFiles.size()));

    for (auto it = sourceFiles.begin(); it != sourceFiles.end(); ++it) {
        QString sFilepath = sourceDir.absoluteFilePath(*it);
        QString rFilePath = resultDir.absoluteFilePath(*it);
        doFile(sFilepath, rFilePath, function, needToDelete, resolveMode);
    }

    if (!futures.isEmpty()) {
        futureWatcher.setFuture(futures.last());
        connect(&futureWatcher, &QFutureWatcher<void>::finished, this, &FileWorker::end);
    } else {
        emit end();
    }

    return false;
}
bool FileWorker::doFile(QString &sourcePath, QString &destinationPath, QString &function, bool needToDelete, WorkModes::CollisionResolveModes &resolveMode) {
    if (aborting) {
        return true;
    }
    QFile * sfile = new QFile(sourcePath);
    if (!sfile->open(QIODevice::ReadOnly)) {
        QMessageBox::information(mw, "Information", QString("Can't open file %1").arg(sourcePath));
        mw->statusBar()->showMessage(QString("Can't open file %1").arg(sourcePath));
        return false;
    }
    if (sfile->size() % 8 != 0) {
        QMessageBox::information(mw, "Information", QString("File %1 is not 8-sized").arg(sourcePath));
        mw->statusBar()->showMessage(QString("File %1 is not 8-sized").arg(sourcePath));
        sfile->close();
        return false;
    }

    QFile *dfile = new QFile(destinationPath);
    if (resolveMode == WorkModes::CollisionResolveModes::Rewrite) {
        if (!dfile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::information(mw, "Information", QString("Can't open file %1").arg(destinationPath));
            mw->statusBar()->showMessage(QString("Can't open file %1").arg(destinationPath));
            sfile->close();
            return false;
        }
    } else if (resolveMode == WorkModes::CollisionResolveModes::Append) {
        unsigned long long num = 0;
        bool ok;
        QFileInfo fileInfo(destinationPath);
        QString baseName = fileInfo.completeBaseName();
        QString path = fileInfo.path();
        QString ext = fileInfo.suffix();
        do  {
            ok = dfile->open(QIODevice::WriteOnly | QIODevice::NewOnly);
            if (!ok) {
                num++;

                dfile->setFileName(QString("%1/%2_%3.%4").arg(path).arg(baseName).arg(num).arg(ext));
                // Ну я надесь за unsigned long long не выйдем?
                // Если выйдем можно ещё раз подобовлять к концу) file_0_1;
                ok = dfile->open(QIODevice::WriteOnly | QIODevice::NewOnly);
                if (num == 1000000) { //magic num, i think we should not try more than this anyway..
                    QMessageBox::information(mw, "Information", QString("Can't create new version of file %1").arg(destinationPath));
                    mw->statusBar()->showMessage(QString("Can't create new version of file %1").arg(destinationPath));
                    sfile->close();
                    return false;
                }
            }
        } while (!ok);
    } else if (resolveMode == WorkModes::CollisionResolveModes::Ignore) {
        bool ok = dfile->open(QIODevice::WriteOnly | QIODevice::NewOnly);
        if (!ok) {
            sfile->close();
            return true;
        }
    }

    filesSize += sfile->size();
    QFuture<void> future = QtConcurrent::run(QThreadPool::globalInstance(), [this, sfile, dfile, needToDelete, function]{
        QDataStream sourceStream(sfile);
        QDataStream destinationStream(dfile);
        std::vector<char> buf(8);
        qint64 size = 8;
        char pos = 0;
        uint16_t cnt = 0;
        while (!sourceStream.atEnd() && (!aborting)) {

            QMutexLocker locker(pauseMutex);
            while (paused) {
                pauseCondition->wait(pauseMutex);
                if (aborting) {
                    sfile->close();
                    dfile->close();
                    delete sfile;
                    delete dfile;
                    return;
                }
            }

            if (cnt == 1024) {
                progress += cnt*8;
                cnt = 0;
                dfile->flush();
            }
            sourceStream.readRawData(buf.data(), size);
            QString hexRepresentation = QString(function.mid(pos, 16));
            uint64_t byteFunc = qbswap(hexRepresentation.toULongLong(nullptr, 16));

            uint64_t byteFile = 0;
            memcpy(&byteFile, buf.data(), sizeof(char)*8);

            uint64_t byteXOR = byteFunc ^ byteFile;

            destinationStream.writeRawData((char*) &byteXOR, size);
            pos = (pos+16)%16;
            cnt++;
        }
        dfile->close();
        if(needToDelete) {
            sfile->close();
            if(!sfile->remove()) {
                emit failToDelete(sfile->fileName());
            }

        } else {
            sfile->close();
        }
        delete dfile;
        delete sfile;
    });
    futures.append(future);
    return 1;
}
void FileWorker::pause(bool pause) {
    this->paused=pause;
}
void FileWorker::abort(bool abort) {
    this->aborting=abort;
}
char FileWorker::getProgressPercent() {
    return 100*((double)progress/(double)filesSize);
}
long long FileWorker::getLoadedSize() {
    return filesSize;
}
