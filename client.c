// client.c — Interactive CLI for the Task Manager

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFSZ 4096

void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1]=='\n'||s[n-1]=='\r')) s[--n]='\0';
}

int recv_until_end(int fd) {
    char line[BUFSZ];
    size_t pos = 0;
    for (;;) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return -1;
        line[pos++] = c;
        if (c == '\n' || pos == sizeof line - 1) {
            line[pos] = '\0';
            if (strcmp(line, "END\n") == 0) break;
            fputs(line, stdout);
            pos = 0;
        }
    }
    return 0;
}

int send_cmd(int fd, const char *cmd) {
    return send(fd, cmd, strlen(cmd), 0) < 0 ? -1 : 0;
}

void menu() {
    printf(
      "\n==== FILE-BASED TASK MANAGER ====\n"
      "1. Add Task\n2. List Tasks\n3. Search Tasks\n4. Mark Task Done\n"
      "5. Delete Task\n6. Download All\n7. Help\n8. Quit\nSelect: ");
    fflush(stdout);
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv = {0};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);
    if (connect(sock, (struct sockaddr*)&serv, sizeof serv) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to %s:%d\n", SERVER_IP, PORT);

    char in[BUFSZ];
    for (;;) {
        menu();
        if (!fgets(in, sizeof in, stdin)) break;
        trim_newline(in);
        if (!*in) continue;

        if (!strcmp(in, "1")) {
            char desc[BUFSZ], prio[64];
            printf("Description: ");
            if (!fgets(desc, sizeof desc, stdin)) continue;
            trim_newline(desc);
            if (!*desc) { puts("Cancelled."); continue; }

            printf("Priority (Low/Medium/High) [Medium]: ");
            if (!fgets(prio, sizeof prio, stdin)) strcpy(prio, "");
            else trim_newline(prio);
            if (!*prio) strcpy(prio, "Medium");

            char cmd[BUFSZ];
            snprintf(cmd, sizeof cmd, "ADD %s|%s", desc, prio);
            send_cmd(sock, cmd);
            recv_until_end(sock);
        }
        else if (!strcmp(in, "2")) {
            send_cmd(sock, "LIST");
            recv_until_end(sock);
        }
        else if (!strcmp(in, "3")) {
            char kw[BUFSZ];
            printf("Keyword: ");
            if (!fgets(kw, sizeof kw, stdin)) continue;
            trim_newline(kw);
            kw[sizeof(kw)-8] = '\0'; // prevent truncation
            char cmd[BUFSZ];
            snprintf(cmd, sizeof cmd, "SEARCH %s", kw);
            send_cmd(sock, cmd);
            recv_until_end(sock);
        }
        else if (!strcmp(in, "4")) {
            char id[64];
            printf("Task ID: ");
            if (!fgets(id, sizeof id, stdin)) continue;
            trim_newline(id);
            char cmd[BUFSZ];
            snprintf(cmd, sizeof cmd, "DONE %s", id);
            send_cmd(sock, cmd);
            recv_until_end(sock);
        }
        else if (!strcmp(in, "5")) {
            char id[64];
            printf("Task ID: ");
            if (!fgets(id, sizeof id, stdin)) continue;
            trim_newline(id);
            char cmd[BUFSZ];
            snprintf(cmd, sizeof cmd, "DELETE %s", id);
            send_cmd(sock, cmd);
            recv_until_end(sock);
        }
        else if (!strcmp(in, "6")) {
            send_cmd(sock, "DOWNLOAD");
            recv_until_end(sock);
        }
        else if (!strcmp(in, "7")) {
            send_cmd(sock, "HELP");
            recv_until_end(sock);
        }
        else if (!strcmp(in, "8")) {
            send_cmd(sock, "QUIT");
            recv_until_end(sock);
            break;
        }
        else puts("Invalid choice.");
    }

    close(sock);
    puts("Disconnected.");
    return 0;
}
