#include <boost/asio.hpp> 
#include <iostream> 
#include <cmath> 
#include <thread> 
#include <vector> 
#include <numeric> 
#include <stdexcept> 
#include <limits> 
#include <chrono> 
 
using boost::asio::ip::tcp; 

double integrate_rectangle(double start, double end, double step) { 
    if (step <= 0) throw std::invalid_argument("Step size must be positive"); 
    double result = 0.0; 
    for (double x = start; x < end; x += step) { 
        result += 1.0 / log(x) * step; 
    } 
    std::cout << "Using Rectangle method for integration.\n"; 
    return result; 
} 
 
double integrate_trapezoidal(double start, double end, double step) { 
    if (step <= 0) throw std::invalid_argument("Step size must be positive"); 
    double result = 0.0; 
    for (double x = start; x < end; x += step) { 
        double next_x = x + step; 
        if (next_x > end) next_x = end; 
        result += (1.0 / log(x) + 1.0 / log(next_x)) * step / 2; 
    } 
    std::cout << "Using Trapezoidal method for integration.\n"; 
    return result; 
} 
 
class Client { 
public: 
    Client(const std::string& server_ip, int port) : socket_(io_context_) { 
        const int max_attempts = 5; 
        int attempts = 0; 
        while (attempts < max_attempts) { 
            try { 
                ++attempts; 
                socket_.connect(tcp::endpoint(boost::asio::ip::address::from_string(server_ip), port)); 
                std::cout << "Connected to server successfully." << std::endl; 
                break; 
            } catch (const boost::system::system_error& e) { 
                std::cerr << "Connection attempt " << attempts << " failed" << std::endl; 
                if (attempts < max_attempts) { 
                    std::cout << "Retrying in 2 seconds..." << std::endl; 
                    std::this_thread::sleep_for(std::chrono::seconds(2)); 
                } else { 
                    throw std::runtime_error("Could not connect to server after multiple attempts."); 
                } 
            } 
        } 
    } 
 
    void send_integration_parameters(double start, double end, double step, int method) { 
        boost::asio::write(socket_, boost::asio::buffer(&start, sizeof(start))); 
        boost::asio::write(socket_, boost::asio::buffer(&end, sizeof(end))); 
        boost::asio::write(socket_, boost::asio::buffer(&step, sizeof(step))); 
        boost::asio::write(socket_, boost::asio::buffer(&method, sizeof(method))); // Send method choice to server 
    } 
 
    void receive_integration_parameters(double& start, double& end, double& step, int& method) { 
        boost::asio::read(socket_, boost::asio::buffer(&start, sizeof(start))); 
        boost::asio::read(socket_, boost::asio::buffer(&end, sizeof(end))); 
        boost::asio::read(socket_, boost::asio::buffer(&step, sizeof(step))); 
        boost::asio::read(socket_, boost::asio::buffer(&method, sizeof(method))); // Receive method choice 
 
        if (step <= 0 || start >= end) { 
            throw std::runtime_error("Received invalid integration parameters from server."); 
        } 
    } 
 
    void send_result(double result) { 
        boost::asio::write(socket_, boost::asio::buffer(&result, sizeof(result))); 
    } 
 
private: 
    boost::asio::io_context io_context_; 
    tcp::socket socket_; 
}; 
 
class IntegrationClientApp { 
public: 
    IntegrationClientApp(const std::string& server_ip, int port, bool is_first_client) 
        : server_ip_(server_ip), port_(port), is_first_client_(is_first_client) {} 
 
    void run() { 
        try { 
            Client client(server_ip_, port_); 
            double start, end, step; 
            int method = 1; 
 
            if (is_first_client_) { 
                request_integration_parameters(start, end, step, method); 
                client.send_integration_parameters(start, end, step, method); 
            } 
 
            client.receive_integration_parameters(start, end, step, method); 
            std::cout << "Received range: [" << start << ", " << end << "] with step " << step << std::endl; 
 
            double partial_result; 
            if (method == 1) { 
                partial_result = integrate_rectangle(start, end, step); 
            } else { 
                partial_result = integrate_trapezoidal(start, end, step); 
            } 
            std::cout << "Partial result: " << partial_result << std::endl; 
 
            client.send_result(partial_result); 
        } 
        catch (const std::exception& e) { 
            std::cerr << "Client error: " << e.what() << std::endl; 
        } 
    } 
 
private: 
    std::string server_ip_; 
    int port_; 
    bool is_first_client_; 
 
    void request_integration_parameters(double& start, double& end, double& step, int& method) { 
        while (true) { 
            try { 
                std::cout << "Enter the lower limit of integration (>= 2): "; 
                std::cin >> start; 
                if (start < 2) { 
                    throw std::invalid_argument("Lower limit must be >= 2."); 
                } 
 
                std::cout << "Enter the upper limit of integration: "; 
                std::cin >> end; 
                if (end <= start) { 
                    throw std::invalid_argument("Upper limit must be greater than lower limit."); 
                } 
 
                std::cout << "Enter the integration step: "; 
                std::cin >> step; 
                if (step <= 0) { 
                    throw std::invalid_argument("Step size must be positive."); 
                } 
 
                std::cout << "Choose integration method (1: Rectangle, 2: Trapezoidal): "; 
                std::cin >> method; 
                if (method != 1 && method != 2) { 
                    throw std::invalid_argument("Invalid method. Enter 1 for Rectangle or 2 for Trapezoidal."); 
                } 
 
                break; 
            } catch (const std::invalid_argument& e) { 
                std::cerr << "Invalid input: " << e.what() << std::endl; 
                std::cout << "Please enter correct values.\n" << std::endl; 
 
                std::cin.clear(); 
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
            } 
        } 
    } 
}; 
 
int main() { 
    setlocale(LC_ALL, "en_US.UTF-8"); 
 
    int client_type; 
    std::cout << "Enter 1 if you are the first client, otherwise enter 2: "; 
    std::cin >> client_type; 
 
    IntegrationClientApp app("127.0.0.1", 12345, client_type == 1); 
    app.run(); 
 
    std::cout << "Press any key to exit..." << std::endl; 
    std::cin.get(); 
    std::cin.get(); 
 
    return 0; 
}