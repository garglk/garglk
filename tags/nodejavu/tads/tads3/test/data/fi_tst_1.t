#include "fi_util.h"

main( args ) {
    "<< littleBlueBall._name >>";
}

class Item: object
;

class Ball: Item
;

class RedBall: Ball
    _init_RedBall() {
        "\n_init_RedBall: << _name >>";
    }
;

class BlueBall: Ball
    _init_BlueBall() {
        "\n_init_BlueBall: << _name >>";
    }

    _init_Ball() {
        "\n_init_Ball (in BlueBall): << _name >>";
    }
;

littleBlueBall: BlueBall;
bigBlueBall: BlueBall;
littleRedBall: RedBall;
bigRedBall: RedBall;

