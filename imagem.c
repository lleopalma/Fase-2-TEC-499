#define _XOPEN_SOURCE 500
#include <stdio.h>
#include "header.h"
#include <unistd.h>
#include <sys/mman.h>
#include "./hps_0.h"
#include <stdlib.h>
#include <stdint.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h> 

extern int iniciarBib();
extern int encerrarBib();
extern void Vizinho_Prox();
extern int write_pixel(unsigned int address, unsigned char data);
extern void Reset();
extern void Replicacao();
extern void Decimacao();
extern void Media();
extern int Flag_Done();
extern int Flag_Error();
extern int Flag_ZOOMMax();
extern int Flag_ZOOMMin();

// Estrutura do cabeçalho BMP
#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPInfoHeader;
#pragma pack(pop)

// Função para carregar e enviar imagem BMP
int enviar_imagem_bmp(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("ERRO: Não foi possível abrir o arquivo '%s'\n", filename);
        return -1;
    }

    // Ler cabeçalhos
    BMPHeader header;
    BMPInfoHeader info;
    
    fread(&header, sizeof(BMPHeader), 1, file);
    fread(&info, sizeof(BMPInfoHeader), 1, file);

    // Verificar se é BMP válido
    if (header.type != 0x4D42) {
        printf("ERRO: Arquivo não é um BMP válido!\n");
        fclose(file);
        return -1;
    }

    printf("Dimensões: %dx%d pixels\n", info.width, info.height);
    printf("Bits por pixel: %d\n", info.bits_per_pixel);

    // Verificar dimensões esperadas (320x240 = 76800 pixels)
    if (info.width != 320 || info.height != 240) {
        printf("AVISO: Imagem com dimensões diferentes de 320x240!\n");
    }

    // Mover para início dos dados de pixel
    fseek(file, header.offset, SEEK_SET);

    // Calcular padding (linhas BMP são alinhadas a 4 bytes)
    int bytes_per_pixel = info.bits_per_pixel / 8;
    int row_size = info.width * bytes_per_pixel;
    int padding = (4 - (row_size % 4)) % 4;

    // Aguardar hardware estar pronto
    while(Flag_Done() == 0) {
        usleep(1000);
    }

    printf("\nEnviando imagem...\n");

    // Buffer para uma linha
    unsigned char *row_buffer = (unsigned char*)malloc(row_size);
    if (!row_buffer) {
        printf("ERRO: Falha ao alocar memória!\n");
        fclose(file);
        return -1;
    }

    int total_pixels = info.width * info.height;
    int pixel_count = 0;

    // BMP armazena de baixo para cima, então invertemos
    for(int y = info.height - 1; y >= 0; y--) {
        // Ler linha
        fseek(file, header.offset + y * (row_size + padding), SEEK_SET);
        fread(row_buffer, 1, row_size, file);

        // Enviar pixels da linha
        for(int x = 0; x < info.width; x++) {
            unsigned char pixel_data;
            
            if (info.bits_per_pixel == 8) {
                // Grayscale 8 bits
                pixel_data = row_buffer[x];
            } 
            else if (info.bits_per_pixel == 24) {
                // BGR para grayscale (média simples)
                int idx = x * 3;
                unsigned char b = row_buffer[idx];
                unsigned char g = row_buffer[idx + 1];
                unsigned char r = row_buffer[idx + 2];
                pixel_data = (r + g + b) / 3;
            }
            else {
                printf("ERRO: Formato de pixel não suportado (%d bits)\n", info.bits_per_pixel);
                free(row_buffer);
                fclose(file);
                return -1;
            }

            // Endereço linear do pixel
            int address = (info.height - 1 - y) * info.width + x;
            write_pixel(address, pixel_data);

            pixel_count++;
            
            // Atualizar progresso
            if(pixel_count % 500 == 0) {
                float progresso = (pixel_count * 100.0) / total_pixels;
                printf("\rProgresso: %d/%d pixels (%.1f%%)    ", 
                       pixel_count, total_pixels, progresso);
                fflush(stdout);
            }
        }
    }

    printf("\rProgresso: %d/%d pixels (100.0%%)    \n", total_pixels, total_pixels);
    printf("Imagem enviada com sucesso!\n");

    free(row_buffer);
    fclose(file);
    return 0;
}

int main() {
    int opcao;
    char filename[256];
    struct input_event ev;
    int fd;
    
    printf("\nINICIANDO API\n");
    if (iniciarBib() != 0) 
    {
        printf("ERRO ao iniciar API!\n");
        return 1;
    }
    printf("API em FUNCIONAMENTO!\n\n");

    fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir mouse");
        return 1;
    }
    
    int continuar = 1;
    Reset();
    do {
        printf("\n--- MENU DE TESTES ---\n");
        printf("1. Enviar imagem BMP\n");
        printf("2. Zoom com o mouse\n");
        printf("3. Sair\n");
        printf("Opcao: ");
        scanf("%d", &opcao);
        
        switch(opcao) 
        {
            case 1:
                enviar_imagem_bmp("c.bmp");
                Reset();
                break;
                
            case 2:
                printf("\nDando zoom com mouse...");
                int verification = 1;
                int contadorcima = 0;
                int contadorbaixo = 0;
                while (verification) {
                    read(fd, &ev, sizeof(struct input_event));

                    if (ev.type == EV_KEY && ev.code == BTN_LEFT && ev.value == 1) {
                        printf("\nBotão esquerdo pressionado. Saindo do zoom...\n");
                        verification = 0;  // Sai do loop
                        break;
                    }
                    
                    if (ev.type == EV_REL && ev.code == REL_WHEEL) {
                        if (ev.value > 0) {
                            contadorcima++;
                            if (contadorcima % 2 == 0){
                                Vizinho_Prox();
                            } else { 
                                Replicacao();
                            }
                        } else if (ev.value < 0) {
                            contadorbaixo++;
                            if (contadorbaixo % 2 == 0){
                                Media();
                            } else { 
                                Decimacao();
                            }
                        } 
                    }
                }
                break;
            case 3:
                printf("\nSaindo...");
                continuar = 0;
                break;  
            default:
                printf("\nDigite uma opção válida!\n");
        }
        
    } 
    while(continuar);
    
    printf("\nEncerrando programa...");
    encerrarBib();
    
    return 0;
}
