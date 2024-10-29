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
#include <memory> 
 
using boost::asio::ip::tcp; 
using namespace std; 
using namespace std::chrono; 
 
class IntegrationTask { 
public: 
    double start; 
    double end; 
    double step; 
    int method;  
}; 
 
class Server { 
public: 
    Server(boost::asio::io_context& io_context, int port) 
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), time_limit_reached(false) { 
        try { 
            run(); 
        } catch (const std::exception& e) { 
            std::cerr << "Critical server error: " << e.what() << std::endl; 
        } 
    } 
 
private: 
    void run() { 
        std::cout << "Server is waiting for the first client to set integration parameters..." << std::endl; 
        auto socket = std::make_shared<tcp::socket>(io_context_); 
        acceptor_.accept(*socket); 
        client_sockets_.push_back(socket); 
 
        std::cout << "First client connected. Waiting for integration parameters..." << std::endl; 
        if (!receive_initial_task(socket)) { 
            std::cerr << "Failed to receive initial integration parameters. Shutting down server." << std::endl; 
            return; 
        } 
 
        std::cout << "Waiting for additional clients to connect for 1 minute..." << std::endl; 
        auto start_time = steady_clock::now(); 
        std::thread timer_thread(&Server::start_timer, this, start_time); 
 
        while (!time_limit_reached) { 
            auto client_socket = std::make_shared<tcp::socket>(io_context_); 
            boost::system::error_code ec; 
            acceptor_.async_accept(*client_socket, [this, client_socket](boost::system::error_code ec) { 
                if (!ec) { 
                    client_sockets_.push_back(client_socket); 
                    std::cout << "New client connected. Total clients: " << client_sockets_.size() << std::endl; 
                } else { 
                    std::cerr << "Error connecting client: " << ec.message() << std::endl; 
                } 
            }); 
 
            io_context_.poll(); 
            this_thread::sleep_for(100ms); 
        } 
 
        if (timer_thread.joinable()) { 
            timer_thread.join(); 
        } 
 
        distribute_task(); 
    } 
 
    bool receive_initial_task(std::shared_ptr<tcp::socket> socket) { 
        try { 
            boost::asio::read(*socket, boost::asio::buffer(&task_.start, sizeof(task_.start))); 
            boost::asio::read(*socket, boost::asio::buffer(&task_.end, sizeof(task_.end))); 
            boost::asio::read(*socket, boost::asio::buffer(&task_.step, sizeof(task_.step))); 
            boost::asio::read(*socket, boost::asio::buffer(&task_.method, sizeof(task_.method)));  // Receive method 
 
            std::cout << "Integration parameters received: " 
                      << "start = " << task_.start 
                      << ", end = " << task_.end 
                      << ", step = " << task_.step 
                      << ", method = " << (task_.method == 1 ? "Rectangle" : "Trapezoidal") << std::endl; 
 
            return true; 
        } catch (const std::exception& e) { 
            std::cerr << "Error receiving initial integration parameters: " << e.what() << std::endl; 
            return false; 
        } 
    } 
 
    void start_timer(steady_clock::time_point start_time) { 
        while (!time_limit_reached) { 
            auto elapsed_seconds = duration_cast<seconds>(steady_clock::now() - start_time).count(); 
            std::cout << "Seconds passed: " << elapsed_seconds << std::endl; 
            if (elapsed_seconds >= 60) { 
                time_limit_reached = true; 
                std::cout << "Waiting time expired." << std::endl; 
            } 
            this_thread::sleep_for(1s); 
        } 
    } 
 
    void distribute_task() { 
        if (client_sockets_.empty()) { 
            std::cerr << "No connected clients to distribute the task." << std::endl; 
            return; 
        } 
 
        double interval = (task_.end - task_.start) / client_sockets_.size(); 
        double current_start = task_.start; 
 
        for (size_t i = 0; i < client_sockets_.size(); ++i) { 
            double current_end = current_start + interval; 
            try { 
                boost::asio::write(*client_sockets_[i], boost::asio::buffer(&current_start, sizeof(current_start))); 
                boost::asio::write(*client_sockets_[i], boost::asio::buffer(&current_end, sizeof(current_end))); 
                boost::asio::write(*client_sockets_[i], boost::asio::buffer(&task_.step, sizeof(task_.step))); 
                boost::asio::write(*client_sockets_[i], boost::asio::buffer(&task_.method, sizeof(task_.method)));  // Send method to each client 
                std::cout << "Range [" << current_start << ", " << current_end << "] and method " 
                          << (task_.method == 1 ? "Rectangle" : "Trapezoidal") << " sent to client " << i + 1 << std::endl; 
                current_start = current_end; 
            } catch (const std::exception& e) { 
                std::cerr << "Error sending range to client " << i + 1 << ": " << e.what() << std::endl; 
            } 
        } 
        receive_results(); 
    } 
 
    void receive_results() { 
        for (size_t i = 0; i < client_sockets_.size(); ++i) { 
            double partial_result = 0.0; 
            try { 
                boost::asio::read(*client_sockets_[i], boost::asio::buffer(&partial_result, sizeof(partial_result))); 
                std::cout << "Partial result received from client " << i + 1 << ": " << partial_result << std::endl; 
                std::lock_guard<std::mutex> lock(result_mutex_); 
                total_result_ += partial_result; 
            } catch (const std::exception& e) { 
                std::cerr << "Error receiving result from client " << i + 1 << ": " << e.what() << std::endl; 
            } 
        } 
        std::cout << "Final integration result: " << total_result_ << std::endl; 
        std::cout << "Server operations completed. Close window..." << std::endl; 
        std::cin.get(); 
    } 
 
    boost::asio::io_context& io_context_; 
    tcp::acceptor acceptor_; 
    IntegrationTask task_; 
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
    } catch (const std::exception& e) { 
        std::cerr << "Server error: " << e.what() << std::endl; 
    } 
    return 0; 
}