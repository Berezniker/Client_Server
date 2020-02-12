#include <stdio.h>		// fprintf()
#include <stdlib.h>		// malloc(), free()
#include <string.h>		// strcmp(), strcpy(), strcat()
#include <sys/types.h>
#include <sys/socket.h>	// send()
#include "database.h"

extern void sockoff(int sockfd);
extern void systemoff(char *error_message);

void db_client_add(LIST *user, int client_sockfd)
{
	LISTNODEPTR curPtr = *user;

	if (*user == NULL) {
		curPtr = *user = (LISTNODEPTR) malloc(sizeof(LISTNODE));
		if (curPtr == NULL)
			systemoff("[server] malloc()");
	} else {
		while (curPtr->nextPtr != NULL)
			curPtr = curPtr->nextPtr;

		curPtr = curPtr->nextPtr = (LISTNODEPTR) malloc(sizeof(LISTNODE));
		if (curPtr == NULL)
			systemoff("[server] malloc()");
	}
	// инициализация полей структуры клиента:
	for (int i = 0; i < NAME_SIZE + 1; ++i)
		curPtr->name[i] = '\0';
	curPtr->sockfd = client_sockfd;
	curPtr->type = USR;
	curPtr->private_to = NULL;
	curPtr->private_from = NULL;
	curPtr->nextPtr = NULL;
}


void db_private_add(PRIVATELIST *prv, char *private_name)
{
	PRIVATELIST curPtr = *prv;

	if (curPtr == NULL) {
		curPtr = *prv = (PRIVATELIST) malloc(sizeof(struct list));
		if (curPtr == NULL)
			systemoff("[server] malloc()");
	} else {
		while (curPtr->nextPtr != NULL) {
			if (!strcmp(curPtr->name, private_name)) return;
			curPtr = curPtr->nextPtr;
		}
		if (!strcmp(curPtr->name, private_name)) return;
		curPtr = curPtr->nextPtr = (PRIVATELIST) malloc(sizeof(struct list));
		if (curPtr == NULL)
			systemoff("[server] malloc()");
	}
	// переписываем имя в приватный список
	for (int i = 0; curPtr->name[i] = private_name[i]; ++i);
	// инициализация полей структуры
	curPtr->nextPtr = NULL;
}


void db_client_del(LIST *user, int client_sockfd)
{
	LISTNODEPTR curPtr = *user, prevPtr = curPtr;

	while (curPtr != NULL)
		if (curPtr->sockfd == client_sockfd) {
			if (curPtr == *user)
				*user = (*user)->nextPtr;
			else 
				prevPtr->nextPtr = curPtr->nextPtr;
			// закрываем дескриптор сокета клиента
			sockoff(curPtr->sockfd);
			// отчищаем приватные списки клиента
			db_clear_private(&curPtr->private_to);
			db_clear_private(&curPtr->private_from);
			// освобождаем память
			free(curPtr);
			// уведомляем на сервере о прекращении соединения
			printf("###[%d]connection terminated\n", client_sockfd);
			return;
		} else {
			prevPtr = curPtr;
			curPtr = curPtr->nextPtr;
		}

	fprintf(stderr, "###The socket file descriptor %d is not registered in the database.\n", client_sockfd);
}


void db_clear(LIST *db)
{
	LISTNODEPTR tempPtr = *db;
	// отчищаем чёрный список
	db_clear_private(&banlist);
	while (*db != NULL) {
		tempPtr = *db;
		*db = (*db)->nextPtr;
		// закрываем дескриптор сокета клиента
		sockoff(tempPtr->sockfd);
		// отчищаем приватные списки
		db_clear_private(&tempPtr->private_to);
		db_clear_private(&tempPtr->private_from);
		// освобождаем память
		free(tempPtr);
	}
}


void db_clear_private(PRIVATELIST *prv)
{
	PRIVATELIST tempPtr = *prv;

	while (*prv != NULL) {
		tempPtr = *prv;
		*prv = (*prv)->nextPtr;
		// освобождаем память
		free(tempPtr);
	}
}


int db_check_uniq(LIST user, char *newname)
{
	// проверка имени в чёрном списке
	if (isban(newname)) return -1;
	// проверка совпадения имени с текущими пользователями
	for (; user != NULL; user = user->nextPtr)
		if (!strcmp(user->name, newname))
			return user->sockfd;

	return 0;
}


int isban(char *checkname)
{
	for (BANLIST bl = banlist; bl != NULL; bl = bl->nextPtr)
		if (!strcmp(bl->name, checkname))
			return 1;

	return 0;
}


LISTNODEPTR db_get_user(LIST user, char *username)
{
	for (; user != NULL; user = user->nextPtr)
		if (!strcmp(user->name, username))
			return user;

	return NULL;
}

void db_send_all(LIST user, int sockfd)
{
	char hand[] = "\n----- online -----\n";
	if (send(sockfd, hand, sizeof(hand), 0) == -1)
		systemoff("[server] send()");

	if (user == NULL) {
		char nocust[] = "(no customers)\n";
		if (send(sockfd, nocust, sizeof(nocust), 0) == -1)
			systemoff("[server] send()");
	} else {
		for (; user != NULL; user = user->nextPtr) {
			char usr[NAME_SIZE + 10] = "\0";
			if (user->name[0] == '\0')
				strcat(usr, "(registration)");
			else
				strcat(usr, user->name);
			if (user->type == ADM)
				strcat(usr, " (admin)");
			strcat(usr, "\n\0");
			if (send(sockfd, usr, sizeof(usr), 0) == -1)
				systemoff("[server] send()");
		}
	}
	char end[] = "------------------\n\n";
	if (send(sockfd, end, sizeof(end), 0) == -1)
		systemoff("[server] send()");
}


void db_send_private(LISTNODEPTR user)
{
	char hand[] = "\n----- private -----\n";
	if (send(user->sockfd, hand, sizeof(hand), 0) == -1)
		systemoff("[server] send()");

	if (user->private_to == NULL && user->private_from == NULL) {
		char nocust[] = "(no customers)\n";
		if (send(user->sockfd, nocust, sizeof(nocust), 0) == -1)
			systemoff("[server] send()");
	} else {
		char to[] = "to:\n";
		if (send(user->sockfd, to, sizeof(to), 0) == -1)
			systemoff("[server] send()");
		for (PRIVATELIST prv = user->private_to; prv != NULL; prv = prv->nextPtr) {
			char usr[NAME_SIZE + 2] = "\0";
			strcat(usr, prv->name);
			strcat(usr, "\n\0");
			if (send(user->sockfd, usr, sizeof(usr), 0) == -1)
				systemoff("[server] send()");
		}

		char from[] = "\nfrom:\n";
		if (send(user->sockfd, from, sizeof(from), 0) == -1)
			systemoff("[server] send()");
		for (PRIVATELIST prv = user->private_from; prv != NULL; prv = prv->nextPtr) {
			char usr[NAME_SIZE + 2] = "\0";
			strcat(usr, prv->name);
			strcat(usr, "\n\0");
			if (send(user->sockfd, usr, sizeof(usr), 0) == -1)
				systemoff("[server] send()");
		}
	}
	char end[] = "-------------------\n\n";
	if (send(user->sockfd, end, sizeof(end), 0) == -1)
		systemoff("[server] send()");
}