#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    //Ελέγχουμε και αποθηκεύουμε τα ορίσματα
    if(argc != 5) {
        perror("Usage: %s <source_dir> <target_dir> <filename> <Operation>\n");
        exit(1);
    }
    char *source_dir = argv[1];
    char *target_dir = argv[2];
    char *filename = argv[3];
    char *operation = argv[4];

    //Αποθηκεύουμε το ονομα-path του αρχειου
    char src_path[100];
    sprintf(src_path, "%s/%s", source_dir, filename);
    int fd = open(src_path, O_RDWR);

    //ελεγχουμε αν το αρχείο υπάρχει
    if(fd == -1 && (strcmp(filename, "ALL") != 0)) {
        printf("File %s does not exist\n", filename);
        perror("Error opening file in worker");
        exit(2);
    }

    char command[512];
    snprintf(command, sizeof(command), "cp -R %s/. %s/", source_dir, target_dir);

    char tgt_path[100];
    //Αν αντί για το όνομα του αρχείου έχουμε ALL, ή στο operation FULL τότε κάνουμε FULL SYNC
    if(strcmp(operation, "FULL") == 0 || strcmp(filename, "ALL") == 0) {
        //execlp("rm", "rm", "-r", "-f", target_dir, (char *)NULL);
        execlp("sh", "sh", "-c", command, (char *)NULL);

        perror("execlp failed");
        exit(2);
    } 
    else if(strcmp(operation, "MODIFY") == 0 || strcmp(operation, "CREATE") == 0) {
        printf("MODIFY/n");

        //Συνθέτουμε το όνομα του path του αρχείου
        sprintf(src_path, "%s/%s", source_dir, filename);
        sprintf(tgt_path, "%s/%s", target_dir, filename);
        execlp("sh", "sh", "-c", command, (char *)NULL);
        perror("execlp failed");
        exit(2);
    } 
    else if(strcmp(operation, "DELETE") == 0) {
        printf("DELETE\n");
        //Συνθέτουμε το όνομα του path του αρχείου
        sprintf(src_path, "%s/%s", source_dir, filename);
        sprintf(tgt_path, "%s/%s", target_dir, filename);
        execlp("rm", "rm", "-r", "-f", tgt_path, (char *)NULL);
        perror("execlp failed");
        exit(2);
    } else {
        perror("Unknown operation");
        exit(1);
    }
    
    printf("%s, %s\n", argv[1], argv[2]);

    exit(0);
}