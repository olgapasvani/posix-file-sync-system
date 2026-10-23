#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    //Ελεγχος παραμέτρων
	if (argc != 3) {
        perror("Usage: %s -l <console-logfile>\n");
        exit(1);
    }

    char consol_file[15];

    if(strcmp(argv[1], "-l") == 0) {strcpy(consol_file, argv[2]);}
    else{
        fprintf(stderr, "Unknown argument %s instead of -l\n", argv[1]);
        exit(1);
    }

    //Ανοίγουμε τα αρχεία
    FILE *logfile = fopen(consol_file, "w");
    if (logfile == NULL) {
        perror("fopen failed");
        exit(1);
    }
    int fss_in_fd, fss_out_fd;
    char input[100];
    while(strcmp(input, "shutdown") != 0) {
    
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("fgets failed");
            break;
        }
        // Αφαιρούμε το \n
        input[strcspn(input, "\n")] = '\0';

        fss_in_fd = open("fss_in", O_WRONLY);
        fss_out_fd = open("fss_out", O_RDONLY);
        if(fss_in_fd == -1 || fss_out_fd == -1) {
            perror("Error opening named pipe");
            exit(2);
        }
        // Γράφουμε αυτό που διβάσαμε στο fss_in για να το πάρει ο manager
        write(fss_in_fd, input, strlen(input)+1);

        // Διαβάζουμε την απάντηση από τον manager
        char answer[150];
        sleep(1);
		int pipe_read = read(fss_out_fd, answer, sizeof(answer) - 1);
        if (pipe_read > 0) {
			answer[pipe_read] = '\0';
            fprintf(logfile, "%s", answer);
	        fflush(logfile);
        }
    }
    close(fss_in_fd);
    close(fss_out_fd);
    fclose(logfile);
    return 0;
}