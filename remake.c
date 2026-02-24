#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h> //strtok

#define MAX_X 50
#define MAX_Y 50
#define RADIUS 8
#define NEGYZET (RADIUS * 2 + 1)
#define MAX_ERMEK (50 * 50) //feltételezem, hogy az egész fájl csak az érmekből áll

typedef struct {
    char tipus;     
    int x;          
    int y;         
    double centrumTavolsag;
    int szomszedokSzama;
    int osszegyujtott;
} Erem;

typedef struct {
    int x;
    int y;
} Rover;

Erem global_ermek[MAX_ERMEK];
int global_ermek_count = 0;

char map[MAX_Y][MAX_X] = {0}; 
char local_view[NEGYZET][NEGYZET]; //local_view = az a radius array

int radius_indexek[MAX_ERMEK]; //az egész mezőben (a nagy mezőben) indexeljük az érmeket
int radius_db = 0;

Rover ROVER_POS;

int fajlBeolvas(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    char sor[1024];
    int row = 0;

    while (fgets(sor, sizeof(sor), fp) && row < MAX_Y) {
        int col = 0;
        char *token = strtok(sor, ",\n\r");
        while (token && col < MAX_X) {
            map[row][col] = token[0];

            if (token[0] == 'S') {
                ROVER_POS.x = col;
                ROVER_POS.y = row;
            }

            col++;
            token = strtok(NULL, ",\n\r");
        }
        row++;
    }

    fclose(fp);
    return 0;
}

void printMap() {
    for (int y = 0; y < MAX_Y; y++) {
        for (int x = 0; x < MAX_X; x++) {
            printf("%c", map[y][x]);
        }
        printf("\n"); 
    }
}

void radiusKiszamitasa(int roverX, int roverY) {
    printf("\nLocal Radius View\n");
    for (int i = 0; i < NEGYZET; i++) {
        for (int j = 0; j < NEGYZET; j++) {
            int mapY = roverY + (i - RADIUS);   
            int mapX = roverX + (j - RADIUS);

            if (mapX >= 0 && mapX < MAX_X && mapY >= 0 && mapY < MAX_Y) {
                local_view[i][j] = map[mapY][mapX];
            } else {
                local_view[i][j] = '#'; //ha nincs benne a rangeban akkor block van, nem léphet ide
            }
        }
    }
    
     
    local_view[RADIUS][RADIUS] = 'S'; //biztonság kedvéért középre rakjuk a roverünket

    for (int i = 0; i < NEGYZET; i++) { //debug
        for (int j = 0; j < NEGYZET; j++) {
            printf("%c", local_view[i][j]);
        }
        printf("\n");
    }
}

int szomszedokFunc(int eremX, int eremY) {
    int szomszedokSzama = 0;
    for (int y = -1; y <= 1; y++) { //elmozdulas abs(1) minden irányba
        for (int x = -1; x <= 1; x++) { 
            if (x == 0 && y == 0) continue; //magát az érmet nem vesszük figyelembe

            int ellenorzoX = eremX + x;
            int ellenorzoY = eremY + y;

            if (ellenorzoX >= 0 && ellenorzoX < MAX_X && ellenorzoY >= 0 && ellenorzoY < MAX_Y) {
                char c = map[ellenorzoY][ellenorzoX];
                if (c == 'G' || c == 'Y' || c == 'B') {
                    szomszedokSzama++;
                }
            }
        }
    }
    return szomszedokSzama; //visszaadjuk a szomszédok számát
}

double centrumTavolsag(int x, int y, int center_x, int center_y) {
    return sqrt(pow(x - center_x, 2) + pow(y - center_y, 2)); //Euclidean distance
}

void globalEremekInit() { //initializáljuk az érmeket, amelyeket majd dinamikusan fogjuk változtatni
    global_ermek_count = 0;
    for (int y = 0; y < MAX_Y; y++) {
        for (int x = 0; x < MAX_X; x++) {
            char c = map[y][x];
            if (c == 'G' || c == 'Y' || c == 'B') { //megvizsgáljuk, hogy a map array jelenlegi karaktere érem-e; ha igen akkor initializálunk
                global_ermek[global_ermek_count].tipus = c;
                global_ermek[global_ermek_count].x = x;
                global_ermek[global_ermek_count].y = y;
                global_ermek[global_ermek_count].osszegyujtott = 0;
                global_ermek_count++; //növeljük eggyel, ez azért kell mert ezzel fogjuk majd elérni a global_ermek tömb egyes elemeit; egész mezőnek az érmek indexeit tartalmazza
                //centrumTavolság és szomszédokSzama attributomak más funkcióhoz tartoznak
            }
        }
    }
}

void radiusEremekFrissitese(int roverX, int roverY) {
    radius_db = 0;
    for (int i = 0; i < global_ermek_count; i++) { //amiután megvan a nagy mezőben lévő érmek száma, initializáljuk a többi attributumot hozzá
        if (global_ermek[i].osszegyujtott) continue; //csak abban az esetben ha még nem szedtük össze-e érmet


        double tavolsag = centrumTavolsag(global_ermek[i].x, global_ermek[i].y, roverX, roverY); 

        if (abs(global_ermek[i].x - roverX) <= RADIUS &&  //biztonsági ellenőrzés, éremek benne vannak-e a rangeban
            abs(global_ermek[i].y - roverY) <= RADIUS) {
            
            global_ermek[i].centrumTavolsag = tavolsag; //initializáljuk a távolságot ill. a szomszéd érmeket
            global_ermek[i].szomszedokSzama = szomszedokFunc(global_ermek[i].x, global_ermek[i].y);
            radius_indexek[radius_db++] = i; //hozzáadjuk a radius index tömbhez az érmek indexét
        }
    }
}

void jatekKezdete() {
    globalEremekInit();
    radiusKiszamitasa(ROVER_POS.x, ROVER_POS.y);
    radiusEremekFrissitese(ROVER_POS.x, ROVER_POS.y); //minden fordulatban kell frissíteni mielőtt mozogna a rover

    printf("Összes érem: %d\n", global_ermek_count);
    printf("Radiusban lévő ermek: %d\n", radius_db);

    for (int i = 0; i < radius_db; i++) {
        int eremIndex = radius_indexek[i];
        printf("Erem #%d: Tipus=%c, Pos=(%d,%d), Tavolsag=%.2f, Szomszedok=%d\n", 
            i, global_ermek[eremIndex].tipus, global_ermek[eremIndex].x, global_ermek[eremIndex].y, 
            global_ermek[eremIndex].centrumTavolsag, global_ermek[eremIndex].szomszedokSzama);
    }
}

int main() {
    if (fajlBeolvas("mars_map_50x50.csv") != 0) return 1;
    printMap(); //debug
    jatekKezdete();

    printf("DEBUG: \n");
    printf("Global indexed: %d\n", radius_db);
    for (int i = 0; i < radius_db; i++) {
        printf("Rad: %d\n", radius_indexek[i]);
    }


    return 0;
}