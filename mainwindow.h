#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "fileworker.h"

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



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void end();

private slots:

    void start();

    void on_StartPushButton_clicked();

    void on_SourceFilesPathPushButton_clicked();

    void on_ResultFilesPathPushButton_clicked();

    void onEnd();

    void on_PausePushButton_clicked();

    void on_ContinuePushButton_clicked();

    void renewStatusBar();

    void onErrorDeleteFile(QString error);

    void on_TerminatePushButton_clicked();

private:

    bool validate();

    Ui::MainWindow *ui;

    bool waitingForRestart = false;

    QString mask="";

    QString sourcePath="";

    QString resultPath="";

    QString function="";

    WorkModes::CollisionResolveModes resolveMode;

    WorkModes::FilterModes filterMode;


    long long timer;
    QMutex pauseMutex;
    QWaitCondition pauseCondition;

    QTimer restartTimer;
    QTimer statusBarTimer;

    long long globalRunsCounter = 0;

    void closeEvent(QCloseEvent *event) override;

    FileWorker *fw;
};
#endif // MAINWINDOW_H
