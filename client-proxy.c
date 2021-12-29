#include "sqlite/sqlite3.h"
#include "cjson/cJSON.h"
#include "protocol.h"

#define MAX_RESPONSE 4096
#define MAX_USERPASS 40
#define MAX_CLIENTS 15
#define PORT 2024

int clients_count;
int clients_pid[MAX_CLIENTS];

void my_handler(int signum)
{
    if (signum == SIGUSR1)
    {
        for (int i = 0; i < clients_count; ++i)
        {
            if (clients_pid[i] != -1)
            {
                kill(clients_pid[i], SIGTERM);
                clients_pid[i] = -1;
            }
        }
        exit(0);
    }
}

int init_connect(int *sd)
{
    if (SIG_ERR == signal(SIGUSR1, my_handler))
    {
        printf("Error signaling.\n");
        exit(0);
    }

    clients_count = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        clients_pid[i] = -1;
    }

    //setam pathurile de la json si database ca sa nu le pierdem cand ne mutam prin directoare:
    char symlinkpath[150];
    strcpy(symlinkpath,"rules.json");
    realpath(symlinkpath, json_path);

    strcpy(symlinkpath,"users.db");
    realpath(symlinkpath,database_path);

    struct sockaddr_in server;

    if (((*sd) = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[proxy]Eroare la socket().\n");
        exit(0);
    }

    explicit_bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind((*sd), (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[proxy]Eroare la bind().\n");
        exit(0);
    }

    return (*sd);
}

static int callback(void *data, int argc, char **argv, char **azColName)
{
    // interogare cu o singura linie rezultat(username,password,id,...)
    if (argc == 0)
    {
        strcpy((char *)data, "NULL");
    }
    else
    {
        strcpy((char *)data, argv[0]);
    }

    return 0;
}

int restrict_file_add(int client)
{
    int fd;
    if (-1 == (fd = open(json_path, O_RDWR)))
    {
        printf("[proxy]Error opening json file.\n");
        exit(0);
    }

    char file[MAX_JSON];
    if (-1 == read(fd, file, MAX_JSON))
    {
        printf("[proxy]Error reading from json file.\n");
        exit(0);
    }

    if(-1 == send(client,file,strlen(file)+1,0))
    {
        printf("[proxy]Error sending json file.\n");
        exit(0);
    }

    cJSON *maxsize = NULL;
    cJSON *filesallowed = NULL;
    cJSON *serverdomains = NULL;

    cJSON *restrictii = cJSON_Parse(file);
    if (restrictii == NULL)
    {
        return -1;
    }

    maxsize = cJSON_GetObjectItemCaseSensitive(restrictii, "maxsize");

    filesallowed = cJSON_GetObjectItemCaseSensitive(restrictii, "filesallowed");

    char rasp[MAX_RESPONSE];

    if (-1 == recv(client, rasp, MAX_RESPONSE, 0))
    {
        printf("[proxy]Error recv y or n.\n");
        exit(0);
    }

    if (rasp[0] == 'y')
    {
        while (1)
        {
            strcpy(rasp, "Please insert one at a time file type with lowercase letters only.(e.g 'xml')");
            strcat(rasp, " If you want to stop insert 'null'.");

            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Error sending add type files.\n");
                exit(0);
            }

            if (-1 == recv(client, rasp, MAX_RESPONSE, 0))
            {
                printf("[proxy]Error recv file type.\n");
                exit(0);
            }

            if (strcmp(rasp, "null") == 0)
            {
                break;
            }

            cJSON *nou = cJSON_CreateString(rasp);
            cJSON_AddItemToArray(filesallowed, nou);
        }
    }
    else if (rasp[1] == 'n')
    {
        // continue
    }

    serverdomains = cJSON_GetObjectItemCaseSensitive(restrictii, "serverdomains");

    strcpy(rasp, "Do you want to add other domain restriction?[y\\n]\n");
    if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
    {
        printf("[proxy]Error sending add domain question.\n");
        exit(0);
    }

    if (-1 == recv(client, rasp, MAX_RESPONSE, 0))
    {
        printf("[proxy]Error recv y or n domain.\n");
        exit(0);
    }

    if (rasp[0] == 'y')
    {
        while (1)
        {
            strcpy(rasp, "Please insert one at a time domain structure.");
            strcat(rasp, " If you want to stop insert 'null'.");
            strcat(rasp, " Insert domain name with lowercase letters only.(e.g '.uaic.ro')");

            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Error sending add domain name.\n");
                exit(0);
            }

            if (-1 == recv(client, rasp, MAX_RESPONSE, 0))
            {
                printf("[proxy]Error recv domain name.\n");
                exit(0);
            }

            if (strcmp(rasp, "null") == 0)
            {
                break;
            }

            cJSON *obj = cJSON_CreateObject();

            cJSON *nume = cJSON_CreateString(rasp);
            cJSON_AddItemToObject(obj, "name", nume);

            cJSON *clienti = cJSON_CreateArray();

            while (1)
            {
                strcpy(rasp, "Please insert one at a time client names.");
                strcat(rasp, " If you want to stop insert 'null'.");
                strcat(rasp, " Insert client name with lowercase letters only.(e.g '.y.ro')");

                if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
                {
                    printf("[proxy]Error sending add client name.\n");
                    exit(0);
                }

                if (-1 == recv(client, rasp, MAX_RESPONSE, 0))
                {
                    printf("[proxy]Error recv client name.\n");
                    exit(0);
                }

                if (strcmp(rasp, "null") == 0)
                {
                    break;
                }

                cJSON *clientname = cJSON_CreateString(rasp);
                cJSON_AddItemToArray(clienti, clientname);
            }

            cJSON_AddItemToObject(obj, "clientsrestricted", clienti);

            cJSON *zile = cJSON_CreateArray();
            strcpy(rasp, "Please insert a string contain digits between 1-7(1:monday,..,7:sunday). ");
            strcat(rasp, "Other digits will be ignored.");

            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Error sending add days.\n");
                exit(0);
            }

            if (-1 == recv(client, rasp, MAX_RESPONSE, 0))
            {
                printf("[proxy]Error recv days.\n");
                exit(0);
            }

            int zile_frecv[8];
            for(int i=1;i<=7;++i)
            {
                zile_frecv[i]=0;
            }
            for(int i=0;i<strlen(rasp);++i)
            {
                if(rasp[i]>='1'&&rasp[i]<='7')
                {
                    zile_frecv[rasp[i]-'0']=1;
                }
            }

            if(zile_frecv[1]==1)
            {
                cJSON *adaug=cJSON_CreateString("monday");
                cJSON_AddItemToArray(zile,adaug);
            }
            if(zile_frecv[2]==1)
            {
                cJSON *adaug=cJSON_CreateString("tuesday");
                cJSON_AddItemToArray(zile,adaug);
            }
            if(zile_frecv[3]==1)
            {
                cJSON *adaug=cJSON_CreateString("wednesday");
                cJSON_AddItemToArray(zile,adaug);
            }
            if(zile_frecv[4]==1)
            {
                cJSON *adaug=cJSON_CreateString("thursday");
                cJSON_AddItemToArray(zile,adaug);
            }
            if(zile_frecv[5]==1)
            {
                cJSON *adaug=cJSON_CreateString("friday");
                cJSON_AddItemToArray(zile,adaug);
            }
            if(zile_frecv[6]==1)
            {
                cJSON *adaug=cJSON_CreateString("saturday");
                cJSON_AddItemToArray(zile,adaug);
            }
            if(zile_frecv[7]==1)
            {
                cJSON *adaug=cJSON_CreateString("sunday");
                cJSON_AddItemToArray(zile,adaug);
            }
            cJSON_AddItemToObject(obj, "daysforbidden", zile);

            cJSON *ore=cJSON_CreateArray();
            while (1)
            {
                strcpy(rasp, "Please insert one at a time hours.");
                strcat(rasp, " If you want to stop insert 'null'.");
                strcat(rasp, " Insert hours between 0 and 24.(e.g '17')");

                if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
                {
                    printf("[proxy]Error sending add hours.\n");
                    exit(0);
                }

                if (-1 == recv(client, rasp, MAX_RESPONSE, 0))
                {
                    printf("[proxy]Error recv hours.\n");
                    exit(0);
                }

                if (strcmp(rasp, "null") == 0)
                {
                    break;
                }

                cJSON *ora = cJSON_CreateNumber(atoi(rasp));
                cJSON_AddItemToArray(ore, ora);
            }
            cJSON_AddItemToObject(obj,"hoursforbidden",ore);
            cJSON_AddItemToArray(serverdomains,obj);
        }
    }
    else if (rasp[0] == 'n')
    {
        // continue
    }

    char* sir_final=cJSON_Print(restrictii);

    close(fd);

    if(-1 == (fd=open(json_path,O_TRUNC | O_WRONLY)))
    {
        printf("[proxy]Eroare truncare json.\n");
        exit(0);
    }

    if(-1 == write(fd,sir_final,strlen(sir_final)))
    {
        printf("[proxy]Eroare rescreiere in json.\n");
        exit(0);
    }

    close(fd);

    return 1;
}

