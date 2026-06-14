#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->ui->ContinueButton->setDisabled(true);
    this->ui->StopPushButton->setDisabled(true);
    connect(this, &MainWindow::start, this, &MainWindow::onStart);
    connect(this, &MainWindow::end, this, &MainWindow::onEnd);
    connect(&restartTimer, &QTimer::timeout, this,  &MainWindow::onStart);
    restartTimer.setSingleShot(true);

    connect(&statusBarTimer, &QTimer::timeout, this,  &MainWindow::renewStatusBar);

    QRegularExpression rx("^[0-9]*$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(rx, this);
    this->ui->TimerLineEdit->setValidator(validator);
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::on_StartButton_clicked()
{
    this->statusBar()->showMessage("Starting...");
    this->ui->StopPushButton->setEnabled(true);


    mask = this->ui->MaskLineEdit->text();
    if (!mask.isEmpty()) {

        if ((mask.split(".").size() != 2) ||
            (mask.indexOf(".") == mask.size()-1)){
            QMessageBox::information(this, "Information", "Mask set wrong");
            this->statusBar()->showMessage("Mask set wrong");
            return -1;
        }
        if (mask.indexOf(".") == 0) {
            filterMode = WorkModes::FilterModes::FileType;
        } else filterMode = WorkModes::FilterModes::FileName;

    } else {
        filterMode = WorkModes::FilterModes::NoFilter;
    }

    if(sourcePath == "") {
        QMessageBox::information(this, "Information", "Source path is not set");
        this->statusBar()->showMessage("Source path is not set");
        return -1;
    }

    if(resultPath == "") {
        QMessageBox::information(this, "Information", "Result path is not set");
        this->statusBar()->showMessage("Result path is not set");
        return -1;
    }
    if (sourcePath == resultPath) {
        QMessageBox::information(this, "Information", "Same paths is not supported");
        this->statusBar()->showMessage("Same paths is not supported");
        return -1;
    }
    function = this->ui->FunctionLine->text();
    if (function.size() != 16) {
        QMessageBox::information(this, "Information", "Function is not set or set wrong");
        this->statusBar()->showMessage("Function is not set or set wrong");
        return -1;
    }

    QSet<QChar> allowedChars = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                'A', 'B', 'C', 'D', 'E', 'F'};
    for (auto it = function.begin(); it != function.end(); ++it) {
        if (!allowedChars.contains(*it)) {
            QMessageBox::information(this, "Information", "Function is not set or set wrong");
            this->statusBar()->showMessage("Function is not set or set wrong");
            return -1;
        }
    }

    if (this->ui->AppendRadioButton->isChecked()) {
        resolveMode = WorkModes::CollisionResolveModes::Append;
    }
    else if (this->ui->IgnoreRadioButton->isChecked()) {
        resolveMode = WorkModes::CollisionResolveModes::Ignore;
    }
    else if (this->ui->RewriteRadioButton->isChecked()) {
        resolveMode = WorkModes::CollisionResolveModes::Rewrite;
    } else {
        QMessageBox::information(this, "Information", "Collision resolve mode is not set");
        this->statusBar()->showMessage("Collision resolve mode is not set");
        return -1;
    }
    if (this->ui->RestartCheckBox->isChecked()) {
        bool ok = false;
        timer = QString(this->ui->TimerLineEdit->text()).toInt(&ok);
        if (!ok) {
            QMessageBox::information(this, "Information", "Timer is not set or set wrong");
            this->statusBar()->showMessage("Timer is not set or set wrong");
            return -1;
        }
    }


    this->statusBar()->showMessage("All checks passed");
    emit start();
    return 1;
}


void MainWindow::on_SourceFilesPathPushButton_clicked()
{
    sourcePath=QFileDialog::getExistingDirectory(
        this,
        "Select source directory",
        "",
        QFileDialog::ShowDirsOnly
        );
    if (sourcePath != "")
        this->ui->SourceFilesPathLabel->setText(sourcePath);
    else
        this->ui->SourceFilesPathLabel->setText("No path specified");
}


void MainWindow::on_ResultFilesPathPushButton_clicked()
{
    resultPath=QFileDialog::getExistingDirectory(
        this,
        "Select result directory",
        "",
        QFileDialog::ShowDirsOnly
        );
    if (resultPath != "")
        this->ui->ResultFilesPathLabel->setText(resultPath);
    else
        this->ui->ResultFilesPathLabel->setText("No path specified");
}

