#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int temparch;
char temparchivepath[256];

int whatisthis(char *path)
{
	struct stat temp;

	if (stat(path, &temp) == -1)
		return 2;	//хз что
	if (S_ISDIR(temp.st_mode))
		return 1;	//директория
	if (S_ISREG(temp.st_mode))
		return 0;	//файл
}

int tryfullpath(char *cwd, char *path, char *full)
{
	strcpy(full, cwd);
	if (path[0] != '/')
		strcat(full, "/");
	strncat(full, path, 255-strlen(cwd));
	return whatisthis(full);
}

void pack(int descr, char *name)
{
	char file[1024], *shortname;
	long int i;
	struct stat temp;

	lseek(descr, 0, 0);
	shortname = strrchr(name, '/');
	if (shortname == NULL)
		shortname = name;		//если файл в одной директории с программой
	i = strlen(shortname) + 1;		//чтобы записать с завершающим нулем
	write(temparch, shortname, i);
	write(temparch, "\0", 1);	//потом по этому байту будем отличать, имя это файла или директории
	fstat(descr, &temp);
	write(temparch, &(temp.st_size), 8);	//запишем размер файла
	for (; i = read(descr, file, 1024);) {	// и порциями сам файл
		write(temparch, file, i);
	}
	close(descr);
}
void packdir(char *path)
{
	printf("pack way %s\n", path);
	DIR *dir = opendir(path);
	struct dirent *temp = readdir(dir);  //откроем первый файл в директории
	char temppath[256], *name;
	long int len = strlen(path), namelen;	//узнаем длину пути

	name = strrchr(path, '/'); //и начало последней части пути - имя директории, в которой находимся
	if (name == NULL)
		name = path;
	else
		name++;
	namelen = len + 1 - (name - path);
	write(temparch, name, namelen);	//записываем имя
	write(temparch, "{", 1); //указывает, что сейчас будет содержимое директории
	strcpy(temppath, path);
	temppath[len] = '/';
	len++;
	temppath[len] = 0;
	for (; temp != NULL;) {
		if (temp->d_name[0] != '.') {
			strcat(temppath, temp->d_name);
			if (strcmp(temparchivepath, temppath)) {
				write(temparch, "\0", 1);		//с 0 начинается каждый файл в директории
				if (!whatisthis(temppath))
					pack(open(temppath, O_RDONLY), temp->d_name);
				else
					packdir(temppath);
			}
		}
		temp = readdir(dir);	//открываем новый файл
		temppath[len] = 0;	//отрезаем имя
	}
	write(temparch, "}", 1); //содержимое директории закончилось
	closedir(dir);
}

void unpackfile(char *path, int f)
{
	char file[1024];
	long int i, len, descr;

	descr = open(path, 0664);
	read(f, &len, 8);
	for (; len > 0;) {
		i = len -= 1024;
		if (i > 0)
			i = 1024;
		else
			i += 1024;
		read(f, file, i);
		write(descr, file, i);
	}
	close(descr);
}

void unpack(int f, char *path);
int unpackunit(int f, char *path);

void unpackdir(char *path, int f)
{
	char check;

	mkdir(path, 0775);
	read(f, &check, 1);
	for (; !check; read(f, &check, 1))
		unpackunit(f, path);
}

int unpackunit(int f, char *path)
{
	char temppath[256];
	int check, len;

	len = strlen(path);
	strcpy(temppath, path);
	temppath[len] = '/';
	len++;
	check = read(f, temppath + len, 1);
	if (check <= 0)
		return check;
	for (; temppath[len];) {
		len++;
		read(f, temppath + len, 1);
	}
	read(f, temppath + len, 1);
	if (temppath[len] == '{') {
		temppath[len] = 0;
		unpackdir(temppath, f);
	} else
		unpackfile(temppath, f);
	return 1;
}

void unpack(int f, char *path) { for (; unpackunit(f, path) > 0;); }

