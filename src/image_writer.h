#ifndef wotreplay__image_writer_h
#define wotreplay__image_writer_h

#include "packet.h"
#include "parser.h"
#include "writer.h"

#include <boost/multi_array.hpp>
#include <png.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace wotreplay {
    /** 
     * wotreplay::image_writer_t draws an image from wotreplay::packet_t on a minimap 
     */
    class image_writer_t : public writer_t {
    public:
        virtual void update(const game_t &game) override;
        virtual void finish() override;
        virtual void reset() override;
        virtual void write(std::ostream &os) override;
        virtual bool is_initialized() const;
        virtual void init(const std::string &map, const std::string &mode);
        virtual void clear() override;
    private:
        /**
         * Load a background image from the combination map name and game mode.
         * @param map_name The map name of the background image.
         * @param game_mode The game mode of the background image.
         */
        void load_base_map(const std::string &map_name, const std::string &game_mode);
        /**
         * Implementation of drawing a 'death' packet on the image.
         * @param packet The packet containing the information to draw.
         * @param game Game
         */
        void draw_death(const packet_t &packet, const game_t &game);
        /**
         * Implementation of drawing a 'position' packet on the image.
         * @param packet The packet containing the information to draw.
         * @param game Game
         */
        void draw_position(const packet_t &packet, const game_t &game, boost::multi_array<float, 3> &image);
        
        /** Background image */
        boost::multi_array<uint8_t, 3> base;
        /** Image containing the frequency a player is located on a coordinate */
        boost::multi_array<float, 3> positions;
        /** Image containing the frequency a player has died on a coordinate */
        boost::multi_array<float, 3> deaths;
        /** Contains the resulting image. */
        boost::multi_array<uint8_t, 3> result;
        
        bool initialized;
    };
}

#endif /* defined(wotreplay__image_writer_h) */
