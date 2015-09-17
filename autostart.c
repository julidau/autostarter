#include <stdio.h>		// fscanf, printf
#include <stdlib.h>		// exit
#include <string.h>		// strcpy
#include <ctype.h>		// tolower, toupper
#include <unistd.h>		// execpv
#include <stdarg.h>		// va_list, va_start, va_end
#include <errno.h>		// errno
#include <dirent.h>		// opendir

extern char ** environ;

#define DEFAULT_DIR "~/.config/autostart/"
#define SHELL "/usr/bin/sh"

void 
die(const char * msg, ...) 
{
	va_list list;
	va_start(list, msg);
	vfprintf(stderr, msg, list);
	va_end(list);
	exit(1);
}
	
char
strcmptill(const char * str, const char * with, char ** end) 
{
	int i;
	for (i = 0; str[i] && with[i] && tolower(str[i]) == tolower(with[i]); ++i);

	// iterate other string till it ends at postion 
	if (end)
	{
		*end = str + i;
	}

	// return 1 if func exhausted with
	return with[i] == 0;
}

char 
empty(const char * str) 
{
	return str[0] == 0;
}

char name[32];
char progpath[256];
char shell;
DIR * dir;

char 
parse_entry(FILE * entry) 
{
	char terminal[6];
	char line[256];
	char * marker;
	char valid = 0;

	name[0] = 0; progpath[0] = 0; terminal[0] = 0;
	shell = 0;

	while(fscanf(entry, " %256[^\n]", line) > 0) 
	{
		if (!valid)
		{
			if (line[0] != '[')
				continue;

			valid = 1;
		} 

		if (!name[0] && strcmptill(line, "Name=", &marker)) 
		{
			strncpy(name, marker, 31);
			name[31] = 0;
		} else 
		if (!progpath[0] && strcmptill(line, "Exec=", &marker))
		{
			strncpy(progpath, marker, 255);
			progpath[255] = 0;
		} else 
		if (!terminal[0] && strcmptill(line, "Terminal=", &marker)) 
		{
			strncpy(terminal, marker, 5);
			terminal[6] = 0;
		}
	}

	if (valid) 
	{
		if (!empty(terminal) && tolower(terminal[0]) == 't') 
			shell = 1;
	}

	return valid;
}

DIR * dir = NULL;
//char ** new_environment;
//#define ENV() (new_environment?new_environment:environ)

static const char * desktopExt = ".desktop";

int 
is_desktop_file(struct dirent * entry) 
{
	if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_REG)
		return 0;

	const char * filename = entry->d_name;

	int i = 0, j = 0;
	for (; filename[i]; ++i) 
	{
		if (filename[i] == '.')
			j = 0;

		if (filename[i] == desktopExt[j])
			++j;
	}
	
	return filename[i] == 0 && desktopExt[j] == 0;
}

void 
process_desktop_file(const char * filename)
{
	FILE * f = fopen(filename, "r");

	if (!f)
		die("could not open file \"%s\"\n", filename);

	if (!parse_entry(f))
		die("file %s was invalid\n", filename);

	fclose(f);

	if (shell)
	{
		char * args[] =
		{
			SHELL,
			"-c",
			progpath,
			NULL
		};
			
		if (execvp(args[0], args) == -1)
			die("autorun: exec: %s (%d)\n", strerror(errno), errno);
	}

	char ** args = (char**)malloc(sizeof(char**));
	int argc = 0;

	char * next = NULL;
	int i = 0, j;

	while(sscanf(progpath+i, "%ms%n", &next, &j)>0) 
	{
		args = (char **)realloc((void*)args, sizeof(char**)*(argc+1));
		args[argc] = next;
		args[argc+1] = NULL;

		argc++;
		i+=j;
	}

	if (argc==0) 
		die("invalid Exec field value %s\n", progpath);

	if (execvp(args[0], args) == -1)
		die("could not exec program: %s (%d)\n", strerror(errno), errno);
}


char *
envVar(const char * varName) 
{
	char ** current = environ;
	char * rest;

	for(;current && !strcmptill(*current, varName, &rest); ++current);

	if (!current)
		return NULL;

	if (rest[0] == '=')
		++rest;

	return rest;
}

void
repair_path(char ** path)
{
	int len = strlen(*path);
	if ((*path)[len-1] == '/')
		return;

	char * new_path = malloc(len+2);
	strcpy(new_path, *path);
	new_path[len] = '/';
	new_path[len+1] = 0;

	free(*path);
	*path = new_path;
}

int 
main(int argc, char ** args) 
{
	struct dirent * current_entry;	
	char * dirpath;

	if (argc < 2) 
	{
		dirpath = strdup(DEFAULT_DIR);
	} else {
		dirpath = strdup(args[1]);
	}

	// copy environment, if needed
/*	if (argc > 2)
	{
		int n = 0;
		for (; environ[n]; ++n);
	
		new_environment = malloc(sizeof(char**)*(n+argc-1));

		memcpy(new_environment, environ, sizeof(char**)*n);
		memcpy(new_environment + n, args + 2, sizeof(char**)*(argc-2));
		new_environment[n+argc-2] = NULL;
	} else
		new_environment = NULL;

*/
	char * homeDir = envVar("HOME");
	if (*dirpath == '~' && homeDir)
	{
		char * oldpath = dirpath;
		int n1 = strlen(homeDir), n2 = strlen(oldpath)-1;

		dirpath = malloc(sizeof(char)*(n1+n2+1));
		memset(dirpath, 0, n1+n2+1);

		strncat(dirpath, homeDir, n1);
		strncat(dirpath, oldpath + 1, n2);
		free(oldpath);
	}

	repair_path(&dirpath);
	dir = opendir(dirpath);

	if (!dir)
		die("could not open dir \"%s\"\n", args[1]);

	char filename[512];

	while((current_entry = readdir(dir))) 
	{
		if (is_desktop_file(current_entry))
		{
			int pid;
			if ((pid = fork()) == 0)
			{
				strcpy(filename, dirpath);
				strcpy(filename + strlen(filename), current_entry->d_name);
				process_desktop_file(filename);
				break;
			}
			printf("cpid: %d\n", pid);
		}
	}

	free(dirpath);
	closedir(dir);
}
