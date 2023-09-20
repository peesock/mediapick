#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <libffmpegthumbnailer/videothumbnailerc.h>
#include <magic.h>

#include "utils.c"

void usage();

int main(int argc, char **argv){
	// set default cachedir etc
	const int PATH_MAX = pathconf(".", _PC_PATH_MAX);
	char* cachedir = (char*)malloc(PATH_MAX);
	cachedir[0] = '\0';
	sprintf(cachedir, "%s/.cache/%s", getenv("HOME"), basename(argv[0]));
	char cwd[PATH_MAX];
	if ((getcwd(cwd, sizeof(cwd))) == NULL) {
		perror("getcwd() error");
		return 1;
	}

	char delimIn = '\n';
	char delimOut = '\n';

	for (int opt;(opt = getopt(argc, argv, "d:i:o:0c:Ch")) != -1;) {
		switch (opt) {
			case 'd':
				delimIn = escaper(optarg);
				delimOut = escaper(optarg);
				break;
			case 'i':
				delimIn = escaper(optarg);
				break;
			case 'o':
				delimOut = escaper(optarg);
				break;
			case '0':
				delimIn = '\0';
				delimOut = '\0';
				break;
			case 'c':
				stpcpy(cachedir, fullpath(optarg, cwd));
				break;
			case 'C':
				fprintf(stderr, "remove '%s'? (y/N): ", cachedir);
				if (getchar() == 'y'){
					char tmp[PATH_MAX+7];
					sprintf(tmp, "rm -rf %s", cachedir);
					system(tmp);
				}
				return 0;
			case 'h':
				usage();
				return 0;
			default:
				usage();
				return 2;
		}
	}
	if (access(cachedir, F_OK) != 0){
		fprintf(stderr, "Cache '%s' not found, creating...\n", cachedir);
		mkdir(cachedir, 0777);
	}

	char** filelist;
	size_t filecount;

	deliminate_string(STDIN_FILENO, &filelist, &filecount, delimIn);
	// for (size_t i=0; i < filecount; i++){
	// 	printf("%s\n", filelist[i]); }

	char** hashlist = (char**)calloc(filecount, 21);
	char* fullPath = (char*)malloc(PATH_MAX);
	char* fileId = (char*)malloc(PATH_MAX + 20*4 + 5);
	char* fileCache = (char*)malloc(PATH_MAX);
	struct stat sb;

	video_thumbnailer* thumb = video_thumbnailer_create();
	video_thumbnailer_set_size(thumb, 0, 0);
	thumb->thumbnail_image_type = Jpeg;
	thumb->overlay_film_strip = 1;

	magic_t magicCookie;
	_Bool isMagical = 0;
	const char* mimeType;

	// initiating piper baghdad
	int fd1[2];
	int fd2[2];
	if (pipe(fd1) == -1)
		return 1;
	if (pipe(fd2) == -1)
		return 1;

	int pid;
	if ((pid = fork()) < 0)
		return 1;
	if (pid == 0){
		dup2(fd1[0], STDIN_FILENO);
		close(fd1[0]);
		close(fd1[1]);

		dup2(fd2[1], STDOUT_FILENO);
		close(fd2[0]);
		close(fd2[1]);
		execlp("nsxiv", "nsxiv", "-iot0", NULL);
		// execlp("xargs", "xargs", "-0", NULL);
		// execlp("cat", "cat", NULL);
	}
	close(fd1[0]);
	close(fd2[1]);


	for (size_t i=0; i < filecount; i++){
		fullPath = fullpath(filelist[i], cwd);
		if (stat(filelist[i], &sb) != 0){
			fprintf(stderr, "Error with %s\n", fullPath);
			continue;
		}
		// produce unique id
		sprintf(fileId, "%s-%lu-%lu-%lu", fullPath, sb.st_ino, sb.st_ctim.tv_nsec, sb.st_size);
		// produce shrimple hash of id
		char* fileHash = hash(fileId);
		// create path to cache file
		sprintf(fileCache, "%s/%s", cachedir, fileHash);
		// printf("file[i]: %s\nfullPath: %s\ncachePath: %s\n", filelist[i], fullPath, fileCache);

		// generate cache if needed
		if (access(fileCache, F_OK) != 0){
			// printf("cache does not exist\n");

			// initialize magic.h and thumbnailer
			if (isMagical == 0){
				// printf("making magic...\n");
				if ((magicCookie = magic_open(MAGIC_MIME_TYPE | MAGIC_SYMLINK | MAGIC_NO_CHECK_TAR)) == NULL){
					fprintf(stderr, "Failed to open magic cookie.\n");
					return 1;
				}
				if (magic_load(magicCookie, NULL) != 0){
					magic_close(magicCookie);
					fprintf(stderr, "Failed to load magic database.\n");
					return 1;
				}
				isMagical = 1;
			}

			// check mimetype, create thumbnails
			mimeType = magic_file(magicCookie, filelist[i]);
			char* mimeShort = strtok((char*)mimeType, "/");
			// printf("mime-simple: %s\n", mimeSimple);
			if (strcmp(mimeShort, "video") == 0){
				// printf("Making video...\n");
				video_thumbnailer_generate_thumbnail_to_file(thumb, filelist[i], fileCache);
			}
			else if (strcmp(mimeShort, "image") == 0){
				// printf("Symlinking...\n");
				symlink(fullPath, fileCache);
			} else {
				fprintf(stderr, "Could not open '%s'\n", filelist[i]);
				continue;
			}
		}

		hashlist[i] = fileHash;
		write(fd1[1], fileCache, strlen(fileCache) + 1);

		free(fullPath);
	}
	// now manage the selected files given by sxiv
	close(fd1[1]);
	size_t selectnum;
	char** selectionlist;
	deliminate_string(fd2[0], &selectionlist, &selectnum, '\0');
	close(fd2[0]);
	close(fd2[1]);
	// for (int i=0; i < filecount; i++) {
	// fprintf(stderr, "guh: %s\n", hashlist[i]);
	// }
	char* str;
	// for every item in selected,
	for (size_t i=0, ii=0; i < selectnum; i++) {
		str = basename(selectionlist[i]);
		// fprintf(stderr, "str: %s\n", str);
		// fprintf(stderr, "hash: %s\n", str);
		// compare against every item in hash list
		for (; ii < filecount; ii++){
			if (hashlist[ii]){
				if (strcmp(hashlist[ii], str) == 0){
					break;
				}
			}
		}
		printf("%s%c", fullpath(filelist[ii], cwd), delimOut);
		ii=0;
	}

	// free(filelist);
	// video_thumbnailer_destroy(thumb);
	// magic_close(magicCookie); // causes segfault :guh:
	return 0;
}
void usage(){
	fputs(("\
Usage: mediapick [options]\n\
\n\
Options:\n\
  -d <d>          set input and output deliminator (default \\n)\n\
  -i <d>          set input deliminator\n\
  -o <d>          set output deliminator\n\
  -c <cachedir>   set cache folder (default $HOME/.cache/$0)\n\
  -C              prompt to delete cachedir\n\
  -h              print help\n\
"), stderr);
	return;
}
