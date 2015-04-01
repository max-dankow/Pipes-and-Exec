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
static const size_t MAX_PAGE_SIZE = 1 << 20;

size_t extract_pipe(int pipe, char* buffer)
{
    char* buffer_start = buffer;

    while (1)
    {
        char ch;
        int code = read(pipe, &ch, 1);
        if (code != 1)
            break;

        *buffer = ch;
        buffer++;
    }

    return buffer - buffer_start;
}

void process_url(char *url, int pipe_write)
{
    //канал для получения цельной страницы от curl
    int curl_pipe[2];

    if (pipe(curl_pipe) == -1)
    {
        perror("pipe");
        _exit(EXIT_FAILURE);
    }

    //перенаправляем вывод curl в канал curl_pipe
    if (dup2(curl_pipe[1], 1) == -1)
    {
        perror("dup2 error.");
        _exit(EXIT_FAILURE);
    }

    char zero_code = (char) 0;
    write(curl_pipe[1], &zero_code, 1);

    pid_t exec_child = fork();

    if (exec_child == -1)
    {
        perror("Can't fork for exec\n");
        _exit(EXIT_FAILURE);
    }

    if (exec_child == 0)
    {
        close(curl_pipe[0]);

        //вызываем curl для адреса из строки url
        char *new_argv[] = {"curl" , "-s", url, NULL};
        if (execvpe("curl", new_argv, environ) == -1)
        {
            perror("Can't run curl.");
            _exit(EXIT_FAILURE);
        }
    }
    else
    {
        close(curl_pipe[1]);
        close(1);

        //получаем цельную страницу - строку
        char page[MAX_PAGE_SIZE];
        size_t page_size = extract_pipe(curl_pipe[0], page);
        page[page_size] = '\0';

        write(pipe_write, page, page_size);

        close(pipe_write);
        _exit(EXIT_SUCCESS);
    }
}

void parent_write_pipe(int result_pipe[2])
{
    FILE *out_file = fopen("out.txt", "w");

    if (out_file == NULL)
    {
        perror("Can't open output file.");
    }

    printf("Try to read\n");
    char last[9];
    memset(last, 0, 9);
    char ch;
    int href_mode = 0;

    while (1)
    {
        int code = read(result_pipe[0], &ch, 1);

        if (code != 1)
            break;

        if (href_mode == 1 && ch == 34)
        {
            href_mode = 0;
            printf("\n");
            //printf("HREF END - %c!\n", ch);
        }

        if (href_mode == 1)
        {
            printf("%c", ch);
        }

        if (ch != '\n' && ch != '\t' && ch != '\0')
        {
            fprintf(out_file, "%c", ch);
        }

        for (int i = 0; i < 7; ++i)
            last[i] = last[i + 1];

        //printf("* char is '%d' *", (unsigned) ch);
        if (ch != (char) 0)
        {
            last[7] = tolower(ch);
        }
        else
        {
            fprintf(out_file, "\n\nNEXT SITE\n\n");
        }

        if (strcmp(last, "<a href=") == 0)
        {
            href_mode = 1;
            read(result_pipe[0], &ch, 1);
            //printf("HREF DETECTED!\n");
        }

        /*if (strcmp(((char*) last) + 1, "</html>") == 0)
        {
            fprintf(out_file, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        }*/

        //fprintf(out_file, "last = %s\n", last + 1);
    }

    printf("\nEnd of message.\n");
    fclose(out_file);
}

int main(void)
{
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
                process_url(line, result_pipe[1]);
            }

            children_num++;
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

    parent_write_pipe(result_pipe);

    //чтение на закончится пока все дети не завершарт работу
    /*for (int i = 0; i < children_num; ++i)
        wait(NULL);*/

    return 0;
}

