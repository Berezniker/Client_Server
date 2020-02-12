#include <stdio.h>		// printf(), fprintf(), perror()
#include <string.h>		// strcmp()
#include <stdlib.h>		// atoi()
#include <unistd.h>		// read(), _exit(), select(), FD_ZERO(), FD_SET(), FD_ISSET()
#include <signal.h>		// signal(), SIGINT, SIGTSTP
#include <netinet/in.h>	// htons(), htonl()
#include <sys/socket.h>	// socket(), setsockopt(), bind(), listen()
#include <sys/types.h>
#include "database.h"
#include "workflow.h"

#define PORT 5000		// номер порта
#define QUEUE_LEN 5		// длина очереди в listen()

int main(int argc, char *argv[])
{
	printf("[server] RUN\n");
	// устанавливаем обработку сигналов <Ctrl+C> и <Ctrl+Z>
	signal(SIGINT, handler);
	signal(SIGTSTP, handler);

	// считывание номера порта сервера из командной строки:
	int port = PORT;
	if (argc > 1) port = atoi(argv[1]);
	if (argc > 2) { fprintf(stderr, "too many arguments.\n"); _exit(-1); }
	printf("port number: %d\n", port);

	// создаём неблокирующийся слушающий сокет 
	if ( (listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
		perror("[server] socket()");
		_exit(-1);
	}

	struct sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(port);
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// для избежания "залипания" TCP-порта по завершению сервера:
	int opt = 1;
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		perror("[server] setsockopt()");
	// связывание сокета с адресом
	if (bind(listener, (struct sockaddr *) &local_addr, sizeof(local_addr)) == -1)
		systemoff("[server] bind()");

	// ожидание и приём клиентских соединений
	if (listen(listener, QUEUE_LEN) == -1)
		systemoff("[server] listen()");

	while(1) {
		fd_set readfds;					// множество читаемых дескрипторов
		int max_sfd = listener;

		FD_ZERO(&readfds);				// очищаем множество
		FD_SET(listener, &readfds);		// добавляем во множество дескриптор слушающего 
		FD_SET(STDIN_FILENO, &readfds);	// добавляем во множество дескриптор стандартного потока ввода

		// добавляем во множество дескрипторы сокетов клиентов:
		for (struct database *user = database; user != NULL; user = user->nextPtr) {
			FD_SET(user->sockfd, &readfds);
			if (user->sockfd > max_sfd)
				max_sfd = user->sockfd; 
		}

		if (select(max_sfd + 1, &readfds, NULL, NULL, NULL) == -1)
			systemoff("[server] select()");

		if (FD_ISSET(STDIN_FILENO, &readfds)) {	// введена команда на сервере
			char cmd[MSG_LEN] = "\0";
			if (read(STDIN_FILENO, cmd, sizeof(cmd)) == -1)
				systemoff("[server] read()");

			int i = 0;
			for (; *(cmd + i) == ' '; ++i); // пропускаем пробелы

			if (!strcmp(cmd + i, "\\exit\n")) {
				sendtoall("###server is shutting down, thanks to everyone.\n", NULL);
				sendtoall("\\exit", NULL);
				correct_shutdown();
			}
			else if (!empty(cmd, MSG_LEN))
				printf("###Unknow command\n");
		}

		if (FD_ISSET(listener, &readfds))	// пришел новый запрос на соединение
			acceptclient();
		
		for (struct database *user = database; user != NULL; user = user->nextPtr)
			if (FD_ISSET(user->sockfd, &readfds)) {
				if (user->name[0] == '\0')
					registration(user);
				else
					recvmsgfrom(user);
			}
	} // end while(1)	
}