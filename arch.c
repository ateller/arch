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
#define PATH_SIZE 512
#define NAME_SIZE 256

#ifndef IF_ERR
#define IF_ERR(check, error, info)\
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

int temp_ar;
char archpath[PATH_SIZE], cwd[PATH_SIZE];

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

	IF_ERR(lseek(descr, 0, 0), -1, name);
	shortname = strrchr(name, '/');
	if (shortname == NULL)
		shortname = name; //путь из раб. каталога
	i = strlen(shortname) + 1;	//записать с нулем
	IF_ERR(write(temp_ar, shortname, i), -1, name);
	IF_ERR(write(temp_ar, "\0", 1), -1, name);
	IF_ERR(fstat(descr, &temp), -1, name);
	IF_ERR(write(temp_ar, &(temp.st_size), 8), -1, name);
	for (; i = read(descr, file, PORTION_SIZE);) {	//файл
		IF_ERR(i, -1, name);
		IF_ERR(write(temp_ar, file, i), -1, name);
	}
	IF_ERR(close(descr), -1, name);
}

int filecmp(char *path1, char *path2)
{
	char buf[PATH_SIZE];
	int len1, len2, check;

	check = strcmp(path1, path2);
	if (!check)
		return check;
	len1 = strlen(path1);
	len2 = strlen(path2);
	if (len1 == len2)
		return 1;
	check = readlink("/proc/self/exe", buf, PATH_SIZE - 1);
	IF_ERR(check, -1, "/proc/self/exe");
	buf[check] = 0;
	check = 1 + strrchr(buf, '/') - buf;
	buf[check] = 0;
	if (len1 < len2) {
		strncat(buf, path1, PATH_SIZE - check);
		check = strcmp(path2, buf);
	} else {
		strncat(buf, path2, PATH_SIZE - check);
		check = strcmp(path1, buf);
	}
	if (!check)
		return check;
	strcpy(buf, cwd);
	if (len1 < len2) {
		if (path1[0] != '/')
			strcat(buf, "/");
		check = strcmp(path2, strncat(buf, path1, PATH_SIZE - check));
	} else {
		if (path2[0] != '/')
			strcat(buf, "/");
		check = strcmp(path1, strncat(buf, path2, PATH_SIZE - check));
	}
	return check;
}

int packdir(char *path)
{
	DIR *dir;
	struct dirent *temp;
	char check, temppath[PATH_SIZE], *name;
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
	IF_ERR(write(temp_ar, name, namelen), -1, path);
	IF_ERR(write(temp_ar, "{", 1), -1, path);
	strcpy(temppath, path);
	temppath[len] = '/';
	len++;
	temppath[len] = 0;
	for (; temp != NULL;) {
		if (strcmp(temp->d_name, ".") && strcmp(temp->d_name, "..")) {
			strcat(temppath, temp->d_name);//приклеим имя
			if (!whatisthis(temppath, NULL)) {
				if (filecmp(temppath, archpath)) {
					namelen = open(temppath, O_RDONLY);
					if (namelen == -1)
						perror(temppath);
					else {
						check = write(temp_ar, "\0", 1);
						IF_ERR(check, -1, archpath);
						pack(namelen, temp->d_name);
					}
				}
			} else {
				check = write(temp_ar, "\0", 1);
				IF_ERR(check, -1, archpath);
				namelen = packdir(temppath);
				if (namelen == -1) {
					check = lseek(temp_ar, -1, 1);
					IF_ERR(check, -1, archpath);
				}
			}
		}
		temp = readdir(dir);	//следующий файл
		temppath[len] = 0;	//отрезаем имя
	}
	IF_ERR(write(temp_ar, "}", 1), -1, archpath); //закончилась
	IF_ERR(closedir(dir), -1, path);
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
	IF_ERR(read(f, &len, 8), -1, path);//читаем длину
	for (; len > 0;) { //переписываем
		templen = len -= PORTION_SIZE;
		if (templen > 0)
			templen = PORTION_SIZE;
		else
			templen += PORTION_SIZE;
		IF_ERR(read(f, file, templen), -1, path);
		IF_ERR(write(descr, file, templen), -1, path);
	}
	IF_ERR(close(descr), -1, path);
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
	IF_ERR(read(f, &check, 1), -1, path);//есть ли еще
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
	IF_ERR(check, -1, path);
	if (check == 0) //проверка конца файла
		return check;
	for (; temppath[len];) { //приклеим имя
		len++;
		IF_ERR(read(f, temppath + len, 1), -1, path);
	}
	IF_ERR(read(f, temppath + len, 1), -1, path);
	if (temppath[len] == '{') { //файл или дир.
		temppath[len] = 0;
		unpackdir(temppath, f);
	} else
		unpackfile(temppath, f);
	return 1;
}

