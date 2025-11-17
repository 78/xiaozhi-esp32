A seguir está um documento do protocolo de comunicação WebSocket baseado na implementação de código, descrevendo como o dispositivo e o servidor interagem por meio do WebSocket.

Este documento é inferido apenas com base no código fornecido. A implantação real pode exigir confirmação ou complementação adicional em conjunto com a implementação no lado do servidor.

---

## 1. Visão geral do processo

1. **Inicialização do lado do dispositivo**
   - Ligue o dispositivo e inicialize o `Application`:
     - Inicialize codecs de áudio, displays, LEDs, etc.
     - Conecte-se à Internet
     - Criar e inicializar uma instância do protocolo WebSocket (`WebsocketProtocol`) que implementa a interface `Protocol`
   - Entre no loop principal para aguardar eventos (entrada de áudio, saída de áudio, tarefas agendadas, etc.).

2. **Estabeleça conexão WebSocket**
   - Quando o dispositivo precisar iniciar uma sessão de voz (como ativação do usuário, acionamento manual de teclas, etc.), chame `OpenAudioChannel()`:
     - Obtenha URL do WebSocket com base na configuração
     - Definir vários cabeçalhos de solicitação (`Autorização`, `Protocol-Version`, `Device-Id`, `Client-Id`)
     - Chame `Connect()` para estabelecer uma conexão WebSocket com o servidor

3. **O dispositivo envia uma mensagem de "olá"**
   - Após a conexão ser bem-sucedida, o dispositivo enviará uma mensagem JSON. A estrutura da amostra é a seguinte:
   ```json
   {
     "type": "hello",
     "version": 1,
     "features": {
       "mcp": true
     },
     "transport": "websocket",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```
   - O campo `features` é opcional e o conteúdo é gerado automaticamente com base na configuração de compilação do dispositivo. Por exemplo: `"mcp": true` indica suporte ao protocolo MCP.
   - O valor de `frame_duration` corresponde a `OPUS_FRAME_DURATION_MS` (por exemplo, 60ms).

4. **O servidor responde "olá"**
   - O dispositivo espera que o servidor retorne uma mensagem JSON contendo `"type": "hello"` e verifica se `"transport": "websocket"` corresponde.
   - O servidor pode opcionalmente enviar o campo `session_id`, e o dispositivo irá gravá-lo automaticamente após recebê-lo.
   - Exemplo:
   ```json
   {
     "type": "hello",
     "transport": "websocket",
     "session_id": "xxx",
     "audio_params": {
       "format": "opus",
       "sample_rate": 24000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```
   - Se houver correspondência, o servidor é considerado pronto e o canal de áudio é marcado como aberto com sucesso.
   - Se uma resposta correta não for recebida dentro do tempo limite (padrão 10 segundos), a conexão será considerada falhada e um retorno de chamada de erro de rede será acionado.

5. **Interação com mensagem de acompanhamento**
   - Existem dois tipos principais de dados que podem ser enviados entre o dispositivo e o servidor:
     1. **Dados de áudio binários** (codificação Opus)
     2. **Mensagem de texto JSON** (usada para transmitir status de bate-papo, eventos TTS/STT, mensagens de protocolo MCP, etc.)

   - No código, o recebimento de retornos de chamada é dividido principalmente em:
     - `OnData(...)`:  
       - Quando `binary` é `true`, é considerado um quadro de áudio; o dispositivo irá decodificá-lo como dados Opus.
       - Quando `binary` é `false`, é considerado texto JSON, que precisa ser analisado com cJSON no lado do dispositivo e processado com a lógica de negócios correspondente (como chat, TTS, mensagens de protocolo MCP, etc.).

   - Quando o servidor ou rede é desconectado, o callback `OnDisconnected()` é acionado:
     - O dispositivo chamará `on_audio_channel_closed_()` e eventualmente retornará ao estado inativo.

6. **Fechar conexão WebSocket**
   - Quando o dispositivo precisar encerrar a sessão de voz, ele chamará `CloseAudioChannel()` para desconectar ativamente e retornar ao estado inativo.
   - Ou se o servidor se desconectar ativamente, o mesmo processo de retorno de chamada também será acionado.

---

## 2. Cabeçalho de solicitação comum

Ao estabelecer uma conexão WebSocket, os seguintes cabeçalhos de solicitação são definidos no exemplo de código:

