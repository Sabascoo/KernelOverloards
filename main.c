// rover_sim.c  (C11)
// Compile:  gcc -O2 -std=c11 rover_sim.c -o rover_sim
// Run:      ./rover_sim
//
// Reads:    mars_map_50x50.csv
// Writes:   rover_log.csv
//           ai_route.txt
//
// Log columns (13):
// round,x,y,battery,v,megtettCellak,totalMinerals,green,yellow,blue,ido_intervalum,napszak,muvelet
// muvelet in {STANDING, MOVING, DIGGING}

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAP_SIZE 50
#define W MAP_SIZE
#define H MAP_SIZE

#define UTES_PER_ORA 2
#define NAP_UTES 32
#define ESZAKA_UTES 16
#define CIKLUS_UTES (NAP_UTES + ESZAKA_UTES) //24 órás napot takar

#define MAX_AKKU 100.0
#define K_ALLANDO 2.0

#define MAX_PATH (W * H)

typedef struct { int8_t x, y; } Pt;

typedef struct {
    int8_t x, y;
    double bat;

    int green, yellow, blue;
    int utes;         // half-hour rounds since start
    int moved_cells;  // total moved cells
} Rover;

typedef struct {
    int total_utess;
    int8_t sx, sy;
} Mission;

typedef struct {
    double g, f;
    int8_t px, py;
    bool open, closed;
} Node;

typedef struct {
    bool ok;
    int end_utes;
    double end_bat;
} FeasResult;

static uint8_t mapv[H][W];      // 0 = free, 1 = obstacle, 2 = mineral
static char    mapc[H][W];      // '.', '#', 'B','Y','G','S'
static uint8_t home_dist[H][W]; // 8-dir BFS distance to home, 255 = unreachable

