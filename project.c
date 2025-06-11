/*============================================================
 *  seat_sort.c  ——  整数奇偶归类 + 排序 + 排座系统
 *  单源文件实现，依赖标准 C 库
 *  编译： gcc seat_sort.c -o seat_sort            // 普通模式
 *        gcc seat_sort.c -DDEBUG_VISUAL -o seat_sort // 带调试可视
 *==========================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/*======================== 常量区 ========================*/
#define MAX_N   1024        /* 最多支持 1 k 名考生        */
#define MAX_R   32          /* 教室最大行数              */
#define MAX_C   32          /* 教室最大列数              */
#define BUF_SZ  256         /* 字符缓冲区大小            */

/*======================== 数据结构 =======================*/
/* 单个座位，用于 2D seatMap；id<=0 表空位                 */
typedef struct {
    int id;
} Seat;

/* 考生数据集：raw 原始顺序；odd/even 归类 + 排序后序列    */
typedef struct {
    int raw[MAX_N];
    int n;                  /* 有效元素个数              */

    int odd[MAX_N];
    int n_odd;

    int even[MAX_N];
    int n_even;
} Dataset;

/* 排座参数：                                          */
typedef struct {
    int rows;               /* 行数（前/后）                */
    int cols;               /* 列数（左/右）                */
    /* mode == 0 → “左右”布局；mode == 1 → “前后”布局  */
    int mode_lr_or_fb;
    /* odd_side == 0 → 奇数在左/前；odd_side == 1 → 奇数在右/后 */
    int odd_side;
} SeatCfg;

/*======================== 工具宏&函数 ====================*/
static inline int is_odd(int x)          { return x & 1; }
static inline void swap_int(int *a,int *b){ int t=*a;*a=*b;*b=t; }

#ifdef DEBUG_VISUAL
#  define DBG_FMT(fmt,...)  printf(fmt,##__VA_ARGS__)
#else
#  define DBG_FMT(fmt,...)
#endif

/*======================== 数据采集 =======================*/
/*--- 手动输入 -------------------------------------------*/
static int load_manual(Dataset *ds)
{
    int n;
    printf("手动输入整数个数(>=20, <=%d)：", MAX_N);
    if (scanf("%d", &n)!=1 || n<20 || n>MAX_N) {
        fprintf(stderr,"输入数量非法！\n");
        return -1;
    }
    printf("逐个输入整数：\n");
    for (int i=0;i<n;++i) {
        if (scanf("%d", &ds->raw[i])!=1){
            fprintf(stderr,"输入错误！\n"); return -1;
        }
    }
    ds->n = n;
    return 0;
}

/*--- 随机生成 -------------------------------------------*/
static int load_random(Dataset *ds)
{
    int n, lo, hi;
    printf("随机生成数量(>=20, <=%d)：", MAX_N);
    if (scanf("%d",&n)!=1 || n<20 || n>MAX_N) return -1;
    printf("输入随机数下界、上界：");
    if (scanf("%d %d",&lo,&hi)!=2 || lo>hi) return -1;

    srand((unsigned)time(NULL));
    for (int i=0;i<n;++i) {
        ds->raw[i]= lo + rand()%(hi-lo+1);
    }
    ds->n = n;
    return 0;
}

/*--- CSV 导入 ------------------------------------------------*/
static int load_csv(Dataset *ds, const char *csv_path)
{
    FILE *fp = fopen(csv_path, "r");
    if (!fp) { perror("fopen csv"); return -1; }

    int n = 0, val;     /* 支持逗号或换行分隔 */
    char line[BUF_SZ];
    while (fgets(line, sizeof(line), fp) && n < MAX_N) {
        char *tok = strtok(line, ",\n\r");
        while (tok && n < MAX_N) {
            if (sscanf(tok, "%d", &val) == 1)
                ds->raw[n++] = val;
            tok = strtok(NULL, ",\n\r");
        }
    }
    fclose(fp);
    if (n < 20) {
        fprintf(stderr, "CSV 元素不足 20\n");
        return -1;
    }
    ds->n = n;
    return 0;
}

/*==================== 奇偶归类三策略 ====================*/
/*---------- 1. classify_stable：额外缓冲，两遍扫描 -------*/
static void classify_stable(Dataset *ds)
{
    ds->n_odd = ds->n_even = 0;
    /* 先收集奇数再收集偶数，稳定 */
    for (int i=0;i<ds->n;++i)
        if (is_odd(ds->raw[i]))
            ds->odd[ds->n_odd++] = ds->raw[i];
    for (int i=0;i<ds->n;++i)
        if (!is_odd(ds->raw[i]))
            ds->even[ds->n_even++] = ds->raw[i];
}

