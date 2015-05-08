#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>

extern char** environ;

void parent_write_pipe(int result_pipe)
{
    FILE* input_stream = fdopen(result_pipe, "r");
  //будем помнить последние 8 символов из потока, приведенных к нижнему регистру
    char last[9];
    memset(last, 0, 9);
    int href_mode = 0;

    while (1)
    {
        int code = fgetc(input_stream);
        if (code == EOF)
        {
            break;
        }
        char ch = (char) code;
        for (int i = 0; i < 7; ++i)
            last[i] = last[i + 1];

        last[7] = tolower(ch);

        if (href_mode == 1 && ch == '"')
        {
            href_mode = 0;
            printf("\n");
        }

        if (href_mode == 1)
        {
            printf("%c", ch);
        }

        if (strcmp(last, "<a href=") == 0)
        {
            href_mode = 1;
            while (1)
            {
                int code = fgetc(input_stream);
                char ch = (char) code;
                if (code == EOF || ch == '"')
                {
                    break;
                }
            }
        }
    }
    fclose(input_stream);
}

int main(void)
{
  //получаем название fifo из переменной окружения  
    char *fifo_name = getenv("URLS_SRC");
    if (mkfifo(fifo_name, O_RDWR) != 0 )
    {
        if (errno != EEXIST)
        {
            perror("mkfifo");
            return EXIT_FAILURE;
        }
    }
  //открываем fifo
    printf("Waiting for url's from FIFO\n");
    FILE* fifo = fopen(fifo_name, "r");
    if (fifo == NULL)
    {
        fprintf(stderr, "Can't open fifo: %s\n", fifo_name);
        return 1;
    }

    int result_pipe[2];
    pipe(result_pipe);
  //порождаем процесс, который будет обрабатывать результаты из пайпа
    pid_t read_child = fork();
    if (read_child == -1)
    {
        fprintf(stderr, "Can't fork.");
        return 1;
    }
    if (read_child == 0)
    {
        close(result_pipe[1]);
        parent_write_pipe(result_pipe[0]);
        return EXIT_SUCCESS;
    }

    while (1)
    {
        int len;
        char* url = NULL;
      //читаем очередной URL  
        int readed = getline(&url, &len, fifo);
        if (readed <= 0)
        {
            free(url);
            break;
        }
        if (url[readed - 1] == '\n')
        {
            url[readed - 1] = '\0';
        }
        printf("url: %s\n", url);

        char line[strlen(url) + 1];
        strcpy(line, url);
        free(url);
      //порождаем процесс для exec  
        pid_t child = fork();
        if (child == -1)
        {
            fprintf(stderr, "Can't fork.");
            return 1;
        }
        if (child == 0)
        {
            printf("%s\n", line);
            close(result_pipe[0]);

          //перенаправляем вывод curl в канал pipe_write
            if (dup2(result_pipe[1], 1) == -1)
            {
                perror("dup2 error.");
                exit(EXIT_FAILURE);
            }
            
            char *new_argv[] = {"curl" , "-s", url, NULL};
            if (execvpe("curl", new_argv, environ) == -1)
            {
                perror("Can't run curl.");
                _exit(EXIT_FAILURE);
            }
        }
        wait(NULL);
    }

    fclose(fifo);
    close(result_pipe[1]);
    wait(NULL);
    return EXIT_SUCCESS;
}

