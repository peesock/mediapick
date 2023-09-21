#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <unistd.h>

// take text, split it into an array of strings.
void deliminate_string(int fd, char*** List, size_t* Count, char delimit){
	size_t length = 64;
	size_t rows = 64;
	char** list;

	list = (char**)malloc(sizeof(char*) * rows);
	for (int ii=0; ii < rows; ii++){
		list[ii] = (char*)malloc(sizeof(char) * length);
	}

	size_t newlength = length;
	int b;
	char c;
	size_t i=0, ii=0;
	while (read(fd, &c, 1) > 0){

		if (ii+1 >= rows){
			rows += 64;
			// printf("del: increasing ii count to %lu\n", count);
			list = (char**)realloc(list, sizeof(char*) * rows);
			for (int n = ii+1; n <= rows; n++){
				list[n] = (char*)malloc(sizeof(char) * length);
			}
		}

		// printf("ii: %lu, i: %lu, c: %c\n", ii, i, c);
		// printf("%lu\n", newlength);
		if (i+1 >= newlength){
			newlength += 64;
			// printf("del: increasing i count to %lu\n", newlength);
			list[ii] = (char*)realloc(list[ii], sizeof(char) * newlength);
		}

		if (c == delimit){
			if (ii == 0 && i == 0)
				continue;
			while (c == delimit && (b = read(fd, &c, 1)) > 0){ }
			list[ii][i] = '\0';
			if (b == 0)
				continue;
			i=1; ii++;
			list[ii][0] = c;
			newlength = length;
			continue;
		}

		list[ii][i] = c;
		i++;
	}

	list[ii][i] = '\0';
	*List = list;
	if (ii == 0 && i == 0)
		*Count = 0;
	else
		*Count = (ii+1);
	return;
}

// basename implementation.
char* basename(char *filename){
	int inlen = strlen(filename);
	int mark = inlen;
	while ( filename[mark] != '/' ){
		if (mark == 0)
			return filename;
		mark--;
	}
	char* out = (char*)malloc(inlen - mark);
	for (int i=0; mark < inlen ;i++, mark++){
		out[i] = filename[mark+1];
	}
	return out;
}

// 2x faster version of realpath, no symlinks.
char* fullpath(char* filename, char* cwd){
	int len;
	char* buffer;
	if (filename[0] != '/'){
		// combine cwd with relative path
		len = strlen(cwd) + strlen(filename) + 1;
		buffer = malloc(len + 1);
		sprintf(buffer, "%s/%s", cwd, filename);
		// printf("concatted: %s\n", buffer);
	} else {
		len = strlen(filename);
		buffer = calloc(len + 1, 1);
		strcpy(buffer, filename);
	}

	// will not work for *all* cases.
	if (cwd[1] == '\0')
		return buffer;

	// look for '.' and '/' chars and manipulate
	char c;
	for (int i = 0; i < len; i++){
		// printf("%c\n", buffer[i]);

		// remove duplicate '/'
		if (buffer[i] == '/' && buffer[i+1] == '/'){
			// printf("removing 1 slash.\n");
			len -= 1;
			for (int ii=i+1; ii <= len; ii++){
				// printf("hi\n");
				buffer[ii] = buffer[ii+1];
			}
			i--;
			continue;
		}

		if (buffer[i] == '/'){
			i++;
			// resolve ".." directories
			if (buffer[i] == '.'){
				// printf("i1: %c, i2: %c\n", buffer[i+1], buffer[i+2]);
				if (buffer[i+1] == '.' && ((c=buffer[i+2]) == '/' || c == '\0')){
					// printf("removing 2 dots.\n");
					// ii must start at the previous directory
					int p=i-2; // char before the last '/'
					for (; buffer[p] != '/'; p--){  }
					int delen = 3 + (i - p - 1);
					len -= delen;
					int ii=p;
					for (; ii <= len; ii++){
						// printf("Moving ii+delen=%d=%c to ii=%d=%c\n", ii + delen, buffer[ii + delen], ii, buffer[ii]);
						buffer[ii] = buffer[ii + delen];
					}
					i -= delen-1;
					continue;
				}

				// resolve "." directories
				if ((c=buffer[i+1]) == '/' || c == '\0'){
					// printf("removing 1 dot.\n");
					int delen = 2;
					len -= delen;
					for (int ii=i-1; ii <= len; ii++){
						// printf("Moving ii+delen=%d=%c to ii=%d=%c\n", ii + delen, buffer[ii + delen], ii, buffer[ii]);
						buffer[ii] = buffer[ii + delen];
					}
					i -= delen-1;
					continue;
				}
			}
		}
	}

	if (len == 0){
		buffer[0] = '/'; buffer[1] = '\0';
		len++;
	}
	else if (buffer[len-1] == '/'){
		buffer[len-1] = '\0';
	}
	return buffer;
}

// return a hex SHA1 hash
char* hash(char* string){
	const int HASHLEN = 20;
	size_t inlen = strlen(string);
	unsigned char* hashbuf = (unsigned char*)malloc(HASHLEN + 1);
	char* output = (char*)malloc(HASHLEN*2 + 1); // to store hex
	output[0] = '\0';

	SHA1((unsigned char*)string, inlen, hashbuf);

	for (int i = 0; i < HASHLEN; i++) {
		sprintf(output, "%s%x", output, hashbuf[i]);
		// btw glibc yells at me for sprintfing a var into itself. Oh well!
	}
	free(hashbuf);
	return output;
}

char escaper(char* escape){
	if (escape[0] == '\\' && escape[1] != '\0' && escape[2] == '\0'){
		char c1 = escape[1];
		switch (c1) {
			case '0':
				return '\0';
			case 'n':
				return '\n';
			case 'r':
				return '\r';
			case 't':
				return '\t';
			case 'v':
				return '\v';
		}
	}
	return escape[0];
}
