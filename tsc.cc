#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/types.h>

#include <grpc++/grpc++.h>
#include "client.h"

#include "sns.grpc.pb.h"
using csce438::IPPortInfo;
using csce438::ListReply;
using csce438::Message;
using csce438::Reply;
using csce438::Request;
using csce438::SNSService;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

Message MakeMessage(const std::string &username, const std::string &msg)
{
    Message m;
    m.set_username(username);
    std::cout << "CLIENT MAKE MSG: " << username << std::endl;
    m.set_msg(msg);
    google::protobuf::Timestamp *timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    m.set_allocated_timestamp(timestamp);
    return m;
}

struct Argument
{
    std::string arg_ip;
    std::string arg_port_number;
    std::string arg_username;
};

class Client : public IClient
{
public:
    Client(const std::string &hname,
           const std::string &uname,
           const std::string &p)
        : hostname(hname), username(uname), port(p)
    {
    }
    static void *HeartbeatThread(void *arguments);
    static std::unique_ptr<SNSService::Stub> stub_original_2_;
    static std::unique_ptr<SNSService::Stub> stub_master_;

    static void SendServerInfoMediator();
    static void LoginMediator();
    static std::string hostname_static;
    static std::string username_static;
    static std::string port_static;
    std::string master_connected_to_login;

protected:
    virtual int connectTo();
    virtual IReply processCommand(std::string &input);
    virtual void processTimeline();

private:
    std::string hostname;
    std::string username;
    std::string port;
    // You can have an instance of the client stub
    // as a member variable.
    std::unique_ptr<SNSService::Stub> stub_routing_;
    std::unique_ptr<SNSService::Stub> stub_;

    IReply Login();
    IReply List();
    IReply Follow(const std::string &username2);
    IReply UnFollow(const std::string &username2);
    void SendServerInfo(const std::string &username, const std::string &to_username, const std::string &ip_address, const std::string &port_num);
    void Timeline(const std::string &username);
};

/************************** GLOBAL VARIABLES  **************************/
std::unique_ptr<SNSService::Stub> Client::stub_original_2_(nullptr);
std::unique_ptr<SNSService::Stub> Client::stub_master_(nullptr);

std::string Client::hostname_static;
std::string Client::username_static;
std::string Client::port_static;
Client *myc = NULL;
bool need_to_reconnect = false;
/***********************************************************************/

int main(int argc, char **argv)
{

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            hostname = optarg;
            break;
        case 'u':
            username = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        default:
            std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client::hostname_static = hostname;
    Client::port_static = port;
    Client::username_static = username;
    myc = new Client(hostname, username, port);

    myc->run_client();
    //pthread_join(pt, NULL);
    //    struct Argument args_client;
    //    pthread_t client_thread;
    //    pthread_create( &client_thread, NULL, myc.run_client, (void *)&args_client );
    //std::thread client_run_thread(run_client, NULL);
    //pthread_join(client_thread, NULL);
    //    client_run_thread.join();
    return 0;
}

int Client::connectTo()
{
    // ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
    // ------------------------------------------------------------
    // we first need to give hostname (10.0.2.15) and port number
    // (we want the routing server port number)
    // we should have 9 port numbers
    //std::string login_info = hostname + ":" + port;
    SendServerInfo(username, "routing_server", hostname, port);

    //stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
    //        grpc::CreateChannel(
    //                login_info, grpc::InsecureChannelCredentials())));

    IReply ire = Login();
    std::cout << "CLIENT after calling login() " << std::endl;
    if (!ire.grpc_status.ok())
    {
        return -1;
    }
    return 1;
}

void Client::SendServerInfoMediator()
{
    myc->SendServerInfo(username_static, "routing_server", hostname_static, port_static);
}

