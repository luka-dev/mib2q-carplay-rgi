/* The global vphone loader gets no new framework dependency. Only airplayd
 * loads the separately guarded association backend. */
extern const char *getprogname(void);
extern int strcmp(const char *,const char *);
extern void *dlopen(const char *,int);
__attribute__((constructor)) static void load(void){
    if(!strcmp(getprogname(),"airplayd"))
        dlopen("/var/mobile/Library/Application Support/MHI2CarPlayLab/wireless-network.dylib",2);
}
