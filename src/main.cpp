#include "image_writer.h"
#include "heatmap_writer.h"
#include "json_writer.h"
#include "logger.h"
#include "parser.h"
#include "logger.h"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <fstream>

#include <float.h>

using namespace wotreplay;
using namespace boost::filesystem;
namespace po = boost::program_options;

void show_help(int argc, const char *argv[], po::options_description &desc) {
    std::stringstream help_message;
    help_message << desc << "\n";
    logger.write(help_message.str());
}

float distance(const std::tuple<float, float, float> &left, const std::tuple<float, float, float> &right) {
    float delta_x = std::get<0>(left) - std::get<0>(right);
    float delta_y = std::get<1>(left) - std::get<1>(right);
    float delta_z = std::get<2>(left) - std::get<2>(right);
    // distance in xy plane
    float dist1 = std::sqrt(delta_x*delta_x + delta_y*delta_y);
    return std::sqrt(dist1*dist1 + delta_z*delta_z);
}

static bool is_not_empty(const packet_t &packet) {
    // list of default properties with no special meaning
    std::set<property_t> standard_properties = { property_t::clock, property_t::player_id, property_t::type, property_t::sub_type };
    auto properties = packet.get_properties();
    for (int i = 0; i < properties.size(); ++i) {
        property_t property = static_cast<property_t>(i);
        // packet has property, but can not be found in default properties
        if (properties[i] &&
            standard_properties.find(property) == standard_properties.end()) {
            return true;
        }
    }
    return false;
}

int create_minimaps(const po::variables_map &vm, const std::string &output, bool debug) {
    if ( vm.count("output") == 0 ) {
        logger.write(wotreplay::log_level_t::error, "parameter output is required to use this mode");
        return -EXIT_FAILURE;
    }

    boost::format file_name_format("%1%/%2%_%3%_%4%.png");

    image_writer_t writer;
    for (const auto &arena_entry : get_arenas()) {
        const arena_t &arena = arena_entry.second;
        for (const auto &configuration_entry : arena.configurations) {
            for (int team_id : { 0, 1 }) {
                const std::string game_mode = configuration_entry.first;
                writer.init(arena, game_mode);
                writer.set_recorder_team(team_id);
                writer.set_use_fixed_teamcolors(false);
                std::string file_name = (file_name_format % output % arena.name % game_mode % team_id).str();
                std::ofstream os(file_name, std::ios::binary);
                writer.finish();
                writer.write(os);
            }
        }
    }
    
    return EXIT_SUCCESS;
}

std::unique_ptr<writer_t> create_writer(const std::string &type, const po::variables_map &vm) {
    std::unique_ptr<writer_t> writer;

    if (type == "png") {
        writer = std::unique_ptr<writer_t>(new image_writer_t());
        auto &image_writer = dynamic_cast<image_writer_t&>(*writer);
        image_writer.set_show_self(true);
        image_writer.set_use_fixed_teamcolors(false);
    } else if (type == "json") {
        writer = std::unique_ptr<writer_t>(new json_writer_t());
        if (vm.count("supress-empty")) {
            writer->set_filter(&is_not_empty);
        }
    } else if (type == "heatmap") {
        writer = std::unique_ptr<writer_t>(new heatmap_writer_t());
    } else {
        logger.writef(log_level_t::error, "Invalid output type (%1%), supported types: png, json and heatmap.\n", type);
    }

    return writer;
}

int process_replay_directory(const po::variables_map &vm, const std::string &input, const std::string &output, const std::string &type, bool debug) {
    if ( !(vm.count("type") > 0 && vm.count("input") > 0) ) {
        logger.write(wotreplay::log_level_t::error, "parameters type and input are required to use this mode\n");
        return -EXIT_FAILURE;
    }

    parser_t parser;
    parser.set_debug(debug);
    parser.load_data();

    std::map<std::string, std::unique_ptr<writer_t>> writers;
    for (auto it = directory_iterator(input); it != directory_iterator(); ++it) {
        if (!is_regular_file(*it) || it->path().extension() != ".wotreplay") {
            continue;
        }

        std::ifstream in(it->path().string(), std::ios::binary);
        if (!in) {
            logger.writef(log_level_t::error, "Failed to open file: %1%\n", it->path().string());
            return -EXIT_FAILURE;
        }

        game_t game;

        try {
            parser.parse(in, game);
        } catch (std::exception &e) {
            logger.writef(log_level_t::error, "Failed to parse file (%1%): %2%\n", it->path().string(), e.what());
            continue;
        }

        if (game.get_arena().name.empty()) {
            continue;
        }

        std::string name = (boost::format("%s_%s") % game.get_map_name() % game.get_game_mode()).str();
        auto writer = writers.find(name);

        if (writer == writers.end()) {
            auto new_writer = create_writer(type, vm);
            auto result = writers.insert(std::make_pair(name, std::move(new_writer)));
            writer = result.first;
            (writer->second)->init(game.get_arena(), game.get_game_mode());
        }

        (writer->second)->update(game);
    }

    for (auto it = writers.begin(); it != writers.end(); ++it) {
        path file_name = path(output) / (boost::format("%s.png") % it->first).str();
        std::ofstream out(file_name.string(), std::ios::binary);
        it->second->finish();
        it->second->write(out);
    }

    return EXIT_SUCCESS;
}

