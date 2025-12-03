#include "grpc_client.h"
#include <thread>
#include <QDebug>

OCRClient::OCRClient() {
    channel = grpc::CreateChannel("192.168.1.5:50051", grpc::InsecureChannelCredentials());
    stub = ocr::OCRService::NewStub(channel);

    grpc::ClientContext *ctx = new grpc::ClientContext();
    stream = stub->SendImage(ctx);

    if (!stream) {
        qDebug() << "Failed to open gRPC stream!";
    } else {
        qDebug() << "Stream opened successfully.";
    }

    std::thread(&OCRClient::listenForResponses, this).detach();
}

void OCRClient::sendImage(QString batchId, QString imageId, QByteArray data) {
    ocr::ImageRequest req;
    req.set_batch_id(batchId.toStdString());
    req.set_image_id(imageId.toStdString());
    req.set_data(data.data(), data.size());
    stream->Write(req);
}

void OCRClient::listenForResponses() {
    ocr::OcrResult res;
    while (stream->Read(&res)) {
        emit resultReady(
            QString::fromStdString(res.image_id()),
            QString::fromStdString(res.text())
        );
    }
}
