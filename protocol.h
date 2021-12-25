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

static int socket_cmd;
static int port_cmd = 21; // basic port for ftp
static char buffer_cmd[4096];
static int socket_data;

//[TO-DO]: ATENTIE LA CITIREA RASPUNSURILOR CU MAI MULTE LINII!!!!!!!!!

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

    if(-1 == chdir("tmp"))
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
    code[0]=rasp[0];
    code[1]=rasp[1];
    code[2]=rasp[2];
    code[3]=0;
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
    printf("[proxy]Attempting to connect to the FTP-server...\n");

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
    printf("[proxy]Connected to the server.\n");

    // Waiting for the welcoming message:
    printf("[proxy]Hello server!\n");
    do
    {
        explicit_bzero(buffer_cmd, 4096);
        if (ftp_recv_cmd(buffer_cmd, 4096) != 220)
        {
            printf("[proxy]Bad server!\n");
            close(socket_cmd);
            return 0;
        }
        printf("%s", buffer_cmd);
    } while (buffer_cmd[3]=='-');
    
    
    printf("[proxy]Attempting to login...\n");
    // send username:
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "USER %s\n", username);
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Login failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, 4096);
    //////////////////////////////////////////////////  aici s-ar putea sa fi incurcat treaba
    // CAZUL IN CARE SERVERUL CERE PAROLA: 331     
    //////////////////////////////////////////////////
    // CAZUL IN CARE SERVERUL NU CERE PAROLA : 220
    //////////////////////////////////////////////////
    int aux = ftp_recv_cmd(buffer_cmd, 4096);

    if (aux == 331)
    {
        // send password:
        explicit_bzero(buffer_cmd, 4096);
        sprintf(buffer_cmd, "PASS %s\n", password);
        if (ftp_send_cmd(buffer_cmd) != 1)
        {
            printf("[proxy]Login failed\n");
            close(socket_cmd);
            return 0;
        }
        explicit_bzero(buffer_cmd, 4096);
        if (ftp_recv_cmd(buffer_cmd, 4096) != 230)
        {
            printf("[proxy]Password invalid\n");
            close(socket_cmd);
            return 0;
        }
    }
    else
    {
        printf("[proxy]User invalid\n");
        close(socket_cmd);
        return 0;
    }

    printf("[proxy]Succesfully logged in.\n");
    
    // set to binary mode:
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "TYPE I\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Binary mode failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, 4096);
    if (ftp_recv_cmd(buffer_cmd, 4096) != 200)
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
    //printf("duude\n");
    if (ftp_send_cmd("PASV\n") != 1)
    {
        printf("[proxy]Passive mode failed\n");
        return 0;
    }
    if (ftp_recv_cmd(buffer_cmd, 4096) != 227)
    {
        printf("[proxy]Entering passive mode failed\n");
        return 0;
    }

    int a, b, c, d, e, f;
    char *find = strchr(buffer_cmd, '(');
    sscanf(find, "(%d,%d,%d,%d,%d,%d)", &a, &b, &c, &d, &e, &f);
    sprintf(ipaddr, "%d.%d.%d.%d", a, b, c, d);
    (*port) = e * 256 + f;
    printf("[proxy]IP si portul de date: %s; %d;\n", ipaddr, *port);
    return 1;
}

int ftp_change_to_parentdirectory()
{
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "CDUP\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Changing to parent working directory command failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, 4096);
    if (ftp_recv_cmd(buffer_cmd, 4096) != 250)
    {
        printf("[proxy]Parent cwd cmd invalid\n");
        close(socket_cmd);
        return 0;
    }

    printf("[proxy]Returned to parent directory\n");
    printf("[proxy]Recommended to list the available file list again...\n");
}

int ftp_change_wdirectory(char* dir)
{
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "CWD %s\n", dir);
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Changing working directory command failed\n");
        close(socket_cmd);
        return 0;
    }
    do
    {
        explicit_bzero(buffer_cmd, 4096);
        if (ftp_recv_cmd(buffer_cmd, 4096) != 250)
        {
            printf("[proxy]Cwd cmd invalid\n");
            close(socket_cmd);
            return 0;
        }
    } while (buffer_cmd[3]=='-');
    
    

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
            printf("[proxy]Error creating clone director: %s\n",dir);
            return 0;
        }
    }
    else
    {
        printf("[proxy]Eroare la deschidere director clona:%s.\n",dir);
        return 0;
    }

    if(-1 == chdir(dir))
    {
        printf("[proxy]Eroare la schimbare working directory in proxy:%s\n",dir);
        return 0;
    }

    printf("[proxy]Current working directory:%s\n",dir);

    return 1;
}

