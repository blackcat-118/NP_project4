#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <boost/asio.hpp>
#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>

using namespace std;

void print() {
    cout << "<script>document.getElementById(\"s0\").innerHTML += \'<b>" << "hello" << "</b>\';</script>" << endl;

}
string do_replace(string s, int length) {
    string s1;
    for (int i = 0; i < (int)s.length(); i++){
        if (s[i] == '\n')
            s1 += "&NewLine;";
        else if (s[i] == '\r') {
            if (i+1 < (int)s.length() && s[i+1] == '\n') {
                s1 += "&NewLine;";
                i++;
            }
        }
        else if (s[i] == ' ') {
            s1 += "&nbsp;";
        }
        else if (s[i] == '\t') {
            s1 += "&Tab;";
        }
        else if (s[i] == '\"') {
            s1 += "&quot;";
        }
        else if (s[i] == '\'') {
            s1 += "&apos;";
        }
        else if (s[i] == '&') {
            s1 += "&amp;";
        }
        else if (s[i] == '>') {
            s1 += "&gt;";
        }
        else if (s[i] == '<') {
            s1 += "&lt;";
        }
        else {
            s1 += s[i];
        }
    }
    s1 += "\0";

    return s1;
}

class mysession {
public:
    string host = "";
    string port = "";
    string file = "";
} session_list[5];

class client : public std::enable_shared_from_this<client>{
public:
    client(int sid, string host, string port, string file, boost::asio::io_context& io_context): sid(sid), host(host), port(port), file(file), resolver_(io_context), socket_(io_context){}

    void start() {
        auto self(shared_from_this());
        boost::asio::ip::tcp::resolver::query query_(host, port);
        resolver_.async_resolve(query_, 
        [this, self](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type result) {
            endpoint_ = result;
            if (!ec) {
                do_connect();
                //cout << "<script>document.getElementById(\"s" << sid << "\").innerHTML += \'<b>" << "success." << "</b>\';</script>" << endl;
                
            }
            else {
                socket_.close();
                //cout << "<script>document.getElementById(\"s" << sid << "\").innerHTML += \'<b>" << "failed." << "</b>\';</script>" << endl;
            }
        });
        
    }
    
private:
    void do_connect() {
        auto self(shared_from_this());
        socket_.async_connect(*(endpoint_.begin()), 
        [this, self](boost::system::error_code ec) {
            if (!ec) {
                //cout << "<script>document.getElementById(\"s" << sid << "\").innerHTML += \'<b>" << do_replace("connected\n") << "</b>\';</script>" << endl;
                if (file != "") {
                    file = "test_case/" + file;
                    fin = ifstream(file.data());
                }
                memset(data_, '\0', 20480);
                do_read();
            }
            else {
                fin.close();
                socket_.close();
            }
        });
    }
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length), 
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                cout << "<script>document.getElementById(\"s" << sid << "\").innerHTML += \'<b>" << do_replace(data_, length) << "</b>\';</script>" << endl;
                
                if (strstr(data_, "% ") != NULL) {
                    memset(data_, '\0', 20480);
                    do_write();
                }
                else {
                    memset(data_, '\0', 20480);
                    do_read();
                }
            }
        });
    }
    void do_write() {
        auto self(shared_from_this());
        char cmd[20480] = "";
        if (fin.getline(cmd, 20480)) {
            cmd[strlen(cmd)] = '\n';
            cout << "<script>document.getElementById(\"s" << sid << "\").innerHTML += \'" << do_replace(cmd, 0) << "\';</script>" << endl;
            boost::asio::async_write(socket_, boost::asio::buffer(cmd, strlen(cmd)), 
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    do_read();
                }
            });
        }
        else {
            fin.close();
            socket_.close();
        }
    }

    int sid;
    string host = "";
    string port = "";
    string file = "";
    ifstream fin;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::resolver::results_type endpoint_;
    boost::asio::ip::tcp::socket socket_;
    enum { max_length = 20480 };
	char data_[max_length];
};

