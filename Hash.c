/* =====================================================================
   timing_template.c  —  โปรแกรมต้นแบบสำหรับวัดเวลาการค้นหา
   รายวิชา 01204212 แบบชนิดข้อมูลนามธรรมและการแก้ปัญหา
   ใช้ประกอบกิจกรรมนำเสนอโครงงาน วันที่ 5 ตุลาคม 2569

   วิธีคอมไพล์ :  gcc timing_template.c -o timing
   วิธีรัน      :  ./timing data_1000.txt targets_1000.txt
                  (บน Windows ใช้  timing.exe data_1000.txt targets_1000.txt)

   ให้แต่ละกลุ่มแทนที่ฟังก์ชัน MySearch ด้วยอัลกอริทึมการค้นหาของกลุ่มตนเอง
   ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* ตัวจับเวลาละเอียดสูง (clock() บน Windows ละเอียดแค่ ~1 ms) */
double NowMs(void)
{
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

#define MAXN   100000
#define REPEAT 1000      /* จำนวนรอบที่วนค้นหาซ้ำ ใช้ค่าเดียวกันทุกการทดลอง */

int data[MAXN];          /* ข้อมูลตามลำดับในไฟล์ (ยังไม่เรียง) */
int sorted_data[MAXN];   /* สำเนาที่เรียงแล้ว สำหรับ Binary Search */
int n;

/* ---------- อ่านไฟล์ข้อมูล: บรรทัดแรกคือ n จากนั้นตามด้วยค่า n ค่า ---------- */
int LoadData(const char *filename, int arr[])
{
    FILE *fp = fopen(filename, "r");
    int  count, i;

    if (fp == NULL) {
        printf("เปิดไฟล์ %s ไม่ได้\n", filename);
        exit(1);
    }
    fscanf(fp, "%d", &count);
    for (i = 0; i < count; i++)
        fscanf(fp, "%d", &arr[i]);
    fclose(fp);
    return count;
}

/* ---------- ใช้เรียงข้อมูลก่อนทำ Binary Search (ไม่นับเวลาส่วนนี้) ---------- */
int CompareInt(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

/* ---------- อัลกอริทึมที่ 1: Sequential Search ---------- */
int SequentialSearch(int arr[], int size, int target)
{
    int i;
    for (i = 0; i < size; i++)
        if (arr[i] == target)
            return i;            /* คืนตำแหน่งที่พบ */
    return -1;                   /* ไม่พบ */
}

/* ---------- อัลกอริทึมที่ 2: Binary Search (ข้อมูลต้องเรียงแล้ว) ---------- */
int BinarySearch(int arr[], int size, int target)
{
    int first = 0, last = size - 1, mid;

    while (first <= last) {
        mid = (first + last) / 2;
        if (target > arr[mid])      first = mid + 1;
        else if (target < arr[mid]) last  = mid - 1;
        else                        return mid;
    }
    return -1;
}

/* =====================================================================
   จุดที่แต่ละกลุ่มต้องแก้ไข
   แทนที่เนื้อในของฟังก์ชันนี้ด้วยอัลกอริทึมการค้นหาของกลุ่มตนเอง
   เช่น การค้นหาใน BST, AVL Tree, Hash Table หรือวิธีอื่นที่ออกแบบไว้
   ===================================================================== */
/* ---------- Hash Table (chaining) ของกลุ่ม ---------- */
#define HSIZE 262144          /* ใหญ่กว่า 2 เท่าของ MAXN */
int hhead[HSIZE];             /* ตำแหน่งตัวแรกในแต่ละช่อง, -1 = ว่าง */
int hnext[MAXN];              /* ตำแหน่งตัวถัดไปในช่องเดียวกัน */

void BuildHash(int arr[], int size)   /* เตรียมข้อมูล ไม่นับเวลา */
{
    int i, h;
    for (i = 0; i < HSIZE; i++) hhead[i] = -1;
    for (i = 0; i < size; i++) {
        h = arr[i] % HSIZE;
        hnext[i] = hhead[h];
        hhead[h] = i;
    }
}

int MySearch(int arr[], int size, int target)
{
    int i;
    if (target < 0) return -1;
    for (i = hhead[target % HSIZE]; i != -1; i = hnext[i])
        if (arr[i] == target)
            return i;
    return -1;
}

/* ---------- พิมพ์ข้อความแล้วเติมช่องว่างให้ครบความกว้างคอลัมน์ ---------- */
void PrintCell(const char *text, int shown_width, int col_width)
{
    int k;
    printf("%s", text);
    for (k = shown_width; k < col_width; k++) putchar(' ');
}

/* ---------- วัดเวลาเฉลี่ยต่อการค้นหาหนึ่งครั้ง หน่วยเป็นมิลลิวินาที ---------- */
double MeasureMillisec(int (*SearchFunc)(int[], int, int),
                       int arr[], int size, int targets[], int tcount)
{
    double  start, end;
    int     r, i, result = 0;

    start = NowMs();                                  /* เริ่มจับเวลา */
    for (r = 0; r < REPEAT; r++)
        for (i = 0; i < tcount; i++)
            result += SearchFunc(arr, size, targets[i]);
    end = NowMs();                                    /* หยุดจับเวลา */

    if (result == -99999999) printf(" ");  /* กันคอมไพเลอร์ตัดโค้ดทิ้ง */

    return (end - start) / (REPEAT * tcount);         /* เฉลี่ยต่อหนึ่งครั้ง (ms) */
}

int main(int argc, char *argv[])
{
    int    targets[100], tcount, i;
    double ms_seq, ms_bin, ms_my;

    if (argc < 3) {
        printf("วิธีใช้: %s <ไฟล์ข้อมูล> <ไฟล์ค่าที่ค้นหา>\n", argv[0]);
        printf("ตัวอย่าง: %s data_1000.txt targets_1000.txt\n", argv[0]);
        return 1;
    }

    /* ---- เตรียมข้อมูล (ไม่จับเวลาส่วนนี้) ---- */
    n      = LoadData(argv[1], data);
    tcount = LoadData(argv[2], targets);

    for (i = 0; i < n; i++) sorted_data[i] = data[i];
    qsort(sorted_data, n, sizeof(int), CompareInt);
    BuildHash(data, n);   /* สร้างตารางแฮช (ไม่จับเวลาส่วนนี้) */

    printf("=====================================================\n");
    printf(" ไฟล์ข้อมูล      : %s\n", argv[1]);
    printf(" จำนวนข้อมูล n   : %d\n", n);
    printf(" จำนวนค่าที่ค้นหา : %d\n", tcount);
    printf(" จำนวนรอบที่วัด  : %d รอบต่อค่า\n", REPEAT);
    printf("=====================================================\n");

    /* ---- ตรวจความถูกต้องก่อนวัดเวลา ---- */
    printf("\n[ ตรวจความถูกต้องของผลการค้นหา ]\n");
    /* %-10s นับเป็น "ไบต์" แต่ภาษาไทยกว้างไม่เท่าไบต์ คอลัมน์เลยเบี้ยว
       จึงเติมช่องว่างเองตามความกว้างที่แสดงจริงบนหน้าจอ */
    PrintCell("ค่าที่ค้นหา", 7, 14);
    PrintCell("Sequential", 10, 12);
    PrintCell("Binary", 6, 12);
    printf("Hash\n");
    for (i = 0; i < tcount; i++) {
        int a = SequentialSearch(data, n, targets[i]);
        int b = BinarySearch(sorted_data, n, targets[i]);
        int c = MySearch(data, n, targets[i]);
        printf("%-14d", targets[i]);
        if (a >= 0) PrintCell("พบ", 2, 12); else PrintCell("ไม่พบ", 4, 12);
        if (b >= 0) PrintCell("พบ", 2, 12); else PrintCell("ไม่พบ", 4, 12);
        if (c >= 0) printf("พบ\n");        else printf("ไม่พบ\n");
    }

    /* ---- วัดเวลา ---- */
    ms_seq = MeasureMillisec(SequentialSearch, data,        n, targets, tcount);
    ms_bin = MeasureMillisec(BinarySearch,     sorted_data, n, targets, tcount);
    ms_my  = MeasureMillisec(MySearch,         data,        n, targets, tcount);

    printf("\n[ ผลการวัดเวลา เฉลี่ยต่อการค้นหาหนึ่งครั้ง ]\n");
    printf(" Sequential Search : %.6f ms\n", ms_seq);
    printf(" Binary Search     : %.6f ms\n", ms_bin);
    printf(" MySearch (ของกลุ่ม): %.6f ms\n", ms_my);
    printf("\nนำค่าที่ได้ไปกรอกในตารางบันทึกผล แล้วรันซ้ำจนครบ 5 ครั้ง\n");

    return 0;
}
                                                                                                                   