int ftp_list()
{
    //partea de conectare la data channel:
    struct sockaddr_in server;
    char ipaddr[32];
    int port;

    if (ftp_pasv(ipaddr, &port) != 1)
    {
        printf("[proxy]Passive mode failed in list\n");
        return 0;
    }
    //printf("IP SI PORT:%s; %d;\n",ipaddr,port);

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
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "NLST\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]List command failed\n");
        close(socket_cmd);
        return 0;
    }
    explicit_bzero(buffer_cmd, 4096);
    if (ftp_recv_cmd(buffer_cmd, 4096) != 150)
    {
        printf("[proxy]List cmd invalid\n");
        close(socket_cmd);
        return 0;
    }

    printf("List available:\n");
    //int* bytes_read;
    char ch;
    //(*bytes_read) = 0;
    while (1)
    {
        int aux = recv(socket_data, &ch, sizeof(char), 0);
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
        //(*bytes_read)++;
    }

    close(socket_data);

    do
    {
        explicit_bzero(buffer_cmd, 4096);
        if (ftp_recv_cmd(buffer_cmd, 4096) != 226)
        {
            printf("[proxy]List cmd troublesome\n");
            close(socket_cmd);
            return 0;
        }
    } while (buffer_cmd[3]=='-');

    return 1;
}

int ftp_help()
{
    // get help from ftp server:
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "HELP\n");
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Help command failed\n");
        close(socket_cmd);
        return 0;
    }

    //citire comenzi help(in afara de prima si ultima linie, nu sunt precedate de cod)
    explicit_bzero(buffer_cmd, 4096);
    int aux=ftp_recv_cmd(buffer_cmd, 4096);
    if (aux != 214)
    {
        printf("[proxy]Help cmd invalid\n");
        close(socket_cmd);
        return 0;
    }
    while(1)
    {
        explicit_bzero(buffer_cmd,4096);
        aux=ftp_recv_cmd(buffer_cmd,4096);
        if(aux == 214 && buffer_cmd[3]!='-')
        {
            break;
        }
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
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "SIZE %s\n", name);
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Failed to send: retrieve filesize\n");
        return 0;
    }
    explicit_bzero(buffer_cmd, 4096);
    if (ftp_recv_cmd(buffer_cmd, 4096) != 213)
    {
        printf("[proxy]Failed to retrieve filesize\n");
        return 0;
    }
    size = atoi(buffer_cmd + 4);
    return size;
}

int create_directories(char* path)
{
    int count_chdir=0;
    char *p=strtok(path,"/");//printf("come on:%s\n",p);
    char *r;
    while(p!=NULL)
    {
        while(strlen(p)<1){
            p=strtok(NULL,"/");
        }
        r=p;
        p=strtok(NULL,"/");//printf("bish direcct:%s; %s;\n",p,r);
        if(p==NULL)//am ajuns la final
        {
            char s[100];
            int fd_aux;//printf("biiish: %s\n",getcwd(s,100));
            if(-1 == (fd_aux = open(r, O_RDWR | O_CREAT | O_TRUNC, 0777)))
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
                    printf("[proxy]Error creating %s director\n",r);
                    return 0;
                }
            }
            else
            {
                printf("[proxy]Eroare la deschidere director pentru fisier %s.\n",r);
                return 0;
            }

            count_chdir++;
            if(-1 == chdir(r))
            {
                perror("[proxy]Eroare la schimbare director.\n");
                return 0;
            }

        }
        
    }
    for(int i=0;i<count_chdir;++i)
    {
        if(-1 == chdir(".."))
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
    //printf("here?\n");
    int fd;
    char path[PATH_MAX];
    char file[PATH_MAX];
    
    /*strcpy(path, "tmp"); //asa era cand nu faceam chdir la inceput in 'tmp'
    strcat(path, name);//printf("pathul:%s\n",path); */
    strcpy(path,name);//aste e nou;
    strcpy(file,"./");
    strcat(file,path);
    create_directories(path);//printf("fisierul: %s\n",file);
    if (-1 == (fd = open(file, O_RDWR)))
    {
        perror("[proxy]Eroare creare fisier.\n");
        exit(0);
    }

    printf("[proxy]Begin downloading...\n");

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

// name=path fisier
int ftp_download(char *name)
{
    struct sockaddr_in server;
    char ipaddr[32];
    int port;

    if (ftp_pasv(ipaddr, &port) != 1)
    {
        printf("[proxy]Passive mode failed\n");
        return 0;
    }
    //printf("IP SI PORT:%s; %d;\n",ipaddr,port);

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
    explicit_bzero(buffer_cmd, 4096);
    sprintf(buffer_cmd, "RETR %s\n", name);
    if (ftp_send_cmd(buffer_cmd) != 1)
    {
        printf("[proxy]Failed retr command\n");
        return 0;
    }
    //printf("retr sent?\n");
    explicit_bzero(buffer_cmd, 4096);
    if (ftp_recv_cmd(buffer_cmd, 4096) != 150)
    {
        printf("[proxy]Failed retrieving file\n");
        close(socket_data);
        return 0;
    }

    int bytes = 0;
    package_reading(&bytes, name); //printf("bish here:%s",name);// functie de citire pe pachete si depozitare in folder temporar
    explicit_bzero(buffer_cmd,4096);
    do
    {
        if(ftp_recv_cmd(buffer_cmd,4096)!=226)
        {
            printf("[proxy]Failed retrieving file.\n");
            return 0;
        }
    } while (buffer_cmd[3]=='-');
    
    
    printf("[proxy]Downloaded %d/%d bytes.\n", bytes, ftp_filesize(name));
    close(socket_data);
    explicit_bzero(buffer_cmd, 4096);
    return 1;
}