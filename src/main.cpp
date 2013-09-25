#include "image_writer.h"
#include "json_writer.h"
#include "logger.h"
#include "parser.h"
#include "logger.h"

#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <fstream>

#include <float.h>

using namespace wotreplay;
namespace po = boost::program_options;

void show_help(int argc, const char *argv[], po::options_description &desc) {
    std::string program_name(argv[0]);
    std::cout << desc << "\n";
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

int parse_replay(const po::variables_map &vm, const std::string &input, const std::string &output, const std::string &type, bool debug) {
    if ( !(vm.count("type") > 0 && vm.count("input") > 0) ) {
        logger.write(wotreplay::log_level_t::error, "parameters type and input are required to use this mode");
        return -EXIT_FAILURE;
    }
    
    std::ifstream in(input, std::ios::binary);
    if (!in) {
        std::cerr << boost::format("Something went wrong with opening file: %1%\n") % input;
        std::exit(0);
    }
    
    parser_t parser;
    game_t game;

    parser.set_debug(debug);
    parser.parse(in, game);

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
    } else {
        std::cout << boost::format("Invalid output type (%1%), supported types: png and json.\n") % type;
    }

    writer->init(game.get_arena(), game.get_game_mode());
    writer->update(game);
    writer->finish();

    std::ostream *out;

    if (vm.count("output") > 0) {
        out = new std::ofstream(output, std::ios::binary);
        if (!out) {
            std::cerr << boost::format("Something went wrong with opening file: %1%\n") % input;
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
        ("output", po::value(&output), "target file")
        ("input" , po::value(&input), "input file")
        ("root"  , po::value(&root), "set root directory")
        ("help"  , "produce help message")
        ("debug"   , "enable parser debugging")
        ("supress-empty", "supress empty packets from json output")
        ("create-minimaps", "create all empty minimaps in output directory")
        ("parse", "parse a replay file");

    po::variables_map vm;

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (std::exception &e) {
        show_help(argc, argv, desc);
        std::exit(-1);
    } catch (...) {
        std::cerr << "Unknown error." << std::endl;
        std::exit(-1);
    }

    if (vm.count("help") > 0) {
        show_help(argc, argv, desc);
        std::exit(0);
    }

    if (vm.count("root") >  0
            && chdir(root.c_str()) != 0) {
        std::cerr << boost::format("Cannot change working directory to: %1%\n") % root;
        std::exit(0);
    }

    bool debug = vm.count("debug") > 0;

    if (debug) {
        logger.set_log_level(log_level_t::debug);
    }

    int exit_code;
    
    if (vm.count("parse") > 0) {
        // parse
        exit_code = parse_replay(vm, input, output, type, debug);
    } else if (vm.count("create-minimaps") > 0) {
        // create all minimaps
        exit_code =  create_minimaps(vm, output, debug);
    } else {
        logger.write(wotreplay::log_level_t::error, "Error: no mode specified");
        exit_code = -EXIT_FAILURE;
    }

    if (exit_code < 0) {
        show_help(argc, argv, desc);
    }

    return exit_code;
}