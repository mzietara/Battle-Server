#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "battle.h"

#ifndef PORT
    #define PORT 30100
#endif

int main(void) {
	srand((unsigned) time(NULL));
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    //struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i;


    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;

        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == 0) {
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
            write(clientfd, "What is your name?", 18);
        }
        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

void startBattle(struct client *firstPlayer, struct client *secondPlayer){
	/* set the hitpoints of the players and the number of power moves.
	 * hitpoints are between 20 and 30
	 * number of power moves is between 1 and 3
	 */
	firstPlayer->hitpoints  = rand() % 11 + 20;
	secondPlayer->hitpoints = rand() % 11 + 20;
	firstPlayer->numOfPowerMoves  = rand() % 3 + 1;
	secondPlayer->numOfPowerMoves = rand() % 3 + 1;


	firstPlayer->state = ACTIVE;
	secondPlayer->state = INACTIVE;
	firstPlayer->opponent = secondPlayer;
	secondPlayer->opponent = firstPlayer;
	char outbuf[256];
	sprintf(outbuf, "You engage %s!\n", secondPlayer->name);
	write(firstPlayer->fd, outbuf, strlen(outbuf));

	sprintf(outbuf, "You engage %s!\n", firstPlayer->name);
	write(secondPlayer->fd, outbuf, strlen(outbuf));

	updateGamePoints(firstPlayer);
	updateGamePoints(secondPlayer);

	updateGameCommands(firstPlayer);
}

void switchTurnsAndCheckIfDead (struct client *p, struct client *top) {
	char outbuf[256];
    struct client *tmp;
        		
    if (p->opponent->hitpoints > 0) {
        p->state = INACTIVE;
  		p->opponent->state = ACTIVE;
		updateGamePoints(p);
		updateGamePoints(p->opponent);
		updateGameCommands(p);
    }
    else {
        sprintf(outbuf, "%s gives up. You win!\n\n", p->opponent->name);
        write(p->fd, outbuf, strlen(outbuf));

        sprintf(outbuf, "You are no match for %s. You scurry away...\n\n", p->name);
        write(p->opponent->fd, outbuf, strlen(outbuf));
        tmp = p->opponent;
		searchForBattle(p, top);
		searchForBattle(tmp, top); 
    }	
}


void updateGamePoints(struct client *p) {

	char outbuf[256];
	sprintf(outbuf, "Your hitpoints: %i\nYour powermoves: %i\n\n%s's hitpoints:%i\n\n",
			p->hitpoints, p->numOfPowerMoves, p->opponent->name, p->opponent->hitpoints);
	write(p->fd, outbuf, strlen(outbuf));
}

void updateGameCommands(struct client *p) {
	if (p->state == ACTIVE) {
		char outbuf[256];
		write(p->fd, "(a)ttack\n(p)owermove\n(s)peak something\n", 40);

		sprintf(outbuf, "Waiting for %s to strike...\n", p->name);
		write(p->opponent->fd, outbuf, strlen(outbuf));

	}
	/* The opponent must be the active one*/
	else{
		updateGameCommands(p->opponent);
	}

}


int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];

    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';

        if (p->state == REGISTERING) {
        	if (strcmp(buf,"\n") == 0) {
        		sprintf(outbuf, "Welcome, %s! ", p->name);
        		write(p->fd, outbuf, strlen(outbuf));
        		sprintf(outbuf, "**%s enters the arena**\n", p->name);
        		broadcastToRegistered(top, outbuf, strlen(outbuf));

        		searchForBattle(p, top);

        	}
        	else {
        		strcat(p->name, buf);
        	}
        }

        /* The player is currently in a battle, so they have an opponent*/
        else if (p->state == ACTIVE) {
        	int damage;
        	if (strcmp(buf,"a") == 0) {
        		/*each attack takes off 2-6 hitpoints*/
        		damage = rand() % 5 + 2;
   				p->opponent->hitpoints -= damage;

    			sprintf(outbuf, "\n%s hits you for %i damage!\n", p->name, damage);
    			write(p->opponent->fd, outbuf, strlen(outbuf));

    			sprintf(outbuf, "\nYou hit %s for %i damage!\n", p->opponent->name, damage);
    			write(p->fd, outbuf, strlen(outbuf));

        		switchTurnsAndCheckIfDead (p, top);

    		}
    		/* perform a powermove only if you have powermoves left */
    		else if (strcmp(buf,"p") == 0 && p->numOfPowerMoves > 0) {
    			damage = 3 * (rand() % 5 + 2);
    			if (rand() % 2) {
    				p->opponent->hitpoints -= damage;
    				sprintf(outbuf, "\n%s powermoves you for %i damage!\n", p->name, damage);
    				write(p->opponent->fd, outbuf, strlen(outbuf));

    				sprintf(outbuf, "\nYou powermove %s for %i damage!\n", p->opponent->name, damage);
    				write(p->fd, outbuf, strlen(outbuf));
    			}
    			else {
    				sprintf(outbuf, "\n%s missed you!\n", p->name);
    				write(p->opponent->fd, outbuf, strlen(outbuf));

    				write(p->fd, "\nYou missed!\n", 13);

    			}
    			p->numOfPowerMoves --;
    			switchTurnsAndCheckIfDead (p, top);
    		}
    		else if (strcmp(buf,"s") == 0) {
    			p->state = WRITING;
    			write(p->fd, "\nSpeak: ", 7);
    			strcpy(p->msg, "");
    		}
        }

        else if (p->state == WRITING) {
        	if (strcmp(buf,"\n") == 0) {
        		sprintf(outbuf, "You speak: %s", p->msg);
        		write(p->fd, outbuf, strlen(outbuf));
        		sprintf(outbuf, "%s takes a break to tell you:\n%s\n\n", p->name, p->msg);
        		write(p->opponent->fd, outbuf, strlen(outbuf));

        		p->state = ACTIVE;
				updateGamePoints(p);
				updateGamePoints(p->opponent);
				updateGameCommands(p);

        	}
        	else {
        		strcat(p->msg, buf);
        	}
        }

        return 0;

    } else if (len == 0) {
        // socket is closed
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        if (p->state >= ACTIVE ) {
       		sprintf(outbuf, "--%s dropped. You win!\n\n", p->name);
        	write(p->opponent->fd, outbuf, strlen(outbuf));
        	searchForBattle(p->opponent, top);
    	}
        sprintf(outbuf, "**%s leaves**\n", p->name);
        broadcastToRegistered(top, outbuf, strlen(outbuf));

        return -1;
    } else { // shouldn't happen
        perror("read");
        return -1;
    }
}


void searchForBattle(struct client *p, struct client *top) {
	write(p->fd, "Awaiting opponent...\n", 21);
	struct client *p2;
	/*Check to see if there is a player that is waiting for a game*/
    for (p2=top; p2 != NULL; p2 = p2->next) {
        if (p2->state == WAITING && (p->opponent == NULL || p->opponent->fd != p2->fd)) {
        	startBattle(p2, p);
        	break;
        }
    }
    /* No player yet, so wait*/
    if (p2 == NULL) {
        p->state = WAITING;
    }
}



 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->state = REGISTERING;
    p->next = top;
    p->opponent = NULL;
    p->name[0] = '\0';
    p->msg[0] = '\0';
    top = p;
    return top;


}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}



static void broadcastToRegistered(struct client *top, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->state != REGISTERING && p->state != WRITING) {
        	write(p->fd, s, size);
        }
    }
    /* should probably check write() return value and perhaps remove client */
}



