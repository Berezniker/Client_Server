#include <stdio.h>		// printf(), fprintf(), perror()
#include <string.h>		// strlen(), strcmp(), strncmp(), strcpy(), strcat()
#include <ctype.h>		// isalpha()
#include <time.h>		// time(), localtime(), strftime()
#include <sys/types.h>
#include <sys/socket.h>	// send(), recv(), shutdown(), SHUT_RDWR
#include <unistd.h>		// accept(), closer(), _exit()
#include <signal.h>		// signal(), SIGINT, SIGTSTP
#include <fcntl.h>		// fcntl()
#include <stdarg.h>		// va_list, va_start(), va_arg()
#include "database.h"	// struct database, db_...()
#include "workflow.h"

/* ------------------------------------------------ */
/* ------------------- WORKFLOW ------------------- */
/* ------------------------------------------------ */

void acceptclient(void)
{
	// устанавливаем соединение с клиентом
	int client_sockfd = accept(listener, NULL, NULL);
	if (client_sockfd == -1)
		systemoff("[server] accept()");

	// устанавливаем неблокирующий дескриптор сокета
	if (fcntl(client_sockfd, F_SETFL, O_NONBLOCK) == -1)
		systemoff("[server] fcntl()");

	// добавляем клиента в базу данных
	db_client_add(&database, client_sockfd);
	printf("###[%d]connection established\n", client_sockfd);

	char invite[] = "###Connection established.\n"
					"     WELCOME!\n"
					"###Maximum name length is 12.\n"
					"###The name can consist only of letters.\n"
					"Enter the name:\n";
	if (send(client_sockfd, invite, sizeof(invite), 0) == -1)
		systemoff("[server] send()");
}

