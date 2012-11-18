#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, const char *argv[])
{
	int fd = 0;
	int i = 0;
	int j  = 0;
	int ret = 0;

	if (argc != 2) {
		printf("argument error: %d\n", argc);
		return -1;
	}

	fd = open(argv[1], O_APPEND | O_CREAT | O_TRUNC | O_RDWR );
	if (fd < 0) {
		printf("failed to open file %s\n", argv[1]);
		return -1;
	}

	for(i = 0; i < 1000000; i++) {
		for(j = 0; j < 1048576; j++) {
			ret = write(fd, "1\0", 1);
			if (ret != 1) {
				printf("failed to write file %s: ret = %d\n", argv[1], ret);
			}
		}
		printf("appended for %d * %d times\n", (i + 1), 1048576);
	}

	return 0;
}
