#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QGridLayout>
#include <QLabel>
#include <QMap>

#include "grpc_client.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void resizeEvent(QResizeEvent *event) override;
    void onUploadClicked();
    void handleResult(QString imageId, QString text);

private:
    OCRClient *client;

    QPushButton *uploadButton;
    QProgressBar *progressBar;
    QWidget *gridContainer;
    QGridLayout *gridLayout;

    struct ImageWidget {
        QLabel *thumbnail;
        QLabel *status;
    };

    QMap<QString, ImageWidget> imageWidgets;


    QString currentBatchId;
    int totalImages = 0;
    int processedImages = 0;
};