- `"Autorização`: utilizado para armazenar tokens de acesso, na forma de `"Bearer <token>"`
- `Protocol-Version`: número da versão do protocolo, consistente com o campo `version` no corpo da mensagem hello
- `Device-Id`: endereço MAC da placa de rede física do dispositivo
- `Client-Id`: UUID gerado por software (redefinido apagando o NVS ou regravando o firmware completo)

Esses cabeçalhos serão enviados ao servidor junto com o handshake do WebSocket, e o servidor poderá realizar verificação, autenticação, etc., conforme necessário.

---

## 3. Versão do protocolo binário

O dispositivo suporta múltiplas versões de protocolo binário, especificadas através do campo `versão` na configuração:

### 3.1 versão 1 (padrão)
Envie dados de áudio Opus diretamente, sem metadados adicionais. O protocolo Websocket distingue entre texto e binário.

### 3.2 versão 2
Usando a estrutura `BinaryProtocol2`:
```c
struct BinaryProtocol2 {
    versão uint16_t; //versão do protocolo
    tipo uint16_t; // Tipo de mensagem (0: OPUS, 1: JSON)
    uint32_t reservado; //campo reservado
    carimbo de data/hora uint32_t; // Timestamp (milissegundos, usado para AEC do lado do servidor)
    uint32_t tamanho_da_carga; //Tamanho da carga útil (bytes)
    carga útil uint8_t[]; //dados de carga útil
} __attribute__((packed));
```

### 3.3 versão 3
Usando a estrutura `BinaryProtocol3`:
```c
struct BinaryProtocol3 {
    tipo uint8_t; //tipo de mensagem
    uint8_t reservado; //campo reservado
    uint16_t tamanho_da_carga; //tamanho da carga útil
    carga útil uint8_t[]; //dados de carga útil
} __attribute__((packed));
```

---

## 4. Estrutura da mensagem JSON

Os quadros de texto WebSocket são transmitidos no modo JSON. A seguir estão os campos `"type"` comuns e sua lógica de negócios correspondente. Se a mensagem contiver campos não listados, eles poderão ser opcionais ou específicos da implementação.

### 4.1 Dispositivo → Servidor

1. **Hello**  
   - Após o sucesso da conexão, será enviado pelo dispositivo para informar ao servidor os parâmetros básicos.
   - Exemplo:
     ```json
     {
       "type": "hello",
       "version": 1,
       "features": {
         "mcp": true
       },
       "transport": "websocket",
       "audio_params": {
         "format": "opus",
         "sample_rate": 16000,
         "channels": 1,
         "frame_duration": 60
       }
     }
     ```

2. **Listen**  
   - Indica que o dispositivo inicia ou para o monitoramento da gravação.
   - Campos comuns:
     - `"session_id"`: ID da sessão
     - `"type": "listen"`  
     - `"state"`: `"start"`, `"stop"`, `"detect"` (a detecção de ativação foi acionada)
     - `"mode"`: `"auto"`, `"manual"` ou `"realtime"`, indicando o modo de reconhecimento.
   - Exemplo: Iniciar monitoramento
     ```json
     {
       "session_id": "xxx",
       "type": "listen",
       "state": "start",
       "mode": "manual"
     }
     ```

3. **Abort**  
   - Encerre a conversa atual (reprodução TTS) ou canal de voz.
   - Exemplo:
     ```json
     {
       "session_id": "xxx",
       "type": "abort",
       "reason": "wake_word_detected"
     }
     ```
   - O valor `reason` pode ser `"wake_word_detected"` ou outro.

4. **Wake Word Detected**  
   - Usado pelo dispositivo para notificar o servidor de que a palavra de ativação foi detectada.
   - Antes de enviar a mensagem, os dados de áudio Opus da wake word podem ser enviados antecipadamente para o servidor realizar a detecção de impressão de voz.
   - Exemplo:
     ```json
     {
       "session_id": "xxx",
       "type": "listen",
       "state": "detect",
       "texto": "Olá Xiao Ming"
     }
     ```

5. **MCP**
   - Protocolo de nova geração recomendado para controle de IoT. Todas as descobertas de capacidade do dispositivo, invocação de ferramentas, etc. são realizadas por meio de mensagens do tipo: "mcp", e a carga útil é JSON-RPC 2.0 padrão internamente (consulte [Documento do protocolo MCP](./mcp-protocol.md) para obter detalhes).
   
   - **Exemplo de dispositivo enviando resultado para servidor:**
     ```json
     {
       "session_id": "xxx",
       "type": "mcp",
       "payload": {
         "jsonrpc": "2.0",
         "id": 1,
         "result": {
           "content": [
             { "type": "text", "text": "true" }
           ],
           "isError": false
         }
       }
     }
     ```

