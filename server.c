#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <ctype.h>
#include "encrypt.h"

#define PORT 4444
#define user_max_length 32
#define buff_length 256
#define men_size 2048

extern int errno;

typedef struct thData
{
    int idThread;
    int cl;
}thData;

static void *treat(void *);
void raspunde(void *);
int verif_inregistrare(sqlite3 *db, const char *username, const char *parola);
int inregistrare(sqlite3 *db, const char *username, const char *parola);
int logare(sqlite3 *db, const char *username, const char *parola);
int interogare_sold(sqlite3 *db, const char *username);
void vizualizare_produse(sqlite3 *db);
int verif_produs(sqlite3 *db, const char *produs);
int verif_cantitate(sqlite3 *db, const char *produs, const char *cantitate);
int verif_sold(sqlite3 *db, const char *username);
void vinde(sqlite3 *db, const char *username, const char *produs, const char *cantitate);
void cumpara(sqlite3 *db, const char *username, const char *produs, const char *cantitate);
void cos_de_cumparaturi(sqlite3 *db, const char *username);
void plateste(sqlite3 *db, const char *username);
void golire_cos();
sqlite3 *db;

int login_status = 0;
int valoare_cos_int;
char user_curent[user_max_length];
char men_cumpara[men_size];
char men_cos[men_size];
char valoare_cos[buff_length];

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    char msg_prim[buff_length];
    int sock_desc;
    pthread_t th[100];
    int nr_thread = 0;

    int rez = sqlite3_open("database.db", &db);

    if (rez != SQLITE_OK) 
    {
        perror("Eroare la deschiderea bazei de date.\n");
        return errno;
    }

    pthread_t thread_cos;
    if (pthread_create(&thread_cos, NULL, (void*)golire_cos, NULL) != 0) 
    {
        perror("Eroare la crearea firului de execuție.\n");
        return errno;
    }

    if((sock_desc = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Eroare la socket.\n");
        return errno;
    }
    
    int on = 1;
    setsockopt(sock_desc,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

    memset(&server, 0, sizeof(server));
    memset(&from, 0, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);

    if (bind (sock_desc, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("Eroare la bind.\n");
      return errno;
    }

    if (listen (sock_desc, 2) == -1)
    {
        perror("Eroare la listen\n");
        return errno;
    }

    printf("Se asteapta la portul %d\n", PORT);
    fflush(stdout);

    while(1)
    {
        int client;
        thData *td;
        int length = sizeof(from);

        if ( (client = accept (sock_desc, (struct sockaddr *) &from, &length)) < 0)
        {
            perror ("Eroare la accept.\n");
            continue;
        }

        td = (struct thData*)malloc(sizeof(struct thData));
        td->idThread = nr_thread;
        td->cl = client;

        pthread_create(&th[nr_thread++], NULL, &treat, td);
    }

}

static void *treat(void *arg)
{
    struct thData tdL;
    tdL = *((struct thData*)arg);
    pthread_detach(pthread_self());
    raspunde((struct thData*)arg);
    close (tdL.cl);
    free(arg);
	return(NULL);
}

void raspunde(void *arg)
{
    int msg_length = 0, cod_rec, corect;
    char msg_prim[buff_length] = "";
	struct thData tdL; 
	tdL= *((struct thData*)arg);

    while(1)
    {
        memset(msg_prim, 0, buff_length);
        if (read (tdL.cl, &cod_rec,sizeof(int)) <= 0)
        {
            printf("Eroare la read() la thread-ul %d.\n", tdL.idThread);
            break;	
        }
        cod_rec = ntohl(cod_rec);

        if(cod_rec == 1)
        {
            corect = 1;
            corect = htonl(corect);
            if (write (tdL.cl, &corect, sizeof(int)) <= 0)
                perror("Eroare la write-ul de logare!\n");
        }

        else if(cod_rec == 2)
        {
            char user[buff_length] = "";
            char par[buff_length] = "";

            if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de logare!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, user, msg_length) < 0)
                perror("Eroare la read-ul de logare!\n");

            if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de logare!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, par, msg_length) < 0)
                perror("Eroare la read-ul de logare!\n");

            if((strlen(user) == 0) || (strlen(par) == 0))
                continue;

            char *ok = "Logare reusita!";
            char *not_ok = "Logare esuata (numele de utilizator sau parola sunt incorecte)!";
            int ok_size, not_ok_size, ok_size_nou, not_ok_size_nou;
            ok_size = strlen(ok);
            not_ok_size = strlen(not_ok);
            ok_size_nou = htonl(ok_size);
            not_ok_size_nou = htonl(not_ok_size);

            unsigned long int criptare = sdbm_hash(par);
            sprintf(par, "%ld", criptare);

            if(logare(db, user, par))
            {
                if (write (tdL.cl, &ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de logare!\n");

                if (write (tdL.cl, ok, ok_size) <= 0)
                    perror("Eroare la write-ul de logare!\n");

                login_status = 1;
                strcpy(user_curent, user);
            }
            else 
            {
                if (write (tdL.cl, &not_ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de logare!\n");

                if (write (tdL.cl, not_ok, not_ok_size) <= 0)
                    perror("Eroare la write-ul de logare!\n");
            }
        }

        else if(cod_rec == 3)
        {
            char user[buff_length] = "";
            char par[buff_length] = "";

            if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de inregistrare!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, user, msg_length) < 0)
                perror("Eroare la read-ul de inregistrare!\n");

            if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de inregistrare!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, par, msg_length) < 0)
                perror("Eroare la read-ul de inregistrare!\n");

            if((strlen(user) == 0) || (strlen(par) == 0))
                continue;

            char *ok = "Inregistrare reusita!";
            char *not_ok = "Inregistrare esuata!";
            char *user_folosit = "Inregistrare esuata (username-ul este deja folosit)!";
            char *introdus_sir_vid = "Nu puteti introduce sirul vid!";
            int ok_size, not_ok_size, ok_size_nou, not_ok_size_nou, user_folosit_size, user_folosit_size_nou;
            ok_size = strlen(ok);
            not_ok_size = strlen(not_ok);
            user_folosit_size = strlen(user_folosit);
            ok_size_nou = htonl(ok_size);
            not_ok_size_nou = htonl(not_ok_size);
            user_folosit_size_nou = htonl(user_folosit_size);

            unsigned long int criptare = sdbm_hash(par);
            sprintf(par, "%ld", criptare);

            if(verif_inregistrare(db, user, par) == -1)
            {
                if (write (tdL.cl, &user_folosit_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de inregistrare!\n");

                if (write (tdL.cl, user_folosit, user_folosit_size) <= 0)
                    perror("Eroare la write-ul de inregistrare!\n");
            }

            else if(inregistrare(db, user, par))
            {   
                if (write (tdL.cl, &ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de inregistrare!\n");

                if (write (tdL.cl, ok, ok_size) <= 0)
                    perror("Eroare la write-ul de inregistrare!\n");
            }
            else
            {
                if (write (tdL.cl, &not_ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de inregistrare!\n");

                if (write (tdL.cl, not_ok, not_ok_size) <= 0)
                    perror("Eroare la write-ul de inregistrare!\n");
            }
        }

        else if(cod_rec == 4)
        {   
            login_status = 0;
            corect = 1;
            corect = htonl(corect);
            if (write (tdL.cl, &corect, sizeof(int)) <= 0)
                perror("Eroare la write-ul de logare!\n");
        }

        else if(cod_rec == 5)
        {   
            corect = 1;
            corect = htonl(corect);
            if (write (tdL.cl, &corect, sizeof(int)) <= 0)
                perror("Eroare la write-ul de logare!\n");
            break;
        }

        else if(cod_rec == 6)
        {
            memset(men_cumpara, 0, men_size);
            strcat(men_cumpara, "   Produs///////////////////////Pret(lei/kg)///////////////////Cantitate(kg)\n");
            strcat(men_cumpara, "\n");
            vizualizare_produse(db);
            int men_length = strlen(men_cumpara);
            int men_length_nou = htonl(men_length);

            if (write (tdL.cl, &men_length_nou,sizeof(int)) <= 0)
                perror("Eroare la write-ul de 'vizualizare produse'!\n");

            if (write (tdL.cl, men_cumpara,men_length) <= 0)
                perror("Eroare la write-ul de 'vizualizare produse'!\n");

        }

        else if(cod_rec == 7)
        {
            char produs_rec[buff_length] = "";
            char cantitate_rec[buff_length] = "";

            if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de 'cumpara'!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, produs_rec, msg_length) < 0)
                perror("Eroare la read-ul de 'cumpara'!\n");

            if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de 'cumpara'!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, cantitate_rec, msg_length) < 0)
                perror("Eroare la read-ul de 'cumpara'!\n");

            char *ok = "Produsele au fost adaugate in cosul de cumparaturi!";
            char *not_ok = "Produsul selectat nu se afla in lista de produse!";
            int ok_size, not_ok_size, ok_size_nou, not_ok_size_nou, verif_int = 1;
            ok_size = strlen(ok);
            not_ok_size = strlen(not_ok);
            ok_size_nou = htonl(ok_size);
            not_ok_size_nou = htonl(not_ok_size);

            for(int i = 0; i < strlen(cantitate_rec); i++)
            {
                if(isdigit(cantitate_rec[i]) == 0)
                {
                    not_ok = "Cantitatea introdusa trebuie sa fie un numar intreg!";
                    not_ok_size = strlen(not_ok);
                    not_ok_size_nou = htonl(not_ok_size);
                    verif_int = 0;
                    break;
                }
            }

            if(verif_produs(db,produs_rec) == 1)
            {
                if(verif_cantitate(db, produs_rec, cantitate_rec) == 0)
                {
                    not_ok = "Cantitatea introdusa nu este disponibila momentan!";
                    not_ok_size = strlen(not_ok);
                    not_ok_size_nou = htonl(not_ok_size);
                }
            }

            if(verif_cantitate(db, produs_rec, cantitate_rec) == 1 &&
               verif_produs(db, produs_rec) == 1 && verif_int == 1)
            {
                if (write (tdL.cl, &ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de 'cumpara'!\n");

                if (write (tdL.cl, ok, ok_size) <= 0)
                    perror("Eroare la write-ul de 'cumpara'!\n");

                cumpara(db, user_curent, produs_rec, cantitate_rec);
            }
            else
            {
                if (write (tdL.cl, &not_ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de 'cumpara'!\n");

                if (write (tdL.cl, not_ok, not_ok_size) <= 0)
                    perror("Eroare la write-ul de 'cumpara'!\n");
            }
        }

        else if(cod_rec == 8)
        {
            char produs_rec[buff_length] = "";
            char cantitate_rec[buff_length] = "";

             if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de 'vinde'!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, produs_rec, msg_length) < 0)
                perror("Eroare la read-ul de 'vinde'!\n");

            if (read (tdL.cl, &msg_length, sizeof(int)) < 0)
                perror("Eroare la read-ul de 'vinde'!\n");
            msg_length = ntohl(msg_length);

            if (read (tdL.cl, cantitate_rec, msg_length) < 0)
                perror("Eroare la read-ul de 'vinde'!\n");


            char *ok = "Tranzactie reusita! Balanta contului a fost actualizata!";
            char *not_ok = "Produsul selectat nu se afla in lista de produse!";
            int ok_size, not_ok_size, ok_size_nou, not_ok_size_nou, verif_int = 1, verif_100 = 1;
            ok_size = strlen(ok);
            not_ok_size = strlen(not_ok);
            ok_size_nou = htonl(ok_size);
            not_ok_size_nou = htonl(not_ok_size);

            for(int i = 0; i < strlen(cantitate_rec); i++)
            {
                if(isdigit(cantitate_rec[i]) == 0)
                {
                    not_ok = "Cantitatea introdusa trebuie sa fie un numar intreg!";
                    not_ok_size = strlen(not_ok);
                    not_ok_size_nou = htonl(not_ok_size);
                    verif_int = 0;
                    break;
                }
            }

            if(atoi(cantitate_rec) > 100)
            {
                not_ok = "Cantitatea introdusa nu poate depasi 100kg!";
                not_ok_size = strlen(not_ok);
                not_ok_size_nou = htonl(not_ok_size);
                verif_100 = 0;
            }

            if(verif_produs(db, produs_rec) == 1 && verif_int == 1 && verif_100 == 1)
            {
                if (write (tdL.cl, &ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de 'vinde'!\n");

                if (write (tdL.cl, ok, ok_size) <= 0)
                    perror("Eroare la write-ul de 'vinde'!\n");

                vinde(db, user_curent, produs_rec, cantitate_rec);
            }
            else 
            {
                if (write (tdL.cl, &not_ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de 'vinde'!\n");

                if (write (tdL.cl, not_ok, not_ok_size) <= 0)
                    perror("Eroare la write-ul de 'vinde'!\n");
            }

        }

        else if(cod_rec == 9)
        {
            int valoare_sold_int = interogare_sold(db, user_curent);
            int valoare_sold_int_nou = htonl(valoare_sold_int);

            if (write (tdL.cl, &valoare_sold_int_nou,sizeof(int)) <= 0)
                perror("Eroare la write-ul de 'vizualizare produse'!\n");
        }

        else if(cod_rec == 10)
        {
            memset(men_cos, 0, men_size);
            cos_de_cumparaturi(db, user_curent);
            int men_length = strlen(men_cos);
            int men_length_nou = htonl(men_length);

            if (write (tdL.cl, &men_length_nou,sizeof(int)) <= 0)
                perror("Eroare la write-ul de 'cos de cumparaturi'!\n");

            if (write (tdL.cl, men_cos,men_length) <= 0)
                perror("Eroare la write-ul de 'cos de cumparaturi'!\n");
        }

        else if(cod_rec == 11)
        {
            char *ok = "Tranzactie reusita! Balanta contului a fost actualizata!";
            char *not_ok = "Pretul depaseste soldul disponibil!";
            int ok_size, not_ok_size, ok_size_nou, not_ok_size_nou;
            ok_size = strlen(ok);
            not_ok_size = strlen(not_ok);
            ok_size_nou = htonl(ok_size);
            not_ok_size_nou = htonl(not_ok_size);

            if(verif_sold(db, user_curent) == 1)
            {
                if (write (tdL.cl, &ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de 'plateste'!\n");

                if (write (tdL.cl, ok, ok_size) <= 0)
                    perror("Eroare la write-ul de 'plateste'!\n");

                plateste(db,user_curent);
            }
            else
            {
                if (write (tdL.cl, &not_ok_size_nou, sizeof(int)) <= 0)
                    perror("Eroare la write-ul de 'plateste'!\n");

                if (write (tdL.cl, not_ok, not_ok_size) <= 0)
                    perror("Eroare la write-ul de 'plateste'!\n");
            }
        }

    }

}

int verif_inregistrare(sqlite3 *db, const char *username, const char *parola)
{
    sqlite3_stmt *verificare_stmt;
    const char *verificare_query = "select * from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, verificare_query, -1, &verificare_stmt, NULL) != SQLITE_OK)
    {
        perror("Eroare la pregatirea interogarii de verificare.\n");
        return errno;
    }

    sqlite3_bind_text(verificare_stmt, 1, username, -1, SQLITE_STATIC);

    int verificare_rez;
    while ((verificare_rez = sqlite3_step(verificare_stmt)) == SQLITE_ROW) 
    {
        sqlite3_finalize(verificare_stmt);
        return -1;
    }

    sqlite3_finalize(verificare_stmt);
    return 0;
}

int inregistrare(sqlite3 *db, const char *username, const char *parola)
{
    sqlite3_stmt *stmt;
    const char *query = "select max(id_utilizator) from utilizatori";
    int rez = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

    if(rez != SQLITE_OK)
    {
        perror("Eroare la pregatirea interogarii.\n");
        return errno;
    }

    char max_id[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(max_id, sqlite3_column_text(stmt,0));

    int max_id_int = atoi(max_id);
    max_id_int += 1;

    sprintf(max_id, "%d", max_id_int);

    query = "insert into utilizatori (id_utilizator, username, parola, sold) values (?, ?, ?, 0)";
    rez = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

    if(rez != SQLITE_OK)
    {
        perror("Eroare la pregatirea interogarii.\n");
        return errno;
    }

    sqlite3_bind_text(stmt, 1, max_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, parola, -1, SQLITE_STATIC);

    rez = sqlite3_step(stmt);

    if (rez != SQLITE_DONE) 
    {
        perror("Eroare la executarea interogării.\n");
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

int logare(sqlite3 *db, const char *username, const char *parola)
{
    sqlite3_stmt *stmt;
    const char *query = "select * from utilizatori where username = ? and parola = ?;";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
        return errno;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, parola, -1, SQLITE_STATIC);

    int rez;

    while ((rez = sqlite3_step(stmt)) == SQLITE_ROW) 
    {
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int interogare_sold(sqlite3 *db, const char *username)
{
    sqlite3_stmt *stmt;
    const char *query = "select sold from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char valoare_sold[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(valoare_sold, sqlite3_column_text(stmt,0));

    int valoare_sold_int = atoi(valoare_sold);

    sqlite3_finalize(stmt);
    return valoare_sold_int;
}

void vizualizare_produse(sqlite3 *db)

{
    sqlite3_stmt *stmt;
    const char *query = "select max(id) from market";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    char max_id[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(max_id, sqlite3_column_text(stmt,0));

    int max_id_int = atoi(max_id);

    for(int i = 1; i <= max_id_int; i++)
    {
        query = "select produs from market where id = ?";
        char i_char[buff_length];
        sprintf(i_char, "%d", i);
        strcat(men_cumpara, i_char);
        if(strlen(i_char) == 1)
            strcat(men_cumpara, "  ");
        else if(strlen(i_char) == 2)
            strcat(men_cumpara, " ");

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt, 1, i_char, -1, SQLITE_STATIC);

        char produs[buff_length];
        memset(produs, 0, buff_length);

        if(sqlite3_step(stmt) == SQLITE_ROW)
            strcpy(produs, sqlite3_column_text(stmt,0));
        strcat(men_cumpara, produs);

        int nr_spatii = 34 - strlen(produs);
        snprintf(men_cumpara + strlen(men_cumpara), 
        sizeof(men_cumpara) - strlen(men_cumpara), 
        "%*s", 
        nr_spatii,
        " ");

        query = "select pret from market where id = ?";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt, 1, i_char, -1, SQLITE_STATIC);

        char pret[buff_length];
        memset(pret, 0, buff_length);

        if(sqlite3_step(stmt) == SQLITE_ROW)
            strcpy(pret, sqlite3_column_text(stmt,0));
        strcat(men_cumpara, pret);

        nr_spatii = 31 - strlen(pret);
        snprintf(men_cumpara + strlen(men_cumpara), 
        sizeof(men_cumpara) - strlen(men_cumpara), 
        "%*s", 
        nr_spatii,
        " ");
        
        query = "select cantitate from market where id = ?";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt, 1, i_char, -1, SQLITE_STATIC);

        if(sqlite3_step(stmt) == SQLITE_ROW)
            strcat(men_cumpara, sqlite3_column_text(stmt,0));
        strcat(men_cumpara,"\n");

    }

    sqlite3_finalize(stmt);
}

int verif_produs(sqlite3 *db, const char *produs)
{
    sqlite3_stmt *stmt;
    const char *query = "select produs from market where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, produs, -1, SQLITE_STATIC);

    char produs_verif[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(produs_verif, sqlite3_column_text(stmt,0));

    if(strcmp(produs_verif, produs) == 0)
    {    
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int verif_cantitate(sqlite3 *db, const char *produs, const char *cantitate)
{
    sqlite3_stmt *stmt;
    const char *query = "select cantitate from market where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, produs, -1, SQLITE_STATIC);

    char cantitate_verif[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(cantitate_verif, sqlite3_column_text(stmt,0));
    
    int cantitate_int, cantitate_verif_int;
    cantitate_int = atoi(cantitate);
    cantitate_verif_int = atoi(cantitate_verif);

    if(cantitate_verif_int >= cantitate_int)
    {
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int verif_sold(sqlite3 *db, const char *username)
{
    sqlite3_stmt *stmt;
    const char *query = "select id_utilizator from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char id_utilizator[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(id_utilizator, sqlite3_column_text(stmt,0));

    int valoare_plateste_int = 0;

    query = "select produs_cos from cos_cumparaturi where id_utilizator = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, id_utilizator, -1, SQLITE_STATIC);

    char produs_cos[buff_length];

    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
        strcpy(produs_cos, sqlite3_column_text(stmt,0));

        sqlite3_stmt *stmt2;
        const char *query2 = "select cantitate_cos from cos_cumparaturi where id_utilizator = ? and produs_cos = ?";

        if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt2, 1, id_utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt2, 2, produs_cos, -1, SQLITE_STATIC);

        char cantitate_cos[buff_length];

        if(sqlite3_step(stmt2) == SQLITE_ROW)
            strcpy(cantitate_cos, sqlite3_column_text(stmt2,0));

        query2 = "select pret from market where produs = ?";

        if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt2, 1, produs_cos, -1, SQLITE_STATIC);

        char pret[buff_length];

        if(sqlite3_step(stmt2) == SQLITE_ROW)
            strcpy(pret, sqlite3_column_text(stmt2,0));

        int pret_int = atoi(pret);
        int cantitate_cos_int = atoi(cantitate_cos);
        valoare_plateste_int = valoare_plateste_int + cantitate_cos_int * pret_int;

        sqlite3_finalize(stmt2);
    }

    query = "select sold from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char valoare_sold[buff_length];
    
    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(valoare_sold, sqlite3_column_text(stmt,0));

    int valoare_sold_int = atoi(valoare_sold);

    if(valoare_sold_int >= valoare_plateste_int)
    {
        sqlite3_finalize(stmt);
        return 1;
    }

    return 0;
    sqlite3_finalize(stmt);

}

void vinde(sqlite3 *db, const char *username, const char *produs, const char *cantitate)
{
    sqlite3_stmt *stmt;
    const char *query = "select sold from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char valoare_sold[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(valoare_sold, sqlite3_column_text(stmt,0));
    int valoare_sold_int = atoi(valoare_sold);

    query = "select pret from market where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, produs, -1, SQLITE_STATIC);

    char pret[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(pret, sqlite3_column_text(stmt,0));
    int pret_int = atoi(pret);

    query = "select cantitate from market where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, produs, -1, SQLITE_STATIC);

    char cantitate_db[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(cantitate_db, sqlite3_column_text(stmt,0));
    int cantitate_db_int = atoi(cantitate_db);
    int cantitate_int = atoi(cantitate);

    cantitate_db_int = cantitate_db_int + cantitate_int;
    valoare_sold_int = valoare_sold_int + cantitate_int * pret_int; 

    query = "update market set cantitate = ? where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sprintf(cantitate_db, "%d", cantitate_db_int);

    sqlite3_bind_text(stmt, 1, cantitate_db, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, produs, -1, SQLITE_STATIC);

    int rez = sqlite3_step(stmt);

    if (rez != SQLITE_DONE) 
    {
        perror("Eroare la executarea interogării.\n");
    }

    query = "update utilizatori set sold = ? where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sprintf(valoare_sold, "%d", valoare_sold_int);

    sqlite3_bind_text(stmt, 1, valoare_sold, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);

    rez = sqlite3_step(stmt);

    if (rez != SQLITE_DONE) 
    {
        perror("Eroare la executarea interogării.\n");
    }

    sqlite3_finalize(stmt);
}

void cumpara(sqlite3 *db, const char *username, const char *produs, const char *cantitate)
{
    sqlite3_stmt *stmt;
    const char *query = "select pret from market where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, produs, -1, SQLITE_STATIC);

    char pret[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(pret, sqlite3_column_text(stmt,0));
    int pret_int = atoi(pret);

    query = "select sold from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char valoare_sold[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(valoare_sold, sqlite3_column_text(stmt,0));
    int valoare_sold_int = atoi(valoare_sold);

    query = "select cantitate from market where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, produs, -1, SQLITE_STATIC);

    char cantitate_db[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(cantitate_db, sqlite3_column_text(stmt,0));
    int cantitate_db_int = atoi(cantitate_db);
    int cantitate_int = atoi(cantitate);

    cantitate_db_int = cantitate_db_int - cantitate_int;

    sprintf(cantitate_db, "%d", cantitate_db_int);

    query = "select id_utilizator from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char id_utilizator[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(id_utilizator, sqlite3_column_text(stmt,0));

    query = "select * from cos_cumparaturi where produs_cos = ? and id_utilizator = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, produs, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, id_utilizator, -1, SQLITE_STATIC);

    int rez = sqlite3_step(stmt);

    if(rez == SQLITE_ROW)
    {   
        query = "select cantitate_cos from cos_cumparaturi where id_utilizator = ? and produs_cos = ?";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt, 1, id_utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, produs, -1, SQLITE_STATIC);

        char cantitate_cos[buff_length];

        if(sqlite3_step(stmt) == SQLITE_ROW)
            strcpy(cantitate_cos, sqlite3_column_text(stmt,0));
        int cantitate_cos_int = atoi(cantitate_cos);

        cantitate_cos_int = cantitate_cos_int + cantitate_int;
        sprintf(cantitate_cos, "%d", cantitate_cos_int);

        query = "update cos_cumparaturi set cantitate_cos = ? where id_utilizator = ? and produs_cos = ?";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt, 1, cantitate_cos, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, id_utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, produs, -1, SQLITE_STATIC);

        int rez2 = sqlite3_step(stmt);

        if (rez2 != SQLITE_DONE) 
        {
            perror("Eroare la executarea interogării.\n");
        }
    }

    else if (rez == SQLITE_DONE)
    {   
        query = "insert into cos_cumparaturi (id_utilizator, produs_cos, cantitate_cos) values(?, ?, ?)";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt, 1, id_utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, produs, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, cantitate, -1, SQLITE_STATIC);

        int rez2 = sqlite3_step(stmt);

        if (rez2 != SQLITE_DONE) 
        {
            perror("Eroare la executarea interogării.\n");
        }
    }
    
    else
        perror("Eroare la executarea interogării.\n");

    query = "update market set cantitate = ? where produs = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, cantitate_db, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, produs, -1, SQLITE_STATIC);

    rez = sqlite3_step(stmt);

    if (rez != SQLITE_DONE) 
    {
        perror("Eroare la executarea interogării.\n");
    }

    sqlite3_finalize(stmt);
}

void cos_de_cumparaturi(sqlite3 *db, const char *username)
{   
    valoare_cos_int = 0;
    memset(valoare_cos, 0, buff_length);

    sqlite3_stmt *stmt;
    const char *query = "select id_utilizator from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char id_utilizator[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(id_utilizator, sqlite3_column_text(stmt,0));

    query = "select produs_cos from cos_cumparaturi where id_utilizator = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, id_utilizator, -1, SQLITE_STATIC);

    char produs_cos[buff_length];

    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
        strcpy(produs_cos, sqlite3_column_text(stmt,0));
        strcat(men_cos, produs_cos);
        strcat(men_cos, ": ");

        sqlite3_stmt *stmt2;
        const char *query2 = "select cantitate_cos from cos_cumparaturi where id_utilizator = ? and produs_cos = ?";

        if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt2, 1, id_utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt2, 2, produs_cos, -1, SQLITE_STATIC);

        char cantitate_cos[buff_length];

        if(sqlite3_step(stmt2) == SQLITE_ROW)
            strcpy(cantitate_cos, sqlite3_column_text(stmt2,0));
        strcat(men_cos, cantitate_cos);
        strcat(men_cos, "\n");

        query2 = "select pret from market where produs = ?";

        if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt2, 1, produs_cos, -1, SQLITE_STATIC);

        char pret[buff_length];

        if(sqlite3_step(stmt2) == SQLITE_ROW)
            strcpy(pret, sqlite3_column_text(stmt2,0));

        int pret_int = atoi(pret);
        int cantitate_cos_int = atoi(cantitate_cos);
        valoare_cos_int = valoare_cos_int + cantitate_cos_int * pret_int;

        sqlite3_finalize(stmt2);
    }

    sprintf(valoare_cos, "%d", valoare_cos_int);
    strcat(men_cos, "Suma produselor din cos este: ");
    strcat(men_cos, valoare_cos);
    strcat(men_cos, " RON");

    sqlite3_finalize(stmt);
}

void plateste(sqlite3 *db, const char *username)
{
    sqlite3_stmt *stmt;
    const char *query = "select id_utilizator from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char id_utilizator[buff_length];

    if(sqlite3_step(stmt) == SQLITE_ROW)
        strcpy(id_utilizator, sqlite3_column_text(stmt,0));

    int valoare_plateste_int = 0;

    query = "select produs_cos from cos_cumparaturi where id_utilizator = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, id_utilizator, -1, SQLITE_STATIC);

    char produs_cos[buff_length];

    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
        strcpy(produs_cos, sqlite3_column_text(stmt,0));

        sqlite3_stmt *stmt2;
        const char *query2 = "select cantitate_cos from cos_cumparaturi where id_utilizator = ? and produs_cos = ?";

        if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt2, 1, id_utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt2, 2, produs_cos, -1, SQLITE_STATIC);

        char cantitate_cos[buff_length];

        if(sqlite3_step(stmt2) == SQLITE_ROW)
            strcpy(cantitate_cos, sqlite3_column_text(stmt2,0));

        query2 = "select pret from market where produs = ?";

        if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        sqlite3_bind_text(stmt2, 1, produs_cos, -1, SQLITE_STATIC);

        char pret[buff_length];

        if(sqlite3_step(stmt2) == SQLITE_ROW)
            strcpy(pret, sqlite3_column_text(stmt2,0));

        int pret_int = atoi(pret);
        int cantitate_cos_int = atoi(cantitate_cos);
        valoare_plateste_int = valoare_plateste_int + cantitate_cos_int * pret_int;

        sqlite3_finalize(stmt2);
    }

    query = "select sold from utilizatori where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char valoare_sold[buff_length];

     if(sqlite3_step(stmt) == SQLITE_ROW)
            strcpy(valoare_sold, sqlite3_column_text(stmt,0));

    int valoare_sold_int = atoi(valoare_sold);
    valoare_sold_int = valoare_sold_int - valoare_plateste_int;
    sprintf(valoare_sold, "%d", valoare_sold_int);

    query = "update utilizatori set sold = ? where username = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, valoare_sold, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);

    int rez = sqlite3_step(stmt);

    if (rez != SQLITE_DONE) 
    {
        perror("Eroare la executarea interogării.\n");
    }

    query = "delete from cos_cumparaturi where id_utilizator = ?";

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
    {
        perror("Eroare la pregătirea interogării.\n");
    }

    sqlite3_bind_text(stmt, 1, id_utilizator, -1, SQLITE_STATIC);

    rez = sqlite3_step(stmt);

    if (rez != SQLITE_DONE) 
    {
        perror("Eroare la executarea interogării.\n");
    }

    sqlite3_finalize(stmt);
}

void golire_cos()
{   
    while(1)
    {   
        sleep(600);

        sqlite3_stmt *stmt;
        const char *query = "select produs_cos, cantitate_cos from cos_cumparaturi";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {   
            char produs_cos[buff_length] = "";
            char cantitate_cos[buff_length] = "";

            strcpy(produs_cos, sqlite3_column_text(stmt,0));
            strcpy(cantitate_cos, sqlite3_column_text(stmt,1));

            sqlite3_stmt *stmt2;
            const char *query2 = "select cantitate from market where produs = ?";

            if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
            {
                perror("Eroare la pregătirea interogării.\n");
            }

            sqlite3_bind_text(stmt2, 1, produs_cos, -1, SQLITE_STATIC);

            char cantitate_db[buff_length];

            if(sqlite3_step(stmt2) == SQLITE_ROW)
                strcpy(cantitate_db, sqlite3_column_text(stmt2,0));
            int cantitate_db_int = atoi(cantitate_db);
            int cantitate_cos_int = atoi(cantitate_cos);

            cantitate_db_int = cantitate_db_int + cantitate_cos_int;
            sprintf(cantitate_db, "%d", cantitate_db_int);

            query2 = "update market set cantitate = ? where produs = ?";

            if (sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL) != SQLITE_OK) 
            {
                perror("Eroare la pregătirea interogării.\n");
            }

            sqlite3_bind_text(stmt2, 1, cantitate_db, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt2, 2, produs_cos, -1, SQLITE_STATIC);

            int rez2 = sqlite3_step(stmt2);

            if (rez2 != SQLITE_DONE) 
            {
                perror("Eroare la executarea interogării.\n");
            }

            sqlite3_finalize(stmt2);
        }

        query = "delete from cos_cumparaturi";

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) 
        {
            perror("Eroare la pregătirea interogării.\n");
        }

        int rez = sqlite3_step(stmt);

        if (rez != SQLITE_DONE) 
        {
            perror("Eroare la executarea interogării.\n");
        }

        sqlite3_finalize(stmt);

        printf("Cosurile de cumparaturi au fost golite!\n");
    }
}