/*---------- 2. classify_partition：原地分区 --------------*/
static void classify_partition(Dataset *ds)
{
    /* 拷贝一份到 odd 数组当作工作区，最终 odd[..n_odd) 偶[..n_even) */
    memcpy(ds->odd, ds->raw, ds->n*sizeof(int));
    int pivot = 0;          /* pivot 指向下一个奇数插入位           */
    for (int i=0;i<ds->n;++i){
        if (is_odd(ds->odd[i])){
            swap_int(&ds->odd[i], &ds->odd[pivot]);
            ++pivot;
        }
    }
    ds->n_odd = pivot;
    ds->n_even = ds->n - pivot;
    memcpy(ds->even, ds->odd + ds->n_odd, ds->n_even*sizeof(int));
    DBG_FMT("[Partition] odd=%d, even=%d\n", ds->n_odd, ds->n_even);
}

/*---------- 3. classify_two_ptr：前后双指针 --------------*/
static void classify_two_ptr(Dataset *ds)
{
    int l = 0, r = ds->n - 1;
    int *buf = ds->odd;     /* 复用 odd 数组作工作区，in-place      */
    memcpy(buf, ds->raw, ds->n*sizeof(int));

    while (l <= r) {
        while (l<=r && is_odd(buf[l])) ++l;
        while (l<=r && !is_odd(buf[r])) --r;
        if (l < r) swap_int(&buf[l], &buf[r]);
    }
    ds->n_odd = l;
    ds->n_even = ds->n - l;
    memcpy(ds->even, buf + ds->n_odd, ds->n_even*sizeof(int));
}

/*======================== 排序算法 =======================*/
/*—— 快速排序：三数取中 + 尾递归优化（不稳定） ————*/
static int  _median3(int *a,int l,int r)
{
    int m = l + ((r-l)>>1);
    if (a[l] > a[m]) swap_int(&a[l], &a[m]);
    if (a[l] > a[r]) swap_int(&a[l], &a[r]);
    if (a[m] > a[r]) swap_int(&a[m], &a[r]);
    /* 现在 a[m] 为中位数 */
    swap_int(&a[m], &a[r-1]);
    return a[r-1];
}
static void _qsort(int *a,int l,int r)
{
    while (l < r){
        /* 小区间插排微优化 */
        if (r - l < 16){
            for (int i=l+1;i<=r;++i){
                int key = a[i], j=i-1;
                while (j>=l && a[j]>key) a[j+1]=a[j--];
                a[j+1]=key;
            }
            return;
        }
        int pivot = _median3(a,l,r);
        int i=l, j=r-1;
        while (1){
            while (a[++i] < pivot);
            while (a[--j] > pivot);
            if (i<j) swap_int(&a[i],&a[j]);
            else    break;
        }
        swap_int(&a[i], &a[r-1]);  /* 归位 pivot */

        /* 尾递归：先较小段递归，较大段循环 */
        if (i-l < r-i){
            _qsort(a,l,i-1);
            l = i+1;
        } else {
            _qsort(a,i+1,r);
            r = i-1;
        }
    }
}
static void quick_sort_wrapper(int *a,int n){ if(n>1) _qsort(a,0,n-1); }

/*—— 堆排序：原地最大堆（不稳定） —————————————*/
static void _sift_down(int *a,int n,int i)
{
    for(;;){
        int l=2*i+1, r=l+1, largest=i;
        if(l<n && a[l]>a[largest]) largest=l;
        if(r<n && a[r]>a[largest]) largest=r;
        if(largest==i) break;
        swap_int(&a[i],&a[largest]);
        i = largest;
    }
}
static void heap_sort(int *a,int n)
{
    /* Build heap */
    for (int i=n/2-1;i>=0;--i) _sift_down(a,n,i);
    /* Extract max */
    for (int i=n-1;i>0;--i){
        swap_int(&a[0],&a[i]);
        _sift_down(a,i,0);
    }
}

