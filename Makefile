sshell: sshell.o
	gcc -Wall -Wextra -Werror -o sshell sshell.o

sshell.o: sshell.c
	gcc -Wall -Wextra -Werror -c sshell.c

clean:
	rm -f *.o sshell
