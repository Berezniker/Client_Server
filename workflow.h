#ifndef WORKFLOW_H
#define WORKFLOW_H

#include "database.h"

/* ------------------------------------------------ */
/* ------------------- WORKFLOW ------------------- */
/* ------------------------------------------------ */

#define MSG_LEN 1024	// message length

/**
 * [acceptclient: establishment of connection with new client]
 */
void acceptclient(void);

/**
 * [registration: user registration in database]
 */
void registration(struct database *user);

/**
 * [exit_client: client <user> exit from the system and prints <end_message>]
 */
void exit_client(struct database *user, char *end_message);

/**
 * [recvmsgfrom: receive message from user]
 */
void recvmsgfrom(struct database *user);

/**
 *  [sendtoall: sends message to all users in database]
 */
void sendtoall(char *message1, ...);

/**
 * [gettime: writes to string <time> the current time in the format "HH:MM"]
 */
char *gettime(char *time);

/**
 * [sendprivatemsg: send private message from user <from_user>]
 */
void sendprivatemsg(char *message, struct database *from_user);

/**
 * [send_help_info: send helpful information to user]
 */
void send_help_info(struct database *user);

/**
 * [clear_channel: reads all the characters from the channel]
 */
void clear_channel(int channelfd);

/**
 * [empty: check if the <message> is empty]
 * @return [1 in case the message is empty, 0 - otherwise]
 */
int empty(char *message, int length);



/* ----------------------------------------------- */
/* -------------------- ADMIN -------------------- */
/* ----------------------------------------------- */

#define PASSWORD "PSWD"	// admin password
#define KICK 0
#define BAN 1

/**
 * [assign_admin: requests an administrator password from a <user>]
 */
void assign_admin(struct database *user);

/**
 * [checkpassword: checks the administrator password entered by the <user>]
 */
void checkpassword(struct database *user, char *password);

/**
 * [adm_kick: disconnect user from server, displaying the cause of the kick
 *            if flag == BAN - adds a username to the banlist               ]
 */
void adm_kick(struct database *admin, char *command, int flag);

/**
 * [changenick: for user : changes nickname to <command>
 *              for admin: changes user nickname         ]
 */
void changenick(struct database *user, char *command);

/**
 * [admshutdown: shutting down the server and sending a <end_message> to all users]
 */
void adm_shutdown(struct database *admin, char *end_message);



/* ------------------------------------------------ */
/* -------------------- SYSTEM -------------------- */
/* ------------------------------------------------ */

DATABASE database;		// customer database
int listener;			// дескриптор слушающего сокета

/**
 * [sockoff: socket shutdown]
 */
void sockoff(int sockfd);

/**
 * [systemoff: emergency server shutdown]
 */
void systemoff(char *error_message);

/**
 * [correct_shutdown: correct server shutdown]
 */
void correct_shutdown(void);

/**
 * [handler: SIGINT(<Ctrl+C>) and SIGTSTP(<Ctrl+Z>) signals handler]
 */
void handler(int signum);

#endif