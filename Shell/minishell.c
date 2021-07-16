#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define BUFSIZE 1024
char buffer[BUFSIZE];
int history_fd;
int background = 0;
int noclobber = 0;

//
int split_buffer(char *a, char *delimeter, char** result);
void parse_buffer(char *buf_cmd);
void history_write(char history[]);
void history_read();
void history_execute(int num);
void operator(char *cmd);
void changeDir(int argc, char* argv[]);
void pipe_execute(char **command, int num);
void exec_cmd(char *buf);

//

void fatal(char *str)
{
	perror(str);
	exit(1);
}

int main()
{
    char *cwd;
    char *tail[10];
    char *erase_buffer;
    while(1)
    {
        bzero(buffer, sizeof(buffer));
        cwd = getcwd(NULL, BUFSIZE);
        int n = split_buffer(cwd, "/", tail);
        printf("\n[MiniShell]:%s jeong$ ", tail[n-1]);
        fgets(buffer, BUFSIZE, stdin); // buffer에다가 넣습니다.
        buffer[strlen(buffer)-1] = '\0'; // buffer에 마지막에 NULL값.

        erase_buffer = buffer;
        while(*erase_buffer != '\0') // 왼쪽 공백 제거.
        {
            if(isspace(*erase_buffer)) erase_buffer++;
            else
            {
                break;
            }
        }

        history_write(erase_buffer);
        parse_buffer(erase_buffer); // buffer를 나누기 위해서

    }
    return 0;
}

int split_buffer(char *a, char *delimeter, char** result) // buffer 나누는 것.
{
    int num=0;
    char *ptr = strtok(a, delimeter);
    result[num++] = ptr;
    while(ptr != NULL)
    {  
        ptr = strtok(NULL, delimeter);
        result[num++] = ptr;
    }
    return num-1;
}

int stack_bracket(char* com){
    int cnt = 0;
    for(int i=0; i<strlen(com); i++)
    {
        if(com[i] == '(') cnt++;
        else if(com[i] == ')') cnt--;
    }
    if(cnt == 0) return 1;
    return 0;
}

void parse_buffer(char *cmd_buffer) // ";" 나누기.
{
    char *c[10]; // ;나눈 친구를 담은 buffer
    char temp[BUFSIZE]; // cd, exit, quit검사하기위해 임시 buffer.
    char *temp_result[10]; // temp를 " "로 나눈것에 대한 결과.
    int k;
    pid_t pid;
    
    int n = split_buffer(cmd_buffer, ";\t", c); // ;갯수를 센다.
    for(int i=0; i<n; i++) // ";" 만큼 fork를 한다.
    {
        
        

        strcpy(temp, c[i]); // ";"로 나눈 c를 temp로 복사한다.
        k = split_buffer(temp, " \t", temp_result); // temp_result로 " \t"로 나눈 값을 보낸다.
        if(k<1) fatal("split buffer error");
        if(strcmp(temp_result[0], "cd") == 0) changeDir(k, temp_result); // temp_result[0]가 cd명령어라면 
        else if(strcmp(temp_result[0], "exit") == 0 || strcmp(temp_result[0], "quit") == 0) // exit, quit명령어라면
        {
            printf("~~~~~~~~Bye~~~~~~~\n");
            exit(1);
        }
        else if(temp_result[0][0] == '!') // history를 실행시키위해 ! 왔다면
        {
            int num = 0;
            int go = 1;
            for(int i=1;i <strlen(temp_result[0]); i++) // 숫자인지 아닌지 판단.
            {
                if(!isdigit(temp_result[0][i])) // ! 뒤에 숫자가 아니라면
                {
                    go = 0; // 뒤에 if문을 통과 못 하고
                    break; // 반복문 나간다.
                }
            }
            num = atoi(&temp_result[0][1]);
            if(go)
            {
                history_execute(num);
                // break;
            }
        
        }
        if(strcmp(c[i], "set +o noclobber") == 0) 
        {
            printf("set clobber set 1\n");
            noclobber = 1;
            break;
        }
        else if(strcmp(c[i], "set -o noclobber") == 0)
        {
            printf("set clobber set 0\n");
            noclobber = 0;
            break;
        }
        
        else // 그 외 명령어 라면
        {
            if(c[i][strlen(c[i])-1] == '&') background = 1; 
            if((pid = fork()) == 0) // 명령어를 실행할 자식이 생성된다.
            {
                exec_cmd(c[i]);
            }
            else if(pid > 0)// 부모
            {

                if(background) 
                {
                    background = 0;
                }
                else 
                {
                    waitpid(pid, NULL, 0); // 자식이 끝나기를 기다린다. background가 아니라면.
                    printf("child[%d] is gone.\n", pid);
                }
            }
            else
            {
                fatal("fork error");
            }
        }   
    }
    
}



void history_write(char history[])
{
    char temp[BUFSIZE];
    char number[10] = "";
    int n;
    int num = 0;
    if((history_fd = open("/Users/jeonggyu/Desktop/Shell/history", O_CREAT | O_RDWR | O_APPEND, 0666)) < 0) fatal("history open error");; // history 파일을 연다.

    // "\n" 개행문자의 개수로 앞에 넣을 숫자를 판단한다.
    while((n = read(history_fd, temp, sizeof(temp))) > 0)
    {
        for(int i=0;i<n; i++)
        {
            if(temp[i] == '\n')
            {
                num++;
            }
        }
    }
    sprintf(number, "[%d]", num);
    write(history_fd, number, strlen(number)); // 앞에 넣을 숫자.

    sprintf(temp, "%s\n", history);
    write(history_fd, temp, strlen(temp)); // history 파일에 쓴다.
    close(history_fd); // 파일을 닫는다.
}

