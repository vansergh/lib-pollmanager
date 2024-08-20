#include <string>
#include <iostream>
#include <stdexcept>
#include <pollmanager/manager/poll.hpp>

using namespace std;
using namespace vsock;


static std::mutex cout_mtx;
static std::mutex socket_mtx;

void InitWinsock() {
    //----------------------
    // Initialize Winsock.
    #ifdef _WIN32
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"WSAStartup failed with error: %ld\n", iResult);
    }
    #endif
}

void ReleaseWinsock() {
    #ifdef _WIN32
    WSACleanup();
    #endif    
}

SocketID Connect(int port) {
    //----------------------
        // Create a SOCKET for connecting to server
    SocketID ConnectSocket;
    ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == VSOCK_INVALID_SOCKET) {
        #ifdef _WIN32
        throw std::runtime_error(std::to_string(WSAGetLastError()));
        #endif        
        return VSOCK_INVALID_SOCKET;
    }
    //----------------------
    // The sockaddr_in structure specifies the address family,
    // IP address, and port of the server to be connected to.
    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientService.sin_port = htons(port);

    //----------------------
    // Connect to server.

    int iResult = connect(ConnectSocket, (sockaddr*)&clientService, sizeof(clientService));
    if (iResult == VSOCK_SOCKET_ERROR) {
        #ifdef _WIN32
        throw std::runtime_error(std::to_string(WSAGetLastError()));
        #endif
        return VSOCK_INVALID_SOCKET;
    }
    return ConnectSocket;
}

SocketID Listen(int port, int backlog) {
    //----------------------
    // Create a SOCKET for listening for
    // incoming connection requests.
    SocketID ListenSocket;
    ListenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == VSOCK_INVALID_SOCKET) {
        #ifdef _WIN32
        wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        #endif
        return VSOCK_INVALID_SOCKET;
    }

    int reuse_opt_val = 1;
    #ifdef _WIN32
    int result = ::setsockopt(ListenSocket, SOL_SOCKET, VSOCK_REBIND_OPTION, reinterpret_cast<const char*>(&reuse_opt_val), sizeof(int));
    #else
    int result = ::setsockopt(ListenSocket, SOL_SOCKET, VSOCK_REBIND_OPTION, reinterpret_cast<const void*>(&reuse_opt_val), sizeof(int));
    #endif
    if (result == VSOCK_SOCKET_ERROR) {
        #ifdef _WIN32
        wprintf(L"reuse failed with error: %ld\n", WSAGetLastError());
        #endif
        return VSOCK_INVALID_SOCKET;
    }
    //----------------------
    // The sockaddr_in structure specifies the address family,
    // IP address, and port for the socket that is being bound.
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &service.sin_addr);

    int res = ::bind(ListenSocket, (sockaddr*)&service, sizeof(service));
    if (res == VSOCK_SOCKET_ERROR) {
        #ifdef _WIN32
        wprintf(L"bind failed with error: %ld\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        #endif
        return VSOCK_INVALID_SOCKET;
    }
    //----------------------
    // Listen for incoming connection requests.
    // on the created socket
    if (::listen(ListenSocket, backlog) == VSOCK_SOCKET_ERROR) {
        #ifdef _WIN32
        wprintf(L"listen failed with error: %ld\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        #endif
        return VSOCK_INVALID_SOCKET;
    }
    return ListenSocket;
}



int main() {

    InitWinsock();
    cout << "\n=========================================================================\n"s;

    {
        ThreadPool pool(32);
        PollManager* poll = new PollManager(&pool);
        int port{ 8080 };
        int backlog{ 4096 };
        int clients_count{ 150 };
        int incoming{ 0 };
        int outgoing{ 0 };

        pool.AddAsyncTask([&]() {

            SocketID listen_socket = Listen(port, backlog);

            poll->Add(listen_socket, (EPOLLIN | EPOLLONESHOT), [&](const SocketID socket_id) {
                SocketID client_id = ::accept(socket_id, 0, 0);
                cout_mtx.lock();
                ++incoming;
                std::cout << "<Server> Client #" << incoming << " connected at Socket[" << client_id << "]\n";
                cout_mtx.unlock();
                poll->ResetFlags(socket_id);
            });

            int sleep_time = 1;
            cout_mtx.lock();
            cout << "<Server> Listen socket [" << listen_socket << "] at port [" << (port) << "]\n";
            cout << "Sleeping " << sleep_time << " sec and stop...\n";
            cout_mtx.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
            cout_mtx.lock();
            cout << "Incoming: " << incoming << "\n";
            cout << "Outgoing: " << outgoing << "\n";
            cout << "Waked up and stoping...\n";
            cout_mtx.unlock();
            delete poll;
            cout_mtx.lock();
            cout << "Stoped!" << std::endl;
            cout_mtx.unlock();
        });

        for (int k = 0; k < 2; ++k) {
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            for (int i = 0; i < clients_count; ++i) {

            std::unique_ptr<Task> connect_task = std::make_unique<Task>();
            connect_task->vars.Add((SocketID)VSOCK_INVALID_SOCKET);
            connect_task->vars.Add(port);
            connect_task->vars.Add(std::ref(outgoing));
            connect_task->vars.Add(i);
            connect_task->SetLoopJob([&](Task& task) {
                SocketID& client_socket_id = task.vars.Get<SocketID>(0);
                const int port = task.vars.Get<int>(1);
                client_socket_id = Connect(port);
            }, std::ref(*connect_task));
            connect_task->SetCondition([&](Task& task) {
                const SocketID client_socket_id = task.vars.Get<SocketID>(0);
                if (client_socket_id == VSOCK_INVALID_SOCKET) {
                    return true;
                }
                const int port = task.vars.Get<int>(1);
                int& outgoing = task.vars.Get<std::reference_wrapper<int>>(2);
                int index = task.vars.Get<int>(3);
                cout_mtx.lock();
                ++outgoing;
                cout << "<Client#" << index << "> Connected at port [" << std::to_string(port) << "], Socket ID: " << client_socket_id << std::endl;
                cout_mtx.unlock();
                return false;
            }, std::ref(*connect_task));
            pool.AddAsyncTask(std::move(connect_task));

        }
    }

        cout << "Done!" << endl;
    }
    ReleaseWinsock();

    return 0;
}
