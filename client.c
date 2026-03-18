#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT 8080
#define BUFFER_SIZE 8192

// Culori ANSI
#define COLOR_RESET  "\x1b[0m"
#define COLOR_CYAN   "\x1b[36m"
#define COLOR_GREEN  "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_BOLD   "\x1b[1m"

void print_header() {
    printf(COLOR_MAGENTA COLOR_BOLD);
    printf("===========================================\n");
    printf("   🚀 GEMINI AI SECURE CLIENT🚀                        \n");
    printf("      Protocol: TLS v1.3 | Port: 8080     \n");
    printf("===========================================\n");
    printf(COLOR_RESET "\n");
}

void typing_effect(const char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        //1.traducem \n-ul din text in rand nou real
        if (text[i] == '\\' && text[i+1] == 'n') {
            printf("\n");
            i++; //sarim peste n
        } 
        //2. Traducem \" in ghilimele normale
        else if (text[i] == '\\' && text[i+1] == '"') {
            printf("\"");
            i++; 
        }
        //3.ignoram stelutele de markdown 
        else if (text[i] == '*') {
            continue;
        }
        else {
            printf("%c", text[i]);
        }

        fflush(stdout);
        usleep(12000); //viteza de tastare
    }
    printf("\n");
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int sock; //declaram sock separat

    //initializare SSL
    SSL_library_init();
    SSL_load_error_strings();
    const SSL_METHOD *method = SSLv23_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    //cream scoketul de baza
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    //adresa
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    //server connect
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf(COLOR_YELLOW "⚠️  Eroare: Serverul nu este pornit.\n" COLOR_RESET);
        return 1;
    }

    //securizam conexiunea
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        system("clear");
        print_header();

        while (1) {
            printf(COLOR_GREEN COLOR_BOLD "👤 Tu: " COLOR_RESET);
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) break;
            buffer[strcspn(buffer, "\n")] = 0;

            if (strcmp(buffer, "exit") == 0) break;

            SSL_write(ssl, buffer, strlen(buffer));

            printf(COLOR_CYAN COLOR_BOLD "🤖 Gemini: " COLOR_RESET COLOR_CYAN);
            memset(buffer, 0, BUFFER_SIZE);
            int bytes = SSL_read(ssl, buffer, sizeof(buffer));
            if (bytes > 0) {
                typing_effect(buffer);
            }
            printf(COLOR_RESET "\n");
        }
    }

    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    return 0;
}
