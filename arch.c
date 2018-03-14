#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define PORTION_SIZE 1024
#define PATH_SIZE 256

#ifndef EXIT_IF_ERROR
#define EXIT_IF_ERROR(check, error, info)\
	do { \
		if (check == error) { \
			perror(info);\
			exit(errno);\
		} \
	} while (0)
#endif
#ifndef IS_ARCH_FLAG
#define IS_ARCH_FLAG(str) (!strcmp("-p", str) || !strcmp("-n", str))
#endif

int temparch;
char temparchivepath[PATH_SIZE], cwd[PATH_SIZE];

int whatisthis(char *path, char *full)
{
	struct stat temp;

	if (stat(path, &temp) == -1) {
		if (full == NULL)
			return 2;
		strcpy(full, cwd);
		if (path[0] != '/')
			strcat(full, "/");
		strncat(full, path, PATH_SIZE-1-strlen(cwd));
		if (stat(full, &temp) == -1)
			return 2;	//неизвестно что
	} else
		if (full != NULL)
			strcpy(full, path);
	if (S_ISDIR(temp.st_mode))
		return 1;	//директория
	if (S_ISREG(temp.st_mode))
		return 0;	//файл
	return 2;
}

void pack(int descr, char *name)
{
	char file[PORTION_SIZE], *shortname;
	long int i;
	struct stat temp;

	lseek(descr, 0, 0);
	shortname = strrchr(name, '/');
	if (shortname == NULL)
		shortname = name; //путь из раб. каталога
	i = strlen(shortname) + 1;	//записать с нулем
	write(temparch, shortname, i);
	write(temparch, "\0", 1); //0 - файл, { - каталог
	fstat(descr, &temp);
	write(temparch, &(temp.st_size), 8); //размер файла
	for (; i = read(descr, file, PORTION_SIZE);) {	//сам файл
		write(temparch, file, i);
	}
	close(descr);
}
int packdir(char *path)
{
	DIR *dir;
	struct dirent *temp;
	char temppath[PATH_SIZE], *name;
	long int len = strlen(path), namelen;	//длина пути

	dir = opendir(path);
	if (dir == NULL) {
		perror(path);
		return -1;
	}
	temp = readdir(dir); //первый файл в дир.
	name = strrchr(path, '/'); //узнаем имя каталога
	if (name == NULL)
		name = path;
	else
		name++;
	namelen = len + 1 - (name - path);
	write(temparch, name, namelen);	//записываем имя
	write(temparch, "{", 1); //0 - файл, { - каталог
	strcpy(temppath, path);
	temppath[len] = '/';
	len++;
	temppath[len] = 0;
	for (; temp != NULL;) {
		if (strcmp(temp->d_name, ".") && strcmp(temp->d_name, "..")) {
			strcat(temppath, temp->d_name);//приклеим имя
			if (strcmp(temparchivepath, temppath)) {
				if (!whatisthis(temppath, NULL)) {
					namelen = open(temppath, O_RDONLY);
					if (namelen == -1)
						perror(temppath);
					else {
						write(temparch, "\0", 1);
						pack(namelen, temp->d_name);
					}
				} else {
					write(temparch, "\0", 1);
					namelen = packdir(temppath);
					if (namelen == -1)
						lseek(temparch, -1, 1);
				}
			}
		}
		temp = readdir(dir);	//следующий файл
		temppath[len] = 0;	//отрезаем имя
	}
	write(temparch, "}", 1); //дир. закончилась
	closedir(dir);
	return 0;
}

int unpackfile(char *path, int f)
{
	char file[PORTION_SIZE];
	long int templen, len, descr;

	descr = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0664);
	if (descr == -1) {
		perror(path);
		return descr;
	}
	read(f, &len, 8);	//читаем длину
	for (; len > 0;) { //переписываем
		templen = len -= PORTION_SIZE;
		if (templen > 0)
			templen = PORTION_SIZE;
		else
			templen += PORTION_SIZE;
		read(f, file, templen);
		write(descr, file, templen);
	}
	close(descr);
	return 0;
}

int unpackunit(int f, char *path);

int unpackdir(char *path, int f)
{
	char check;

	check = mkdir(path, 0775); //создаем директорию
	if (check) {
		if (errno != EEXIST) {
			perror(path);
			return check;
		}
	}
	read(f, &check, 1); //есть ли содержимое еще
	for (; !check; read(f, &check, 1))
		unpackunit(f, path); //следующая часть
	return 0;
}

