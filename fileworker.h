#ifndef FILEWORKER_H
#define FILEWORKER_H

#include <QWaitCondition>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QWidget>
#include <QDir>

namespace WorkModes {
    enum CollisionResolveModes {
        Rewrite = 0,
        Append,
        Ignore,
    };
    enum FilterModes {
        FileType = 0,
        FileName,
        NoFilter,
    };
}
class MainWindow;
class FileWorker : public QObject
{

    Q_OBJECT
public:
    FileWorker(MainWindow *mw, QMutex *pauseMutex, QWaitCondition *pauseCondition, QObject *parent = nullptr);

    char getProgressPercent();
    long long getLoadedSize();

public slots:
    bool start(QString &sourcePath, QString &resultPath, QString &mask, QString &function, bool needToDelete, WorkModes::FilterModes &filterMode, WorkModes::CollisionResolveModes &resolveMode);

    void pause(bool pause);

    void abort(bool abort);

signals:
    void end();

    void failToDelete(QString filename);

private:

    bool doFile(QString &sourcePath, QString &destinationPath, QString &function, bool needToDelete, WorkModes::CollisionResolveModes &resolveMode);

    QList<QFuture<void>> futures;
    QFutureWatcher<void> futureWatcher;

    QMutex *pauseMutex;
    QWaitCondition *pauseCondition;

    MainWindow *mw;

    std::atomic<bool> aborting{false};
    std::atomic<bool> paused{false};
    std::atomic<long long> progress{0};
    std::atomic<long long> filesSize{0};

};

#endif // FILEWORKER_H
