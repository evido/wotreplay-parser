#include "image_writer.h"
#include "animation_writer.h"
#include "heatmap_writer.h"
#include "class_heatmap_writer.h"
#include "json_writer.h"
#include "logger.h"
#include "parser.h"
#include "regex.h"
#include "tank.h"
#include "version.h"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

#ifdef ENABLE_TBB
#include <tbb/tbb.h>
#include <tbb/pipeline.h>
#include <tbb/flow_graph.h>
#endif // ifdef ENABLE_TBB

#include <fstream>
#include <float.h>

#ifdef _MSC_VER
#include <direct.h>
#define EX_OK 0
#define EX_USAGE 64
#define EX_SOFTWARE 70
#else
#include <unistd.h>
#include <sysexits.h>
#endif // ifdef _MSC_VER

using namespace wotreplay;
using namespace boost::filesystem;
namespace po = boost::program_options;

void show_help(int argc, const char *argv [], po::options_description &desc) {
    std::stringstream help_message;
    help_message << desc << "\n";
    logger.write(help_message.str());
}

void show_version() {
    std::stringstream version_message;
    version_message << "wotreplay-parser " << Version::BUILD_VERSION << "\n"
        << "Commit SHA1: " << Version::GIT_SHA1 << "\n"
        << "Commit date: " << Version::GIT_DATE << "\n"
        << "Build date: " << Version::BUILD_DATE << "\n";
    logger.write(version_message.str());
}

