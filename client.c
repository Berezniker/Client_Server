#include <stdio.h>		// printf(), getchar(), fprintf(), perror(), fflush()
#include <stdlib.h>		// atoi()
#include <string.h>		// strcmp(), strncmp(), strlen()
#include <ctype.h>		// isspace()
#include <unistd.h>		// write(), _exit(), close(), select(), FD_ZERO(), FD_SET(), FD_ISSET()
#include <signal.h>		// signal(), SIGINT, SIGTSTP
#include <netinet/in.h>	// htons(), htonl()
#include <sys/socket.h>	// socket(), connect(), shutdown(), send(), recv()
#include <sys/types.h>

#define MSG_LEN 1024	// длина сообщения
#define PORT 5000		// номер порта (должен совпадать с номером порта сервера)

int client_sockfd;		// дескриптор сокета клиента

/**
 * [readall: reads a message from the client into a string <string> of length <length>]
 * @return [0 in case of line overflow, otherwise the number of characters read]
 */
int readall(char *string, int length);

/**
 * [recvmsg: reads a message from the server into a string <message> of length <length>]
 */
void recvservermsg(char *message, int length);

/**
 * [systemoff: system shutdown]
 */
void systemoff(char *error_message);

/**
 * [sockoff: socket shutdown]
 */
void sockoff(int sockfd);

/**
 * [handler: SIGINT(<Ctrl+C>) and SIGTSTP(<Ctrl+Z>) signals handler]
 */
void handler(int signal_number);


int main(int argc, char *argv[])
{
	printf("[client] RUN\n");
	// устанавливаем обработку сигналов <Ctrl+C> и <Ctrl+Z>
	signal(SIGINT, handler);
	signal(SIGTSTP, handler);
	// считывание номера порта для подсоединения к серверу из командной строки:
	int addr = INADDR_LOOPBACK;	// для локальной сети
	int port = PORT;
	if (argc > 1) port = atoi(argv[1]);
	if (argc > 2) { fprintf(stderr, "too many arguments.\n"); _exit(-1); }
	// создание сокета
	if ( (client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("[client] socket()");
		_exit(-1);
	}
	// структура сервера
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(addr);
	// установление соединения
	if (connect(client_sockfd,(struct sockaddr *) &server, sizeof(server)) == -1) {
		perror("[client] connect()");
		close(client_sockfd);
		_exit(-1);
	}

	while (1) {
		fd_set readfds;							// множество читаемых дескрипторов
		char message[MSG_LEN];					// буфер-сообщения

		for (int i = 0; i < MSG_LEN; ++i)		// очищаем буфер-сообщения
			*(message + i) = '\0';

		FD_ZERO(&readfds);						// очищаем множество
		FD_SET(client_sockfd, &readfds);		// добавляем во множество дескриптор сокета
		FD_SET(STDIN_FILENO, &readfds);			// добавляем во множество дескриптор стандартного потока ввода

		if (select(client_sockfd + 1, &readfds, NULL, NULL, NULL) == -1)
			systemoff("[client] select()");

		if (FD_ISSET(client_sockfd, &readfds))	// пришло сообщение с сервера
			recvservermsg(message, MSG_LEN);

		if (FD_ISSET(STDIN_FILENO, &readfds))	// введено сообщение от клиента
			if (readall(message, MSG_LEN)) {
				if (send(client_sockfd, message, strlen(message) + 1, 0) == -1)
					systemoff("[client] send()");
				if (!strncmp(message, "\\quit", 5))
					break;	// выход из чата по команде \quit
			}

	} // end while(1)
	// закрываем дескриптор сокета
	sockoff(client_sockfd);
	printf("\r[client] End of run.\n");
	return 0;
}

int readall(char *str, int len)
{
	int i = 0;
	char c;
	// пропускаем все начальные пробельные символы
	while (isspace(c = getchar()));

	for (int spaceflag = 0; i < len && c != '\n'; c = getchar())
		if (isspace(c))
			spaceflag = 1; // пропускаем повторные пробелы между слов
		else {
			if (spaceflag)
				*(str + i++) = ' ';
			spaceflag = 0;
			*(str + i++) = c;
		}				

	// превышена максимальная длина сообщения
	if (i == len) {
		printf(  "###Message volume exceeded.\n"
				 "###Message not send\n"
				 "###Maximum message size = %d\n"
				 "###Try again\n", len);
		while ( (c = getchar()) != '\n');
	}
	else if (i < 78)				// \033[A - перемещение курсора вверх на одну строку, но в тот же столбец
		printf("\033[A\33[2K\r");	// \33[2K - стирает всю строку, на которой курсор находится в данный момент
									// \r     - переводит курсор в начало строки
	// печатаем приглашение к вводу
	printf(">> ");
	fflush(stdout);

	return i == len ? 0 : i;
}

void recvservermsg(char *msg, int len)
{
	printf("\r   \r"); // стираем приглашение к вводу - ">> "
	fflush(stdout);
	int nrecv = -1;
	do {
		if ( (nrecv = recv(client_sockfd, msg, len, 0)) == -1)
			systemoff("[client] recv()");
		if (write(STDOUT_FILENO, msg, nrecv) == -1)
			systemoff("[client] write()");
	} while (nrecv == len);

	if (!strcmp(msg, "\\exit") || !strcmp(msg + strlen(msg) - 5, "\\exit") || !strcmp(msg + strlen(msg) + 1, "\\exit")) {
		// закрываем дескриптор сокета
		sockoff(client_sockfd);
		printf("\r[client] End of run.\n");
		_exit(0); // сервер завершился => завершение работы клиента
	}
	// печатаем приглашение к вводу
	printf(">> ");
	fflush(stdout);
}

void systemoff(char *errmsg)
{
	// выводим сообщение об ошибке
	if (errmsg) perror(errmsg);
	// маячок для сервера о завершении работы клиента
	if (send(client_sockfd, "\\quit", 6, 0) == -1)
		perror("[client] send()");
	// закрываем дескриптор сокета
	sockoff(client_sockfd);
	_exit(-1);
}

void sockoff(int sockfd)
{
	if (shutdown(sockfd, SHUT_RDWR) == -1)
		perror("[client] shutdown()");
	if (close(sockfd) == -1)
		perror("[client] close()");
}

void handler(int signum)
{
	// сообщаем о приходе сигнала на сервер
	fprintf(stderr, "\r###%s signal came\n", signum == SIGINT ? "SIGINT" : "SIGTSTP");
	// завершаем работу клиента
	systemoff(NULL);
}