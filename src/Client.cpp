#include <boost/asio.hpp>   
#include <iostream>   
#include <cmath>   
#include <thread>   
#include <vector>   
#include <numeric>   
   
using boost::asio::ip::tcp;   
   
double integrate(double start, double end, double step) {   
    double result = 0.0;   
    for (double x = start; x < end; x += step) {   
        result += 1.0 / log(x) * step;   
    }   
    return result;   
}   
   
double parallel_integrate(double start, double end, double step, int num_threads) {   
    double range = (end - start) / num_threads;   
    std::vector<std::thread> threads;   
    std::vector<double> results(num_threads);   
   
    for (int i = 0; i < num_threads; ++i) {   
        double sub_start = start + i * range;   
        double sub_end = (i == num_threads - 1) ? end : sub_start + range;   
   
        threads.emplace_back([=, &results]() {   
            results[i] = integrate(sub_start, sub_end, step);   
        });   
    }   
   
    for (auto& thread : threads) {   
        thread.join();   
    }   
   
    return std::accumulate(results.begin(), results.end(), 0.0);   
}   
   
void run_client(const std::string& server_ip, int port, bool is_first_client) {   
    try {   
        boost::asio::io_context io_context;   
        tcp::socket socket(io_context);   
        socket.connect(tcp::endpoint(boost::asio::ip::address::from_string(server_ip), port));   
   
        double start, end, step;   
   
        if (is_first_client) {   
            std::cout << "Enter the lower limit of integration: ";   
            std::cin >> start;   
            std::cout << "Enter the upper limit of integration: ";   
            std::cin >> end;   
            std::cout << "Enter the integration step: ";   
            std::cin >> step;   
   
            boost::asio::write(socket, boost::asio::buffer(&start, sizeof(start)));   
            boost::asio::write(socket, boost::asio::buffer(&end, sizeof(end)));   
            boost::asio::write(socket, boost::asio::buffer(&step, sizeof(step)));   
        }   
   
        boost::asio::read(socket, boost::asio::buffer(&start, sizeof(start)));   
        boost::asio::read(socket, boost::asio::buffer(&end, sizeof(end)));   
        boost::asio::read(socket, boost::asio::buffer(&step, sizeof(step)));   
        std::cout << "Received range: [" << start << ", " << end << "] with step " << step << std::endl;   
   
        int num_threads = std::thread::hardware_concurrency();   
        std::cout << "Using cores: " << num_threads << std::endl;   
   
        double partial_result = integrate(start, end, step);   
        std::cout << "Partial result: " << partial_result << std::endl;   
    
        boost::asio::write(socket, boost::asio::buffer(&partial_result, sizeof(partial_result)));   
        
    }   
    catch (std::exception& e) {   
        std::cerr << "Client error: " << e.what() << std::endl;   
    }   
}   
   
int main() {   
    int client_type;   
    std::cout << "Enter 1 if you are the first client, otherwise enter 2: ";   
    std::cin >> client_type;   
   
    run_client("127.0.0.1", 12345, client_type == 1);   
    return 0;   
}