build:
	@gcc -c imagem.c -std=c99 -o imagem.o
	@gcc -c api.s -o api.o
	@gcc api.o imagem.o -o scr

run:
	sudo ./scr

help:
	@echo ""
	@echo "📘 Comandos disponíveis:"
	@echo "  make build  - Compila o programa (gera pixel_test)"
	@echo "  make run    - Executa o programa (usa sudo)"
	@echo "  make help   - Mostra esta mensagem de ajuda"
	@echo ""

