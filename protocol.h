#pragma once
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
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include "cjson/cJSON.h"

#define MAX_CMD 4096
#define MAX_JSON 8192
#define LONG_RESPONSE 100 * 4096
#define MAX_PATH 2048
#define MAX_COMMAND 300
#define ANONYMOUS "anonymous"

static char json_path[PATH_MAX];
static char database_path[PATH_MAX];

static int socket_cmd;
static int port_cmd = 21; // basic port for ftp
static char buffer_cmd[MAX_CMD];
static int socket_data;

//[TO-DO]: ATENTIE LA CITIREA RASPUNSURILOR CU MAI MULTE LINII!!!!!!!!!
//[to-do]: functiile de listare sunt doar cu detalii/fara detalii
//          ca sa vad daca e directory trebuie sa ma uit in LIST: drwxr-xr-x: prima litera => directory, altfel e file

int create_tmp()
{
    DIR *dir = opendir("tmp"); // director temporar cu fisierele din proxy
    if (dir)
    {
        /* Directory exists. */
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        /* Directory does not exist. */
        if (mkdir("tmp", 0777) == -1)
        {
            printf("[proxy]Error creating temporary director\n");
            return 0;
        }
    }
    else
    {
        printf("[proxy]Eroare la deschidere director temporar.\n");
        return 0;
    }

    if (-1 == chdir("tmp"))
    {
        printf("[proxy]Error entering tmp folder.\n");
        return 0;
    }

    return 1;
}

// send command:
int ftp_send_cmd(char *cmd)
{
    printf("[proxy]Sending command: %s\n", cmd);

    if (send(socket_cmd, cmd, strlen(cmd), 0) < 0)
    {
        printf("[proxy]Command was not sent.\n");
        return 0;
    }
    return 1;
}

// receive answer to command:
int ftp_recv_cmd(char *rasp, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        if (recv(socket_cmd, &rasp[i], 1, 0) < 0)
        {
            printf("[proxy]Error at receiving the answer.\n");
            return 0;
        }
        if (rasp[i] == '\n')
        {
            break;
        }
    }

    rasp[i + 1] = 0;
    printf("[proxy]Response:%s\n", rasp);
    char code[4];
    code[0] = rasp[0];
    code[1] = rasp[1];
    code[2] = rasp[2];
    code[3] = 0;
    return atoi(code); // codul corespunzator handlerului de la server
}

/* e.g.
        addr = ftp.pureftpd.org
        port = 21
        username = anonymous
        password = anonymous
*/
// OBS: in caz ca vrea sa dea ip-ul sa fac overload la functie[TO-DO]
int ftp_login(char *addr, int port, char *username, char *password)
{
    // printf("[proxy]Attempting to connect to the FTP-server...\n");

    struct sockaddr_in serv_addr;

    struct hostent *server = gethostbyname(addr);
    if (server == NULL)
    {
        perror("[proxy]There is no such host\n");
        exit(0);
    }

    socket_cmd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_cmd < 0)
        perror("[proxy]Error opening command socket\n");

    explicit_bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(socket_cmd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[proxy]Error connecting the command socket.\n");
        return 0;
    }
    // printf("[proxy]Connected to the server.\n");

    // Waiting for the welcoming message:
    // printf("[proxy]Hello server!\n");
    do
    {
        explicit_bzero(buffer_cmd, MAX_CMD);
        if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 220)
        {
            printf("[proxy]Bad server!\n");
            close(socket_cmd);
            return 0;
        }
        printf("%s", buffer_cmd);
    } while (buffer_cmd[3] == '-');

    // printf("[proxy]Attempting to login...\n");
    //  send username:
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "USER %s\n", username);
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Login failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, MAX_CMD);
    //////////////////////////////////////////////////  aici s-ar putea sa fi incurcat treaba
    // CAZUL IN CARE SERVERUL CERE PAROLA: 331
    //////////////////////////////////////////////////
    // CAZUL IN CARE SERVERUL NU CERE PAROLA : 220
    //////////////////////////////////////////////////
    int aux = ftp_recv_cmd(buffer_cmd, MAX_CMD);

    if (aux == 331)
    {
        // send password:
        explicit_bzero(buffer_cmd, MAX_CMD);
        sprintf(buffer_cmd, "PASS %s\n", password);
        if (ftp_send_cmd(buffer_cmd) != 1)
        {
            printf("[proxy]Login failed\n");
            close(socket_cmd);
            return 0;
        }
        explicit_bzero(buffer_cmd, MAX_CMD);
        if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 230)
        {
            printf("[proxy]Password invalid\n");
            close(socket_cmd);
            return 0;
        }
    }
    else if (aux == 230)
    {
        // no pass required
    }
    else
    {
        printf("[proxy]User invalid\n");
        close(socket_cmd);
        return 0;
    }

    // printf("[proxy]Succesfully logged in.\n");
    // printf("[proxy]Please insert 'list-all' command to see if file is directory or not before attempting to download.\n");

    // set to binary mode:
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "TYPE I\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Binary mode failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, MAX_CMD);
    if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 200)
    {
        printf("[proxy]Binary mode invalid\n");
        close(socket_cmd);
        return 0;
    }

    return 1;
}

