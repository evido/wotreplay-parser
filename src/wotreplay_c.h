#ifndef wotreplay_c_h
#define wotreplay_c_h

#include "game.h"
#include "packet.h"

struct game_packet {
	uint32_t type;
	float clock;
	uint32_t player_id;
	float position[3];
};

extern "C" WOTREPLAY_EXPORT wotreplay::game_t*  wotreplay_game_parse(const char* filename);
extern "C" WOTREPLAY_EXPORT game_packet wotreplay_game_get_packet(wotreplay::game_t* game, int index);
extern "C" WOTREPLAY_EXPORT int wotreplay_game_get_packet_count(wotreplay::game_t * game);
extern "C" WOTREPLAY_EXPORT void wotreplay_game_free(wotreplay::game_t * game);

#endif