void Client::LoginMediator()
{
    myc->Login();
}
IReply Client::processCommand(std::string &input)
{
    // ------------------------------------------------------------
    // GUIDE 1:
    // In this function, you are supposed to parse the given input
    // command and create your own message so that you call an
    // appropriate service method. The input command will be one
    // of the followings:
    //
    // FOLLOW <username>
    // UNFOLLOW <username>
    // LIST
    // TIMELINE
    //
    // - JOIN/LEAVE and "<username>" are separated by one space.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // GUIDE 2:
    // Then, you should create a variable of IReply structure
    // provided by the client.h and initialize it according to
    // the result. Finally you can finish this function by returning
    // the IReply.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Join" service method for JOIN command,
    // IReply can be set as follow:
    //
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Join(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //
    //      return ire;
    //
    // IMPORTANT:
    // For the command "LIST", you should set both "all_users" and
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------
    IReply ire;
    std::size_t index = input.find_first_of(" ");
    if (index != std::string::npos)
    {
        std::string cmd = input.substr(0, index);

        /*
        if (input.length() == index + 1) {
            std::cout << "Invalid Input -- No Arguments Given\n";
        }
        */

        std::string argument = input.substr(index + 1, (input.length() - index));

        if (cmd == "FOLLOW")
        {
            return Follow(argument);
        }
        else if (cmd == "UNFOLLOW")
        {
            return UnFollow(argument);
        }
    }
    else
    {
        if (input == "LIST")
        {
            ire = List();
            if (!(ire.grpc_status.ok()))
            {
                std::cout << "LIST FAILED we need to do something about this " << std::endl;
                need_to_reconnect = true;
            }
            return ire;
        }
        else if (input == "TIMELINE")
        {
            ire.comm_status = SUCCESS;
            return ire;
        }

        else if (input == "LOGIN")
        {
            ire.comm_status = SUCCESS;
            return ire;
        }
    }

    ire.comm_status = FAILURE_INVALID;
    return ire;
}

void Client::processTimeline()
{
    Timeline(username);
    // ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
    // ------------------------------------------------------------
}

IReply Client::List()
{
    //Data being sent to the server
    Request request;
    request.set_username(username);

    //Container for the data from the server
    ListReply list_reply;

    //Context for the client
    ClientContext context;

    if (need_to_reconnect)
    {
        SendServerInfo(username, "routing_server_reconnect", hostname, port);
        IReply ire = Login();
        need_to_reconnect = false;
    }
    Status status = stub_->List(&context, request, &list_reply);
    IReply ire;
    ire.grpc_status = status;
    //Loop through list_reply.all_users and list_reply.following_users
    //Print out the name of each room
    if (status.ok())
    {
        ire.comm_status = SUCCESS;
        std::string all_users;
        std::string following_users;
        for (std::string s : list_reply.all_users())
        {
            ire.all_users.push_back(s);
        }
        for (std::string s : list_reply.followers())
        {
            ire.followers.push_back(s);
        }
    }

    return ire;
}

IReply Client::Follow(const std::string &username2)
{
    Request request;
    request.set_username(username);
    request.add_arguments(username2);

    Reply reply;
    ClientContext context;

    Status status = stub_->Follow(&context, request, &reply);
    IReply ire;
    ire.grpc_status = status;
    if (reply.msg() == "Follow Failed -- Invalid Username")
    {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    }
    else if (reply.msg() == "Follow Failed -- Invalid Username")
    {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    }
    else if (reply.msg() == "Follow Failed -- Already Following User")
    {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
    }
    else if (reply.msg() == "Follow Successful")
    {
        ire.comm_status = SUCCESS;
    }
    else
    {
        ire.comm_status = FAILURE_UNKNOWN;
    }
    return ire;
}

IReply Client::UnFollow(const std::string &username2)
{
    Request request;

    request.set_username(username);
    request.add_arguments(username2);

    Reply reply;

    ClientContext context;

    Status status = stub_->UnFollow(&context, request, &reply);
    IReply ire;
    ire.grpc_status = status;
    if (reply.msg() == "UnFollow Failed -- Invalid Username")
    {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    }
    else if (reply.msg() == "UnFollow Failed -- Invalid Username")
    {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    }
    else if (reply.msg() == "UnFollow Successful")
    {
        ire.comm_status = SUCCESS;
    }
    else
    {
        ire.comm_status = FAILURE_UNKNOWN;
    }

    return ire;
}

IReply Client::Login()
{
    Request request;
    request.set_username(username);
    Reply reply;
    ClientContext context;

    Status status = stub_->Login(&context, request, &reply);

    IReply ire;
    ire.grpc_status = status;
    if (reply.msg() == "you have already joined")
    {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
    }
    else
    {
        ire.comm_status = SUCCESS;
    }
    return ire;
}

void Client::SendServerInfo(const std::string &username, const std::string &to_username, const std::string &ip_address, const std::string &port_num)
{
    // send first to the routing send that info back to the client and then send client to the master

    // if want to go back to routing or to an available master
    IPPortInfo ip_port_info_in;

    ip_port_info_in.set_from_user(username);
    ip_port_info_in.set_to_user(to_username);
    ip_port_info_in.set_ip_address(ip_address);
    ip_port_info_in.set_port_num(port_num);

    IPPortInfo ip_port_info_out;

    ClientContext context;
    bool first_ask = false;

    // from client to routing server
    if (to_username == "routing_server")
    {

        std::string login_info_routing = hostname + ":" + port;
        stub_routing_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
            grpc::CreateChannel(
                login_info_routing, grpc::InsecureChannelCredentials())));
        Status status = stub_routing_->SendServerInfo(&context, ip_port_info_in, &ip_port_info_out);
        std::cout << "CLIENT: GOT THE CREDS" << std::endl;

        // getting the information of the available master
        std::string login_info_available_master = ip_port_info_out.ip_address() + ":" + ip_port_info_out.port_num();
        master_connected_to_login = login_info_available_master;
        stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
            grpc::CreateChannel(
                login_info_available_master, grpc::InsecureChannelCredentials())));
    }

    if (to_username == "routing_server_reconnect")
    {

        std::string login_info_routing = hostname + ":" + port;
        stub_routing_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
            grpc::CreateChannel(
                login_info_routing, grpc::InsecureChannelCredentials())));
        Status status = stub_routing_->SendServerInfo(&context, ip_port_info_in, &ip_port_info_out);
        std::cout << "CLIENT: GOT THE CREDS AFTER CRASH" << std::endl;

        // getting the information of the available master
        std::string login_info_available_master = ip_port_info_out.ip_address() + ":" + ip_port_info_out.port_num();
        master_connected_to_login = login_info_available_master;
        stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
            grpc::CreateChannel(
                login_info_available_master, grpc::InsecureChannelCredentials())));
    }
}