// modul pasiv, in care se face transferul de fisiere
int ftp_pasv(char *ipaddr, int *port)
{
    if (ftp_send_cmd("PASV\n") != 1)
    {
        printf("[proxy]Passive mode failed\n");
        return 0;
    }
    if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 227)
    {
        printf("[proxy]Entering passive mode failed\n");
        return 0;
    }

    int a, b, c, d, e, f;
    char *find = strchr(buffer_cmd, '(');
    sscanf(find, "(%d,%d,%d,%d,%d,%d)", &a, &b, &c, &d, &e, &f);
    sprintf(ipaddr, "%d.%d.%d.%d", a, b, c, d);
    (*port) = e * 256 + f;
    // printf("[proxy]IP si portul de date: %s; %d;\n", ipaddr, *port);
    return 1;
}

int ftp_change_to_parentdirectory(int client)
{
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "CDUP\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Changing to parent working directory command failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, MAX_CMD);
    if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 250)
    {
        printf("[proxy]Parent cwd cmd invalid\n");
        close(socket_cmd);
        return 0;
    }
    else
    {
        if (-1 == chdir(".."))
        {
            printf("[proxy]Eroare la schimbare to parent directory in proxy: ..\n");
            return 0;
        }
    }

    explicit_bzero(buffer_cmd, MAX_CMD);
    strcpy(buffer_cmd, "Returned to parent directory.\nRecommended to list the available file list again...\n");
    if (-1 == send(client, buffer_cmd, strlen(buffer_cmd) + 1, 0))
    {
        printf("[proxy]Error send move-up complete.\n");
        exit(0);
    }

    // printf("[proxy]Returned to parent directory\n");
    // printf("[proxy]Recommended to list the available file list again...\n");
}

int ftp_change_wdirectory(char *dir, int client)
{
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "CWD %s\n", dir);
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Changing working directory command failed\n");
        close(socket_cmd);
        return 0;
    }
    do
    {
        explicit_bzero(buffer_cmd, MAX_CMD);
        if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 250)
        {
            printf("[proxy]Cwd cmd invalid\n");
            close(socket_cmd);
            return 0;
        }
    } while (buffer_cmd[3] == '-');

    DIR *direkt = opendir(dir); // director clona pentru directorul din ftp
    if (direkt)
    {
        /* Directory exists. */
        closedir(direkt);
    }
    else if (ENOENT == errno)
    {
        /* Directory does not exist. */
        if (mkdir(dir, 0777) == -1)
        {
            printf("[proxy]Error creating clone director: %s\n", dir);
            return 0;
        }
    }
    else
    {
        printf("[proxy]Eroare la deschidere director clona:%s.\n", dir);
        return 0;
    }

    if (-1 == chdir(dir))
    {
        printf("[proxy]Eroare la schimbare working directory in proxy:%s\n", dir);
        return 0;
    }

    // printf("[proxy]Current working directory:%s\n",dir);
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "Current working directory:%s\n", dir);
    if (-1 == send(client, buffer_cmd, strlen(buffer_cmd) + 1, 0))
    {
        printf("[proxy]Error send move-down complete.\n");
        exit(0);
    }

    return 1;
}

