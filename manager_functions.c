#include "manager_functions.h"

int parameter_check(int argc, char *argv[], int *worker_limit) {
	//Ελεγχος παραμέτρων
	if (argc != 7) {
		perror("Usage: %s -l <logfile> -c <config_file> -n <worker_limit>\n");
		exit(1);
  	}

  	(*worker_limit) = 0;
  	//ελέγχουμε ότι οι παράμετροι έχουν εισαχθεί με σωστό τρόπο
  	if(strcmp(argv[1], "-l") != 0) {
		fprintf(stderr, "Unknown argument %s instead of -l\n", argv[1]);
		exit(1);
  	}

  	if(strcmp(argv[3], "-c") != 0) {
		fprintf(stderr, "Unknown argument %s instead of -c\n", argv[3]);
		exit(1);
  	}

  	if(strcmp(argv[5], "-n") == 0) {
	  if(atoi(argv[6]) > MAX_WORKERS) {
		  printf("Max workers limit is %d\n", MAX_WORKERS);
		  (*worker_limit) = MAX_WORKERS;
	  } else (*worker_limit) = atoi(argv[6]);
  	} else{
		fprintf(stderr, "Unknown argument %s instead of -n\n", argv[5]);
		exit(1);
  	}
	return 0;
}

int open_config_file(char *config_file, sync_info **head) {
	int fd = open(config_file, O_RDWR); //ανοίγουμε το file με τους καταλόγους
	if(fd == -1){
	  perror("Error opening config file");
	  exit(3);
	}

	sync_info *tail = NULL;
	int stop = 1;    //Θα διαβάσουμε τα ονόματα των καταλόγων μέχρι το αρχείο να φτάσει στο τέλος του
	while(stop != 0){
		//Δημιουργούμε έναν κόμβο για να γεμίσουμε την δομή μας με τα ζευγάρια καταλόγων
		sync_info *current = malloc(sizeof(sync_info));
		if (current == NULL) {
            perror("malloc failed");
            exit(3);
        }

		//αποθηκέουμε προσωρινά τα ονόματα των καταλόγων
		char *source = malloc(99*sizeof(char)); 
    	char *target = malloc(99*sizeof(char));
		if (source == NULL || target == NULL) {
            perror("malloc failed");
            exit(3);
        }

		//Διαβάζουμε το αρχείο μέχρι να βρούμε το διάστημα που χωρίζει τα δύο ονόματα
		int i, j;
		char ch;
		for(i = 0; i < 99; i++) { //διαβάζουμε το source_dir
	  		stop = read(fd, &ch, 1);
			if(stop == 0) break;
	  		if(ch == ' ') break;
	  		source[i] = ch;
		}
		source[i] = '\0';

		for(j = 0; j < 99; j++) { //διαβάζουμε το target_dir
        	stop = read(fd, &ch, 1);
			if(stop == 0) break;
        	if(ch == ' ' || ch == '\n') break;
			target[j] = ch;
        }
		target[j] = '\0';
		
		//Αποθηκεύουμε τα ονόματα στον κόμβο
		strcpy(current->source_dir, source);
		strcpy(current->target_dir, target);
		get_time(current->last_sync_time);
		strcpy(current->status, "just added to the system");
		current->active = 0; //Θα αποθηκεύουμε εδώ το watch descriptor της inotify
		current->error_count = 0;
		current->next = NULL;

		//Αποθηκεύουμε τον κόμβο στην λίστα
		if(*head == NULL) {
			*head = current;
			tail = current;
		} else {
			tail->next = current;
			tail = current;
		}

		free(source);
		free(target);
	}
	return fd;
}



int search_to_sync(sync_info *head, struct inotify_event *event, int i, int worker_limit, int *active_workers, FILE *logfile_fopen) {
	sync_info *current = head;
	while(current != NULL) {
		if(event->wd == current->active) {
/////////////Αν το event αφορά κάποιον καταλογο τον αποθηκεύουμε
			//event_queue[i] = event;
			sync_dir(active_workers, worker_limit, current, event->mask, event->name);
			//Αποθηκεύουμε την ώρα συγχρονισμού στο αρχείο καταγραφής
			save_time_of_sync(logfile_fopen, current);
			break; // Βρήκαμε τον κατάλογο που ταιριάζει, δεν χρειάζεται να συνεχίσουμε
		}
		current = current->next;
	}
	printf("> ");
	fflush(stdout);
	return 0;
}

