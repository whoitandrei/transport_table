#include <iostream>

#include "board.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class BoardServiceImpl final : public transport::board::BoardService::Service
{
    grpc::Status GetBoard(grpc::ServerContext *ctx,
                          const transport::board::BoardRequest *request,
                          transport::board::Board *response)
    {
        std::cerr << "Requested: " << request->device().device_id() << std::endl;

        response->mutable_server_time()->set_unix_seconds(time(nullptr));

        return grpc::Status::OK;
    }

};

int main()
{
    BoardServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cerr << "Server started" << std::endl;
    server->Wait();
}