int ftp_list_all(int client)
{
    char rasp[LONG_RESPONSE] = "";
    // partea de conectare la data channel:
    struct sockaddr_in server;
    char ipaddr[32];
    int port;

    if (ftp_pasv(ipaddr, &port) != 1)
    {
        printf("[proxy]Passive mode failed in list\n");
        return 0;
    }
    // printf("IP SI PORT:%s; %d;\n",ipaddr,port);

    socket_data = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_data < 0)
        perror("[proxy]Error making data socket in list\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ipaddr);
    server.sin_port = htons(port);

    if (connect(socket_data, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0)
    {
        perror("[proxy]Failed data tunnel in list\n");
        return 0;
    }

    // lists directories and files from ftp server:
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "LIST\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]List command failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, MAX_CMD);
    if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 150)
    {
        printf("[proxy]List cmd invalid\n");
        close(socket_cmd);
        return 0;
    }

    /*printf("The files with their lines beginning with 'd' are directories.\n");
    printf("You can download every other file(that does not have 'd' as their first letter in line).\n");
    printf("List of all directories and files available in the current working directory:\n");*/

    strcat(rasp, "The files with their lines beginning with 'd' are directories.\n");
    strcat(rasp, "You can download every other file(that does not have 'd' as their first letter in line).\n");
    strcat(rasp, "List of all directories and files available in the current working directory:\n");

    int contor = strlen(rasp);
    char ch;
    while (1)
    {
        int aux = recv(socket_data, &ch, sizeof(char), 0);
        rasp[contor++] = ch;
        if (aux == 0)
        {
            printf("[proxy]Listing completed.\n");
            break;
        }
        if (aux < 0)
        {
            printf("[proxy]Listing interrupted.\n");
            break;
        }
        if (write(0, &ch, sizeof(char)) < 0)
        {
            printf("[proxy]Writing in stdout failed.\n");
            break;
        }
    }
    rasp[contor] = 0;

    close(socket_data);

    do
    {
        explicit_bzero(buffer_cmd, MAX_CMD);
        if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 226)
        {
            printf("[proxy]List cmd troublesome\n");
            close(socket_cmd);
            return 0;
        }
    } while (buffer_cmd[3] == '-');

    if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
    {
        printf("[proxy]Error send list-all complete.\n");
        exit(0);
    }

    return 1;
}

int ftp_list_files(int client)
{
    char rasp[LONG_RESPONSE] = "";
    // partea de conectare la data channel:
    struct sockaddr_in server;
    char ipaddr[32];
    int port;

    if (ftp_pasv(ipaddr, &port) != 1)
    {
        printf("[proxy]Passive mode failed in list\n");
        return 0;
    }
    // printf("IP SI PORT:%s; %d;\n",ipaddr,port);

    socket_data = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_data < 0)
        perror("[proxy]Error making data socket in list\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ipaddr);
    server.sin_port = htons(port);

    if (connect(socket_data, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0)
    {
        perror("[proxy]Failed data tunnel in list\n");
        return 0;
    }

    // lists files from ftp server:
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "NLST\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]List command failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, MAX_CMD);
    if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 150)
    {
        printf("[proxy]List cmd invalid\n");
        close(socket_cmd);
        return 0;
    }

    strcat(rasp, "List of files and directories names:\n");
    int contor = strlen(rasp);
    char ch;
    while (1)
    {
        int aux = recv(socket_data, &ch, sizeof(char), 0);
        rasp[contor++] = ch;
        if (aux == 0)
        {
            printf("[proxy]Listing completed.\n");
            break;
        }
        if (aux < 0)
        {
            printf("[proxy]Listing interrupted.\n");
            break;
        }
        if (write(0, &ch, sizeof(char)) < 0)
        {
            printf("[proxy]Writing in stdout failed.\n");
            break;
        }
    }
    rasp[contor] = 0;

    close(socket_data);

    do
    {
        explicit_bzero(buffer_cmd, MAX_CMD);
        if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 226)
        {
            printf("[proxy]List cmd troublesome\n");
            close(socket_cmd);
            return 0;
        }
    } while (buffer_cmd[3] == '-');

    if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
    {
        printf("[proxy]Error send list-files complete.\n");
        exit(0);
    }

    return 1;
}

