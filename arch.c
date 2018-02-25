#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

char cwd[256];
int temparch = 0, archivecounter = 0;

/*void pack()
{
	int i;
	DIR *dir;
	struct dirent *temp = NULL;
	//dir = opendir(temppath);
	temp = readdir(dir);
	for (i = 0; temp!=NULL; i++)
	{
		
		//strcat (temppath, temp->d_name);
		printf("file %d %s\n", i, temp->d_name);
		temp = readdir(dir);
	}
}
*/
void pack(int descr)
{
	printf("pack file descr %d\n", descr);
}
void packdir(char* path)
{
	printf("pack way %s\n", path);
}

void unpack(int f, char* path)
{
	printf("unpack file descr %d\n", f);
}


int whatisthis(char* path)
{
	struct stat temp;
	if (stat(path, &temp) == -1) return 2; 	//хз что	
	if (S_ISDIR(temp.st_mode)) return 1;	//директория
	if (S_ISREG(temp.st_mode)) return 0;	//файл
}

int tryfullpath(char* path, char* full)
{	
	strcpy(full, cwd);
	if (path[0] != '/') strcat(full, "/");
	strncat(full, path, 255-strlen(cwd));
	return whatisthis(full);  
}

int createarchive(char* path, char* name)
{	
	char fullpath[256];
	fullpath[0] = 0;
	if(path[0])
	{
		strcpy(fullpath, path);
		strcat(fullpath, "/");
	}
	if (!name[0])
	{
		name[0] = 'a';
		name[1] = 48 + archivecounter;
		name[2] = 0;
		strcat(name, ".daf");	
	}
	strncat(fullpath, name, 255 - strlen(fullpath));
	temparch = open(fullpath, O_CREAT|O_WRONLY|O_TRUNC, S_IWUSR|S_IWGRP|S_IWOTH);
	printf("temparchivepath %s\n", fullpath);		
	archivecounter++;
	write(temparch, "ARCHIVE", 7);
}

int main(int argc, char *argv[])
{
	int i = 1, flag, tempf;
	char tempname[256];
	char path[256];
	char try[256];
	char check[7];
	
	tempname[0] = 0;
	path[0] = 0;
	getcwd(cwd, 255);
	
	printf ("%s %d\n", cwd, argc);
	for(; i<argc; i++)
	{
		printf ("%d %s %s\n", i, argv[i], argv[i+1]);
		if (!strcmp("-p", argv[i]))	//флаг пути
		{
			if (temparch) 	//закрыть текущий архив, в который идет запись, если открыт
			{
				close(temparch);		
				temparch = 0;	
				tempname[0] = 0;
				path[0] = 0;
			}
			if (i == (argc-1)) printf("Warning, the last argument is flag\n");
			else if (!strcmp("-p", argv[i+1]) || !strcmp("-n", argv[i+1])) printf("Warning, two flags are entered one after another\n"); //если два флага подряд, забить на первый
			else 
			{
				switch (whatisthis(argv[i+1]))
				{															//проверить, что после флага
					case 0:															//если файл
						printf("Error, the path to the file was entered, the default path will be used\n");
						i--;			
						break;
					case 1:														//если каталог, все норм
						strcpy(path, argv[i+1]);
						break;
					case 2:													//если непонятно, что, проверяем полный путь	
						switch (tryfullpath(argv[i+1],try))
						{
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
		}
		else if (!strcmp("-n", argv[i]))		//флаг имени
		{	
			if (temparch) 	//закрыть текущий архив, в который идет запись, если открыт
			{
				close(temparch);		
				temparch = 0;	
				tempname[0] = 0;
				path[0] = 0;
			}
			if (i == (argc-1)) printf("Warning, the last argument is flag\n");
			else if (!strcmp("-p", argv[i+1]) || !strcmp("-n", argv[i+1])) printf("Warning, two flags are entered one after another\n"); //если два флага подряд, забить на первый
			else if (strchr(argv[i+1], '/')) printf ("The file name can not contain /\n");
			else 
			{
				strncpy(tempname, argv[i+1], 255);
				i++;
			}
		}
		else 
		{
			flag = whatisthis(argv[i]);
			switch (flag)
			{
				case 2:
					flag = tryfullpath(argv[i],try);
					switch (flag)
					{
						case 2:
							printf("Error, something bad was entered, the default path will be used\n"); 
							break;
						case 0:
							tempf = open(try, O_RDONLY);
							read(tempf, check, 7);
							if (!strncmp("ARCHIVE", check, 7)) 	//если на входе архив, распаковать
							{
								unpack(tempf, path);
								break;
							}
						case 1:
							if (!temparch) 	createarchive(path, tempname); //создать файл для архивирования, если не создан
							if (!flag) 
							{
								pack(tempf);
								break;
							}
							else packdir(try);
					}
					break;
				case 0:
					tempf = open(argv[i], O_RDONLY);
					read(tempf, check, 7);
					if (!strncmp("ARCHIVE", check, 7)) 	//если на входе архив, распаковать
					{
						unpack(tempf, path);
						break;
					}
				case 1: 
					if (!temparch) 	createarchive(path, tempname); //создать файл для архивирования, если не создан
					if (!flag) 
					{
						pack(tempf);
						break;
					}
					else packdir(argv[i]);
			}
		}
	}
	if (temparch) close(temparch);
	exit (0);
}