static inline int inRange(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

static inline double hatarRogzito(double bemenet, double alsoHatar, double felsoHatar) {
    if (bemenet < alsoHatar) return alsoHatar;
    if (bemenet > felsoHatar) return felsoHatar;
    return bemenet;
}

static inline double mozgasKoltseg(int v) { return K_ALLANDO * (double)(v * v); } //k * v^2
static inline double idlePozicio(bool banyasz) { return banyasz ? 2.0 : 1.0; }
static inline double napiToltes(bool nap) { return nap ? 10.0 : 0.0; }


static bool banyaszhat(double bat) {
    return bat >= idlePozicio(true); // banyasz cost = 2
}


static double mozogFunc(double bat, bool nap, int v) {
    return hatarRogzito(bat - mozgasKoltseg(v) + napiToltes(nap), 0.0, MAX_AKKU);
}

static double banyaszFunc(double bat, bool nap) {
    return hatarRogzito(bat - idlePozicio(true) + napiToltes(nap), 0.0, MAX_AKKU);
}

static double allFunc(double bat, bool nap) {
    if (nap) {
        return hatarRogzito(bat - idlePozicio(false) + napiToltes(nap), 0.0, MAX_AKKU);
    }
    // At night, if battery < 1, rover cannot spend that energy; remains at 0.
    if (bat >= idlePozicio(false)) {
        return hatarRogzito(bat - idlePozicio(false), 0.0, MAX_AKKU);
    }
    return 0.0;
}


static bool mozoghat(double bat, int v) {
    return bat >= mozgasKoltseg(v);
}

static bool allhat(bool nap, double bat) {
    (void)bat;
    if (nap) return true;
    return bat >= idlePozicio(false) || bat <= 0.0; // at night with 0, time can still pass at 0
}


// Verification helpers (Checks if action is possible)
static bool varakozasLehetseges(bool nap, double bat) {
    return allhat(nap, bat);
}

static bool mozgasLehetseges(double bat, int v) {
    return mozoghat(bat, v);
}

static bool banyaszasLehetseges(double bat) {
    // You have a function called banyaszhat, but it's missing a return type in your snippet.
    // I'll define the check here:
    return bat >= 2.0; 
}

// Application helpers (Applies the battery cost/gain)
static double varakozasAlkalmazasa(double bat, bool nap) {
    return allFunc(bat, nap);
}

static double mozgasAlkalmazasa(double bat, bool nap, int v) {
    return mozogFunc(bat, nap, v);
}

static double banyaszasAlkalmazasa(double bat, bool nap) {
    return banyaszFunc(bat, nap);
}

// Helpers for sim_step calls
static bool can_do_mine(double bat) {
    return bat >= 2.0;
}

static double apply_mine(double bat, bool nap) {
    return banyaszFunc(bat, nap);
}

static double apply_stand(double bat, bool nap) {
    return allFunc(bat, nap);
}

static inline bool nap_van_ciklus(int utes) {
    // utes 0 starts at sunrise (06:00), naptime lasts 16h = 32 utess
    return (utes % CIKLUS_UTES) < NAP_UTES;
}

static inline float napCiklus(int utes) {
    float t = 6.0f + 0.5f * (float)utes;
    while (t >= 24.0f) t -= 24.0f;
    while (t < 0.0f)   t += 24.0f;
    return t;
}




static double chebyshev(int kezdo_x, int kezdo_y, int cel_x, int cel_y) {
    int x = abs(kezdo_x - cel_x);
    int y = abs(kezdo_y - cel_y);
    return (double)((x > y) ? x : y);
}

static int minUtesekLepesekhez(int lepesek, int v) {
    if (lepesek <= 0) return 0;
    return (lepesek + v - 1) / v;
}

static int teljesUtMinUtesek(int out_lepesek, bool kell_banyaszni, int in_lepesek) {
    return minUtesekLepesekhez(out_lepesek, 3)
         + (kell_banyaszni ? 1 : 0)
         + minUtesekLepesekhez(in_lepesek, 3);
}

// -------------------- GUI-compatible logger --------------------
static void log_gui_line(FILE *f, int round, const Rover *r,
                         int v, int megtett_cellak,
                         const char *ido_intervalum, float napszak,
                         const char *muvelet)
{
    int total = r->green + r->yellow + r->blue;
    fprintf(f, "%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%s,%.2f,%s\n",
            round,
            (int)r->x, (int)r->y,
            (float)r->bat,
            v,
            megtett_cellak,
            total,
            r->green, r->yellow, r->blue,
            ido_intervalum,
            napszak,
            muvelet);
}

// -------------------- BFS home distance (8-dir, unit cost) --------------------
static void cellatolHazaTav(int8_t bazis_x, int8_t bazis_y) {
    memset(home_dist, 255, sizeof(home_dist)); // Initialize all to 255
    static int16_t sor_x[MAX_PATH], sor_y[MAX_PATH];
    int sor_eleje = 0, sor_vege = 0;

    home_dist[bazis_y][bazis_x] = 0;
    sor_x[sor_vege] = bazis_x;
    sor_y[sor_vege] = bazis_y;
    sor_vege++;

    static const int szomszed_x[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int szomszed_y[8] = {-1,  0,  1,-1, 1,-1, 0, 1};

    while (sor_eleje < sor_vege) {
        int aktualis_x = sor_x[sor_eleje];
        int aktualis_y = sor_y[sor_eleje];
        sor_eleje++;

        uint8_t aktualis_tavolsag = home_dist[aktualis_y][aktualis_x];

        for (int szomszed_index = 0; szomszed_index < 8; szomszed_index++) {
            int kovetkezo_x = aktualis_x + szomszed_x[szomszed_index];
            int kovetkezo_y = aktualis_y + szomszed_y[szomszed_index];

            if (!inRange(kovetkezo_x, kovetkezo_y)) continue;
            if (mapv[kovetkezo_y][kovetkezo_x] == 1) continue;
            if (home_dist[kovetkezo_y][kovetkezo_x] != 255) continue;

            home_dist[kovetkezo_y][kovetkezo_x] = (uint8_t)(aktualis_tavolsag + 1);
            sor_x[sor_vege] = kovetkezo_x;
            sor_y[sor_vege] = kovetkezo_y;
            sor_vege++;
        }
    }
}

// -------------------- A* pathfinding (8-dir, unit cost) --------------------
static int utvonalVisszaepites(Node nodes[H][W], int start_x, int start_y, int cel_x, int cel_y, Pt *utvonal, int max_hossz) {
    int ut_hossz = 0;
    int aktualis_x = cel_x;
    int aktualis_y = cel_y;

    while (!(aktualis_x == start_x && aktualis_y == start_y)) {

        if (ut_hossz >= max_hossz)
            return -1;

        utvonal[ut_hossz++] = (Pt){(int8_t)aktualis_x, (int8_t)aktualis_y};

        int elozo_x = nodes[aktualis_y][aktualis_x].px;
        int elozo_y = nodes[aktualis_y][aktualis_x].py;

        if (elozo_x < 0 || elozo_y < 0)
            return -1;

        aktualis_x = elozo_x;
        aktualis_y = elozo_y;
    }

    if (ut_hossz >= max_hossz)
        return -1;

    utvonal[ut_hossz++] = (Pt){(int8_t)start_x, (int8_t)start_y};

    for (int i = 0; i < ut_hossz / 2; i++) {
        Pt csere = utvonal[i];
        utvonal[i] = utvonal[ut_hossz - 1 - i];
        utvonal[ut_hossz - 1 - i] = csere;
    }

    return ut_hossz;
}

static int astarAlg(int start_x, int start_y, int cel_x, int cel_y, Pt *utvonal, int max_hossz) {
    Node nodes[H][W];
    for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
        nodes[y][x].g = 1e100;
        nodes[y][x].f = 1e100;
        nodes[y][x].px = -1;
        nodes[y][x].py = -1;
        nodes[y][x].open = false;
        nodes[y][x].closed = false;
    }
}

    nodes[start_y][start_x].g = 0.0;
    nodes[start_y][start_x].f = chebyshev(start_x, start_y, cel_x, cel_y);
    nodes[start_y][start_x].open = true;

    static const int szomszed_x[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int szomszed_y[8] = {-1,  0,  1,-1, 1,-1, 0, 1};

    while (1) {
        int legjobb_x = -1;
        int legjobb_y = -1;
        double legjobb_f = 1e100;

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (nodes[y][x].open &&
                    !nodes[y][x].closed &&
                    nodes[y][x].f < legjobb_f) {
                    legjobb_f = nodes[y][x].f;
                    legjobb_x = x;
                    legjobb_y = y;
                }
            }
        }

        if (legjobb_x < 0)
            return -1;

        if (legjobb_x == cel_x && legjobb_y == cel_y) {
            return utvonalVisszaepites(nodes, start_x, start_y, cel_x, cel_y, utvonal, max_hossz);
        }

        nodes[legjobb_y][legjobb_x].closed = true;

        for (int szomszed_index = 0; szomszed_index < 8; szomszed_index++) {
            int kovetkezo_x = legjobb_x + szomszed_x[szomszed_index];
            int kovetkezo_y = legjobb_y + szomszed_y[szomszed_index];

            if (!inRange(kovetkezo_x, kovetkezo_y)) continue;
            if (mapv[kovetkezo_y][kovetkezo_x] == 1) continue;

            double uj_g = nodes[legjobb_y][legjobb_x].g + 1.0;

            if (uj_g < nodes[kovetkezo_y][kovetkezo_x].g) {
                nodes[kovetkezo_y][kovetkezo_x].g = uj_g;
                nodes[kovetkezo_y][kovetkezo_x].f =
                    uj_g + chebyshev(kovetkezo_x, kovetkezo_y, cel_x, cel_y);
                nodes[kovetkezo_y][kovetkezo_x].px = (int8_t)legjobb_x;
                nodes[kovetkezo_y][kovetkezo_x].py = (int8_t)legjobb_y;
                nodes[kovetkezo_y][kovetkezo_x].open = true;
            }
        }
    }
}

