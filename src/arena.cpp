#include "arena.h"
#include "regex.h"

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <string>
#include <sstream>

using namespace wotreplay;

static xmlDocPtr get_arena_xml_content(const boost::filesystem::path &path) {
    std::ifstream is(path.string());
    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    std::string base_file_name = path.leaf().string();
    size_t pos;
    while ((pos = content.find(base_file_name)) != -1) {
        content.replace(pos, base_file_name.length(), "arena");
    }
    xmlDocPtr doc = xmlReadMemory(content.c_str(), (int) content.length(), base_file_name.c_str(), NULL, 0);
    return doc;
}

static std::tuple<float, float> getCoordinate(std::string value) {
    size_t pos;
    while ((pos = value.find(",")) != -1) {
        value.replace(pos, 1, ".");
    }
    
    std::stringstream ss(value);
    float x_value, y_value;
    ss >> x_value >> y_value;
    
    return std::make_tuple(x_value, y_value);
}

static std::map<int, std::vector<std::tuple<float, float>>> getTeamPositions(xmlNodePtr teamPointsNode) {
    std::map<int, std::vector<std::tuple<float, float>>> team_positions;
    for (xmlNodePtr node = teamPointsNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            int team = boost::lexical_cast<int>(node->name[std::strlen((const char*) node->name) - 1]);
            std::vector<std::tuple<float, float>> positions;
            for (xmlNodePtr positionNode = node->children; positionNode; positionNode = positionNode->next) {
                if (positionNode->type == XML_ELEMENT_NODE) {
                    std::string nodeName((const char*) positionNode->name);
                    std::tuple<float, float> position = getCoordinate((const char*) xmlNodeGetContent(positionNode));
                    positions.push_back(position);
                }
            }
            team_positions[team] = positions;
        }
    }
    return team_positions;
}

static bounding_box_t get_bounding_box(xmlNodePtr boundingBoxNode) {
    bounding_box_t bounding_box;
    for (xmlNodePtr node = boundingBoxNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (!std::strcmp((const char*) node->name, "bottomLeft")) {
                bounding_box.bottom_left = getCoordinate((const char*) xmlNodeGetContent(node));
            }

            if (!std::strcmp((const char*) node->name, "upperRight")) {
                bounding_box.upper_right = getCoordinate((const char*) xmlNodeGetContent(node));
            }
        }
    }
    return bounding_box;
}

static arena_configuration_t getArenaConfiguration(xmlNodePtr gameplayTypeNode) {
    arena_configuration_t configuration;
    configuration.mode = (const char*) gameplayTypeNode->name;
    for (xmlNodePtr node = gameplayTypeNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (!std::strcmp((const char*) node->name, "teamBasePositions")) {
                configuration.team_base_positions = getTeamPositions(node);
            }

            if (!std::strcmp((const char*) node->name, "teamSpawnPoints")) {
                configuration.team_spawn_points = getTeamPositions(node);
            }

            if (!std::strcmp((const char*) node->name, "controlPoint")) {
                configuration.control_point = getCoordinate((const char*) xmlNodeGetContent(node));
            }
        }
    }
    return configuration;
}

static std::map<std::string, arena_configuration_t> getArenaConfigurations(xmlNodePtr gameplayTypesNode) {
    std::map<std::string, arena_configuration_t> configurations;
    for (xmlNodePtr node = gameplayTypesNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            std::string nodeName((const char*) node->name);
            const std::map<std::string, std::string> modeMap = {
                {"domination", "dom"},
                {"assault",    "ass"},
                {"ctf",        "ctf"},
                {"nations",    "nat"}
            };
            auto it = modeMap.find(nodeName);
            if (modeMap.find(nodeName) != modeMap.end()) {
                configurations[it->second] = getArenaConfiguration(node);
            }
        }
    }
    return configurations;
}

static arena_t get_arena_definition(const boost::filesystem::path &path) {
    std::ifstream is(path.string());
    
    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    xmlDocPtr doc = get_arena_xml_content(path);
    xmlNodePtr root = xmlDocGetRootElement(doc);

    arena_t arena;
    for (xmlNodePtr node = root->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (!std::strcmp((const char*) node->name, "name")) {
                regex re("arenas:(.*)/name");
                smatch match;
                std::string nodeContent((const char*) xmlNodeGetContent(node));
                if (regex_search(nodeContent, match, re)) {
                    arena.name = match[1];
                    arena.mini_map = "maps/images/" + arena.name + ".png";
                }
            }

            if (!std::strcmp((const char*) node->name, "boundingBox")) {
                arena.bounding_box = get_bounding_box(node);
            }

            if (!std::strcmp((const char*) node->name, "gameplayTypes")) {
                arena.configurations = getArenaConfigurations(node);
            }
        }
    }
    xmlFreeDoc(doc);
    return arena;
}

static std::map<std::string, arena_t> get_arena_definitions() {
    xmlInitParser();
    std::map<std::string, arena_t> arenas;
    boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
    for( boost::filesystem::directory_iterator it( "maps/definitions" ); it != end_itr; ++it )
    {
        // Skip if not a file
        if( !boost::filesystem::is_regular_file( it->status() ) ) continue;
        // Skip if no match
        if( it->path().extension() != ".xml" ) continue;
        // File matches, store it
        arena_t arena = get_arena_definition(it->path());
        arenas[arena.name] = arena;
    }
    xmlCleanupParser();
    return arenas;
}


std::map<std::string, arena_t> arenas;
static bool is_arenas_initalized = false;

const std::map<std::string, arena_t> &wotreplay::get_arenas() {
    if (!is_arenas_initalized) {
        arenas = get_arena_definitions();
        is_arenas_initalized = true;
    }
    return arenas;
}

bool wotreplay::get_arena(const std::string &name, arena_t& arena) {
    if (!is_arenas_initalized) {
        arenas = get_arena_definitions();
        is_arenas_initalized = true;
    }
    
    auto it = arenas.find(name);
    if (it == arenas.end()) {
        if (name == "north_america") {
            arena = arenas["44_north_america"];
        } else {
            for (auto entry : arenas) {
                std::string map_name = entry.first;
                std::string short_map_name(map_name.begin() + 3, map_name.end());
                if (short_map_name == name) {
                    // rewrite map name
                    arena = entry.second;
                }
            }
        }
    } else {
        arena = arenas[name];
    }
    
    return true;
}