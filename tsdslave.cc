/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or witut
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "sns.grpc.pb.h"

using csce438::IPPortInfo;
using csce438::ListReply;
using csce438::Message;
using csce438::Reply;
using csce438::Request;
using csce438::SNSSlaveService;
using google::protobuf::Duration;
using google::protobuf::Timestamp;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

struct Client
{
    std::string username;
    bool connected = true;
    ServerReaderWriter<Message, Message> *stream = 0;
};

class SNSSlaveServiceImpl final : public SNSSlaveService::Service
{
    Status RoutingSlave(ServerContext *context,
                        ServerReaderWriter<Message, Message> *stream) override
    {
        Message message;
        Client *c;
        while (stream->Read(&message))
        {
            std::string ip_address = message.ip_address();
            std::string port_num = message.port_num();

            if (message.msg() == "Still Alive?") // Slave recieves msg asking if it's still alive from router
            {
                Message new_msg;

                new_msg.set_msg("Slave Alive"); // slave responds it's alive
                stream->Write(new_msg);
            }
        }
        //If the client disconnected from Chat Mode, set connected to false
        c->connected = false;
        return Status::OK;
    }

    Status MasterSlave(ServerContext *context,
                       ServerReaderWriter<Message, Message> *stream) override
    {
        Message message;
        Client *c;
        stream->Read(&message);
        std::string ip_address = message.ip_address();
        std::string port_num = message.port_num();
        while (stream->Read(&message))
        {
            if (message.msg() == "Master Alive") // Slave recieves msg from master saying it's still alive
            {
                // Slave should let master know it's alive and okay
                Message new_msg;
                new_msg.set_msg("Slave Alive"); // slave replies it's alive
                stream->Write(new_msg);
            }
        }
        // resurrect master
        //std::cout << "SLAVE: Stopped receiving from the master! " << std::endl;
        //int port_num_int = std::stoi(port_num);
        //port_num_int = port_num_int + 7;
        //std::string p = std::to_string(port_num_int);
        std::string cmd_torun = "./tsd -p ";
        cmd_torun.append(port_num);
        cmd_torun.append(" -i ");
        cmd_torun.append(ip_address);
        cmd_torun.append(" -s master");
        char buf[256];
        memset(buf, 0, sizeof(buf));
        strcpy(buf, cmd_torun.c_str());
        sleep(30);
        std::cout << "SLAVE: FINISHED SLEEPING  " << buf << std::endl;
        std::cout << "SLAVE: command to run to resurrect my master partner " << buf << std::endl;
        system(buf);

        return Status::OK;
    }
};

void RunServer(std::string port_no, std::string ip)
{
    std::string server_address = ip + ":" + port_no;
    SNSSlaveServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << " Slave Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char **argv)
{

    std::string port = "3011";
    std::string ip_address = "10.0.2.5";
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:i:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = optarg;
            break;
        case 'i':
            ip_address = optarg;
            break;
        default:
            std::cerr << "Invalid Command Line Argument\n";
        }
    }
    RunServer(port, ip_address);

    return 0;
}