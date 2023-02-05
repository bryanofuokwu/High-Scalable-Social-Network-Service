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
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"
#include "client.h"

using csce438::IPPortInfo;
using csce438::ListReply;
using csce438::Message;
using csce438::Reply;
using csce438::Request;
using csce438::SNSService;
using csce438::SNSSlaveService;
using google::protobuf::Duration;
using google::protobuf::Timestamp;
using grpc::Channel;
using grpc::ClientAsyncReaderWriter;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Server;
using grpc::ServerAsyncReaderWriter;
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
    int following_file_size = 0;
    std::vector<Client *> client_followers;
    std::vector<Client *> client_following;
    ServerReaderWriter<Message, Message> *stream = 0;
    bool operator==(const Client &c1) const
    {
        return (username == c1.username);
    }
};

struct MasterServer
{
    std::string ip;
    std::string port_number;
    bool available = true;
};

struct Argument
{
    std::string arg_ip;
    std::string arg_port_number;
    std::string arg_server_type;
};

struct ClientInformation
{
    std::string client_user;
    MasterServer *client_master;
};

class ClientandServer
{
public:
    ClientandServer(const std::string &hname,
                    const std::string &uname,
                    const std::string &p)
        : hostname(hname), username(uname), port(p)
    {
    }
    static void *func(void *arg)
    {
        std::cout << "hello test" << std::endl;
    }
    void Heartbeat(const std::string &ip_address, const std::string &port_num);
    static void *HeartbeatThread(void *arguments);
    void MasterSlave(const std::string &ip_address, const std::string &port_num);
    static void *MasterSlaveThread(void *arguments);
    static std::unique_ptr<SNSSlaveService::Stub> stub_2;
    static std::unique_ptr<SNSService::Stub> stub_original_2_;

private:
    std::string hostname;
    std::string username;
    std::string port;
    // You can have an instance of the client stub
    // as a member variable.
    std::unique_ptr<SNSSlaveService::Stub> stub_;
    std::unique_ptr<SNSService::Stub> stub_original_;

    void SendServerInfo(const std::string &username, const std::string &to_username, const std::string &ip_address, const std::string &port_num);
    void RoutingSlave(const std::string &ip_address, const std::string &port_num);
};

/************************** GLOBAL VARIABLES  **************************/
//Vector that stores every client that has been created
std::vector<Client> client_db;
//std::queue<MasterServer> master_servers_database;
std::vector<MasterServer *> master_objects_database;
std::vector<MasterServer *> master_servers_database(3, NULL);
std::vector<ClientInformation *> client_to_master_servers_database;
std::string server_type = "routing";
std::string port = "3010";
std::string ip = "0:0:0:0";
std::string routing_login_info = "10.0.2.15:3010";
std::string available_master_heartbeat_check = "10.0.2.15:3010";
std::unique_ptr<SNSService::Stub> stub_original_;
std::unique_ptr<SNSSlaveService::Stub> stub_;
ClientandServer *clientandServer_routing = NULL;
ClientandServer *clientandServer_slave = NULL;
std::unique_ptr<SNSService::Stub> ClientandServer::stub_original_2_(nullptr);
std::unique_ptr<SNSSlaveService::Stub> ClientandServer::stub_2(nullptr);
bool connected_to_master = false;
bool send_election_to_client = false;
std::mutex mtx;
std::condition_variable client_master_connection_condition;
bool client_disconnected = false;
MasterServer *new_elected_master = NULL;
/***********************************************************************/

//Helper function used to find a Client object given its username
int find_user(std::string username)
{
    int index = 0;
    for (Client c : client_db)
    {
        if (c.username == username)
            return index;
        index++;
    }
    return -1;
}

Message MakeMessage(const std::string &ip_address, const std::string &port_num, const std::string &msg, const std::string &name)
{
    Message m;
    m.set_ip_address(ip_address);
    m.set_port_num(port_num);
    m.set_msg(msg);
    m.set_username(name);
    return m;
}

class SNSServiceImpl final : public SNSService::Service
{