// -------------------- Battery / action helpers --------------------
//
// Main rule interpretation used here:
// - Movement and banyasz are forbidden if battery is insufficient for their action cost.
// - naptime standing is allowed even from 0 battery, because solar charging happens in the same half-hour.
// - Nighttime standing with battery 0 just leaves battery at 0.



// -------------------- Feasibility simulator --------------------
//
// Simulates whether a plan is feasible by mission end.
// Plan form:
//   1) move "out_lepesek" cells
//   2) optionally mine once
//   3) move "in_lepesek" cells
//
// It greedily chooses the locally best legal action that preserves eventual feasibility.
// This is used for:
// - target selection
// - safe-return guarantees
// - choosing a move v that keeps the plan feasible
//
static FeasResult utvonalTervSzimulacio(int kezdo_utes, double kezdo_akku, int oda_lepesek, bool kell_banyaszni, int vissza_lepesek, int max_utes) {
    int aktualis_utes = kezdo_utes;
    double aktualis_akku = kezdo_akku;

    while (aktualis_utes < max_utes) {

        if (oda_lepesek == 0 && !kell_banyaszni && vissza_lepesek == 0) {
            FeasResult eredmeny = { true, aktualis_utes, aktualis_akku };
            return eredmeny;
        }

        bool nappal = nap_van_ciklus(aktualis_utes);
        int hatralevo_utes = max_utes - (aktualis_utes + 1);

        bool van_ervenyes_lepes = false;
        double legjobb_akku = -1e100;
        int legjobb_haladas = -1;

        if (varakozasLehetseges(nappal, aktualis_akku)) {
            double uj_akku = varakozasAlkalmazasa(aktualis_akku, nappal);

            if (teljesUtMinUtesek(oda_lepesek, kell_banyaszni, vissza_lepesek) <= hatralevo_utes) {
                van_ervenyes_lepes = true;
                legjobb_akku = uj_akku;
                legjobb_haladas = 0;
            }
        }

        if (oda_lepesek > 0 || (!kell_banyaszni && vissza_lepesek > 0)) {
            int aktualis_szakasz_lepes = (oda_lepesek > 0) ? oda_lepesek : vissza_lepesek;

            for (int sebesseg = 1; sebesseg <= 3; sebesseg++) {
                if (sebesseg > aktualis_szakasz_lepes) continue;
                if (!mozgasLehetseges(aktualis_akku, sebesseg)) continue;

                int uj_oda_lepesek = oda_lepesek;
                int uj_vissza_lepesek = vissza_lepesek;

                if (oda_lepesek > 0) uj_oda_lepesek -= sebesseg;
                else                 uj_vissza_lepesek -= sebesseg;

                double uj_akku = mozgasAlkalmazasa(aktualis_akku, nappal, sebesseg);

                if (teljesUtMinUtesek(uj_oda_lepesek, kell_banyaszni, uj_vissza_lepesek) <= hatralevo_utes) {
                    if (!van_ervenyes_lepes ||
                        uj_akku > legjobb_akku ||
                        (fabs(uj_akku - legjobb_akku) < 1e-9 && sebesseg > legjobb_haladas)) {
                        van_ervenyes_lepes = true;
                        legjobb_akku = uj_akku;
                        legjobb_haladas = sebesseg;
                    }
                }
            }
        }

        if (oda_lepesek == 0 && kell_banyaszni && banyaszasLehetseges(aktualis_akku)) {
            double uj_akku = banyaszasAlkalmazasa(aktualis_akku, nappal);

            if (teljesUtMinUtesek(oda_lepesek, false, vissza_lepesek) <= hatralevo_utes) {
                if (!van_ervenyes_lepes ||
                    uj_akku > legjobb_akku ||
                    (fabs(uj_akku - legjobb_akku) < 1e-9 && 1 > legjobb_haladas)) {
                    van_ervenyes_lepes = true;
                    legjobb_akku = uj_akku;
                    legjobb_haladas = 1;
                }
            }
        }

        if (!van_ervenyes_lepes) {
            FeasResult eredmeny = { false, aktualis_utes, aktualis_akku };
            return eredmeny;
        }

        if (legjobb_haladas == 0) {
            aktualis_akku = varakozasAlkalmazasa(aktualis_akku, nappal);
        }
        else if (oda_lepesek > 0 || (!kell_banyaszni && vissza_lepesek > 0)) {
            int aktualis_szakasz_lepes = (oda_lepesek > 0) ? oda_lepesek : vissza_lepesek;

            if (legjobb_haladas <= aktualis_szakasz_lepes &&
                mozgasLehetseges(aktualis_akku, legjobb_haladas)) {

                if (oda_lepesek > 0) oda_lepesek -= legjobb_haladas;
                else                 vissza_lepesek -= legjobb_haladas;

                aktualis_akku = mozgasAlkalmazasa(aktualis_akku, nappal, legjobb_haladas);
            }
            else {
                FeasResult eredmeny = { false, aktualis_utes, aktualis_akku };
                return eredmeny;
            }
        }
        else if (oda_lepesek == 0 && kell_banyaszni && legjobb_haladas == 1) {
            if (!banyaszasLehetseges(aktualis_akku)) {
                FeasResult eredmeny = { false, aktualis_utes, aktualis_akku };
                return eredmeny;
            }

            kell_banyaszni = false;
            aktualis_akku = banyaszasAlkalmazasa(aktualis_akku, nappal);
        }
        else {
            FeasResult eredmeny = { false, aktualis_utes, aktualis_akku };
            return eredmeny;
        }

        aktualis_utes++;
    }

    FeasResult eredmeny = { false, aktualis_utes, aktualis_akku };
    return eredmeny; 
}