int main() {
    string requests = getenv("QUERY_STRING");

    boost::asio::io_context io_context;

    std::size_t indx1 = 0;
    std::size_t indx2 = 0;

    for (int i = 0; i < 5; i++) {
        indx1 = requests.find_first_of("=", indx1+1);
        indx2 = requests.find_first_of("&", indx2+1);
        session_list[i].host = requests.substr(indx1+1, indx2-indx1-1);    
        indx1 = requests.find_first_of("=", indx1+1);
        indx2 = requests.find_first_of("&", indx2+1);
        session_list[i].port = requests.substr(indx1+1, indx2-indx1-1);
        indx1 = requests.find_first_of("=", indx1+1);
        indx2 = requests.find_first_of("&", indx2+1);
        if (indx2 == string::npos) {
            session_list[i].file = requests.substr(indx1+1);
            break;
        }
        else {
            session_list[i].file = requests.substr(indx1+1, indx2-indx1-1);
        }
    }

    //h0=nplinux7.cs.nycu.edu.tw&p0=12345&f0=t1.txt&h1=nplinux3.cs.nycu.edu.tw&p1=12344&f1=t2.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=
    cout << "Content-type: text/html\r\n\r\n" << flush;
    cout << "<!DOCTYPE html>" << endl;
    cout << "<html lang=\"en\">" << endl;
    cout << "<head>" << endl;
    cout << "<meta charset=\"UTF-8\" />" << endl;
    cout << "<title>NP Project 3 Sample Console</title>" << endl;
    cout << "<link" << endl;
    cout << "  rel=\"stylesheet\"" << endl;
    cout << "  href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"" << endl;
    cout << "  integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"" << endl;
    cout << "  crossorigin=\"anonymous\"" << endl;
    cout << "/>" << endl;
    cout << "<link" << endl;
    cout << "  href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"" << endl;
    cout << "  rel=\"stylesheet\"" << endl;
    cout << "/>" << endl;
    cout << "<link" << endl;
    cout << "  rel=\"icon\"" << endl;
    cout << "  type=\"image/png\"" << endl;
    cout << "  href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"" << endl;
    cout << "/>" << endl;
    cout << "<style>" << endl;
    cout << "  * {" << endl;
    cout << "font-family: \'Source Code Pro\', monospace;" << endl;
    cout << "font-size: 1rem !important;" << endl;
    cout << "  }" << endl;
    cout << "  body {" << endl;
    cout << "background-color: #212529;" << endl;
    cout << "  }" << endl;
    cout << "  pre {" << endl;
    cout << "color: #cccccc;" << endl;
    cout << "  }" << endl;
    cout << "  b {" << endl;
    cout << "color: #01b468;" << endl;
    cout << "  }" << endl;
    cout << "</style>" << endl;
    cout << "  </head>" << endl;
    cout << "  <body>" << endl;
    cout << "<table class=\"table table-dark table-bordered\">" << endl;
    cout << "  <thead>" << endl;
    cout << "<tr>" << endl;
    for (int i = 0; i < 5; i++) {
        if (session_list[i].host == "") {
            continue;
        }
        cout << " <th scope=\"col\">" << session_list[i].host << ":" << session_list[i].port << "</th>" << endl;
    }
    // cout << "  <th scope=\"col\">nplinux1.cs.nycu.edu.tw:1234</th>" << endl;
    // cout << "  <th scope=\"col\">nplinux2.cs.nycu.edu.tw:5678</th>" << endl;
    cout << "</tr>" << endl;
    cout << "  </thead>" << endl;
    cout << "  <tbody>" << endl;
    cout << "<tr>" << endl;
    for (int i = 0; i < 5; i++) {
        if (session_list[i].host == "") {
            continue;
        }
        cout << "  <td><pre id=\"s" << i << "\" class=\"mb-0\"></pre></td>" << endl;
    }
    cout << "</tr>" << endl;
    cout << "  </tbody>" << endl;
    cout << "</table>" << endl;
    cout << "  </body>" << endl;
    cout << "</html>" << endl;
    for (int i = 0; i < 5; i++) {
        if (session_list[i].host == "") {
            continue;
        }
        std::make_shared<client>(i, session_list[i].host, session_list[i].port, session_list[i].file, io_context)->start();
        // client* c = new client(i, session_list[i].host, session_list[i].port, session_list[i].file, io_context);
        // c->start();
    }
    io_context.run();

    return 0;
}

