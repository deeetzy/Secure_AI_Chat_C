#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <pthread.h>
#include <curl/curl.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 8192

//mutex cu implementarea unei cozi
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

//clienti
typedef struct {
    int fd;
    SSL *ssl;
    char ip[INET_ADDRSTRLEN];
} client_data_t;

//regulile de securitate SSL
SSL_CTX *create_context() {
    const SSL_METHOD *method = SSLv23_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) { perror("SSL context error"); exit(EXIT_FAILURE); }

    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void get_ai_response(const char *user_input, char *output_buffer) {
    char command[10000];
    char clean_input[2048];

    strncpy(clean_input, user_input, 2047);
    clean_input[strcspn(clean_input, "\r\n\"")] = 0;

    //cererea catre google cu cheia
    snprintf(command, sizeof(command), 
    "curl -k -s -X POST 'https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash:generateContent?key=Put_your_key' "
    "-H 'Content-Type: application/json' "
    "-d '{ \"contents\": [{ \"parts\":[{ \"text\": \"%s\" }] }] }'", clean_input);

    //executie de comanda si citire
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        strcpy(output_buffer, "Eroare popen.");
        return;
    }

    char *raw = malloc(30000);
    memset(raw, 0, 30000);
    fread(raw, 1, 29999, fp);
    pclose(fp);

    //cautarea raspunsului
    char *text_pos = strstr(raw, "\"text\":");
    if (text_pos) {
        char *start = strchr(text_pos + 7, '\"'); 
        if (start) {
            start++;
            char *end = strstr(start, "\""); 
            if (end) {
                size_t len = end - start;
                if (len > 8191) len = 8191;
                strncpy(output_buffer, start, len);
                output_buffer[len] = '\0';
                free(raw);
                return;
            }
        }
    }
    free(raw);
    strcpy(output_buffer, "AI error or quota exceeded.");
}

//salvam istoricul conversatiilor
void log_to_file(const char *ip_addr, const char *tag, const char *message) {

    FILE *f = fopen("chat_history.log", "a");
    if (f != NULL) {
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strlen(ts) - 1] = '\0';//delete enter de la sfarsit
        fprintf(f, "[%s] [%s] %s: %s\n", ts, ip_addr, tag, message);
        fclose(f);
    }
}

//rulam clientii pe threaduri separate
void *handle_client(void *arg) {
    client_data_t *data = (client_data_t *)arg;//type casting
    char buffer[BUFFER_SIZE];
    char *ai_res = malloc(9000);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = SSL_read(data->ssl, buffer, sizeof(buffer));//citim
        if (bytes <= 0) break;

        //START WAIT LIST
        pthread_mutex_lock(&global_lock); 
        
        printf("[Queue] Procesez cererea pentru %s...\n", data->ip);
        fflush(stdout);

        log_to_file(data->ip, "USER_REQUEST", buffer);

        memset(ai_res, 0, 9000);
        get_ai_response(buffer, ai_res);

        log_to_file(data->ip, "AI_RESPONSE", ai_res);

        //trimitem raspunsul
        SSL_write(data->ssl, ai_res, strlen(ai_res));

        printf("[Queue] Finalizat cerere pentru %s. Eliberez coada.\n", data->ip);
        fflush(stdout);

        pthread_mutex_unlock(&global_lock);
        //END WAIT LIST
    }

    SSL_shutdown(data->ssl);
    SSL_free(data->ssl);
    close(data->fd);
    free(ai_res);
    free(data);
    return NULL;
}

int main() {
    //OpenSSL library
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL_CTX *ctx = create_context();

    //socket principal
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = INADDR_ANY };
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("Server pornit pe portul %d. Sistemul de Wait List este ACTIV.\n", PORT);
    fflush(stdout);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t len = sizeof(caddr);
        int client_fd = accept(server_fd, (struct sockaddr*)&caddr, &len);      //asteapta clienti
        if (client_fd < 0) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &caddr.sin_addr, client_ip, INET_ADDRSTRLEN);

        //conexiune securizata
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_fd);

        //cheile de criptare
        if (SSL_accept(ssl) <= 0) {
            close(client_fd);
            continue;
        }

        client_data_t *cdata = malloc(sizeof(client_data_t));
        cdata->fd = client_fd;
        cdata->ssl = ssl;
        strcpy(cdata->ip, client_ip);

        //creare thread nou
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, cdata);

        //elibereaza memoria odata ce termina
        pthread_detach(tid); 
    }
    return 0;
}
