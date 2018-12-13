#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpc++/grpc++.h>

#include "./pb_cpp/maths.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

using maths::MathRequest;
using maths::MathResponse;
using maths::Maths;

enum class Type { READ = 1, WRITE = 2, CONNECT = 3, DONE = 4, FINISH = 5 };

// NOTE: This is a complex example for an asynchronous, bidirectional streaming
// server. For a simpler example, start with the
// greeter_server/greeter_async_server first.

// Most of the logic is similar to AsyncBidiGreeterClient, so follow that class
// for detailed comments. Two main differences between the server and the client
// are: (a) Server cannot initiate a connection, so it first waits for a
// 'connection'. (b) Server can handle multiple streams at the same time, so
// the completion queue/server have a longer lifetime than the client(s).
class AsyncMathsServer {
  public:
    AsyncMathsServer() {
        // In general avoid setting up the server in the main thread
        // (specifically, in a constructor-like function such as this). We
        // ignore this in the context of an example.
        std::string server_address("0.0.0.0:50051");

        ServerBuilder builder;
        builder.AddListeningPort(server_address,
                                 grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);
        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();

        // This initiates a single stream for a single client. To allow multiple
        // clients in different threads to connect, simply 'request' from the
        // different threads. Each stream is independent but can use the same
        // completion queue/context objects.
        stream_.reset(
            new ServerAsyncReaderWriter<MathResponse, MathRequest>(&context_));

        service_.RequestDoMathStream(&context_, stream_.get(), cq_.get(),
                                     cq_.get(),
                                     reinterpret_cast<void *>(Type::CONNECT));

        // This is important as the server should know when the client is done.
        context_.AsyncNotifyWhenDone(reinterpret_cast<void *>(Type::DONE));

        grpc_thread_.reset(
            new std::thread((std::bind(&AsyncMathsServer::GrpcThread, this))));
        std::cout << "Server listening on " << server_address << std::endl;
    }

    void SetResponse(const std::string &response) {
        if (response == "quit" && IsRunning()) {
            stream_->Finish(grpc::Status::CANCELLED,
                            reinterpret_cast<void *>(Type::FINISH));
        }
        response_str_ = response;
    }

    ~AsyncMathsServer() {
        std::cout << "Shutting down server...." << std::endl;
        server_->Shutdown();
        // Always shutdown the completion queue after the server.
        cq_->Shutdown();
        grpc_thread_->join();
    }

    bool IsRunning() const { return is_running_; }

  private:
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

    void AsyncWaitForMathRequest() {
        if (IsRunning()) {
            // In the case of the server, we wait for a READ first and then
            // write a response. A server cannot initiate a connection so the
            // server has to wait for the client to send a message in order for
            // it to respond back.
            stream_->Read(&request_, reinterpret_cast<void *>(Type::READ));
        }
    }

    void AsyncHelloSendResponse() {
        MathResponse response;
        response.set_result(
            op(request_.number1(), request_.number2(), request_.operation()));
        stream_->Write(response, reinterpret_cast<void *>(Type::WRITE));
    }

    void GrpcThread() {
        while (true) {
            void *got_tag = nullptr;
            bool ok = false;
            if (!cq_->Next(&got_tag, &ok)) {
                std::cerr << "Server stream closed. Quitting" << std::endl;
                break;
            }

            if (ok) {
                // std::cout << std::endl
                //           << "**** Processing completion queue tag " <<
                //           got_tag
                //           << std::endl;
                switch (static_cast<Type>(reinterpret_cast<size_t>(got_tag))) {
                case Type::READ:
                    //   std::cout << "Read a new message." << std::endl;
                    AsyncHelloSendResponse();
                    break;
                case Type::WRITE:
                    //   std::cout << "Sending message (async)." << std::endl;
                    AsyncWaitForMathRequest();
                    break;
                case Type::CONNECT:
                    //   std::cout << "Client connected." << std::endl;
                    AsyncWaitForMathRequest();
                    break;
                case Type::DONE:
                    //   std::cout << "Server disconnecting." << std::endl;
                    is_running_ = false;
                    break;
                case Type::FINISH:
                    //   std::cout << "Server quitting." << std::endl;
                    break;
                default:
                    //   std::cerr << "Unexpected tag " << got_tag << std::endl;
                    GPR_ASSERT(false);
                }
            }
        }
    }

  private:
    MathRequest request_;
    std::string response_str_ = "Default server response";
    ServerContext context_;
    std::unique_ptr<ServerCompletionQueue> cq_;
    Maths::AsyncService service_;
    std::unique_ptr<Server> server_;
    std::unique_ptr<ServerAsyncReaderWriter<MathResponse, MathRequest>> stream_;
    std::unique_ptr<std::thread> grpc_thread_;
    bool is_running_ = true;
};

int main(int argc, char **argv) {
    AsyncMathsServer server;

    std::string response;
    while (server.IsRunning()) {
        std::cout << "Enter next set of responses (type quit to end): ";
        std::cin >> response;
        server.SetResponse(response);
    }

    return 0;
}
