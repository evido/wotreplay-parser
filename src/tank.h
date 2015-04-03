#ifndef wotreplay_tank_def_h
#define wotreplay_tank_def_h

#include <map>
#include <string>

/** @file */

namespace wotreplay {
    /**
     * tank information
     */
    struct tank_t {
        int country_id;
        std::string country_name;
        int tank_id;
        std::string name;
        int comp_desc;
        std::string icon;
        int class_id;
        std::string class_name;
        int tier;
        int active;
    };

    /**
     * Get the complete list of available tanks
     * @return tank definitions
     */
    const std::map<std::string, tank_t> &get_tanks();

    /**
     * Init tank defintion.
     */
    void init_tank_definition();
}

#endif