int MainWindow::onStart() {
    waitingForRestart = false;
    disconnect(&futureWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::end);
    futures.clear();

    QDir sourceDir(sourcePath);
    QDir resultDir(resultPath);

    if ((!resultDir.exists()) || (!sourceDir.exists())) {
        QMessageBox::information(this, "Information", "Directory was deleted!");
        this->statusBar()->showMessage("Directory was deleted!");
        return -1;
    }

    sourceDir.setFilter(QDir::Files);

    QStringList sourceFiles;

    if (filterMode == WorkModes::FilterModes::FileName) {
        if (sourceDir.exists(mask))
            sourceFiles.append(mask);
        else {
            this->statusBar()->showMessage("No source files found");
            emit end();
            return 0;
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
    this->statusBar()->showMessage(QString("Found %1 source files").arg(sourceFiles.size()));
    currFileNum = 0;
    for (auto it = sourceFiles.begin(); it != sourceFiles.end(); ++it) {
        results[sourceDir.absoluteFilePath(*it)].first = 0;
        results[sourceDir.absoluteFilePath(*it)].second = 0;
    }
    filesAmnt = sourceFiles.size();

    renewStatusBar();
    statusBarTimer.start(1000);
    for (auto it = sourceFiles.begin(); it != sourceFiles.end(); ++it) {
        QString sFilepath = sourceDir.absoluteFilePath(*it);
        QString rFilePath = resultDir.absoluteFilePath(*it);
        this->statusBar()->showMessage(QString("Processing file %1/%2").arg(currFileNum).arg(filesAmnt));
        doFile(sFilepath, rFilePath);
        currFileNum++;
    }

    if (!futures.isEmpty()) {
        futureWatcher.setFuture(futures.last());
        connect(&futureWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::end);
    } else {
        emit end();
    }

    return 1;
}
int MainWindow::doFile(QString &sourcePath, QString &destinationPath) {
    if (aborting) return -1;
    QFile * sfile = new QFile(sourcePath);
    if (!sfile->open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, "Information", QString("Can't open file %1").arg(sourcePath));
        this->statusBar()->showMessage(QString("Can't open file %1").arg(sourcePath));
        return -1;
    }
    if (sfile->size() % 8 != 0) {
        QMessageBox::information(this, "Information", QString("File %1 is not 8-sized").arg(sourcePath));
        this->statusBar()->showMessage(QString("File %1 is not 8-sized").arg(sourcePath));
        return -1;
    }

    QFile *dfile = new QFile(destinationPath);
    if (resolveMode == WorkModes::CollisionResolveModes::Rewrite) {
        if (!dfile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::information(this, "Information", QString("Can't open file %1").arg(destinationPath));
            this->statusBar()->showMessage(QString("Can't open file %1").arg(destinationPath));
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
            }
        } while (!ok);
    } else if (resolveMode == WorkModes::CollisionResolveModes::Ignore) {
        bool ok = dfile->open(QIODevice::WriteOnly | QIODevice::NewOnly);
        if (!ok) return 1;
    }


    bool needToDelete = this->ui->DeleteSourcesCheckBox->isChecked();
    QFuture<void> future = QtConcurrent::run(QThreadPool::globalInstance(), [this, sfile, dfile, needToDelete]{
        QDataStream sourceStream(sfile);
        QDataStream destinationStream(dfile);
        char *buf;
        buf = (char*) malloc(sizeof(char)*8);
        qint64 size = 8;
        char pos = 0;
        uint16_t cnt = 0;
        results[sfile->fileName()].first = sfile->size();
        results[sfile->fileName()].second = cnt;
        while (!sourceStream.atEnd() && (!aborting)) {

            QMutexLocker locker(&pauseMutex);
            while (paused) {
                pauseCondition.wait(&pauseMutex);
                if (aborting) {
                    free(buf);
                    return;
                }
            }

            if (cnt == 1024) {
                results[sfile->fileName()].second += cnt*8;
                cnt = 0;
                dfile->flush();
            }
            sourceStream.readRawData(buf, size);
            QString hexRepresentation = QString(function.mid(pos, 16));
            uint64_t byteFunc = qbswap(hexRepresentation.toULongLong(nullptr, 16));

            uint64_t byteFile = 0;
            memcpy(&byteFile, buf, sizeof(char)*8);

            uint64_t byteXOR = byteFunc ^ byteFile;

            destinationStream.writeRawData((char*) &byteXOR, size);
            pos = (pos+16)%16;
            cnt++;
        }
        results[sfile->fileName()].second = results[sfile->fileName()].first;
        free(buf);
        dfile->close();
        if(needToDelete) {
            sfile->remove();
        } else {
            sfile->close();
        }
        delete dfile;
        delete sfile;
    });
    futures.append(future);
    return 1;
}


void MainWindow::onEnd() {
    if (this->ui->RestartCheckBox->isChecked()) {
        waitingForRestart = true;
        this->statusBar()->showMessage(QString("Done %1 run! Waiting for restart...").arg(globalRunsCounter++));
        restartTimer.setInterval(timer*1000);
        restartTimer.start();
        return;
    }
    statusBarTimer.stop();
    this->statusBar()->showMessage("Done!");
    this->ui->StartButton->setEnabled(true);
}

void MainWindow::on_StopPushButton_clicked()
{
    restartTimer.stop();
    this->ui->StartButton->setDisabled(true);
    this->ui->ContinueButton->setEnabled(true);
    this->ui->StopPushButton->setDisabled(true);
    paused = true;
}


void MainWindow::on_ContinueButton_clicked()
{
    if (waitingForRestart == true) {
        restartTimer.start();
    }
    this->ui->ContinueButton->setDisabled(true);
    this->ui->StopPushButton->setEnabled(true);
    paused = false;
    pauseCondition.wakeAll();
}
void MainWindow::renewStatusBar() {
    long long processedSize = 0;
    long long totalSize = 0;
    for(auto it = results.begin(); it != results.end(); ++it) {
        processedSize += it->second;
        totalSize += it->first;
    }
    this->statusBar()->showMessage(QString("Done %1 KB. Opent files for %2 KB").arg(processedSize/1024).arg(totalSize/1024));
}
void MainWindow::closeEvent(QCloseEvent *event) {
    aborting = true;
    pauseCondition.wakeAll();
    event->accept();
}
