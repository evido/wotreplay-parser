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
    std::cout
        << boost::format("Usage: %1% --root <working directory> --type <output type> --input <input file> --output <output file>\n\n") % program_name
        << desc << "\n";
}

bool has_required_options(po::variables_map &vm) {
    return vm.count("type") > 0 && vm.count("input") > 0;
}

float distance(const std::tuple<float, float, float> &left, const std::tuple<float, float, float> &right) {
    float delta_x = std::get<0>(left) - std::get<0>(right);
    float delta_y = std::get<1>(left) - std::get<1>(right);
    float delta_z = std::get<2>(left) - std::get<2>(right);
    // distance in xy plane
    float dist1 = std::sqrt(delta_x*delta_x + delta_y*delta_y);
    return std::sqrt(dist1*dist1 + delta_z*delta_z);
}


int main(int argc, const char * argv[]) {
    po::options_description desc("Allowed options");

    std::string type, output, input, root;
    
    desc.add_options()
        ("type,t"  , po::value(&type), "select output type")
        ("output,o", po::value(&output), "target file")
        ("input,i" , po::value(&input), "input file")
        ("root,r"  , po::value(&root), "set root directory")
        ("help,h"  , "produce help message")
        ("debug"   , "enable parser debugging");

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

    if (vm.count("help")
            || !has_required_options(vm)) {
        show_help(argc, argv, desc);
        std::exit(0);
    }

    if (vm.count("root") >  0
            && chdir(root.c_str()) != 0) {
        std::cerr << boost::format("Cannot change working directory to: %1%\n") % root;
        std::exit(0);
    }
    
    std::ifstream in(input, std::ios::binary);
    if (!in) {
        std::cerr << boost::format("Something went wrong with opening file: %1%\n") % input;
        std::exit(0);
    }

    parser_t parser;
    game_t game;

    bool debug = vm.count("debug") > 0;

    if (debug) {
        wotreplay::log.set_log_level(log_level_t::debug);
    }

    parser.set_debug(debug);
    parser.parse(in, game);

    std::unique_ptr<writer_t> writer;
    
    if (type == "png") {
        writer = std::unique_ptr<writer_t>(new image_writer_t());
        auto &image_writer = dynamic_cast<image_writer_t&>(*writer);
        image_writer.set_show_self(true);
    } else if (type == "json") {
        writer = std::unique_ptr<writer_t>(new json_writer_t());
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