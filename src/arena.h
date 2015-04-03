//
//  arena_def.h
//  wotreplay-parser
//
//  Created by Jan Temmerman on 05/08/13.
//
//

#ifndef wotreplay_arena_def_h
#define wotreplay_arena_def_h

#include <map>
#include <string>
#include <tuple>
#include <vector>

/** @file */

namespace wotreplay {
    /**
     * define a bounding box of an arena by its bottom left
     * and upper right corner
     */
    struct bounding_box_t {
        /** bottom left corner of the bounding box */
        std::tuple<float, float> bottom_left;
        /** upper right corner of the bounding box */
        std::tuple<float, float> upper_right;
    };

    /**
     * Configuration of the arena, defines the positions of the 
     * spawn points, control poins and base points
     */
    struct arena_configuration_t {
        /** control point in ctf mode */
        std::tuple<float, float> control_point;
        /** spawn points for the teams */
        std::map<int, std::vector<std::tuple<float, float>>> team_spawn_points;
        /** base position for the teams */
        std::map<int, std::vector<std::tuple<float, float>>> team_base_positions;
        /** game type */
        std::string mode;
    };

    /**
     * arena definition, provides a description of the arena size
     * and its available game modes
     */
    struct arena_t {
        /** maps available game modes to configuration */
        std::map<std::string, arena_configuration_t> configurations;
        /** arena name */
        std::string name;
        /** arena bounding box */
        bounding_box_t bounding_box;
        /** mini map path */
        std::string mini_map;
    };

    /**
     * Find the arena definition for the arena name
     * @param name the arena name
     * @param arena output variable to store the arena
     * @param when definition is not found in cache, force lookup on disk
     * @return \c true if the arena was found \c false if not
     */
    bool get_arena(const std::string &name, arena_t &arena, bool force);

    /**
     * Get the complete list of available arena definitions
     * @return arena definition map
     */
    const std::map<std::string, arena_t> &get_arenas();

    /**
     * Init arena defintion.
     */
    void init_arena_definition();
}

#endif /* defined(wotreplay_arena_def_h) */
