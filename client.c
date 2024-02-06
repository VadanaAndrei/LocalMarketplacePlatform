#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include "encrypt.h"

#define buff_length 256
#define men_size 2048

extern int errno;

int port;
int login_status = 0;

int main(int argc, char* argv[])
{
    int sock_desc;
    struct sockaddr_in server;
    char msg_send[buff_length];
    int msg_length, msg_length_nou, corect;

    if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

    port = atoi(argv[2]);

    if ((sock_desc = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

    server.sin_family = AF_INET;
    if (inet_pton(AF_INET, argv[1], &server.sin_addr) <= 0) 
    {
        perror("Eroare la inet_pton().\n");
        return errno;
    }

    server.sin_port = htons(port);
    if (connect (sock_desc, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("Eroare la connect().\n");
      return errno;
    }


    while(1)
    {   
        if(login_status == 0)
            printf("Tastati 'inregistrare' pentru a va inregistra, 'logare' pentru a va loga sau 'help' pentru o lista de comenzi.\n");

        memset(msg_send, 0, buff_length);
        fgets(msg_send, buff_length, stdin);
        printf("\n");
        msg_send[strlen(msg_send)-1] = '\0';
        msg_length = strlen(msg_send);
        
        int cod_send = 0;

        if( strcmp(msg_send,"quit") != 0 && strcmp(msg_send,"vizualizare produse") != 0  && 
            strcmp(msg_send,"inregistrare") != 0 && strcmp(msg_send,"logare") != 0 && 
            strcmp(msg_send,"delogare") != 0 && strcmp(msg_send,"help") != 0 && strcmp(msg_send,"cumpara") != 0 &&
            strcmp(msg_send,"vinde") != 0 && strcmp(msg_send,"interogare sold") != 0 && 
            strcmp(msg_send,"cos de cumparaturi") != 0 && strcmp(msg_send,"plateste") != 0)
        {
            printf("Trebuie sa introduceti o comanda!\n");
        }

        else if(strcmp(msg_send, "help") == 0)
        {   
            cod_send = 1;
            cod_send = htonl(cod_send);
            
            if (write (sock_desc, &cod_send, sizeof(int)) <= 0)
            {
                perror("Eroare la trimiterea codului spre server!\n");
                return errno;
            }

            if (read (sock_desc,&corect,sizeof(int)) <= 0)
            {
                perror("Eroare la read-ul de logare!\n");
                return errno;
            }
            corect = ntohl(corect);

            if(corect == 1)
            {
                printf("-help: afiseaza o lista de comenzi\n");
                printf("-logare: un utilizator se autentifica cu username si parola\n");
                printf("-inregistrare: se creeaza un cont nou de utilizator\n");
                printf("-delogare: utilizatorul isi paraseste contul\n");
                printf("-quit: se inchide clientul\n");
                printf("-vizualizare produse: se afiseaza o lista cu produsele, pretul si cantitatea disponibila\n");
                printf("                      se recomanda vizualizarea produselor inaintea folosirii operatiilor de vanzare/cumparare\n");
                printf("-cumpara: se introduc produsul si cantitatea care vor fi adaugate in cosul de cumparaturi\n");
                printf("-vinde: se introduc produsul si cantitatea date spre vanzare\n");
                printf("-interogare sold: se afiseaza balanta actuala a contului\n");
                printf("-cos de cumparaturi: se afiseaza produsele, cantitatea si pretul total al cosului de cumparaturi\n");
                printf("                     se recomanda vizualizarea cosului de cumparaturi inaintea folosirii comenzii 'plateste'\n");
                printf("-plateste: se achita produsele din cosul de cumparaturi\n");
            }
        }

        else if(strcmp(msg_send, "logare") == 0)
        {
            if(login_status == 1)

                printf("Sunteti deja logat!\n");
            
            else
            {
                cod_send = 2;
                cod_send = htonl(cod_send);
                
                if (write (sock_desc, &cod_send, sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                char user[buff_length] = "";
                char par[buff_length] = "";

                printf("Username: ");
                fflush(stdout);

                fgets(user, buff_length, stdin);
                user[strlen(user)-1] = '\0';
                msg_length = strlen(user);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de logare!\n");
                    return errno;
                }

                if (write (sock_desc, user, msg_length) < 0)
                {
                    perror("Eroare la write-ul de logare!\n");
                    return errno;
                }

                printf("Parola: ");
                fflush(stdout);

                fgets(par, buff_length, stdin);
                par[strlen(par)-1] = '\0';
                msg_length = strlen(par);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de logare!\n");
                    return errno;
                }

                if (write (sock_desc, par, msg_length) < 0)
                {
                    perror("Eroare la write-ul de logare!\n");
                    return errno;
                }

                if((strlen(user) == 0) || (strlen(par) == 0))
                {
                    printf("Username-ul si parola nu pot fi vide!\n");
                    continue;
                }

                char raspuns[buff_length] = "";
                int raspuns_size;

                if (read (sock_desc,&raspuns_size,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de logare!\n");
                    return errno;
                }
                raspuns_size = ntohl(raspuns_size);

                if (read (sock_desc,raspuns,raspuns_size) <= 0)
                {
                    perror("Eroare la read-ul de logare!\n");
                    return errno;
                }

                if(strcmp(raspuns, "Logare reusita!")  == 0)
                    login_status = 1;

                printf("%s\n", raspuns);

            }
        }

        else if(strcmp(msg_send,"inregistrare") == 0) 
        {

            if(login_status == 1)
                printf("Pentru a inregistra un utilizator nou trebuie sa va delogati!\n");       
            else
            {
                cod_send = 3;
                cod_send = htonl(cod_send);
                
                if (write (sock_desc, &cod_send, sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                printf("Username: ");
                fflush(stdout);

                char user[buff_length] = "";
                char par[buff_length] = "";

                fgets(user, buff_length, stdin);
                user[strlen(user)-1] = '\0';
                msg_length = strlen(user);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de inregistrare!\n");
                    return errno;
                }

                if (write (sock_desc, user, msg_length) < 0)
                {
                    perror("Eroare la write-ul de inregistrare!\n");
                    return errno;
                }

                printf("Parola: ");
                fflush(stdout);

                fgets(par, buff_length, stdin);
                par[strlen(par)-1] = '\0';
                msg_length = strlen(par);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de inregistrare!\n");
                    return errno;
                }

                if (write (sock_desc, par, msg_length) < 0)
                {
                    perror("Eroare la write-ul de inregistrare!\n");
                    return errno;
                }

                if((strlen(user) == 0) || (strlen(par) == 0))
                {
                    printf("Username-ul si parola nu pot fi vide!\n");
                    continue;
                }

                char raspuns[buff_length] = "";
                int raspuns_size;

                if (read (sock_desc,&raspuns_size,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de inregistrare!\n");
                    return errno;
                }
                raspuns_size = ntohl(raspuns_size);

                if (read (sock_desc,raspuns,raspuns_size) <= 0)
                {
                    perror("Eroare la read-ul de inregistrare!\n");
                    return errno;
                }

                printf("%s\n", raspuns);
            }
        }

        else if(strcmp(msg_send, "delogare") == 0)
        {
            cod_send = 4;
            cod_send = htonl(cod_send);

            if (write (sock_desc,&cod_send, sizeof(int)) <= 0)
            {
                perror("Eroare la trimiterea codului spre server!\n");
                return errno;
            }

            if (read (sock_desc,&corect,sizeof(int)) <= 0)
            {
                perror("Eroare la read-ul de 'cumpara'!\n");
                return errno;
            }
            corect = ntohl(corect);

            if(corect == 1)
            {
                if(login_status == 0)
                    printf("Nu sunteti logat!\n");
                else
                {
                    login_status = 0;
                    printf("Delogare reusita!\n");
                }
            }
        }

        else if(strcmp(msg_send,"quit") == 0) 
        {
            cod_send = 5;
            cod_send = htonl(cod_send);
            if (write (sock_desc,&cod_send, sizeof(int)) <= 0)
            {
                perror("Eroare la trimiterea codului spre server!\n");
                return errno;
            }

            if (read (sock_desc,&corect,sizeof(int)) <= 0)
            {
                perror("Eroare la read-ul de 'cumpara'!\n");
                return errno;
            }
            corect = ntohl(corect);

            if(corect ==1)
                break;
        }

        else if(strcmp(msg_send, "vizualizare produse") == 0)
        {
            if(login_status == 0)
                printf("Pentru a vedea produsele trebuie sa va logati mai intai!\n");
            else
            {
                char men_cumpara[men_size];
                memset(men_cumpara, 0, men_size);
                cod_send = 6;
                cod_send = htonl(cod_send);
                int men_length;

                if (write (sock_desc,&cod_send,sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                if (read (sock_desc,&men_length,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de 'vizualizare produse'!\n");
                    return errno;
                }
                men_length = ntohl(men_length);

                if (read (sock_desc,&men_cumpara,men_length) <= 0)
                {
                    perror("Eroare la read-ul de 'vizualizare produse'!\n");
                    return errno;
                }

                printf("%s\n", men_cumpara);
            }
        }

        else if(strcmp(msg_send, "cumpara") == 0)
        {
            if(login_status == 0)
            {
                printf("Trebuie sa fiti logat pentru a cumpara produse!\n");
            }
            else
            {
                cod_send = 7;
                cod_send = htonl(cod_send);

                if (write (sock_desc,&cod_send,sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                char produs[buff_length]  = "";
                char cantitate[buff_length] = "";

                printf("Produs: ");
                fflush(stdout);

                fgets(produs, buff_length, stdin);
                produs[strlen(produs)-1] = '\0';
                msg_length = strlen(produs);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de 'cumpara'!\n");
                    return errno;
                }

                if (write (sock_desc, produs, msg_length) < 0)
                {
                    perror("Eroare la write-ul de 'cumpara'!\n");
                    return errno;
                }

                printf("Cantitate: ");
                fflush(stdout);

                fgets(cantitate, buff_length, stdin);
                cantitate[strlen(cantitate)-1] = '\0';
                msg_length = strlen(cantitate);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de 'cumpara'!\n");
                    return errno;
                }

                if (write (sock_desc, cantitate, msg_length) < 0)
                {
                    perror("Eroare la write-ul de 'cumpara'!\n");
                    return errno;
                }

                char raspuns[buff_length] = "";
                int raspuns_size;

                if (read (sock_desc,&raspuns_size,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de 'cumpara'!\n");
                    return errno;
                }
                raspuns_size = ntohl(raspuns_size);

                if (read (sock_desc,raspuns,raspuns_size) <= 0)
                {
                    perror("Eroare la read-ul de 'cumpara'!\n");
                    return errno;
                }

                printf(".\n");
                usleep(750000);
                printf(".\n");
                usleep(750000);
                printf(".\n");
                usleep(750000);
                printf("\n");
                printf("%s\n", raspuns);
            }
        } 

        else if(strcmp(msg_send, "vinde") == 0)
        {
            if(login_status == 0)
            {
                printf("Trebuie sa fiti logat pentru a vinde produse!\n");
            }
            else
            {
                cod_send = 8;
                cod_send = htonl(cod_send);

                if (write (sock_desc,&cod_send,sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                char produs[buff_length]  = "";
                char cantitate[buff_length] = "";

                printf("Produs: ");
                fflush(stdout);

                fgets(produs, buff_length, stdin);
                produs[strlen(produs)-1] = '\0';
                msg_length = strlen(produs);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de 'vinde'!\n");
                    return errno;
                }

                if (write (sock_desc, produs, msg_length) < 0)
                {
                    perror("Eroare la write-ul de 'vinde'!\n");
                    return errno;
                }

                printf("Cantitate: ");
                fflush(stdout);

                fgets(cantitate, buff_length, stdin);
                cantitate[strlen(cantitate)-1] = '\0';
                msg_length = strlen(cantitate);
                msg_length_nou = htonl(msg_length);

                if (write (sock_desc, &msg_length_nou, sizeof(int)) < 0)
                {
                    perror("Eroare la write-ul de 'vinde'!\n");
                    return errno;
                }

                if (write (sock_desc, cantitate, msg_length) < 0)
                {
                    perror("Eroare la write-ul de 'vinde'!\n");
                    return errno;
                }

                char raspuns[buff_length] = "";
                int raspuns_size;

                if (read (sock_desc,&raspuns_size,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de 'vinde'!\n");
                    return errno;
                }
                raspuns_size = ntohl(raspuns_size);

                if (read (sock_desc,raspuns,raspuns_size) <= 0)
                {
                    perror("Eroare la read-ul de 'vinde'!\n");
                    return errno;
                }
                printf(".\n");
                usleep(750000);
                printf(".\n");
                usleep(750000);
                printf(".\n");
                usleep(750000);
                printf("\n");
                printf("%s\n", raspuns);
            }

        }

        else if(strcmp(msg_send, "interogare sold") == 0)
        {
            if(login_status == 0)
                printf("Trebuie sa fiti logat pentru a va interoga soldul!\n");

            else
            {
                cod_send = 9;
                cod_send = htonl(cod_send);
                
                if (write (sock_desc,&cod_send,sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                int valoare_sold_int;

                if (read (sock_desc,&valoare_sold_int,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de 'vizualizare produse'!\n");
                    return errno;
                }
                valoare_sold_int = ntohl(valoare_sold_int);

                printf("Balanta actuala a contului este: %d RON\n", valoare_sold_int);
            }
        }

        else if(strcmp(msg_send, "cos de cumparaturi") == 0)
        {   
            if(login_status == 0)
                printf("Pentru a accesa cosul de cumparaturi trebuie sa va logati mai intai!\n");
            else
            {   
                char men_cos[men_size];
                memset(men_cos, 0, men_size);
                cod_send = 10;
                cod_send = htonl(cod_send);
                int men_length;

                if (write (sock_desc,&cod_send,sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                if (read (sock_desc,&men_length,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de 'cos de cumparaturi'!\n");
                    return errno;
                }
                men_length = ntohl(men_length);

                if (read (sock_desc,&men_cos,men_length) <= 0)
                {
                    perror("Eroare la read-ul de 'cos de cumparaturi'!\n");
                    return errno;
                }

                printf("%s\n", men_cos);
            }
        }

        else if(strcmp(msg_send, "plateste") == 0)
        {
            if(login_status == 0)
                printf("Pentru a plati trebuie sa va logati mai intai!\n");
            else
            {
                cod_send = 11;
                cod_send = htonl(cod_send);
                
                if (write (sock_desc,&cod_send,sizeof(int)) <= 0)
                {
                    perror("Eroare la trimiterea codului spre server!\n");
                    return errno;
                }

                char raspuns[buff_length] = "";
                int raspuns_size;

                if (read (sock_desc,&raspuns_size,sizeof(int)) <= 0)
                {
                    perror("Eroare la read-ul de 'plateste'!\n");
                    return errno;
                }
                raspuns_size = ntohl(raspuns_size);

                if (read (sock_desc,raspuns,raspuns_size) <= 0)
                {
                    perror("Eroare la read-ul de 'plateste'!\n");
                    return errno;
                }

                printf(".\n");
                usleep(750000);
                printf(".\n");
                usleep(750000);
                printf(".\n");
                usleep(750000);
                printf("\n");
                printf("%s\n", raspuns);
            }
        }   

    }
    close(sock_desc);
}