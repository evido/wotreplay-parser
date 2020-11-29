#ifndef wotreplay_c_h
#define wotreplay_c_h

#include "wotreplay_export.h"

#include <stdint.h>

typedef struct game_packet {
	uint32_t type;
	float clock;
	uint32_t player_id;
	float position[3];
} game_packet;

#ifdef __cplusplus
extern "C" {
#endif

WOTREPLAY_EXPORT void*  wotreplay_game_parse(const char* filename);
WOTREPLAY_EXPORT game_packet wotreplay_game_get_packet(void* game, int index);
WOTREPLAY_EXPORT int wotreplay_game_get_packet_count(void* game);
WOTREPLAY_EXPORT void wotreplay_game_free(void* game);

#ifdef __cplusplus
}
#endif

#endif