static bool is_not_empty(const packet_t &packet) {
    // list of default properties with no special meaning
    std::set<property_t> standard_properties = { property_t::clock, property_t::player_id, property_t::type, property_t::sub_type, property_t::length };
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

void generate_minimap(const arena_t &arena, const std::string &game_mode, int team_id, const std::string &output) {
    image_writer_t writer;

    boost::format file_name_format("%1%/%2%_%3%_%4%.png");
    std::string file_name = (file_name_format % output % arena.name % game_mode % team_id).str();

    logger.writef(log_level_t::info, "Generating mini map (%1%, %2%, %3%, %4%)", output, arena.name, game_mode, team_id);

    try {
        writer.init(arena, game_mode);
        writer.set_no_basemap(false);
        writer.draw_basemap();
        writer.set_recorder_team(team_id);
        writer.set_use_fixed_teamcolors(false);
        std::ofstream os(file_name, std::ios::binary);
        writer.finish();
        writer.write(os);
    }
    catch (const std::exception &exc) {
        logger.writef(log_level_t::error, "Failed to create %1%: %2%", file_name, exc.what());
    }
}

int create_minimaps(const po::variables_map &vm, const std::string &output, bool debug) {
    if (vm.count("output") == 0) {
        logger.write(wotreplay::log_level_t::error, "parameter output is required to use this mode");
        return EX_USAGE;
    }

    init_arena_definition();

    image_writer_t writer;
    for (const auto &arena_entry : get_arenas()) {
        const arena_t &arena = arena_entry.second;
        for (const auto &configuration_entry : arena.configurations) {
            for (int team_id : { 0, 1 }) {
                generate_minimap(arena, configuration_entry.first, team_id, output);
            }
        }
    }

    return EX_OK;
}

void apply_settings(image_writer_t * const writer, const po::variables_map &vm) {
    writer->set_image_width(vm["size"].as<int>());
    writer->set_image_height(vm["size"].as<int>());
    writer->set_no_basemap(vm.count("overlay") > 0);
}

void apply_settings(heatmap_writer_t * const writer, const po::variables_map &vm) {
    writer->skip = vm["skip"].as<double>();
    writer->bounds = std::make_pair(vm["bounds-min"].as<double>(),
        vm["bounds-max"].as<double>());
}

void apply_settings(class_heatmap_writer_t * const writer, const po::variables_map &vm) {
    std::vector<draw_rule_t> rules = parse_draw_rules(vm["rules"].as<std::string>());
    writer->set_draw_rules(rules);
}

void apply_settings(animation_writer_t * const writer, const po::variables_map &vm) {
    writer->set_model_update_rate(vm["model-update-rate"].as<int>());
    writer->set_frame_rate(vm["frame-rate"].as<int>());
}

void apply_settings(json_writer_t * const writer, const po::variables_map &vm) {
    if (vm.count("supress-empty")) {
        writer->set_filter(&is_not_empty);
    }
}

std::unique_ptr<writer_t> create_writer(const std::string &type, const po::variables_map &vm) {
    std::unique_ptr<writer_t> writer;

    if (type == "png") {
        writer = std::unique_ptr<writer_t>(new image_writer_t());
        auto &image_writer = dynamic_cast<image_writer_t&>(*writer);
        image_writer.set_show_self(true);
        image_writer.set_use_fixed_teamcolors(false);

        apply_settings(dynamic_cast<image_writer_t*>(writer.get()), vm);
    }
    else if (type == "json") {
        writer = std::unique_ptr<writer_t>(new json_writer_t());
        apply_settings(dynamic_cast<json_writer_t*>(writer.get()), vm);
    }
    else if (type == "heatmap" || type == "team-heatmap" || type == "team-heatmap-soft") {
        writer = std::unique_ptr<writer_t>(new heatmap_writer_t());
        auto &heatmap_writer = dynamic_cast<heatmap_writer_t&>(*writer);

        if (type == "heatmap") {
            heatmap_writer.mode = heatmap_mode_t::combined;
        }
        else if (type == "team-heatmap") {
            heatmap_writer.mode = heatmap_mode_t::team;
        }
        else if (type == "team-heatmap-soft") {
            heatmap_writer.mode = heatmap_mode_t::team_soft;
        }

        apply_settings(dynamic_cast<heatmap_writer_t*>(writer.get()), vm);
        apply_settings(dynamic_cast<image_writer_t*>(writer.get()), vm);
    }
    else if (type == "class-heatmap") {
        writer = std::unique_ptr<writer_t>(new class_heatmap_writer_t());

        apply_settings(dynamic_cast<class_heatmap_writer_t*>(writer.get()), vm);
        apply_settings(dynamic_cast<heatmap_writer_t*>(writer.get()), vm);
        apply_settings(dynamic_cast<image_writer_t*>(writer.get()), vm);
    }
    else if (type == "gif") {
        writer.reset(new animation_writer_t());

        apply_settings(dynamic_cast<image_writer_t*>(writer.get()), vm);
        apply_settings(dynamic_cast<animation_writer_t*>(writer.get()), vm);
    }
    else {
        logger.writef(log_level_t::error, "Invalid output type (%1%), supported types: png, gif, json, heatmap, team-heatmap, team-heatmap-soft or class-heatmap.\n", type);
    }

    return writer;
}

#ifdef ENABLE_TBB
int process_replay_directory(const po::variables_map &vm, const std::string &input, const std::string &output, const std::string &type, bool debug)
{
    // load all arena's so we can use manual load parser
    init_arena_definition();
    init_tank_definition();

    auto it = directory_iterator(input);
    auto f_generate_replay_paths = [&it](tbb::flow_control &fc) -> std::string {
        while (it != directory_iterator()) {
            auto path = it->path();
            ++it;
            if (is_regular_file(path) && path.extension() == ".wotreplay") {
                return path.string();
            }
        }

        if (it == directory_iterator()) {
            fc.stop();
        }

        return "";
    };

    auto f_parse_replay = [](std::string file_name) -> game_t* {
        std::ifstream in(file_name, std::ios::binary);

        if (!in) {
            logger.writef(log_level_t::error, "Failed to open file: %1%\n", file_name);
            return nullptr;
        }

        std::unique_ptr<game_t> game(new game_t());
        parser_t parser(load_data_mode_t::manual);
        try {
            parser.parse(in, *game);
        }
        catch (std::exception &e) {
            logger.writef(log_level_t::error, "Failed to parse file (%1%): %2%\n", file_name, e.what());
            return nullptr;
        }

        // if we can't load arena data, skip this replay
        if (game->get_arena().name.empty()) {
            return nullptr;
        }

        return game.release();
    };

    auto f_generate_image = [&type, &vm](game_t *game_) -> image_writer_t* {
        if (game_ == nullptr) return nullptr;
        std::unique_ptr<game_t> game(game_);
        std::unique_ptr<writer_t> writer(create_writer(type, vm));
        if (writer && dynamic_cast<image_writer_t*>(writer.get())) {
            std::unique_ptr<image_writer_t> image_writer(static_cast<image_writer_t*>(writer.release()));
            image_writer->init(game->get_arena(), game->get_game_mode());
            image_writer->update(*game);
            return image_writer.release();
        }
        return nullptr;
    };

    std::map<std::tuple<std::string, std::string>, image_writer_t*> writers;

    auto f_merge_image = [&writers](image_writer_t *writer_) {
        if (writer_ == nullptr) return;
        std::unique_ptr<image_writer_t> writer(writer_);

        auto key = std::make_tuple(writer->get_arena().name, writer->get_game_mode());
        auto it = writers.find(key);

        if (it == writers.end()) {
            writers.insert(std::make_pair(key, writer.release()));
        }
        else {
            it->second->merge(*writer);
        }
    };

    int tokens = vm["tokens"].as<int>();
    tbb::parallel_pipeline(tokens,
        tbb::make_filter<void, std::string>(tbb::filter::serial_in_order, f_generate_replay_paths) &
        tbb::make_filter<std::string, game_t*>(tbb::filter::parallel, f_parse_replay) &
        tbb::make_filter<game_t*, image_writer_t*>(tbb::filter::parallel, f_generate_image) &
        tbb::make_filter<image_writer_t*, void>(tbb::filter::serial_out_of_order, f_merge_image)
        );

    typedef std::map<std::tuple<std::string, std::string>, image_writer_t*>::iterator::value_type item_t;
    tbb::parallel_do(writers.begin(), writers.end(), [&output](const item_t &it) {
        path file_name = path(output) / (boost::format("%s_%s.png") %
            std::get<0>(it.first) %
            std::get<1>(it.first)).str();
        std::ofstream out(file_name.string(), std::ios::binary);
        it.second->finish();
        it.second->write(out);
        delete it.second;
    });

    return EX_OK;
}
#else
int process_replay_directory(const po::variables_map &vm, const std::string &input, const std::string &output, const std::string &type, bool debug) {
    if (!(vm.count("type") > 0 && vm.count("input") > 0)) {
        logger.write(wotreplay::log_level_t::error, "parameters type and input are required to use this mode\n");
        return EX_USAGE;
    }

    parser_t parser(load_data_mode_t::bulk);
    parser.set_debug(debug);

    std::map<std::string, std::unique_ptr<writer_t>> writers;
    for (auto it = directory_iterator(input); it != directory_iterator(); ++it) {
        if (!is_regular_file(*it) || it->path().extension() != ".wotreplay") {
            continue;
        }

        std::ifstream in(it->path().string(), std::ios::binary);
        if (!in) {
            logger.writef(log_level_t::error, "Failed to open file: %1%\n", it->path().string());
            return EX_SOFTWARE;
        }

        game_t game;

        try {
            parser.parse(in, game);
        }
        catch (std::exception &e) {
            logger.writef(log_level_t::error, "Failed to parse file (%1%): %2%\n", it->path().string(), e.what());
            continue;
        }

        // if we can't load arena data, skip this replay
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
        if (!out) {
            logger.writef(log_level_t::error, "Something went wrong with opening file: %1%\n", file_name);
            return EX_SOFTWARE;
        }
        it->second->finish();
        it->second->write(out);
    }

    return EX_OK;
}
#endif

int process_replay_file(const po::variables_map &vm, const std::string &input, const std::string &output, const std::string &type, bool debug) {
    static std::map<std::string, std::string> suffixes = {
        {"png", ".png"},
        {"json", ".json"},
        {"heatmap", "_heatmap.png"},
        {"team-heatmap", "_team_heatmap.png"},
        {"team-heatmap-soft", "_team_heatmap_soft.png"},
        {"class-heatmap", "_class_heatmap.png"}
    };

    if (!(vm.count("type") > 0 && vm.count("input") > 0)) {
        logger.write(wotreplay::log_level_t::error, "parameters type and input are required to use this mode\n");
        return EX_USAGE;
    }

    std::ifstream in(input, std::ios::binary);
    if (!in) {
        logger.writef(log_level_t::error, "Failed to open file: %1%\n", input);
        return EX_SOFTWARE;
    }

    parser_t parser(load_data_mode_t::on_demand);
    game_t game;

    parser.set_debug(debug);
    parser.parse(in, game);

    boost::char_separator<char> sep(",");
    boost::tokenizer<boost::char_separator<char>> tokens(type, sep);
    bool single = std::distance(tokens.begin(), tokens.end()) == 1;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        std::unique_ptr<writer_t> writer = create_writer(*it, vm);

        if (!writer) {
            return EX_SOFTWARE;
        }

        writer->init(game.get_arena(), game.get_game_mode());
        writer->update(game);
        writer->finish();

        std::ostream *out;

        if (vm.count("output") > 0) {
            auto file_name = single ? output : output + suffixes[*it];
            out = new std::ofstream(file_name, std::ios::binary);

            if (!*out) {
                logger.writef(log_level_t::error, "Something went wrong with opening file: %1%\n", input);
                return EX_SOFTWARE;
            }
        }
        else {
            out = &std::cout;
        }

        writer->write(*out);

        if (dynamic_cast<std::ofstream*>(out)) {
            dynamic_cast<std::ofstream*>(out)->close();
            delete out;
        }
    }

    return EX_OK;
}