// -------------------- Safe target selection --------------------
static bool pick_target_safe(const Rover *r, const Mission *m, int *out_tx, int *out_ty) {
    int best_tx = -1, best_ty = -1;
    int best_trip_lepesek = 1e9;
    int best_to_lepesek = 1e9;
    double best_end_bat = -1.0;

    Pt tmp[MAX_PATH];

    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (mapv[y][x] != 2) continue;
        if (home_dist[y][x] == 255) continue;

        int len = astarAlg(r->x, r->y, x, y, tmp, MAX_PATH);
        if (len < 0) continue;

        int lepesek_to = len - 1;
        int lepesek_home = (int)home_dist[y][x];

        FeasResult fr = utvonalTervSzimulacio(r->utes, r->bat,
                                           lepesek_to, true, lepesek_home,
                                           m->total_utess);
        if (!fr.ok) continue;

        int trip_lepesek = lepesek_to + lepesek_home;

        // All minerals equal: prefer shorter safe round-trip.
        // Tie-break by closer target, then more remaining battery.
        if (trip_lepesek < best_trip_lepesek ||
            (trip_lepesek == best_trip_lepesek && lepesek_to < best_to_lepesek) ||
            (trip_lepesek == best_trip_lepesek && lepesek_to == best_to_lepesek && fr.end_bat > best_end_bat))
        {
            best_trip_lepesek = trip_lepesek;
            best_to_lepesek = lepesek_to;
            best_end_bat = fr.end_bat;
            best_tx = x;
            best_ty = y;
        }
    }

    if (best_tx < 0) return false;
    *out_tx = best_tx;
    *out_ty = best_ty;
    return true;
}