---

### 4.2 Servidor→Dispositivo

1. **Hello**  
   - A mensagem de confirmação do handshake retornada pelo servidor.
   - Deve conter `"type": "hello"` e `"transport": "websocket"`.
   - Pode vir acompanhado de `audio_params`, indicando os parâmetros de áudio esperados pelo servidor, ou configuração alinhada ao lado do dispositivo.
   - O servidor pode opcionalmente enviar o campo `session_id`, e o dispositivo irá gravá-lo automaticamente após recebê-lo.
   - Após a recepção bem-sucedida, o dispositivo definirá o sinalizador de evento para indicar que o canal WebSocket está pronto.

2. **STT**  
   - `{"session_id": "xxx", "type": "stt", "text": "..."}`
   - Indica que o servidor reconheceu a voz do usuário. (por exemplo, resultados de fala para texto)
   - O aparelho pode exibir este texto na tela e depois entrar no processo de atendimento.

3. **LLM**  
   - `{"session_id": "xxx", "type": "llm", "emotion": "happy", "text": "😀"}`
   - O servidor instrui o dispositivo a ajustar animações de emote/expressões de UI.

4. **TTS**  
   - `{"session_id": "xxx", "type": "tts", "state": "start"}`: O servidor está pronto para entregar áudio TTS e o dispositivo entra no estado de reprodução "falante".
   - `{"session_id": "xxx", "type": "tts", "state": "stop"}`: Indica o final deste TTS.
   - `{"session_id": "xxx", "type": "tts", "state": "sentence_start", "text": "..."}`
     - Faça com que o dispositivo exiba o segmento de texto atualmente reproduzido ou falado na interface (por exemplo, para exibição ao usuário).

5. **MCP**
   - O servidor emite instruções de controle relacionadas à IoT ou retorna os resultados da chamada através de mensagens do tipo: “mcp”. A estrutura da carga útil é a mesma acima.
   
   - **Exemplo de envio de ferramentas/chamada do servidor para o dispositivo:**
     ```json
     {
       "session_id": "xxx",
       "type": "mcp",
       "payload": {
         "jsonrpc": "2.0",
         "method": "tools/call",
         "params": {
           "name": "self.light.set_rgb",
           "arguments": { "r": 255, "g": 0, "b": 0 }
         },
         "id": 1
       }
     }
     ```

6. **System**
   - Comandos de controle do sistema, frequentemente usados ​​para atualizações e atualizações remotas.
   - Exemplo:
     ```json
     {
       "session_id": "xxx",
       "type": "system",
       "command": "reboot"
     }
     ```
   - Comandos suportados:
     - `"reboot"`: Reinicie o dispositivo

7. **Personalizado** (opcional)
   - Mensagens personalizadas, suportadas quando `CONFIG_RECEIVE_CUSTOM_MESSAGE` está habilitado.
   - Exemplo:
     ```json
     {
       "session_id": "xxx",
       "type": "custom",
       "payload": {
         "mensagem": "conteúdo personalizado"
       }
     }
     ```

8. **Dados de áudio: quadro binário**
   - Quando o servidor envia quadros binários de áudio (codificados em Opus), o dispositivo os decodifica e os reproduz.
   - Se o dispositivo estiver no estado de "escuta" (gravação), os quadros de áudio recebidos serão ignorados ou apagados para evitar conflitos.

---

## 5. Codec de áudio

1. **O dispositivo envia dados de gravação**
   - Após a entrada de áudio ter sofrido possível cancelamento de eco, redução de ruído ou ganho de volume, ela é empacotada em quadros binários e enviada ao servidor por meio da codificação Opus.
   - Dependendo da versão do protocolo, é possível enviar dados Opus diretamente (versão 1) ou utilizar o protocolo binário com metadados (versão 2/3).

2. **Reproduza o áudio recebido no dispositivo**
   - Quando um quadro binário é recebido do servidor, também é considerado dado Opus.
   - O dispositivo irá decodificá-lo e reproduzi-lo através da interface de saída de áudio.
   - Se a taxa de amostragem de áudio do servidor for inconsistente com o dispositivo, a reamostragem será realizada após a decodificação.

---

## 6. Transições de status comuns

A seguir estão fluxos comuns de status de chave do lado do dispositivo, correspondentes a mensagens WebSocket:

