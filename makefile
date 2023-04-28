#  Frederico Ferreira 2021217116
#  Nuno Carvalho do Nascimento 2020219249

all: sensor home_iot user_console

sensor: sensor.c functions.c
	gcc sensor.c functions.c -Wall -Wextra -pthread -g -o sensor

home_iot: sys_manager.c functions.c
	gcc sys_manager.c functions.c -Wall -Wextra -pthread -g  -o home_iot 

user_console: user_console.c functions.c
	gcc user_console.c functions.c -Wall -Wextra -pthread -g -o user_console
