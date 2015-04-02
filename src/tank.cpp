#include "logger.h"
#include "regex.h"
#include "tank.h"

#include <boost/lexical_cast.hpp>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include <fstream>

using namespace wotreplay;

static std::map<std::string, tank_t> tanks;
static bool is_tanks_initialized = false;

static xmlDocPtr get_tanks_xml_content(const std::string &file_name) {
    std::ifstream is(file_name);
    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    xmlDocPtr doc = xmlReadMemory(content.c_str(), (int) content.length(), file_name.c_str(), NULL, 0);
    return doc;
}

static tank_t get_tank_definition(xmlNodePtr node) {
    tank_t tank;

    xmlAttr* attribute = node->properties;
    while(attribute && attribute->name && attribute->children)
    {
        xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);

        if (!std::strcmp((const char*) attribute->name, "countryid")) {
            tank.country_id = boost::lexical_cast<int>(value);
        } else if (!std::strcmp((const char*) attribute->name, "countryname")) {
            tank.country_name = (const char*) value;
        } else if (!std::strcmp((const char*) attribute->name, "tankid")) {
            tank.tank_id = boost::lexical_cast<int>(value);
        } else if (!std::strcmp((const char*) attribute->name, "tankname")) {
            tank.name = (const char*) value;
        } else if (!std::strcmp((const char*) attribute->name, "compDescr")) {
            tank.comp_desc = boost::lexical_cast<int>(value);
        } else if (!std::strcmp((const char*) attribute->name, "icon")) {
            tank.icon = (const char*) value;
        } else if (!std::strcmp((const char*) attribute->name, "class")) {
            tank.class_id = boost::lexical_cast<int>(value);
        } else if (!std::strcmp((const char*) attribute->name, "classname")) {
            tank.class_name = (const char*) value;
        } else if (!std::strcmp((const char*) attribute->name, "tier")) {
            tank.tier = boost::lexical_cast<int>(value);
        } else if (!std::strcmp((const char*) attribute->name, "active")) {
            tank.active = boost::lexical_cast<int>(value);
        }

        xmlFree(value);
        attribute = attribute->next;
    }

    return tank;
}

static std::map<std::string, tank_t> get_tank_definitions() {
    xmlInitParser();
    xmlDocPtr doc = get_tanks_xml_content("tanks.xml");
    xmlNodePtr root = xmlDocGetRootElement(doc);

    std::map<std::string, tank_t> tanks;

    if (!root) {
        logger.writef(log_level_t::error, "Failed to load tanks.xml\n");
        return tanks;
    }

    for (xmlNodePtr node = root->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (!std::strcmp((const char*) node->name, "tank")) {
                tank_t tank = get_tank_definition(node);
                tanks[tank.icon] = tank;
            }
        }
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
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