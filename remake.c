#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_X 50
#define MAX_Y 50
#define RADIUS 8
#define NEGYZET (RADIUS * 2 + 1)
#define MAX_ERMEK (MAX_X * MAX_Y)
#define MAX_KOROK 24

typedef struct {
    char tipus;
    int x, y;
    int osszegyujtott;
    double bfsTavolsag;
    int szomszedokSzama;
} Erem;

typedef struct {
    int x, y;
} Rover;

char map[MAX_Y][MAX_X];
char local_view[NEGYZET][NEGYZET];

Erem global_ermek[MAX_ERMEK];
int global_ermek_count = 0;

int radius_indexek[MAX_ERMEK];
int radius_db = 0;

Rover roverPoz;

int osszesFelszedett = 0;
int eppenAsunk = 0;


/* ------------------------------------------------------------
   FILE READ
------------------------------------------------------------ */

int fajlBeolvas(const char *filename)
{
    // Initialize map to empty so unused cells are predictable
    for (int y = 0; y < MAX_Y; y++) {
        for (int x = 0; x < MAX_X; x++) {
            map[y][x] = '.';
        }
    }

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
            char c = token[0];

            /* Normalize invalid characters */
            if (c != '#' && c != '.' && c != 'S' && c != 'G' && c != 'Y' && c != 'B')
                c = '.';

            map[row][col] = c;

            if (c == 'S') {
                roverPoz.x = col;
                roverPoz.y = row;
            }

            col++;
            token = strtok(NULL, ",\n\r");
        }
        row++;
    }

    fclose(fp);
    return 0;
}

/* ------------------------------------------------------------
   PRINT MAP
------------------------------------------------------------ */

void printMap()
{
    for (int y = 0; y < MAX_Y; y++) {
        for (int x = 0; x < MAX_X; x++)
            printf("%c", map[y][x]);
        printf("\n");
    }
}

/* ------------------------------------------------------------
   LOCAL RADIUS VIEW
------------------------------------------------------------ */

void radiusKiszamitasa(int roverX, int roverY)
{
    printf("\nLocal Radius View\n");

    for (int i = 0; i < NEGYZET; i++) {
        for (int j = 0; j < NEGYZET; j++) {
            int mapY = roverY + (i - RADIUS);
            int mapX = roverX + (j - RADIUS);

            if (mapX >= 0 && mapX < MAX_X && mapY >= 0 && mapY < MAX_Y)
                local_view[i][j] = map[mapY][mapX];
            else
                local_view[i][j] = '#';
        }
    }

    local_view[RADIUS][RADIUS] = 'S';

    for (int i = 0; i < NEGYZET; i++) {
        for (int j = 0; j < NEGYZET; j++)
            printf("%c", local_view[i][j]);
        printf("\n");
    }
}

/* ------------------------------------------------------------
   NEIGHBOR COUNT
------------------------------------------------------------ */

int szomszedokFunc(int x, int y)
{
    int count = 0;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;

            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < MAX_X && ny >= 0 && ny < MAX_Y) {
                char c = map[ny][nx];
                if (c == 'G' || c == 'Y' || c == 'B')
                    count++;
            }
        }
    }
    return count;
}

/* ------------------------------------------------------------
   BFS DISTANCE
------------------------------------------------------------ */

int bfs_distance(int sx, int sy, int tx, int ty)
{
    int visited[MAX_Y][MAX_X] = {0};
    int qx[MAX_X * MAX_Y], qy[MAX_X * MAX_Y], dist[MAX_X * MAX_Y];

    int head = 0, tail = 0;

    qx[tail] = sx;
    qy[tail] = sy;
    dist[tail] = 0;
    visited[sy][sx] = 1;
    tail++;

    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};

    while (head < tail) {
        int x = qx[head];
        int y = qy[head];
        int d = dist[head];
        head++;

        if (x == tx && y == ty)
            return d;

        for (int i = 0; i < 4; i++) {
            int nx = x + dirs[i][0];
            int ny = y + dirs[i][1];

            if (nx < 0 || nx >= MAX_X || ny < 0 || ny >= MAX_Y) continue;
            if (visited[ny][nx]) continue;
            if (map[ny][nx] == '#') continue;

            visited[ny][nx] = 1;
            qx[tail] = nx;
            qy[tail] = ny;
            dist[tail] = d + 1;
            tail++;
        }
    }

    return -1;
}