int unpackunit(int f, char *path)
{
	char temppath[PATH_SIZE];
	int check, len;

	len = strlen(path); //длина
	strcpy(temppath, path);
	if (len) {
		temppath[len] = '/';
	len++;
	}
	check = read(f, temppath + len, 1);
	if (check <= 0) //проверка конца файла
		return check;
	for (; temppath[len];) { //приклеим имя
		len++;
		read(f, temppath + len, 1);
	}
	read(f, temppath + len, 1);
	if (temppath[len] == '{') { //файл или дир.
		temppath[len] = 0;
		unpackdir(temppath, f);
	} else
		unpackfile(temppath, f);
	return 0;
}

void unpack(int f, char *path)
{ for (; unpackunit(f, path) > 0;);
}

int create(int *num, char *path, char *name)
{
	temparchivepath[0] = 0;
	if (path[0]) { //начинаем собирать путь
		strcpy(temparchivepath, path);
		strcat(temparchivepath, "/");
	}
	if (!name[0]) { //имя по умолчанию
		name[0] = 'a';
		name[1] = '0' + *num;
		name[2] = 0;
	}
	strcat(name, ".daf"); //dream archive file
	strncat(temparchivepath, name, PATH_SIZE - 1 - strlen(temparchivepath));
	temparch = open(temparchivepath, O_CREAT|O_WRONLY|O_TRUNC, 0664);
	if (temparch == -1) {
		perror(temparchivepath);
		return -1;
	}
	(*num)++;
	write(temparch, "ARCHIVE", 7); //чтобы отличать
	return 0;
}

void tonextflag(int *j, char **argv, int argc)
{
	int i;

	for (; i < (argc - 1); i++)
		if (!strcmp("-p", argv[i+1]) || !strcmp("-n", argv[i+1]))
			break;
	*j = i;
}

int main(int argc, char *argv[])
{
	int i = 1, flag, tempf, num = 0;
	char name[PATH_SIZE], path[PATH_SIZE], full[PATH_SIZE];
	char check[7];
	char *twoflagserr = "Warning, two flags are entered one after another";
	char *patherr = "Warning, the path to the file was entered";
	char *bad = "Warning, something bad was entered";

	name[0] = 0;
	temparch = 0;
	path[0] = 0;
	getcwd(cwd, PATH_SIZE - 1);

	if (argc == 1) {
		printf("No arguments entered. There is nothing to do\n");
		exit(0);
	}
	for (; i < argc; i++) {
		if (!strcmp("-p", argv[i])) { //флаг пути
			if (temparch) {	//закрыть старый архив
				printf("Archive %s is created\n", name);
				close(temparch);
				temparch = 0;
				name[0] = 0;
				path[0] = 0;
			}
			if (i == (argc-1))
				printf("Warning, the last argument is flag\n");
			else
				if (IS_ARCH_FLAG(argv[i+1]))
					printf("%s\n", twoflagserr);
			else {
				switch (whatisthis(argv[i+1], full)) {
				//что после флага
				case 0: //если файл
					printf("%s\n", patherr);
					i--;
					break;
				case 1: //если каталог
					strcpy(path, full);
					break;
				case 2://проверим полный путь
					printf("%s\n", bad);
					break;
				}
				i++;
			}
		} else if (!strcmp("-n", argv[i]))	{ //флаг имени
			if (temparch) { //закрыть старый архив
				close(temparch);
				temparch = 0;
				name[0] = 0;
				path[0] = 0;
			}
			if (i == (argc-1))
				printf("Warning, the last argument is flag\n");
			else if (IS_ARCH_FLAG(argv[i+1]))
				printf("%s\n", twoflagserr);
			else if (strchr(argv[i+1], '/'))
				printf("The file name can not contain /\n");
			else {
				strncpy(name, argv[i+1], PATH_SIZE - 1);
				i++;
			}
		} else {
			flag = whatisthis(argv[i], full);
			switch (flag) {
			case 2:
				printf("%s\n", bad);
				break;
			case 0:
				tempf = open(full, O_RDONLY);
				if (tempf != -1) {
					read(tempf, check, 7);
					if (!strncmp("ARCHIVE", check, 7)) {
					//на входе архив
						unpack(tempf, path);
						//распаковать
						break;
					}
				} else {
					perror(full);
					break;
				}
			case 1:
				if (!temparch)
					if (create(&num, path, name) == -1) {
						tonextflag(&i, argv, argc);
							flag = 2;
					}
				if (flag == 0) {
					pack(tempf, full);
					break;
				}
				if (flag == 1)
					packdir(full);
			}
		}
	}
	if (temparch > 0) {
		printf("Archive %s is created\n", name);
		close(temparch);
	}
	exit(0);
}

