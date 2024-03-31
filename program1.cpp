#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <mpi.h>

// Structure to represent traffic signal data
struct tData {
    std::string timestamp;
    int light_id;
    int num_cars;
};

// Function to read data from file
std::vector<tData> readDataFromData(const std::string& filename) {
    std::vector<tData> data;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << filename << std::endl;
        return data; // Return empty data vector
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        tData td;
        if (iss >> td.timestamp >> td.light_id >> td.num_cars) {
            data.push_back(td);
        } else {
            std::cerr << "Error: Invalid data format in file: " << filename << std::endl;
            data.clear(); // Clear data vector
            break;
        }
    }
    file.close();
    return data;
}

// Function to find top congested lights for a given timestamp
void findTopCongestedLights(const std::string& timestamp, const std::map<std::string, std::vector<tData>>& traffic_data) {
    auto it = traffic_data.find(timestamp);
    if (it != traffic_data.end()) {
        std::vector<tData> data_for_timestamp = it->second;
        
        // Sort data based on the number of cars passed
        std::sort(data_for_timestamp.begin(), data_for_timestamp.end(),
                  [](const tData& a, const tData& b) { return a.num_cars > b.num_cars; });

        // Display the top 5 congested traffic lights for the given timestamp
        std::cout << "Top 5 congested traffic lights for timestamp " << timestamp << ":" << std::endl;
        for (size_t i = 0; i < std::min(static_cast<size_t>(5), data_for_timestamp.size()); ++i) {
            std::cout << "Light ID: " << data_for_timestamp[i].light_id << ", Number of Cars: " << data_for_timestamp[i].num_cars << std::endl;
        }
        std::cout << std::endl;
    } else {
        std::cout << "No data available for timestamp " << timestamp << std::endl;
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int num_processes, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    const std::string filename = "data.txt"; // Change this to your data file name

    std::vector<tData> data;
    if (rank == 0) {
        // Master process reads data from file
        data = readDataFromData(filename);
    }

    // Broadcast the number of data items to all processes
    int num_data = data.size();
    MPI_Bcast(&num_data, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Scatter data to all processes
    std::vector<tData> local_data(num_data / num_processes);
    MPI_Scatter(data.data(), num_data / num_processes * sizeof(tData), MPI_CHAR, 
                local_data.data(), num_data / num_processes * sizeof(tData), MPI_CHAR, 
                0, MPI_COMM_WORLD);

    // Gather data from all processes
    std::vector<tData> all_data(num_data);
    MPI_Gather(local_data.data(), num_data / num_processes * sizeof(tData), MPI_CHAR, 
               all_data.data(), num_data / num_processes * sizeof(tData), MPI_CHAR, 
               0, MPI_COMM_WORLD);

    // Create a map to store traffic data for each timestamp
    std::map<std::string, std::vector<tData>> traffic_data;
    if (rank == 0) {
        for (const auto& td : all_data) {
            traffic_data[td.timestamp].push_back(td);
        }
    }

    // Synchronize all processes
    MPI_Barrier(MPI_COMM_WORLD);

    // Find top congested lights for each hour
    for (int hour = 7; hour < 12; ++hour) {
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << hour << ":00:00";
        std::string timestamp = oss.str();
        findTopCongestedLights(timestamp, traffic_data);
    }

    MPI_Finalize();
    return 0;
}