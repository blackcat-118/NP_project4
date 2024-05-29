#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <cstddef>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <boost/asio.hpp>
#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>

using namespace std;
vector<int> pid_pool;
string service_name;
boost::asio::io_context io_context;
unsigned int port_counter = 0;
unsigned int port_base = 21235;

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::io_context& io_context, boost::asio::ip::tcp::socket socket): resolver_(io_context), server_socket_(io_context), 
        client_socket_(std::move(socket)), acceptor_(io_context) {}

    void start() {
        memset(read_from_client, '\0', max_length);
        memset(read_from_server, '\0', max_length);
        read_request();
    }

private:
    unsigned int vn;
    unsigned int cd;
    unsigned int dst_port;
    bool accept = false;
    string dst_ip = "";
    string userid = "";
    string domain_name = "";
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::resolver::results_type endpoint_;
    boost::asio::ip::tcp::socket server_socket_;
    boost::asio::ip::tcp::socket client_socket_;
    boost::asio::ip::tcp::acceptor acceptor_;
    enum { max_length = 10240 };
    char read_from_client[max_length];
    char read_from_server[max_length];

    void do_resolve() {
        auto self(shared_from_this());
        boost::asio::ip::tcp::resolver::query query_(domain_name, to_string(dst_port));
        resolver_.async_resolve(query_, 
        [this, self](const boost::system::error_code &ec,
            const boost::asio::ip::tcp::resolver::results_type results){
            if(!ec){
                endpoint_ = results;
                connect_v4a();
            }else{
                client_socket_.close();
            }
        });
    }

    void do_reject() {
        unsigned char* reply = reply_generator(false, 0);
        auto self(shared_from_this());
        boost::asio::async_write(client_socket_, boost::asio::buffer(reply, 8),
            [this, self, reply](boost::system::error_code ec, std::size_t /*length*/) {
            free(reply);
        });
        client_socket_.close();
    }
    void print_info() {
        cout << "<S_IP>: " << client_socket_.remote_endpoint().address().to_string() << endl;
        cout << "<S_PORT>: " << to_string(client_socket_.remote_endpoint().port()) << endl;
        cout << "<D_IP>: " << server_socket_.remote_endpoint().address().to_string() << endl;
        cout << "<D_PORT>: " << dst_port << endl;
        if (cd == 1) {
            cout << "<Command>: CONNECT" << endl;
        }
        else {
            cout << "<Command>: BIND" << endl;
        }
        if (vn == 5) {
            cout << "<Reply>: Reject" << endl;
            cout << "-----------------------" << endl;
            return;
        }
        if (accept) {
            cout << "<Reply>: Accept" << endl;
            cout << "-----------------------" << endl;
        }
        else {
            cout << "<Reply>: Reject" << endl;
            cout << "-----------------------" << endl;
            return;
        }
    }
    void process_request() {
        // cout << vn << " " << cd << " " << dst_port << " " << dst_ip << " id: " << userid << " dom: " << domain_name << endl;
        if (vn == 5) {
            do_reject();
            print_info();
            return;
        }
        // do firewall 
        accept = firewall();
        if (accept) {
            if (cd == 1) {
                if (domain_name == "") {
                    connect_v4();
                }
                else {
                    do_resolve();
                }
            }
            else if (cd == 2) {
                bind();
            }
        }
        else {
            do_reject();
            return;
        }
    }

    void read_request() {
        auto self(shared_from_this());
        client_socket_.async_read_some(boost::asio::buffer(read_from_client, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                request_parser(read_from_client);
                process_request();
            }
        });
    }
    void client_read() {
        memset(read_from_client, '\0', max_length);
        auto self(shared_from_this());
        client_socket_.async_read_some(boost::asio::buffer(read_from_client, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                server_write(length);
            }
            else {
                if(client_socket_.is_open())
                    client_socket_.close(); 
                if(server_socket_.is_open())
                    server_socket_.close();
                // server_read();
            }
        });
    }

    void server_read() {
        memset(read_from_server, '\0', max_length);
        auto self(shared_from_this());
        server_socket_.async_read_some(boost::asio::buffer(read_from_server, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                // cout << read_from_server << endl;
                // strncpy(write_to_client, read_from_server, length);
                client_write(length);
            }
            else {
                
                // client_read();
            }
        });
    }

    void client_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(client_socket_, boost::asio::buffer(read_from_server, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // memset(write_to_client, '\0', max_length);
                server_read();
            }
            else {
                if(client_socket_.is_open())
                    client_socket_.close(); 
                if(server_socket_.is_open())
                    server_socket_.close();
            }
        });
    }

    void server_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(server_socket_, boost::asio::buffer(read_from_client, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // memset(write_to_server, '\0', max_length);
                client_read();
            }
            else {
                if(client_socket_.is_open())
                    client_socket_.close(); 
                if(server_socket_.is_open())
                    server_socket_.close();
            }
        });
    }
    bool firewall() {
        ifstream fin = ifstream("./socks.conf");
        char rule[64];
        char d_ip[64];
        strcpy(d_ip, dst_ip.data());

        memset(rule, '\0', 64);
        while (fin.getline(rule, 64)) {
            // cout << rule << endl;
            char* token = strtok(rule, " ");
            char* parsed_ip;
            if (token == NULL) {
                continue;
            }
            if (strcmp(token, "permit") != 0) {
                continue;
            }
            token = strtok(NULL, " ");
            // cout << token << endl;
            if (strcmp(token, "c") == 0 && cd != 1) {
                continue;
            }
            if (strcmp(token, "b") == 0 && cd != 2) {
                continue;
            }
            
            token = strtok(NULL, " ");
            // cout << token << endl;
            for (int i = 0; i < 4; i++) {
                token = strtok(token, ".");
                token = strtok(NULL, ".");
                if (i == 0) {
                    parsed_ip = strtok(d_ip, ".");
                }
                else {
                    parsed_ip = strtok(NULL, ".");
                }
                if (strcmp(token, "*") == 0) {
                    return true;
                }
                else if (strcmp(token, parsed_ip) != 0) {
                    break;
                }
            }
            memset(rule, '\0', 64);
        }
        return false;
    }
    void do_accept() {
        auto self(shared_from_this());
        acceptor_.async_accept(
            [this, self](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                server_socket_ = std::move(socket);
                print_info();
                int listen_port = acceptor_.local_endpoint().port();
                unsigned char* reply = reply_generator(true, listen_port);
                auto self(shared_from_this());
                boost::asio::async_write(client_socket_, boost::asio::buffer(reply, 8),
                    [this, self, reply](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        free(reply);
                        server_read();
                        client_read();
                    }
                    else {
                        if(client_socket_.is_open())
                            client_socket_.close(); 
                        if(server_socket_.is_open())
                            server_socket_.close();
                    }
                });
                do_accept();
            }
        });        
    }

    void connect_v4() {
        // cout << "connecting..." << endl;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(dst_ip), dst_port);
        // cout << endpoint.address() << endl;
        auto self(shared_from_this());
        server_socket_.async_connect(endpoint,
                              [this, self](boost::system::error_code ec) {
            if (!ec) {
                // cout << "connected" << endl;
                print_info();
                unsigned char* reply;
                reply = reply_generator(true, 0);

                auto self1(shared_from_this());
                boost::asio::async_write(client_socket_, boost::asio::buffer(reply, 8),
                                        [this, self1, reply](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        free(reply);
                        server_read();
                        client_read();
                    }
                    else {
                        if(client_socket_.is_open())
                            client_socket_.close(); 
                        if(server_socket_.is_open())
                            server_socket_.close();
                    }
                });
                
            }
            else {
                do_reject();
                return;
            }
        });
    }

    void connect_v4a() {
        // cout << "connecting..." << endl;
        // do_resolve();
        auto self(shared_from_this());
        server_socket_.async_connect(*(endpoint_.begin()), 
                              [this, self](boost::system::error_code ec) {
            if (!ec) {
                print_info();
                // cout << "connected" << endl;
                unsigned char* reply;
                reply = reply_generator(true, 0);

                auto self1(shared_from_this());
                boost::asio::async_write(client_socket_, boost::asio::buffer(reply, 8),
                                        [this, self1, reply](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        free(reply);
                        server_read();
                        client_read();
                    }
                    else {
                        if(client_socket_.is_open())
                            client_socket_.close(); 
                        if(server_socket_.is_open())
                            server_socket_.close();
                    }
                });
                
            }
            else {
                do_reject();
                return;
            }
        });
    }
    void bind() {
                
        // bind a port
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), 0);
        acceptor_.open(ep.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(ep);
        acceptor_.listen(boost::asio::socket_base::max_connections);
        unsigned int listen_port = acceptor_.local_endpoint().port();

        unsigned char* reply = reply_generator(true, listen_port);

        auto self(shared_from_this());
        boost::asio::async_write(client_socket_, boost::asio::buffer(reply, 8),
                                [this, self, reply](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                free(reply);
                do_accept();
            }
            else {
                if(client_socket_.is_open())
                    client_socket_.close(); 
                if(server_socket_.is_open())
                    server_socket_.close();
            }
        });
    }
    void request_parser(char req[1024]) {
        vn = req[0];
        cd = req[1];
        dst_port = 256 * (unsigned char)req[2] + (unsigned char)req[3];
        dst_ip = to_string((unsigned char)req[4]) + '.' + to_string((unsigned char)req[5]) + '.' + to_string((unsigned char)req[6]) + '.' + to_string((unsigned char)req[7]);
        //cout << to_string((int)req[4]) << to_string((int)req[5]) << to_string((int)req[6]) << to_string((int)req[7]) << endl;
        int i = 8;
        while (req[i] != '\0') {
            userid = userid + req[i];
            i++;
        }
        i++;
        while (req[i] != '\0') {
            domain_name = domain_name + req[i];
            i++;
        }
        return;
    }

    unsigned char* reply_generator(bool accept, unsigned int dport) {
        unsigned char* r = (unsigned char*)malloc(8);
        r[0] = 0;
        if (accept) {
            r[1] = 90;
        }
        else {
            r[1] = 91;
        }
        r[2] = dport/256;
        r[3] = dport%256;
        // ip address is 0
        for (int i = 4; i < 8; i++) {
            r[i] = 0;
        }
 
        return r;
    }
};

class server {
public:
    server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                io_context.notify_fork(boost::asio::io_context::fork_prepare);
                int childpid = fork();
                if (childpid < 0) {
                    usleep(1000);
                    childpid = fork();
                    pid_pool.push_back(childpid);
                }
                if (childpid == 0) {
                    // std::make_shared<session>(io_context)->start();
                    io_context.notify_fork(boost::asio::io_context::fork_child);
                    std::make_shared<session>(io_context, std::move(socket))->start();
                }

                else {
                    io_context.notify_fork(boost::asio::io_context::fork_parent);
                    socket.close();
                    int wstatus;
                    for (int i = 0; i < (int)pid_pool.size(); i++) {
                        if (waitpid(pid_pool[i], &wstatus, WNOHANG) > 0) {
                            pid_pool.erase(pid_pool.begin()+i);
                            i--;
                        }
                    }
                    do_accept();
                }
            }
        });
    }

    boost::asio::ip::tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_socks_server <port>\n";
            return 1;
        }

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