int main(int argc, const char * argv []) {
    po::options_description desc("Allowed options");

    std::string type, output, input, root, rules;
    double skip, bounds_min, bounds_max;
    int size, frame_rate, model_rate;

#ifdef ENABLE_TBB
    int tokens = 10;
#endif

    desc.add_options()
        ("type", po::value(&type), "select output type")
        ("output", po::value(&output), "output file or directory")
        ("input", po::value(&input), "input file or directory")
        ("root", po::value(&root), "set root directory")
        ("help", "produce help message")
        ("debug", "enable parser debugging")
        ("supress-empty", "supress empty packets from json output")
        ("create-minimaps", "create all empty minimaps in output directory")
        ("parse", "parse a replay file")
        ("quiet", "supress diagnostic messages")
        ("skip", po::value(&skip)->default_value(60., "60"), "for heatmaps, skip a certain number of seconds after the start of the battle")
        ("bounds-min", po::value(&bounds_min)->default_value(0.02, "0.02"), "for heatmaps, set min value to display")
        ("bounds-max", po::value(&bounds_max)->default_value(0.98, "0.98"), "for heatmaps, set max value to display")
        ("size", po::value(&size)->default_value(512), "output image size for image writers")
        ("rules", po::value(&rules)->default_value("#ff0000 := team = '1'; #00ff00 := team = '0'"),
            "specify drawing rules, allowing the user to choose the colors used")
        ("parse-rules", "parse rules only and print parsed expression")
        ("overlay", "generate overlay, don't draw basemap in output image")
        ("frame-rate", po::value(&frame_rate)->default_value(10), "set gif frame rate")
        ("model-update-rate", po::value(&model_rate)->default_value(100), "set model update rate")
        ("version", "display version")
#ifdef ENABLE_TBB
        ("tokens", po::value(&tokens)->default_value(10), "number of pipeline tokens")
#endif
        ;

    po::variables_map vm;

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (std::exception &e) {
        show_help(argc, argv, desc);
        std::exit(-1);
    }
    catch (...) {
        logger.write(log_level_t::error, "Unknown error.\n");
        std::exit(-1);
    }

    if (vm.count("help") > 0) {
        show_help(argc, argv, desc);
        std::exit(0);
    }

    if (vm.count("version") > 0) {
        show_version();
        std::exit(0);
    }

    if (vm.count("root") > 0
        && chdir(root.c_str()) != 0) {
        logger.writef(log_level_t::error, "Cannot change working directory to: %1%\n", root);
        std::exit(0);
    }

    bool debug = vm.count("debug") > 0;
    if (debug) {
        logger.set_log_level(log_level_t::debug);
    }
    else if (vm.count("quiet") > 0) {
        logger.set_log_level(log_level_t::none);
    }
    else {
        logger.set_log_level(log_level_t::warning);
    }

    if (vm.count("parse-rules") > 0) {
        logger.set_log_level(log_level_t::debug);
        parse_draw_rules(vm["rules"].as<std::string>());
        std::exit(0);
    }

    int exit_code;
    if (vm.count("parse") > 0) {
        // parse
        if (is_directory(input)) {
            exit_code = process_replay_directory(vm, input, output, type, debug);
        }
        else {
            exit_code = process_replay_file(vm, input, output, type, debug);
        }
    }
    else if (vm.count("create-minimaps") > 0) {
        // create all minimaps
        exit_code = create_minimaps(vm, output, debug);
    }
    else {
        logger.write(wotreplay::log_level_t::error, "Error: no mode specified\n");
        exit_code = EX_USAGE;
    }

    if (exit_code == EX_USAGE) {
        show_help(argc, argv, desc);
    }

    return exit_code;
}