// -------------------- Choose current safe move v --------------------
static int choose_safe_move_v(const Rover *r, const Mission *m,
                                  const Pt *path, int lepesek_to_goal,
                                  bool target_is_mineral, int target_x, int target_y)
{
    bool nap = nap_van_ciklus(r->utes);
    int order[3];

    // Strategy:
    // - naptime: prefer v=2, then v=1, then v=3
    // - nighttime: prefer v=1, then v=2, then v=3
    if (nap) {
        order[0] = 2; order[1] = 1; order[2] = 3;
    } else {
        order[0] = 1; order[1] = 2; order[2] = 3;
    }

    // If time is exactly tight, try higher v first.
    int rem_utess = m->total_utess - r->utes;
    int min_need_now;
    if (target_is_mineral) {
        min_need_now = teljesUtMinUtesek(lepesek_to_goal, true, (int)home_dist[target_y][target_x]);
    } else {
        min_need_now = teljesUtMinUtesek(lepesek_to_goal, false, 0);
    }
    if (rem_utess <= min_need_now) {
        order[0] = 3; order[1] = 2; order[2] = 1;
    }

    for (int i = 0; i < 3; i++) {
        int v = order[i];
        if (v > lepesek_to_goal) continue;
        if (!mozoghat(r->bat, v)) continue;

        double bat2 = mozogFunc(r->bat, nap, v);

        if (target_is_mineral) {
            int rem_to_target = lepesek_to_goal - v;
            int home_from_target = (int)home_dist[target_y][target_x];
            FeasResult fr = utvonalTervSzimulacio(r->utes + 1, bat2,
                                               rem_to_target, true, home_from_target,
                                               m->total_utess);
            if (fr.ok) return v;
        } else {
            int rem_home = lepesek_to_goal - v;
            FeasResult fr = utvonalTervSzimulacio(r->utes + 1, bat2,
                                               rem_home, false, 0,
                                               m->total_utess);
            if (fr.ok) return v;
        }
    }

    return 0;
}

