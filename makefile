all: sensor home_iot user_console

sensor: sensor.c
	gcc sensor.c -Wall -Wextra -o sensor

home_iot: sys_manager.c
	gcc sys_manager.c -Wall -Wextra -o home_iot 

user_console: user_console.c
	gcc user_console.c -Wall -Wextra -o user_console
