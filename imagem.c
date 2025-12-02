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
#include <string.h>

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
extern int Flag_Max();
extern int Flag_Min();
extern int Enviar_Coordenadas(int x, int y);

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

// Estrutura para seleção de região
typedef struct {
    int x_inicio;
    int y_inicio;
    int x_fim;
    int y_fim;
    int arrastando;
    int ativa;
} SelecaoRegiao;

// Buffer global para backup da imagem original
unsigned char* imagem_backup = NULL;
// Buffer para guardar a região recortada
unsigned char* imagem_recorte = NULL;
// Coordenadas da região selecionada (nas coordenadas da imagem 320x240)
int regiao_x_min = 0, regiao_y_min = 0, regiao_x_max = 0, regiao_y_max = 0;
int regiao_ativa = 0;

// Função para carregar e enviar imagem BMP
int enviar_imagem_bmp(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("ERRO: Não foi possível abrir o arquivo '%s'\n", filename);
        return -1;
    }

    BMPHeader header;
    BMPInfoHeader info;
    
    fread(&header, sizeof(BMPHeader), 1, file);
    fread(&info, sizeof(BMPInfoHeader), 1, file);

    if (header.type != 0x4D42) {
        printf("ERRO: Arquivo não é um BMP válido!\n");
        fclose(file);
        return -1;
    }

    printf("Dimensões: %dx%d pixels\n", info.width, info.height);
    printf("Bits por pixel: %d\n", info.bits_per_pixel);

    if (info.width != 320 || info.height != 240) {
        printf("AVISO: Imagem com dimensões diferentes de 320x240!\n");
    }

    fseek(file, header.offset, SEEK_SET);

    int bytes_per_pixel = info.bits_per_pixel / 8;
    int row_size = info.width * bytes_per_pixel;
    int padding = (4 - (row_size % 4)) % 4;

    // Aguardar hardware estar pronto
    while(Flag_Done() == 0) {
        usleep(1000);
    }

    printf("\nEnviando imagem...\n");

    unsigned char *row_buffer = (unsigned char*)malloc(row_size);
    if (!row_buffer) {
        printf("ERRO: Falha ao alocar memória!\n");
        fclose(file);
        return -1;
    }

    // Aloca ou realoca buffer de backup
    if (imagem_backup == NULL) {
        imagem_backup = (unsigned char*)malloc(320 * 240);
    }
    
    if (!imagem_backup) {
        printf("ERRO: Falha ao alocar memória para backup!\n");
        free(row_buffer);
        fclose(file);
        return -1;
    }

    int total_pixels = info.width * info.height;
    int pixel_count = 0;

    // BMP armazena de baixo para cima, então invertemos
    for(int y = info.height - 1; y >= 0; y--) {
        fseek(file, header.offset + y * (row_size + padding), SEEK_SET);
        fread(row_buffer, 1, row_size, file);

        for(int x = 0; x < info.width; x++) {
            unsigned char pixel_data;
            
            if (info.bits_per_pixel == 8) {
                pixel_data = row_buffer[x];
            } 
            else if (info.bits_per_pixel == 24) {
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

            int address = (info.height - 1 - y) * info.width + x;
            
            // Salva no backup
            imagem_backup[address] = pixel_data;
            
            // Envia para FPGA
            write_pixel(address, pixel_data);

            pixel_count++;
            
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
    
    // Limpa região anterior ao carregar nova imagem
    regiao_ativa = 0;
    if (imagem_recorte != NULL) {
        free(imagem_recorte);
        imagem_recorte = NULL;
    }
    
    return 0;
}

// Função para restaurar imagem completa na memória do FPGA
void restaurar_imagem_completa() {
    if (imagem_backup == NULL) return;
    
    while(Flag_Done() == 0) {
        usleep(1000);
    }
    
    printf("\n🔄 Restaurando imagem completa...\n");
    for (int i = 0; i < 76800; i++) {
        write_pixel(i, imagem_backup[i]);
        if (i % 5000 == 0) {
            printf("\rProgresso: %.1f%%", (i * 100.0f) / 76800.0f);
            fflush(stdout);
        }
    }
    printf("\r✅ Imagem completa restaurada! (100%%)    \n");
}

// Função para aplicar recorte centralizado
void aplicar_recorte_centralizado() {
    if (!regiao_ativa || imagem_recorte == NULL) return;
    
    int largura_regiao = regiao_x_max - regiao_x_min + 1;
    int altura_regiao = regiao_y_max - regiao_y_min + 1;
    
    int offset_centro_x = (320 - largura_regiao) / 2;
    int offset_centro_y = (240 - altura_regiao) / 2;
    
    while(Flag_Done() == 0) {
        usleep(1000);
    }
    
    printf("\n🖼️  Aplicando recorte centralizado...\n");
    
    int pixels_processados = 0;
    int total_pixels = 320 * 240;
    
    for (int y_dest = 0; y_dest < 240; y_dest++) {
        for (int x_dest = 0; x_dest < 320; x_dest++) {
            int address = y_dest * 320 + x_dest;
            
            int x_regiao = x_dest - offset_centro_x;
            int y_regiao = y_dest - offset_centro_y;
            
            if (x_regiao >= 0 && x_regiao < largura_regiao &&
                y_regiao >= 0 && y_regiao < altura_regiao) {
                
                int x_orig = regiao_x_min + x_regiao;
                int y_orig = regiao_y_min + y_regiao;
                int addr_orig = y_orig * 320 + x_orig;
                
                write_pixel(address, imagem_backup[addr_orig]);
            } else {
                write_pixel(address, 0);
            }
            
            pixels_processados++;
            if (pixels_processados % 2000 == 0) {
                float progresso = (pixels_processados * 100.0f) / total_pixels;
                printf("\rProgresso: %.1f%%    ", progresso);
                fflush(stdout);
            }
        }
    }
    
    printf("\r✅ Recorte aplicado! (100%%)    \n");
}

// Função para centralizar região selecionada e pintar resto de preto
int aplicar_mascara_regiao(unsigned char* imagem_completa, 
                            int x1, int y1, int x2, int y2) {
    
    // Normaliza coordenadas
    int x_min = (x1 < x2) ? x1 : x2;
    int x_max = (x1 < x2) ? x2 : x1;
    int y_min = (y1 < y2) ? y1 : y2;
    int y_max = (y1 < y2) ? y2 : y1;
    
    // A imagem 320x240 está centralizada na tela 640x480
    int offset_x = (640 - 320) / 2;
    int offset_y = (480 - 240) / 2;
    
    // Remove o offset e mapeia para coordenadas da imagem real
    x_min = x_min - offset_x;
    x_max = x_max - offset_x;
    y_min = y_min - offset_y;
    y_max = y_max - offset_y;
    
    // Garante que está dentro dos limites da imagem
    if (x_min < 0) x_min = 0;
    if (x_max >= 320) x_max = 319;
    if (y_min < 0) y_min = 0;
    if (y_max >= 240) y_max = 239;
    
    // Verifica se a seleção está completamente fora da imagem
    if (x_max < 0 || x_min >= 320 || y_max < 0 || y_min >= 240) {
        printf("❌ Seleção fora da área da imagem!\n");
        return -1;
    }
    
    printf("\n📐 Região selecionada (coordenadas da imagem): (%d,%d) → (%d,%d)\n", 
           x_min, y_min, x_max, y_max);
    
    int largura_regiao = x_max - x_min + 1;
    int altura_regiao = y_max - y_min + 1;
    
    if (largura_regiao < 10 || altura_regiao < 10) {
        printf("❌ Região muito pequena! Selecione uma área maior.\n");
        return -1;
    }
    
    printf("📦 Dimensões da região: %dx%d pixels\n", largura_regiao, altura_regiao);
    
    // Salva as coordenadas da região
    regiao_x_min = x_min;
    regiao_y_min = y_min;
    regiao_x_max = x_max;
    regiao_y_max = y_max;
    regiao_ativa = 1;
    
    // Aloca buffer para o recorte se necessário
    if (imagem_recorte == NULL) {
        imagem_recorte = (unsigned char*)malloc(320 * 240);
    }
    
    // Aplica o recorte
    aplicar_recorte_centralizado();
    
    return 0;
}

// Função para processar seleção de região
void processar_selecao_regiao(int fd) {
    struct input_event ev;
    int screen_width = 640;
    int screen_height = 480;
    int acum_x = screen_width / 2;
    int acum_y = screen_height / 2;
    int primeiro_clique = 0;
    
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║   🎯 MODO SELEÇÃO DE REGIÃO CENTRALIZADA      ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    printf("  • CLIQUE 1: Define primeiro canto da região\n");
    printf("  • CLIQUE 2: Define segundo canto e aplica\n");
    printf("  • Região será centralizada na tela 320x240\n");
    printf("  • Resto da tela ficará preto\n");
    printf("  • Botão DIREITO para cancelar\n");
    printf("════════════════════════════════════════════════\n\n");

    SelecaoRegiao sel = {0, 0, 0, 0, 0, 0};
    
    Enviar_Coordenadas(acum_x, acum_y);

    while (1) {
        read(fd, &ev, sizeof(struct input_event));
        int atualizar_coord = 0;

        if (ev.type == EV_REL && ev.code == REL_X) {
            acum_x += ev.value;
            if (acum_x < 0) acum_x = 0;
            if (acum_x >= screen_width) acum_x = screen_width - 1;
            atualizar_coord = 1;
        }
        
        if (ev.type == EV_REL && ev.code == REL_Y) {
            acum_y += ev.value;
            if (acum_y < 0) acum_y = 0;
            if (acum_y >= screen_height) acum_y = screen_height - 1;
            atualizar_coord = 1;
        }
        
        if (atualizar_coord) {
            Enviar_Coordenadas(acum_x, acum_y);
            if (primeiro_clique) {
                printf("\r🖱️  Movendo: Ponto 1=(%3d,%3d) | Ponto 2=(%3d,%3d)    ", 
                       sel.x_inicio, sel.y_inicio, acum_x, acum_y);
            } else {
                printf("\rPosição: X=%3d, Y=%3d    ", acum_x, acum_y);
            }
            fflush(stdout);
        }

        if (ev.type == EV_KEY && ev.code == BTN_LEFT && ev.value == 1) {
            if (!primeiro_clique) {
                sel.x_inicio = acum_x;
                sel.y_inicio = acum_y;
                primeiro_clique = 1;
                printf("\n✅ Ponto 1 marcado: (%d, %d)\n", acum_x, acum_y);
                printf("👉 Mova o mouse e clique novamente para definir o segundo ponto\n");
            }
            else {
                sel.x_fim = acum_x;
                sel.y_fim = acum_y;
                sel.ativa = 1;
                
                printf("\n✅ Ponto 2 marcado: (%d, %d)\n", acum_x, acum_y);
                printf("📐 Região selecionada (VGA): (%d,%d) → (%d,%d)\n", 
                       sel.x_inicio, sel.y_inicio, sel.x_fim, sel.y_fim);
                
                if (imagem_backup == NULL) {
                    printf("❌ ERRO: Nenhuma imagem carregada!\n");
                    break;
                }
                
                printf("\n🔧 Centralizando região selecionada...\n");
                if (aplicar_mascara_regiao(imagem_backup,
                                           sel.x_inicio, sel.y_inicio,
                                           sel.x_fim, sel.y_fim) == 0) {
                    printf("\n✨ Use a opção 3 para dar zoom na região centralizada!\n");
                }
                break;
            }
        }

        if (ev.type == EV_KEY && ev.code == BTN_RIGHT && ev.value == 1) {
            printf("\n❌ Seleção cancelada\n");
            break;
        }
    }
}

// Função de zoom com controle automático de recorte e escolha de operação
void zoom_com_mouse(int fd) {
    struct input_event ev;
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║          🔍 MODO ZOOM COM MOUSE               ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    // Perguntar tipo de zoom IN apenas UMA VEZ
    printf("╔════════════════════════════════════╗\n");
    printf("║      🔍 ESCOLHA O TIPO DE ZOOM IN  ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║ 1. Vizinho Próximo                 ║\n");
    printf("║ 2. Replicação                      ║\n");
    printf("╚════════════════════════════════════╝\n");
    printf("Opção: ");
    int tipo_zoom_in;
    scanf("%d", &tipo_zoom_in);
    getchar(); // Limpa buffer
    
    if (tipo_zoom_in != 1 && tipo_zoom_in != 2) {
        printf("❌ Opção inválida! Usando Vizinho Próximo.\n");
        tipo_zoom_in = 1;
    }
    
    // Perguntar tipo de zoom OUT apenas UMA VEZ
    printf("\n╔════════════════════════════════════╗\n");
    printf("║      🔍 ESCOLHA O TIPO DE ZOOM OUT ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║ 1. Média                           ║\n");
    printf("║ 2. Decimação                       ║\n");
    printf("╚════════════════════════════════════╝\n");
    printf("Opção: ");
    int tipo_zoom_out;
    scanf("%d", &tipo_zoom_out);
    getchar(); // Limpa buffer
    
    if (tipo_zoom_out != 1 && tipo_zoom_out != 2) {
        printf("❌ Opção inválida! Usando Média.\n");
        tipo_zoom_out = 1;
    }
    
    printf("\n════════════════════════════════════════════════\n");
    printf("✅ Configuração:\n");
    printf("   Zoom IN  → %s\n", tipo_zoom_in == 1 ? "Vizinho Próximo" : "Replicação");
    printf("   Zoom OUT → %s\n", tipo_zoom_out == 1 ? "Média" : "Decimação");
    printf("════════════════════════════════════════════════\n");
    printf("  • Mova o mouse para posicionar cursor\n");
    printf("  • Scroll UP: Zoom IN (dinâmico)\n");
    printf("  • Scroll DOWN: Zoom OUT (dinâmico)\n");
    printf("  • Botão ESQUERDO: Reset + Sair\n");
    if (regiao_ativa) {
        printf("  • RECORTE: Zoom OUT até 320x240 → ORIGINAL\n");
        printf("  • ORIGINAL: Zoom IN até recorte → RECORTE\n");
    }
    printf("  • Limite mínimo: 320x240 (resolução original)\n");
    printf("════════════════════════════════════════════════\n\n");

    int verification = 1;
    int screen_width = 640;
    int screen_height = 480;
    int acum_x = screen_width / 2;
    int acum_y = screen_height / 2;
    int modo_recorte = regiao_ativa;
    
    int largura_recorte_original = 0;
    int altura_recorte_original = 0;
    if (regiao_ativa) {
        largura_recorte_original = regiao_x_max - regiao_x_min + 1;
        altura_recorte_original = regiao_y_max - regiao_y_min + 1;
    }

    Enviar_Coordenadas(acum_x, acum_y);
    printf("Posição inicial: X=%d, Y=%d\n", acum_x, acum_y);
    if (modo_recorte) {
        printf("🖼️  Modo: RECORTE [%dx%d]\n\n", largura_recorte_original, altura_recorte_original);
    } else {
        printf("🖼️  Modo: IMAGEM COMPLETA [320x240]\n\n");
    }

    while (verification) {
        read(fd, &ev, sizeof(struct input_event));
        int atualizar_coord = 0;

        if (ev.type == EV_REL && ev.code == REL_X) {
            acum_x += ev.value;
            if (acum_x < 0) acum_x = 0;
            if (acum_x >= screen_width) acum_x = screen_width - 1;
            atualizar_coord = 1;
        }
        
        if (ev.type == EV_REL && ev.code == REL_Y) {
            acum_y += ev.value;
            if (acum_y < 0) acum_y = 0;
            if (acum_y >= screen_height) acum_y = screen_height - 1;
            atualizar_coord = 1;
        }
        
        if (atualizar_coord) {
            Enviar_Coordenadas(acum_x, acum_y);
            printf("\rPosição: X=%3d, Y=%3d | Modo: %s    ", 
                   acum_x, acum_y, 
                   modo_recorte ? "RECORTE" : "ORIGINAL");
            fflush(stdout);
        }

        if (ev.type == EV_KEY && ev.code == BTN_LEFT && ev.value == 1) {
            printf("\n🔄 Botão esquerdo pressionado. Resetando para imagem original...\n");
            restaurar_imagem_completa();
            regiao_ativa = 0;
            modo_recorte = 0;
            Reset();
            printf("✅ Imagem restaurada! Saindo do zoom...\n");
            verification = 0;
            break;
        }
        
        if (ev.type == EV_REL && ev.code == REL_WHEEL) {
            // ========== ZOOM IN ==========
            if (ev.value > 0) {
                // Aplica o tipo de zoom IN escolhido no início
                if (tipo_zoom_in == 1) {
                    Vizinho_Prox();
                } else {
                    Replicacao();
                }
                
                // Verifica se atingiu limite máximo
                usleep(50000);
                if (Flag_Max() != 0) {
                    if (regiao_ativa && !modo_recorte) {
                        printf("\n🔄 Tamanho do recorte atingido! Voltando para modo RECORTE...\n");
                        aplicar_recorte_centralizado();
                        modo_recorte = 1;
                        Reset();
                    }
                }
            } 
            // ========== ZOOM OUT ==========
            else if (ev.value < 0) {
                // Aplica o tipo de zoom OUT escolhido no início
                if (tipo_zoom_out == 1) {
                    Media();
                } else {
                    Decimacao();
                }
                
                // Verifica se atingiu 320x240 APÓS aplicar o zoom out
                usleep(50000);
                if (Flag_Min() != 0) {
                    if (modo_recorte && regiao_ativa) {
                        printf("\n🔄 Tamanho 320x240 atingido! Mudando para modo ORIGINAL...\n");
                        restaurar_imagem_completa();
                        modo_recorte = 0;
                        Reset();
                    }
                }
            } 
        }
    }
}

int main() {
    int opcao;
    int fd;
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║   SISTEMA DE PROCESSAMENTO DE IMAGEM  ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("INICIANDO API...\n");
    if (iniciarBib() != 0) {
        printf("❌ ERRO ao iniciar API!\n");
        return 1;
    }
    printf("✅ API em FUNCIONAMENTO!\n\n");

    fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) {
        perror("❌ Erro ao abrir mouse");
        encerrarBib();
        return 1;
    }
    
    int continuar = 1;
    Reset();
    
    do {
        printf("\n╔════════════════════════════════════════╗\n");
        printf("║          MENU PRINCIPAL                ║\n");
        printf("╠════════════════════════════════════════╣\n");
        printf("║ 1. Carregar imagem BMP                 ║\n");
        printf("║ 2. Selecionar e centralizar região     ║\n");
        printf("║ 3. Zoom com mouse                      ║\n");
        printf("║ 4. Resetar imagem original             ║\n");
        printf("║ 5. Sair                                ║\n");
        printf("╚════════════════════════════════════════╝\n");
        if (regiao_ativa) {
            printf("📌 Região recortada ativa: (%d,%d) → (%d,%d)\n", 
                   regiao_x_min, regiao_y_min, regiao_x_max, regiao_y_max);
        }
        printf("Opção: ");
        scanf("%d", &opcao);
        getchar(); // Limpa buffer
        
        switch(opcao) {
            case 1: {
                char nome_arquivo[256];
                printf("\n📁 Digite o nome do arquivo BMP (ex: imagem.bmp): ");
                scanf("%s", nome_arquivo);
                getchar(); // Limpa buffer
                printf("Carregando '%s'...\n", nome_arquivo);
                if (enviar_imagem_bmp(nome_arquivo) == 0) {
                    Reset();
                } else {
                    printf("❌ Falha ao carregar imagem!\n");
                }
                break;
            }
                
            case 2:
                if (imagem_backup == NULL) {
                    printf("\n❌ Carregue uma imagem primeiro (opção 1)!\n");
                } else {
                    processar_selecao_regiao(fd);
                }
                break;
                
            case 3:
                if (imagem_backup == NULL) {
                    printf("\n❌ Carregue uma imagem primeiro (opção 1)!\n");
                } else {
                    zoom_com_mouse(fd);
                }
                break;
                
            case 4:
                if (imagem_backup == NULL) {
                    printf("\n❌ Nenhuma imagem carregada!\n");
                } else {
                    printf("\n🔄 Restaurando imagem original...\n");
                    restaurar_imagem_completa();
                    regiao_ativa = 0;
                    Reset();
                    printf("✅ Imagem restaurada para 320x240!\n");
                }
                break;
                
            case 5:
                printf("\n👋 Saindo...\n");
                continuar = 0;
                break;
                
            default:
                printf("\n❌ Opção inválida!\n");
        }
        
    } while(continuar);
    
    printf("\n🔚 Encerrando programa...\n");
    
    if (imagem_backup != NULL) {
        free(imagem_backup);
    }
    
    if (imagem_recorte != NULL) {
        free(imagem_recorte);
    }
    
    close(fd);
    encerrarBib();
    
    return 0;
}