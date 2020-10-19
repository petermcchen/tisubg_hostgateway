#include <stdio.h>
#include <stdlib.h>

int main(){
	//Initialize the character array
	char str[100];
	int i=0,j=0;
	
	printf("Enter the string into the file\n");
	//takes all the characters until enter is pressed
	while((str[i]=getchar())!='\n'){
		//increment the index of the character array
		i++;
	}
	
	//after taking all the character add null pointer 
	//at the end of the string
	str[i]='\0';
	printf("\nThe file content is - ");
	//loop is break when null pointer is encountered
	while(str[j]!='\0'){
		//print the characters
		putchar(str[j]);
		j++;
	}
	
	return 0;
}
