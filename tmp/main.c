#include <linux/spi/spidev.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>

int main() {
    int fd = open("/dev/spidev1.0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    uint8_t tx[] = { 0x9F }; // JEDEC ID
    uint8_t rx[3] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(rx),
        .speed_hz = 10000000,
        .bits_per_word = 8,
    };

    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    printf("JEDEC ID: %02x %02x %02x\n", rx[0], rx[1], rx[2]);

    close(fd);
    return 0;
}

