#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "./pb_cpp/maths.grpc.pb.h"

using maths::MathRequest;
using maths::MathResponse;
using maths::Maths;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;

double op(double num1, double num2, std::string oper) {
    if (oper == "+") {
        return num1 + num2;
    } else if (oper == "-") {
        return num1 - num2;
    } else if (oper == "*") {
        return num1 * num2;
    } else if (oper == "/") {
        return num1 / num2;
    }
}

// Logic and data behind the server's behavior.
class MathsService final : public Maths::Service {
    Status DoMathStream(
        ServerContext *context,
        ServerReaderWriter<MathResponse, MathRequest> *stream) override {
        MathRequest req;
        MathResponse res;
        while (stream->Read(&req)) {
            double result = op(req.number1(), req.number2(), req.operation());
            res.set_result(result);
            stream->Write(res);
        }
        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("localhost:50051");

    MathsService service;
    ServerBuilder builder;

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());

    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char **argv) {
    RunServer();
    return 0;
}
