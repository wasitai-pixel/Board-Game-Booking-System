/*
 * ระบบจองบอร์ดเกมห้องสมุด (C11, console)
 * คอมไพล์:  gcc -O2 -std=c11 boardgame.c -o boardgame -lm
 * Windows:  chcp 65001  ก่อนรัน เพื่อแสดงภาษาไทย
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#define NG 10
#define NT 6
#define MAXB 200
#define LIMIT (15 * 60 + 30) /* คืนบอร์ดเกมไม่เกิน 15:30 น. */

typedef struct { char code[8], name[128], en[48]; int pair, count; } Game;
typedef struct { int id; char name[64]; int size, game, table, time, st, active; } Booking; /* st: 1=ถูกจอง 2=กำลังใช้งาน */
typedef struct { char name[64]; int big; } Table;
typedef struct { Booking **a; int n; } Heap;

static const char *ST[] = {"ว่าง", "ถูกจอง", "กำลังใช้งาน"};

static Game G[NG] = {
    {"AB001", "เบาะแสมรณะ", "Dying Message", 1, 0},
    {"AB002", "เบาะแสสุดท้าย", "Last Message", 1, 0},
    {"AB003", "เกมล่าปริศนามนุษย์หมาป่า Werewolf Extreme และ 1 คืน", "Werewolf Extreme", 2, 0},
    {"AB004", "ปริศนาเกมล่ามนุษย์หมาป่ารุ่งอรุณ", "One Night Daybreak", 2, 0},
    {"AB005", "ทาโก้ แมว แพะ ชีส พิซซ่า", "Taco Cat Goat Cheese Pizza", 3, 0},
    {"AB006", "แมลงเม่าจอมโกง", "Cheating Moth", 3, 0},
    {"AB007", "โกทาวน์", "Go Town", 4, 0},
    {"AB008", "เกมต่อรถตะลุย", "New York", 4, 0},
    {"AB009", "บ้านนี้ขาย!", "For Sale", 5, 0},
    {"AB010", "เกมค้าเพชร", "Splendor", 5, 0}};

static Table T[NT] = {{"โต๊ะใหญ่ 1 (7-10 คน)", 1}, {"โต๊ะใหญ่ 2 (7-10 คน)", 1}, {"โต๊ะเล็ก 1 (2-6 คน)", 0},
                      {"โต๊ะเล็ก 2 (2-6 คน)", 0},   {"โต๊ะเล็ก 3 (2-6 คน)", 0},   {"โต๊ะเล็ก 4 (2-6 คน)", 0}};

static Booking B[MAXB];
static Booking *heapStore[MAXB];
static Heap H = {heapStore, 0};
static int seq = 0;

/* ---------- Max Heap (priority = ขนาดกลุ่มใหญ่กว่าก่อน, เท่ากันมาก่อนได้ก่อน) ---------- */
static long pr(const Booking *b) { return (long)b->size * 10000000L - b->id; }

static void hpush(Heap *h, Booking *x) {
    int i = h->n++;
    h->a[i] = x;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (pr(h->a[p]) >= pr(h->a[i])) break;
        Booking *t = h->a[p]; h->a[p] = h->a[i]; h->a[i] = t;
        i = p;
    }
}

static Booking *hpop(Heap *h) {
    if (!h->n) return NULL;
    Booking *top = h->a[0];
    h->a[0] = h->a[--h->n];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->n && pr(h->a[l]) > pr(h->a[m])) m = l;
        if (r < h->n && pr(h->a[r]) > pr(h->a[m])) m = r;
        if (m == i) break;
        Booking *t = h->a[m]; h->a[m] = h->a[i]; h->a[i] = t;
        i = m;
    }
    return top;
}

static void hremove(Heap *h, Booking *b) {
    Booking *tmp[MAXB]; int k = 0; Booking *x;
    while ((x = hpop(h))) if (x != b) tmp[k++] = x;
    while (k) hpush(h, tmp[--k]);
}

