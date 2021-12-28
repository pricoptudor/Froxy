//#include <protocol.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>

#define DEFAULT_PORT 2024
#define MAX_JSON 8192
#define MAX_RESPONSE 4096
#define MAX_COMMAND 150
#define MAX_USERPASS 40

// SA MA UIT DUPA CHECKPOINTS!!!

char CURRENT_USER[MAX_USERPASS];
int USER_LOGGED;
int ADMIN_LOGGED;
int port;

void command_error(int command_count, char *command_list[])
{
    printf("Command unknown.\n");
    printf("Here is a list with available commands:\n");
    for (int i = 0; i < command_count; ++i)
    {
        printf("\t\t%s\n", command_list[i]);
    }
    printf("\n\n");
}

char *getNetIp()
{
    // daca e conectat la internet returneaza adresa routerului
    //   altfel returneaza 127.0.0.1
    char cmd[100];
    sprintf(cmd, "%s", "ifconfig | grep inet | grep 192 | xargs | cut -d' ' -f2");

    FILE *fd = popen(cmd, "r");

    char line[4096];

    char *text = (char *)malloc(100);
    explicit_bzero(text, 100);

    while (fgets(line, 4096, fd) != NULL)
    {
        strcat(text, line);
    }

    text[strlen(text) - 1] = '\0';
    if (strlen(text) < 1)
    {
        strcpy(text, "127.0.0.1");
    }
    return text;
}

void welcome()
{
    int siteftp_count = 3;
    char *siteftp_list[] = {"freebsd.cs.nctu.edu.tw",
                            "ftp.pureftpd.org",
                            "ftp.gnu.org"};

    printf("Welcome to my File Transfer client!\n");
    printf("Here is a list with ftp site suggestions(they are rare):\n");
    for(int i=0;i<siteftp_count;++i)
    {
        printf("\t\t\t%s\n",siteftp_list[i]);
    }
    printf("\\\\\\\\\\\\\\\\\\\\\\\\ \n");

    USER_LOGGED = 0;
    ADMIN_LOGGED = 0;

    DIR *dir = opendir("download"); // director download
    if (dir)
    {
        /* Directory exists. */
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        /* Directory does not exist. */
        if (mkdir("download", 0777) == -1)
        {
            printf("[proxy]Error creating download director\n");
            //return 0;
        }
    }
    else
    {
        printf("[proxy]Eroare la deschidere director download.\n");
        //return 0;
    }

    if (-1 == chdir("download"))
    {
        printf("[proxy]Error entering download folder.\n");
        //return 0;
    }
}

