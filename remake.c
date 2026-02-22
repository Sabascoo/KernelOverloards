#include <stdio.h>
#include <stdint.h>  //uint8_t
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 6000 //lehet, hogy 5000-ra kell levenni majd
#define MAX_X 50
#define MAX_Y 50


typedef uint8_t MarsMap;
MarsMap marsmap[MAX_X][MAX_Y];

int fileBeolvas(char *file, MarsMap marsmap[MAX_X][MAX_Y]) {



    size_t filedesc = open(file, O_RDONLY);
    if (filedesc < 0) {
        printf("Failed reading in");
        return 1;
    } else {
        printf("Success read\n");

    }
    printf("%lu\n", filedesc);


    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while((bytes_read = read(filedesc, buffer, BUFFER_SIZE)) > 0) {
        printf("%i\n", bytes_read);

        
    }


    int row = 0;
    int column = 0;

    for (int i = 0; i < strlen(buffer); i++) {
        int commanHex = 44;
        int newlineHex = 10; //debug --> find if debugs are comprehended, if yes then use it to separate rows
        int S_Hex = 83;

        if (commanHex == buffer[i]) { //all commas deleted
            continue;
        }

        
        if (newlineHex == buffer[i]) {
            row += 1; //shift our row down
            column = 0; //set column again to zeroooooooo ;) 
            
        }

        if (S_Hex == buffer[i]) {

            printf("Van rover\n");

        }
        
        
        marsmap[row][column] = buffer[i];
        

       
        column += 1; //shift out array by 1 to the right
       
    }

}

int main() {

    fileBeolvas("mars_map_50x50.csv", marsmap); 

    printf("%s", marsmap);

    return 0;
}