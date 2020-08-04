# Chat
 
1. обработка сигналов < Ctrl+C >, < Ctrl+Z >  
2. команды клиента: \help  
3. команды сервера: \exit  
  

Компиляция сервера: make server  
Компиляция клиента: make client  

Запуск сервера: make create  
Запуск сервера с valgrind: make valsrv  
Запуск клиента: make connect  
Запуск клиента с valgrind: make valcln  

Запуск с задание параметров в командной строке:  
1) для сервера  
./server < number_of_port >  
2) для клиента  
./client < number_of_port >