int init_connection(int argc, char *argv[], int *sd)
{
    struct sockaddr_in server;

    char adresaIp[100];
    strcpy(adresaIp, getNetIp());

    if (argc != 1)
    {
        printf("Sintaxa: %s \n", argv[0]);
        exit(0);
    }

    port = DEFAULT_PORT;

    if (((*sd) = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        exit(0);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(adresaIp);
    server.sin_port = htons(port);

    if (connect((*sd), (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        exit(0);
    }

    char msg[150]; // mesaj daca s-a conectat sau serverul e prea plin.
    if (-1 == recv((*sd), msg, 150, 0))
    {
        printf("[client]Error receiving connected welcome from proxy.\n");
        exit(0);
    }
    printf("%s\n", msg);

    if (strncmp(msg, "Full", 4) == 0)
    {
        sleep(3);
        exit(0);
    }

    return (*sd);
}

int dns_name(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return 1;
    }
    if (c >= 'A' && c <= 'Z')
    {
        return 1;
    }
    if (c >= '0' && c <= '9')
    {
        return 1;
    }
    if (c == '.' || c == '-')
    {
        return 1;
    }
    return 0;
}

int mode00(char *command, int sd) //[checkpoint]:testare mod 00;
{
    /*comenzi disponibile:
            login: username  -+->  pass: password
            create: username password
            exit*/
    int command_count = 3;
    char *command_list[] = {"login: [username]",
                            "create: [username] [password]",
                            "exit"};

    int len = strlen(command);
    if (strncmp(command, "login: ", 7) == 0 && len > 7)
    {
        int flag = 1;
        if (command[len - 1] == '\n')
        {
            command[--len] = 0;
        }
        for (int i = 7; i < len && flag; ++i)
        {
            if (command[i] < 33 || command[i] > 126)
            {
                flag = 0;
                printf("Username must contain only alphanumeric(no space). Please retry.\n");
            }
        }

        if (flag == 1)
        {
            char full_command[300];
            strcpy(full_command, command);

            char usr_aux[MAX_USERPASS];
            strcpy(usr_aux, command + 7);

            printf("Please insert password command like this:\n");
            printf("\tpass: [password]\n");
            explicit_bzero(command, 150);
            fgets(command, 150, stdin);

            if (strncmp(command, "pass: ", 6) == 0 && strlen(command) > 6)
            {
                int flag2 = 1;
                int passlen = strlen(command);
                if (command[passlen - 1] == '\n')
                {
                    command[--passlen] = 0;
                }
                for (int i = 6; i < passlen && flag2; ++i)
                {
                    if (command[i] < 33 || command[i] > 126)
                    {
                        flag2 = 0;
                        printf("Password must contain only alphanumeric(no space).Please retry login.\n");
                    }
                }

                if (flag2 == 1)
                {
                    strcpy(command, command + 5);
                    strcat(full_command, command);
                    if (-1 == send(sd, full_command, strlen(full_command) + 1, 0))
                    {
                        printf("[client]Error sending login command to proxy.\n");
                        exit(0);
                    }

                    char rasp[20];
                    if (-1 == recv(sd, rasp, 20, 0))
                    {
                        printf("[client]Error receiving response from proxy.\n");
                        exit(0);
                    }

                    USER_LOGGED = rasp[0] - '0';
                    ADMIN_LOGGED = rasp[1] - '0';
                    strcpy(rasp, rasp + 2);
                    if (strcmp(rasp, "connected") == 0)
                    {
                        strcpy(CURRENT_USER, usr_aux);
                        printf("User succesfully logged in.\n");
                    }
                    else
                    {
                        if (strcmp(rasp, "user") == 0)
                        {
                            printf("Username does not exist. Please retry or create a new user.\n");
                        }
                        else if (strcmp(rasp, "password") == 0)
                        {
                            printf("Password incorrect. Please try again.\n");
                        }
                    }
                }
            }
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else if (strncmp(command, "create: ", 8) == 0 && len > 8)
    {
        int flag = 1;
        if (command[len - 1] == '\n')
        {
            command[--len] = 0;
        }
        int contor = 8;
        while (command[contor] != ' ')
        {
            if (command[contor] == 0)
            {
                printf("Command failed to process. Please retry in correct format:");
                printf(" create: [username] [password]\n");
                flag = 0;
                break;
            }
            if (command[contor] < 33 || command[contor] > 126)
            {
                printf("Username and password must contain only alphanumeric. ");
                printf("Please retry.\n");
                flag = 0;
                break;
            }
            contor++;
        }
        contor++;
        while (flag && command[contor] != 0)
        {
            if (command[contor] == ' ')
            {
                printf("Username and password must not contain any white spaces.]\n");
                printf("Please retry.\n");
                flag = 0;
                break;
            }
            if (command[contor] < 33 || command[contor] > 126)
            {
                printf("Username and password must contain only alphanumeric. ");
                printf("Please retry.\n");
                flag = 0;
                break;
            }
            contor++;
        }

        if (flag == 1)
        {
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending create command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving create response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else if (strncmp(command, "exit", 4) == 0)
    {
        int flag = 1;
        for (int i = 4; i < len && flag; ++i)
        {
            if (command[i] != ' ' && command[i] != '\n')
            {
                flag = 0;
                command_error(command_count, command_list);
            }
        }

        if (flag == 1)
        {
            command[4] = 0;
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending exit command to proxy.\n");
                exit(0);
            }

            char rasp[30];
            if (-1 == recv(sd, rasp, 30, 0))
            {
                printf("[client]Error receiving exit response from proxy.\n");
                exit(0);
            }
            printf("%s\n", rasp);
            sleep(3);
            close(sd);
            exit(0);
        }
        else
        {
            command_error(command_count, command_list);
        }
        //[to-do]: raspuns server + exit()
    }
    else
    {
        command_error(command_count, command_list);
    }
}

int mode10(char *command, int sd)
{
    /*comenzi disponibile:
            login: ...(mesaj informativ: you must logout first!)
            create: ...(mesaj informativ: you must logout first!)
            logout
            server: server-name
            exit*/

    int command_count = 3;
    char *command_list[] = {"logout",
                            "server: [server-name]",
                            "exit"};

    int len = strlen(command);
    if (strncmp(command, "login", 5) == 0 || strncmp(command, "create", 6) == 0)
    {
        printf("You must logout before doing this operation.\n");
    }
    else if (strncmp(command, "logout", 6) == 0)
    {
        int flag = 1;
        if (command[len - 1] == '\n')
        {
            command[--len] = 0;
        }
        for (int i = 6; i < len && flag; ++i)
        {
            if (command[i] != ' ')
            {
                flag = 0;
            }
        }

        if (flag == 1)
        {
            command[6] = 0;
            strcat(command, ": ");
            strcat(command, CURRENT_USER);

            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending logout command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving logout response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);

            if (strncmp(rasp, "Success", 7) == 0)
            {
                USER_LOGGED = 0;
                ADMIN_LOGGED = 0;
            }
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else if (strncmp(command, "exit", 4) == 0)
    {
        int flag = 1;
        for (int i = 4; i < len && flag; ++i)
        {
            if (command[i] != ' ' && command[i] != '\n')
            {
                flag = 0;
                command_error(command_count, command_list);
            }
        }

        if (flag == 1)
        {
            strcpy(command, "logout: ");
            strcat(command, CURRENT_USER);

            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending logout command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving logout response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);

            if (strncmp(rasp, "Success", 7) == 0)
            {
                USER_LOGGED = 0;
                ADMIN_LOGGED = 0;
            }

            strcpy(command, "exit");
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending exit command to proxy.\n");
                exit(0);
            }

            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving exit response from proxy.\n");
                exit(0);
            }
            printf("%s\n", rasp);
            sleep(3);
            close(sd);
            exit(0);
        }
        else
        {
            command_error(command_count, command_list);
        }
        //[to-do]: raspuns server + exit()
    }
    else if (strncmp(command, "server: ", 8) == 0 && len > 8)
    {
        int flag = 1;
        if (command[len - 1] == '\n')
        {
            command[--len] = 0;
        }
        for (int i = 8; i < len && flag; ++i)
        {
            if (command[i] < 33 || command[i] > 126)
            {
                flag = 0;
                printf("Server name must contain only alphanumerical. Please retry.\n");
            }
        }

        if (flag == 1)
        {
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending server command to proxy.\n");
                exit(0);
            }

            ///////////////[to-do]:aici incepe partea ftp;
            char rasp[MAX_RESPONSE];
            int stop=0;
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error recv ftp list.\n");
                exit(0);
            }
            printf("%s\n", rasp);
            
            if(strncmp(rasp,"Client is not allowed",21)==0)
            {
                stop=1;
            }

            strcpy(rasp,"OK");
            if(-1 == send(sd,rasp,MAX_RESPONSE,0))
            {
                printf("[client]Error send ok server.\n");
            }

            if(stop)
            {
                return 0;
            }

            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error recv ftp list.\n");
                exit(0);
            }
            printf("%s\n", rasp);

            while (1)
            {
                printf("Insert your command here: ");
                fgets(command, MAX_COMMAND, stdin);

                if (command[strlen(command) - 1] == '\n')
                {
                    command[strlen(command) - 1] = 0;
                }

                ///[to-do]:organizare ftp!!!

                if (strncmp(command, "help", 4) == 0)
                {
                    int flag = 1;
                    for (int i = 4; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[4] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send help ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv help resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "exit", 4) == 0)
                {
                    int flag = 1;
                    for (int i = 4; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[4] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send exit ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv help resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                        break;
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "list-all", 8) == 0)
                {
                    int flag = 1;
                    for (int i = 8; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[8] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send list-all ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv list-all resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "list-files", 10) == 0)
                {
                    int flag = 1;
                    for (int i = 10; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[10] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send list-files ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv list-files resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "download: ", 10) == 0 && strlen(command) > 10)
                {
                    int flag = 1;
                    for (int i = 10; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] < 33 || command[i] > 126)
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send download ftp.\n");
                            exit(0);
                        }
                        int stop=0;
                        //nu trece de restrictii:
                        //[to-do]: atentie si la cealalta restrictie sa trimit si mesaj bun,nu numai rau aici
                        if(-1 == recv(sd,rasp,MAX_RESPONSE,0))
                        {
                            printf("Error recv download good.\n");
                            exit(0);
                        }
                        printf("%s\n",rasp);
                        if(strncmp(rasp,"Client is not allowed",21)==0)
                        {
                            stop=1;
                        }
                        strcpy(rasp,"Ok");
                        if(-1 == send(sd,rasp,strlen(rasp)+1,0))
                        {
                            printf("[client]Error send approve download.\n");
                            exit(0);
                        }

                        if(stop)
                        {
                            continue;
                        }

                        //[to-do]receive the file:
                        //[to-do]if files exists, give different name;
                        strcpy(command,command+10);
                        int fd;
                        
                        if (-1 == (fd = open(command, O_RDWR | O_CREAT,0777)))
                        {
                            perror("[client]Eroare creare fisier.\n");
                            exit(0);
                        }
                        //freebsd.cs.nctu.edu.tw

                        char ch;
                        int bytes_read;
                        //int stop;
                        bytes_read = 0;
                        /*if(-1 == recv(sd,&stop,sizeof(int),0))
                        {
                            perror("WTF is this./n");
                            exit(0);
                        }
                        printf("\nstop:%d\n",stop);*/
                        
                        while (1)
                        {
                            /*if(-1 == recv(sd,&stop,sizeof(int),0))
                            {
                                perror("This is a problem.\n");
                            }
                            if(stop==1)
                            {
                                break;
                            }*/
                            int aux = recv(sd, &ch, sizeof(char), 0);
                            if(ch==0)
                            {
                                printf("Download complete.\n");
                                break;
                            }
                            if (aux == 0)
                            {
                                printf("[proxy]Download completed.\n");
                                break;
                            }
                            if (aux < 0)
                            {
                                printf("[proxy]Download interrupted.\n");
                                break;
                            }
                            if (write(fd, &ch, sizeof(char)) < 0)
                            {
                                printf("[proxy]Writing in file failed.\n");
                                break;
                            }
                            bytes_read++;
                        }

                        close(fd);

                        strcpy(command,"Done");
                        if(-1 == send(sd,command,strlen(command)+1,0))
                        {
                            printf("[client]error send done download.\n");
                            exit(0);
                        }

                        if(-1 == recv(sd,rasp,MAX_RESPONSE,0))
                        {
                            printf("[client]error recv done download.\n");
                            exit(0);
                        }
                        printf("%s\n",rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect(or filename incorrect). Retry.\n");
                    }
                }
                else if (strncmp(command, "move-down: ", 11) == 0 && strlen(command) > 11)
                {
                    int flag = 1;
                    for (int i = 11; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] < 33 || command[i] > 126)
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("[client]Error send move-down ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv move-down resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect(or directory name incorrect). Retry.\n");
                    }
                }
                else if (strncmp(command, "move-up", 7) == 0)
                {
                    int flag = 1;
                    for (int i = 7; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[7] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send move-up ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv move-up resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else
                {
                    printf("[client]Command format incorrect. Retry.\n");
                }
            }
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else
    {
        command_error(command_count, command_list);
    }
}

int mode11(char *command, int sd)
{
    /*comenzi disponibile:
            login: ...(mesaj informativ: you must logout first!)
            create: ...(mesaj informativ: you must logout first!)
            logout
            server: server-name
            create-admin: username password
            forbidden
            exit-proxy
            exit*/

    int command_count = 3;
    char *command_list[] = {"logout",
                            "server: [server-name]",
                            "exit",
                            "create-admin: [username] [password]",
                            "forbidden",
                            "exit-proxy",
                            "exit"};

    int len = strlen(command);
    if (strncmp(command, "login", 5) == 0)
    {
        printf("You must logout before doing this operation.\n");
    }
    else if (strncmp(command, "forbidden", 9) == 0)
    {
        int flag = 1;
        for (int i = 9; i < len && flag; ++i)
        {
            if (command[i] != ' ' && command[i] != '\n')
            {
                flag = 0;
                command_error(command_count, command_list);
            }
        }

        if (flag == 1)
        {
            command[9] = 0;
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending forbidden command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving forbidden response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);
            do
            {
                printf("Please insert the letter: ");
                fgets(command, 150, stdin);
                if (command[0] != 'a' && command[0] != 'm')
                {
                    printf("Please introduce only the letter 'a' or 'm'.\n");
                }
                else
                {
                    command[1] = 0;
                }
            } while (command[0] != 'a' && command[0] != 'm');

            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending forbidden a or m letter.\n");
                exit(0);
            }

            if (command[0] == 'a')
            {
                char file[MAX_JSON];
                if (-1 == recv(sd, file, MAX_JSON, 0))
                {
                    printf("[client]Error recv json file.\n");
                    exit(0);
                }

                printf("This is the JSON config file:\n%s\n", file);
                printf("\nDo you want to add other types of files?[y\\n]\n");
                /*if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                {
                    printf("[client]Error recv first question.\n");
                    exit(0);
                }
                printf("%s\n", rasp);*/

                do
                {
                    printf("Please insert your answer: ");
                    fgets(command, 150, stdin);
                    if (command[0] != 'y' && command[0] != 'n')
                    {
                        printf("Please introduce only the letter 'y' or 'n'.\n");
                    }
                    else
                    {
                        command[1] = 0;
                    }
                } while (command[0] != 'y' && command[0] != 'n');

                if (-1 == send(sd, command, strlen(command) + 1, 0))
                {
                    printf("[client]Error sending forbidden y or n letter.\n");
                    exit(0);
                }

                if (command[0] == 'y')
                {
                    while (1)
                    {
                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv add type file.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                        printf("Insert here1: ");

                        int flag;
                        do
                        {
                            fgets(command, 150, stdin);
                            if (command[strlen(command) - 1] == '\n')
                            {
                                command[strlen(command) - 1] = 0;
                            }

                            flag = 0;
                            for (int i = 0; i < strlen(command); ++i)
                            {
                                if (!(command[i] >= 'a' && command[i] <= 'z'))
                                {
                                    printf("Insert command again in correct format(e.g 'xml'): ");
                                    flag = 1;
                                    break;
                                }
                            }
                        } while (flag);

                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("[client]Error sending forbidden file type.\n");
                            exit(0);
                        }

                        if (strcmp(command, "null") == 0)
                        {
                            break;
                        }
                    }
                }

                if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                {
                    printf("[client]Error recv domain question.\n");
                    exit(0);
                }
                printf("%s\n", rasp);

                do
                {
                    printf("Please insert your answer: ");
                    fgets(command, 150, stdin);
                    if (command[0] != 'y' && command[0] != 'n')
                    {
                        printf("Please introduce only the letter 'y' or 'n'.\n");
                    }
                    else
                    {
                        command[1] = 0;
                    }
                } while (command[0] != 'y' && command[0] != 'n');

                if (-1 == send(sd, command, strlen(command) + 1, 0))
                {
                    printf("[client]Error sending forbidden domain y or n letter.\n");
                    exit(0);
                }

                if (command[0] == 'y')
                {
                    while (1)
                    {
                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv add domain name.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                        printf("Insert here2: ");

                        int flag;
                        do
                        {
                            fgets(command, 150, stdin);
                            if (command[strlen(command) - 1] == '\n')
                            {
                                command[strlen(command) - 1] = 0;
                            }

                            flag = 0;
                            for (int i = 0; i < strlen(command); ++i)
                            {
                                if (!dns_name(command[i]))
                                {
                                    printf("Insert command again in correct format(e.g '.uaic.ro'): ");
                                    flag = 1;
                                    break;
                                }
                            }
                        } while (flag);

                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("[client]Error sending forbidden domain name.\n");
                            exit(0);
                        }

                        if (strcmp(command, "null") == 0)
                        {
                            break;
                        }

                        while (1) // lista cu numele clientilor
                        {
                            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                            {
                                printf("[client]Error recv add client name.\n");
                                exit(0);
                            }
                            printf("%s\n", rasp);
                            printf("Insert here3: ");

                            int flag;
                            do
                            {
                                fgets(command, 150, stdin);
                                if (command[strlen(command) - 1] == '\n')
                                {
                                    command[strlen(command) - 1] = 0;
                                }

                                flag = 0;
                                for (int i = 0; i < strlen(command); ++i)
                                {
                                    if (!dns_name(command[i]))
                                    {
                                        printf("Insert command again in correct format(e.g '.uaic.ro'): ");
                                        flag = 1;
                                        break;
                                    }
                                }
                            } while (flag);

                            if (-1 == send(sd, command, strlen(command) + 1, 0))
                            {
                                printf("[client]Error sending forbidden domain name.\n");
                                exit(0);
                            }

                            if (strcmp(command, "null") == 0)
                            {
                                break;
                            }
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv add days.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                        printf("Insert here4: ");

                        // int flag;
                        do
                        {
                            fgets(command, 150, stdin);
                            if (command[strlen(command) - 1] == '\n')
                            {
                                command[strlen(command) - 1] = 0;
                            }

                            flag = 0;
                            for (int i = 0; i < strlen(command); ++i)
                            {
                                if (!(command[i] >= '0' && command[i] <= '9'))
                                {
                                    printf("Insert command again in correct format(e.g '15367'): ");
                                    flag = 1;
                                    break;
                                }
                            }
                        } while (flag);

                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("[client]Error sending forbidden days.\n");
                            exit(0);
                        }

                        while (1)
                        {
                            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                            {
                                printf("[client]Error recv add hours.\n");
                                exit(0);
                            }
                            printf("%s\n", rasp);
                            printf("Insert here5: ");

                            int flag;
                            do
                            {
                                fgets(command, 150, stdin);
                                if (command[strlen(command) - 1] == '\n')
                                {
                                    command[strlen(command) - 1] = 0;
                                }

                                if (strcmp(command, "null") == 0)
                                {
                                    break;
                                }

                                flag = 0;
                                for (int i = 0; i < strlen(command); ++i)
                                {
                                    if (!(command[i] >= '0' && command[i] <= '9'))
                                    {
                                        printf("Insert command again in correct format(e.g '17'): ");
                                        flag = 1;
                                        break;
                                    }
                                }

                                if (atoi(command) < 0 || atoi(command) > 24)
                                {
                                    printf("Insert command again in correct format(e.g 0<=17<=24): ");
                                    flag = 1;
                                }
                            } while (flag);

                            if (-1 == send(sd, command, strlen(command) + 1, 0))
                            {
                                printf("[client]Error sending forbidden hours.\n");
                                exit(0);
                            }

                            if (strcmp(command, "null") == 0)
                            {
                                break;
                            }
                        }
                    }
                }
            }
            else if (command[0] == 'm') // modify maxsize, deleting file types
            {
                char file[MAX_JSON];
                if (-1 == recv(sd, file, MAX_JSON, 0))
                {
                    printf("[client]Error recv json file.\n");
                    exit(0);
                }

                printf("This is the JSON config file:\n%s\n\n", file);
                printf("Do you want to modify the maximum allowed size for download files?[y\\n]\n");

                do
                {
                    printf("Please insert your answer: ");
                    fgets(command, 150, stdin);
                    if (command[0] != 'y' && command[0] != 'n')
                    {
                        printf("Please introduce only the letter 'y' or 'n'.\n");
                    }
                    else
                    {
                        command[1] = 0;
                    }
                } while (command[0] != 'y' && command[0] != 'n');

                if (command[0] == 'y')
                {

                    char command2[150];
                    strcpy(command2, command);

                    while (1)
                    {
                        printf("Please insert new maximum size allowed for download files(only digits): ");
                        int flag = 1;
                        fgets(command, 150, stdin);
                        if (command[strlen(command) - 1] == '\n')
                        {
                            command[strlen(command) - 1] = 0;
                        }
                        for (int i = 0; i < strlen(command) && flag; ++i)
                        {
                            if (command[i] < '0' || command[i] > '9')
                            {
                                printf("Only digits allowed. Repeat.\n");
                                flag = 0;
                            }
                        }
                        if (flag)
                        {
                            break;
                        }
                    }

                    strcat(command2, command);

                    if (-1 == send(sd, command2, strlen(command2) + 1, 0))
                    {
                        printf("[client]Error sending forbiddenM y or n letter.\n");
                        exit(0);
                    }
                }
                else
                {
                    if (-1 == send(sd, command, strlen(command) + 1, 0))
                    {
                        printf("[client]Error sending forbiddenM y or n letter.\n");
                        exit(0);
                    }
                }

                if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                {
                    printf("[client]Error recv next question.\n");
                    exit(0);
                }
                printf("%s\n", rasp);

                do
                {
                    printf("Please insert your answer: ");
                    fgets(command, 150, stdin);
                    if (command[0] != 'y' && command[0] != 'n')
                    {
                        printf("Please introduce only the letter 'y' or 'n'.\n");
                    }
                    else
                    {
                        command[1] = 0;
                    }
                } while (command[0] != 'y' && command[0] != 'n');

                //[checkpoint]: verific ca toate recv sa fie in rasp;
                // sa pun modify maxsize SI file types!!(le sarisem)

                if (-1 == send(sd, command, strlen(command) + 1, 0))
                {
                    printf("[client]Error sending forbiddenM y or n letter.\n");
                    exit(0);
                }

                if (command[0] == 'y')
                {
                    while (1)
                    {
                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error in delete file types.\n");
                            exit(0);
                        }

                        if (strcmp(rasp, "null") == 0)
                        {
                            break;
                        }
                        printf("%s\n", rasp);

                        do
                        {
                            printf("Please insert your answer: ");
                            fgets(command, 150, stdin);
                            if (command[0] != 'y' && command[0] != 'n')
                            {
                                printf("Please introduce only the letter 'y' or 'n'.\n");
                            }
                            else
                            {
                                command[1] = 0;
                            }
                        } while (command[0] != 'y' && command[0] != 'n');

                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("[client]Error sending forbidden delete type y or n letter.\n");
                            exit(0);
                        }
                    }
                    strcpy(rasp, "Stop");
                    if (-1 == send(sd, rasp, strlen(rasp) + 1, 0))
                    {
                        printf("[client]Error sending stop.\n");
                        exit(0);
                    }
                }

                if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                {
                    printf("[client]Error recv next question.\n");
                    exit(0);
                }
                printf("%s\n", rasp);
                // printf("Do you want to delete restrictions to servers?[y\\n]\n");
                do
                {
                    printf("Please insert your answer: ");
                    fgets(command, 150, stdin);
                    if (command[0] != 'y' && command[0] != 'n')
                    {
                        printf("Please introduce only the letter 'y' or 'n'.\n");
                    }
                    else
                    {
                        command[1] = 0;
                    }
                } while (command[0] != 'y' && command[0] != 'n');

                if (-1 == send(sd, command, strlen(command) + 1, 0))
                {
                    printf("[client]Error sending forbiddenM y or n letter.\n");
                    exit(0);
                }

                if (command[0] == 'y')
                {
                    while (1)
                    {
                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error in delete server restrict.\n");
                            exit(0);
                        }

                        if (strcmp(rasp, "null") == 0)
                        {
                            break;
                        }
                        printf("%s\n", rasp);

                        do
                        {
                            printf("Please insert your answer: ");
                            fgets(command, 150, stdin);
                            if (command[0] != 'y' && command[0] != 'n')
                            {
                                printf("Please introduce only the letter 'y' or 'n'.\n");
                            }
                            else
                            {
                                command[1] = 0;
                            }
                        } while (command[0] != 'y' && command[0] != 'n');

                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("[client]Error sending forbidden delete server y or n letter.\n");
                            exit(0);
                        }
                    }
                    strcpy(rasp, "Stop");
                    if (-1 == send(sd, rasp, strlen(rasp) + 1, 0))
                    {
                        printf("[client]Error send stop.\n");
                        exit(0);
                    }
                }

                /*strcpy(command, "Done");
                if (-1 == send(sd, command, strlen(command) + 1, 0))
                {
                    printf("[client]Error sending done in m.\n");
                    exit(0);
                }*/
            }

            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving forbidden response.\n");
                exit(0);
            }

            printf("%s\n", rasp);
            fflush(stdout);
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else if (strncmp(command, "exit-proxy", 10) == 0)
    {
        int flag = 1;
        for (int i = 10; i < len && flag; ++i)
        {
            if (command[i] != ' ' && command[i] != '\n')
            {
                flag = 0;
                command_error(command_count, command_list);
            }
        }

        if (flag == 1)
        {
            strcpy(command, "logout: ");
            strcat(command, CURRENT_USER);

            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending logout command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving logout response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);

            if (strncmp(rasp, "Success", 7) == 0)
            {
                USER_LOGGED = 0;
                ADMIN_LOGGED = 0;
            }

            strcpy(command, "exit-proxy");
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending exit command to proxy.\n");
                exit(0);
            }

            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving exit response from proxy.\n");
                exit(0);
            }
            printf("%s\n", rasp);
            sleep(3);
            close(sd);
            exit(0);
        }
        else
        {
            command_error(command_count, command_list);
        }
        //[to-do]: raspuns server + exit()
    }
    else if (strncmp(command, "create-admin: ", 14) == 0 && len > 14)
    {
        int flag = 1;
        if (command[len - 1] == '\n')
        {
            command[--len] = 0;
        }
        int contor = 14;
        while (command[contor] != ' ')
        {
            if (command[contor] == 0)
            {
                printf("Command failed to process. Please retry in correct format:");
                printf(" create-admin: [username] [password]\n");
                flag = 0;
                break;
            }
            if (command[contor] < 33 || command[contor] > 126)
            {
                printf("Username and password must contain only alphanumeric. ");
                printf("Please retry.\n");
                flag = 0;
                break;
            }
            contor++;
        }
        contor++;
        while (flag && command[contor] != 0)
        {
            if (command[contor] == ' ')
            {
                printf("Username and password must not contain any white spaces.\n");
                printf("Please retry.\n");
                flag = 0;
                break;
            }
            if (command[contor] < 33 || command[contor] > 126)
            {
                printf("Username and password must contain only alphanumeric. ");
                printf("Please retry.\n");
                flag = 0;
                break;
            }
            contor++;
        }

        if (flag == 1)
        {
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending create command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving create response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else if (strncmp(command, "server: ", 8) == 0 && len > 8)
    {
        int flag = 1;
        if (command[len - 1] == '\n')
        {
            command[--len] = 0;
        }
        for (int i = 8; i < len && flag; ++i)
        {
            if (command[i] < 33 || command[i] > 126)
            {
                flag = 0;
                printf("Server name must contain only alphanumerical. Please retry.\n");
            }
        }

        if (flag == 1)
        {
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending server command to proxy.\n");
                exit(0);
            }

            ///////////////[to-do]:aici incepe partea ftp;
            char rasp[MAX_RESPONSE];
            int stop=0;
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error recv ftp list.\n");
                exit(0);
            }
            printf("%s\n", rasp);
            
            if(strncmp(rasp,"Client is not allowed",21)==0)
            {
                stop=1;
            }

            strcpy(rasp,"OK");
            if(-1 == send(sd,rasp,MAX_RESPONSE,0))
            {
                printf("[client]Error send ok server.\n");
            }

            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error recv ftp list.\n");
                exit(0);
            }
            printf("%s\n", rasp);

            if(stop)
            {
                return 0;
            }

            while (1)
            {
                printf("Insert your command here: ");
                fgets(command, MAX_COMMAND, stdin);

                if (command[strlen(command) - 1] == '\n')
                {
                    command[strlen(command) - 1] = 0;
                }

                ///[to-do]:organizare ftp!!!

                if (strncmp(command, "help", 4) == 0)
                {
                    int flag = 1;
                    for (int i = 4; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[4] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send help ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv help resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "exit", 4) == 0)
                {
                    int flag = 1;
                    for (int i = 4; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[4] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send exit ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv help resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                        break;
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "list-all", 8) == 0)
                {
                    int flag = 1;
                    for (int i = 8; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[8] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send list-all ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv list-all resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "list-files", 10) == 0)
                {
                    int flag = 1;
                    for (int i = 10; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[10] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send list-files ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv list-files resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else if (strncmp(command, "download: ", 10) == 0 && strlen(command) > 10)
                {
                    int flag = 1;
                    for (int i = 10; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] < 33 || command[i] > 126)
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send download ftp.\n");
                            exit(0);
                        }
                        int stop=0;
                        //nu trece de restrictii:
                        //[to-do]: atentie si la cealalta restrictie sa trimit si mesaj bun,nu numai rau aici
                        if(-1 == recv(sd,rasp,MAX_RESPONSE,0))
                        {
                            printf("Error recv download good.\n");
                            exit(0);
                        }
                        printf("%s\n",rasp);
                        if(strncmp(rasp,"Client is not allowed",21)==0)
                        {
                            stop=1;
                        }
                        strcpy(rasp,"Ok");
                        if(-1 == send(sd,rasp,strlen(rasp)+1,0))
                        {
                            printf("[client]Error send approve download.\n");
                            exit(0);
                        }

                        if(stop)
                        {
                            continue;
                        }

                        //[to-do]receive the file:
                        //[to-do]if files exists, give different name;
                        strcpy(command,command+10);
                        int fd;
                        
                        if (-1 == (fd = open(command, O_RDWR | O_CREAT,0777)))
                        {
                            perror("[client]Eroare creare fisier.\n");
                            exit(0);
                        }
                        //freebsd.cs.nctu.edu.tw

                        char ch;
                        int bytes_read;
                        //int stop;
                        bytes_read = 0;
                        /*if(-1 == recv(sd,&stop,sizeof(int),0))
                        {
                            perror("WTF is this./n");
                            exit(0);
                        }
                        printf("\nstop:%d\n",stop);*/
                        
                        while (1)
                        {
                            /*if(-1 == recv(sd,&stop,sizeof(int),0))
                            {
                                perror("This is a problem.\n");
                            }
                            if(stop==1)
                            {
                                break;
                            }*/
                            int aux = recv(sd, &ch, sizeof(char), 0);
                            if(ch==0)
                            {
                                printf("Download complete.\n");
                                break;
                            }
                            if (aux == 0)
                            {
                                printf("[proxy]Download completed.\n");
                                break;
                            }
                            if (aux < 0)
                            {
                                printf("[proxy]Download interrupted.\n");
                                break;
                            }
                            if (write(fd, &ch, sizeof(char)) < 0)
                            {
                                printf("[proxy]Writing in file failed.\n");
                                break;
                            }
                            bytes_read++;
                        }

                        close(fd);

                        strcpy(command,"Done");
                        if(-1 == send(sd,command,strlen(command)+1,0))
                        {
                            printf("[client]error send done download.\n");
                            exit(0);
                        }

                        if(-1 == recv(sd,rasp,MAX_RESPONSE,0))
                        {
                            printf("[client]error recv done download.\n");
                            exit(0);
                        }
                        printf("%s\n",rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect(or filename incorrect). Retry.\n");
                    }
                }
                else if (strncmp(command, "move-down: ", 11) == 0 && strlen(command) > 11)
                {
                    int flag = 1;
                    for (int i = 11; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] < 33 || command[i] > 126)
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("[client]Error send move-down ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv move-down resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect(or directory name incorrect). Retry.\n");
                    }
                }
                else if (strncmp(command, "move-up", 7) == 0)
                {
                    int flag = 1;
                    for (int i = 7; i < strlen(command) && flag; ++i)
                    {
                        if (command[i] != ' ')
                        {
                            flag = 0;
                        }
                    }

                    if (flag == 1)
                    {
                        command[7] = 0;
                        if (-1 == send(sd, command, strlen(command) + 1, 0))
                        {
                            printf("Error send move-up ftp.\n");
                            exit(0);
                        }

                        if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
                        {
                            printf("[client]Error recv move-up resp.\n");
                            exit(0);
                        }
                        printf("%s\n", rasp);
                    }
                    else
                    {
                        printf("[client]Command format incorrect. Retry.\n");
                    }
                }
                else
                {
                    printf("[client]Command format incorrect. Retry.\n");
                }
            }
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else if (strncmp(command, "logout", 6) == 0)
    {
        int flag = 1;
        if (command[len - 1] == '\n')
        {
            command[--len] = 0;
        }
        for (int i = 6; i < len && flag; ++i)
        {
            if (command[i] != ' ')
            {
                flag = 0;
            }
        }

        if (flag == 1)
        {
            command[6] = 0;
            strcat(command, ": ");
            strcat(command, CURRENT_USER);

            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending logout command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving logout response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);

            if (strncmp(rasp, "Success", 7) == 0)
            {
                USER_LOGGED = 0;
                ADMIN_LOGGED = 0;
            }
        }
        else
        {
            command_error(command_count, command_list);
        }
    }
    else if (strncmp(command, "exit", 4) == 0)
    {
        int flag = 1;
        for (int i = 4; i < len && flag; ++i)
        {
            if (command[i] != ' ' && command[i] != '\n')
            {
                flag = 0;
                command_error(command_count, command_list);
            }
        }

        if (flag == 1)
        {
            strcpy(command, "logout: ");
            strcat(command, CURRENT_USER);

            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending logout command to proxy.\n");
                exit(0);
            }

            char rasp[MAX_RESPONSE];
            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving logout response from proxy.\n");
                exit(0);
            }

            printf("%s\n", rasp);

            if (strncmp(rasp, "Success", 7) == 0)
            {
                USER_LOGGED = 0;
                ADMIN_LOGGED = 0;
            }

            strcpy(command, "exit");
            if (-1 == send(sd, command, strlen(command) + 1, 0))
            {
                printf("[client]Error sending exit command to proxy.\n");
                exit(0);
            }

            if (-1 == recv(sd, rasp, MAX_RESPONSE, 0))
            {
                printf("[client]Error receiving exit response from proxy.\n");
                exit(0);
            }
            printf("%s\n", rasp);
            sleep(3);
            close(sd);
            exit(0);
        }
        else
        {
            command_error(command_count, command_list);
        }
        //[to-do]: raspuns server + exit()
    }
}

