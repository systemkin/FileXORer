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
    this->ui->ContinuePushButton->setDisabled(true);
    this->ui->PausePushButton->setDisabled(true);
    this->ui->TerminatePushButton->setDisabled(true);
    connect(&restartTimer, &QTimer::timeout, this,  &MainWindow::start);
    restartTimer.setSingleShot(true);
    statusBarTimer.setInterval(1000);
    connect(&statusBarTimer, &QTimer::timeout, this,  &MainWindow::renewStatusBar);

    QRegularExpression rxTime("^[0-9]*$");
    QRegularExpressionValidator *validatorTime = new QRegularExpressionValidator(rxTime, this);
    this->ui->TimerLineEdit->setValidator(validatorTime);

    QRegularExpression rxHex("^[0-9a-fA-F]{16}$");
    QRegularExpressionValidator *validatorHex = new QRegularExpressionValidator(rxHex, this);
    this->ui->FunctionLine->setValidator(validatorHex);

    fw = new FileWorker(&pauseMutex, &pauseCondition);
    connect(fw, &FileWorker::end, this, &MainWindow::onEnd);
    connect(fw, &FileWorker::unspecifiedError, this, &MainWindow::onError);
}

MainWindow::~MainWindow()
{
    delete fw;
    delete ui;
}

void MainWindow::on_StartPushButton_clicked()
{
    if (validate()) {
        this->ui->StartPushButton->setDisabled(true);
        start();
    }
}

void MainWindow::start() {

    this->ui->inputWidget->setDisabled(true);
    this->ui->PausePushButton->setEnabled(true);

    statusBarTimer.start(1000);
    fw->start(sourcePath, resultPath, mask, function, this->ui->DeleteSourcesCheckBox->isChecked(), filterMode, resolveMode);

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


void MainWindow::onEnd() {
    statusBarTimer.stop();
    if (this->ui->RestartCheckBox->isChecked()) {
        this->statusBar()->showMessage(QString("Done %1 run! Waiting for restart...").arg(globalRunsCounter++));
        restartTimer.setInterval(timer*1000);
        restartTimer.start();
        return;
    }
    else
        waitingForRestart = false;

    this->statusBar()->showMessage("Done!");
    this->ui->inputWidget->setDisabled(false);
    this->ui->PausePushButton->setDisabled(true);
    this->ui->StartPushButton->setEnabled(true);
}

void MainWindow::on_PausePushButton_clicked()
{
    restartTimer.stop();
    statusBarTimer.stop();
    this->ui->ContinuePushButton->setEnabled(true);
    this->ui->PausePushButton->setDisabled(true);
    this->ui->TerminatePushButton->setEnabled(true);

    fw->pause(true);
}


bool MainWindow::validate() {

    this->statusBar()->showMessage("Starting...");

    mask = this->ui->MaskLineEdit->text();
    if (!mask.isEmpty()) {

        if ((mask.split(".").size() != 2) ||
            (mask.indexOf(".") == mask.size()-1)){
            QMessageBox::information(this, "Information", "Mask set wrong");
            this->statusBar()->showMessage("Mask set wrong");
            return false;
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
        return false;
    }

    if(resultPath == "") {
        QMessageBox::information(this, "Information", "Result path is not set");
        this->statusBar()->showMessage("Result path is not set");
        return false;
    }
    if (sourcePath == resultPath) {
        QMessageBox::information(this, "Information", "Same paths is not supported");
        this->statusBar()->showMessage("Same paths is not supported");
        return false;
    }
    function = this->ui->FunctionLine->text();
    if (function.size() != 16) {
        QMessageBox::information(this, "Information", "Function is not set or set wrong");
        this->statusBar()->showMessage("Function is not set or set wrong");
        return false;
    }

    QSet<QChar> allowedChars = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                'A', 'B', 'C', 'D', 'E', 'F'};
    for (auto it = function.begin(); it != function.end(); ++it) {
        if (!allowedChars.contains(*it)) {
            QMessageBox::information(this, "Information", "Function is not set or set wrong");
            this->statusBar()->showMessage("Function is not set or set wrong");
            return false;
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
        return false;
    }
    if (this->ui->RestartCheckBox->isChecked()) {
        bool ok = false;
        timer = QString(this->ui->TimerLineEdit->text()).toInt(&ok);
        if (!ok) {
            QMessageBox::information(this, "Information", "Timer is not set or set wrong");
            this->statusBar()->showMessage("Timer is not set or set wrong");
            return false;
        }
    }
    if (this->ui->RestartCheckBox->isChecked())
        waitingForRestart = true;
    else
        waitingForRestart = false;

    globalRunsCounter = 0;

    this->statusBar()->showMessage("All checks passed");
    return true;
}

void MainWindow::on_ContinuePushButton_clicked()
{
    if (waitingForRestart == true) {
        restartTimer.start();
    } else {
        statusBarTimer.start();
    }
    this->ui->ContinuePushButton->setDisabled(true);
    this->ui->TerminatePushButton->setDisabled(true);
    this->ui->PausePushButton->setEnabled(true);
    fw->pause(false);
    pauseCondition.wakeAll();
}
void MainWindow::renewStatusBar() {

    this->statusBar()->showMessage(QString("Done %1 %. Open files for %2 KB").arg((int)fw->getProgressPercent()).arg(fw->getLoadedSize()/1024));
}
void MainWindow::closeEvent(QCloseEvent *event) {

    fw->abort();
    pauseCondition.wakeAll();
    event->accept();
}
void MainWindow::onError(QString error) {
    QMessageBox::information(this, "Error", error);
}


void MainWindow::on_TerminatePushButton_clicked()
{
    fw->abort();
    pauseCondition.wakeAll();
    delete fw;

    fw = new FileWorker(&pauseMutex, &pauseCondition);
    connect(fw, &FileWorker::end, this, &MainWindow::onEnd);
    connect(fw, &FileWorker::unspecifiedError, this, &MainWindow::onError);

    this->ui->inputWidget->setEnabled(true);
    this->ui->StartPushButton->setEnabled(true);
    this->ui->ContinuePushButton->setDisabled(true);
    this->ui->PausePushButton->setDisabled(true);
    this->ui->TerminatePushButton->setDisabled(true);

    this->statusBar()->showMessage("Process was terminated");
}

