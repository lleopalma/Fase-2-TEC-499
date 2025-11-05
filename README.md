<h1 align="center"> Integração HPS–FPGA na DE1-SoC - API para Cooprocessador de Imagens </h1>  
<h2>Descrição do Projeto</h2>
<p>
Nesta etapa foi desenvolvida uma API em Assembly para gerenciar a comunicação entre o HPS (Hard Processor System) e o coprocessador gráfico na FPGA, implementado em Verilog. Essa interface permite o envio de instruções e dados entre o processador ARM e o módulo de redimensionamento de imagens, viabilizando o controle das operações de zoom. A API atua como um driver de baixo nível, traduzindo os comandos do software em instruções da ISA do coprocessador e garantindo a integração entre o sistema embarcado e a lógica de hardware da DE1-SoC.

O principal objetivo desta etapa é estabelecer um protocolo de comunicação eficiente e confiável entre o software em C, executado no HPS com Linux embarcado, e a lógica do coprocessador implementado na FPGA. Para isso, são utilizadas as linguagens Assembly e Verilog, que trabalham em conjunto para garantir a troca precisa de dados e comandos entre o processador ARM e o hardware dedicado, assegurando desempenho, sincronização e compatibilidade no sistema integrado.
</p>

Sumário
=================
<!--ts-->
* [Arquitetura da API e Integração HPS–FPGA](#arquitetura)
* [Fluxo de Execução da API](#fluxo)
* [Implementação da Biblioteca em Assembly](#assembly)
* [Protocolo de Comunicação e Flags](#protocolo)
* [Interface em C e Funções de Controle](#c)
* [Análise dos Resultados Alcançados](#analise)
* [Referências](#referencias)
<!--te-->

<h2 id="arquitetura">Arquitetura da API e Integração HPS–FPGA</h2>

A arquitetura desenvolvida nesta etapa tem como foco estabelecer a comunicação direta entre o HPS (Hard Processor System) e a FPGA da placa DE1-SoC, permitindo que o processador ARM controle o coprocessador gráfico implementado em Verilog. Essa interação ocorre através do barramento Lightweight (LW Bridge), que mapeia os registradores da FPGA em um espaço de memória acessível ao HPS por meio do endereço físico da placa.

O código em Assembly ARM atua como uma camada de abstração de baixo nível, responsável por realizar o mapeamento de memória, envio de instruções e leitura de flags do hardware. Ele acessa diretamente os registradores PIO definidos na FPGA — PIO_INSTRUCT, PIO_ENABLE e PIO_FLAGS — para transmitir comandos e receber o status de execução do coprocessador.

No lado do software em C, a API se conecta a essa base Assembly utilizando funções de sistema como open(), mmap() e munmap() para criar a ponte entre o Linux embarcado e o hardware. Dessa forma, o programa principal (main.c) consegue enviar instruções de zoom, replicação, decimação e média de blocos ao módulo da FPGA com apenas chamadas de função, sem necessidade de interação direta com o hardware.

A comunicação entre as duas camadas segue o seguinte fluxo:

1. O HPS envia uma instrução para o registrador PIO_INSTRUCT;
2. O bit de execução é ativado em PIO_ENABLE;
3. A FPGA processa o comando e retorna o estado de conclusão ou erro em PIO_FLAGS;
4. A API interpreta esses sinais e retorna o resultado para o programa em C.

Essa arquitetura garante sincronismo, baixo atraso e controle total via software, permitindo que o coprocessador gráfico seja manipulado de forma segura e eficiente pelo HPS dentro do ambiente Linux embarcado.

<h2 id="fluxo">Fluxo de Execução da API</h2>


<p>
O ciclo básico de funcionamento da API segue uma sequência bem definida de etapas que garantem a comunicação segura e sincronizada entre o HPS e a FPGA. Cada parte do código tem uma função específica nesse processo, controlando tanto o acesso à memória quanto a execução das instruções no coprocessador implementado em Verilog.
</p>

<h3>1. Inicialização do barramento e mapeamento de memória</h3>
<p>
O processo começa com a função de inicialização da API, que abre o dispositivo de memória física e realiza o mapeamento do endereço base do barramento Lightweight (0xFF200000) para o espaço de endereçamento do processo no Linux. Esse mapeamento cria um ponteiro que permite ao programa acessar diretamente os registradores PIO da FPGA, estabelecendo o canal de comunicação entre software e hardware.
</p>

<h3>2. Envio de instruções ao coprocessador</h3>
<p>
Com o barramento inicializado, o software envia instruções ao coprocessador por meio da escrita nos registradores de controle da FPGA. A instrução é armazenada no registrador responsável por receber comandos, e o sinal de ativação é aplicado para iniciar o processamento da operação selecionada, como zoom, replicação ou média de blocos.
</p>

<h3>3. Espera pelo sinal de conclusão (DONE)</h3>
<p>
Após o envio da instrução, a API entra em um loop de espera, monitorando o registrador de flags da FPGA. O programa permanece nesse estado até que o sinal de conclusão seja ativado, indicando que o processamento terminou. Durante essa fase, também é verificado se houve algum erro de hardware.
</p>

<h3>4. Leitura de flags de status e tratamento de erros</h3>
<p>
Quando o sinal de conclusão é detectado, a API realiza a leitura dos registradores de status para confirmar o resultado da operação. Se uma flag de erro estiver ativada, o sistema retorna um código de falha e interrompe o processo. Caso contrário, a execução é considerada bem-sucedida. As flags adicionais, como de limite máximo e mínimo de zoom, também são verificadas para informar o estado atual do sistema.
</p>

<h3>5. Encerramento e liberação de recursos</h3>
<p>
Após o término das operações, a API executa a função de encerramento, que libera todos os recursos utilizados. O mapeamento de memória é desfeito e o descritor do dispositivo é fechado, evitando vazamentos de memória e garantindo que o hardware possa ser reutilizado com segurança.
</p>


<h2 id="assembly">Implementação da Biblioteca em Assembly</h2>

<p>
A biblioteca em Assembly ARM foi desenvolvida para fornecer uma camada de comunicação direta entre o processador HPS e o coprocessador implementado na FPGA. Seu papel é traduzir as operações de alto nível da API em comandos binários que controlam o hardware, manipulando registradores e sinalizações com precisão e baixo atraso. Todas as funções seguem o padrão de chamada do ARM, utilizando registradores para passagem de parâmetros e retorno de valores.
</p>

<h3>Funções de inicialização e encerramento</h3>
<p>
As rotinas de inicialização e finalização, chamadas de <strong>iniciarBib</strong> e <strong>encerrarBib</strong>, são responsáveis por preparar e liberar o ambiente de comunicação entre o HPS e a FPGA. 
Na inicialização, ocorre a abertura do dispositivo de memória física e o mapeamento do endereço base da FPGA para o espaço de endereçamento do processador. 
Essa etapa garante que o software possa acessar diretamente os registradores da FPGA. 
No encerramento, o mapeamento é desfeito e o descritor de arquivo é fechado, evitando vazamento de memória e garantindo o desligamento seguro do sistema.
</p>

<h3>Função de escrita na VRAM</h3>
<p>
A função <strong>write_pixel</strong> tem como função principal enviar dados de pixel à memória de vídeo do coprocessador. 
Ela empacota o endereço e o valor do pixel em uma instrução única, que é transmitida ao registrador de instrução da FPGA. 
Após o envio, a função ativa o registrador de execução e aguarda o sinal de conclusão. 
Caso o endereço informado seja inválido ou ocorra erro de hardware, a rotina retorna um código de erro específico. 
Essa função é essencial para o controle de escrita de dados gráficos diretamente pela API.
</p>
<h3>Funções de leitura de status</h3>
<p>
As funções <strong>Flag_Done</strong>, <strong>Flag_Error</strong>, <strong>Flag_Max</strong> e <strong>Flag_Min</strong> realizam a leitura do registrador de status da FPGA, interpretando o estado atual do coprocessador.  
Cada função verifica um bit específico de sinalização, indicando se o processamento foi concluído, se houve erro de execução ou se o sistema atingiu os limites de zoom máximo ou mínimo.  
Essas leituras permitem que o software em C interprete com precisão o estado do hardware e trate automaticamente cada situação, garantindo confiabilidade na comunicação entre HPS e FPGA.
</p>

<p>
A tabela a seguir resume as funções de leitura e o significado de cada flag de status:
</p>

<table>
  <thead>
    <tr>
      <th>Função Assembly</th>
      <th>Máscara (Flag)</th>
      <th>Descrição</th>
      <th>Ação Esperada no Software</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Flag_Done</td>
      <td>0x01</td>
      <td>Indica que a operação foi concluída com sucesso.</td>
      <td>Prosseguir com a execução normal e exibir mensagem de sucesso.</td>
    </tr>
    <tr>
      <td>Flag_Error</td>
      <td>0x02</td>
      <td>Sinaliza erro de hardware ou falha durante o processamento.</td>
      <td>Interromper a operação e exibir aviso de erro ao usuário.</td>
    </tr>
    <tr>
      <td>Flag_Max</td>
      <td>0x03</td>
      <td>Indica que o limite máximo de zoom foi atingido.</td>
      <td>Bloquear novas tentativas de ampliação até o reset do coprocessador.</td>
    </tr>
    <tr>
      <td>Flag_Min</td>
      <td>0x04</td>
      <td>Indica que o limite mínimo de zoom foi atingido.</td>
      <td>Impedir novas operações de redução até o reset do sistema.</td>
    </tr>
  </tbody>
</table>

<p>
Com essa estrutura de sinalização, o HPS pode monitorar o progresso e o estado de cada operação enviada à FPGA, reagindo de forma controlada e segura conforme o resultado retornado pelo coprocessador.
</p>

<h3>Funções de controle de operação</h3>

<p>
As funções <strong>Vizinho_Prox</strong>, <strong>Replicacao</strong>, <strong>Decimacao</strong>, <strong>Media</strong> e <strong>Reset</strong> são responsáveis por enviar comandos de processamento à FPGA. 
Cada uma delas grava um código de operação específico no registrador de instrução e ativa o sinal de execução, iniciando o algoritmo correspondente no coprocessador. 
Essas rotinas são utilizadas para acionar os diferentes métodos de redimensionamento de imagem implementados em Verilog, enquanto a função de reset restaura o estado interno do sistema e limpa as sinalizações de controle.
</p>

<table>
  <thead>
    <tr>
      <th>Função Assembly</th>
      <th>Opcode Enviado</th>
      <th>Algoritmo Executado</th>
      <th>Descrição da Operação</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Vizinho_Prox</td>
      <td>0x03</td>
      <td>Zoom In - Vizinho Mais Próximo</td>
      <td>Duplica pixels próximos, ampliando a imagem sem filtragem.</td>
    </tr>
    <tr>
      <td>Replicacao</td>
      <td>0x04</td>
      <td>Zoom In - Replicação de Pixels</td>
      <td>Amplia a imagem repetindo valores de pixels, aumentando a resolução.</td>
    </tr>
    <tr>
      <td>Decimacao</td>
      <td>0x05</td>
      <td>Zoom Out - Decimação</td>
      <td>Reduz a imagem descartando pixels de forma controlada.</td>
    </tr>
    <tr>
      <td>Media</td>
      <td>0x06</td>
      <td>Zoom Out - Média de Blocos</td>
      <td>Reduz a imagem calculando a média dos pixels em blocos adjacentes.</td>
    </tr>
    <tr>
      <td>Reset</td>
      <td>0x07</td>
      <td>Reinicialização do Coprocessador</td>
      <td>Limpa os registradores e sinalizações da FPGA antes de uma nova execução.</td>
    </tr>
  </tbody>
</table>

<p>
Cada chamada dessas funções faz com que o HPS envie à FPGA o opcode correspondente, acionando o módulo responsável pelo algoritmo de redimensionamento de imagem. 
Essa estrutura modular facilita o controle do coprocessador e torna a API flexível para futuras expansões ou inclusão de novos algoritmos.
</p>

<h3>Protocolo de chamada e uso de registradores</h3>
<p>
Todas as rotinas seguem o protocolo de chamada padrão do ARM, utilizando registradores para passagem de parâmetros e retorno de resultados. 
Os registradores de uso geral armazenam endereços, dados e códigos de operação, enquanto as instruções de chamada ao sistema, representadas por <strong>svc</strong>, são utilizadas para executar funções do sistema operacional, como abrir, mapear e fechar dispositivos de memória. 
Esse modelo de comunicação garante desempenho elevado, controle direto sobre o hardware e integração transparente com o código em C da API.
</p>

<h2 id="protocolo">Protocolo de Comunicação HPS–FPGA</h2>

<p>
O protocolo de comunicação entre o processador ARM (HPS) e a FPGA foi desenvolvido para garantir uma troca de comandos simples, direta e sincronizada. 
A interface utiliza três registradores principais mapeados no barramento <em>Lightweight (LW Bridge)</em> — <strong>PIO_INSTRUCT</strong>, <strong>PIO_ENABLE</strong> e <strong>PIO_FLAGS</strong> — responsáveis pelo envio de instruções, ativação de execução e leitura de status, respectivamente.
</p>

<h3>Etapas do Protocolo</h3>

<ol>
  <li>
    <strong>Escrita da instrução em PIO_INSTRUCT:</strong>  
    O HPS envia um código de operação (opcode) correspondente ao algoritmo desejado, como zoom, replicação ou reset.  
    Esse valor é gravado no registrador de instruções da FPGA.
  </li>
  <li>
    <strong>Ativação do bit em PIO_ENABLE:</strong>  
    Após a escrita da instrução, o HPS ativa o sinal de execução, escrevendo o valor “1” em <strong>PIO_ENABLE</strong>.  
    Em seguida, esse mesmo bit é zerado para indicar que o comando foi enviado e aguardar a resposta da FPGA.
  </li>
  <li>
    <strong>Leitura do resultado em PIO_FLAGS:</strong>  
    O registrador de status (<strong>PIO_FLAGS</strong>) é monitorado até que o bit de conclusão seja acionado.  
    A FPGA atualiza esse registrador para informar o término da operação ou a ocorrência de erros e limites de zoom.
  </li>
</ol>

<h3>Máscaras de Flag</h3>

<p>
As máscaras de flag representam bits específicos dentro do registrador <strong>PIO_FLAGS</strong>, responsáveis por sinalizar o estado atual da execução.  
Cada flag é verificada pela API em C para interpretar o resultado retornado pela FPGA.
</p>

<table>
  <thead>
    <tr>
      <th>Flag</th>
      <th>Máscara (Hexadecimal)</th>
      <th>Significado</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>DONE</td>
      <td>0x01</td>
      <td>Indica que a operação foi concluída com sucesso e os dados estão prontos.</td>
    </tr>
    <tr>
      <td>ERROR</td>
      <td>0x02</td>
      <td>Informa que houve falha de execução no coprocessador (erro de hardware ou instrução inválida).</td>
    </tr>
    <tr>
      <td>ZOOM_MAX</td>
      <td>0x04</td>
      <td>Indica que o limite máximo de ampliação foi atingido, impossibilitando novo zoom in.</td>
    </tr>
    <tr>
      <td>ZOOM_MIN</td>
      <td>0x08</td>
      <td>Indica que o limite mínimo de redução foi alcançado, bloqueando novas operações de zoom out.</td>
    </tr>
  </tbody>
</table>

<h3>Códigos de Retorno da API</h3>

<p>
A API implementada em C utiliza códigos de retorno padronizados para indicar o resultado das operações executadas, conforme descrito abaixo:
</p>

<table>
  <thead>
    <tr>
      <th>Código</th>
      <th>Significado</th>
      <th>Descrição</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>0</td>
      <td>Sucesso</td>
      <td>A operação foi concluída corretamente e o coprocessador respondeu com DONE.</td>
    </tr>
    <tr>
      <td>-1</td>
      <td>Timeout</td>
      <td>O HPS aguardou o sinal DONE por tempo excessivo sem resposta da FPGA.</td>
    </tr>
    <tr>
      <td>-2</td>
      <td>Erro de Hardware</td>
      <td>A flag ERROR foi acionada, indicando falha na execução ou instrução inválida.</td>
    </tr>
    <tr>
      <td>-3</td>
      <td>Falha de Comunicação</td>
      <td>Erro de mapeamento ou acesso à memória física do FPGA.</td>
    </tr>
    <tr>
      <td>-4</td>
      <td>Zoom Máximo</td>
      <td>O limite de ampliação foi atingido; nenhuma nova operação de zoom in é permitida.</td>
    </tr>
    <tr>
      <td>-5</td>
      <td>Zoom Mínimo</td>
      <td>O limite de redução foi alcançado; não é possível diminuir mais a imagem.</td>
    </tr>
  </tbody>
</table>

<p>
Esse protocolo garante comunicação confiável entre o HPS e a FPGA, permitindo que o software controle o coprocessador de forma síncrona e segura, 
detectando automaticamente erros e condições de limite sem comprometer a integridade do sistema.
</p>



<h2 id="c">Interface em C e Funções de Controle</h2>

<p>
A camada em linguagem C atua como o elo entre o software em alto nível e a API de baixo nível desenvolvida em Assembly, 
permitindo que o processador ARM do HPS envie comandos e monitore o funcionamento do coprocessador na FPGA de forma estruturada e segura. 
Essa camada abstrai detalhes de mapeamento de memória e comunicação, oferecendo funções de fácil utilização pelo desenvolvedor.
</p>

<h3>Funções Principais</h3>

<table>
  <thead>
    <tr>
      <th>Função</th>
      <th>Descrição</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><strong>iniciarAPI()</strong></td>
      <td>Abre o dispositivo <code>/dev/mem</code> e realiza o mapeamento da região de memória correspondente à FPGA através do barramento <em>Lightweight</em>. 
      Essa função inicializa os ponteiros de acesso e valida a comunicação entre o HPS e o hardware.</td>
    </tr>
    <tr>
      <td><strong>encerrarAPI()</strong></td>
      <td>Libera os recursos utilizados pela API, encerrando o mapeamento de memória e fechando o descritor de arquivo. 
      Garante a finalização limpa e evita vazamentos de recurso no sistema embarcado.</td>
    </tr>
    <tr>
      <td><strong>executar_instrucao()</strong></td>
      <td>É a função central da API em C. Recebe um opcode e o envia ao registrador <strong>PIO_INSTRUCT</strong>, ativa o bit de execução (<strong>PIO_ENABLE</strong>), 
      e aguarda a conclusão do processamento monitorando o registrador <strong>PIO_FLAGS</strong>. 
      Essa rotina genérica é utilizada internamente por todas as funções de controle de zoom e reset.</td>
    </tr>
    <tr>
      <td><strong>zoom_in_vizinho_proximo()</strong></td>
      <td>Aciona o algoritmo de ampliação da imagem utilizando o método de <em>vizinho mais próximo</em> na FPGA.</td>
    </tr>
    <tr>
      <td><strong>zoom_in_replicacao()</strong></td>
      <td>Executa o algoritmo de ampliação via <em>replicação de pixels</em>, aumentando o tamanho da imagem mantendo padrões de cor.</td>
    </tr>
    <tr>
      <td><strong>zoom_out_vizinho_proximo()</strong></td>
      <td>Aplica a técnica de <em>decimação</em>, reduzindo o tamanho da imagem com base em amostragem de pixels vizinhos.</td>
    </tr>
    <tr>
      <td><strong>zoom_out_media_blocos()</strong></td>
      <td>Executa o algoritmo de redução por <em>média de blocos</em>, suavizando a imagem ao reduzir sua resolução.</td>
    </tr>
    <tr>
      <td><strong>reset_coprocessador()</strong></td>
      <td>Envia a instrução de reset (opcode 0x07) para restaurar o estado inicial do coprocessador, limpando os sinais de status e flags pendentes.</td>
    </tr>
    <tr>
      <td><strong>exibir_flags()</strong></td>
      <td>Lê o registrador <strong>PIO_FLAGS</strong> e apresenta, no terminal, os estados das flags de status: DONE, ERROR, MAX e MIN. 
      Essa função auxilia no monitoramento e depuração em tempo real.</td>
    </tr>
  </tbody>
</table>

<h3>Fluxo de Operação e Menu Interativo</h3>

<p>
O arquivo <strong>main.c</strong> implementa um menu interativo que permite ao usuário testar diretamente o coprocessador a partir do terminal do Linux embarcado. 
Após inicializar a API, o usuário pode selecionar a operação desejada (zoom in, zoom out ou reset), e a função correspondente é chamada. 
Cada execução envia um opcode à FPGA, aguarda o sinal <em>DONE</em> e exibe o resultado de status e eventuais mensagens de erro.
</p>

<p>
Esse menu facilita a validação funcional do sistema HPS–FPGA, permitindo testes rápidos das operações gráficas e verificação das respostas do coprocessador sem a necessidade de recompilar o código. 
Assim, a interface em C atua como uma camada de alto nível que controla, valida e supervisiona o comportamento do hardware em tempo real.
</p>



<h2 id="analise">Análise dos Resultados Alcançados</h2>

<p>
A implementação da API e sua integração com o coprocessador gráfico na FPGA apresentaram resultados satisfatórios, confirmando o correto funcionamento do barramento <em>Lightweight (LW Bridge)</em> e a comunicação estável entre o HPS e a lógica em Verilog. 
O protocolo de troca de instruções mostrou-se eficiente e confiável, permitindo que as operações fossem executadas de forma sincronizada e com baixa latência.
</p>

<p>
Durante os testes, todas as funções principais da API — incluindo as operações de zoom in, zoom out e reset — foram executadas com sucesso, validando a robustez do mapeamento de memória e a precisão da leitura dos registradores <strong>PIO_INSTRUCT</strong>, <strong>PIO_ENABLE</strong> e <strong>PIO_FLAGS</strong>. 
A leitura das flags demonstrou alta confiabilidade, garantindo o reconhecimento imediato de sinais de conclusão (<em>DONE</em>), erros (<em>ERROR</em>) e limites de zoom (<em>MAX</em> e <em>MIN</em>), sem necessidade de reinicialização do sistema.
</p>

<p>
<p>
O menu interativo em C foi desenvolvido nesta etapa como uma nova funcionalidade, proporcionando uma interface prática e intuitiva para o controle direto das operações do coprocessador. 
Essa camada de software consolidou a integração entre o código Assembly e o hardware, permitindo que o usuário execute e teste os algoritmos de zoom e reset de forma simples e organizada, 
além de facilitar o processo de validação e depuração do sistema.
</p>

</p>

<p>
<p>
Como perspectivas futuras, o sistema pode evoluir para uma interface mais dinâmica e interativa, aproximando-se de aplicações gráficas reais. 
Entre as melhorias planejadas, destaca-se a possibilidade de realizar o controle de zoom diretamente por meio das teclas “+” e “−” ou através do movimento de rolagem do mouse, 
permitindo um ajuste contínuo e intuitivo da ampliação da imagem. 
Além disso, pretende-se implementar a execução de zoom em áreas específicas, possibilitando que o usuário selecione regiões de interesse na imagem para ampliar ou reduzir com precisão. 
Também está prevista a adição de novos algoritmos de redimensionamento, ampliando o repertório de técnicas disponíveis no coprocessador e tornando a API mais versátil e próxima de um sistema de manipulação de imagens completo.
</p>

<p>
Esses resultados demonstram que a arquitetura proposta cumpre seus objetivos de integração HPS–FPGA, fornecendo uma base sólida para expansão e aprimoramento de futuras aplicações de processamento de imagens.
</p>


<h2 id="referencias">Referências</h2>

  * PATTERSON, D. A.; HENNESSY, J. L. Computer organization and design : the hardware/software interface, ARM edition / Computer organization and design : the hardware/software interface, ARM edition.<br>
  * Cyclone V Device Overview. Disponível em: <https://www.intel.com/content/www/us/en/docs/programmable/683694/current/cyclone-v-device-overview.html>.<br>
  * TECHNOLOGIES, T. Terasic - SoC Platform - Cyclone - DE1-SoC Board. Disponível em: <https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&No=836>.<br>
  * FPGAcademy. Disponível em: <https://fpgacademy.org>.<br>
