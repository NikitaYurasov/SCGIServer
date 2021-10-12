#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdexcept>
#include <cstring>
#include <string_view>
#include <vector>
#include <map>
#include <charconv>
#include <unistd.h>
#include <cassert>

void set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("Can't get flags");

    flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) == -1)
        throw std::runtime_error("Can't set flags");
}

class MainSocket
{
public:
    MainSocket(unsigned short port)
    {
        _sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_sock_fd == -1)
        {
            throw std::runtime_error("Error creating _sock_fd");
        }

        struct sockaddr_in server_address;
        memset(&server_address, 0, sizeof(struct sockaddr_in));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(port);

        int sockoptval = 1;
        setsockopt(_sock_fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof (sockoptval));

        if (bind(_sock_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)
        {
            throw std::runtime_error("Error: bind");
        }

        set_non_blocking(_sock_fd);

        if (listen(_sock_fd, SOMAXCONN) < 0)
        {
            throw std::runtime_error("Error: listen");
        }
    }

    ~MainSocket()
    {
        close(_sock_fd);
    }

    int fd()
    {
        return _sock_fd;
    }

private:
    int _sock_fd;
};

class Client
{
public:
    const size_t MAX_READ_SIZE = 4096;

    Client(int fd)
        : _fd(fd)
    {
    }

    int fd() const
    {
        return _fd;
    }

    bool read_ready()
    {
        try
        {
            char buffer[MAX_READ_SIZE];
            for (;;)
            {
                ssize_t len = read(_fd, buffer, MAX_READ_SIZE);
                if (len <= 0)
                {
                    break;
                }
                _data.append(buffer, len);
            }
            before_switch:
            switch (_state)
            {
            case READING_HEADER_LENGTH:
            {
                size_t header_length_end = _data.find(':');
                if (header_length_end != std::string::npos)
                {
                    _header_length_str = std::string_view(_data.data(), header_length_end);
                    auto res = std::from_chars(_header_length_str.data(), _header_length_str.data() + _header_length_str.size(), _header_length);
                    if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range || res.ptr != _header_length_str.data() + _header_length_str.size())
                    {
                        return false;
                    }
                    _state = READING_HEADER;
                    goto before_switch;
                }
                break;
            }
            case READING_HEADER:
            {
                if (_data.size() >= _header_length_str.size() + 1 + _header_length + 1)
                {
                    _header = std::string_view(_data.data() + _header_length_str.size() + 1, _header_length);
                    auto get_header_content = [this](const std::string& header_name_)
                    {
                        std::string header_name = header_name_ + "\0";
                        size_t header_name_end = _header.find(header_name);
                        if (header_name_end == std::string_view::npos)
                        {
                            throw std::runtime_error("");
                        }
                        header_name_end += header_name.size();
                        size_t header_value_end = _header.find('\0', header_name_end + 1);
                        if (header_value_end == std::string_view::npos)
                        {
                            throw std::runtime_error("");
                        }
                        return std::string_view(_header.data() + header_name_end + 1, header_value_end - header_name_end - 1);
                    };
                    std::string_view content_length_str = get_header_content("CONTENT_LENGTH");
                    auto res = std::from_chars(content_length_str.data(), content_length_str.data() + content_length_str.size(), _content_length);
                    if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range || res.ptr != content_length_str.data() + content_length_str.size())
                    {
                        return false;
                    }
                    _uri = get_header_content("REQUEST_URI");
                    _state = READING_CONTENT;
                    goto before_switch;
                }
                break;
            }
            case READING_CONTENT:
            {
                if (_data.size() >= _header_length_str.size() + 1 + _header_length + 1 + _content_length)
                {
                    if (_response.empty())
                    {
                        _response = std::string("Status: 200 OK""\x0d""\x0a"
                                                 "Content-Type: application/octet-stream""\x0d""\x0a"
                                                 "\x0d""\x0a"
                                                 "You requested URI: ") +
                                     std::string(_uri) +
                                     std::string("\x0d""\x0a"
                                                 "Your request content was: ") +
                                     std::string(_content) +
                                     std::string("\x0d""\x0a"
                                                 "Garbage to make the response larger""\x0d""\x0a");
                        _response += std::string((1 << 16), 'A');
                    }
                    write_ready();

                    if (_response_position == _response.size())
                    {
                        return false;
                    }
                }
                break;
            }
            }
        }
        catch (const std::exception)
        {
            return false;
        }
        return true;
    }

    bool write_ready()
    {
        for (int i = 0;; ++i)
        {
            ssize_t len = write(_fd, _response.data() + _response_position, _response.size() - _response_position);
            if (len <= 0)
            {
                break;
            }
            _response_position += len;
        }
        if (!_response.empty() && _response_position == _response.size())
        {
            return false;
        }
        return true;
    }

private:
    int _fd;
    enum State
    {
        READING_HEADER_LENGTH,
        READING_HEADER,
        READING_CONTENT
    };
    State _state = READING_HEADER_LENGTH;
    std::string _data;
    std::string_view _header_length_str;
    size_t _header_length;
    std::string_view _header;
    size_t _content_length;
    std::string_view _uri;
    std::string_view _content;
    std::string _response;
    size_t _response_position = 0;
};

class EpollServer
{
public:

    size_t MAX_EPOLL_EVENTS_COUNT = 256;
    EpollServer(const MainSocket& socket)
        : _events(MAX_EPOLL_EVENTS_COUNT)
        , _main_socket(socket)
    {
        _epoll_fd = epoll_create1(0);
        if (_epoll_fd < 0)
        {
            throw std::runtime_error("epoll error");
        }

        struct epoll_event event;
        event.events = EPOLLIN;

        event.data.fd = _main_socket.fd();
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event) == -1)
        {
            throw std::runtime_error("Failed to add epoll");
        }
    }

    void event_loop()
    {
        for (;;)
        {
            int nu_fds = epoll_wait(_epoll_fd, _events.data(), _events.size(), -1);
            if (nu_fds != -1)
            {
                for (int i = 0; i < nu_fds; i++)
                {
                    if (_events[i].data.fd == _main_socket.fd())
                    {
                        handle_accept();
                    }
                    else
                    {
                        handle_client(_events[i]);
                    }
                }
            }
        }
    }

private:
    void handle_accept()
    {
        int client_fd = accept(_main_socket.fd(), NULL, NULL);
        if (client_fd == -1)
        {
            return;
        }

        set_non_blocking(client_fd);

        auto [it, flag] = _clients.emplace(client_fd, Client(client_fd));
        assert(flag);

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLOUT;
        ev.data.ptr = &(it->second);

        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
        {
            _clients.erase(it);
            throw std::runtime_error("epoll_ctl fail");
        }
    }

    bool handle_client(epoll_event ev)
    {
        Client* client = reinterpret_cast<Client*>(ev.data.ptr);

        if (ev.events & EPOLLIN)
        {
            if (!client->read_ready())
            {
                remove_client(client);
                return false;
            }
        }

        if (ev.events & EPOLLRDHUP)
        {
            remove_client(client);
            return false;
        }

        if (ev.events & EPOLLOUT)
        {
            if (!client->write_ready())
            {
                remove_client(client);
                return false;
            }
        }

        return true;
    }

    void remove_client(Client* client)
    {
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client->fd(), NULL);
        close(client->fd());
        _clients.erase(client->fd());
    }

private:
    std::vector<struct epoll_event> _events;
    std::map<int, Client> _clients;
    MainSocket _main_socket;
    int _epoll_fd;
};

#endif //SERVER_H
