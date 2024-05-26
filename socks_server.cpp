#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
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

void Parser(string http_req) {
    vector<string> parsed_req;

    boost::split(parsed_req, http_req, boost::is_any_of(" \r\n"), boost::token_compress_on);

    // env_vars[0].second = parsed_req[0];
    // env_vars[1].second = parsed_req[1];
    size_t indx = parsed_req[1].find_first_of('?');
    string query = "";
    if (indx != string::npos) {
        service_name = parsed_req[1].substr(1, indx-1);
        query = parsed_req[1].substr(indx+1);
    }
    else {
        service_name = parsed_req[1].substr(1);
        //cout << service_name << endl;
    }
    // env_vars[2].second = query;
    // env_vars[3].second = parsed_req[2];
    // env_vars[4].second = parsed_req[4];

    return;
}

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::io_context& io_context, boost::asio::ip::tcp::socket socket): resolver_(io_context), server_socket_(io_context), client_socket_(std::move(socket)) {}

    void start() {
        // set env
        env_vars.push_back(pair<string, string>("REQUEST_METHOD", ""));
        env_vars.push_back(pair<string, string>("REQUEST_URI", ""));
        env_vars.push_back(pair<string, string>("QUERY_STRING", ""));
        env_vars.push_back(pair<string, string>("SERVER_PROTOCOL", ""));
        env_vars.push_back(pair<string, string>("HTTP_HOST", ""));
        env_vars.push_back(pair<string, string>("SERVER_ADDR", ""));
        env_vars.push_back(pair<string, string>("SERVER_PORT", ""));
        env_vars.push_back(pair<string, string>("REMOTE_ADDR", ""));
        env_vars.push_back(pair<string, string>("REMOTE_PORT", ""));
        memset(read_data_, '\0', max_length);
        read_request();
    }

private:
    unsigned int vn;
    unsigned int cd;
    unsigned int dst_port;
    string dst_ip = "";
    string userid = "";
    string domain_name = "";
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::resolver::results_type endpoint_;
    boost::asio::ip::tcp::socket server_socket_;
    boost::asio::ip::tcp::socket client_socket_;
    enum { max_length = 1024 };
    char read_data_[max_length];
    char write_data_[max_length];
    vector<pair<string, string>> env_vars;

    void do_resolve(boost::asio::ip::tcp::resolver::query query_) {
        
        resolver_.async_resolve(query_, 
        [this](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type result) {
            endpoint_ = result;
            if (!ec) {
                connect_v4a();                
            }
            else {
                client_socket_.close();
            }
        });
    }
    void read_request() {
        auto self(shared_from_this());
        client_socket_.async_read_some(boost::asio::buffer(read_data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                // Parser(string(data_));
                // env_vars[5].second = socket_.local_endpoint().address().to_string();
                // env_vars[6].second = to_string(socket_.local_endpoint().port());
                // env_vars[7].second = socket_.remote_endpoint().address().to_string();
                // env_vars[8].second = to_string(socket_.remote_endpoint().port());
                request_parser(read_data_);
                cout << "<S_IP>: " << endl;
                cout << "<S_PORT>: " << endl;
                cout << "<D_IP>: " << dst_ip << endl;
                cout << "<D_PORT>: " << dst_port << endl;
                cout << flush;
                cout << vn << " " << cd << " " << dst_port << " " << dst_ip << " id: " << userid << " dom: " << domain_name << endl;
                if (vn == 5) {
                    cout << "<Reply>: Reject" << endl;
                    client_socket_.close();
                    return;
                }
                // connect
                if (cd == 1) {
                    cout << "<Command>: CONNECT" << endl;
                    cout << "<Reply>: Accept" << endl;
                    string reply = reply_generator();
                    strncpy(write_data_, reply.data(), 8);
                    auto self1(shared_from_this());
                    boost::asio::async_write(client_socket_, boost::asio::buffer(write_data_, 8),
                                            [this, self1](boost::system::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                            if (domain_name == "") {
                                connect_v4();
                            }
                            else {
                                boost::asio::ip::tcp::resolver::query query_(domain_name, to_string(dst_port));
                                do_resolve(query_);
                                
                            }
                        }
                        else {
                            cout << "error" << endl;
                        }
                    });
                    
                    // client_read();
                    
                    // client_write(reply.size());
                    
                }
                //   int childpid = fork();
                //   if (childpid == 0) {
                //     for (int i = 0; i < 9; i++) {
                //       setenv(env_vars[i].first.data(), env_vars[i].second.data(), 1);
                //     }
                //     dup2(socket_.native_handle(), STDIN_FILENO);
                //     dup2(socket_.native_handle(), STDOUT_FILENO);
                //     dup2(socket_.native_handle(), STDERR_FILENO);

                //     cout << env_vars[3].second << " 200 OK\r\n" << flush;

                //     char sn[20] = {};
                //     strcpy(sn, service_name.data());
                //     char* argv[5] = {sn, NULL};

                //     if (execv(service_name.data(), argv) < 0) {
                //       cerr << "error: " << strerror(errno) << endl;
                //       exit(1);
                //     }
                //     exit(0);

                //   }
                //   else {
                //     socket_.close();
                //     int wstatus;
                //     waitpid(childpid, &wstatus, 0);
                //   }
                //   //do_write(length);
            }
        });
    }
    void client_read() {
        cout << "client read " << endl;
        auto self(shared_from_this());
        client_socket_.async_read_some(boost::asio::buffer(read_data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                // cout << read_data_ << endl;
                strcpy(write_data_, read_data_);
                server_write(strlen(write_data_));
            }
        });
    }

    void server_read() {
        auto self(shared_from_this());
        server_socket_.async_read_some(boost::asio::buffer(read_data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                // cout << read_data_ << endl;
                strcpy(write_data_, read_data_);
                client_write(strlen(write_data_));
            }
        });
    }

    void client_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(client_socket_, boost::asio::buffer(write_data_, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                client_read();
            }
        });
    }
    void server_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(server_socket_, boost::asio::buffer(write_data_, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                server_read();
            }
        });
    }
    void connect_v4() {
        cout << "connecting..." << endl;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(dst_ip), dst_port);
        cout << endpoint.address() << endl;
        auto self(shared_from_this());
        server_socket_.async_connect(endpoint,
                              [this, self](boost::system::error_code ec) {
            if (!ec) {
                cout << "connected" << endl;
                // if (file != ""){
                //     file = "./test_case/" + file;
                //     fin = ifstream(file.data());
                // }
                // memset(data_, '\0', 20480);
                server_read();
            }
            else {
                // fin.close();
                client_socket_.close();
            }
        });
    }

    void connect_v4a() {
        cout << "connecting..." << endl;
        auto self(shared_from_this());
        server_socket_.async_connect(*(endpoint_.begin()),
                              [this, self](boost::system::error_code ec) {
            if (!ec) {
                cout << "connected" << endl;
                // if (file != ""){
                //     file = "./test_case/" + file;
                //     fin = ifstream(file.data());
                // }
                // memset(data_, '\0', 20480);
                client_read();
            }
            else {
                // fin.close();
                client_socket_.close();
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
        // i++;
        // while (req[i] != '\0') {
        //     domain_name = domain_name + req[i];
        //     i++;
        // }
        return;
    }

    string reply_generator() {
        cout << string('\0'+90+"\0\0\0\0\0\0") << endl;
        return string('\0'+90+"\0\0\0\0\0\0");
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
