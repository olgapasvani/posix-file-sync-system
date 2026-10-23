#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define MAX_WORKERS 5

//Δομη συγχρονισμού
//Θα αποθηκεύουμε τα ονόματα των καταλόγων που θέλουμε να συγχρονίσουμε
typedef struct sync_info {
	char source_dir[100];
	char target_dir[100];
	char status[32];
	char last_sync_time[25]; //Αποθηκεύουμε την ώρα συγχρονισμού
	int active;	// Θα αποθηκεύουμε εδώ το watch descriptor της inotify
	int error_count;

	struct sync_info *next; //κάνουμε μια linked list για τα ζεύγη καταλόγων
}sync_info;

//Ανοιγουμε το αρχείο καταλόγων και γεμίζουμε την δομή μας με τα ζευγάρια καταλόγων
//Επιστρέφουμε το file descriptor του αρχείου
int open_config_file(char *config_file, sync_info **head);

//Συγχρονισμός ενός ζεύγους καταλόγων
int sync_dir(int *, int , sync_info *, uint32_t , char *);

//Ψάχνουμε αν το γεγονός που μας έδωσε η inotify αφορά κάποιον από τους καταλόγους
int search_to_sync(sync_info *, struct inotify_event *, int , int, int *, FILE *);

//Ελέγχουμε αν οι παράμετροι που πήραμε είναι σωστές
int parameter_check(int , char *[], int *);

//Αποθηκευουμε την ώρα συγχρονισμού στο αρχείο καταγραφής
void save_time_of_sync(FILE *, sync_info *);

//επιστρέφει την ώρα στην μορφή [YYYY-MM-DD HH:MM:SS]
char* get_time(char *);

//Κλείνουμε το σύστημα, όταν ο χρήστης δώσει στην κονσόλα την εντολή shutdown
int shutdown(FILE *, int *, int, int);

//Προσθέτουμε ένα νέο ζεύγος καταλόγων στην λίστα συγχρονισμού
int add_dirs(char *, sync_info *, int *, int , int , FILE *, int);

//Επιστρέφουμε την κατάσταση ενός ζεύγους καταλόγων
int print_status(char *, sync_info *, int);

//Ακυρώνουμε την παρακολούθηση ενός ζεύγους καταλόγων
int cancel_sync(char *, sync_info *, int, int);

//ψάχνουμε εναν κατάλογο που έδωσε ο χρήστης για να τον συγχρονήσουμε
int sync_console(char *, sync_info *, int , int *, int, int);