/* ---------- My search(): Sequential Search O(n) ---------- */
static int ci_eq(const char *a, const char *b) {
    while (*a && *b) { if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0; a++; b++; }
    return *a == *b;
}
static int ci_has(const char *h, const char *n) {
    size_t m = strlen(n);
    if (!m) return 1;
    for (; *h; h++) {
        size_t i = 0;
        while (i < m && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)n[i])) i++;
        if (i == m) return 1;
    }
    return 0;
}
/* ค้นด้วยรหัส (ตรงทั้งหมด) หรือชื่อ (มีคำนั้นอยู่) คืนจำนวนที่พบ และเก็บ index ใน out */
int mySearch(const Game *L, int n, const char *key, int *out) {
    int c = 0;
    for (int i = 0; i < n; i++)
        if (ci_eq(L[i].code, key) || ci_has(L[i].name, key) || ci_has(L[i].en, key)) out[c++] = i;
    return c;
}

/* ---------- สถานะเกม/โต๊ะ ---------- */
static int gstat(int g) {
    for (int i = 0; i < MAXB; i++) if (B[i].active && B[i].game == g) return B[i].st;
    return 0;
}
static Booking *tbook(int t) {
    for (int i = 0; i < MAXB; i++) if (B[i].active && B[i].table == t) return &B[i];
    return NULL;
}
static int findT(const Booking *b) {
    int wantBig = (b->size >= 7); /* กลุ่มใหญ่ -> โต๊ะใหญ่เท่านั้น, กลุ่มเล็ก -> โต๊ะเล็กเท่านั้น */
    for (int t = 0; t < NT; t++)
        if (T[t].big == wantBig && !tbook(t)) return t;
    return -1;
}
/* จัดโต๊ะให้กลุ่มใหญ่สุดที่เข้าได้ก่อน */
static void assign(void) {
    int moved = 1;
    while (moved) {
        moved = 0;
        Booking *tmp[MAXB]; int k = 0; Booking *b;
        while ((b = hpop(&H))) {
            int t = findT(b);
            if (t >= 0) { b->table = t; moved = 1; break; }
            tmp[k++] = b;
        }
        while (k) hpush(&H, tmp[--k]);
    }
}

