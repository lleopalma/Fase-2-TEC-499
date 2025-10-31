#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <time.h>
#include "header.h"
#include "hps_0.h"

#define LW_BRIDGE_BASE 0xFF200000
#define LW_BRIDGE_SPAN 0x00200000

#define FLAG_DONE  0x01
#define FLAG_ERROR 0x02
#define OPCODE_STORE 0x02

#define IMG_WIDTH  320
#define IMG_HEIGHT 240
#define TOTAL_PIXELS (IMG_WIDTH * IMG_HEIGHT)

static volatile uint32_t *h2p_lw_base = NULL;
static int fd = -1;

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

// ================== API BASE ==================
int iniciarAPI() {
    fd = open("/dev/mem", (O_RDWR | O_SYNC));
    if (fd == -1) {
        perror("Erro ao abrir /dev/mem");
        return -1;
    }

    h2p_lw_base = (uint32_t *)mmap(NULL, LW_BRIDGE_SPAN,
                                    (PROT_READ | PROT_WRITE),
                                    MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (h2p_lw_base == MAP_FAILED) {
        perror("Erro ao mapear memória");
        close(fd);
        fd = -1;
        return -1;
    }

    printf("API iniciada com sucesso\n");
    return 0;
}

int encerrarAPI() {
    if (h2p_lw_base && h2p_lw_base != MAP_FAILED) {
        munmap((void *)h2p_lw_base, LW_BRIDGE_SPAN);
        h2p_lw_base = NULL;
    }
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    printf("API finalizada com sucesso\n");
    return 0;
}

static int aguardar_conclusao(int timeout_seconds) {
    time_t start_time = time(NULL);
    uint32_t flags;

    // Aguarda DONE ir a 0 (execução começou)
    while (1) {
        flags = *(h2p_lw_base + (PIO_FLAGS / 4));
        if (flags & FLAG_ERROR) return -2;
        if (!(flags & FLAG_DONE)) break;
        if (difftime(time(NULL), start_time) >= timeout_seconds) return -1;
    }

    // Aguarda DONE voltar a 1 (execução terminou)
    start_time = time(NULL);
    while (1) {
        flags = *(h2p_lw_base + (PIO_FLAGS / 4));
        if (flags & FLAG_ERROR) return -2;
        if (flags & FLAG_DONE) return 0;
        if (difftime(time(NULL), start_time) >= timeout_seconds) return -1;
    }
}

int escrever_pixel(uint32_t address, uint8_t pixel_value) {
    if (!h2p_lw_base) {
        fprintf(stderr, "API não inicializada\n");
        return -1;
    }
    if (address >= TOTAL_PIXELS) {
        fprintf(stderr, "Endereço inválido %u\n", address);
        return -2;
    }

    uint32_t instrucao = 0;
    instrucao |= (OPCODE_STORE & 0x07);
    instrucao |= ((address & 0x1FFFF) << 3);
    instrucao |= (((uint32_t)pixel_value & 0xFF) << 21);

    *(h2p_lw_base + (PIO_ENABLE / 4)) = 0;
    *(h2p_lw_base + (PIO_INSTRUCT / 4)) = instrucao;
    *(h2p_lw_base + (PIO_ENABLE / 4)) = 1;
    *(h2p_lw_base + (PIO_ENABLE / 4)) = 0;

    return aguardar_conclusao(3);
}

// ================== BMP 8-BIT ==================
static void ler_paleta(FILE *arquivo, uint32_t num_cores, uint8_t *paleta) {
    for (uint32_t i = 0; i < num_cores; i++) {
        uint8_t bgrx[4];
        fread(bgrx, 1, 4, arquivo);
        uint8_t b = bgrx[0], g = bgrx[1], r = bgrx[2];
        paleta[i] = (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
    }
}

int carregar_imagem_bmp(const char *filename) {
    FILE *arquivo;
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    uint8_t *imageData = NULL;
    uint8_t paleta[256];

    printf("Abrindo arquivo: %s\n", filename);
    arquivo = fopen(filename, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo BMP");
        return -1;
    }

    fread(&fileHeader, sizeof(BMPFileHeader), 1, arquivo);
    fread(&infoHeader, sizeof(BMPInfoHeader), 1, arquivo);

    if (fileHeader.bfType != 0x4D42) {
        fprintf(stderr, "Arquivo não é BMP válido\n");
        fclose(arquivo);
        return -2;
    }

    if (infoHeader.biBitCount != 8) {
        fprintf(stderr, "Apenas BMP de 8 bits é suportado\n");
        fclose(arquivo);
        return -3;
    }

    if (infoHeader.biWidth != IMG_WIDTH || abs(infoHeader.biHeight) != IMG_HEIGHT) {
        fprintf(stderr, "Imagem deve ter %dx%d pixels\n", IMG_WIDTH, IMG_HEIGHT);
        fclose(arquivo);
        return -4;
    }

    uint32_t num_cores = infoHeader.biClrUsed ? infoHeader.biClrUsed : 256;
    printf("Lendo paleta de %u cores...\n", num_cores);
    ler_paleta(arquivo, num_cores, paleta);

    int row_size = ((infoHeader.biWidth + 3) / 4) * 4;
    int image_size = row_size * abs(infoHeader.biHeight);
    int bottom_up = (infoHeader.biHeight > 0);

    fseek(arquivo, fileHeader.bfOffBits, SEEK_SET);
    imageData = (uint8_t *)malloc(image_size);
    fread(imageData, 1, image_size, arquivo);
    fclose(arquivo);

    printf("Carregando imagem no coprocessador...\n");
    time_t inicio = time(NULL);
    int erros = 0;

    for (int y = 0; y < IMG_HEIGHT; y++) {
        int src_y = bottom_up ? (IMG_HEIGHT - 1 - y) : y;
        uint8_t *row_ptr = imageData + src_y * row_size;

        for (int x = 0; x < IMG_WIDTH; x++) {
            uint8_t index = row_ptr[x];
            uint8_t gray_value = paleta[index];
            uint8_t hw_value = 255 - gray_value; // inverte o brilho
            uint32_t addr = y * IMG_WIDTH + x;

            if (escrever_pixel(addr, hw_value) != 0) {
                erros++;
                if (erros > 100) break;
            }
        }

        if ((y + 1) % 24 == 0)
            printf("Progresso: %3d%%\r", ((y + 1) * 100) / IMG_HEIGHT);
        fflush(stdout);
        if (erros > 100) break;
    }

    double tempo = difftime(time(NULL), inicio);
    printf("\nConcluído em %.1f s, erros: %d\n", tempo, erros);
    free(imageData);
    return (erros == 0) ? 0 : -1;
}

// ================== MAIN ==================
int main() {
    printf("=== Carregador de Imagem BMP 8-bit ===\n");
    printf("Esperado: 320x240, 8 bits, escala de cinza\n\n");

    if (iniciarAPI() != 0) return EXIT_FAILURE;
    int r = carregar_imagem_bmp("ba.bmp");
    encerrarAPI();
    return (r == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
