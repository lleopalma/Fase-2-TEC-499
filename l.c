#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <time.h>
#include "header.h"
#include "hps_0.h"

// Endereço base do barramento lightweight HPS-to-FPGA
#define LW_BRIDGE_BASE 0xFF200000
#define LW_BRIDGE_SPAN 0x00200000

// Ponteiro global para a memória mapeada
static volatile uint32_t *h2p_lw_base = NULL;
static int fd = -1;

// Máscaras para flags
#define FLAG_DONE       0x01
#define FLAG_ERROR      0x02

// Opcodes das instruções
#define OPCODE_STORE    0x02  // Store pixel

// Dimensões da imagem
#define IMG_WIDTH  320
#define IMG_HEIGHT 240
#define TOTAL_PIXELS (IMG_WIDTH * IMG_HEIGHT)

// Estrutura do cabeçalho BMP
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      // Deve ser 'BM' (0x4D42)
    uint32_t bfSize;      // Tamanho do arquivo
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;   // Offset para os dados da imagem
} BMPFileHeader;

typedef struct {
    uint32_t biSize;          // Tamanho do cabeçalho
    int32_t  biWidth;         // Largura da imagem
    int32_t  biHeight;        // Altura da imagem
    uint16_t biPlanes;
    uint16_t biBitCount;      // Bits por pixel
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

/**
 * @brief Inicializa a API, mapeando a memória do FPGA
 * @return 0 em sucesso, -1 em erro
 */
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

/**
 * @brief Finaliza a API, desmapeando a memória
 * @return 0 em sucesso, -1 em erro
 */
int encerrarAPI() {
    if (h2p_lw_base != NULL && h2p_lw_base != MAP_FAILED) {
        if (munmap((void *)h2p_lw_base, LW_BRIDGE_SPAN) != 0) {
            perror("Erro ao desmapear memória");
            return -1;
        }
        h2p_lw_base = NULL;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    printf("API finalizada com sucesso\n");
    return 0;
}

/**
 * @brief Aguarda a conclusão de uma operação verificando a flag DONE
 * @param timeout_seconds Tempo máximo de espera em segundos
 * @return 0 em sucesso, -1 em timeout, -2 em erro de hardware
 */
static int aguardar_conclusao(int timeout_seconds) {
    time_t start_time = time(NULL);
    time_t current_time;
    uint32_t flags;

    // Primeira fase: aguarda DONE ir para 0 (instrução começou a executar)
    while (1) {
        flags = *(h2p_lw_base + (PIO_FLAGS / 4));
        
        // Verifica erro
        if (flags & FLAG_ERROR) {
            return -2;
        }
        
        // DONE = 0 significa que começou a executar
        if (!(flags & FLAG_DONE)) {
            break;
        }
        
        // Timeout
        current_time = time(NULL);
        if (difftime(current_time, start_time) >= timeout_seconds) {
            return -1;
        }
    }

    // Segunda fase: aguarda DONE voltar para 1 (instrução concluída)
    start_time = time(NULL);
    while (1) {
        flags = *(h2p_lw_base + (PIO_FLAGS / 4));

        // Verifica erro
        if (flags & FLAG_ERROR) {
            return -2;
        }

        // DONE = 1 significa que terminou
        if (flags & FLAG_DONE) {
            return 0;
        }

        // Timeout
        current_time = time(NULL);
        if (difftime(current_time, start_time) >= timeout_seconds) {
            return -1;
        }
    }
}

/**
 * @brief Escreve um pixel na memória A do coprocessador
 * @param address Endereço do pixel (0 a 76799)
 * @param pixel_value Valor do pixel (0-255 escala de cinza)
 * @return 0 em sucesso, valores negativos em erro
 */
int escrever_pixel(uint32_t address, uint8_t pixel_value) {
    if (h2p_lw_base == NULL) {
        fprintf(stderr, "Erro: API não inicializada\n");
        return -1;
    }

    if (address >= TOTAL_PIXELS) {
        fprintf(stderr, "Erro: Endereço inválido %u (máximo: %d)\n", address, TOTAL_PIXELS - 1);
        return -2;
    }

    // Monta a instrução STORE conforme documentação:
    // Opcode: 010 (bits 2:0)
    // Address: bits 19:3 (17 bits)
    // Sel Mem: bit 20 (não usado, mas vamos setar como 0)
    // Valor: bits 28:21 (8 bits)
    
    uint32_t instrucao = 0;
    
    // Opcode nos bits 2:0
    instrucao |= (OPCODE_STORE & 0x07);
    
    // Address nos bits 19:3 
    instrucao |= ((address & 0x1FFFF) << 3);
    
    // Valor do pixel nos bits 28:21
    instrucao |= (((uint32_t)pixel_value & 0xFF) << 21);
    
    // Debug: mostra a instrução para os primeiros pixels
    static int debug_count = 0;
    if (debug_count < 5) {
        printf("DEBUG pixel[%u] = %u (0x%02X) -> instrução: 0x%08X\n", 
               address, pixel_value, pixel_value, instrucao);
        printf("  Bits 28:21 (valor): 0x%02X\n", (instrucao >> 21) & 0xFF);
        printf("  Bits 19:3 (addr): 0x%05X\n", (instrucao >> 3) & 0x1FFFF);
        printf("  Bits 2:0 (opcode): 0x%X\n", instrucao & 0x07);
        debug_count++;
    }

    // Desativa enable
    *(h2p_lw_base + (PIO_ENABLE / 4)) = 0x00;

    // Escreve a instrução
    *(h2p_lw_base + (PIO_INSTRUCT / 4)) = instrucao;

    // Pulso de enable
    *(h2p_lw_base + (PIO_ENABLE / 4)) = 0x01;
    *(h2p_lw_base + (PIO_ENABLE / 4)) = 0x00;

    // Aguarda conclusão com timeout maior
    int resultado = aguardar_conclusao(3);
    
    if (resultado != 0) {
        if (debug_count < 5) {
            fprintf(stderr, "Erro ao escrever pixel no endereço %u (código: %d)\n", address, resultado);
        }
        return -3;
    }

    return 0;
}

/**
 * @brief Converte pixel RGB para escala de cinza
 * @param r Componente vermelho (0-255)
 * @param g Componente verde (0-255)
 * @param b Componente azul (0-255)
 * @return Valor em escala de cinza (0-255)
 */
uint8_t rgb_para_cinza(uint8_t r, uint8_t g, uint8_t b) {
    // Fórmula padrão de luminância: Y = 0.299*R + 0.587*G + 0.114*B
    return (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
}

/**
 * @brief Carrega uma imagem BMP e envia pixel por pixel para o coprocessador
 * @param filename Nome do arquivo BMP (deve ser 320x240, 8 bits, escala de cinza)
 * @return 0 em sucesso, valores negativos em erro
 */
int carregar_imagem_bmp(const char *filename) {
    FILE *arquivo;
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    uint8_t *imageData = NULL;
    int resultado = 0;

    printf("Abrindo arquivo: %s\n", filename);

    arquivo = fopen(filename, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo BMP");
        return -1;
    }

    // Lê o cabeçalho do arquivo
    if (fread(&fileHeader, sizeof(BMPFileHeader), 1, arquivo) != 1) {
        fprintf(stderr, "Erro ao ler cabeçalho do arquivo\n");
        fclose(arquivo);
        return -2;
    }

    // Verifica se é um arquivo BMP válido
    if (fileHeader.bfType != 0x4D42) {
        fprintf(stderr, "Arquivo não é um BMP válido (assinatura: 0x%X)\n", fileHeader.bfType);
        fclose(arquivo);
        return -3;
    }

    // Lê o cabeçalho de informações
    if (fread(&infoHeader, sizeof(BMPInfoHeader), 1, arquivo) != 1) {
        fprintf(stderr, "Erro ao ler cabeçalho de informações\n");
        fclose(arquivo);
        return -4;
    }

    printf("Dimensões: %dx%d pixels\n", infoHeader.biWidth, abs(infoHeader.biHeight));
    printf("Bits por pixel: %d\n", infoHeader.biBitCount);

    // Verifica as dimensões
    if (infoHeader.biWidth != IMG_WIDTH || abs(infoHeader.biHeight) != IMG_HEIGHT) {
        fprintf(stderr, "Erro: Imagem deve ser %dx%d pixels (encontrado: %dx%d)\n",
                IMG_WIDTH, IMG_HEIGHT, infoHeader.biWidth, abs(infoHeader.biHeight));
        fclose(arquivo);
        return -5;
    }

    // Verifica se está em escala de cinza (8 bits)
    if (infoHeader.biBitCount != 8) {
        fprintf(stderr, "Erro: Imagem deve estar em escala de cinza (8 bits)\n");
        fclose(arquivo);
        return -6;
    }

    // Calcula o tamanho de cada linha (com padding)
    int bytes_por_pixel = 1;
    int row_size = ((infoHeader.biWidth * bytes_por_pixel + 3) / 4) * 4;
    int image_size = row_size * abs(infoHeader.biHeight);

    // Aloca memória para a imagem
    imageData = (uint8_t *)malloc(image_size);
    if (!imageData) {
        fprintf(stderr, "Erro ao alocar memória para imagem\n");
        fclose(arquivo);
        return -7;
    }

    // Move para o início dos dados da imagem
    fseek(arquivo, fileHeader.bfOffBits, SEEK_SET);

    // Lê os dados da imagem
    if (fread(imageData, 1, image_size, arquivo) != image_size) {
        fprintf(stderr, "Erro ao ler dados da imagem\n");
        free(imageData);
        fclose(arquivo);
        return -8;
    }

    fclose(arquivo);

    printf("Carregando imagem no coprocessador...\n");
    printf("Tamanho da linha (com padding): %d bytes\n", row_size);
    
    // Debug: mostra os primeiros pixels
    printf("Primeiros 10 pixels da imagem (original): ");
    for (int i = 0; i < 10 && i < image_size; i++) {
        printf("%d ", imageData[i]);
    }
    printf("\n");
    printf("NOTA: Valores serão invertidos (255 - valor) pois o hardware interpreta ao contrário\n");
    
    time_t inicio = time(NULL);
    int pixels_escritos = 0;
    int erros = 0;
    int timeout_erros = 0;
    int hw_erros = 0;

    // Escreve os pixels considerando o padding das linhas E INVERTENDO OS VALORES
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            // Calcula o offset na imagem considerando o padding
            int offset = y * row_size + x;
            
            // Lê o pixel e INVERTE (255 - valor) porque o hardware está invertido
            uint8_t gray_value = 255 - imageData[offset];
            
            // Calcula o endereço linear (0 a 76799)
            uint32_t address = y * IMG_WIDTH + x;
            
            // Escreve o pixel
            int ret = escrever_pixel(address, gray_value);
            if (ret == 0) {
                pixels_escritos++;
            } else {
                erros++;
                if (ret == -3) timeout_erros++;
                if (ret == -2) hw_erros++;
                
                // Para após muitos erros consecutivos
                if (erros > 100) {
                    fprintf(stderr, "\nMuitos erros consecutivos! Abortando...\n");
                    break;
                }
            }
        }
        
        // Mostra progresso a cada 10%
        if ((y + 1) % (IMG_HEIGHT / 10) == 0) {
            int progresso = ((y + 1) * 100) / IMG_HEIGHT;
            printf("Progresso: %d%% (%d/%d linhas, %d erros)\n", 
                   progresso, y + 1, IMG_HEIGHT, erros);
        }
        
        // Para se muitos erros
        if (erros > 100) break;
    }

    time_t fim = time(NULL);
    double tempo_total = difftime(fim, inicio);

    printf("\n=== Resumo ===\n");
    printf("Pixels escritos com sucesso: %d\n", pixels_escritos);
    printf("Total de erros: %d\n", erros);
    printf("  - Timeouts: %d\n", timeout_erros);
    printf("  - Erros de hardware: %d\n", hw_erros);
    printf("Tempo total: %.0f segundos\n", tempo_total);
    
    if (erros == 0) {
        printf("✓ Imagem carregada com sucesso!\n");
        resultado = 0;
    } else {
        printf("⚠ Imagem carregada com %d erros\n", erros);
        resultado = -9;
    }

    free(imageData);
    return resultado;
}

/**
 * @brief Programa principal
 */
int main() {
    int resultado;

    printf("=== Carregador de Imagem BMP ===\n");
    printf("Arquivo: s.bmp\n");
    printf("Resolução esperada: %dx%d pixels\n\n", IMG_WIDTH, IMG_HEIGHT);

    // Inicializa a API
    resultado = iniciarAPI();
    if (resultado != 0) {
        fprintf(stderr, "Falha ao inicializar API\n");
        return EXIT_FAILURE;
    }

    // Carrega a imagem BMP
    resultado = carregar_imagem_bmp("s.bmp");

    // Finaliza a API
    encerrarAPI();

    return (resultado == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}