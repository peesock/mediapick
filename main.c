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

int fd1[2];
int fd2[2];
char** hashlist;
char** filelist;
size_t filecount;
const char* mimeType;
magic_t magicCookie;
_Bool isMagical;
_Bool optCache = 1;
_Bool optView = 1;
_Bool optVerb = 0;
char* cachedir;
char* fileHash;
char* fileCache;
char* fileId;
char* cwd;
video_thumbnailer* thumb;
struct stat sb;

int looper(size_t i);

int main(int argc, char **argv){
	// set default cachedir etc
	const int PATH_MAX = pathconf(".", _PC_PATH_MAX);
	cachedir = (char*)malloc(PATH_MAX);
	cachedir[0] = '\0';
	sprintf(cachedir, "%s/.cache/%s", getenv("HOME"), basename(argv[0]));
	cwd = alloca(PATH_MAX);
	if ((getcwd(cwd, PATH_MAX)) == NULL) {
		printf("%s\n", cwd);
		perror("getcwd() error");
		return 1;
	}

	char delimIn = '\n';
	char delimOut = '\n';

	for (int opt;(opt = getopt(argc, argv, "n:d:i:o:0D:Cvh")) != -1;) {
		switch (opt) {
			case 'n':
				switch (optarg[0]){
					case 'v': optView = 0; break;
					case 'c': optCache = 0; break;
					default: usage(); return 2;
				}
				break;
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
			case 'D':
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
			case 'v':
				optVerb = 1;
				break;
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

	deliminate_string(STDIN_FILENO, &filelist, &filecount, delimIn);

	if (optView ==1){
		// initiating piper baghdad
		if (pipe(fd1) == -1)
			return 1;
		if (pipe(fd2) == -1)
			return 1;

		// fd1 sends files to nsxiv, fd2 takes marked files from nsxiv
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
			execlp("nsxiv", "nsxiv", "-iot0q", NULL);
		}
		close(fd1[0]);
		close(fd2[1]);
	}

	// vars to be used by looper
	hashlist = (char**)calloc(filecount, 21);
	fileId = (char*)malloc(PATH_MAX + 20*4 + 5);
	fileCache = (char*)malloc(PATH_MAX);

	thumb = video_thumbnailer_create();
	video_thumbnailer_set_size(thumb, 0, 0);
	thumb->thumbnail_image_type = Jpeg;
	thumb->overlay_film_strip = 1;

	isMagical = 0;

	// go through items in filelist and hash, check if they exist in cache, and cache and/or open in nsxiv.
	int Exit;
	for (size_t i=0; i < filecount; i++){
		Exit = looper(i);
		if (Exit == 1)
			continue;
		if (Exit == 2)
			return 1;

		// set up hashlist for indexing later, send cachefiles to nsxiv
		if (optView == 1){
			hashlist[i] = fileHash;
			write(fd1[1], fileCache, strlen(fileCache) + 1);
		}
	}

	if (optView == 1){
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
	}

	return 0;
}

int looper(size_t i){
	char* fullPath = fullpath(filelist[i], cwd);
	if (stat(filelist[i], &sb) != 0){
		fprintf(stderr, "Error reading %s\n", fullPath);
		return 1;
	}
	// produce unique id
	sprintf(fileId, "%s-%lu-%lu-%lu", fullPath, sb.st_ino, sb.st_ctim.tv_nsec, sb.st_size);
	// produce shrimple hash of id
	fileHash = hash(fileId);
	// create path to cache file
	sprintf(fileCache, "%s/%s", cachedir, fileHash);
	// printf("file[i]: %s\nfullPath: %s\ncachePath: %s\n", filelist[i], fullPath, fileCache);

	if (optCache == 1){
		// generate cache if needed
		if (access(fileCache, F_OK) != 0){
			if (optVerb == 1)
				fprintf(stderr, "Caching '%s'\n", filelist[i]);

			// initialize magic.h and thumbnailer
			if (isMagical == 0){
				// printf("making magic...\n");
				if ((magicCookie = magic_open(MAGIC_MIME_TYPE | MAGIC_SYMLINK | MAGIC_NO_CHECK_TAR)) == NULL){
					fprintf(stderr, "Failed to open magic cookie.\n");
					return 2;
				}
				if (magic_load(magicCookie, NULL) != 0){
					magic_close(magicCookie);
					fprintf(stderr, "Failed to load magic database.\n");
					return 2;
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
				fprintf(stderr, "Could not cache '%s'\n", fullPath);
				return 1;
			}
		}
	}
	free(fullPath);
	return 0;
}

void usage(){
	fputs(("\
Usage: mediapick [options]\n\
\n\
Options:\n\
-D <cachedir>   set cache folder (default $HOME/.cache/$0)\n\
-d <d>          set input and output deliminator (default \\n)\n\
-i <d>          set input deliminator\n\
-o <d>          set output deliminator\n\
-n <c|v>        skip caching or skip image viewing\n\
-C              prompt to delete cachedir\n\
-v              verbose\n\
-h              print help\n\
"), stderr);
	return;
}