    Status List(ServerContext *context, const Request *request, ListReply *list_reply) override
    {
        std::cout << "THIS IS LIST IN THE " << server_type << " with: " << ip << ":" << port << std::endl;
        Client user = client_db[find_user(request->username())];
        int index = 0;
        for (Client c : client_db)
        {
            list_reply->add_all_users(c.username);
        }
        std::vector<Client *>::const_iterator it;
        for (it = user.client_followers.begin(); it != user.client_followers.end(); it++)
        {
            list_reply->add_followers((*it)->username);
        }
        return Status::OK;
    }

    Status Follow(ServerContext *context, const Request *request, Reply *reply) override
    {
        std::cout << "THIS IS FOLLOW IN THE " << server_type << " with: " << ip << ":" << port << std::endl;
        std::string username1 = request->username();
        std::string username2 = request->arguments(0);
        int join_index = find_user(username2);
        if (join_index < 0 || username1 == username2)
            reply->set_msg("Follow Failed -- Invalid Username");
        else
        {
            Client *user1 = &client_db[find_user(username1)];
            Client *user2 = &client_db[join_index];
            if (std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end())
            {
                reply->set_msg("Follow Failed -- Already Following User");
                return Status::OK;
            }
            user1->client_following.push_back(user2);
            user2->client_followers.push_back(user1);
            reply->set_msg("Follow Successful");
        }
        return Status::OK;
    }