// -------------------- Simulation step --------------------
// -------------------- Simulation step --------------------
static void sim_step(Rover *r, const Mission *m, FILE *log_csv, FILE *log_ai) {
    bool nap = nap_van_ciklus(r->utes);
    const char *tp = nap ? "nap" : "NIGHT";
    float et = napCiklus(r->utes);

    // 0) CRITICAL SAFETY CHECK
    int dist_to_home = (int)home_dist[r->y][r->x];
    
    // If we can't find home in the distance map, something is wrong with BFS
    if (dist_to_home == 255) {
        goto force_home; 
    }

    // How many half-hours do we need at max speed (v=3)?
    int time_needed_to_return = (dist_to_home + 2) / 3; 
    int time_left = m->total_utess - r->utes;

    // If time is running out, jump straight to movement logic
    if (time_left <= time_needed_to_return + 1) {
        goto force_home; 
    }

    // 1) If rover is on a mineral, mine only if banyasz now still preserves guaranteed return.
    if (mapv[r->y][r->x] == 2 && home_dist[r->y][r->x] != 255) {
        int lepesek_home = (int)home_dist[r->y][r->x];
        if (can_do_mine(r->bat)) {
            double bat2 = apply_mine(r->bat, nap);
            FeasResult fr = utvonalTervSzimulacio(r->utes + 1, bat2,
                                               0, false, lepesek_home,
                                               m->total_utess);
            if (fr.ok) {
                char mineral = mapc[r->y][r->x];
                if (mineral == 'G') r->green++;
                else if (mineral == 'Y') r->yellow++;
                else if (mineral == 'B') r->blue++;

                mapv[r->y][r->x] = 0;
                mapc[r->y][r->x] = '.';
                r->bat = bat2;

                log_gui_line(log_csv, r->utes, r, 0, r->moved_cells, tp, et, "DIGGING");
                log_gui_line(log_ai,  r->utes, r, 0, r->moved_cells, tp, et, "DIGGING");
                r->utes++;
                return;
            }
        }
    }

    // 2) Try a safe mineral target first.
    int tx = -1, ty = -1;
    bool have_target = pick_target_safe(r, m, &tx, &ty);

    if (have_target) {
        Pt path[MAX_PATH];
        int plen = astarAlg(r->x, r->y, tx, ty, path, MAX_PATH);

        if (plen >= 0) {
            int lepesek_to_goal = plen - 1;
            if (lepesek_to_goal > 0) {
                int v = choose_safe_move_v(r, m, path, lepesek_to_goal, true, tx, ty);
                if (v > 0) {
                    Pt dest = path[v];
                    r->x = dest.x;
                    r->y = dest.y;
                    r->moved_cells += v;
                    r->bat = mozogFunc(r->bat, nap, v);

                    log_gui_line(log_csv, r->utes, r, v, r->moved_cells, tp, et, "MOVING");
                    log_gui_line(log_ai,  r->utes, r, v, r->moved_cells, tp, et, "MOVING");
                    r->utes++;
                    return;
                }
            }
        }
    }

    // 3) Home Return Priority
force_home: 
    if (r->x != m->sx || r->y != m->sy) {
        Pt home_path[MAX_PATH];
        int home_len = astarAlg(r->x, r->y, m->sx, m->sy, home_path, MAX_PATH);

        if (home_len >= 0) {
            int lepesek_home = home_len - 1;
            if (lepesek_home > 0) {
                int vhome = choose_safe_move_v(r, m, home_path, lepesek_home, false, m->sx, m->sy);
                if (vhome > 0) {
                    Pt dest = home_path[vhome];
                    r->x = dest.x;
                    r->y = dest.y;
                    r->moved_cells += vhome;
                    r->bat = mozogFunc(r->bat, nap, vhome);

                    log_gui_line(log_csv, r->utes, r, vhome, r->moved_cells, tp, et, "MOVING");
                    log_gui_line(log_ai,  r->utes, r, vhome, r->moved_cells, tp, et, "MOVING");
                    r->utes++;
                    return;
                }
            }
        }
    }

    // 4) Default Action: Standing
    r->bat = apply_stand(r->bat, nap);
    log_gui_line(log_csv, r->utes, r, 0, r->moved_cells, tp, et, "STANDING");
    log_gui_line(log_ai,  r->utes, r, 0, r->moved_cells, tp, et, "STANDING");
    r->utes++;
}

