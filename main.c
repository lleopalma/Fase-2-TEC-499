#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include "header.h"
#include "hps_0.h"
#include <locale.h>

// Endereço base do barramento lightweight HPS-to-FPGA
#define LW_BRIDGE_BASE 0xFF200000
#define LW_BRIDGE_SPAN 0x00200000

// Ponteiro global para a memória mapeada
static volatile uint32_t *h2p_lw_base = NULL;
static int fd = -1;

// Máscaras para flags
#define FLAG_DONE       0x01
#define FLAG_ERROR      0x02
#define FLAG_MAX_ZOOM   0x04
#define FLAG_MIN_ZOOM   0x08

// Opcodes
#define OPCODE_NHI_ALG  0x03  // Vizinho mais próximo (Zoom In)
#define OPCODE_PR_ALG   0x04  // Replicação de pixel (Zoom In)
#define OPCODE_NH_ALG   0x05  // Vizinho mais próximo (Zoom Out)
#define OPCODE_BA_ALG   0x06  // Média de blocos (Zoom Out)
#define OPCODE_RST      0x07  // Reset

// ===========================================================
// Funções de controle da API
// ===========================================================
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

    printf("API iniciada com sucesso.\n");
    return 0;
}

int encerrarAPI() {
    if (h2p_lw_base && h2p_lw_base != MAP_FAILED)
        munmap((void *)h2p_lw_base, LW_BRIDGE_SPAN);
    if (fd != -1)
        close(fd);

    printf("API finalizada com sucesso.\n");
    return 0;
}

// ===========================================================
// Função auxiliar: aguarda DONE e verifica erro
// ===========================================================
static int aguardar_conclusao(uint32_t timeout_max) {
    uint32_t timeout = 0;
    while (timeout++ < timeout_max) {
        uint32_t flags = *(h2p_lw_base + (PIO_FLAGS / 4));

        if (flags & FLAG_ERROR)
            return -2; // erro de hardware
        if (flags & FLAG_DONE)
            return 0; // sucesso

        usleep(1);
    }
    return -1; // timeout
}

// ===========================================================
// Função genérica para executar instrução
// ===========================================================
static int executar_instrucao(uint32_t opcode) {
    *(h2p_lw_base + (PIO_INSTRUCT / 4)) = opcode;
    usleep(10);
    *(h2p_lw_base + (PIO_ENABLE / 4)) = 1;
    usleep(10);
    *(h2p_lw_base + (PIO_ENABLE / 4)) = 0;

    int r = aguardar_conclusao(0x1000000);
    if (r != 0) return r;

    uint32_t flags = *(h2p_lw_base + (PIO_FLAGS / 4));
    if (flags & FLAG_ERROR)  return -3;
    if (flags & FLAG_MAX_ZOOM) return -4;
    if (flags & FLAG_MIN_ZOOM) return -5;
    return 0;
}

// ===========================================================
// Funções específicas para cada algoritmo
// ===========================================================
int zoom_in_vizinho_proximo() {
    printf("Executando Zoom In (Vizinho Mais Próximo)...\n");
    return executar_instrucao(OPCODE_NHI_ALG);
}

int zoom_in_replicacao() {
    printf("Executando Zoom In (Replicação de Pixel)...\n");
    return executar_instrucao(OPCODE_PR_ALG);
}

int zoom_out_vizinho_proximo() {
    printf("Executando Zoom Out (Decimação)...\n");
    return executar_instrucao(OPCODE_NH_ALG);
}

int zoom_out_media_blocos() {
    printf("Executando Zoom Out (Média de Blocos)...\n");
    return executar_instrucao(OPCODE_BA_ALG);
}

int reset_coprocessador() {
    printf("Executando RESET do coprocessador...\n");
    return executar_instrucao(OPCODE_RST);
}

// ===========================================================
// Exibe flags
// ===========================================================
void exibir_flags() {
    uint32_t flags = *(h2p_lw_base + (PIO_FLAGS / 4));
    printf("Flags atuais: 0x%02X (DONE=%d, ERROR=%d, MAX=%d, MIN=%d)\n",
           flags,
           (flags & FLAG_DONE) ? 1 : 0,
           (flags & FLAG_ERROR) ? 1 : 0,
           (flags & FLAG_MAX_ZOOM) ? 1 : 0,
           (flags & FLAG_MIN_ZOOM) ? 1 : 0);
}

// ===========================================================
// Programa principal com menu
// ===========================================================
int main() 
{
    if (iniciarAPI() != 0) return EXIT_FAILURE;
    setlocale(LC_ALL, "pt_BR.UTF-8");
    int opcao = 0;
    while (1) {
        printf("\n=== MENU COPROCESSADOR ===\n");
        printf("1 - Zoom In (Vizinho Próximo)\n");
        printf("2 - Zoom In (Replicação de Pixel)\n");
        printf("3 - Zoom Out (Decimação)\n");
        printf("4 - Zoom Out (Média de Blocos)\n");
        printf("5 - RESET\n");
        printf("0 - Sair\n");
        printf("Escolha uma opção: ");
        scanf("%d", &opcao);

        int r = 0;
        switch (opcao) {
            case 1: 
                r = zoom_in_vizinho_proximo(); 
                break;
            case 2: 
                r = zoom_in_replicacao(); 
                break;
            case 3: 
                r = zoom_out_vizinho_proximo(); 
                break;
            case 4: 
                r = zoom_out_media_blocos(); 
                break;
            case 5: 
                r = reset_coprocessador(); 
                break;
            case 0: 
                encerrarAPI(); 
                return 0;
            default: 
                printf("Opção inválida.\n");
                continue;
        }

        // Interpreta resultado
        if (r == 0) 
            printf("✓ Operação concluída com sucesso.\n");
        else if (r == -1) 
            printf("✗ Timeout aguardando DONE.\n");
        else if (r == -2) 
            printf("✗ Flag de erro ativada.\n");
        else if (r == -3) 
            printf("✗ Falha de hardware.\n");
        else if (r == -4) 
            printf("⚠ Zoom máximo atingido.\n");
        else if (r == -5) 
            printf("⚠ Zoom mínimo atingido.\n");

        exibir_flags();
    }

    encerrarAPI();
    return 0;
}
