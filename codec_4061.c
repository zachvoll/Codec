/*
 * name: Zachary Vollen, Yadu Kiran
 * Date: 10/28/2015
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include "codec.c"
#include "makeargv.h"

//#define DEBUG

int mode; // Set to 1 for encoding, 2 for Decoding;
mode_t perm = 0755; //Permissions for creating new files
char *src_path,*dst_path;//Stores the Source and Destination Path
FILE *report;//File Handle for the Report File

//Store Properties of a File that will be written in the report
char type[32];
unsigned long input_size = 0;
unsigned long output_size = 0;

//Used to keep track of Hard Links
int numEntries = 0;
//Array which stores the inode number of all the files that have been encoded/decoded
unsigned long inodeArray[64];

//used for processing report file lines
int entries = 0;
char* lines[64];

//Function Declarations
int createdir(char *path);
void readDirContents(char *path);
void readFileContents(char *inputfilepath);
void writeEntry(char *name);
int isHardlink(unsigned long inodenumber);
void writeLines();
int cmpfunc (const void * a, const void * b);

void main(int argc, char *argv[])
{
	//Check if the user has entered the correct number of Arguments
	if(argc != 4)
	{
		fprintf(stderr,"Error: Insufficient number of Arguments \n Usage: ./codec_4061 <-e for encoding, -d for decoding> <input directory> <output directory>\n");
		exit(1);
	}

	//If operation is encode set mode to 1, or 2 if decode
	if(strcmp(argv[1],"-e") == 0)
	{
		mode = 1;
	}
	else if(strcmp(argv[1],"-d") == 0)
	{
		mode = 2;
	}
	else
	{
		fprintf(stderr,"Incorrect mode, must use -e for encoding or -d for decoding.\n");
		exit(1);
	}		
	
	src_path = argv[2];
	dst_path = argv[3];
	
	//Execute the Program
	printf("Executing.......\nPlease Be Patient \n");
	readDirContents(src_path);

	//Create the path for the Report File
	char reportpath[4096]; 
	strcpy(reportpath,dst_path);
	strcat(reportpath,"/");
	strcat(reportpath,src_path);
	strcat(reportpath,"_report.txt");

	//fprintf(stdout,"the report path: %s\n", reportpath);

	report = fopen(reportpath, "w+");
	
	//sort report file lines
	qsort(lines, entries, sizeof(char*), cmpfunc);	
	
	//write lines to report file
	writeLines();	
	
	fclose(report);

	printf("Finished Execution \n");
		
}

//creates a directory
int createdir(char *path)
{
	//Create a Directory specified by path
	int retval = 0; 
	if((mkdir(path, perm)) == -1)
	{
			retval = -1;
	}
	return retval;//Returns 0 on success, -1 on error
}
	
//processes directories, handles symbolic links, and calls readFileContents to process regular files and handle hard links
void readDirContents(char *path)
{
	DIR *inputdir;
	struct dirent *dentry;
	char *filename;
	
	//Open the Directory
	if((inputdir=opendir(path)) == NULL)
	{
		perror("Error Opening Input Directory");
		exit(1);
	}
	
	//Create the new output path
	char outputPath[4096];
	strcpy(outputPath,dst_path);
	strcat(outputPath,"/");
	strcat(outputPath,path);
	
	#ifdef DEBUG
	fprintf(stdout,"Output Path:%s \n", outputPath);
	#endif
	
	//Create a directory in output directory
	if(createdir(outputPath) == -1)
	{
		perror("Error Creating Output Directory\n ");
		exit(1);
	}
	
	//Read each Entry in the Directory
	while((dentry=readdir(inputdir)) != NULL)
	{
		//Skip if directory is . or .. 
		if((strcmp(dentry->d_name,".") == 0)||(strcmp(dentry->d_name,"..") == 0))
		{
			continue;
		}
		
		//new_path stores the path of the entry which is currently being Encoded/Decoded
		char new_path[4096];
		strcpy(new_path,path);
		strncat(new_path,"/",1);
		strncat(new_path,dentry->d_name,strlen(dentry->d_name));
		#ifdef DEBUG
		fprintf(stdout,"New Path:%s\n",new_path);
		#endif
		//If the Entry is a Soft Link, write to reportskip to next entry
		if(dentry->d_type == DT_LNK)
		{
			strcpy(type,", sym link, ");
			input_size = 0;
			output_size = 0;
			writeEntry(dentry->d_name);
			continue;
		}
		//If the entry is a directory, Recurse
		else if(dentry->d_type == DT_DIR)
		{	
			//set info for report file
			
			strcpy(type,", directory, ");
			input_size = 0;
			output_size = 0;
			writeEntry(dentry->d_name);
			readDirContents(new_path);
			continue;
		}
		
		//If the entry is a file, call readFileContents
		else
		{
			readFileContents(new_path);
			writeEntry(dentry->d_name);		
		}
	}
	
	//Close the Directory
	if(closedir(inputdir) == -1)
	{
		perror("Error encountered when closing the directory");
		exit(1);
	}
}

//processes regular files and handles hard links
void readFileContents(char *inputfilepath)
{
	//Open File
	FILE *input;
	input = fopen(inputfilepath,"r");
	
	//Set the path for the Output FIle
	char outputpath[4096];	
	strcpy(outputpath, dst_path);
	strcat(outputpath, "/");
	strcat(outputpath, inputfilepath);
	#ifdef DEBUG
	fprintf(stderr,"Outpath: %s\n", outputpath);
	#endif
	
	//Determine input file length
	struct stat filestat;
	stat(inputfilepath, &filestat);
	unsigned long size = filestat.st_size;
	unsigned long inode = filestat.st_ino;
	input_size = size;
	strcpy(type,", regular file, ");
	
	//If it's a hard link we've already seen, add a line to the report file and ignore the file
	if(isHardlink(inode) == 1)
	{
		strcpy(type, ", hard link, ");
		input_size = 0;
		output_size = 0;
		return;		
	}
	else
	{
		strcpy(type,", regular file, ");
		inodeArray[numEntries] = inode;
		numEntries++;
	}
	
	//Open the Outputfile
	FILE *output;
	output = fopen(outputpath, "w");
	
	if(size != 0)
	{
		//If operation is Encode
		if(mode == 1)
		{
			//Read 3 characters at a time and store it in the buffer
			uint8_t inputbuf[3];
			uint8_t outputbuf[4];			
			while(size != 0)
			{				
				int i;
				if(size >= 3){
					for(i = 0; i < 3; i++)
					{
						inputbuf[i] = fgetc(input);
					}
					size -= 3;
					
					//Call the Encode Block
					int size_encoded = encode_block(inputbuf, outputbuf, 3);
					if (size_encoded != 4)
					{
						fprintf(stderr,"Failed to Encode particular block\n");
						exit(1);
					}
				}
				else
				{	//If size is not a multiple of 3, pad it with 0s
					unsigned long j;
					for(j = 0; j < size; j++)
					{
						inputbuf[j] = fgetc(input);
					}
					for(; j< 3; j++)
					{
						inputbuf[j] = 0;
					}
		
					int size_encoded= encode_block(inputbuf, outputbuf, size);
					if (size_encoded != 4)
					{
						fprintf(stderr, "Failed to Encode the last block of characters \n");
						exit(1);
					}
					size = 0;
				}
				//write to output file
				size_t bytes_written;
				bytes_written = fwrite(outputbuf, 1, 4, output);
				if (bytes_written != 4)
				{
					fprintf(stderr, "Failed to write to Output File \n");
					exit(1);
				}				
			}
			//Add new line to output file	
			unsigned char ch = 0x0a;
			fputc(ch, output);					
		}
		//If operation is Decode
		else if(mode == 2)
		{
			int inputbuff_index = 0;
			uint8_t inputbuf[4];
			uint8_t outputbuf[3];
			while(size != 0)
			{
				uint8_t currentchar = fgetc(input);
				if(is_valid_char(currentchar) == 1)//Check if Read Character is Valid
				{
					inputbuf[inputbuff_index] = currentchar;
					inputbuff_index++;
				}
				if(inputbuff_index == 4)
				{
					int size_decoded=decode_block(inputbuf, outputbuf);
					//Write to Output File
					fwrite(outputbuf, 1, size_decoded,output);
					inputbuff_index = 0;
				}
				size--;
			 }
		 }	 
	}
	
	//Close input file
	if(fclose(input) == EOF)
	{
		perror("Error failed to close input file.\n");
	}
	//Close output file
	if(fclose(output) == EOF)
	{
		perror("Error failed to close output file.\n");
	}	
	
	//Set the Size of the Output File
	struct stat outputfilestat;
	stat(outputpath, &outputfilestat);
	output_size = outputfilestat.st_size;
		
}

//puts a report file line into the lines array to later be sorted
void writeEntry(char *name)
{	
	char line[1024];
	line[0]=0;
	strcat(line,name);
	strcat(line,type);
	
	//Convert the Input and Output Size from Integer to String
	char str[25];
	sprintf(str, "%d", input_size);
	strcat(line,str);
	strcat(line,", ");
	char str2[25];
	sprintf(str2, "%d", output_size);
	strcat(line,str2);
	strcat(line,"\n");
	
	//add lines to report file array
	lines[entries] = malloc(sizeof(line));
	strcpy(lines[entries], line);
	entries++;
}

//returns 1 for true if a regular file with the same inode has been processed before
int isHardlink(unsigned long inodenumber) 
{
	int retval = 0;
	int i;
	for(i = 0; i < numEntries; i++)
	{
		if(inodeArray[i] == inodenumber)
		{
			retval = 1;
		}			
	}
	return retval;
}

//writes all of the lines to the report file
void writeLines()
{
	int i;
	for(i = 0; i < entries; i++)
	{
		fwrite(lines[i], 1, strlen(lines[i]), report);
		free(lines[i]);
	}
}

//compare function used by qsort
int cmpfunc (const void * a, const void * b)
{
   return strcmp( *(char * const *)a, *(char * const*)b );
}