int restrict_file_modify(int client)
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

    if(-1 == send(client,file,strlen(file)+1,0))
    {
        printf("[proxy]Error sending json file.\n");
        exit(0);
    }

    cJSON *maxsize = NULL;
    cJSON *filesallowed = NULL;
    cJSON *serverdomains = NULL;

    cJSON *restrictii = cJSON_Parse(file);
    if(restrictii == NULL)
    {
        return -1;
    }

    maxsize = cJSON_GetObjectItemCaseSensitive(restrictii,"maxsize");

    filesallowed=cJSON_GetObjectItemCaseSensitive(restrictii,"filesallowed");

    char rasp[MAX_RESPONSE];

    if(-1 == recv(client,rasp,MAX_RESPONSE,0))
    {
        printf("[proxy]Error recv y or n.\n");
        exit(0);
    }

    if(rasp[0]=='y')
    {
        strcpy(rasp,rasp+1);
        strcat(rasp,"kB");

        strcpy(maxsize->valuestring,rasp);
    }

    strcpy(rasp,"Do you want to delete allowed for download file types?[y\\n]");
    if(-1 == send(client,rasp,strlen(rasp)+1,0))
    {
        printf("[proxy]Error send 2nd question.\n");
        exit(0);
    }

    if(-1 == recv(client,rasp,MAX_RESPONSE,0))
    {
        printf("[proxy]Error recv y or n.\n");
        exit(0);
    }

    if(rasp[0] == 'y')
    {
        int contor=0;
        char to_delete[300]={0};
        cJSON *fileallowed;
        cJSON_ArrayForEach(fileallowed,filesallowed)
        {
            strcpy(rasp,"Do you want to delete the file type: '");
            strcat(rasp,fileallowed->valuestring);
            strcat(rasp,"'?[y\\n]");

            if(-1 == send(client,rasp,strlen(rasp)+1,0))
            {
                printf("[proxy]Error sending delete file type.\n");
                exit(0);
            }

            if(-1 == recv(client,rasp,MAX_RESPONSE,0))
            {
                printf("[proxy]Error in recv delete file type.\n");
                exit(0);
            }
            if(rasp[0]=='y')
            {
                to_delete[contor]=1;
            }
            contor++;
        }
        strcpy(rasp,"null");
        if(-1 == send(client,rasp,strlen(rasp)+1,0))
        {
            printf("[proxy]Error send ending file type while.\n");
            exit(0);
        }
        int offset=0;
        for(int i=0;i<contor;++i)
        {
            if(to_delete[i]==1)
            {
                cJSON_DeleteItemFromArray(filesallowed,i-offset);
                offset++;
            } 
        }
        if(-1 == recv(client,rasp,MAX_RESPONSE,0))
        {
            printf("[proxy]Error recv server stop.\n");
            exit(0);
        }

    }

    strcpy(rasp,"Do you want to delete restrictions to servers?[y\\n]");
    if(-1 == send(client,rasp,strlen(rasp)+1,0))
    {
        printf("[proxy]Eroare send question.\n");
        exit(0);
    }

    if(-1 == recv(client,rasp,MAX_RESPONSE,0))
    {
        printf("[proxy]Eroare recv question resp.\n");
        exit(0);
    }
    
    serverdomains=cJSON_GetObjectItemCaseSensitive(restrictii,"serverdomains");

    if(rasp[0]=='y')
    {
        int contor=0;
        char to_delete[300]={0};
        cJSON *serverdomain;
        cJSON_ArrayForEach(serverdomain,serverdomains)
        {
            cJSON *name=cJSON_GetObjectItemCaseSensitive(serverdomain,"name");
            strcpy(rasp,"Do you want to delete the server restriction: '");
            strcat(rasp,name->valuestring);
            strcat(rasp,"'?[y\\n]");

            if(-1 == send(client,rasp,strlen(rasp)+1,0))
            {
                printf("[proxy]Error sending delete server restriction.\n");
                exit(0);
            }

            if(-1 == recv(client,rasp,MAX_RESPONSE,0))
            {
                printf("[proxy]Error in recv delete server restriction.\n");
                exit(0);
            }
            if(rasp[0]=='y')
            {
                to_delete[contor]=1;
            }
            contor++;       
        }
        strcpy(rasp,"null");
        if(-1 == send(client,rasp,strlen(rasp)+1,0))
        {
            printf("[proxy]Error send ending server restrict while.\n");
            exit(0);
        }
        int offset=0;
        for(int i=0;i<contor;++i)
        {
            if(to_delete[i]==1)
            {
                cJSON_DeleteItemFromArray(serverdomains,i-offset);
                offset++;
            }
        }
        if(-1 == recv(client,rasp,MAX_RESPONSE,0))
        {
            printf("[proxy]Error recv done.\n");
            exit(0);
        }
    }

    char* sir_final=cJSON_Print(restrictii);

    if(-1 == (fd=open(json_path,O_TRUNC | O_WRONLY)))
    {
        printf("[proxy]Eroare truncare json.\n");
        exit(0);
    }

    if(-1 == write(fd,sir_final,strlen(sir_final)))
    {
        printf("[proxy]Eroare rescreiere in json.\n");
        exit(0);
    }

    close(fd);

    return 1;
}