/* ---------- อินพุต ---------- */
static void readline(const char *prompt, char *buf, int len) {
    printf("%s", prompt); fflush(stdout);
    if (!fgets(buf, len, stdin)) { buf[0] = 0; return; }
    buf[strcspn(buf, "\r\n")] = 0;
}
static int nowMin(void) { time_t t = time(NULL); struct tm *l = localtime(&t); return l->tm_hour * 60 + l->tm_min; }
static int parseTime(const char *s) {
    int h, m;
    if (!*s) return nowMin();
    if (sscanf(s, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) return h * 60 + m;
    return -1;
}
static int findCode(const char *code) { for (int i = 0; i < NG; i++) if (ci_eq(G[i].code, code)) return i; return -1; }

/* ---------- แสดงผล ---------- */
static void showGames(void) {
    printf("\n== สถานะบอร์ดเกม ==\n");
    for (int i = 0; i < NG; i++) {
        int s = gstat(i);
        printf("%s | %-28s | %s", G[i].code, G[i].en, ST[s]);
        if (s) for (int j = 0; j < MAXB; j++) if (B[j].active && B[j].game == i) printf(" | ผู้จอง %s เวลา %02d:%02d", B[j].name, B[j].time / 60, B[j].time % 60);
        printf("\n");
    }
}
static void showTables(void) {
    printf("\n== สถานะโต๊ะ ==\n");
    for (int t = 0; t < NT; t++) {
        Booking *b = tbook(t);
        printf("%-24s | %s", T[t].name, b ? ST[b->st] : ST[0]);
        if (b) printf(" | %s (%d คน) %s เวลา %02d:%02d", b->name, b->size, G[b->game].code, b->time / 60, b->time % 60);
        printf("\n");
    }
}
static int cmpPr(const void *a, const void *b) { return pr(*(Booking *const *)b) > pr(*(Booking *const *)a) ? 1 : -1; }
static void showQueue(void) {
    printf("\n== คิวรอโต๊ะ (Max Heap) ==\n");
    if (!H.n) { printf("ไม่มีคิว\n"); return; }
    Booking *c[MAXB];
    memcpy(c, H.a, sizeof(Booking *) * H.n);
    qsort(c, H.n, sizeof(Booking *), cmpPr);
    for (int i = 0; i < H.n; i++) printf("%d. %s | %d คน | %s | %02d:%02d\n", i + 1, c[i]->name, c[i]->size, G[c[i]->game].code, c[i]->time / 60, c[i]->time % 60);
}
static int showBookings(void) {
    int n = 0;
    printf("\n== รายการจอง ==\n");
    for (int i = 0; i < MAXB; i++) if (B[i].active) {
        n++;
        printf("#%d %s (%d คน) | %s %s | %s | เวลา %02d:%02d | %s\n", B[i].id, B[i].name, B[i].size, G[B[i].game].code, G[B[i].game].en,
               B[i].table >= 0 ? T[B[i].table].name : "รอโต๊ะ", B[i].time / 60, B[i].time % 60, ST[B[i].st]);
    }
    if (!n) printf("ยังไม่มีการจอง\n");
    return n;
}
static int cmpGame(const void *a, const void *b) { return ((const Game *)b)->count - ((const Game *)a)->count; }
static void showStats(void) {
    Game c[NG];
    memcpy(c, G, sizeof G);
    qsort(c, NG, sizeof(Game), cmpGame);
    printf("\n== สถิติการยืม (มาก -> น้อย) ==\n");
    for (int i = 0; i < NG; i++) printf("%2d. %s %-28s %d ครั้ง\n", i + 1, c[i].code, c[i].en, c[i].count);
}

/* ---------- การจอง ---------- */
static void book(int g, const char *name, int size, int tm) {
    if (gstat(g)) {
        int p = -1;
        for (int i = 0; i < NG; i++) if (G[i].pair == G[g].pair && i != g) p = i;
        printf("\n%s %s %s\n", G[g].code, G[g].en, ST[gstat(g)]);
        char ans[32];
        if (p >= 0 && !gstat(p)) {
            printf("แนะนำเกมใกล้เคียง: %s %s (ว่าง)\n", G[p].code, G[p].en);
            readline("จองเกมนี้แทนหรือไม่ (y/n): ", ans, sizeof ans);
            if (tolower((unsigned char)ans[0]) != 'y') return;
            g = p;
        } else {
            if (p >= 0) printf("เกมใกล้เคียง %s %s ก็ไม่ว่าง\n", G[p].code, G[p].en);
            int any = 0;
            printf("เกมที่ว่างตอนนี้:\n");
            for (int i = 0; i < NG; i++) if (!gstat(i)) { printf("  %s %s\n", G[i].code, G[i].en); any = 1; }
            if (!any) { printf("  ไม่มีเกมว่าง\n"); return; }
            readline("พิมพ์รหัสเกมที่ต้องการ (เว้นว่าง = ยกเลิก): ", ans, sizeof ans);
            int k = findCode(ans);
            if (k < 0 || gstat(k)) { printf("ยกเลิกการจอง\n"); return; }
            g = k;
        }
    }
    Booking *b = NULL;
    for (int i = 0; i < MAXB; i++) if (!B[i].active) { b = &B[i]; break; }
    if (!b) { printf("รายการจองเต็ม\n"); return; }
    memset(b, 0, sizeof *b);
    b->id = ++seq; b->size = size; b->game = g; b->table = -1; b->time = tm; b->st = 1; b->active = 1;
    strncpy(b->name, name, sizeof b->name - 1);
    hpush(&H, b);
    assign();
    if (b->table >= 0) printf("จอง %s สำเร็จ ได้%s (รหัสจอง #%d) กรุณายืนยันการจอง\n", G[g].en, T[b->table].name, b->id);
    else printf("จอง %s แล้ว แต่โต๊ะเต็ม อยู่ในคิวรอ (รหัสจอง #%d)\n", G[g].en, b->id);
}

static void actionSearchBook(void) {
    char name[64], s[64], key[96];
    readline("ชื่อผู้จอง: ", name, sizeof name);
    if (!*name) { printf("ต้องระบุชื่อผู้จอง\n"); return; }
    readline("จำนวนคน (2-10): ", s, sizeof s);
    int size = atoi(s);
    if (size < 2 || size > 10) { printf("จำนวนคนต้อง 2-10\n"); return; }
    readline("เวลาจอง HH:MM (เว้นว่าง = ปัจจุบัน): ", s, sizeof s);
    int tm = parseTime(s);
    if (tm < 0) { printf("รูปแบบเวลาไม่ถูกต้อง\n"); return; }
    if (tm >= LIMIT) { printf("เลยเวลา 15:30 น. แล้ว ไม่สามารถจองได้\n"); return; }
    readline("ค้นหาด้วยรหัสหรือชื่อเกม (เว้นว่าง = ทั้งหมด): ", key, sizeof key);
    int out[NG], n = mySearch(G, NG, key, out);
    if (!n) { printf("ไม่พบเกม\n"); return; }
    for (int i = 0; i < n; i++) printf("  %s %s (%s) - %s\n", G[out[i]].code, G[out[i]].name, G[out[i]].en, ST[gstat(out[i])]);
    int g = out[0];
    if (n > 1) {
        readline("พิมพ์รหัสเกมที่ต้องการจอง: ", s, sizeof s);
        g = findCode(s);
        if (g < 0) { printf("ไม่พบรหัสเกม\n"); return; }
    }
    book(g, name, size, tm);
}

/* mode: 1=ยืนยัน 2=ยกเลิก 3=คืนเกม */
static void actionManage(int mode) {
    if (!showBookings()) return;
    char s[64], name[64];
    readline("รหัสการจอง (#): ", s, sizeof s);
    int id = atoi(s);
    Booking *b = NULL;
    for (int i = 0; i < MAXB; i++) if (B[i].active && B[i].id == id) b = &B[i];
    if (!b) { printf("ไม่พบรหัสการจอง\n"); return; }
    readline("พิมพ์ชื่อผู้จอง: ", name, sizeof name);
    if (!ci_eq(name, b->name)) { printf("ชื่อผู้จองไม่ตรงกับการจอง\n"); return; }
    if (mode == 1) {
        if (b->st != 1) { printf("รายการนี้ยืนยันไปแล้ว\n"); return; }
        if (b->table < 0) { printf("ยังไม่ได้โต๊ะ ยืนยันไม่ได้\n"); return; }
        b->st = 2; G[b->game].count++;
        printf("ยืนยันแล้ว เริ่มใช้งาน (คืนก่อน 15:30 น.)\n");
    } else if (mode == 2) {
        if (b->st != 1) { printf("กำลังใช้งานอยู่ ใช้เมนูคืนเกมแทน\n"); return; }
        if (b->table < 0) hremove(&H, b);
        b->active = 0; assign();
        printf("ยกเลิกการจองแล้ว\n");
    } else {
        if (b->st != 2) { printf("ยังไม่ได้ยืนยัน ใช้เมนูยกเลิกแทน\n"); return; }
        b->active = 0; assign();
        printf("คืนเกมและโต๊ะแล้ว\n");
    }
}

/* ============================== Hash Table ============================== */
typedef struct {
    char **keys;
    int *vals;
    int cap;
} Hash;

static char *dup_str(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static unsigned long djb2(const char *s) {
    unsigned long h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

static void hash_init(Hash *h, int n) {
    int cap = 16;
    while (cap < n * 2) cap <<= 1;
    h->cap = cap;
    h->keys = calloc(cap, sizeof(char *));
    h->vals = calloc(cap, sizeof(int));
}

static void hash_put(Hash *h, const char *key, int val) {
    int i = (int)(djb2(key) & (h->cap - 1));
    while (h->keys[i] && strcmp(h->keys[i], key) != 0) i = (i + 1) & (h->cap - 1);
    if (!h->keys[i]) h->keys[i] = dup_str(key);
    h->vals[i] = val;
}

static int hash_get(const Hash *h, const char *key) {
    int i = (int)(djb2(key) & (h->cap - 1));
    while (h->keys[i]) {
        if (strcmp(h->keys[i], key) == 0) return h->vals[i];
        i = (i + 1) & (h->cap - 1);
    }
    return -1;
}

static void hash_free(Hash *h) {
    for (int i = 0; i < h->cap; i++) free(h->keys[i]);
    free(h->keys);
    free(h->vals);
}

/* ============================== ทดสอบ Big-O ============================== */
static double wall_seconds(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void benchmark(void) {
    const int sizes[3] = {1000, 10000, 100000};
    const int rounds = 1000;
    double totals[3];
    puts("==========================================");
    puts("   เริ่มการทดสอบประสิทธิภาพและเทียบ Big-O");
    puts("==========================================");
    for (int s = 0; s < 3; s++) {
        int n = sizes[s];
        printf("\nกำลังสร้างข้อมูลจำลองการจอง n = %d รายการ...\n", n);
        Hash h;
        hash_init(&h, n);
        char key[48];
        for (int i = 0; i < n; i++) {
            snprintf(key, sizeof key, "ผู้จอง%d", i);
            hash_put(&h, key, i);
        }
        volatile int found = 0;
        char miss[64][48];                          /* ชื่อที่ไม่มีในระบบ 64 ชื่อ เฉลี่ยความยาว probe */
        for (int i = 0; i < 64; i++) snprintf(miss[i], sizeof miss[i], "ไม่มีชื่อนี้%d", i);
        double total = 1e9;
        for (int trial = 0; trial < 31; trial++) {   /* รอบแรก warm-up แล้วเลือกรอบที่เร็วสุดเพื่อลด noise */
            double t0 = wall_seconds();
            for (int r = 0; r < rounds; r++)
                if (hash_get(&h, miss[r & 63]) >= 0) found++;
            double dt = wall_seconds() - t0;
            if (trial > 0 && dt < total) total = dt;
        }
        totals[s] = total;
        printf("n = %d\n", n);
        printf("จำนวนรอบที่วัด   : %d รอบ\n", rounds);
        printf("เวลารวม          : %.6f วินาที\n", total);
        printf("เวลาเฉลี่ยต่อครั้ง : %.6f มิลลิวินาที\n", total / rounds * 1000.0);
        printf("ผลการค้นหา       : %d รายการ\n", found);
        hash_free(&h);
    }
    puts("\n== วิเคราะห์เทียบ Big-O ==");
    double ksum = 0;
    for (int i = 1; i < 3; i++) {
        double ratio = totals[i - 1] > 0 ? totals[i] / totals[i - 1] : 1.0;
        double k = ratio > 0 ? log(ratio) / log((double)sizes[i] / sizes[i - 1]) : 0.0;
        ksum += k;
        printf("%d  -> %d  : อัตราส่วนเวลา %.2f เท่า, k = %.2f\n", sizes[i - 1], sizes[i], ratio, k);
    }
    double avg_k = ksum / 2.0;
    const char *res = avg_k < 0.3 ? "O(log n) หรือ O(1)"
                    : avg_k < 0.7 ? "O(sqrt n)"
                    : avg_k < 1.3 ? "O(n)" : "O(n log n) หรือสูงกว่า";
    printf("ผลประเมิน Big-O จากเวลาจริง : %s\n", res);
    puts("==========================================");
}

int main(void) {
    for (;;) {
        printf("\n===== ระบบจองบอร์ดเกมห้องสมุด (คืนก่อน 15:30 น.) =====\n"
               "1. ค้นหา/จองเกม\n2. สถานะโต๊ะ\n3. สถานะบอร์ดเกม\n4. ยืนยันการจอง\n5. ยกเลิกการจอง\n"
               "6. คืนเกม\n7. สถิติการยืม\n8. คิวรอโต๊ะ\n9. ทดสอบ Big-O (n=1000,10000,100000)\n0. ออก\n");
        char c[16];
        readline("เลือก: ", c, sizeof c);
        switch (atoi(c)) {
            case 1: actionSearchBook(); break;
            case 2: showTables(); break;
            case 3: showGames(); break;
            case 4: actionManage(1); break;
            case 5: actionManage(2); break;
            case 6: actionManage(3); break;
            case 7: showStats(); break;
            case 8: showQueue(); break;
            case 9: benchmark(); break;
            case 0: return 0;
            default: if (!*c && feof(stdin)) return 0; printf("เมนูไม่ถูกต้อง\n");
        }
    }
}