void history_read()
{
    int n;
    char instruct[BUFSIZE];
    if((history_fd = open("/Users/jeonggyu/Desktop/Shell/history", O_RDONLY)) < 0) fatal("history open error");
    lseek(history_fd, 0, SEEK_SET);
    while((n = read(history_fd, instruct, sizeof(instruct))) > 0)
    {
        write(STDOUT_FILENO, instruct, n);
    }
    close(history_fd);
    exit(1);
}

void history_execute(int num_line)
{
    char temp[BUFSIZE];
    char argv[BUFSIZE] = {0,};
    int n,i, cnt=0;
    int num = 0;
    if((history_fd = open("/Users/jeonggyu/Desktop/Shell/history", O_RDONLY)) < 0) fatal("history open error");
    while((n = read(history_fd, temp, sizeof(temp))) > 0)
    {
        for(i=0; i<n && num<num_line; i++) // num_line까지 돌고 만약 n이 넘는다면 에러발생.
        {
            if(temp[i] == '\n')
            {
                num++; // num_line까지 history를 돌려 temp[i]에 저장한다.
            }
        }
    }
        do // [숫자] 부분을 제거하기 위해 일의 자리, 십의 자리를 구분한다.
        {
            num /= 10;
            cnt++;
        } while(num != 0);

        for(int j=0;;j++,i++) 
        {
            if(temp[i+cnt+2] == '\n') // 만약 개행문자를 만났다면 끝
            {
                break;
            }
            argv[j] = temp[i+cnt+2]; // temp[i]에서 [숫자]를 제외한 명령어만 뽑아온다.
        }
    close(history_fd); // history파일을 닫는다.

    parse_buffer(argv); // 뽑아온 명령어를 다시 돌린다.
}

void operator(char *cmd)
{
    char *file_name;
    int fd1,fd2;
    int len = strlen(cmd);
    for(int i=0; i<len; i++) // >, >>, <, << 를 찾는다.
    {
        if(cmd[i] == '>' && cmd[i+1] == '>') // >> 이라면
        {
            file_name = strtok(&cmd[i+2], " \t"); // 파일 이름을 가져온다.
            if((fd1 = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0666)) < 0) fatal("file open error"); // 끝에 추가해야 해서 O_APPEND를 추가한다.
            dup2(fd1, STDOUT_FILENO); // 명령어의 출력을 fd1에다가 집어넣는다.
            close(fd1); // fd1을 닫는다.
            cmd[i] = '\0';
            cmd[i+1] = '\0';
            break;
        }
        else if(cmd[i] == '>') // > 이라면
        {
            if(cmd[i+1] == '|' || noclobber==0)
            {
                if(cmd[i+1] == '|') 
                {
                    file_name = strtok(&cmd[i+2], " \t");
                }
                else file_name = strtok(&cmd[i+1], " \t");
                if((fd1 = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) fatal("file open error"); // 파일에 써야 하므로 O_WRONLY로 만든다.
                dup2(fd1, STDOUT_FILENO);
                close(fd1);
                cmd[i] = '\0';
                if(cmd[i+1] == '|') cmd[i+1] = '\0';
                break;
            }
            else
            {
                printf("Clobber is set. ERROR\n");
                exit(1);
            }
        }
        else if(cmd[i] == '<') // < 이라면
        {
            file_name = strtok(&cmd[i+1], " \t");
            if((fd1 = open(file_name, O_RDONLY, 0666)) < 0) fatal("file open error"); // 파일을 읽어 와야 하므로 O_RDONLY로 만든다.
            dup2(fd1, STDIN_FILENO);
            close(fd1);
            cmd[i] = '\0';
            break;
        }
        
    }
}

void changeDir(int argc, char* argv[])
{
    chdir(argv[1]);
}


void pipe_execute(char **command, int num)
{
    int i;
    int pfd[2];
    for(i=0; i<num-1; i++)
    {
		pipe(pfd);
		switch(fork())
		{
			case -1:  // error
            fatal("fork error");
            case  0:  // 자식은 첫번째 명령어 실행한 뒤 부모에게 그 값 알려줌
            close(pfd[0]);
            dup2(pfd[1], STDOUT_FILENO);
            exec_cmd(command[i]);
            default: // 부모는 그 자식값 받고
            close(pfd[1]);
            dup2(pfd[0], STDIN_FILENO);
		}
	}
	exec_cmd(command[i]); // 순차적으로 다 실행되었으면 마지막 실행.
}

void exec_cmd(char *command)
{
    char *pipe_args[10];
    operator(command);
    int pipe_num = split_buffer(command, "|\t", pipe_args);
    if(pipe_num > 1) // pipe라면
    {
        pipe_execute(pipe_args, pipe_num);
    }
    
    else // pipe가 아니라면
    {
        if(command[strlen(command)-1] == '&') command[strlen(command)-1] = '\0';
        char *args[10]; // buf를 나눈 인자로 받을 친구들.
        int op_num = split_buffer(command, " \t", args); // " "로 나눈다.
        if(op_num < 1) fatal("\" \t\" split error"); // 만약 " "로 나눈게 없다면.
        if(strcmp(args[0], "history") == 0) history_read(); // history명령어
        if(background) printf("\n");
        if(args[0][0] == '!') exit(1);
        if(execvp(args[0], args) == -1) // 명령을 실행한다.
        {
            fatal("execvp error");
        }       
    }
}