int sync_dir(int *active_workers, int worker_limit, sync_info *head, uint32_t mask, char *event_name) {
	int status;
	//Καλούμε  έναν worker για να συγχρονίσει τους καταλόγους
	while (*active_workers >= worker_limit) {//Έλεγχος ορίου διεργασιών
		//Περιμένουμε να τελειώσει κάποιος worker
		pid_t finished_pid = wait(&status);
		if (finished_pid > 0) {
		   (*active_workers)--;
			//printf("Worker %d finished. Active workers now: %d\n", finished_pid, *active_workers);
		}
	}

	//Εφόσον δεν έχουμε φτάσει το όριο των workers, καλούμε την fork()
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork failed");
		exit(2);
	} else if (pid == 0) { //Child process
		//Εκτελούμε το fss_worker()
		if(mask & IN_MODIFY) {
			execlp("./fss_worker", "./fss_worker", head->source_dir, head->target_dir, event_name, "MODIFY", (char *)NULL);
		} else if(mask & IN_CREATE) {
			execlp("./fss_worker", "./fss_worker", head->source_dir, head->target_dir, event_name, "CREATE", (char *)NULL);
		} else if(mask & IN_DELETE) {
			printf("Going to delete: \n");
			execlp("./fss_worker", "./fss_worker", head->source_dir, head->target_dir, event_name, "DELETE", (char *)NULL);
		} else {
			execlp("./fss_worker", "./fss_worker", head->source_dir, head->target_dir, event_name, "FULL", (char *)NULL);
		}
		perror("execlp failed");
		strcpy(head->status, "Error");
		head->error_count++;
		return -1;
	}
	// Είμαστε στον γονιό (manager), και αφού έχουμε κανει fork() για worker αυκάνουμε τον μετρητή active_workers
	strcpy(head->status, "Active");
	get_time(head->last_sync_time);
	(*active_workers)++;
	return 0;
}

void save_time_of_sync(FILE *logfile_fopen, sync_info *current) {
	char buffer[26];
	char cwd[100];
	//Αποθηκεύουμε την τρέχουσα ώρα
	get_time(buffer);

	char src_path[150];
    sprintf(src_path, "%s/%s", getcwd(cwd, sizeof(cwd)), current->source_dir);

	char trg_path[150];
	sprintf(trg_path, "%s/%s", getcwd(cwd, sizeof(cwd)), current->target_dir);

	fprintf(logfile_fopen, "%s Added directory: %s -> %s\n", get_time(buffer), src_path, trg_path);
	fflush(logfile_fopen);
	printf("%s Added directory: %s -> %s\n", buffer, src_path, trg_path);

	fprintf(logfile_fopen, "%s Monitoring started for %s\n", get_time(buffer), src_path);
	fflush(logfile_fopen);	
	printf("%s Monitoring started for %s\n", buffer, src_path);
}

char* get_time(char *buffer) {
	time_t t;
	struct tm *tm_info;
	//Αποθηκεύουμε την τρέχουσα ώρα
	time(&t);
	tm_info = localtime(&t);
	strftime(buffer, 26, "[%Y-%m-%d %H:%M:%S]", tm_info);
	return buffer;
}

