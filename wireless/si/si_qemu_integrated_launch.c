/* Disposable launch/teardown owner. SI/DIO are native; only the I2C/radio/
 * phone and hardware edges are substituted. Never installed on HU. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
static void env(const char *key,const char *value){if(setenv(key,value,1))exit(2);}
static pid_t start_daemon(unsigned iteration) {
    pid_t daemon=fork();
    if(!daemon){
        env("MHI2Q_INTEGRATED_DAEMON_ITER",iteration?"1":"0");
        env("LD_PRELOAD","/proc/boot/libmock_mfi_qemu.so");
        execl("/ramdisk/mhi2q-wireless-daemon","mhi2q-wireless-daemon",(char*)0);_exit(127);
    }
    FILE *pid=fopen("/dev/shmem/integrated-daemon.pid","w");if(!pid||daemon<=1)exit(2);
    fprintf(pid,"%ld\n",(long)daemon);fclose(pid);return daemon;
}
int main(void) {
    unsetenv("LD_PRELOAD");
    FILE *fault=fopen("/proc/boot/integrated-fault","r");char mode[32];
    if(!fault||fscanf(fault,"%31s",mode)!=1)return 1;
    fclose(fault);env("MHI2Q_INTEGRATED_FAULT",mode);
    FILE *cycles=fopen("/proc/boot/integrated-cycles","r");char count[8];
    if(!cycles||fscanf(cycles,"%7s",count)!=1)return 1;
    fclose(cycles);env("MHI2Q_INTEGRATED_CYCLES",count);
    env("IPL_CONFIG_DIR_DIO_MANAGER","/proc/boot");
    env("MHI2Q_CP_MODEL","MHI2Q");env("MHI2Q_CP_SERIAL","QEMU");env("MHI2Q_CP_HARDWARE","MHI2Q");
    env("MHI2Q_CP_SSID","QemuCarPlay");env("MHI2Q_CP_PASSPHRASE","testpassword");env("MHI2Q_CP_CHANNEL","11");
    env("MHI2Q_CP_BT_MAC","00:01:02:03:04:05");env("MHI2Q_CP_IP_ADDRESS","127.0.0.1");
    env("MHI2Q_CP_AIRPLAY_PORT","5000");env("MHI2Q_CP_DEVICE_ID","02:00:00:00:00:01");
    env("MHI2Q_CP_RECEIVER","stock210");env("MHI2Q_CP_SOURCE_VERSION","210.81");
    env("MHI2Q_CP_MFI_PATH","/dev/shmem/qemu_mfi");env("MHI2Q_CP_IE_INDEX","2");
    env("MHI2Q_CP_UAPUTL","/proc/boot/fake_uaputl");env("MHI2Q_CP_SI_CONTROL","/tmp/mhi2q_si_control.sock");
    env("MHI2Q_CP_SI_STARTUP_WAIT_MS","40000");
    env("MHI2Q_PHONE_QEMU_CONTROL","1");env("MHI2Q_PHONE_QEMU_INTEGRATED","1");
    FILE *marker=fopen("/dev/shmem/independent_srp_ready","w");if(!marker)return 1;fclose(marker);
    pid_t observer=fork();if(!observer){execl("/proc/boot/integrated-observer","integrated-observer","--baseline",(char*)0);_exit(127);}
    int initial=0;if(observer<=1||waitpid(observer,&initial,0)!=observer||!WIFEXITED(initial)||WEXITSTATUS(initial))return 1;
    pid_t daemon=start_daemon(0);
    pid_t phone=fork();if(!phone){execl("/ramdisk/phone-qemu-probe","phone-qemu-probe",(char*)0);_exit(127);}
    int status=0,okay=0;unsigned restarts=0;
    while(phone>1) {
        pid_t done=waitpid(phone,&status,WNOHANG);
        if(done==phone){okay=WIFEXITED(status)&&!WEXITSTATUS(status);break;}
        if(done<0)break;
        if(waitpid(daemon,&status,WNOHANG)==daemon) {
            if(strcmp(mode,"daemon-crash")||restarts++)break;
            daemon=start_daemon(1);
            puts("[integrated-media] fixture supervisor restarted the production daemon");fflush(stdout);
        }
        usleep(20000);
    }
    if(!okay&&phone>1){kill(phone,SIGTERM);waitpid(phone,&status,0);}
    kill(daemon,SIGTERM);waitpid(daemon,&status,0);
    puts(okay?"SI-INTEGRATED: complete daemon/SI/DIO/phone lifecycle PASS":"SI-INTEGRATED: FAIL");fflush(stdout);
    return okay?0:1;
}