int command_sequence(int *sd)
{
    char command[150];

    while (1)
    {
        if (USER_LOGGED == 0 && ADMIN_LOGGED == 0) // noone logged in
        {
            int command_count = 3;
            char *command_list[] = {"login: [username]",
                                    "create: [username] [password]",
                                    "exit"};

            printf("You may introduce one command from the following list:\n");
            for (int i = 0; i < command_count; ++i)
            {
                printf("\t\t%s\n", command_list[i]);
            }
            printf("\nYou must login before using additional commands.\n");

            printf("Please insert your command:\n");
            fgets(command, 150, stdin);

            mode00(command, (*sd));
        }
        else if (USER_LOGGED == 1 && ADMIN_LOGGED == 0) // normal user
        {
            int command_count = 3;
            char *command_list[] = {"logout",
                                    "server: [server-name]",
                                    "exit"};

            printf("You may introduce one command from the following list:\n");
            for (int i = 0; i < command_count; ++i)
            {
                printf("\t\t%s\n", command_list[i]);
            }

            printf("Please insert your command:\n");
            fgets(command, 150, stdin);

            mode10(command, (*sd));
        }
        else if (USER_LOGGED == 1 && ADMIN_LOGGED == 1) // admin user
        {
            /*comenzi disponibile:
                login: ...(mesaj informativ: you must logout first!)
                create: ...(mesaj informativ: you must logout first!)
                logout
                server: server-name
                create-admin: username password
                forbidden
                exit-proxy
                exit*/
            int command_count = 6;
            char *command_list[] = {"logout",
                                    "server: [server-name]",
                                    "create-admin: [username] [password]",
                                    "forbidden",
                                    "exit-proxy",
                                    "exit"};

            printf("You may introduce one command from the following list:\n");
            for (int i = 0; i < command_count; ++i)
            {
                printf("\t\t%s\n", command_list[i]);
            }

            printf("Please insert your command:\n");
            fgets(command, 150, stdin);

            mode11(command, (*sd));
        }
    }
}

int main(int argc, char *argv[])
{
    int sd;

    welcome();

    init_connection(argc, argv, &sd);

    command_sequence(&sd);

    close(sd);
}