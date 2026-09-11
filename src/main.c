#include "base.h"
#include "game/game.h"

i32 main(i32 argc, char** argv) {
    if(!game_run(TRUE)) {
        system("pause");
        return -1;
    }
    return 0;
}