/* ------------------------------------------------------------
   BFS NEXT STEP
------------------------------------------------------------ */

void bfs_next_step(int sx, int sy, int tx, int ty, int *outX, int *outY)
{
    int visited[MAX_Y][MAX_X] = {0};
    int parentX[MAX_Y][MAX_X];
    int parentY[MAX_Y][MAX_X];

    int qx[MAX_X * MAX_Y], qy[MAX_X * MAX_Y];
    int head = 0, tail = 0;

    qx[tail] = sx;
    qy[tail] = sy;
    visited[sy][sx] = 1;
    parentX[sy][sx] = -1;
    parentY[sy][sx] = -1;
    tail++;

    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};

    while (head < tail) {
        int x = qx[head];
        int y = qy[head];
        head++;

        if (x == tx && y == ty)
            break;

        for (int i = 0; i < 4; i++) {
            int nx = x + dirs[i][0];
            int ny = y + dirs[i][1];

            if (nx < 0 || nx >= MAX_X || ny < 0 || ny >= MAX_Y) continue;
            if (visited[ny][nx]) continue;
            if (map[ny][nx] == '#') continue;

            visited[ny][nx] = 1;
            parentX[ny][nx] = x;
            parentY[ny][nx] = y;
            qx[tail] = nx;
            qy[tail] = ny;
            tail++;
        }
    }

    if (!visited[ty][tx]) {
        *outX = sx;
        *outY = sy;
        return;
    }

    int cx = tx, cy = ty;

    while (parentX[cy][cx] != -1) {
        int px = parentX[cy][cx];
        int py = parentY[cy][cx];

        if (px == sx && py == sy) {
            *outX = cx;
            *outY = cy;
            return;
        }

        cx = px;
        cy = py;
    }

    *outX = sx;
    *outY = sy;
}

/* ------------------------------------------------------------
   INIT COINS
------------------------------------------------------------ */

void globalEremekInit()
{
    global_ermek_count = 0;

    for (int y = 0; y < MAX_Y; y++) {
        for (int x = 0; x < MAX_X; x++) {
            char c = map[y][x];
            if (c == 'G' || c == 'Y' || c == 'B') {
                Erem *e = &global_ermek[global_ermek_count++];
                e->tipus = c;
                e->x = x;
                e->y = y;
                e->osszegyujtott = 0;
                e->bfsTavolsag = -1;
                e->szomszedokSzama = 0;
            }
        }
    }
}

/* ------------------------------------------------------------
   UPDATE RADIUS COINS
------------------------------------------------------------ */

void radiusEremekFrissitese(int rx, int ry)
{
    radius_db = 0;

    for (int i = 0; i < global_ermek_count; i++) {
        Erem *e = &global_ermek[i];
        if (e->osszegyujtott) continue;

        int tav = bfs_distance(rx, ry, e->x, e->y);

        if (abs(e->x - rx) <= RADIUS &&
            abs(e->y - ry) <= RADIUS &&
            tav != -1)
        {
            e->bfsTavolsag = tav;
            e->szomszedokSzama = szomszedokFunc(e->x, e->y);
            radius_indexek[radius_db++] = i;
        }
    }
}

/* ------------------------------------------------------------
   COIN VALUE
------------------------------------------------------------ */

int eremErteke(char t)
{
    if (t == 'G') return 3;
    if (t == 'Y') return 2;
    if (t == 'B') return 1;
    return 0;
}