int solve_client(int client)
{
    char rasp[MAX_RESPONSE];
    char command[MAX_COMMAND];

    while (1)
    {
        explicit_bzero(command, MAX_COMMAND);

        // citire comanda
        if (recv(client, command, MAX_COMMAND, 0) <= 0)
        {
            perror("[proxy]Eroare la read() comanda de la client.\n");
            exit(0);
        }

        printf("[proxy]Comanda a fost receptionata...%s\n", command);

        if (strncmp(command, "login: ", 7) == 0)
        {
            // parola nu poate fi "NULL"
            // raspunsul: daca user nu exista =>>
            //            daca parola nu e buna =>>
            //            conectat
            //            + return 2 cifre (prima pentru user obisnuit, a doua pt admin)
            char utilizator[MAX_USERPASS] = "";
            char parola[MAX_USERPASS] = "";

            char *aux = strtok(command, " ");
            while (aux != NULL)
            {
                if (strcmp(aux, "login:") == 0)
                {
                    aux = strtok(NULL, " ");
                }
                if (strlen(aux) > 0)
                {
                    if (strlen(utilizator) == 0)
                    {
                        strcpy(utilizator, aux);
                    }
                    else if (strlen(parola) == 0)
                    {
                        strcpy(parola, aux);
                    }
                    else
                    {
                        break;
                    }
                }
                aux = strtok(NULL, " ");
            }

            sqlite3 *db;
            char *zErrMsg = 0;
            int rc;
            char sql[100];
            char data[MAX_USERPASS];

            rc = sqlite3_open(database_path, &db);

            if (rc)
            {
                printf("[proxy]Can't open database: %s\n", sqlite3_errmsg(db));
                exit(0);
            }
            else
            {
                // database opened
            }

            // Create SQL statement
            sprintf(sql, "SELECT password from USERS WHERE username='%s';", utilizator);
            // printf("\n%s\n", sql);

            // Execute SQL statement
            rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

            if (rc != SQLITE_OK)
            {
                printf("[proxy]SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                exit(0);
            }
            else
            {
                // successful
            }

            if (strcmp(data, "NULL") == 0)
            {
                // raspuns:"00user"
                strcpy(rasp, "00user");
            }
            else
            {
                if (strcmp(data, parola) == 0)
                {
                    sprintf(sql, "SELECT admin from USERS WHERE username='%s';", utilizator);

                    rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

                    if (rc != SQLITE_OK)
                    {
                        printf("[proxy]SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                        exit(0);
                    }
                    else
                    {
                        // successful
                    }

                    if (strcmp(data, "0") == 0)
                    {
                        // raspuns:"10connected"
                        strcpy(rasp, "10connected");
                    }
                    else
                    {
                        // raspuns:"11connected"
                        strcpy(rasp, "11connected");
                    }

                    sprintf(sql, "UPDATE USERS SET logged=1 WHERE username='%s';", utilizator);

                    rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

                    if (rc != SQLITE_OK)
                    {
                        printf("[proxy]SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                        exit(0);
                    }
                    else
                    {
                        // successful
                    }
                }
                else
                {
                    // raspuns:"00password"
                    strcpy(rasp, "00password");
                }
            }

            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Error sending login response from proxy.\n");
                exit(0);
            }

            sqlite3_close(db);
        }
        else if (strncmp(command, "create: ", 8) == 0)
        {
            // daca userul exista : raspuns exista =>>
            // created successfully
            char utilizator[MAX_USERPASS] = "";
            char parola[MAX_USERPASS] = "";

            char *aux = strtok(command, " ");
            while (aux != NULL)
            {
                if (strcmp(aux, "create:") == 0)
                {
                    aux = strtok(NULL, " ");
                }
                if (strlen(aux) > 0)
                {
                    if (strlen(utilizator) == 0)
                    {
                        strcpy(utilizator, aux);
                    }
                    else if (strlen(parola) == 0)
                    {
                        strcpy(parola, aux);
                    }
                    else
                    {
                        break;
                    }
                }
                aux = strtok(NULL, " ");
            }

            sqlite3 *db;
            char *zErrMsg = 0;
            int rc;
            char sql[150];
            char data[MAX_USERPASS];

            rc = sqlite3_open(database_path, &db);

            if (rc)
            {
                printf("[proxy]Can't open database: %s\n", sqlite3_errmsg(db));
                exit(0);
            }
            else
            {
                // database opened
            }

            sprintf(sql, "SELECT password from USERS WHERE username='%s';", utilizator);

            rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

            if (rc != SQLITE_OK)
            {
                printf("[proxy]SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                exit(0);
            }
            else
            {
                // successful
            }

            if (strcmp(data, "NULL"))
            {
                sprintf(sql, "INSERT INTO USERS VALUES((SELECT MAX(id) FROM USERS)+1,'%s','%s',0,0);", utilizator, parola);

                rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

                if (rc != SQLITE_OK)
                {
                    printf("[proxy]SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                    exit(0);
                }
                else
                {
                    // successful
                }

                strcpy(rasp, "User successfully created, please login with the new credentials.");
            }
            else
            {
                strcpy(rasp, "User already exists.");
            }

            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Eroare trimitere raspuns create from proxy.\n");
                exit(0);
            }
        }
        else if (strncmp(command, "logout", 6) == 0)
        {
            char utilizator[MAX_USERPASS];

            strcpy(utilizator, command + 8);

            sqlite3 *db;
            char *zErrMsg = 0;
            int rc;
            char sql[100];
            char data[MAX_USERPASS];

            rc = sqlite3_open(database_path, &db);

            if (rc)
            {
                printf("[proxy]Can't open database: %s\n", sqlite3_errmsg(db));
                exit(0);
            }
            else
            {
                // database opened
            }

            sprintf(sql, "UPDATE USERS SET logged=0 WHERE username='%s';", utilizator);

            rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

            if (rc != SQLITE_OK)
            {
                printf("[proxy]SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                exit(0);
            }
            else
            {
                // successful
            }

            strcpy(rasp, "Success. User logged out.");
            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Eroare trimitere raspuns create from proxy.\n");
                exit(0);
            }
        }
        else if (strncmp(command, "server: ", 8) == 0) //aici incepe partea ftp
        {
            strcpy(command,command+8);
            ftp_mode(command,client);
        }
        else if (strncmp(command, "create-admin: ", 14) == 0)
        {
            // daca userul exista : raspuns exista =>>
            // created successfully
            char utilizator[MAX_USERPASS] = "";
            char parola[MAX_USERPASS] = "";

            char *aux = strtok(command, " ");
            while (aux != NULL)
            {
                if (strcmp(aux, "create-admin:") == 0)
                {
                    aux = strtok(NULL, " ");
                }
                if (strlen(aux) > 0)
                {
                    if (strlen(utilizator) == 0)
                    {
                        strcpy(utilizator, aux);
                    }
                    else if (strlen(parola) == 0)
                    {
                        strcpy(parola, aux);
                    }
                    else
                    {
                        break;
                    }
                }
                aux = strtok(NULL, " ");
            }

            sqlite3 *db;
            char *zErrMsg = 0;
            int rc;
            char sql[150];
            char data[MAX_USERPASS];

            rc = sqlite3_open(database_path, &db);

            if (rc)
            {
                printf("[proxy]Can't open database: %s\n", sqlite3_errmsg(db));
                exit(0);
            }
            else
            {
                // database opened
            }

            sprintf(sql, "SELECT password from USERS WHERE username='%s';", utilizator);

            rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

            if (rc != SQLITE_OK)
            {
                printf("[proxy]SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                exit(0);
            }
            else
            {
                // successful
            }

            if (strcmp(data, "NULL"))
            {
                sprintf(sql, "INSERT INTO USERS VALUES((SELECT MAX(id) FROM USERS)+1,'%s','%s',1,0);", utilizator, parola);

                rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

                if (rc != SQLITE_OK)
                {
                    printf("[proxy]SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                    exit(0);
                }
                else
                {
                    // successful
                }

                strcpy(rasp, "User successfully created, please login with the new credentials.");
            }
            else
            {
                strcpy(rasp, "User already exists.");
            }

            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Eroare trimitere raspuns create from proxy.\n");
                exit(0);
            }
        }
        else if (strncmp(command, "forbidden", 9) == 0)
        {
            strcpy(rasp, "Follow the instructions in order to modify the restriction file.\n");
            strcat(rasp, "Do you want to add('a') or modify('m') the content? Type the letter and press enter.");
            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Eroare trimite intrebare 1 from proxy.\n");
                exit(0);
            }

            if (-1 == recv(client, command, MAX_RESPONSE, 0))
            {
                printf("[proxy]Eroare primire forbidden litera.\n");
                exit(0);
            }

            if (command[0] == 'a')
            {
                restrict_file_add(client);
            }
            else if (command[0] == 'm')
            {
                restrict_file_modify(client);
            }

            strcpy(rasp,"Altering the configuration file completed.");
            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Eroare trimite raspuns forbidden from proxy.\n");
                exit(0);
            }

        }
        else if (strncmp(command, "exit-proxy", 10) == 0)
        {
            sqlite3 *db;
            char *zErrMsg = 0;
            int rc;
            char sql[150];
            char data[MAX_USERPASS];

            rc = sqlite3_open(database_path, &db);

            if (rc)
            {
                printf("[proxy]Can't open database: %s\n", sqlite3_errmsg(db));
                exit(0);
            }
            else
            {
                // database opened
            }

            sprintf(sql, "UPDATE USERS SET logged=0;");

            rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);

            if (rc != SQLITE_OK)
            {
                printf("[proxy]SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                exit(0);
            }
            else
            {
                // successful
            }

            strcpy(rasp, "The server and all clients will close. Goodbye.");
            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Error closing the client.\n");
                exit(0);
            }
            sleep(3);
            close(client);
            kill(getppid(), SIGUSR1);
            exit(0);
        }
        else if (strncmp(command, "exit", 4) == 0)
        {
            strcpy(rasp, "Client will close. Goodbye.");
            if (-1 == send(client, rasp, strlen(rasp) + 1, 0))
            {
                printf("[proxy]Error closing the client.\n");
                exit(0);
            }
            close(client);
            return client;
        }
    }
}

int serve_clients(int sd)
{
    struct sockaddr_in from;
    bzero(&from, sizeof(from));

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, 1) == -1)
    {
        perror("[proxy]Eroare la listen().\n");
        exit(0);
    }

    /* servim in mod concurent clientii... */
    while (1)
    {
        // printf("Sunt %d clienti activi.\n",clients_count);

        int client;
        int length = sizeof(from);

        printf("[proxy]Asteptam la portul %d...\n", PORT);
        fflush(stdout);

        /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
        client = accept(sd, (struct sockaddr *)&from, &length);

        /* eroare la acceptarea conexiunii de la un client */
        if (client < 0)
        {
            perror("[proxy]Eroare la accept().\n");
            continue;
        }

        char msg[150];
        if (clients_count >= MAX_CLIENTS)
        {
            strcpy(msg, "Full server right now. Try again later.");

            if (-1 == send(client, msg, strlen(msg) + 1, 0))
            {
                printf("[proxy]Error sending welcome connected to client.\n");
                exit(0);
            }

            close(client);
            continue;
        }

        int pid;
        if ((pid = fork()) == -1)
        {
            close(client);
            continue;
        }
        else if (pid > 0)
        {
            // parinte

            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients_pid[i] == -1) // slot liber
                {
                    strcpy(msg, "Connected to server.");
                    if (-1 == send(client, msg, strlen(msg) + 1, 0))
                    {
                        printf("[proxy]Error sending welcome connected to client.\n");
                        exit(0);
                    }
                    // printf("[proxy]Connected to server.\n");
                    clients_count++;
                    clients_pid[i] = pid;
                    break;
                }
            }

            close(client);
            int pid_to_close;
            while (pid_to_close = waitpid(-1, NULL, WNOHANG))
            {
                for (int i = 0; i < MAX_CLIENTS; ++i)
                {
                    if (clients_pid[i] == pid_to_close)
                    {
                        clients_pid[i] = -1;
                        clients_count--;
                        break;
                    }
                }
            }
            continue;
        }
        else if (pid == 0)
        {
            // copil
            close(sd);

            // client requests:
            solve_client(client);

            // am terminat cu acest client, inchidem conexiunea
            // close(client);
            exit(0);
        }

    }
}

int main()
{
    int sd;

    if (init_connect(&sd) == -1)
    {
        printf("[proxy]Error initiating server socket.\n");
        exit(0);
    }

    if (serve_clients(sd) == -1)
    {
        printf("[proxy]Error while serving clients.\n");
        exit(0);
    }

    return 0;
}