1. **Idle** → **Connecting**  
   - Depois que o usuário aciona ou acorda, o dispositivo chama `OpenAudioChannel()` → estabelece uma conexão WebSocket → envia `"type":"hello"`.

2. **Connecting** → **Listening**  
   - Após estabelecer a conexão com sucesso, se você continuar a executar `SendStartListening(...)`, ele entrará no estado de gravação. Neste momento, o dispositivo continuará a codificar os dados do microfone e enviá-los ao servidor.

3. **Listening** → **Speaking**  
   - Mensagem de início do TTS do servidor recebida (`{"type":"tts","state":"start"}`) → Pare a gravação e reproduza o áudio recebido.

4. **Speaking** → **Idle**  
   - Parada TTS do servidor (`{"type":"tts","state":"stop"}`) → A reprodução do áudio termina. Caso não continue entrando na escuta automática, retornará para Idle; se um loop automático estiver configurado, ele entrará em escuta novamente.

5. **Ouvir** / **Falar** → **Inativo** (encontrando exceção ou interrupção ativa)
   - Chame `SendAbortSpeaking(...)` ou `CloseAudioChannel()` → interrompa a sessão → feche o WebSocket → retorne ao estado Idle.

### Fluxograma de status do modo automático

```mermaid
stateDiagram
  direction TB
  [*] --> kDeviceStateUnknown
  kDeviceStateUnknown --> kDeviceStateStarting: inicialização
  kDeviceStateStarting -> kDeviceStateWifiConfigurando: Configurar WiFi
  kDeviceStateStarting --> kDeviceStateActivating: Ativando o dispositivo
  kDeviceStateActivating --> kDeviceStateUpgrading: Nova versão detectada
  kDeviceStateActivating --> kDeviceStateIdle: ativação concluída
  kDeviceStateIdle -> kDeviceStateConnecting: inicia a conexão
  kDeviceStateConnecting --> kDeviceStateIdle: Falha na conexão
  kDeviceStateConnecting -> kDeviceStateListening: Conexão bem-sucedida
  kDeviceStateListening -> kDeviceStateSpeaking: comece a falar
  kDeviceStateSpeaking -> kDeviceStateListening: Terminar a fala
  kDeviceStateListening -> kDeviceStateIdle: encerramento manual
  kDeviceStateSpeaking --> kDeviceStateIdle: Encerramento automático
```

### Fluxograma de status do modo manual

```mermaid
stateDiagram
  direction TB
  [*] --> kDeviceStateUnknown
  kDeviceStateUnknown --> kDeviceStateStarting: inicialização
  kDeviceStateStarting -> kDeviceStateWifiConfigurando: Configurar WiFi
  kDeviceStateStarting --> kDeviceStateActivating: Ativando o dispositivo
  kDeviceStateActivating --> kDeviceStateUpgrading: Nova versão detectada
  kDeviceStateActivating --> kDeviceStateIdle: ativação concluída
  kDeviceStateIdle -> kDeviceStateConnecting: inicia a conexão
  kDeviceStateConnecting --> kDeviceStateIdle: Falha na conexão
  kDeviceStateConnecting -> kDeviceStateListening: Conexão bem-sucedida
  kDeviceStateIdle -> kDeviceStateListening: comece a ouvir
  kDeviceStateListening -> kDeviceStateIdle: parar de ouvir
  kDeviceStateIdle -> kDeviceStateSpeaking: comece a falar
  kDeviceStateSpeaking -> kDeviceStateIdle: termina a fala
```

---

## 7. Tratamento de erros

1. **Falha na conexão**
   - Acione o retorno de chamada `on_network_error_()` se `Connect(url)` retornar falha ou expirar enquanto espera pela mensagem "hello" do servidor. O dispositivo exibirá a mensagem “Não é possível conectar-se ao serviço” ou uma mensagem de erro semelhante.

2. **Servidor desconectado**
   - Se o WebSocket for desconectado de forma anormal, retorno de chamada `OnDisconnected()`:
     - Retorno de chamada do dispositivo `on_audio_channel_closed_()`
     - Mude para Idle ou outra lógica de nova tentativa.

---

## 8. Outros assuntos que precisam de atenção

1. **Autenticação**
   - O dispositivo fornece autenticação configurando `Autorização: Bearer <token>`, e o servidor precisa verificar se é válido.
   - Se o token expirar ou for inválido, o servidor poderá rejeitar o handshake ou desconectar-se posteriormente.

2. **Controle de Sessão**
   - Algumas mensagens no código contêm `session_id` para distinguir conversas ou operações independentes. O servidor pode separar sessões diferentes conforme necessário.

