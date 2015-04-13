#ifndef _BATTLE_H
#define _BATTLE_H



/* These are the different states that a player can be in*/
#define REGISTERING  1
#define WAITING      2
#define ACTIVE       3
#define INACTIVE	 4
#define WRITING      5



struct client {

    int fd;

    /* flags for current state as explained above*/
    int state;

    int hitpoints, numOfPowerMoves;
    char name[256], msg[256];


    struct in_addr ipaddr;
    struct client *next, *opponent, *lastOpponent;
};


static struct client *addclient(struct client *top, int fd, struct in_addr addr);

static struct client *removeclient(struct client *top, int fd);

static void broadcastToRegistered(struct client *top, char *s, int size);
int handleclient(struct client *p, struct client *top);
void searchForBattle(struct client *p, struct client *top);
void startBattle(struct client *firstPlayer, struct client *secondPlayer);

void updateGamePoints(struct client *p);
void updateGameCommands(struct client *p);
void switchTurnsAndCheckIfDead (struct client *p, struct client *top);


int bindandlisten(void);













#endif