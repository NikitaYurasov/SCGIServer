#include <fstream>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <vector>
#include <thread>
#include "server.h"

using namespace std;

class Config
{

public:
    Config()
        : num_threads(1)
        , server_port(3000)
    {
    }

    int ReadFromFile(string filename)
    {
        ifstream ifile(filename);
        size_t line = 1;
        if (ifile.is_open())
        {
            string s;
            while (ifile.good())
            {
                getline(ifile, s);
                stringstream ss;
                ss << s;
                string s1, s2;
                ss >> s1 >> s2;

                if (process_config_string_pair(s1, s2) != 0)
                {
                    cerr << "Configuration file error in " << filename << " line " << line << endl;
                }

                ++line;
            }
        }
        else
        {
            cerr << "Configuration file error: File " << filename << " does not exist" << endl;
        }

        ifile.close();

        return 0;
    }

    unsigned int GetNumThreads() const
    {
        return num_threads;
    }

    unsigned int GetPort() const
    {
        return server_port;
    }

private:
    int process_config_string_pair(const string& s1, const string& s2)
    {
        if (s1 == "THREADS")
        {
            unsigned int num_threads = stoi(s2);
            if (num_threads > 0)
            {
                this->num_threads = num_threads;
            }
        }
        else if (s1 == "PORT")
        {
            unsigned int port = stoi(s2);
            if (port <= (2 << 16))
            {
                this->server_port = port;
            }
        }
        else if (s1 == "")
        {
            return 0;
        }
        else
        {
            return 1;
        }
        return 0;
    }

    unsigned int num_threads;
    unsigned int server_port;
};

ostream& operator<<(ostream& os, const Config& config)
{
    os << "Number of Threads: " << config.GetNumThreads()
       << "\nPort Number: " << config.GetPort();
    return os;
}

int main() try
{
    Config config;
    config.ReadFromFile("config.txt");
    cout << config << endl;
    MainSocket socket(config.GetPort());
    auto thread_function = [&socket]()
    {
        EpollServer server(socket);
        server.event_loop();
    };
    vector<thread> threads;
    for (size_t i = 1; i < config.GetNumThreads(); ++i)
        threads.push_back(thread(thread_function));
    thread_function();
}
catch (const exception& e)
{
    cerr << "global error: " << e.what() << endl;
}
