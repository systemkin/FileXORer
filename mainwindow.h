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

private slots:

    /*!
     * \brief Слот начала обработки. Использует заранее заполненные приватные поля
     */
    void start();

    /*!
     * \brief Обрабатывает конец обработки файлов - либо запускает таймер до очередного повторения либо завершает обработку
     */
    void onEnd();

    /*!
     * \brief Функция для обновления статус бара в соответсвии с текущим прогрессом обработки
     */
    void renewStatusBar();

    /*!
     * \brief Сбрасывает счётчик повторений, вызывает validate() и start().
     */
    void on_StartPushButton_clicked();

    /*!
     * \brief Слот отображения ошибок на экран
     * \param текст ошибки
     */
    void onError(QString error);

    void on_SourceFilesPathPushButton_clicked();

    void on_ResultFilesPathPushButton_clicked();

    void on_PausePushButton_clicked();

    void on_ContinuePushButton_clicked();

    void on_TerminatePushButton_clicked();

private:

    /*!
     * \brief Функция проверки заполнения формы. Заполняет соответствующие приватные поля
     * \return успешность проверки
     */
    bool validate();

    Ui::MainWindow *ui;

    bool waitingForRestart = false;

    /*!
     * \brief маска для фильтрации исходных файлов
     */
    QString mask="";

    /*!
     * \brief Директории файлов-исходников и результирующих файлов
     */
    QString sourcePath="";
    QString resultPath="";

    /*!
     * \brief HEX-функция
     */
    QString function="";

    /*!
     * \brief Режим разрешения конфликтов результирующих файлов
     */
    WorkModes::CollisionResolveModes resolveMode;

    /*!
     * \brief Режим фильтрации файлов
     */
    WorkModes::FilterModes filterMode;

    /*!
     * \brief время в мсек для перезапуска обработки
     */
    long long timer;

    QMutex pauseMutex;
    QWaitCondition pauseCondition;

    /*!
     * \brief Таймер перезапуска обработки
     */
    QTimer restartTimer;

    /*!
     * \brief Таймер обновления статус-бара в соответсвии с прогрессом
     */
    QTimer statusBarTimer;

    /*!
     * \brief Число выполненных повторений по таймеру
     */
    long long globalRunsCounter = 0;

    /*!
     * \brief Обработка эвента закрытия
     * \param event
     */
    void closeEvent(QCloseEvent *event) override;

    /*!
     * \brief Обработчик файлов
     */
    FileWorker *fw;
};
#endif // MAINWINDOW_H
