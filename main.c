#include <stdio.h>
#include <unistd.h> //read

#include <fcntl.h> //open

//open file descriptor, fildes; read nbyte bytes 
// ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
//open is a system call that is used to open a new file and obtain its file descriptor

//szerintem az érceket el fogom menteni ide valahova, legyen globális és hozzáférhető adatbázisból
//lehet, hogy flagokat érdemes lenne bitesebben eltárolni
#define MAX_X 50
#define MAX_Y 50
#define BUFFER_SIZE 6000

#define sharpASCII 35
#define B_ASCII 66 //kék ásvány
#define Y_ASCII 89 //sárga ásvány
#define G_ASCII 71 //zöld ásvány
#define dotASCII 46



int mezoEllenorzo(char array[MAX_X][MAX_Y], int ROVER_POS[], int lepes) {

    //0,1,2,3,4,5,6,7,8
    //fel, le, jobbra, balra, felésjobb; felésbal; leésjobb; leésbal (most max 7 indexű)
    int mozgas; // 1 vagy 0 FLAG 1 = van mozgás; 0 = nincs mozgás
    int ercMezo; //indikátor, van-e érc a mezőn 1 vagy 0 

    int mozgasXtengely[] = {-1, 1, 0, 0, -1, -1, 1, 1}; 
    int mozgasYtengely[] = {0, 0, 1, -1, 1, -1, 1, -1};
    
    int x = ROVER_POS[0] + mozgasXtengely[lepes];
    int y = ROVER_POS[1] + mozgasYtengely[lepes];


    if (x >= 0 && x < MAX_X && y >= 0 && y < MAX_Y) {

        switch (array[x][y]) {
            case sharpASCII:
                printf("Blokk van! Nem lépünk!\n");
                mozgas = 0;
                break;

            case B_ASCII:
            case Y_ASCII:
            case G_ASCII:
                printf("Érc mező! Lépjünk!\n");
                mozgas = 1;
                ercMezo = 1;
                ROVER_POS[0] = x;
                ROVER_POS[1] = y;
                break;

            case dotASCII:
                printf("Üres mező! Lépjünk!\n");
                mozgas = 1;
                ROVER_POS[0] = x;
                ROVER_POS[1] = y;
                break; 

            default:
                mozgas = 0;
                break;
        }

        return mozgas, ercMezo;
    }
}

void banyaszFunc(char array[MAX_X][MAX_Y], int ROVER_POS) {

}

void Iranyitas(char array[MAX_X][MAX_Y], int ROVER_POS[]) {

    int banyasz = -1; //1 or 0; ez is egy flag

    while (!(banyasz != 0 || banyasz != 1)) {
        printf("Bányászik vagy lép?\n");
        scanf("%d", &banyasz);
    }

    if (banyasz) {
        banyaszFunc(array, ROVER_POS);
    }

    int lepes = -1;
    printf("Rover poziciója mozgás előtt: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);


    while(lepes < 0 || lepes > 7) {
        printf("Hova mozogjon a rover? (lásd a számokat)\n"); 
        scanf("%d", &lepes);
    }

    if (!(mezoEllenorzo(array, ROVER_POS, lepes))) { //ha 0-t ad vissza az hamis
        printf("A mezo nem szabad. Vagy nemlétező mező\n");
        
    } 

    //lepes = 0;
}

void JatekKezdete(char array[MAX_X][MAX_Y], int ROVER_POS[]) { //játék logika
    
    int akku = 100;
    int adottIdoTartam;

    while (adottIdoTartam < 24) {
        printf("Adja meg egy időtartamot (óra): \n");
        scanf("%d", &adottIdoTartam);
    }

    int lepesekSzama = adottIdoTartam * 2; //egy óra = 2 félóra
    printf("Sikeresen töltöttünk be az időt!\n");

    printf("Rover Pozíciója: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
    printf("Aksi játek elején: %d\n", akku);
    printf("Adott idő: %d\n", adottIdoTartam);
    printf("Elindulási idő: 6:30\n");
    printf("Fordulatok/Lépések Száma: %d\n", lepesekSzama);
    
    for (int i = 0; i < lepesekSzama; i++) {
        printf("Lépés száma: %d a %d-ból/ből\n", (i+1), lepesekSzama);
        Iranyitas(array, ROVER_POS);
        printf("New position of the rover: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
    }


}


int main() { //fájl beolvasás, fájl műveletek és funkció hívások


    char *FILENAME = "mars_map_50x50.csv";
    size_t filedesc = open(FILENAME, O_RDONLY);
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

    char array[50][50] = {0}; //tömb, a mezővel
    int ROVER_POS[2]; 

    //Rover_POS; akku; nap, éj, áll, mozog, bányász, lassú, gyors, normál, elindulási idő 


    int newLineCount = 0; //we count newlines and separate by them
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
            newLineCount += 1;
            row += 1; //shift our row down
            column = 0; //set column again to zeroooooooo ;) 
            
        }

        if (S_Hex == buffer[i]) {

            ROVER_POS[0] = row;
            ROVER_POS[1] = column; 

        }
        char convertedBuff = buffer[i];
        array[row][column] = convertedBuff; //converts hex to the char
        

       
        column += 1; //shift out array by 1 to the right
       
    }

    JatekKezdete(array, ROVER_POS);


    if (close(filedesc) < 0) {
        printf("Failed closing");
        return 1;
    } else {
        printf("Successful close\n");
        return 0;
    }

    return 0;
}