3. **Carga de áudio**
   - O código usa o formato Opus por padrão e define `sample_rate = 16000`, mono. A duração do quadro é controlada por `OPUS_FRAME_DURATION_MS`, geralmente 60ms. Os ajustes podem ser feitos adequadamente com base na largura de banda ou no desempenho. Para uma melhor reprodução de música, o áudio downstream do servidor pode usar uma taxa de amostragem de 24.000.

4. **Configuração da versão do protocolo**
   - Configure a versão do protocolo binário (1, 2 ou 3) através do campo `versão` nas configurações
   - Versão 1: Envie dados Opus diretamente
   - Versão 2: usa protocolo binário com carimbo de data e hora, adequado para AEC do lado do servidor
   - Versão 3: usa protocolo binário simplificado

5. **Protocolo MCP recomendado para controle de IoT**
   - Recomenda-se que a descoberta de capacidade IoT, sincronização de status, instruções de controle, etc. entre dispositivos e servidores sejam implementadas através do protocolo MCP (tipo: "mcp"). A solução type: "iot" original foi descontinuada.
   - O protocolo MCP pode ser transmitido em vários protocolos subjacentes, como WebSocket e MQTT, e possui melhores capacidades de escalabilidade e padronização.
   - Para uso detalhado, consulte [Documento do protocolo MCP](./mcp-protocol.md) e [Uso de controle de IoT do MCP](./mcp-usage.md).

6. **Erro ou exceção JSON**
   - Quando faltarem campos necessários no JSON, como `{"type": ...}`, o dispositivo registrará um log de erros (`ESP_LOGE(TAG, "Missing message type, data: %s", data);`) e não realizará nenhum negócio.

---

## 9. Exemplo de mensagem

Um exemplo típico de mensagem bidirecional é fornecido abaixo (processo simplificado):

1. **Dispositivo → Servidor** (aperto de mão)
   ```json
   {
     "type": "hello",
     "version": 1,
     "features": {
       "mcp": true
     },
     "transport": "websocket",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```

2. **Servidor → Dispositivo** (resposta de handshake)
   ```json
   {
     "type": "hello",
     "transport": "websocket",
     "session_id": "xxx",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000
     }
   }
   ```

3. **Dispositivo → Servidor** (iniciar monitoramento)
   ```json
   {
     "session_id": "xxx",
     "type": "listen",
     "state": "start",
     "mode": "auto"
   }
   ```
   Ao mesmo tempo, o dispositivo começa a enviar quadros binários (dados Opus).

4. **Servidor → Dispositivo** (resultado ASR)
   ```json
   {
     "session_id": "xxx",
     "type": "stt",
     "text": "O que o usuário disse"
   }
   ```

5. **Servidor → Dispositivo** (TTS inicia)
   ```json
   {
     "session_id": "xxx",
     "type": "tts",
     "state": "start"
   }
   ```
   Em seguida, o servidor envia quadros de áudio binários ao dispositivo para reprodução.

6. **Servidor → Dispositivo** (TTS termina)
   ```json
   {
     "session_id": "xxx",
     "type": "tts",
     "state": "stop"
   }
   ```
   O dispositivo para de reproduzir áudio e retorna ao estado inativo se não houver mais instruções.

---

## 10. Resumo

Este protocolo transmite texto JSON e quadros de áudio binários na camada superior do WebSocket e completa funções, incluindo upload de fluxo de áudio, reprodução de áudio TTS, reconhecimento de fala e gerenciamento de status, emissão de comando MCP, etc.

- **Fase de handshake**: Envie `"type":"hello"` e aguarde o retorno do servidor.
- **Canal de Áudio**: usa quadros binários codificados pelo Opus para transmitir fluxos de voz em ambas as direções, suportando múltiplas versões de protocolo.
- **Mensagem JSON**: Use `"type"` para identificar diferentes lógicas de negócios para campos principais, incluindo TTS, STT, MCP, WakeWord, System, Custom, etc.
- **Extensibilidade**: Os campos podem ser adicionados à mensagem JSON de acordo com as necessidades reais, ou autenticação adicional pode ser realizada nos cabeçalhos.

O servidor e o dispositivo precisam concordar antecipadamente sobre os significados dos campos, a lógica de temporização e as regras de tratamento de erros de várias mensagens para garantir uma comunicação tranquila. As informações acima podem ser utilizadas como documentos básicos para facilitar posterior ancoragem, desenvolvimento ou expansão.