    Status UnFollow(ServerContext *context, const Request *request, Reply *reply) override
    {
        std::cout << "THIS IS UNFOLLOW IN THE " << server_type << " with: " << ip << ":" << port << std::endl;
        std::string username1 = request->username();
        std::string username2 = request->arguments(0);
        int leave_index = find_user(username2);
        if (leave_index < 0 || username1 == username2)
            reply->set_msg("UnFollow Failed -- Invalid Username");
        else
        {
            Client *user1 = &client_db[find_user(username1)];
            Client *user2 = &client_db[leave_index];
            if (std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end())
            {
                reply->set_msg("UnFollow Failed -- Not Following User");
                return Status::OK;
            }
            user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2));
            user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
            reply->set_msg("UnFollow Successful");
        }
        return Status::OK;
    }

    Status Login(ServerContext *context, const Request *request, Reply *reply) override
    {
        Client c;
        std::string username = request->username();
        int user_index = find_user(username);
        if (user_index < 0)
        {
            c.username = username;
            client_db.push_back(c);
            reply->set_msg("Login Successful!");
        }
        else
        {
            Client *user = &client_db[user_index];
            if (user->connected)
                reply->set_msg("Invalid Username");
            else
            {
                std::string msg = "Welcome Back " + user->username;
                reply->set_msg(msg);
                user->connected = true;
            }
        }
        return Status::OK;
    }

    // if this is the routing server then it should receive credentials about other masters that log in
    // the routing server is always going to be available in the port and host number it registers to
    Status LoginServer(ServerContext *context, const IPPortInfo *ip_port_info, Reply *reply) override
    {
        //std::cout << "ROUTING ENTERED LOGIN SERVER " << master_servers_database.size() << std::endl;
        MasterServer *ms = new MasterServer;
        ms->ip = ip_port_info->ip_address();
        ms->port_number = ip_port_info->port_num();
        ms->available = true; // hasn't been elected yet
        for (int i = 0; i < master_servers_database.size(); i++)
        {
            if (master_servers_database[i] == NULL)
            {
                master_servers_database[i] = ms;
                std::cout << "PUSHED BACK SERVER: " << master_servers_database[i] << std::endl;
                break;
            }
        }
        //routing server is a client when calling the heartbeat
        //std::cout << "ROUTING: I have this many master servers " << ms << " " << master_servers_database.size() << std::endl;
        return Status::OK;
    }

    Status SendServerInfo(ServerContext *context, const IPPortInfo *ip_port_in, IPPortInfo *ip_port_out) override
    {
        std::string ip_address_in = ip_port_in->ip_address();
        std::string port_num_in = ip_port_in->port_num();
        std::string info_to_user = ip_port_in->to_user();
        std::string info_from_user = ip_port_in->from_user();

        // initially will be routing server
        /*
         * FROM MASTER(not the routing): give back info about the routing server
         * FROM ROUTING: need to send credentials of the available master
         * ELSE: send back Status::ERROR
         * */

        // from routing server
        if (info_to_user == "routing_server")
        {
            //std::cout << "ROUTING TO CLIENT: SENDING REDIRECTION INFO" << master_servers_database.size() << std::endl;
            ip_port_out->set_from_user("routing_server");
            ip_port_out->set_to_user("client");

            ClientInformation *new_client = new ClientInformation;
            new_client->client_user = info_from_user;
            for (int i = 0; i < master_servers_database.size(); i++)
            {
                if (master_servers_database[i] != NULL)
                {
                    if ((master_servers_database[i]->available) == true)
                    {
                        ip_port_out->set_ip_address(master_servers_database[i]->ip);
                        ip_port_out->set_port_num(master_servers_database[i]->port_number);
                        master_servers_database[i]->available = false;
                        new_client->client_master = master_servers_database[i];
                        //std::cout << "ROUTING SEND SERVER INFO Available master we connected to: " << master_servers_database[i]->ip << ":" << master_servers_database[i]->port_number <<  " is it available " << master_servers_database[i]->available  << std::endl;
                        break;
                    }
                }
            }
            client_to_master_servers_database.push_back(new_client);
            //std::cout << "CLIENT WITH NEW AVAIL MASTER  " << new_client << " " << new_client->client_master << std::endl;
            connected_to_master = true;
            return Status::OK;
        }

        if (info_to_user == "routing_server_reconnect")
        {
            //std::cout << "ROUTING TO CLIENT: SENDING REDIRECTION INFO AFTER CRASH " << master_servers_database.size() << std::endl;
            ip_port_out->set_from_user("routing_server");
            ip_port_out->set_to_user("client");
            for (int i = 0; i <= master_servers_database.size(); i++) {
                if (master_servers_database[i] != NULL) {
                    if (new_elected_master == master_servers_database[i]) {
                        if (master_servers_database[i]->available == true) {
                            ip_port_out->set_ip_address(master_servers_database[i]->ip);
                            ip_port_out->set_port_num(master_servers_database[i]->port_number);
                            master_servers_database[i]->available == false;
                            //std::cout << "ROUTING SEND SERVER INFO Available master we connected to: "
//                                      << master_servers_database[i]->ip << ":" << master_servers_database[i]->port_number
//                                      << std::endl;
                            break;
                        }

                    }
                }
            }
            connected_to_master = true;
            return Status::OK;
        }
    }

    Status Timeline(ServerContext *context,
                    ServerReaderWriter<Message, Message> *stream) override
    {
        Message message;
        Client *c;
        while (stream->Read(&message))
        {
            std::string username = message.username();
            int user_index = find_user(username);
            c = &client_db[user_index];

            //Write the current message to "username.txt"
            std::string filename = username + ".txt";
            std::ofstream user_file(filename, std::ios::app | std::ios::out | std::ios::in);
            google::protobuf::Timestamp temptime = message.timestamp();
            std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
            std::string fileinput = time + " :: " + message.username() + ":" + message.msg() + "\n";
            //"Set Stream" is the default message from the client to initialize the stream
            if (message.msg() != "Set Stream")
                user_file << fileinput;
            //If message = "Set Stream", print the first 20 chats from the people you follow
            else
            {
                if (c->stream == 0)
                    c->stream = stream;
                std::string line;
                std::vector<std::string> newest_twenty;
                std::ifstream in(username + "following.txt");
                int count = 0;
                //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
                while (getline(in, line))
                {
                    if (c->following_file_size > 20)
                    {
                        if (count < c->following_file_size - 20)
                        {
                            count++;
                            continue;
                        }
                    }
                    newest_twenty.push_back(line);
                }
                Message new_msg;
                //Send the newest messages to the client to be displayed
                for (int i = 0; i < newest_twenty.size(); i++)
                {
                    new_msg.set_msg(newest_twenty[i]);
                    stream->Write(new_msg);
                }
                continue;
            }
            //Send the message to each follower's stream
            std::vector<Client *>::const_iterator it;
            for (it = c->client_followers.begin(); it != c->client_followers.end(); it++)
            {
                Client *temp_client = *it;
                if (temp_client->stream != 0 && temp_client->connected)
                    temp_client->stream->Write(message);
                //For each of the current user's followers, put the message in their following.txt file
                std::string temp_username = temp_client->username;
                std::string temp_file = temp_username + "following.txt";
                std::ofstream following_file(temp_file, std::ios::app | std::ios::out | std::ios::in);
                following_file << fileinput;
                temp_client->following_file_size++;
                std::ofstream user_file(temp_username + ".txt", std::ios::app | std::ios::out | std::ios::in);
                user_file << fileinput;
            }
        }
        //If the client disconnected from Chat Mode, set connected to false
        c->connected = false;
        return Status::OK;
    }

    Status Heartbeat(ServerContext *context,
                     ServerReaderWriter<Message, Message> *stream) override
    {
        Message message;
        stream->Read(&message);
        std::string ip_address = message.ip_address();
        std::string port_num = message.port_num();
        std::string master_address = ip_address + ":" + port_num;
        if (message.username() == "master")
        {
            Message message_2;
            while (stream->Read(&message_2))
            { // routing will receive this message from master asking if still alive
                if (message_2.msg() == "Still Alive?")
                {
                    Message new_msg;
                    new_msg.set_msg("Master Alive");
                    stream->Write(new_msg);
                }
            }
            //std::cout << "ROUTING: MASTER CONNECTION IS GONE" << std::endl;

            bool deleted_crash = false;
            bool elected_new = false;
            for (auto iter = master_servers_database.begin(); iter != master_servers_database.end(); ++iter) {
                //THE ERROR HAPPENS HERE i need to do check if not null
                if ((*iter) != NULL)
                {
                    std::string address = (*iter)->ip + ":" + (*iter)->port_number;
                    if (address == master_address)
                    {
                        //master_servers_database.erase(iter);
                        *iter = NULL;
                        delete (*iter);
                        deleted_crash = true;
                    }
                    else if ((address != master_address) and ((*iter)->available == true )) {
                        new_elected_master = ((*iter));
                        elected_new = true;
                    }
                    if (elected_new and deleted_crash){
                        break;
                    }

                }
            }
            //std::cout << "AFTER DELETING this is our newly elected master "  << new_elected_master << std::endl;
            int indexer = 0;
            for (auto iter = master_servers_database.begin(); iter != master_servers_database.end(); ++iter)
            {
                std::cout << "[" << indexer << "] " << *iter << std::endl;
                indexer++;
            }
        }
        return Status::OK;
    }
};