int shutdown(FILE *logfile_fopen, int *active_workers, int worker_limit, int fss_out_fd) {
	char time_buffer[25];
	int status;
	printf("%s Shutting down manager...\n", get_time(time_buffer));
	fprintf(logfile_fopen, "%s Shutting down manager...\n", time_buffer);
	fflush(logfile_fopen);

	printf("%s Waiting for all active workers to finish.\n",  get_time(time_buffer));
	fprintf(logfile_fopen, "%s Waiting for all active workers to finish.\n", time_buffer);
	fflush(logfile_fopen);
	while (*active_workers > 0) {//Έλεγχος ορίου διεργασιών
	//Περιμένουμε να τελειώσουν οι workers
		pid_t finished_pid = wait(&status);
		if (finished_pid > 0) {
			(*active_workers)--;
		}
	}

	char cwd[100];
	char answer[150];
	sprintf(answer, "Command shutdown %s/\n", getcwd(cwd, sizeof(cwd)));
	write(fss_out_fd, answer, sizeof(answer));

	printf("%s Processing remaining queued tasks.\n",  get_time(time_buffer));
	fprintf(logfile_fopen, "%s Processing remaining queued tasks.\n", time_buffer);
	fflush(logfile_fopen);

	printf("%s Manager shutdown complete.\n",  get_time(time_buffer));
	fprintf(logfile_fopen, "%s Manager shutdown complete.\n", time_buffer);
	fflush(logfile_fopen);
	return 0;
}

int add_dirs(char *command, sync_info *head, int *active_workers, int worker_limit, int inotify_fd, FILE *logfile_fopen, int fss_out_fd) {
	//Σπάμε την έντολή που πήραμε απο την κονσόλα σε tokens για να πάρουμε τα ονόματα των καταλόγων
	char *token = strtok(command, " ");
	char *source = strtok(NULL, " ");
	char *target = strtok(NULL, " ");
	if (token == NULL || source == NULL || target == NULL) {
		perror("Invalid command format. Usage: add <source_dir> <target_dir>");
		return -1;
	}
					
	sync_info *new_node = malloc(sizeof(sync_info));
	sync_info *current = malloc(sizeof(sync_info));
	if (new_node == NULL || current == NULL) {
		perror("malloc failed");
		return -1;
	}
	//Αποθηκεύουμε τα ονόματα στον κόμβο
	strcpy(new_node->source_dir, source);
	strcpy(new_node->target_dir, target);
	new_node->active = 0;
	new_node->error_count = 0;
	new_node->next = NULL;
	if (head == NULL) {
		head = new_node;
	} else {
		current = head;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = new_node; //προσθέτουμε τον κόμβο στο τέλος της λίστας
	}
	current = new_node;
	printf(" Added directories: %s -> %s\n", current->source_dir, current->target_dir);
	
	int status;
	while ((*active_workers) >= worker_limit) {//Έλεγχος ορίου διεργασιών
	//Περιμένουμε να τελειώσει κάποιος worker
		pid_t finished_pid = wait(&status);
		if (finished_pid > 0) {
			(*active_workers)--;
			//printf("Worker %d finished. Active workers now: %d\n", finished_pid, *active_workers);
		}
	}
						
	//Εφόσον δεν έχουμε φτάσει το όριο των workers, καλούμε την fork()
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork failed");
		exit(2);
	} else if (pid == 0) {
		//Στην διεργασία παιδί καλούμε execlp για να εκτελεσει mkdir και να φτιάξει τουε καταλόγους
		execlp("mkdir", "mkdir", "-p", current->source_dir, current->target_dir, NULL);
		perror("execlp failed make directorys");
		return -1;
	}

	//Περιμένουμε να τελειώσει η διεργασία mkdir πριν καλέσουμε την inotify_add_watch()
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		perror( "mkdir failed\n");
		char answer[150];
		sprintf(answer, "error: add, mkdir failed\n");
		write(fss_out_fd, answer, sizeof(answer));
		return -1;
	}

	//Αφού έχουμε βάλει τα ονόματα των καταλόγων στην δομή μας
	//και έχουμε φτιάξει τους καταλόγους
	//καλούμε την inotify_add_watch() για να παρακολουθήσουμε τους καταλόγους
	int wd = inotify_add_watch(inotify_fd, current->source_dir, IN_MODIFY | IN_CREATE | IN_DELETE);
	if (wd < 0) {
		perror("inotify_add_watch failed");
		return -1;
	}
	current->active = wd;

	//Καλούμε την συνάρτηση συγχρονισμού για να συγχρονίσουμε τους καταλόγους
	int s = sync_dir(active_workers, worker_limit, current, 0, "ALL");

	//Αποθηκεύουμε την ώρα συγχρονισμού στο αρχείο καταγραφής
	if(s == 0)save_time_of_sync(logfile_fopen, current);

	//στέλνουμε μήνυμα στην κονσόλα ότι προσθέσαμε τους καταλόγους
	char answer[220];
	sprintf(answer, "Command add %s/ -> %s/\n", current->source_dir, current->target_dir);
	write(fss_out_fd, answer, sizeof(answer));

	return 0;
}

