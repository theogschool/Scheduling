Create: implement.c
	gcc -Wall -Wpedantic -Wextra -Werror -lrt -std=gnu99 -pthread implement.c -o implement
clean:
	rm -f implement