void AddMastertoRouting(const std::string &ip_address, const std::string &port_num)
{
    ClientContext context;
    IPPortInfo ipPortInfo;
    Reply reply;
    ipPortInfo.set_ip_address(ip_address);
    ipPortInfo.set_port_num(port_num);

    stub_original_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
        grpc::CreateChannel(
            routing_login_info, grpc::InsecureChannelCredentials())));
    stub_original_->LoginServer(&context, ipPortInfo, &reply);
}
void *RunServer(void *arguments)
{
    struct Argument *args = (struct Argument *)arguments;
    std::string server_type = args->arg_server_type;
    std::string server_ip = args->arg_ip;
    std::string port_no = args->arg_port_number;

    std::string server_address;
    if (server_type == "routing")
    {
        server_address = server_ip + ":" + port_no;
    }
    else if (server_type == "master")
    {
        //TODO: if it is the master then need to tell master that this was added so we can add to the database
        AddMastertoRouting(server_ip, port_no);
        server_address = server_ip + ":" + port_no;
    }

    SNSServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

// Client functionality for pinging
// Master and Slave communication

// the master runs this
void *ClientandServer::HeartbeatThread(void *arguments)
{

    ClientContext context;
    struct Argument *args = (struct Argument *)arguments;
    std::string ip_address = args->arg_ip;
    std::string port_num = args->arg_port_number;
    std::string server = args->arg_server_type;
    // "login" to the routing server
    //    std::string login_info = ip_address + ":" + port_num;
    ClientandServer::stub_original_2_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
        grpc::CreateChannel(
            routing_login_info, grpc::InsecureChannelCredentials())));
    std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
        stub_original_2_->Heartbeat(&context));
    //std::cout << "MASTER: We called the hearbeat function to the master " << routing_login_info << std::endl;

    std::string input = "Still Alive?"; // Master tells the routing that it is still alive.
    Message m = MakeMessage(ip_address, port_num, input, server);
    stream->Write(m);
    while (1)
    {
        stream->Write(m);
    }
}

