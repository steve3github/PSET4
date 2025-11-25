#include <grpcpp/grpcpp.h>
#include "ocr_server.h"

int main() {
    std::string address = "0.0.0.0:50051";  // Change to desired server address
    OCRServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
    return 0;
}
