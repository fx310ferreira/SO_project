all: sensor home_iot user_console

sensor: sensor.c functions.c
	gcc sensor.c functions.c -Wall -Wextra -g -o sensor

home_iot: sys_manager.c functions.c
	gcc sys_manager.c functions.c -Wall -Wextra -g  -o home_iot 

user_console: user_console.c functions.c
	gcc user_console.c functions.c -Wall -Wextra -g  -o user_console
