#include "logger.h"
#include "regex.h"
#include "tank.h"

#include <boost/lexical_cast.hpp>
#include "tinyxml2.h"

#include <fstream>

using namespace wotreplay;
using namespace tinyxml2;

static std::map<std::string, tank_t> tanks;
static bool is_tanks_initialized = false;

static std::string get_tanks_xml_content(const std::string &file_name) {
    std::ifstream is(file_name);
    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    return content;
}

static tank_t get_tank_definition(XMLElement *node) {
    tank_t tank;


	for (const tinyxml2::XMLAttribute* attr = node->FirstAttribute(); attr != 0; attr = attr->Next())
	{

		if (!std::strcmp((const char*)attr->Name(), "countryid")) {
			tank.country_id = attr->IntValue();
		}
		else if (!std::strcmp((const char*)attr->Name(), "countryname")) {
			tank.country_name = (const char*)attr->Value();
		}
		else if (!std::strcmp((const char*)attr->Name(), "tankid")) {
			tank.tank_id = attr->IntValue();
		}
		else if (!std::strcmp((const char*)attr->Name(), "tankname")) {
			tank.name = (const char*)attr->Value();
		}
		else if (!std::strcmp((const char*)attr->Name(), "compDescr")) {
			tank.comp_desc = attr->IntValue();
		}
		else if (!std::strcmp((const char*)attr->Name(), "icon")) {
			tank.icon = (const char*)attr->Value();
		}
		else if (!std::strcmp((const char*)attr->Name(), "class")) {
			tank.class_id = attr->IntValue();
		}
		else if (!std::strcmp((const char*)attr->Name(), "classname")) {
			tank.class_name = (const char*)attr->Value();
		}
		else if (!std::strcmp((const char*)attr->Name(), "tier")) {
			tank.tier = attr->IntValue();
		}
		else if (!std::strcmp((const char*)attr->Name(), "active")) {
			tank.active = attr->IntValue();
		}
	}

    return tank;
}

static std::map<std::string, tank_t> get_tank_definitions() {
	XMLDocument doc;
	doc.Parse(get_tanks_xml_content("tanks.xml").c_str());

	XMLElement* root = doc.RootElement();

    std::map<std::string, tank_t> tanks;

    if (!root) {
        logger.writef(log_level_t::error, "Failed to load tanks.xml\n");
        return tanks;
    }

    for (XMLElement *node = root->FirstChildElement(); node; node = node->NextSiblingElement()) {
        if (!std::strcmp((const char*) node->Name(), "tank")) {
            tank_t tank = get_tank_definition(node);
            tanks[tank.icon] = tank;
        }
    }

    return tanks;
}

void wotreplay::init_tank_definition() {
    if (!is_tanks_initialized) {
        tanks = get_tank_definitions();
        is_tanks_initialized = true;
    }
}

const std::map<std::string, tank_t> &wotreplay::get_tanks() {
    return tanks;
}