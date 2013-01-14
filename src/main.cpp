#include "image_writer.h"
#include "json_writer.h"
#include "parser.h"

#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <fstream>

using namespace wotreplay;
namespace po = boost::program_options;

void show_help(int argc, const char *argv[], po::options_description &desc) {
    std::string program_name(argv[0]);
    std::cout
        << boost::format("Usage: %1% --root <working directory> --type <output type> --input <input file> --output <output file>\n\n") % program_name
        << desc << "\n";
}

bool has_required_options(po::variables_map &vm) {
    return vm.count("output") > 0 || vm.count("type") > 0
            || vm.count("root") > 0|| vm.count("input") > 0;
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

    if (chdir(root.c_str()) != 0) {
        std::cerr << boost::format("cannot change working directory to: %1%\n") % root;
        std::exit(0);
    }

    std::ifstream is(input, std::ios::binary);

    if (!is) {
        std::cerr << boost::format("Something went wrong with reading file: %1%\n") % input;
        std::exit(0);
    }

    parser_t parser;
    game_t game;
    
    bool debug = vm.count("debug") > 0;
    parser.set_debug(debug);
    parser.parse(is, game);
    is.close();

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

    writer->init(game.get_map_name(), game.get_game_mode());
    writer->update(game);
    writer->finish();
    
    std::ofstream file(output);
    writer->write(file);
    
    return EXIT_SUCCESS;
}
