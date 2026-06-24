#ifndef FILEWORKER_H
#define FILEWORKER_H

#include <QWaitCondition>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QWidget>
#include <QDir>

namespace WorkModes {
    // Варианты разрешения конфликтов имён файлов
    enum CollisionResolveModes {
        Rewrite = 0,    // Перезаписать файлы
        Append,         // Добавить "_X" к файлу
        Ignore,         // Игнорировать соответсвующий исходный файл
    };

    // Режимы фильтрации по маске
    enum FilterModes {
        FileType = 0,   // Указан тип файлов (".AAA")
        FileName,       // Указано имя файла ("BBB.AAA")
        NoFilter,       // Без фильтра (Берём все файлы)
    };
}
class MainWindow;
class FileWorker : public QObject
{

    Q_OBJECT
public:

    /*!
     * \brief Конструктор с заданием параметров синхронизации
     * \param pauseMutex - мьютекс, отвечающий за установку паузы
     * \param pauseCondition - кондишн, отвечающий за установку паузы
     * \param parent
     */
    FileWorker(QMutex *pauseMutex, QWaitCondition *pauseCondition, QObject *parent = nullptr);

    ~FileWorker();
    //!
    //! \brief Функция для получения процента выполнения текущих запущенных задач
    //! \return Процент выполнения
    //!
    char getProgressPercent();

    //!
    //! \brief Функция для получения объёма текущих задач
    //! \return Сумма байт файлов
    //!
    long long getLoadedSize();

public slots:
    /*!
     * \brief Функция запуска обработки файлов. Рассчитываем что все значения валидны, а директории существуют
     * \param sourcePath - путь к файлам-исходнникам
     * \param resultPath - путь к результирующим файлам
     * \param mask - маска файлов для поиска среди исходников. Либо "AAA.BBB" либо ".BBB"
     * \param function - Функция для примененияя к исходным файлам в HEX. 16 символов
     * \param needToDelete - Надо ли удалять исходные файлы
     * \param filterMode - режим фильтрации входных файлов
     * \param resolveMode - режим разрешения конфликтов имён выходных файлов
     * \return
     */
    bool start(QString &sourcePath, QString &resultPath, QString &mask, QString &function, bool needToDelete, WorkModes::FilterModes &filterMode, WorkModes::CollisionResolveModes &resolveMode);

    /*!
     * \brief Функция для установки-снятия паузы
     * \param pause - true для установки, false для снятия
     */
    void pause(bool pause);

    /*!
     * \brief Функция для  экстренного завершения
     */
    void abort();

signals:
    /*!
     * \brief Сигнал завершения обработки
     */
    void end();

    /*!
     * \brief Сигнал ошибки обработки
     * \param Текст ошибки
     */
    void unspecifiedError(QString errorDesc);

private:

    /*!
     * \brief Функция обработки конкретного файла
     * \param sourcePath - путь к файлу
     * \param destinationPath - ожидаемый путь к результирующему файлу
     * \param function - HEX функция
     * \param needToDelete - надо ли удалить исходный файл
     * \param resolveMode - режим разрешения конфликтов коллизий результирующих файлов
     * \return Успешна ли обработка файла
     */
    bool doFile(QString &sourcePath, QString &destinationPath, QString &function, bool needToDelete, WorkModes::CollisionResolveModes &resolveMode);

    /*!
     * \brief Для отслеживаеня прогресса выполнения
     */
    QList<QFuture<void>> futures;
    std::vector<std::unique_ptr<QFutureWatcher<void>>> futureWatchers;

    /*!
     * \brief Для постановки-снятия с паузы
     */
    QMutex *pauseMutex;
    QWaitCondition *pauseCondition;

    MainWindow *mw;

    std::atomic<bool> aborting{false};
    std::atomic<bool> paused{false};

    /*!
     * \brief Число обработанных байт
     */
    std::atomic<long long> progress{0};

    /*!
     * \brief Число байт в обработке
     */
    std::atomic<long long> filesSize{0};

    /*!
     * \brief Отслеживание процесса обработки по числу файлов
     */
    int totalFiles = 0;
    int finishedFiles = 0;

};

#endif // FILEWORKER_H