int ftp_help(int client)
{
    char rasp[LONG_RESPONSE] = "";
    // get help from ftp server:
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "HELP\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Help command failed\n");
        close(socket_cmd);
        return 0;
    }

    // citire comenzi help(in afara de prima si ultima linie, nu sunt precedate de cod)
    explicit_bzero(buffer_cmd, MAX_CMD);
    int aux = ftp_recv_cmd(buffer_cmd, MAX_CMD);
    strcat(rasp, buffer_cmd + 4);
    if (aux != 214)
    {
        printf("[proxy]Help cmd invalid\n");
        close(socket_cmd);
        return 0;
    }
    while (1)
    {
        explicit_bzero(buffer_cmd, MAX_CMD);
        aux = ftp_recv_cmd(buffer_cmd, MAX_CMD);
        if (aux == 214 && buffer_cmd[3] != '-')
        {
            break;
        }
        else
        {
            strcat(rasp, buffer_cmd);
        }
    }

    if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
    {
        printf("[proxy]Error send help complete.\n");
        exit(0);
    }

    return 1;
}

void ftp_quit()
{
    ftp_send_cmd("QUIT\n");
    close(socket_cmd);
}

int ftp_filesize(char *name)
{
    int size;
    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "SIZE %s\n", name);
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Failed to send: retrieve filesize\n");
        return 0;
    }
    explicit_bzero(buffer_cmd, MAX_CMD);
    if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 213)
    {
        printf("[proxy]Failed to retrieve filesize\n");
        return 0;
    }
    size = atoi(buffer_cmd + 4);
    return size;
}

int delete_directories()
{
    char path[MAX_PATH];

    getcwd(path, MAX_PATH);

    int count = 0, flag = 0;
    char *p = strtok(path, "/");
    while (p != NULL)
    {
        if (flag == 1)
        {
            count++;
        }
        if (strcmp(p, "tmp") == 0)
        {
            flag = 1;
        }
        p = strtok(NULL, "/");
    }
    for (int i = 0; i <= count; ++i)
    {
        if (-1 == chdir(".."))
        {
            printf("[proxy]Error moving to parent directory in delete.\n");
            exit(0);
        }
    }

    int pid;
    if (-1 == (pid = fork()))
    {
        printf("[proxy]Error fork delete.\n");
        exit(0);
    }

    if (pid > 0)
    {
        printf("[proxy]Temporary directory deleted.\n");
        return 1;
    }
    else if (pid == 0)
    {
        execlp("rm", "rm", "-r", "tmp", NULL);
        printf("[proxy]Error execlp delete.\n");
        exit(0);
    }
    // printf("Current working directory: %s\n", path);
}