/*==================== 排序算法选择包装 ===================*/
typedef enum { SORT_QUICK=1, SORT_HEAP=2 } SortAlg;
static void sort_array(int *a,int n, SortAlg alg)
{
    switch (alg){
        case SORT_QUICK: quick_sort_wrapper(a,n); break;
        case SORT_HEAP : heap_sort(a,n);          break;
        default:        quick_sort_wrapper(a,n);  break;
    }
}

/*------------ 插入单个整数并保持归类后序列有序 ----------*/
static int insert_value(Dataset *ds, int val)
{
    if (ds->n >= MAX_N) {
        fprintf(stderr, "数据已满，无法插入\n");
        return -1;
    }
    ds->raw[ds->n++] = val;

    if (ds->n_odd + ds->n_even == ds->n - 1) {
        /* 已经归类过，维护有序插入 */
        if (is_odd(val)) {
            int i = ds->n_odd;
            while (i > 0 && ds->odd[i-1] > val) {
                ds->odd[i] = ds->odd[i-1];
                --i;
            }
            ds->odd[i] = val;
            ds->n_odd++;
        } else {
            int i = ds->n_even;
            while (i > 0 && ds->even[i-1] > val) {
                ds->even[i] = ds->even[i-1];
                --i;
            }
            ds->even[i] = val;
            ds->n_even++;
        }
    }
    return 0;
}
/*====================== 座位映射 =========================*
 * 根据 cfg 生成 seatMap，返回有效行列（通过 cfg.rows/cols） */
static void gen_seat_map(const Dataset *ds,
                         Seat seatMap[MAX_R][MAX_C],
                         SeatCfg *cfg)
{
    /* -------- 1) 自动推导行列 -------- */
    int total = ds->n;
    if (cfg->rows <= 0 || cfg->cols <= 0) {
        /* 默认：左右布局 → 行≈√n，列=6;   前后布局 → 行=2, 列=ceil(n/2) */
        if (cfg->mode_lr_or_fb == 0) {          /* LR */
            cfg->cols = 6;
            cfg->rows = (total + cfg->cols - 1) / cfg->cols;
        } else {                               /* FB */
            cfg->rows = 2;
            cfg->cols = (total + cfg->rows - 1) / cfg->rows;
        }
        if (cfg->rows > MAX_R) cfg->rows = MAX_R;
        if (cfg->cols > MAX_C) cfg->cols = MAX_C;
    }

    /* -------- 2) 准备两串指针 -------- */
    const int *pOdd  = ds->odd;
    const int *pEven = ds->even;
    int idxOdd = 0, idxEven = 0;

    /* -------- 3) 填 seatMap ---------- */
    for (int r = 0; r < cfg->rows; ++r)
        for (int c = 0; c < cfg->cols; ++c)
            seatMap[r][c].id = 0;            /* 空位初始化 */

    if (cfg->mode_lr_or_fb == 0) { /* =============== 左右模式 =============== */
        int half = cfg->cols / 2;            /* 左右各 half 列 */
        int cOddStart  =  cfg->odd_side ? half      : 0;     /* 奇数列起点 */
        int cEvenStart = !cfg->odd_side ? half      : 0;     /* 偶数列起点 */

        /* 逐行填充 */
        for (int r = 0; r < cfg->rows; ++r) {
            /* --- 奇数区 --- */
            for (int c = 0; c < half && idxOdd < ds->n_odd; ++c)
                seatMap[r][cOddStart + c].id = pOdd[idxOdd++];
            /* --- 偶数区 --- */
            for (int c = 0; c < half && idxEven < ds->n_even; ++c)
                seatMap[r][cEvenStart + c].id = pEven[idxEven++];
        }
    } else {                 /* =============== 前后模式 =============== */
        int half = cfg->rows / 2;            /* 前后各 half 行 */
        int rOddStart  =  cfg->odd_side ? half      : 0;
        int rEvenStart = !cfg->odd_side ? half      : 0;

        /* 奇数排在 rOddStart..rOddStart+half-1 行 */
        for (int r = 0; r < half && idxOdd < ds->n_odd; ++r)
            for (int c = 0; c < cfg->cols && idxOdd < ds->n_odd;++c)
                seatMap[rOddStart + r][c].id = pOdd[idxOdd++];

        /* 偶数排在 rEvenStart.. */
        for (int r = 0; r < half && idxEven < ds->n_even; ++r)
            for (int c = 0; c < cfg->cols && idxEven < ds->n_even;++c)
                seatMap[rEvenStart + r][c].id = pEven[idxEven++];
    }
}

