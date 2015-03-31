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

/*char *get_command_path(char *command_name, char *dst)
{
    char *env_path = getenv("PATH");
    printf("%s\n", env_path);
    char *current = strtok(env_path, ":");

    while (current != NULL)
    {
        char full_path[PATH_MAX + 1];
        sprintf(full_path, "%s/%s", current, command_name);

        if (access(full_path, F_OK) == 0)
        {
            strcpy(dst, full_path);
            return dst;
        }

        current = strtok(NULL, ":");
    }

    strcpy(dst, "");
    return dst;
}*/

void process_url(char *url, char *curl_path, int write_pipe)
{
    dup2(write_pipe, 1);

    if (fork() == 0)
    {
        printf("CHILD - %s\n", url);
        char *new_argv[] = {"curl", url, NULL};
        if (execvpe("curl", new_argv, environ) == -1)
        {
            close(write_pipe);
            _exit(EXIT_SUCCESS);
        }
    }
    else
    {
        wait(NULL);
        printf("END OF PAGE\n");
        close(write_pipe);
        _exit(EXIT_SUCCESS);
    }
}

void parent_write_pipe(int result_pipe[2])
{
    FILE *out_file = fopen("out.txt", "w");
    if (out_file == NULL)
        perror("file");
    printf("Try to read\n");
    //char ch;
    char last[8];
    char ch;
    last[7] = '\0';
    while (1)
    {
        int code = read(result_pipe[0], &ch, 1);

        if (code != 1)
            break;

        //if (ch != '\n' && ch != '\t')
       // {
            fprintf(out_file, "%c", ch);
       // }

        for (int i = 0; i < 6; ++i)
            last[i] = last[i + 1];

        last[6] = ch;

        if (strcmp(last, "</html>") == 0)
        {
            fprintf(out_file, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        }
        //printf("last = %s\n", last);
    }

    printf("\nEnd of message.\n");
    fclose(out_file);
}

int main(void)
{
    char curl_path[PATH_MAX + 1];
    //printf("curl == %s\n", name);

    char *fifo_name = getenv("URLS_SRC");

    if (mkfifo(fifo_name, O_RDWR) != 0 )
    {
        fprintf(stderr, "Can't create fifo.\n");

        if (errno != EEXIST)
            return 1;
    }

    int fifo = open(fifo_name, O_RDONLY);

    if (fifo == -1)
    {
        fprintf(stderr, "Can't open fifo: %s\n", fifo_name);
        return 1;
    }

    char* current_url = (char*) malloc(255);
    char ch;
    ssize_t read_code = 0;
    size_t index = 0;

    int children_num = 0;

    int result_pipe[2];
    pipe(result_pipe);
    //char *new_argv[] = {"curl", "ya.ru"};
    //execvpe("curl", new_argv, environ);

    while (1)
    {
        read_code = read(fifo, &ch, 1);

        if (read_code == -1)
        {
            fprintf(stderr, "Can't read from fifo.\n");
            return 1;
        }

        if (read_code == 0)
        {
            break;
        }

        if (ch == '\n')
        {
            pid_t child = fork();

            if (child == -1)
            {
                fprintf(stderr, "Can't fork.");
                return 1;
            }

            if (child == 0)
            {
                char* line = malloc(strlen(current_url));
                strcpy(line, current_url);
                printf("new child - %s\n", line);

                close(result_pipe[0]);

                process_url(line, curl_path, result_pipe[1]);
            }
            parent_write_pipe(result_pipe);
            wait(NULL);
            children_num++;
            //printf("%d\n", children_num);
            index = 0;
            current_url[index] = '\0';
        }
        else
        {
            current_url[index++] = ch;
            current_url[index] = '\0';
        }
    }
    close(fifo);

    close(result_pipe[1]);

    //чтение на закончится пока все дети не завершарт работу
    /*for (int i = 0; i < children_num; ++i)
        wait(NULL);*/

    return 0;
}

