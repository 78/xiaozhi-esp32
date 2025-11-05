#Um chatbot baseado em MCP
(Chinês | Inglês | Japonês )
##introduzir
👉Humanos : Instalando câmeras em IA vs. IA: Descobrindo na hora que seu dono não lavou o cabelo por três dias [bilibili]
👉Crie sua própria namorada de IA: um guia para iniciantes [bilibili]
O chatbot Xiaozhi AI funciona como um portal de interação por voz, aproveitando os recursos de IA de grandes modelos como Qwen e DeepSeek para alcançar o controle de múltiplos terminais por meio do protocolo MCP.

##Notas da versão
A versão atual v2 é incompatível com a tabela de partições da versão v1; portanto, não é possível atualizar da v1 para a v2 via OTA. Consulte partitions/v2/README.md para obter detalhes sobre a tabela de partições .
Todos os dispositivos que utilizam a versão v1 podem ser atualizados para a versão v2 através da atualização manual do firmware.
A versão estável da v1 é a 1.9.2. Você pode git checkout v1alternar para a versão v1 usando o comando. Esta ramificação será mantida até fevereiro de 2026.
##Funcionalidades implementadas
    • Wi-Fi / ML307 Cat.1 4G
    • Despertar por voz offline ESP-SR
    • Suporta dois protocolos de comunicação ( Websocket ou MQTT+UDP)
    • Utilizando o codec de áudio OPUS
    • Interação por voz baseada em arquitetura de streaming ASR + LLM + TTS
    • Reconhecimento de impressão vocal, identificando a identidade do falante atual ( Alto-falante 3D)
    • Tela OLED/LCD, compatível com exibição de expressões faciais.
    • Indicador de nível de bateria e gerenciamento de energia
    • Suporta vários idiomas (chinês, inglês, japonês)
    • Compatível com as plataformas de chip ESP32-C3, ESP32-S3 e ESP32-P4.
    • O controle do dispositivo (volume, luzes, motores, GPIO, etc.) é realizado através do MCP (Multi-Controller Protocol) do lado do dispositivo.
    • Amplie as capacidades de modelos de grande porte através de MCP baseado em nuvem (controle de casa inteligente, operação de desktop de PC, pesquisa de conhecimento, envio e recebimento de e-mails, etc.).
    • Personalize palavras de ativação, fontes, emoticons e planos de fundo do chat; a edição online é suportada na web ( gerador de recursos personalizados ).
hardware
Prática de construção com placa de ensaio
Consulte o tutorial de documentação do Lark para obter detalhes:
👉 "Enciclopédia Xiaozhi AI Chatbot"
O projeto da placa de ensaio é mostrado abaixo:

Compatível com mais de 70 dispositivos de hardware de código aberto (apenas uma parte deles é mostrada).
    • Placa de Desenvolvimento LCSC ESP32-S3 - Aplicação Prática
    • Espressif ESP32-S3-BOX3
    • M5Stack CoreS3
    • M5Stack AtomS3R + Echo Base
    • Botão Mágico 2.4
    • Micro-ondas ESP32-S3-Touch-AMOLED-1.8
    • LILYGO T-Circle-S3
    • Camarão Bro Mini C3
    • Pingente de IA brilhante
    • Tecnologia Nologo - Star Intelligence - TFT de 1,54"
    • Observador SenseCAP
    • ESP-HI Cão Robô de Custo Ultrabaixo
           
software
Gravação de firmware
Para iniciantes, recomenda-se não configurar um ambiente de desenvolvimento na primeira vez, mas usar diretamente o firmware que não requer um ambiente de desenvolvimento para ser gravado.
O firmware está conectado ao servidor oficial xiaozhi.me por padrão , e os usuários individuais podem registrar uma conta para usar o modelo em tempo real Qwen gratuitamente.
👉Guia para Iniciantes em Atualização de Firmware
Ambiente de desenvolvimento
    • Cursor ou VSCode
    • Instale o plugin ESP-IDF, selecionando a versão 5.4 ou superior do SDK.
    • O Linux é melhor que o Windows; ele compila mais rápido e evita problemas com drivers.
    • Este projeto utiliza o estilo de codificação C++ do Google; certifique-se de que seu código esteja em conformidade com as diretrizes de estilo ao enviá-lo.
Documentação do desenvolvedor
    • Guia de Placa de Desenvolvimento Personalizada - Aprenda como criar uma placa de desenvolvimento personalizada para Xiaozhi AI
    • Instruções de uso do protocolo MCP para controle de IoT - Aprenda a controlar dispositivos IoT usando o protocolo MCP.
    • Fluxo de interação do protocolo MCP - Implementação do protocolo MCP no dispositivo
    • Documento do Protocolo de Comunicação Híbrida MQTT + UDP
    • Um documento detalhado sobre o protocolo de comunicação WebSocket.
Configuração de modelo grande
Se você já possui um dispositivo chatbot Xiaozhi AI e o conectou ao servidor oficial, pode acessar o console xiaozhi.me para configurá-lo.
👉Tutorial em vídeo sobre operação do backend (interface antiga)
Projetos de código aberto relacionados
Para implantar um servidor em um computador pessoal, você pode consultar os seguintes projetos de código aberto de terceiros:
    • Servidor Python xinnan-tech/xiaozhi-esp32-server
    • joey-zhou/xiaozhi-esp32-server-java Servidor Java
    • Servidor Golang AnimeAIChat/xiaozhi-server-go
Projetos de clientes terceirizados que utilizam o protocolo de comunicação Xiaozhi:
    • cliente Python huangjunsen0406/py-xiaozhi
    • TOM88812/xiaozhi-android-client Cliente Android
    • Cliente Linux fornecido por 100askTeam/xiaozhi-linux.
    • Firmware do chip Bluetooth 78/xiaozhi-sf32 da Siche Technology
    • QuecPython/solution-xiaozhiAI Quectel fornece firmware QuecPython
Sobre o projeto
Este é um projeto ESP32 de código aberto desenvolvido por Xia Ge, lançado sob a licença MIT, permitindo que qualquer pessoa o utilize, modifique ou use para fins comerciais gratuitamente.
Esperamos que, por meio deste projeto, possamos ajudar a todos a compreender o desenvolvimento de hardware de IA e a aplicar os modelos de linguagem de grande escala, que estão em rápida evolução, a dispositivos de hardware reais.
Se tiver alguma ideia ou sugestão, não hesite em levantar questões ou juntar-se ao grupo do QQ: 1011329060
História das Estrelas
 
