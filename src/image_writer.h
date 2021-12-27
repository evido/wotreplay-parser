#ifndef wotreplay__image_writer_h
#define wotreplay__image_writer_h

#include "game.h"
#include "packet.h"
#include "writer.h"

#include <boost/multi_array.hpp>
#include <stdint.h>
#include <string>
#include <vector>

namespace wotreplay {
    /** 
     * wotreplay::image_writer_t draws an image from wotreplay::packet_t on a minimap 
     */
    class image_writer_t : public writer_t {
    public:
        /** default constructor */
        image_writer_t();
        virtual void update(const game_t &game) override;
        virtual void finish() override;
        virtual void reset() override;
        virtual void write(std::ostream &os) override;
        virtual bool is_initialized() const override;
        virtual void init(const arena_t &arena, const std::string &mode) override;
        virtual void clear() override;
        virtual void set_filter(filter_t filter) override;
        /**
         * Set if the recording player should be drawn on the map in a seperate color.
         * @param show_self Show the recording player ?
         */
        virtual void set_show_self(bool show_self);
        /**
         * Returns if the recording player is drawn on the map in a seperate color.
         * @return Show the recording player ?
         */
        virtual bool get_show_self() const;
        /**
         * When true, colors of positions will not follow the color of the recorder. Team 1 will
         * be colored green, team 2 will be colored red.
         * @param use_fixed_teamcolors new value for use_fixed_teamcolors
         */
        virtual void set_use_fixed_teamcolors(bool use_fixed_teamcolors);
        /**
         * get the current value of use_fixed_teamcolors
         * @return current value of use_fixed_teamcolors
         */
        virtual bool get_use_fixed_teamcolors() const;
        /**
         * set the recorder team, this controls the colors of the elements on the map
         * @param recorder_team the new recorder team
         */
        void set_recorder_team(int recorder_team);
        /**
         * get the recorder team
         * @return recorder_team
         */
        int get_recorder_team() const;
        /**
         * Get current arena definition
         * @returns arena definition with which the writer was initialized
         */
        virtual const arena_t &get_arena() const;
        /**
         * Get current game mode
         * @returns game mode with which the writer was initialized
         */
        virtual const std::string &get_game_mode() const;
        /**
         * Include data from referenced writer in current writer
         * @param writer other writer
         */
        virtual void merge(const image_writer_t &writer);
        /**
         * @return image width
         */
        int get_image_width() const;
        /**
         * set image width
         * @param image_width new image width
         */
        void set_image_width(int image_width);
        /**
         * @return image height
         */
        int get_image_height() const;
        /**
         * set image height
         * @param image_height new image height
         */
        void set_image_height(int image_height);
        /**
         * @return no basemap
         */
        bool get_no_basemap() const;
        /**
         * set no basemap
         * @param no_basemap new no_basemap value
         */
        void set_no_basemap(bool no_basemap);
		/*
		* get result image
		* @return image data
		*/
		const boost::multi_array<uint8_t, 3> &get_result() const;
        virtual void draw_basemap();
    protected:
        /**
         * Load a background image from the combination map name and game mode.
         * @param path the path to the minimap image
         */
        void load_base_map(const std::string &path);
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
        /**
         * read map element by name
         * @param name element name
         * @return bitmap of the element
         */
        boost::multi_array<uint8_t, 3> get_element(const std::string &name);
        /**
         * draw a grid on the given image
         * @param image the target image
         */
        void draw_grid(boost::multi_array<uint8_t, 3> &image);
        /**
         * draw an element with coordinates in 'image' space
         * @param element element the element to draw
         * @param position the coordinate in 'game' space
         * @param mask the mask to apply to the rgba value
         */
        void draw_element(const boost::multi_array<uint8_t, 3> &element, std::tuple<float, float> position, int mask = 0xFFFFFFFF);
        /**
         * draw an element with coordinates in 'image' space
         * @param element element the element to draw
         * @param x the x position of the upper left corner
         * @param y the y position of the upper left corner
         * @param mask the mask to apply to the rgba value
         */
        void draw_element(const boost::multi_array<uint8_t, 3> &element, int x, int y, int mask);
        /**
         * Draw game elements on the map
         * @param recorder_team the team of the recorder
         */
        void draw_elements();
        /** Background image */
        boost::multi_array<uint8_t, 3> base;
        /** Image containing the frequency a player is located on a coordinate */
        boost::multi_array<float, 3> positions;
        /** Image containing the frequency a player has died on a coordinate */
        boost::multi_array<float, 3> deaths;
        /** Contains the resulting image. */
        boost::multi_array<uint8_t, 3> result;
        bool initialized = false;
        bool show_self = false;
        bool use_fixed_teamcolors = true;
        std::string game_mode;
        arena_t arena;
        int recorder_team = -1;
        filter_t filter;
        int image_width;
        int image_height;
        bool no_basemap;
    };
}

#endif /* defined(wotreplay__image_writer_h) */
