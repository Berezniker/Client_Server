#ifndef DATABASE_H
#define DATABASE_H

#define NAME_SIZE 12			// максимальная длина имени пользователя

struct list {
	char name[NAME_SIZE + 1];	// имя {приватного / заблокированного} пользователя 
	struct list *nextPtr;
};

typedef struct list *PRIVATELIST;
typedef struct list *BANLIST;

BANLIST banlist;				// список заблокированных имен

typedef enum {USR, CHK, ADM} user_type;

struct database {
	char name[NAME_SIZE + 1];	// имя пользователя
	int sockfd;					// дескриптор сокета
	user_type type;				// тип пользователя: обыкновенный - USR, администратор - ADM
	struct database *nextPtr;
	PRIVATELIST private_to;		// список пользователей, получивших приватное сообщение
	PRIVATELIST private_from;	// список пользователей, отправлявших приватные сообщения
};

typedef struct database LISTNODE;
typedef LISTNODE *LISTNODEPTR;
typedef LISTNODEPTR LIST;
typedef LIST DATABASE;

/**
 * [db_client_add: adds client to database]
 */
void db_client_add(LIST *database, int client_sockfd);

/**
 * [db_private_add: adds private client to the user's private list]
 */
void db_private_add(PRIVATELIST *prvlst, char *private_name);

/**
 * [db_client_del: deletes client from database]
 */
void db_client_del(LIST *database, int client_sockfd);

/**
 * [db_clear: clears database]
 */
void db_clear(LIST *database);

/**
 * [db_clear_private: clears the list of private clients]
 */
void db_clear_private(PRIVATELIST *private_list);

/**
 * [db_check_uniq: checks the uniqueness of the <newclient_name> in the <database>]
 * @return [0 in case this name is unique, otherwhise - descriptor of the client's socket with the same name]
 */
int db_check_uniq(LIST database, char *newclient_name);

/**
 * [isban: checks the <checkname> in the banlist]
 * @return [1 in case this name in banlist, otherwhise - 0]
 */
int isban(char *checkname);

/**
 * [db_get_user: search for a user in the <database> by his <username>]
 * @return [the structure of the user]
 */
LISTNODEPTR db_get_user(LIST database, char *username);

/**
 * [db_send_all: sends the list of users in the <database> who are online to client with deskriptor <to_client_sokfd>]
 */
void db_send_all(LIST database, int to_client_sockfd);

/**
 * [db_send_private: sends the private list of user to <to_user>]
 */
void db_send_private(LISTNODEPTR to_user);

#endif