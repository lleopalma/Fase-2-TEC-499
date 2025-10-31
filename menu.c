#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Funções Assembly
extern int iniciarAPI();
extern int encerrarAPI();
extern int write_pixel(unsigned int address, unsigned char data);
extern void Vizinho_Prox();
extern void Replicacao();
extern void Decimacao();
extern void Media_Blocos();
extern void Reset();

int main() {
    int opcao;
    
    printf("\n=== INICIANDO API ===\n");
    if (iniciarAPI() != 0) {
        printf("ERRO ao iniciar API!\n");
        return 1;
    }
    printf("API OK!\n\n");
    
    do {
        printf("\n--- MENU DE TESTES ---\n");
        printf("1. Enviar imagem\n");
        printf("2. Vizinho Proximo\n");
        printf("3. Replicacao\n");
        printf("4. Decimacao\n");
        printf("5. Media Blocos\n");
        printf("6. Reset\n");
        printf("7. Sair\n");
        printf("Opcao: ");
        scanf("%d", &opcao);
        
        switch(opcao) {
            case 1:
                printf("\nEnviando imagem...\n");
                for (int i = 0; i < 19200; i++) {
                    write_pixel(i, 0x00);
                    
                    // Atualizar porcentagem a cada 1%
                    if (i % 192 == 0) {
                        int porcentagem = (i * 100) / 19200;
                        printf("\rProgresso: [");
                        
                        // Barra de progresso
                        int barras = porcentagem / 2;
                        for (int j = 0; j < 50; j++) {
                            if (j < barras) printf("=");
                            else if (j == barras) printf(">");
                            else printf(" ");
                        }
                        
                        printf("] %d%%", porcentagem);
                        fflush(stdout);
                    }
                }
                printf("\rProgresso: [==================================================] 100%%\n");
                printf("Imagem enviada com sucesso!\n");
                break;
                
            case 2:
                printf("\nExecutando Vizinho Proximo...");
                Vizinho_Prox();
                printf(" OK!\n");
                break;
                
            case 3:
                printf("\nExecutando Replicacao...");
                Replicacao();
                printf(" OK!\n");
                break;
                
            case 4:
                printf("\nExecutando Decimacao...");
                Decimacao();
                printf(" OK!\n");
                break;
                
            case 5:
                printf("\nExecutando Media Blocos...");
                Media_Blocos();
                printf(" OK!\n");
                break;
                
            case 6:
                printf("\nExecutando Reset...");
                Reset();
                printf(" OK!\n");
                break;
                
            case 7:
                printf("\nSaindo...\n");
                break;
                
            default:
                printf("\nOpcao invalida!\n");
        }
        
    } while(opcao != 0);
    
    printf("\nEncerrando API...");
    encerrarAPI();
    printf(" OK!\n");
    
    return 0;
}