int create_directories(char *path)
{
    int count_chdir = 0;
    char *p = strtok(path, "/"); // printf("come on:%s\n",p);
    char *r;
    while (p != NULL)
    {
        while (strlen(p) < 1)
        {
            p = strtok(NULL, "/");
        }
        r = p;
        p = strtok(NULL, "/"); // printf("bish direcct:%s; %s;\n",p,r);
        if (p == NULL)         // am ajuns la final
        {
            char s[100];
            int fd_aux; // printf("biiish: %s\n",getcwd(s,100));
            if (-1 == (fd_aux = open(r, O_RDWR | O_CREAT | O_TRUNC, 0777)))
            {
                perror("[proxy]Eroare la  creare fisier.\n");
                return 0;
            }

            close(fd_aux);
        }
        else
        {
            DIR *dir = opendir(r);
            if (dir)
            {
                /* Directory exists. */
                closedir(dir);
            }
            else if (ENOENT == errno)
            {
                /* Directory does not exist. */
                if (mkdir(r, 0777) == -1)
                {
                    printf("[proxy]Error creating %s director\n", r);
                    return 0;
                }
            }
            else
            {
                printf("[proxy]Eroare la deschidere director pentru fisier %s.\n", r);
                return 0;
            }

            count_chdir++;
            if (-1 == chdir(r))
            {
                perror("[proxy]Eroare la schimbare director.\n");
                return 0;
            }
        }
    }
    for (int i = 0; i < count_chdir; ++i)
    {
        if (-1 == chdir(".."))
        {
            perror("[proxy]Error returning through directories.\n");
            return 0;
        }
    }

    return 1;
}

void package_reading(int *bytes_read, char *name)
{
    /*
    //printf("here?\n");
    // portiunea asta pot sa o fac la deschiderea proxy-ului;
    DIR *dir = opendir("tmp"); // director temporar cu fisierele din proxy
    if (dir)
    {
        // Directory exists.
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        // Directory does not exist.
        if (mkdir("tmp", 0777) == -1)
        {
            printf("[proxy]Error creating temporary director\n");
            exit(0);
        }
    }
    else
    {
        printf("[proxy]Eroare la deschidere director temporar.\n");
        exit(0);
    }*/
    ////////////////////////////
    // printf("here?\n");
    int fd;
    char path[PATH_MAX];
    char file[PATH_MAX];

    /*strcpy(path, "tmp"); //asa era cand nu faceam chdir la inceput in 'tmp'
    strcat(path, name);//printf("pathul:%s\n",path); */
    strcpy(path, name); // aste e nou;
    strcpy(file, "./");
    strcat(file, path);
    create_directories(path); // printf("fisierul: %s\n",file);
    if (-1 == (fd = open(file, O_RDWR)))
    {
        perror("[proxy]Eroare creare fisier.\n");
        exit(0);
    }

    // printf("[proxy]Begin downloading...\n");

    char ch;
    (*bytes_read) = 0;
    while (1)
    {
        int aux = recv(socket_data, &ch, sizeof(char), 0);
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
            printf("[proxy]Writing in tmp failed.\n");
            break;
        }
        (*bytes_read)++;
    }

    close(fd);
}

int check_file(char* name,int client)
{
    int fd;
    if(-1 == (fd = open(json_path,O_RDWR)))
    {
        perror("[proxy]Error opening json file.\n");
        exit(0);
    }

    char file[MAX_JSON];
    if(-1 == read(fd, file, MAX_JSON))
    {
        printf("[proxy]Error reading from json file.\n");
        exit(0);
    }

    close(fd);

    cJSON *maxsize = NULL;
    cJSON *filesallowed = NULL;
    cJSON *restrictii = cJSON_Parse(file);
    if(restrictii == NULL)
    {
        return -1;
    }

    maxsize = cJSON_GetObjectItemCaseSensitive(restrictii,"maxsize");
    filesallowed = cJSON_GetObjectItemCaseSensitive(restrictii,"filesallowed");

    long long int marime;
    char aux[150];
    strcpy(aux,maxsize->valuestring);
    aux[strlen(aux)-2]=0;
    sscanf(aux,"%lld",&marime);
    marime*=1024;

    if(ftp_filesize(name)>marime)
    {
        printf("[proxy]Dimensiune prea mare:%d > %lld\n",ftp_filesize(name),marime);
        return 0;
    }

    int flag=0;
    cJSON* type;
    cJSON_ArrayForEach(type,filesallowed)
    {
        int contor=0;
        int contor2=strlen(type->valuestring)-1;
        while(type->valuestring[contor2]!='.')
        {
            aux[contor++]=type->valuestring[contor2--];
        }
        aux[contor]=0;
        
        char term[30];
        contor2=0;
        for(int i=contor-1;i>=0;--i)
        {
            term[contor2++]=aux[i];
        }
        term[contor2]=0;

        if(strcmp(term,type->valuestring))
        {
            flag=1;
        }
    }

    if(flag==0)
    {
        printf("[proxy]Tip fisier nu e permis.\n");
        return 0;
    }

    return 1;
}

