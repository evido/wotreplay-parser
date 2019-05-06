#include "arena.h"
#include "regex.h"

#include "tinyxml2.h"

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

using namespace wotreplay;
using namespace boost::filesystem;
using namespace tinyxml2;

static std::string get_arena_xml_content(const boost::filesystem::path& path) {
	std::ifstream is(path.string());
	std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
	std::string base_file_name = path.leaf().string();
	size_t pos;
	while ((pos = content.find(base_file_name)) != -1) {
		content.replace(pos, base_file_name.length(), "arena");
	}

	return content;
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

static std::map<int, std::vector<std::tuple<float, float>>> getTeamPositions(XMLElement* teamPointsNode) {
	std::map<int, std::vector<std::tuple<float, float>>> team_positions;
	for (XMLElement* node = teamPointsNode->FirstChildElement(); node; node = node->NextSiblingElement()) {
		int team = boost::lexical_cast<int>(node->Name()[std::strlen((const char*)node->Name()) - 1]);
		std::vector<std::tuple<float, float>> positions;
		for (XMLElement* positionNode = node->FirstChildElement(); positionNode; positionNode = positionNode->NextSiblingElement()) {
			std::string nodeName((const char*)positionNode->Name());
			std::tuple<float, float> position = getCoordinate((const char*)positionNode->GetText());
			positions.push_back(position);
		}
		team_positions[team] = positions;
	}
	return team_positions;
}

static bounding_box_t get_bounding_box(XMLElement* boundingBoxNode) {
	bounding_box_t bounding_box;
	for (XMLElement* node = boundingBoxNode->FirstChildElement(); node; node = node->NextSiblingElement()) {
		if (!std::strcmp((const char*)node->Name(), "bottomLeft")) {
			bounding_box.bottom_left = getCoordinate((const char*)node->GetText());
		}

		if (!std::strcmp((const char*)node->Name(), "upperRight")) {
			bounding_box.upper_right = getCoordinate((const char*)node->GetText());
		}
	}
	return bounding_box;
}

static arena_configuration_t getArenaConfiguration(XMLElement* gameplayTypeNode) {
	arena_configuration_t configuration;
	configuration.mode = (const char*)gameplayTypeNode->Name();
	for (XMLElement* node = gameplayTypeNode->FirstChildElement(); node; node = node->NextSiblingElement()) {
		if (!std::strcmp((const char*)node->Name(), "teamBasePositions")) {
			configuration.team_base_positions = getTeamPositions(node);
		}

		if (!std::strcmp((const char*)node->Name(), "teamSpawnPoints")) {
			configuration.team_spawn_points = getTeamPositions(node);
		}

		if (!std::strcmp((const char*)node->Name(), "controlPoint")) {
			configuration.control_point = getCoordinate((const char*)node->GetText());
		}
	}
	return configuration;
}

static std::map<std::string, arena_configuration_t> getArenaConfigurations(XMLElement* gameplayTypesNode) {
	std::map<std::string, arena_configuration_t> configurations;
	for (XMLElement* node = gameplayTypesNode->FirstChildElement(); node; node = node->NextSiblingElement()) {
		//if (node->type == XML_ELEMENT_NODE) {
		std::string nodeName((const char*)node->Name());
		configurations[nodeName] = getArenaConfiguration(node);
		//}
	}
	return configurations;
}

static arena_t get_arena_definition(const boost::filesystem::path& path) {
	XMLDocument doc;
	doc.Parse(get_arena_xml_content(path).c_str());

	XMLElement* root = doc.RootElement();

	arena_t arena;
	for (XMLElement* node = root->FirstChildElement(); node; node = node->NextSiblingElement()) {
		//if (node->type == XML_ELEMENT_NODE) {
		if (!std::strcmp((const char*)node->Name(), "name")) {
			regex re("arenas:(.*)/name");
			smatch match;
			std::string nodeContent((const char*)node->GetText());
			if (regex_search(nodeContent, match, re)) {
				arena.name = match[1];
				arena.mini_map = "maps/images/" + arena.name + ".png";
			}
		}

		if (!std::strcmp((const char*)node->Name(), "boundingBox")) {
			arena.bounding_box = get_bounding_box(node);
		}

		if (!std::strcmp((const char*)node->Name(), "gameplayTypes")) {
			arena.configurations = getArenaConfigurations(node);
		}
		//}
	}

	//xmlFreeDoc(doc);

	return arena;
}

static std::map<std::string, arena_t> get_arena_definitions() {
	//xmlInitParser();
	std::map<std::string, arena_t> arenas;
	boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
	for (boost::filesystem::directory_iterator it("maps/definitions"); it != end_itr; ++it)
	{
		// Skip if not a file
		if (!boost::filesystem::is_regular_file(it->status())) continue;
		// Skip if no match
		if (it->path().extension() != ".xml") continue;
		// File matches, store it
		arena_t arena = get_arena_definition(it->path());
		arenas[arena.name] = arena;
	}
	//xmlCleanupParser();
	return arenas;
}


static std::map<std::string, arena_t> arenas;
static bool is_arenas_initalized = false;

void wotreplay::init_arena_definition() {
	if (!is_arenas_initalized) {
		arenas = get_arena_definitions();
		is_arenas_initalized = true;
	}
}

const std::map<std::string, arena_t>& wotreplay::get_arenas() {
	return arenas;
}

bool wotreplay::get_arena(const std::string & name, arena_t & arena, bool force) {
	bool has_result = false;
	auto it = arenas.find(name);

	if (it == arenas.end()) {
		if (name == "north_america") {
			arena = arenas["44_north_america"];
			has_result = true;
		}
		else {
			for (auto entry : arenas) {
				std::string map_name = entry.first;
				std::string short_map_name(map_name.begin() + 3, map_name.end());
				if (short_map_name == name) {
					// rewrite map name
					arena = entry.second;
					has_result = true;
				}
			}
		}
	}
	else {
		arena = arenas[name];
		has_result = true;
	}

	if (!has_result && force) {
		//xmlInitParser();
		path path("maps/definitions");
		path /= name + ".xml";
		if (is_regular_file(path)) {
			arena = arenas[name] = get_arena_definition(path);
			has_result = true;
		}
		//xmlCleanupParser();
	}

	return has_result;
}