#include <boost/asio.hpp>    
#ifdef _WIN32    
    #pragma comment(lib, "ws2_32.lib")    
#endif    
#include <iostream>    
#include <cmath>    
#include <vector>    
#include <thread>    
#include <mutex>    
#include <chrono>    
#include <atomic>    
    
using boost::asio::ip::tcp;    
using namespace std;    
using namespace std::chrono;    
    
    
class Server {    
public:    
    Server(boost::asio::io_context& io_context, int port)    
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), time_limit_reached(false) {    
        run();    
    }    
    
private:    
    struct Task {    
        double start;    
        double end;    
        double step;    
    };    
    
    void run() {    
        cout << "The server is waiting for the first client to connect to set the integration parameters..." << endl;    
    
        // Connect first client    
        auto socket = std::make_shared<tcp::socket>(io_context_);    
        acceptor_.accept(*socket);    
        client_sockets_.push_back(socket);    
    
        cout << "First client connected. Waiting for integration parameters..." << endl;    
        receive_initial_task(socket);    
    
        cout << "Waiting for additional clients to connect for 1 minute..." << endl;    
        auto start_time = steady_clock::now();    
        thread timer_thread([this, start_time]() {    
            while (!time_limit_reached) {    
                auto elapsed_seconds = duration_cast<seconds>(steady_clock::now() - start_time).count();    
                cout << "Seconds passed: " << elapsed_seconds << endl;    
                if (elapsed_seconds >= 10) {    
                    time_limit_reached = true;    
                    cout << "Waiting time out." << endl;    
                }    
                this_thread::sleep_for(1s);    
            }    
        });    
    
        while (!time_limit_reached) {    
            auto client_socket = std::make_shared<tcp::socket>(io_context_);    
            acceptor_.async_accept(*client_socket, [this, client_socket](boost::system::error_code ec) {    
                if (!ec) {    
                    client_sockets_.push_back(client_socket);    
                    cout << "New client connected. Total number of clients: " << client_sockets_.size() << endl;    
                }    
                else {    
                    cerr << "Error connecting client: " << ec.message() << endl;    
                }    
            });    
    
            io_context_.poll();    
            if (time_limit_reached) break;    
            this_thread::sleep_for(100ms);    
        }    
    
        if (timer_thread.joinable()) {    
            timer_thread.join();    
        }    
    
        distribute_task();    
    }    
    
    void receive_initial_task(std::shared_ptr<tcp::socket> socket) {    
        boost::asio::read(*socket, boost::asio::buffer(&task_.start, sizeof(task_.start)));    
        boost::asio::read(*socket, boost::asio::buffer(&task_.end, sizeof(task_.end)));    
        boost::asio::read(*socket, boost::asio::buffer(&task_.step, sizeof(task_.step)));    
    
        cout << "Integration parameters obtained: "    
            << "start = " << task_.start    
            << ", end = " << task_.end    
            << ", step = " << task_.step << endl;    
    }    
    
    void distribute_task() {    
        if (client_sockets_.empty()) {    
            cerr << "There are no connected clients to distribute the task." << endl;    
            return;    
        }    
    
        double interval = (task_.end - task_.start) / client_sockets_.size();    
        double current_start = task_.start;    
    
        for (size_t i = 0; i < client_sockets_.size(); ++i) {    
            double current_end = current_start + interval;    
    
            boost::asio::write(*client_sockets_[i], boost::asio::buffer(&current_start, sizeof(current_start)));    
            boost::asio::write(*client_sockets_[i], boost::asio::buffer(&current_end, sizeof(current_end)));    
            boost::asio::write(*client_sockets_[i], boost::asio::buffer(&task_.step, sizeof(task_.step)));    
    
            cout << "Range [" << current_start << ", " << current_end << "] sent to client " << i + 1 << endl;    
    
            current_start = current_end;    
        }    
    
        receive_results();    
    }    
    
    void receive_results() {    
        for (size_t i = 0; i < client_sockets_.size(); ++i) {    
            auto socket = client_sockets_[i];    
            double partial_result;    
    
            boost::asio::read(*socket, boost::asio::buffer(&partial_result, sizeof(partial_result)));    
    
            cout << "Partial result received from client " << i + 1 << ": " << partial_result << endl;    
    
            std::lock_guard<std::mutex> lock(result_mutex_);    
            total_result_ += partial_result;    
        }    
    
        cout << "Final integration result: " << total_result_ << endl;    
 
         std::cout << "Server operations completed. Close window..." << std::endl;
        std::cin.get(); // Wait for user input before closing console
    }    
    
    boost::asio::io_context& io_context_;    
    tcp::acceptor acceptor_;    
    Task task_;    
    std::vector<std::shared_ptr<tcp::socket>> client_sockets_;    
    std::mutex result_mutex_;    
    double total_result_ = 0.0;    
    std::atomic<bool> time_limit_reached;    
};    
    
int main() {    
    try {    
        boost::asio::io_context io_context;    
        Server server(io_context, 12345);    
        io_context.run();   
    }    
    catch (std::exception& e) {    
        std::cerr << "Server error: " << e.what() << std::endl;    
    }    
    return 0;    
}