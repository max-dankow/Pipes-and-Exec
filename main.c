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

char *get_command_path(char *command_name, char *dst)
{
    char *env_path = getenv("PATH");
    char *current = strtok(env_path, ":");

    while (current != NULL)
    {
        current = strtok(NULL, ":");

        struct stat info;
        char full_path[PATH_MAX + 1];
        sprintf(full_path, "%s/%s", current, command_name);

        if (lstat(full_path, &info) == 0)
        {
            strcpy(dst, full_path);
            return dst;
        }
    }

    strcpy(dst, "");
    return dst;
}

void process_url(char *url, char *curl_path)
{
    printf("CHILD - %s - %s\n", url, curl_path);
    if (execl(curl_path, curl_path, url, NULL) == -1)
        perror("cant exec\n");
}

int main(void)
{
    char curl_path[PATH_MAX + 1];
    char *name = get_command_path("curl", curl_path);
    printf("curl == %s\n", name);
    //process_url("ya.ru", curl_path);
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

    //write(fifo, "Hello\n", sizeof("Hello\n"));
    char* MyResult = (char*) malloc(255);
    char ch;
    ssize_t read_code = 0;
    size_t index = 0;
    int children_num = 0;
//process_url("ya.ru", curl_path);
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
            //process_url("ya.ru", curl_path);

            pid_t child = fork();

            if (child == 0)
            {
                char* line = malloc(strlen(MyResult));
                strcpy(line, MyResult);
                process_url(line, curl_path);
                free(line);
                return 0;
            }

            children_num++;
            index = 0;
            MyResult[index] = '\0';
        }
        else
        {
            //printf("%c", ch);
            MyResult[index++] = ch;
            MyResult[index] = '\0';
        }
    }

    close(fifo);

    for (int i = 0; i < children_num; ++i)
        wait(NULL);

    /*fifo = open(fifo_name, O_RDONLY);
    char* MyResult = (char*) malloc(255);
    read(fifo, MyResult, sizeof(MyResult));

    close(fifo);*/
    return 0;
}