/*====================== ASCII 可视化 =====================*/
static void render_ascii(const Seat seatMap[MAX_R][MAX_C],
                         const SeatCfg *cfg)
{
    const int seatW = 4;                 /* [NN] 宽度含空格 */
    int lineLen = cfg->cols * seatW + 6; /* + 边框/标题 */

    /* 顶部标题框 */
    printf("+");
    for (int i = 0; i < lineLen-2; ++i) putchar('-');
    printf("+\n|%*s|\n+", lineLen-2, "考试排座示意");
    for (int i = 0; i < lineLen-2; ++i) putchar('-');
    printf("+\n");

    /* Front / Rear 标签（左右布局始终 Front 在上；前后布局根据 odd_side） */
    const char *front = cfg->mode_lr_or_fb ? (cfg->odd_side ? "Rear" : "Front")
                                           : "Front";
    printf("|%*s%*s|\n", (lineLen-2- (int)strlen(front))/2, "",
            (int)strlen(front), front);

    /* 主体座位 */
    for (int r = 0; r < cfg->rows; ++r) {
        printf("| ");
        for (int c = 0; c < cfg->cols; ++c) {
            int id = seatMap[r][c].id;
            if (id)
                printf("[%02d] ", id);
            else
                printf(" --  ");
        }
        printf("|\n");
    }

    /* Rear 标签（若左右布局，置底；若前后布局，顶到底区别于 front） */
    if (cfg->mode_lr_or_fb == 0) {       /* LR 固定 bottom Rear */
        printf("|%*s%*s|\n", (lineLen-2-4)/2, "", 4, "Rear");
    } else {
        const char *rear = cfg->odd_side ? "Front" : "Rear";
        printf("|%*s%*s|\n", (lineLen-2- (int)strlen(rear))/2, "",
                (int)strlen(rear), rear);
    }

    /* 底框 */
    printf("+");
    for (int i = 0; i < lineLen-2; ++i) putchar('-');
    printf("+\n");

    /* 左右标签 */
    if (cfg->mode_lr_or_fb == 0) {
        printf("%*sLeft   Aisle   Right\n", (lineLen-2-20)/2, "");
    }
}

/*====================== CSV 导出 =========================*/
static void write_csv(const Dataset *ds,
                      const Seat seatMap[MAX_R][MAX_C],
                      const SeatCfg *cfg,
                      const char *odd_path,
                      const char *even_path,
                      const char *seat_path)
{
    /* odd.csv / even.csv */
    FILE *fo = fopen(odd_path, "w");
    FILE *fe = fopen(even_path, "w");
    if (!fo || !fe) { perror("fopen"); if(fo)fclose(fo); if(fe)fclose(fe); return; }
    for (int i=0;i<ds->n_odd;++i) fprintf(fo,"%d%s",ds->odd[i],
                    (i==ds->n_odd-1?"\n":","));
    for (int i=0;i<ds->n_even;++i) fprintf(fe,"%d%s",ds->even[i],
                    (i==ds->n_even-1?"\n":","));
    fclose(fo); fclose(fe);

    /* seat_map.csv ：行、列与 id */
    FILE *fs = fopen(seat_path,"w");
    if (!fs) { perror("fopen"); return; }
    for (int r = 0; r < cfg->rows; ++r){
        for (int c = 0; c < cfg->cols; ++c){
            fprintf(fs,"%d%s", seatMap[r][c].id,
                    (c==cfg->cols-1?"":","));
        }
        fputc('\n',fs);
    }
    fclose(fs);
    printf("已导出 %s / %s / %s\n", odd_path, even_path, seat_path);
}

/*====================== 性能测试 =========================*/
static void bench_once(const Dataset *src,
                       void (*classifier)(Dataset*),
                       SortAlg alg)
{
    Dataset ds = *src;                  /* 拷贝，保持原 raw */
    clock_t t0=clock();
    classifier(&ds);
    clock_t t1=clock();
    sort_array(ds.odd , ds.n_odd , alg);
    sort_array(ds.even, ds.n_even, alg);
    clock_t t2=clock();

    printf("归类 %.3f ms, 排序 %.3f ms, 总计 %.3f ms\n",
           (t1-t0)*1000.0/CLOCKS_PER_SEC,
           (t2-t1)*1000.0/CLOCKS_PER_SEC,
           (t2-t0)*1000.0/CLOCKS_PER_SEC);
}