void registration(struct database *user)
{
	char username[NAME_SIZE + 1] = "\0";
	// получаем имя пользователя
	int nrecv = -1;
	if ( (nrecv = recv(user->sockfd, username, NAME_SIZE, 0)) == -1)
		systemoff("[server] recv()");
	// игнорируем оставшиеся в канале символы
	if (nrecv == NAME_SIZE)
		clear_channel(user->sockfd);

	// маячок. клиент завершил программу сигналом до регистрации
	if (!strcmp(username, "\\quit")) {
		db_client_del(&database, user->sockfd);
		return;
	}
	// имя начинается не с буквы
	if (!isalpha(username[0])) {
		char err[] = "###invalid name\n"
					 "Enter the name:\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}
	// вырезаем имя
	for (int i = 0; i < NAME_SIZE; ++i)
		if (!isalpha(username[i]))
			for (; i < NAME_SIZE; ++i)
				username[i] = '\0';
	// проверка имени на пустоту
	if (empty(username, NAME_SIZE)) {
		char reinv[] = "Enter the name:\n";
		if (send(user->sockfd, reinv, sizeof(reinv), 0) == -1)
			systemoff("[server] send()");
		return;
	}

	// проверка имени на уникальность и наличие его в чёрном списке (banlist)
	if (db_check_uniq(database, username)) {
		char reinv[] =  "The name is taken. Try again.\n"
						"Enter the name:\n";
		if (send(user->sockfd, reinv, sizeof(reinv), 0) == -1)
			systemoff("[server] send()");
		return;
	}
	// записываем имя пользователя в его структуру данных
	for (int i = 0; i < NAME_SIZE && username[i] != '\0'; ++i)
		user->name[i] = username[i];

	char msg[] = "Registration completed successfully\n"
				 "For further information use '\\help'\n";
	if (send(user->sockfd, msg, sizeof(msg), 0) == -1)
		systemoff("[server] send()");

	// уведомление о входе нового пользователя:
	sendtoall("***User '", user->name, "' entered\n", NULL);
}

void exit_client(struct database *user, char *msg)
{
	char tempname[NAME_SIZE] = "\0";
	strcat(tempname, user->name);
	if (msg[5] == ' ') {	// прощальное сообщение
		char timer[7] = "\0";
		sendtoall(user->name, "[", gettime(timer), "]: ", msg + 6, "\n", NULL);
	}
	db_client_del(&database, user->sockfd);
	sendtoall("***User '", tempname, "' is out.\n", NULL);
}

void recvmsgfrom(struct database *user)
{
	char message[MSG_LEN] = "\0";
	int nrecv = -1;
	// получаем сообщение от клиента
	if ( (nrecv = recv(user->sockfd, message, MSG_LEN, 0)) == -1)
		systemoff("[server] recv()");
	// проверка на пустоту
	if (empty(message, MSG_LEN)) return;
	// пользователь в режиме ввода пароля
	if (user->type == CHK) {
		checkpassword(user, message);
		// очищаем канал
		if (nrecv == MSG_LEN)
			clear_channel(user->sockfd);
		return;
	}
	// команда
	if (message[0] == '\\') {
		int fail = 0;
		if (!strcmp(message, "\\help"))
			send_help_info(user);
		else if (!strcmp(message, "\\user"))
			db_send_all(database, user->sockfd);
		else if (!strcmp(message, "\\privates"))
			db_send_private(user);
		else if (!strcmp(message, "\\private") || !strncmp(message, "\\private ", 9))
			sendprivatemsg(message, user);
		else if (!strcmp(message, "\\quit") || !strncmp(message, "\\quit ", 6))
			exit_client(user, message);
		else if (!strcmp(message, "\\admin"))
			assign_admin(user);
		else if (!strcmp(message, "\\nick") || !strncmp(message, "\\nick ", 6))
			changenick(user, message);
		else if (!strcmp(message, "\\kick") || !strncmp(message, "\\kick ", 6))
			adm_kick(user, message, KICK);
		else if (!strcmp(message, "\\ban") || !strncmp(message, "\\ban ", 5))
			adm_kick(user, message, BAN);
		else if (!strcmp(message, "\\shutdown") || !strncmp(message, "\\shutdown ", 10))
			adm_shutdown(user, message);
		else {
			char err[] = "###Unknow command\n";
			if (send(user->sockfd, err, sizeof(err), 0) == -1)
				systemoff("[server] send()");
			fail = 1;
		}
		if (!fail) return;
	}
	// обычное сообщение
	char timer[7] = "\0";
	sendtoall(user->name, " [", gettime(timer), "]: ", message, "\n", NULL);
}

void sendtoall(char *msg, ...)
{
	va_list argPtr;
	char *temp = msg;
	int msglen = 0;

	for (va_start(argPtr, msg); temp != NULL; temp = va_arg(argPtr, char *))
		msglen += strlen(temp);
	
	char buf[msglen + 1];
	for (int i = 0; i < msglen + 1; ++i)
		*(buf + i) = '\0';

	for (temp = msg, va_start(argPtr, msg); temp != NULL; temp = va_arg(argPtr, char *))
		strcat(buf, temp);

	for (struct database *user = database; user != NULL; user = user->nextPtr)
		if (send(user->sockfd, buf, msglen + 1, 0) == -1)
			systemoff("[server] send()");
}

char *gettime(char *tm)
{
	const time_t sec = time(NULL);
	struct tm *stm = localtime(&sec);
	strftime(tm, 7, "%H:%M", stm);
	return tm;
}

void sendprivatemsg(char *msg, struct database *from_user)
{
	char prvtto[NAME_SIZE] = "\0";
	int j = 9; // '\private '

	if (!strcmp(msg, "\\private")) {
		char err[] = "###Missing private recipient.\n";
		if (send(from_user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}

	for (int i = 0; i < NAME_SIZE && msg[j] != ' ' && msg[j] != '\0'; ++j, ++i)
		prvtto[i] = msg[j];

	if (!strcmp(from_user->name, prvtto)) {
		char err[] = "###You cann't send a private message to yourself.\n";
		if (send(from_user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}

	if (msg[j] == '\0') {
		char err[] = "###Missing private message.\n";
		if (send(from_user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}

	int prvtfd =  db_check_uniq(database, prvtto);
	if (prvtfd == 0 || prvtfd == -1) {
		char err[] = "###This client isn't online.\n";
		if (send(from_user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}
	// добавляем пользователей в приватные списки обоих
	db_private_add(&from_user->private_to, prvtto);
	struct database *user_to = db_get_user(database, prvtto);
	db_private_add(&user_to->private_from ,from_user->name);

	char prvtmsg[MSG_LEN] = "\0";
	char timer[7];
	// склеиваем и отправляем сообщение
	strcat(prvtmsg, from_user->name);
	strcat(prvtmsg, " [");
	strcat(prvtmsg, gettime(timer));
	strcat(prvtmsg, "]: *");
	strcat(prvtmsg, msg + j + 1);
	strcat(prvtmsg, "\n");
	if (send(prvtfd, prvtmsg, strlen(prvtmsg) + 1, 0) == -1)
		systemoff("[server] send()");
}

void send_help_info(struct database *user)
{
	char info[] =	"\n"
					" [CHAT 3]\n"
					"\n"
					"   ~~message formatting rules:\n"
					"  <user> [<time>]: <message>\n"
					"  The maximum size of a message is 1000 characters\n"
					"  private message starts with *\n"
					"  channel event notification starts with ***\n"
					"  service message starts with ###\n"
					"\n"
					"   ~~available commands:\n"
					"  \\user                         -- print a list of users who are online now\n"
					"  \\quit <message>               -- exit from chat with a farewell <message>\n"
					"  \\private <nickname> <message> -- send a private <message> to <nickname>\n"
					"  \\privates                     -- print usernames to which you sent private\n"
					"                                    messages\n"
					"  \\nick <newnickname>           -- change your nickname to <newnickname>\n"
					"  \\help                         -- output valid commands\n"
					"  \\admin                        -- administrator rights request\n"
					"                                    (you need to know the password)\n"
					"\n"
					"   ~~commands for administrators:\n"
					"  \\ban <nickname> <message>     -- disables the user <nickname> and denies new\n"
					"                                    connections to the server with such names,\n"
					"                                    while the user is shown the reason for his\n"
					"                                    blocking - <message>\n"
					"  \\kick <nickname> <message>    -- disables the user <nickname>, while the user\n"
					"                                    is shown the reason for his blocking\n"
					"  \\nick <oldnick> <newnick>     -- force user name change\n"
					"  \\shutdown <message>           -- shuts down the server, and <message> is sent\n"
					"                                    to all users\n"
					"\n";

	if (send(user->sockfd, info, sizeof(info), 0) == -1)
		systemoff("[server] send()");
}

void clear_channel(int chnlfd)
{
	int nrecv = -1;
	int size = 100;
	char grbg[size];

	do {
		if ( (nrecv = recv(chnlfd, grbg, size, 0)) == -1)
			systemoff("[server] recv()");
	} while (nrecv == size);
}

int empty(char *msg, int len)
{
	for (int i = 0; i < len; ++i)
		if (*(msg + i) != '\0' && *(msg + i) != ' ' && *(msg + i) != '\n')
			return 0;
	return 1;
}



/* ----------------------------------------------- */
/* -------------------- ADMIN -------------------- */
/* ----------------------------------------------- */

void assign_admin(struct database *user)
{
	if (user->type == ADM) {
		char err[] = "###You are already an administrator.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
	} else {
		user->type = CHK;
		char pswd[] = "Enter the admin's password:\n";
		if (send(user->sockfd, pswd, sizeof(pswd), 0) == -1)
			systemoff("[server] send()");
	}
}

void checkpassword(struct database *user, char *pswd)
{
	// маячок. клиент завершил программу сигналом
	if (!strcmp(pswd, "\\quit")) {
		db_client_del(&database, user->sockfd);
		return;
	}
	// проверка пароля
	if (!strcmp(PASSWORD, pswd)) {
		user->type = ADM;
		sendtoall("***User '", user->name, "' is admin.\n", NULL);
	} else {
		user->type = USR;
		char err[] = "###Wrong password!\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		sendtoall("***User '", user->name, "' tried to hack server.\n", NULL);
	}
}

void adm_kick(struct database *user, char *cmd, int flag)
{
	char kickname[NAME_SIZE] = "\0";
	int j = 6 - flag; // '\kick ', '\ban '
	// проверка прав администратора у пользователя
	if (user->type != ADM) {
		char err[] = "###Сommand is available only to the administrator.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}
	// проверка параметров команды
	if (!strcmp(cmd, "\\kick")) {
		char err[] = "###Username missing.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}
	// вырезаем имя пользователя из команды
	for (int i = 0; i < NAME_SIZE && cmd[j] != ' ' && cmd[j] != '\0'; ++j, ++i)
		kickname[i] = cmd[j];
	// получаем структуру пользователя из базы данных
	struct database *kickuser = db_get_user(database, kickname);

	if (kickuser == NULL) {
		char err[] = "###This client isn't online.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}

	if (!strcmp(kickuser->name, user->name)) {
		char err[] = "###You cann't kick yourself.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}

	if (kickuser->type == ADM) {
		char err[] = "###You can not affect other admins.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;

	}
	// отправляем пользователю причину блокировки
	char kickmsg[MSG_LEN] = "\0";
	strcat(kickmsg, "###You are kicked by the administrator '");
	strcat(kickmsg, user->name);
	strcat(kickmsg, "'... ");
	strcat(kickmsg, cmd + j);
	strcat(kickmsg, "\n");
	if (send(kickuser->sockfd, kickmsg, strlen(kickmsg) + 1, 0) == -1)
		systemoff("[server] send()");
	// сигнал пользователю для завершения работы
	if (send(kickuser->sockfd, "\\exit", 6, 0) == -1)
		systemoff("[server] send()");
	// удаляем пользователя из базы данных
	db_client_del(&database, kickuser->sockfd);
	// уведомляем всех о случившемся
	if (flag == KICK)
		sendtoall("***User '", kickname, "' is kicked.\n", NULL);
	else { // flag == BAN => добавляем имя в черный список
		sendtoall("***User ", kickname, "' is banned.\n", NULL);
		db_private_add(&banlist, kickname);
	}
}

void changenick(struct database *user, char *cmd)
{
	char newnickname[NAME_SIZE + 1] = "\0";
	int j = 6; // '\nick '

	if (!strcmp(cmd, "\\nick"))	{
		char err[] = "###Missing new nickname.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}

	for (int i = 0; i < NAME_SIZE && isalpha(cmd[j]); ++j, ++i)
		newnickname[i] = cmd[j];
	for (; cmd[j] != ' ' && cmd[j] != '\0'; ++j);

	if (cmd[j++] == '\0') {
		if (db_check_uniq(database, newnickname) || newnickname[0] == '\0') {
			char err[] = "###This nickname is not available.\n";
			if (send(user->sockfd, err, sizeof(err), 0) == -1)
				systemoff("[server] send()");
		} else {
			sendtoall("***User '", user->name, "' has change his nickname to '", newnickname, "'\n", NULL);
			for (int i = 0; i < NAME_SIZE; ++i)
				user->name[i] = newnickname[i];
		}
		return;
	}
	// проверка прав администратора у пользователя
	if (user->type != ADM) {
		char err[] = "###Сommand is available only to the administrator.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}
	// другая структура команды -> переписываем
	char oldnickname[NAME_SIZE + 1] = "\0";
	strcpy(oldnickname, newnickname);
	if (!db_check_uniq(database, oldnickname)) {
		char err[] = "###1This nickname is not available.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
		return;
	}
	// очищаем буфер
	for (int i = 0; i < NAME_SIZE; ++i)
		newnickname[i] = '\0';
	// вырезаем новый никнейм для пользователя <oldnickname>
	for (int i = 0; i < NAME_SIZE && isalpha(cmd[j]); ++i, ++j)
		newnickname[i] = cmd[j];
	if (db_check_uniq(database, newnickname) || newnickname[0] == '\0') {
		char err[] = "###2This nickname is not available.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
	} else {
		struct database *newnk = db_get_user(database, oldnickname);
		if (newnk->type == ADM) {
			char err[] = "###You can not affect other admins.\n";
			if (send(user->sockfd, err, sizeof(err), 0) == -1)
				systemoff("[server] send()");
		} else {
			for (int i = 0; i < NAME_SIZE; ++i)
				newnk->name[i] = newnickname[i];
			sendtoall("***Admin '", user->name, "' changed the name of the user '", oldnickname,"' to '", newnickname, "'\n", NULL);
		}

	}
}

void adm_shutdown(struct database *user, char *cmd)
{
	// проверка прав администратора у пользователя
	if (user->type != ADM) {
		char err[] = "###Сommand is available only to the administrator.\n";
		if (send(user->sockfd, err, sizeof(err), 0) == -1)
			systemoff("[server] send()");
	} else {
		// уведомляем всех о причине завершения сервера
		if (!empty(cmd + 10, MSG_LEN - 10))
			sendtoall("***", cmd + 10, "\n", NULL);
		// сигнал всем пользователям для завершения работы
		sendtoall("\\exit", NULL);
		// завершаем работу сервера
		correct_shutdown();
	}
}



/* ------------------------------------------------ */
/* -------------------- SYSTEM -------------------- */
/* ------------------------------------------------ */

void sockoff(int sockfd)
{
	if (shutdown(sockfd, SHUT_RDWR) == -1)
		perror("[server] shutdown()");
	if (close(sockfd) == -1)
		perror("[server] close()");
}

void systemoff(char *errmsg)
{
	// выводим сообщение об ошибке
	if (errmsg) perror(errmsg);
	sendtoall("###emergency server shutdown\n", NULL);
	// сигнал всем пользователям для завершения работы
	sendtoall("\\exit", NULL);
	// отчищаем базу данных
	db_clear(&database);
	// закрываем дескриптор слущающего сокета
	sockoff(listener);
	_exit(-1);
}

void correct_shutdown(void)
{
	// отчищаем базу данных
	db_clear(&database);
	// закрываем дескриптор слущающего сокета
	sockoff(listener);
	printf("[server] End of run.\n");
	_exit(0);
}

void handler(int signum)
{
	// сообщаем о приходе сигнала на сервер
	fprintf(stderr, "\r###%s signal came\n", signum == SIGINT ? "SIGINT" : "SIGTSTP");
	// завершаем работу сервера
	systemoff(NULL);
}