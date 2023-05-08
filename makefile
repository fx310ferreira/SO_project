#  Frederico Ferreira 2021217116
#  Nuno Carvalho do Nascimento 2020219249

all: sensor home_iot user_console

sensor: sensor.c
	gcc sensor.c -Wall -Wextra -pthread -g -o sensor

home_iot: sys_manager.c 
	gcc sys_manager.c -Wall -Wextra -pthread -g  -o home_iot 

user_console: user_console.c 
	gcc user_console.c -Wall -Wextra -pthread -g -o user_console
