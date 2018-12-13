#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "./pb_cpp/maths.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

using maths::MathRequest;
using maths::MathResponse;
using maths::Maths;

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;

int messageCount = 1000000;

class MathsClient {
  public:
    MathsClient(std::shared_ptr<Channel> channel)
        : stub_(Maths::NewStub(channel)) {}

    void DoMathStream(double num1, double num2, std::string op) {
        ClientContext context;

        std::shared_ptr<ClientReaderWriter<MathRequest, MathResponse>> stream(
            stub_->DoMathStream(&context));

        std::thread reader([stream]() {
            MathResponse res;
            for (int k = 0; k < messageCount; k++) {
                stream->Read(&res);
                if (res.result() != 32) {
                    std::cout << "WRONG RESULT!" << std::endl;
                    exit(1);
                }
            }
        });

        for (int i = 0; i < messageCount; i++) {
            MathRequest *req = new MathRequest();
            req->set_number1(num1);
            req->set_number2(num2);
            req->set_operation(op);
            stream->Write(*req);
            delete req;
        }
        stream->WritesDone();
        reader.join();
        Status status = stream->Finish();
        if (!status.ok()) {
            std::cout << "RouteChat rpc failed." << std::endl;
        }
    }

  private:
    std::unique_ptr<Maths::Stub> stub_;
};

int main(int argc, char **argv) {
    MathsClient client(grpc::CreateChannel("localhost:50051",
                                           grpc::InsecureChannelCredentials()));

    client.DoMathStream(64, 2, "/");

    return 0;
}
