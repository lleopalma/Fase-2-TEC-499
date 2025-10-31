#ifndef HEADER_H
#define HEADER_H

// Offsets dos registradores PIO
#define PIO_INSTRUCT 0x00
#define PIO_ENABLE   0x10
#define PIO_FLAGS    0x30

// Constantes de Controle e Status
#define VRAM_MAX_ADDR 76800   // Endereço máximo da VRAM (0x4B00)
#define STORE_OPCODE  0x02    // Opcode para operação de escrita/armazenamento
#define FLAG_DONE_MASK 0x01   // Máscara para o bit 'DONE' (operação concluída)
#define FLAG_ERROR_MASK 0x02  // Máscara para o bit 'ERROR' (erro de hardware)
#define TIMEOUT_COUNT 0x0 // Valor de timeout para a operação de hardware

/**
 * @brief Inicializa a API, abrindo /dev/mem e mapeando o endereço base do FPGA na memória.
 * @details Mapeia o endereço 0xFF200000 (LW_BASE) com tamanho 0x1000 (LW_SPAM).
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int iniciarAPI();

/**
 * @brief Finaliza a API, desmapeando a memória e fechando o descritor de arquivo.
 * @return 0 em caso de sucesso, -1 em caso de erro de munmap.
 */
int encerrarAPI();

/**
 * @brief Escreve um valor de pixel (data) em um endereço específico da VRAM.
 * @details Empacota o 'address' (r0) e 'data' (r1) na instrução do PIO e espera a conclusão.
 * @param address Endereço de memória de vídeo (0 a VRAM_MAX_ADDR - 1).
 * @param data Valor de 8 bits a ser escrito. O Assembly empacota os bits de 28:21 (data) e 19:3 (address)
 * @return 0 em sucesso. -1 (INVALID_ADDR), -2 (TIMEOUT), -3 (HW_ERROR).
 */
int write_pixel(unsigned int address, unsigned char data);

/**
 * @brief Inicia o processamento de 'Vizinho Próximo'.
 * @details Envia a instrução 3 para o PIO.
 */
void Vizinho_Prox();

/**
 * @brief Inicia o processamento de 'Replicação'.
 * @details Envia a instrução 4 para o PIO.
 */
void Replicacao();

/**
 * @brief Inicia o processamento de 'Decimação'.
 * @details Envia a instrução 5 para o PIO.
 */
void Decimacao();

/**
 * @brief Inicia o processamento de 'Média de Blocos'.
 * @details Envia a instrução 6 para o PIO.
 */
void Media_Blocos();

#endif
