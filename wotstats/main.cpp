//
//  main.cpp
//  wotstats
//
//  Created by Jan Temmerman on 14/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "replay_file.h"

#include <iostream>
#include <fstream>
#include <memory>

using std::ifstream;
using std::ios;
using std::ofstream;
using std::ostream_iterator;
using std::string;

using wotstats::replay_file;
using wotstats::buffer_t;

int main(int argc, const char * argv[])
{
    string replay_file_name("/Users/jantemmerman/wotreplays/20120628_2039_germany-E-75_siegfried_line.wotreplay");
    
    ifstream is(replay_file_name, std::ios::binary);
    buffer_t buffer;
    
    replay_file replay(is);
    is.close();
        
    ofstream game_begin("/Users/jantemmerman/wotreplays/game_begin.txt", ios::binary | ios::ate);
    std::copy(replay.get_game_begin().begin(),
              replay.get_game_begin().end(),
              ostream_iterator<char>(game_begin));
    game_begin.close();
    
    
    
    ofstream game_end("/Users/jantemmerman/wotreplays/game_end.txt", ios::binary | ios::ate);
    std::copy(replay.get_game_end().begin(),
              replay.get_game_end().end(),
              ostream_iterator<char>(game_end));
    game_end.close();
    
    std::ofstream replay_content("/Users/jantemmerman/wotreplays/replay.dat");
    std::copy(replay.get_replay().begin(),
              replay.get_replay().end(),
              ostream_iterator<char>(replay_content));
    replay_content.close();
    
    return 0;
}