// -------------------- Map reading --------------------
static int load_map_csv(const char *fname, Mission *m) {
    FILE *f = fopen(fname, "r");
    if (!f) return 0;

    char buf[2048];
    bool foundS = false;

    for (int y = 0; y < H; y++) {
        if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }

        char *t = strtok(buf, ",\r\n");
        for (int x = 0; x < W; x++) {
            if (!t) { fclose(f); return 0; }
            char c = t[0];
            mapc[y][x] = c;

            if (c == 'S') {
                m->sx = (int8_t)x;
                m->sy = (int8_t)y;
                mapv[y][x] = 0;
                foundS = true;
            } else if (c == '#') {
                mapv[y][x] = 1;
            } else if (c == 'B' || c == 'Y' || c == 'G') {
                mapv[y][x] = 2;
            } else if (c == '.') {
                mapv[y][x] = 0;
            } else {
                // Unknown cell treated as free surface for robustness.
                mapv[y][x] = 0;
                mapc[y][x] = '.';
            }

            t = strtok(NULL, ",\r\n");
        }
    }

    fclose(f);
    return foundS ? 1 : 0;
}

// -------------------- main --------------------
int main(void) {
    Mission m;
    int hours;

    printf("Küldetés hossza (óra, >=24): ");
    if (scanf("%d", &hours) != 1) return 1;
    if (hours < 24) hours = 24;

    m.total_utess = hours * UTES_PER_ORA;

    if (!load_map_csv("mars_map_50x50.csv", &m)) {
        printf("Hiba: nem tudtam beolvasni a mars_map_50x50.csv térképet (vagy nincs 'S').\n");
        return 1;
    }

    cellatolHazaTav(m.sx, m.sy);

    Rover r;
    r.x = m.sx; r.y = m.sy;
    r.bat = 100.0;
    r.green = r.yellow = r.blue = 0;
    r.utes = 0;
    r.moved_cells = 0;

    FILE *log_csv = fopen("rover_log.csv", "w");
    FILE *log_ai  = fopen("ai_route.txt", "w");
    if (!log_csv || !log_ai) {
        printf("Hiba: nem tudok log fájl(oka)t megnyitni írásra.\n");
        if (log_csv) fclose(log_csv);
        if (log_ai) fclose(log_ai);
        return 1;
    }

    // Core fix: simulation runs until time expires, not until battery hits zero.
    log_gui_line(log_csv, -1, &r, 0, r.moved_cells, "nap", napCiklus(0), "STANDING");
    log_gui_line(log_ai,  -1, &r, 0, r.moved_cells, "nap", napCiklus(0), "STANDING");

    while (r.utes < m.total_utess) {
        sim_step(&r, &m, log_csv, log_ai);
    }

    fclose(log_csv);
    fclose(log_ai);

    bool success = (r.x == m.sx && r.y == m.sy);
    printf("\n--- Küldetés vége ---\n");
    printf("Eredmény: %s\n", success ? "SIKERES (hazatért)" : "NEM (nem ért haza)");
    printf("Ásványok: total=%d (G=%d, Y=%d, B=%d)\n",
           r.green + r.yellow + r.blue, r.green, r.yellow, r.blue);
    printf("Akkumulátor: %.1f%%\n", r.bat);
    printf("Kimenetek: rover_log.csv  +  ai_route.txt\n");

    return 0;
}