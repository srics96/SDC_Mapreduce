#include <iostream>
#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>


#include "crow.h"
#include "../util/zook.h"

using namespace boost::algorithm;
using namespace std;

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        auto request_body = crow::json::load(req.body);
        
        if (!request_body)
            return crow::response(400);
        
        if (!request_body.has("reducer_count"))
            return crow::response(400, "Reducer count missing");
        if (!request_body.has("shard_size"))
            return crow::response(400, "Shard size missing");
        if (!request_body.has("files"))
            return crow::response(400, "Input file paths missing");

        int reducer_count = request_body["reducer_count"].i();
        int shard_size = request_body["shard_size"].i();
        auto files = request_body["files"];

        std::vector<crow::json::rvalue> files_rvalue = files.lo();
        std::vector <string> file_paths;

        for (auto file: files_rvalue)
            file_paths.push_back(file.s());
        
        string string_file_paths = boost::algorithm::join(file_paths, "$");
        
        auto zoo_keeper = ZookeeperHelper();
        zoo_keeper.create_if_not_exists("/jobs", string());
        string job_id = zoo_keeper.create("/jobs/job_", string(), ZOO_SEQUENCE);
        

        string prefix = "/jobs/job_" + job_id;
        zoo_keeper.create(prefix + "/status", "CREATED");
        zoo_keeper.create(prefix + "/shard_size", to_string(shard_size));
        zoo_keeper.create(prefix + "/reducer_count", to_string(reducer_count));
        zoo_keeper.create(prefix + "/files", string_file_paths);        

        string response_string = "Job successfully submitted: " + job_id;
        return crow::response(200, response_string);
    });

    app.port(5000).run();
}