void Client::Timeline(const std::string &username)
{
    ClientContext context;

    std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
        stub_->Timeline(&context));

    //Thread used to read chat messages and send them to the server
    std::thread writer([username, stream]() {
        std::string input = "Set Stream";
        Message m = MakeMessage(username, input);
        stream->Write(m);
        while (1)
        {
            input = getPostMessage();
            m = MakeMessage(username, input);
            stream->Write(m);
        }
        stream->WritesDone();
    });

    std::thread reader([username, stream]() {
        Message m;
        while (stream->Read(&m))
        {

            google::protobuf::Timestamp temptime = m.timestamp();
            std::time_t time = temptime.seconds();
            displayPostMessage(m.username(), m.msg(), time);
        }
    });

    //Wait for the threads to finish
    writer.join();
    reader.join();
}

/************************************************SCRAP CODE MIGHT BE USEFUL*********************************************/

//this is handling the redirection to another available master
//    if (first_ask) {
//        while (1){
//            IPPortInfo ip_port_info_in_2;
//            ip_port_info_in_2.set_from_user(username);
//            ip_port_info_in_2.set_to_user("routing_server_2");
//            ip_port_info_in_2.set_ip_address(ip_address);
//            ip_port_info_in_2.set_port_num(port_num);
//
//            IPPortInfo ip_port_info_out_2;
//            Status status = stub_routing_->SendServerInfo(&context, ip_port_info_in_2, &ip_port_info_out_2);
//
//            std::string login_info_available_master_2 = ip_port_info_out.ip_address() + ":" + ip_port_info_out.port_num();
//            stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
//                    grpc::CreateChannel(
//                            login_info_available_master_2, grpc::InsecureChannelCredentials())));
//        }
//    }

