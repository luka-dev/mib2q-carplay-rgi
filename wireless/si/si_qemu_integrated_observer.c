/* Read-only oracle for the one-phone native SI fixture. No SIW2 HELLO,
 * admission command, appState fabrication or target-memory writes. */
#include "si_runtime_guard.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int fd;
static uint32_t word(uint32_t address) {uint32_t value;if(pread(fd,&value,4,address)!=4)exit(2);return value;}
int main(int argc,char **argv) {
    int idle=argc==2&&!strcmp(argv[1],"--idle"), baseline=argc==2&&!strcmp(argv[1],"--baseline");
    FILE *f=fopen("/ramdisk/var/run/smartphone_integrator.pid","r");long pid=0;
    if(!f||fscanf(f,"%ld",&pid)!=1||pid<=1)return 1;
    fclose(f);char path[80];snprintf(path,sizeof(path),"/proc/%ld/as",pid);
    fd=open(path,O_RDONLY);if(fd<0)return 1;
    uint32_t controller=word(SI_CONTROLLER_PTR),dp=controller+264;
    uint32_t count=word(dp+440),stored=word(dp+544),array=word(dp+448),id=0;
    printf("[integrated-media] raw native present=%u stored=%u app=%d\n",count,stored,(int32_t)word(dp+1240));fflush(stdout);
    unsigned initial=0,initial_stored=0;
    if(baseline) {
        f=fopen("/dev/shmem/integrated-baseline","w");if(!f)return 1;
        fprintf(f,"%u %u\n",count,stored);fclose(f);return 0;
    }
    f=fopen("/dev/shmem/integrated-baseline","r");
    if(!f||fscanf(f,"%u %u",&initial,&initial_stored)!=2)return 1;
    fclose(f);
    if(count!=initial+1||stored!=initial_stored+1)return 1;
    for(unsigned i=0;i<count;++i) {
        uint32_t entry=word(array+i*4);
        if(entry&&word(entry+28)==2&&word(entry+32)==2)id=word(entry);
    }
    if(!id||(int32_t)word(dp+1240)!=(idle?-1:0))return 1;
    f=fopen("/dev/shmem/integrated-native-id","r");unsigned saved=0;
    if(f){int n=fscanf(f,"%u",&saved);fclose(f);if(n!=1||saved!=id)return 1;}
    else {f=fopen("/dev/shmem/integrated-native-id","w");if(!f)return 1;fprintf(f,"%u\n",id);fclose(f);}
    char line[160];int n=snprintf(line,sizeof(line),"[integrated-media] native observation id=%u present=%u stored=%u idle=%d PASS\n",id,count,stored,idle);
    if(n>0&&(size_t)n<sizeof(line))(void)write(1,line,(size_t)n);
    close(fd);return 0;
}