int createarchive(int *archivecounter, char *path, char *name)
{
	temparchivepath[0] = 0;
	if (path[0]) {
		strcpy(temparchivepath, path);
		strcat(temparchivepath, "/");
	}
	if (!name[0]) {
		name[0] = 'a';
		name[1] = 48 + *archivecounter;
		name[2] = 0;
	}
	strcat(name, ".daf");
	strncat(temparchivepath, name, 255 - strlen(temparchivepath));
	temparch = open(temparchivepath, 0664);
	(*archivecounter)++;
	write(temparch, "ARCHIVE", 7);
}

int main(int argc, char *argv[])
{
	int i = 1, flag, tempf, archivecounter;
	char tempname[256], path[256], try[256], check[7], cwd[256];

	tempname[0] = 0;
	temparch = 0;
	path[0] = 0;
	getcwd(cwd, 255);

	printf("%s %d\n", cwd, argc);
	for (; i < argc; i++) {
		printf("%d %s %s\n", i, argv[i], argv[i+1]);
		if (!strcmp("-p", argv[i])) { //флаг пути
			if (temparch) {	//закрыть текущий архив, в который идет запись, если открыт
				close(temparch);
				temparch = 0;
				tempname[0] = 0;
				path[0] = 0;
			}
			if (i == (argc-1))
				printf("Warning, the last argument is flag\n");
			else if (!strcmp("-p", argv[i+1]) || !strcmp("-n", argv[i+1]))
				printf("Warning, two flags are entered one after another\n"); //если два флага подряд, забить на первый
			else {
				switch (whatisthis(argv[i+1])) {	//проверить, что после флага
				case 0:															//если файл
					printf("Error, the path to the file was entered, the default path will be used\n");
					i--;
					break;
				case 1:										//если каталог, все норм
					strcpy(path, argv[i+1]);
					break;
				case 2:				//если непонятно, что, проверяем полный путь
					switch (tryfullpath(cwd, argv[i+1], try)) {
					case 0:
						printf("Error, the path to the file was entered, the default path will be used\n");
						i--;
						break;
					case 1:
						strcpy(path, try);
						break;
					case 2:
						printf("Error, something bad was entered, the default path will be used\n"); //если вообще непонятно что введено после флага
						break;
					}
					break;
				}
				i++;
			}
		} else if (!strcmp("-n", argv[i]))	{	//флаг имени
			if (temparch) {	//закрыть текущий архив, в который идет запись, если открыт
				close(temparch);
				temparch = 0;
				tempname[0] = 0;
				path[0] = 0;
			}
			if (i == (argc-1))
				printf("Warning, the last argument is flag\n");
			else if (!strcmp("-p", argv[i+1]) || !strcmp("-n", argv[i+1]))
				printf("Warning, two flags are entered one after another\n"); //если два флага подряд, забить на первый
			else if (strchr(argv[i+1], '/'))
				printf("The file name can not contain /\n");
			else {
				strncpy(tempname, argv[i+1], 255);
				i++;
			}
		} else {
			flag = whatisthis(argv[i]);
			switch (flag) {
			case 2:
				flag = tryfullpath(cwd, argv[i], try);
				switch (flag) {
				case 2:
					printf("Error, something bad was entered, the default path will be used\n");
					break;
				case 0:
					tempf = open(try, O_RDONLY);
					read(tempf, check, 7);
					if (!strncmp("ARCHIVE", check, 7)) {
					//если на входе архив, распаковать
						unpack(tempf, path);
						break;
					}
				case 1:
					if (!temparch)
						createarchive(&archivecounter, path, tempname);
						//создать файл для архивирования, если не создан
					if (!flag) {
						pack(tempf, argv[i]);
						break;
					}
					packdir(try);
				}
				break;
			case 0:
				tempf = open(argv[i], O_RDONLY);
				read(tempf, check, 7);
				if (!strncmp("ARCHIVE", check, 7)) {
					//если на входе архив, распаковать
					unpack(tempf, path);
					break;
				}
			case 1:
				if (!temparch)
					createarchive(&archivecounter, path, tempname);
					//создать архив, если не создан
				if (!flag)
					pack(tempf, argv[i]);
				else
					packdir(argv[i]);
			}
		}
	}
	if (temparch)
		close(temparch);
	exit(0);
}

