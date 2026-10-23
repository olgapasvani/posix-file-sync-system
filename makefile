CC = gcc
CFLAGS = -Wall -g

#targets
all: fss_manager fss_console fss_worker

fss_console: fss_console.c manager_functions.c manager_functions.h
	$(CC) $(CFLAGS) -o fss_console fss_console.c manager_functions.c

# Manager build (χρησιμοποιεί manager_functions)
fss_manager: fss_manager.c manager_functions.c manager_functions.h
	$(CC) $(CFLAGS) -o fss_manager fss_manager.c manager_functions.c

# Worker build
fss_worker: fss_worker.c
	$(CC) $(CFLAGS) -o fss_worker fss_worker.c

# Clean up
clean:
	rm -f fss_console fss_manager fss_worker *.o