void *ClientandServer::MasterSlaveThread(void *arguments)
{

    ClientContext context;
    struct Argument *args = (struct Argument *)arguments;
    std::string ip_address = args->arg_ip;
    std::string port_num = args->arg_port_number;
    std::string server = args->arg_server_type;

    int port_int = std::stoi(port_num);
    port_int = port_int + 1;
    std::string pp = std::to_string(port_int);

    // "login" to the routing server
    std::string login_info = ip_address + ":" + pp;
    ClientandServer::stub_2 = std::unique_ptr<SNSSlaveService::Stub>(SNSSlaveService::NewStub(
        grpc::CreateChannel(
            login_info, grpc::InsecureChannelCredentials())));
    std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
        stub_2->MasterSlave(&context));
    //std::cout << "MASTER: We called the masterslave function to the slave " << login_info << std::endl;

    /******************** RESTART SLAVE COMMAND ********************/
    //int port_num_int = std::stoi(port_num);
    //port_num_int = +1;
    //std::string p = std::to_string(port_num_int);
    std::string cmd_torun = "./tsdslave -p ";
    cmd_torun.append(pp);
    cmd_torun.append(" -i ");
    cmd_torun.append(ip_address);
    char buf[256];
    memset(buf, 0, sizeof(buf));
    strcpy(buf, cmd_torun.c_str());
    /***************************************************************/

    //Thread used to read chat messages and send them to the server
    std::thread writer([ip_address, port_num, server, stream]() {
        std::string input = "Master Alive"; // Master tells slave it's alive
        Message m = MakeMessage(ip_address, port_num, input, server);
        stream->Write(m);
        while (1)
        {
            stream->Write(m);
        }
        stream->WritesDone();
    });

    std::thread reader([buf, stream]() {
        Message m;
        while (stream->Read(&m))
        {
            if (m.msg() != "Slave Alive") // Master receives Slave's status, if dead it resurrects it w system call
            {
                // restart slave
                //TODO: make sure this works
                std::cout << "MASTER: command to run to resurrect my slave partner " << buf << std::endl;
                system(buf);
            }
        }
        //std::cout << "MASTER: command to run to resurrect my slave partner " << buf << std::endl;
        system(buf);
    });

    //Wait for the threads to finish
    writer.join();
    reader.join();
}

int main(int argc, char **argv)
{

    int opt = 0;
    while ((opt = getopt(argc, argv, "p:i:s:r:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = optarg;
            break;
        case 'i':
            ip = optarg;
            break;
        case 's':
            server_type = optarg;
            break;
        case 'r':
            routing_login_info = optarg;
            break;
        default:
            std::cerr << "Invalid Command Line Argument\n";
        }
    }

    pthread_t pt;
    pthread_t slave_thread;
    if (server_type == "routing")
    {
        ClientandServer *clientandServer_routing_temp = new ClientandServer(ip, server_type, port);
        clientandServer_routing = clientandServer_routing_temp;
        //std::cout << "MASTER: this is the routing with the routing pointer " << clientandServer_routing << std::endl;
    }
    else
    { // else it is a master
        struct Argument args;
        args.arg_port_number = port;
        args.arg_ip = ip;
        pthread_create(&pt, NULL, clientandServer_routing->HeartbeatThread, (void *)&args);
        pthread_create(&slave_thread, NULL, clientandServer_slave->MasterSlaveThread, (void *)&args);
    }

    pthread_t run_sever_thread;
    struct Argument args;
    args.arg_port_number = port;
    args.arg_ip = ip;
    args.arg_server_type = server_type;
    pthread_create(&run_sever_thread, NULL, &RunServer, (void *)&args);
    pthread_join(run_sever_thread, NULL); /* Wait until thread is finished */
    pthread_join(pt, NULL);
    pthread_join(slave_thread, NULL);

    //RunServer(port, server_type, ip);

    return 0;
}
