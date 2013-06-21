#include "common_header.hpp"

#define INPUT_FILE "fruitybun-data.vrp"

float euclideanDistance
(point_t first, point_t second)
{
    unsigned int x1, y1, x2, y2;

    x1 = first.first;
    y1 = first.second;
    x2 = second.first;
    y2 = second.second;

    return sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
}

// aggregate function for sorting points
bool sort_agg
(point_info_t first, point_info_t second)
{
    return first.distance < second.distance;
}

// get data from file
void RunInfo::parseInput
(std::string const& settings_file)
{
    std::ifstream file_stream(settings_file.c_str(), std::ifstream::in);
    std::string file_line;

    size_t str_return;
    size_t substr_pos;
    std::string sub;

    do
    {
        std::getline(file_stream, file_line);
        str_return = file_line.find("CAPACITY");
    }
    while(str_return == std::string::npos && file_stream.good());
    substr_pos = file_line.find(" : ");
    sub = file_line.substr(substr_pos + 3);
    capacity = std::atoi(sub.c_str());

    std::getline(file_stream, file_line);

    int x, y;
    while(file_line.find("DEMAND") == std::string::npos && file_stream.good())
    {
        std::getline(file_stream, file_line);
        std::istringstream stream(file_line);

        // not using the first
        stream >> sub;

        stream >> sub;
        x = atoi(sub.c_str());
        stream >> sub;
        y = atoi(sub.c_str());
        point_t coord(x, y);

        node_coords.push_back(coord);
    }
    node_coords.pop_back();

    int depot, demand;
    while(file_line.find("DEPOT") == std::string::npos && file_stream.good())
    {
        std::getline(file_stream, file_line);
        std::istringstream stream(file_line);

        stream >> sub;
        depot = atoi(sub.c_str())-1;
        stream >> sub;
        demand = atoi(sub.c_str());
        point_t demand_pair(depot, demand);

        node_demands.push_back(demand_pair);
    }
    node_demands.pop_back();
}

void RunInfo::getSortedCWS
(void)
{
    unsigned int jj;
    unsigned int ii;
    for (ii = 1; ii < node_coords.size(); ii++)
    {
        for (jj = ii+1; jj < node_coords.size(); jj++)
        {
            point_info_t new_info;
            new_info.first_index = ii;
            new_info.second_index = jj;
            new_info.distance = euclideanDistance(node_coords.at(ii), node_coords.at(jj));
            CWS_pair_list.push_back(new_info);
        }
    }

    std::sort(CWS_pair_list.begin(), CWS_pair_list.end(), sort_agg);
}

RunInfo::RunInfo
(std::string const& input_file)
{
    parseInput(input_file);
    getSortedCWS();
}

int main
(int argc, char* argv[])
{
    RunInfo r(INPUT_FILE);

    OCLLearn ocl_learner(r, OCL_TYPE);

    alg_result_t result;

    // track routes
    std::stringstream best_route;
    float best_distance = 10000.0f;

    result = ocl_learner.run();

    // auto find best solution in range
    uint max_cap, max_stops;
    max_cap = 220;
    max_stops = 15;

    // see if its possible to find a better route
    for(max_cap = 100; max_cap <= 220; max_cap++) {
    for(max_stops = 7; max_stops <= 15; max_stops++) {

        // initial counts
        unsigned int cur_capacity = 0;
        float total_distance = 0.0f;
        unsigned int stops_taken = 1;
        unsigned int ii, jj;
        std::stringstream cur_route;

        total_distance += euclideanDistance(
            r.node_coords.at(0),
            r.node_coords.at(result.second.at(0)));
        cur_capacity = r.node_demands.at(result.second.at(0)).second;

        cur_route << "1";
        for (ii = 0, jj = 1;
        ii < result.second.size() - 1, jj < result.second.size();
        ii++, jj++)
        {
            cur_capacity +=
                r.node_demands.at(result.second.at(jj)).second;
            if(cur_capacity > max_cap || stops_taken > max_stops)
            {
                stops_taken = 1;

                total_distance += euclideanDistance(
                    r.node_coords.at(result.second.at(ii)),
                    r.node_coords.at(0));
                total_distance += euclideanDistance(
                    r.node_coords.at(0),
                    r.node_coords.at(result.second.at(jj)));

                cur_capacity =
                    r.node_demands.at(result.second.at(jj)).second;

                cur_route << "->" << result.second.at(ii) + 1;
                cur_route << "->1";
                cur_route << std::endl;
                cur_route << "1";
            }
            else
            {
                total_distance += euclideanDistance(
                    r.node_coords.at(result.second.at(ii)),
                    r.node_coords.at(result.second.at(jj)));
                cur_route << "->" << result.second.at(ii) + 1;
            }
        }
        cur_route << "->1";
        cur_route << std::endl;

        total_distance += euclideanDistance(
            r.node_coords.at(result.second.at(r.node_coords.size()-2)),
            r.node_coords.at(0));

        if(total_distance < best_distance)
        {
            best_distance = total_distance;
            best_route.str("");
            best_route.str(cur_route.str());
        }
    }}

    std::cout << "login mb8224" << std::endl;
    std::cout << "cost " << result.first << std::endl;
    std::cout << best_route.str();

    return 0;
}

