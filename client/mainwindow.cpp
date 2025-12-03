#include "mainwindow.h"

#include <QFileDialog>
#include <QVBoxLayout>
#include <QBuffer>
#include <QUuid>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    client = new OCRClient();
    connect(client, &OCRClient::resultReady, this, &MainWindow::handleResult);

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    uploadButton = new QPushButton("Upload Images");
    progressBar = new QProgressBar();
    // resultsList = new QListWidget();

    layout->addWidget(uploadButton);
    layout->addWidget(progressBar);
    // layout->addWidget(resultsList);

    gridContainer = new QWidget(this);
    gridLayout = new QGridLayout(gridContainer);
    gridLayout->setSpacing(15);
    gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    layout->addWidget(gridContainer);

    setCentralWidget(central);
    resize(700, 400);

    connect(uploadButton, &QPushButton::clicked, this, &MainWindow::onUploadClicked);

    currentBatchId = QUuid::createUuid().toString();
}

void MainWindow::onUploadClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Upload Images");

    for (QString path : files) {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        QByteArray data = f.readAll();

        QString imgId = QUuid::createUuid().toString();

        // --- Create preview widget ---
        ImageWidget item;
        item.thumbnail = new QLabel();
        item.thumbnail->setFixedSize(120, 120);
        item.thumbnail->setStyleSheet("border: 1px solid gray;");

        QPixmap pix(path);
        item.thumbnail->setPixmap(
            pix.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );

        item.status = new QLabel("Processing...");
        item.status->setAlignment(Qt::AlignCenter);
        item.status->setWordWrap(true);

        QWidget *cell = new QWidget();
        QVBoxLayout *cellLayout = new QVBoxLayout(cell);
        cellLayout->addWidget(item.thumbnail);
        cellLayout->addWidget(item.status);

        int index = imageWidgets.size();
        int row = index / 3;   // 3 columns
        int col = index % 3;

        gridLayout->addWidget(cell, row, col);

        imageWidgets[imgId] = item;

        totalImages++;
        progressBar->setMaximum(totalImages);

        client->sendImage(currentBatchId, imgId, data);
    }
}


void MainWindow::handleResult(QString imageId, QString text) {
    processedImages++;
    progressBar->setValue(processedImages);

    if (imageWidgets.contains(imageId)) {
        imageWidgets[imageId].status->setText(text);
    }

    if (processedImages == totalImages) {
        totalImages = 0;
        processedImages = 0;
        currentBatchId = QUuid::createUuid().toString();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (!gridLayout || imageWidgets.isEmpty())
        return;

    int cellWidth = 150;  // thumbnail + margins
    int availableWidth = gridContainer->width();

    int columns = availableWidth / cellWidth;
    if (columns < 1)
        columns = 1;

    // Re-layout all widgets
    int index = 0;
    for (auto it = imageWidgets.begin(); it != imageWidgets.end(); ++it) {
        int row = index / columns;
        int col = index % columns;

        QWidget *cellWidget = it.value().thumbnail->parentWidget();
        gridLayout->addWidget(cellWidget, row, col);

        index++;
    }
}

