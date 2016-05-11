#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "es51922.h"

volatile bool running = true;

void process(const uint8_t *buf, int count) {
        char str[100];
        ES51922 p;
        if(!p.parse(buf, count)) return;
        p.toString(str, sizeof(str));
        printf("%s\n", str);
        return;
}

int main(int argc, char *argv[]) {
        if(argc < 2) {
                fprintf(stderr, "Usage: %s <serial device>\n", argv[0]);
                return 1;
        }

        int ret, status;
        struct termios oldtio,newtio;

        const char *deviceName = argv[1];
        int fd = open(deviceName, O_RDWR | O_NOCTTY); 
        if (fd <0) {
                fprintf(stderr, "Failed to open '%s': %s\n", deviceName, strerror(errno));
                return 1;
        }

        ret = tcgetattr(fd, &oldtio); /* save current serial port settings */
        if(ret) {
                fprintf(stderr, "Failed to get terminal control attributes for '%s': %s\n", deviceName, strerror(errno));
                return 1;
        }
        memset(&newtio, 0, sizeof(newtio));

        newtio.c_cflag = B19200 | CS7 | PARODD | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR | IGNBRK;
        newtio.c_oflag = 0;
        newtio.c_lflag = 0;

        ret = tcflush(fd, TCIFLUSH);
        if(ret) {
                fprintf(stderr, "Failed to flush '%s': %s\n", deviceName, strerror(errno));
                return 1;
        }
        ret = tcsetattr(fd, TCSANOW, &newtio);
        if(ret) {
                fprintf(stderr, "Failed to set terminal control attributes for '%s': %s\n", deviceName, strerror(errno));
                return 1;
        }

        ret = ioctl(fd, TIOCMGET, &status);
        if(ret) {
                fprintf(stderr, "Failed to get status bits from '%s': %s\n", deviceName, strerror(errno));
                return 1;
        }
        status |= TIOCM_DTR;
        status &= ~TIOCM_RTS;
        ret = ioctl(fd, TIOCMSET, &status);
        if(ret) {
                fprintf(stderr, "Failed to set status bits to '%s': %s\n", deviceName, strerror(errno));
                return 1;
        }

        int count = 0;
        uint8_t buf[16];
        while(running) {
                char byte;
                int c = read(fd, &byte, 1);
                if(c == 0) continue;
                if(c == -1) {
                        if(errno == EAGAIN) continue;
                        fprintf(stderr, "ERROR: %s\n", strerror(errno));
                        break;
                }
                if(count >= sizeof(buf)) {
                        count = 0;
                        continue;
                }
                buf[count] = byte;
                count++;
                if(byte == 0x0A) {
                        process(buf, count);
                        count = 0;
                }
        }
        /* restore the old port settings */
        tcsetattr(fd,TCSANOW,&oldtio);
        return 0;
}