void unpack(int f, char *path)
{ for (; unpackunit(f, path) > 0;);
}

int create(int *num, char *path, char *name)
{
	archpath[0] = 0;
	if (path[0]) { //начинаем собирать путь
		strcpy(archpath, path);
		strcat(archpath, "/");
	}
	if (!name[0]) { //имя по умолчанию
		name[0] = 'a';
		name[1] = '0' + *num;
		name[2] = 0;
	}
	strcat(name, ".daf"); //dream archive file
	strncat(archpath, name, PATH_SIZE - 1 - strlen(archpath));
	temp_ar = open(archpath, O_CREAT|O_WRONLY|O_TRUNC, 0664);
	if (temp_ar == -1) {
		perror(archpath);
		return -1;
	}
	(*num)++;
	IF_ERR(write(temp_ar, "ARCHIVE", 7), -1, archpath); //отличие
	return 0;
}

void tonextflag(int *j, char **argv, int argc)
{
	int i = *j;

	for (; i < (argc - 1); i++)
		if (!strcmp("-p", argv[i+1]) || !strcmp("-n", argv[i+1]))
			break;
	*j = i;
}

//Телятников, группа 3О-309Б

int main(int argc, char *argv[])
{
	int i = 1, flag, tempf, num = 0;
	char name[NAME_SIZE], path[PATH_SIZE], full[PATH_SIZE];
	char check[7];
	char *twoflagserr = "Warning, two flags are entered one after another";
	char *patherr = "Warning, the path to the file was entered";
	char *bad = "Warning, something bad was entered:";

	name[0] = 0;
	temp_ar = 0;
	path[0] = 0;
	getcwd(cwd, PATH_SIZE - 1);

	if (argc == 1) {
		printf("No arguments entered. There is nothing to do\n");
		exit(0);
	}
	for (; i < argc; i++) {
		if (!strcmp("-p", argv[i])) { //флаг пути
			if (temp_ar) {	//закрыть старый архив
				IF_ERR(close(temp_ar), -1, name);
				printf("Archive %s is created\n", name);
				temp_ar = 0;
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
					printf("%s %s\n", bad, argv[i+1]);
					break;
				}
				i++;
			}
		} else if (!strcmp("-n", argv[i]))	{ //флаг имени
			if (temp_ar) { //закрыть старый архив
				IF_ERR(close(temp_ar), -1, name);
				printf("Archive %s is created\n", name);
				temp_ar = 0;
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
				printf("%s %s\n", bad, argv[i]);
				break;
			case 0:
				tempf = open(full, O_RDONLY);
				if (tempf != -1) {
					IF_ERR(read(tempf, check, 7), -1, full);
					if (!strncmp("ARCHIVE", check, 7)) {
					//на входе архив
						unpack(tempf, path);
						IF_ERR(close(tempf), -1, name);
						printf("%s unpacked\n", full);
						//распаковать
						break;
					}
				} else {
					perror(full);
					break;
				}
			case 1:
				if (!temp_ar)
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
	if (temp_ar > 0) {
		IF_ERR(close(temp_ar), -1, name);
		printf("Archive %s was created\n", name);
	}
	exit(0);
}

