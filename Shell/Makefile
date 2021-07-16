shell : main.o
	gcc -o shell main.o
main.o : minishell.c
	gcc -c -o main.o minishell.c
clean :
	rm *.o shell