/*====================== 交互菜单 =========================*/
static void menu_loop(void)
{
    Dataset   ds = { .n = 0 };
    Seat      seatMap[MAX_R][MAX_C];
    SeatCfg   cfg = {0};
    void (*classifier)(Dataset*) = classify_partition;
    SortAlg   sorter = SORT_QUICK;

    char cmd[16];
    while (1) {
        puts("\n===== SeatSorter =====");
        puts("[1] 导入/生成数据");
        puts("[2] 选择归类算法  (1 稳定 2 原地 3 双指针)");
        puts("[3] 选择排序算法  (1 快排 2 堆排)");
        puts("[4] 座位排布设置  (L/R or F/B, 奇数在哪侧)");
        puts("[5] 排座并显示+导出");
        puts("[6] 性能测试      (当前配置)");
        puts("[7] 插入一个整数");
        puts("[q] 退出");
        printf(">>> ");
        if (!fgets(cmd,sizeof(cmd),stdin)) break;

        switch (cmd[0]) {
        case '1': {
            printf("a) 手动  b) 随机  c) CSV 文件\n>> ");
            char m = getchar(); while(getchar()!='\n');
            int ok=-1;
            if (m=='a') ok = load_manual(&ds);
            else if (m=='b') ok = load_random(&ds);
            else if (m=='c') {
                char path[128];
                printf("CSV 路径："); scanf("%127s",path); while(getchar()!='\n');
                ok = load_csv(&ds, path);
            }
            if (!ok) puts("数据载入完成！");
            break; }
        case '2':
            printf("选择 1/2/3："); fgets(cmd,sizeof(cmd),stdin);
            classifier = (cmd[0]=='1')?classify_stable:
                         (cmd[0]=='3')?classify_two_ptr:classify_partition;
            break;
        case '3':
            printf("选择 1/2："); fgets(cmd,sizeof(cmd),stdin);
            sorter = (cmd[0]=='2')?SORT_HEAP:SORT_QUICK;
            break;
        case '4': {
            printf("模式 0=左右,1=前后："); int m; scanf("%d",&m);
            printf("奇数侧 0=左/前,1=右/后："); int s; scanf("%d",&s);
            printf("行 列 (0 0 自动)："); scanf("%d %d",&cfg.rows,&cfg.cols);
            while(getchar()!='\n');
            cfg.mode_lr_or_fb = m; cfg.odd_side = s;
            break; }
        case '5': {
            if (ds.n<20){ puts("请先载入数据！"); break; }
            classifier(&ds);
            sort_array(ds.odd , ds.n_odd , sorter);
            sort_array(ds.even, ds.n_even, sorter);
            gen_seat_map(&ds, seatMap, &cfg);
            render_ascii(seatMap, &cfg);
            char odd_f[128] = "odd.csv", even_f[128] = "even.csv", seat_f[128] = "seat_map.csv";
            printf("奇数 CSV 路径(默认 odd.csv): ");
            fgets(cmd, sizeof(cmd), stdin);
            if (cmd[0] != '\n') { cmd[strcspn(cmd,"\r\n")] = '\0'; strncpy(odd_f, cmd, sizeof(odd_f)-1); }
            printf("偶数 CSV 路径(默认 even.csv): ");
            fgets(cmd, sizeof(cmd), stdin);
            if (cmd[0] != '\n') { cmd[strcspn(cmd,"\r\n")] = '\0'; strncpy(even_f, cmd, sizeof(even_f)-1); }
            printf("座位 CSV 路径(默认 seat_map.csv): ");
            fgets(cmd, sizeof(cmd), stdin);
            if (cmd[0] != '\n') { cmd[strcspn(cmd,"\r\n")] = '\0'; strncpy(seat_f, cmd, sizeof(seat_f)-1); }
            write_csv(&ds, seatMap, &cfg, odd_f, even_f, seat_f);
            break; }
        case '6': {
            if (ds.n<20){ puts("请先载入数据！"); break; }
            bench_once(&ds, classifier, sorter);
            break; }
        case '7': {
            printf("输入要插入的整数：");
            int v; if (scanf("%d", &v)!=1){ puts("输入错误"); while(getchar()!='\n'); break; }
            while(getchar()!='\n');
            if (!insert_value(&ds, v)) puts("已插入");
            break; }
        case 'q': return;
        default : break;
        }
    }
}

int main(void)
{
    menu_loop();
    return 0;
}