int process_replay_file(const po::variables_map &vm, const std::string &input, const std::string &output, const std::string &type, bool debug) {
    if ( !(vm.count("type") > 0 && vm.count("input") > 0) ) {
        logger.write(wotreplay::log_level_t::error, "parameters type and input are required to use this mode\n");
        return -EXIT_FAILURE;
    }
    
    std::ifstream in(input, std::ios::binary);
    if (!in) {
        logger.writef(log_level_t::error, "Failed to open file: %1%\n", input);
        return -EXIT_FAILURE;
    }
    
    parser_t parser;
    game_t game;

    parser.set_debug(debug);
    parser.load_data();
    parser.parse(in, game);

    std::unique_ptr<writer_t> writer = create_writer(type, vm);

    if (!writer) {
        return -EXIT_FAILURE;
    }

    writer->init(game.get_arena(), game.get_game_mode());
    writer->update(game);
    writer->finish();

    std::ostream *out;

    if (vm.count("output") > 0) {
        out = new std::ofstream(output, std::ios::binary);
        if (!out) {
            logger.writef(log_level_t::error, "Something went wrong with opening file: %1%\n", input);
            std::exit(0);
        }
    } else {
        out = &std::cout;
    }

    writer->write(*out);

    if (dynamic_cast<std::ofstream*>(out)) {
        dynamic_cast<std::ofstream*>(out)->close();
        delete out;
    }

    return EXIT_SUCCESS;
}

int main(int argc, const char * argv[]) {
    po::options_description desc("Allowed options");

    std::string type, output, input, root;
    
    desc.add_options()
        ("type"  , po::value(&type), "select output type")
        ("output", po::value(&output), "output file or directory")
        ("input" , po::value(&input), "input file or directory")
        ("root"  , po::value(&root), "set root directory")
        ("help"  , "produce help message")
        ("debug"   , "enable parser debugging")
        ("supress-empty", "supress empty packets from json output")
        ("create-minimaps", "create all empty minimaps in output directory")
        ("parse", "parse a replay file")
        ("quiet", "supress diagnostic messages");

    po::variables_map vm;

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (std::exception &e) {
        show_help(argc, argv, desc);
        std::exit(-1);
    } catch (...) {
        logger.write(log_level_t::error, "Unknown error.\n");
        std::exit(-1);
    }

    if (vm.count("help") > 0) {
        show_help(argc, argv, desc);
        std::exit(0);
    }

    if (vm.count("root") >  0
            && chdir(root.c_str()) != 0) {
        logger.writef(log_level_t::error, "Cannot change working directory to: %1%\n", root);
        std::exit(0);
    }
    
    bool debug = vm.count("debug") > 0;
    if (debug) {
        logger.set_log_level(log_level_t::debug);
    } else if (vm.count("quiet") > 0) {
        logger.set_log_level(log_level_t::none);
    } else {
        logger.set_log_level(log_level_t::warning);
    }

    int exit_code;
    if (vm.count("parse") > 0) {
        // parse
        if (is_directory(input)) {
            exit_code = process_replay_directory(vm, input, output, type, debug);
        } else {
            exit_code = process_replay_file(vm, input, output, type, debug);
        }
    } else if (vm.count("create-minimaps") > 0) {
        // create all minimaps
        exit_code = create_minimaps(vm, output, debug);
    } else {
        logger.write(wotreplay::log_level_t::error, "Error: no mode specified\n");
        exit_code = -EXIT_FAILURE;
    }

    if (exit_code < 0) {
        show_help(argc, argv, desc);
    }

    return exit_code;
}