// name=path fisier
int ftp_download(char *name, int client)
{
    char rasp[MAX_CMD];
    //check restrictii:
    if(check_file(name,client)==0)
    {//checkpoint
        
        strcpy(rasp,"Client is not allowed to download this file");
        if(-1 == send(client,rasp,strlen(rasp)+1,0))
        {
            printf("[proxy]Error send download approve.\n");
            exit(0);
        }
        if(-1 == recv(client,rasp,MAX_CMD,0))
        {
            printf("[proxy]Error recv continue download.\n");
            exit(0);
        }
        return 0;
    }
    else
    {
        strcpy(rasp,"Good to go");
        if(-1 == send(client,rasp,strlen(rasp)+1,0))
        {
            printf("[proxy]Error send download approve.\n");
            exit(0);
        }
        if(-1 == recv(client,rasp,MAX_CMD,0))
        {
            printf("[proxy]Error recv continue download.\n");
            exit(0);
        }
    }

    

    // char rasp[LONG_RESPONSE]="";
    if (access(name, F_OK) == 0)
    {
        // file exists
    }
    else
    { // file doesn't exists
        struct sockaddr_in server;
        char ipaddr[32];
        int port;

        if (ftp_pasv(ipaddr, &port) != 1)
        {
            printf("[proxy]Passive mode failed\n");
            return 0;
        }
        // printf("IP SI PORT:%s; %d;\n",ipaddr,port);

        socket_data = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_data < 0)
            perror("[proxy]Error making data socket\n");

        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(ipaddr);
        server.sin_port = htons(port);

        if (connect(socket_data, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0)
        {
            perror("[proxy]Failed data tunnel\n");
            return 0;
        }

        // send download command:
        explicit_bzero(buffer_cmd, MAX_CMD);
        sprintf(buffer_cmd, "RETR %s\n", name);
        if (ftp_send_cmd(buffer_cmd) != 1)
        {
            printf("[proxy]Failed retr command\n");
            return 0;
        }
        // printf("retr sent?\n");
        explicit_bzero(buffer_cmd, MAX_CMD);
        if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 150)
        {
            printf("[proxy]Failed retrieving file\n");
            close(socket_data);
            return 0;
        }

        int bytes = 0;
        package_reading(&bytes, name); // functie de citire pe pachete si depozitare in folder temporar
        explicit_bzero(buffer_cmd, MAX_CMD);
        do
        {
            if (ftp_recv_cmd(buffer_cmd, MAX_CMD) != 226)
            {
                printf("[proxy]Failed retrieving file.\n");
                return 0;
            }
        } while (buffer_cmd[3] == '-');

        printf("[proxy]Downloaded %d/%d bytes.\n", bytes, ftp_filesize(name));
        close(socket_data);
        explicit_bzero(buffer_cmd, MAX_CMD);
    }

    // download pentru clientul proxy-ului:
    int fd;
    if (-1 == (fd = open(name, O_RDWR)))
    {
        perror("[proxy]Eroare creare fisier.\n");
        exit(0);
    }

    //printf("\nbefore sending\n");
    
    char ch[MAX_CMD];
    int bytes_written;
    int stop=1,aux;
    bytes_written = 0;
    /*if(-1 == send(client,&stop,sizeof(int),0))
    {
        perror("WTF HERE??");
        exit(0);
    }
    printf("\nstop:%d\n",stop);*/
    while (1)
    {
        if(-1 ==(aux = read(fd, ch, MAX_CMD)))
        {
            perror("Eroare fking fisier.\n");
        }
        //printf("\n%s\n",ch);
        if (aux == 0)
        {
            //stop=1;
            //send(client,&stop,sizeof(int),0);
            printf("[proxy]Download completed.\n");
            break;
        }
        if (aux < 0)
        {
            printf("[proxy]Download interrupted.\n");
            break;
        }
        if(aux>0)
        {
            //send(client,&stop,sizeof(int),0);
            if (send(client, ch, strlen(ch), 0) < 0)
            {
                printf("[proxy]Writing in client download failed.\n");
                break;
            }
            bytes_written+=aux;
        }
    }
    char end=0;
    if (send(client, &end, sizeof(char), 0) < 0)
    {
        printf("[proxy]Writing in client download end failed.\n");
        exit(0);
    }
    
    //printf("\nafter sending\n");

    if (-1 == recv(client, buffer_cmd, MAX_CMD, 0))
    {
        printf("[proxy]Error recv done donwload.\n");
        exit(0);
    }

    explicit_bzero(buffer_cmd, MAX_CMD);
    sprintf(buffer_cmd, "Downloaded %d/%d bytes.\n", bytes_written, ftp_filesize(name));
    if (-1 == send(client, buffer_cmd, strlen(buffer_cmd), 0))
    {
        printf("Error download.\n");
        exit(0);
    }

    close(fd);

    return 1;
}