// You MUST invoke "run_client" function to start business logic
//    struct Argument args;
//    pthread_t pt;
//    args.arg_port_number = port;
//    args.arg_ip = hostname;
//    args.arg_username = username;
//    std::cout << "CLIENT: " << args.arg_username  << std::endl;
//    pthread_create( &pt, NULL, myc->HeartbeatThread, (void*)&args );
/*
 * void* Client::HeartbeatThread(void *arguments) {

    ClientContext context;
    struct Argument *args = (struct Argument *) arguments;
    std::string ip_address = args->arg_ip;
    std::string port_num = args->arg_port_number;
    std::string usr = args->arg_username;

    std::cout << "CLIENT HEARTBEAT FUNCTION: " << usr  << std::endl;
    std::string login_info = "10.0.2.5:3012";
    Client::stub_master_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
            grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
    std::shared_ptr <ClientReaderWriter<Message, Message>> stream(
            stub_master_->Heartbeat(&context));
    Message m = MakeMessage("u1", "Hi");
    stream->Write(m);
/*
    std::thread writer([usr, stream]() {
        std::string input = "Still Alive?"; // Router ask master if it's alive
        Message m = MakeMessage(usr, input);
        stream->Write(m);
        while (1) {
            stream->Write(m);
        }
    });

    std::thread reader([stream]() {
        Message m;
        Message m_initial;
        stream->Read(&m_initial);
        std::string master_ip = m_initial.ip_address();
        std::string master_port = m_initial.port_num();
        while (stream->Read(&m))
        {
            std::cout << "CLIENT: I AM READING  " << m.msg()<< std::endl;
            if (m.msg() == "Elect New Master") // if master is not alive then router updated the availability
            {
                std::cout << "THE CLIENTS MASTER DIED!!!!! "<< std::endl;
                // need to update the master server database availability
                // appoint a new available master
                Client::SendServerInfoMediator();
                Client::LoginMediator();
            }
        }
    });
    writer.join();
    reader.join();*/

//void* Client::HeartbeatThread(void *arguments) {
//
//    ClientContext context;
//    struct Argument *args = (struct Argument *) arguments;
//    std::string ip_address = args->arg_ip;
//    std::string port_num = args->arg_port_number;
//    std::string usr = args->arg_username;
//
//    std::cout << "CLIENT HEART: " << usr  << std::endl;
//
//    // "login" to the master
//    std::string login_info = "10.0.2.15:3010";
//    Client::stub_original_2_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
//            grpc::CreateChannel(
//                    login_info, grpc::InsecureChannelCredentials())));
//    std::shared_ptr <ClientReaderWriter<Message, Message>> stream(
//            stub_original_2_->Heartbeat(&context));
//    std::cout << "CLIENT : we are now in the heartbeat function AFTER STUB CALL " << std::endl;
//
////    Thread used to read chat messages and send them to the server
//    std::thread writer([usr, stream]() {
//        std::string input = "Still Alive?"; // Router ask master if it's alive
//        Message m = MakeMessage(usr, input);
//        stream->Write(m);
//        while (1) {
//            stream->Write(m);
//        }
//    });
//
//        std::thread reader([stream]() {
//        Message m;
//        Message m_initial;
//        stream->Read(&m_initial);
//        std::string master_ip = m_initial.ip_address();
//        std::string master_port = m_initial.port_num();
//        while (stream->Read(&m))
//        {
//            std::cout << "CLIENT: I AM READING  " << m.msg()<< std::endl;
//            /*******WE NEED TO RUN SEND SERVER INFO AGAIN********/
//            if (m.msg() == "Elect New Master") // if master is not alive then router updated the availability
//            {
//                std::cout << "THE CLIENTS MASTER DIED!!!!! "<< std::endl;
//                // need to update the master server database availability
//                // appoint a new available master
//                Client::SendServerInfoMediator();
//                Client::LoginMediator();
//            }
//            }
//        });
//    writer.join();
//    reader.join();
//}
