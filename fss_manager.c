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
#include <time.h>
#include <poll.h>

#include "manager_functions.h"


int main(int argc, char *argv[]) {
	//Έλεγχος παραμέτρων
	int worker_limit = 0;
	parameter_check(argc, argv, &worker_limit);

	//Αποθηκεύουμε τα ονόματα των αρχείων
	char log_file[15], config_file[15];
	strcpy(log_file, argv[2]);
	strcpy(config_file, argv[4]);

//----------Τέλος ελέγχου και αποθήκευσης Παραμέτρων------------------------------


//Προετοιμάζουμε το σύστημα για τον συγχρονισμό των καταλόγων--------------------
	
	//φτιάχνουμε τους δύο βασικούς pipes fss_in, fss_out
	unlink("fss_in");
	unlink("fss_out");
	if(( mkfifo("fss_in", 0666) == -1) || (mkfifo("fss_out", 0666) == -1)) {
	  perror("Error opening named pipes");
	  exit(2);
	}

	//Αποθηκεύουμε τα ονόματα των κλαταλόγων
	sync_info *head = NULL;
	int config_fd = open_config_file(config_file, &head);

	// Άνοιγμα των named pipes για επικοινωνία
    int fss_in_fd = open("fss_in", O_RDWR);
    int fss_out_fd = open("fss_out", O_RDWR);
	// Έλεγχος αν τα named pipes άνοιξαν επιτυχώς 
	if (fss_in_fd == -1 || fss_out_fd == -1) {
		perror("Error opening named pipes");
		exit(2);
	}

	//Ανοίγουμε το αρχείο καταγραφής manager_logfile
	FILE *logfile_fopen = fopen(log_file, "w");
	if (logfile_fopen == NULL) {
		perror("Error opening manager logfile");
		exit(2);
	}

	int status;
	int active_workers = 0;
	

//----------ξεκινάει ο συγχρονισµός των καταλόγων-----------------

	sync_info *current = head;
	while (current != NULL) {

		//Για κάθε κόμβο δηλαδή για κάθε ζευγάρι καταλόγων καλούμε την συνάρτηση συγχρονισμού
		int s = sync_dir(&active_workers, worker_limit, current, 0, "ALL");

		//Αποθηκεύουμε την ώρα συγχρονισμού στο αρχείο καταγραφής
		if(s == 0)save_time_of_sync(logfile_fopen, current);
		else if(s == -1) {
			perror("Error in sync_dir");
			exit(2);
		}

		//Πάμε στο επόμενο ζεύγος καταλόγων
		current = current->next;
	}

//-----Τέλος αρχικού συγχρονισμού-------------------------------------------------------

	//Αρχικοποιούμε την inotify
	int inotify_fd = inotify_init();
	if (inotify_fd < 0) {
		perror("inotify initialization failed");
		exit(2);
	}

	//Δημιουργούμε την inotify watch list
	current = head;
	while (current != NULL) {

		//Για κάθε source_dir θέλουμε έναν inotify watch descriptor
		int wd = inotify_add_watch(inotify_fd, current->source_dir, IN_MODIFY | IN_CREATE | IN_DELETE);
		if (wd < 0) {
			perror("inotify_add_watch failed");
			exit(2);
		}
		current->active = wd; //αποθηκεύουμε το watch descriptor
		current = current->next;
	}

//Ανοίγουμε την fss_console-------------------------------------------------
	pid_t conssole_pid = fork();
	if (conssole_pid < 0) {
		perror("fork failed");
		exit(2);
	} else if (conssole_pid == 0) { //Child process
		execlp("./fss_console", "./fss_console", "-l", "console-logfile", NULL);
		perror("execlp failed");
		exit(2);
	} else {
		//printf("console with PID: %d\n", conssole_pid);
	}


//Παρακολούθηση για γεγονότα-------------------------------------------------------------
	char event_buffer[1024];
	//struct inotify_event **event_queue;
	int i = 0;
	//event_queue = malloc(sizeof(struct inotify_event *));
	/*if (event_queue == NULL) {
		perror("malloc failed");
		exit(3);
	}*/
		while (1) {
			struct pollfd fds[2];
		
			fds[0].fd = inotify_fd;
			fds[0].events = POLLIN;
		
			fds[1].fd = fss_in_fd;
			fds[1].events = POLLIN;
		
			int poll_result = poll(fds, 2, -1);
			if (poll_result < 0) {
				perror("poll failed");
				exit(1);
			}
		
			// Έλεγχος αν υπάρχει event από inotify
			if (fds[0].revents & POLLIN) {
				int length = read(inotify_fd, event_buffer, sizeof(event_buffer));
				if (length < 0) {
					perror("read from inotify failed");
					exit(3);
				}
				struct inotify_event *event = (struct inotify_event *) event_buffer;
				if (event->mask & (IN_MODIFY | IN_CREATE | IN_DELETE)) {
					search_to_sync(head, event, i, worker_limit, &active_workers, logfile_fopen);
				}
				//printf("event\n");
			}
		
			// Έλεγχος αν υπάρχει εντολή από pipe
			if (fds[1].revents & POLLIN) {
				char command[100];
				int pipe_read = read(fss_in_fd, command, sizeof(command) - 1);
				if (pipe_read > 0) {
					command[pipe_read] = '\0';
					
					//Shutdown manager
					if (strncmp(command, "shutdown", 8) == 0) {
						shutdown(logfile_fopen, &active_workers, worker_limit, fss_out_fd);
						break;
					} 
					//Προσθέτουμε νέο ζεύγος καταλόγων στο σύστημα
					else if (strncmp(command, "add", 3) == 0) {
						add_dirs(command, head, &active_workers, worker_limit, inotify_fd, logfile_fopen, fss_out_fd);
					} 
					else if (strncmp(command, "status", 6) == 0) {
						print_status(command, head, fss_out_fd);
					}
					else if (strncmp(command, "cancel", 6) == 0) {
						cancel_sync(command, head, inotify_fd, fss_out_fd);
					}
					else if (strncmp(command, "sync", 4) == 0) {
						sync_console(command, head, inotify_fd, &active_workers, worker_limit, fss_out_fd);
					}
					else {
						printf("%s: command not found\n", command);
						write(fss_out_fd, "error: command not found\n", 25);
					}
				}
			}
			i++;
			//event_queue = realloc(event_queue, (i + 1) * sizeof(struct inotify_event *));
			/*if (event_queue == NULL) {
				perror("realloc failed");
				exit(3);
			}*/
		}

	// Περιμένουμε ΟΛΟΥΣ τους τελευταίους workers να τελειώσουν
    while (active_workers > 0) {
        pid_t finished_pid = wait(&status);
        if (finished_pid > 0) {
            active_workers--;
            printf("Worker %d finished. Active workers now: %d\n", finished_pid, active_workers);
        }
    }

//κλεινουμε το αρχείο καταλόγων------------------------------------------
	close(config_fd);
	close(fss_in_fd);
	close(fss_out_fd);
	fclose(logfile_fopen);

	//Κλείνουμε την inotify
	current = head;
	while (current->next != NULL) {
		if (inotify_rm_watch(inotify_fd, current->active) < 0) {
			perror("inotify_rm_watch failed");
			exit(2);
		}
		current = current->next;
	}
	
	close(inotify_fd);
	//Απελευθερώνουμε τη μνήμη
	logfile_fopen = fopen(log_file, "w");
    sync_info *tmp;
	char cwd[100];
    while (head != NULL) {
		char err[20];
		if(head->error_count > 0) {
			strcpy(err, "[ERROR]");
		}else { strcpy(err, "[SUCCESS]");}
		
		fprintf(logfile_fopen, "%s/%s -> /%s Last Sync: %s %s\n",  getcwd(cwd, sizeof(cwd)), head->source_dir, head->target_dir, head->last_sync_time, err);
        tmp = head;
        head = head->next;
        free(tmp);
    }
	fclose(logfile_fopen);
	return 0;
}