int check_domain(char* server_ftp)
{
    int fd;
    if(-1 == (fd = open(json_path,O_RDWR)))
    {
        printf("[proxy]Error opening json file.\n");
        exit(0);
    }

    char file[MAX_JSON];
    if(-1 == read(fd, file, MAX_JSON))
    {
        printf("[proxy]Error reading from json file.\n");
        exit(0);
    }

    close(fd);

    cJSON *serverdomains = NULL;
    cJSON *restrictii = cJSON_Parse(file);
    if(restrictii == NULL)
    {
        return -1;
    }
    serverdomains=cJSON_GetObjectItemCaseSensitive(restrictii,"serverdomains");
    cJSON *serverdomain = NULL;
    cJSON_ArrayForEach(serverdomain,serverdomains)
    {
        cJSON *name= cJSON_GetObjectItemCaseSensitive(serverdomain,"name");
        cJSON *clienti = cJSON_GetObjectItemCaseSensitive(serverdomain,"clientsrestricted");
        cJSON *days=cJSON_GetObjectItemCaseSensitive(serverdomain,"daysforbidden");
        cJSON *hours=cJSON_GetObjectItemCaseSensitive(serverdomain,"hoursforbidden");

        if(strstr(server_ftp,name->valuestring)!=NULL)
        {
            //sa iau numele clientului
            char nume_client[500];
            gethostname(nume_client,500);

            cJSON *client;
            cJSON_ArrayForEach(client,clienti)
            {
                if(strstr(nume_client,client->valuestring)!=NULL)
                {
                    time_t t = time(NULL);
                    struct tm tm = *localtime(&t);
                    int ora;
                    char zi[20];
                    ora=tm.tm_hour;

                    if(tm.tm_wday==0)
                    {
                        strcpy(zi,"sunday");
                    }
                    else if(tm.tm_wday==1)
                    {
                        strcpy(zi,"monday");
                    }
                    else if(tm.tm_wday==2)
                    {
                        strcpy(zi,"tuesday");
                    }
                    else if(tm.tm_wday==3)
                    {
                        strcpy(zi,"wednesday");
                    }
                    else if(tm.tm_wday==4)
                    {
                        strcpy(zi,"thursday");
                    }
                    else if(tm.tm_wday==5)
                    {
                        strcpy(zi,"friday");
                    }
                    else if(tm.tm_wday==6)
                    {
                        strcpy(zi,"saturday");
                    }

                    cJSON* day;
                    cJSON_ArrayForEach(day,days)
                    {
                        if(strcmp(day->valuestring,zi)==0)
                        {
                            cJSON* hour;
                            cJSON_ArrayForEach(hour,hours)
                            {
                                if(ora==hour->valueint)
                                {
                                    return 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return 1;
}

int ftp_mode(char *server_ftp, int client)
{
    int command_count = 7;
    char *command_list[] = {"help",
                            "exit",
                            "list-all",
                            "list-files",
                            "download: [filename]",
                            "move-down: [directory-name]",
                            "move-up"};

    //[to-do]verificare restrictii!!!
    //verificare serverdomains:
    char rasp[MAX_CMD];
    if(check_domain(server_ftp)==0)
    {
        
        strcpy(rasp,"Client is not allowed to connect to this ftp.");
        if(-1 == send(client,rasp,strlen(rasp)+1,0))
        {
            printf("[proxy]error send not allowed.\n");
            exit(0);
        }

        if(-1 == recv(client,rasp,MAX_CMD,0))
        {
            printf("[proxy]ERror server approve recv.\n");
            exit(0);
        }

        return 0;
    }
    else
    {
        strcpy(rasp,"Good to go.");
        if(-1 == send(client,rasp,strlen(rasp)+1,0))
        {
            printf("[proxy]error send not allowed.\n");
            exit(0);
        }

        if(-1 == recv(client,rasp,MAX_CMD,0))
        {
            printf("[proxy]ERror server approve recv.\n");
            exit(0);
        }
    }

    if (ftp_login(server_ftp, port_cmd, ANONYMOUS, ANONYMOUS) != 1)
    {
        printf("[proxy]Eroare la conectare prxoy<->ftp.\n");
        exit(0);
    }

    if (create_tmp() != 1)
    {
        printf("[proxy]Eroare la creare folder temporar pe proxy.\n");
        exit(0);
    }

    // printf("These are the available commands:\n");
    //char rasp[MAX_CMD];
    strcpy(rasp, "These are the available commands:\n");
    for (int i = 0; i < command_count; ++i)
    {
        // printf("\t\t\t%s\n",command_list[i]);
        char aux[MAX_COMMAND];
        sprintf(aux, "\t\t\t%s\n", command_list[i]);
        strcat(rasp, aux);
    }
    strcat(rasp, "Please insert 'list-all' command to see if file is directory or not before attempting to download.\n");

    if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
    {
        printf("[proxy]Eroare send ftp list.\n");
        exit(0);
    }

    char comanda[MAX_COMMAND];
    while (1)
    {
        if (-1 == recv(client, comanda, MAX_COMMAND, 0))
        {
            printf("[proxy]Error recv ftp command.\n");
            exit(0);
        }

        if (comanda[strlen(comanda) - 1] == '\n')
        {
            comanda[strlen(comanda) - 1] = 0;
        }

        if (strncmp(comanda, "exit", 4) == 0)
        {
            strcpy(rasp, "Exit command completed.\n");
            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Error send exit complete.\n");
                exit(0);
            }
            break;
        }
        else if (strncmp(comanda, "help", 4) == 0)
        {
            ftp_help(client);
        }
        else if (strncmp(comanda, "list-all", 8) == 0)
        {
            ftp_list_all(client);
        }
        else if (strncmp(comanda, "list-files", 10) == 0)
        {
            ftp_list_files(client);
        }
        else if (strncmp(comanda, "download: ", 10) == 0)
        {
            strcpy(comanda, comanda + 10);
            ftp_download(comanda, client);
        }
        else if (strncmp(comanda, "move-down: ", 11) == 0)
        {
            strcpy(comanda, comanda + 11);
            ftp_change_wdirectory(comanda, client);
        }
        else if (strncmp(comanda, "move-up", 7) == 0)
        {
            ftp_change_to_parentdirectory(client);
        }
    }

    if (delete_directories() != 1)
    {
        printf("[proxy]Eroare la stergere folder temporar pe proxy.\n");
        exit(0);
    }

    return 1;
}