/* ------------------------------------------------------------
   SELECT TARGET COIN
------------------------------------------------------------ */

int valasszCelErmet(int maradekKorok)
{
    double best = -1.0;
    int bestIndex = -1;

    for (int k = 0; k < radius_db; k++) {
        int idx = radius_indexek[k];
        Erem *e = &global_ermek[idx];

        if (e->osszegyujtott) continue;
        if (e->bfsTavolsag < 0) continue;

        int koltseg = (int)e->bfsTavolsag + 1;
        if (koltseg > maradekKorok) continue;

        int ertek = eremErteke(e->tipus);
        if (ertek == 0) continue;

        double pont = (double)ertek / koltseg;

        if (pont > best) {
            best = pont;
            bestIndex = idx;
        }
    }

    return bestIndex;
}

/* ------------------------------------------------------------
   GAME LOOP
------------------------------------------------------------ */

/* ------------------------------------------------------------
   GAME LOOP
------------------------------------------------------------ */

void jatek(int orak)
{
    int maradekKorok = orak * 2;   // 1 round = 0.5 hours
    int osszPont = 0;
    int osszesFelszedett = 0;

    globalEremekInit();

    while (maradekKorok > 0) {
        printf("\n--- Kör, maradék körök: %d ---\n", maradekKorok);

        int eppenAsunk = 0;   // reset digging flag

        radiusKiszamitasa(roverPoz.x, roverPoz.y);
        radiusEremekFrissitese(roverPoz.x, roverPoz.y);

        printf("Összes érem: %d, radiusban: %d\n", global_ermek_count, radius_db);

        /* Check if rover stands on a coin (by coordinates) */
        for (int i = 0; i < global_ermek_count; i++) {
            Erem *e = &global_ermek[i];
            if (!e->osszegyujtott &&
                e->x == roverPoz.x &&
                e->y == roverPoz.y)
            {
                int ertek = eremErteke(e->tipus);
                e->osszegyujtott = 1;

                osszPont += ertek;
                osszesFelszedett++;
                eppenAsunk = 1;

                maradekKorok--;

                printf("Érme felszedve (%c), pont=%d, összPont=%d\n",
                       e->tipus, ertek, osszPont);

                goto kor_vege;
            }
        }

        /* Select target */
        int celIndex = valasszCelErmet(maradekKorok);
        if (celIndex == -1) {
            printf("Nincs elérhető érem az időkereten belül.\n");
            break;
        }

        Erem *cel = &global_ermek[celIndex];
        printf("Cél érme: %c (%d,%d), BFS=%.0f\n",
               cel->tipus, cel->x, cel->y, cel->bfsTavolsag);

        int nx, ny;
        bfs_next_step(roverPoz.x, roverPoz.y, cel->x, cel->y, &nx, &ny);

        printf("Rover lép: (%d,%d) -> (%d,%d)\n",
               roverPoz.x, roverPoz.y, nx, ny);

        /* Move rover */
        map[roverPoz.y][roverPoz.x] = '.';
        roverPoz.x = nx;
        roverPoz.y = ny;
        map[roverPoz.y][roverPoz.x] = 'S';

        maradekKorok--;

    kor_vege:

        printf("Felszedett érmék eddig: %d\n", osszesFelszedett);
        printf("Éppen ásunk: %s\n", eppenAsunk ? "igen" : "nem");

        if (maradekKorok <= 0) {
            printf("Elfogyott az idő.\n");
            break;
        }
    }

    printf("\nJáték vége. Összpont: %d\n", osszPont);
}

/* ------------------------------------------------------------
   MAIN
------------------------------------------------------------ */

int main()
{
    int orak;

    printf("Add meg az időt órában: ");
    scanf("%d", &orak);

    if (fajlBeolvas("mars_map_50x50.csv") != 0)
        return 1;

    printf("Kezdő pálya:\n");
    printMap();

    jatek(orak);
    return 0;
}