int print_status(char *command, sync_info *head, int fss_out_fd){
	//Σπάμε την έντολή που πήραμε απο την κονσόλα σε tokens για να πάρουμε τα ονόματα των καταλόγων
	char *token = strtok(command, " ");
	char *directory = strtok(NULL, " ");
	if (token == NULL || directory == NULL) {
		perror("Invalid command format. Usage: status <directory>");
		return -1;
	}
	char time_buffer[25];
	printf("%s Status requested for %s\n", get_time(time_buffer), directory);

	sync_info *current = head;
	while(current != NULL) {
		if (strcmp(current->source_dir, directory) == 0) {
			printf("Directory: %s\n", current->source_dir);
			printf("Target: %s\n", current->target_dir);
			printf("Last Sync: %s\n", current->last_sync_time);
			printf("Errors: %d\n", current->error_count);
			printf("Status: %s\n", current->status);

			char answer[150];
			sprintf(answer, "Command status %s/\n", current->source_dir);
			write(fss_out_fd, answer, sizeof(answer));
			return 0;
		}
		current = current->next;
	}
	printf("Source Directory %s not found in the system\n", directory);
	return 1;
}

int cancel_sync(char *command, sync_info *head, int inotify_fd, int fss_out_fd) {
	//Σπάμε την έντολή που πήραμε απο την κονσόλα σε tokens για να πάρουμε τα ονόματα των καταλόγων
	char *token = strtok(command, " ");
	char *directory = strtok(NULL, " ");
	if (token == NULL || directory == NULL) {
		perror("Invalid command format. Usage: status <directory>");
		return -1;
	}

	sync_info *current = head;
	while(current != NULL) {
		if (strcmp(current->source_dir, directory) == 0) {
			if (inotify_rm_watch(inotify_fd, current->active) < 0) {
				perror("inotify_rm_watch failed");
				exit(2);
			}
			char answer[150];
			sprintf(answer, "Command cancel %s/\n", current->source_dir);
			write(fss_out_fd, answer, sizeof(answer));
			return 0;
		}
		current = current->next;
	}

	//Αν δεν βρούμε τον κατάλογο που ζητάμε
	printf("Source Directory %s not found in the system\n", directory);
	char answer[150];
	sprintf(answer, "error in cancel: %s/ not found\n", current->source_dir);
	write(fss_out_fd, answer, sizeof(answer));
	return 1;
}

int sync_console(char *command, sync_info *head, int inotify_fd, int *active_workers, int worker_limit, int fss_out_fd) {
	//Σπάμε την έντολή που πήραμε απο την κονσόλα σε tokens για να πάρουμε τα ονόματα των καταλόγων
	char *token = strtok(command, " ");
	char *directory = strtok(NULL, " ");
	if (token == NULL || directory == NULL) {
		perror("Invalid command format. Usage: status <directory>");
		return -1;
	}

	sync_info *current = head;
	while(current != NULL) {
		if (strcmp(current->source_dir, directory) == 0) {
			int wd = inotify_add_watch(inotify_fd, current->source_dir, IN_MODIFY | IN_CREATE | IN_DELETE);
			if (wd < 0) {
				perror("inotify_add_watch failed");
				exit(2);
			}
			current->active = wd; //αποθηκεύουμε το watch descriptor
			sync_dir(active_workers, worker_limit, head, 0, "ALL");

			char answer[150];
			sprintf(answer, "Command sync %s/\n", current->source_dir);
			write(fss_out_fd, answer, sizeof(answer));
			return 0;
		}
		current = current->next;
	}
	
	//Αν δεν βρούμε τον κατάλογο που ζητάμε
	printf("Source Directory %s not found in the system\n", directory);
	char answer[150];
	sprintf(answer, "error in sync: %s/ not found\n", current->source_dir);
	write(fss_out_fd, answer, sizeof(answer));
	return 1;
}