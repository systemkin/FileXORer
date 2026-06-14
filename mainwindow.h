#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWaitCondition>
#include <QMutex>
#include <QTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:

    void start();

    void end();

private slots:

    int on_StartButton_clicked();

    void on_SourceFilesPathPushButton_clicked();

    void on_ResultFilesPathPushButton_clicked();

    int onStart();

    void onEnd();

    void on_StopPushButton_clicked();

    void on_ContinueButton_clicked();

    void renewStatusBar();

private:

    Ui::MainWindow *ui;

    QString mask="";

    QString sourcePath="";

    QString resultPath="";

    QString function="";

    WorkModes::CollisionResolveModes resolveMode;

    WorkModes::FilterModes filterMode;

    int doFile(QString &sourcePath, QString &destinationPath);

    int filesAmnt = 0;
    int currFileNum = 0;

    long long timer;
    QMutex pauseMutex;
    QWaitCondition pauseCondition;
    bool paused = false;
    bool waitingForRestart = false;

    QTimer restartTimer;
    QTimer statusBarTimer;

    QList<QFuture<void>> futures;
    QFutureWatcher<void> futureWatcher;
    QMap<QString, QPair<long long, long long>> results;

    long long globalRunsCounter = 0;

    bool aborting = false;

    void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H
