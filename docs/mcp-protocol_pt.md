Processo de interação #MCP (Model Context Protocol)

AVISO: Geração assistida por IA, ao implementar serviços em segundo plano, consulte o código para confirmar os detalhes!!

O protocolo MCP neste projeto é usado para comunicação entre a API backend (cliente MCP) e o dispositivo ESP32 (servidor MCP) para que o backend possa descobrir e chamar as funções (ferramentas) fornecidas pelo dispositivo.

## Formato do protocolo

De acordo com o código (`main/protocols/protocol.cc`, `main/mcp_server.cc`), as mensagens MCP são encapsuladas no corpo da mensagem do protocolo de comunicação subjacente (como WebSocket ou MQTT). Sua estrutura interna segue a especificação [JSON-RPC 2.0](https://www.jsonrpc.org/specification).

Exemplo de estrutura geral da mensagem:

```json
{
  "session_id": "...", //ID da sessão
  "type": "mcp", // Tipo de mensagem, fixado em "mcp"
  "carga útil": { // carga útil JSON-RPC 2.0
    "jsonrpc": "2.0",
    "method": "...", // nome do método (como "initialize", "tools/list", "tools/call")
    "params": { ... }, // parâmetros do método (para solicitação)
    "id": ..., // ID da solicitação (para solicitação e resposta)
    "resultado": { ... }, // Resultado da execução do método (para resposta de sucesso)
    "error": { ... } // Mensagem de erro (para resposta de erro)
  }
}
```

Entre eles, a parte `payload` é uma mensagem JSON-RPC 2.0 padrão:

- `jsonrpc`: string fixa "2.0".
- `method`: O nome do método a ser chamado (para Request).
- `params`: Os parâmetros do método, um valor estruturado, geralmente um objeto (para Request).
- `id`: O identificador da solicitação, fornecido pelo cliente ao enviar a solicitação, e retornado como está quando o servidor responde. Usado para combinar solicitações e respostas.
- `resultado`: O resultado quando o método é executado com sucesso (para Resposta de Sucesso).
- `error`: Mensagem de erro quando a execução do método falha (para Error Response).

## Processo de interação e tempo de envio

A interação do MCP gira principalmente em torno do cliente (API backend) descobrindo e chamando as “ferramentas” no dispositivo.

1. **Estabelecimento de conexão e notificação de capacidade**

    - **Tempo:** depois que o dispositivo for inicializado e conectado com êxito à API de back-end.
    - **Remetente:** Dispositivo.
    - **Mensagem:** O dispositivo envia uma mensagem de protocolo básico "hello" para a API de back-end, que contém uma lista de recursos suportados pelo dispositivo, por exemplo, suportando o protocolo MCP (`"mcp": true`).
    - **Exemplo (não carga útil do MCP, mas mensagem de protocolo subjacente):**
      ```json
      {
        "type": "hello",
        "version": ...,
        "features": {
          "mcp": true,
          ...
        },
        "transport": "websocket", // ou "mqtt"
        "audio_params": { ... },
        "session_id": "..." // O dispositivo pode ser configurado após receber olá do servidor
      }
      ```

2. **Inicializar sessão MCP**

    - **Tempo:** Depois que a API em segundo plano recebe a mensagem "hello" do dispositivo e confirma que o dispositivo suporta MCP, ela geralmente é enviada como a primeira solicitação da sessão MCP.
    - **Remetente:** API de back-end (cliente).
    - **Método:** `inicializar`
    - **Mensagem (carga útil do MCP):**

      ```json
      {
        "jsonrpc": "2.0",
        "method": "initialize",
        "params": {
          "capabilities": {
            // Capacidades do cliente, opcional

            //Relacionado à visão da câmera
            "vision": {
              "url": "...", //Câmera: endereço de processamento de imagem (deve ser um endereço http, não um endereço websocket)
              "token": "..." // url token
            }

            // ...outras capacidades do cliente
          }
        },
        "id": 1 // ID da solicitação
      }
      ```

    - **Tempo de resposta do dispositivo:** Depois que o dispositivo recebe e processa a solicitação `initialize`.
    - **Mensagem de resposta do dispositivo (carga útil do MCP):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 1, // ID da solicitação de correspondência
        "result": {
          "protocolVersion": "2024-11-05",
          "capabilities": {
            "tools": {} // As ferramentas aqui não parecem listar informações detalhadas e ferramentas/lista são necessárias
          },
          "serverInfo": {
            "nome": "...", // nome do dispositivo (BOARD_NAME)
            "version": "..." // Versão do firmware do dispositivo
          }
        }
      }
      ```

3. **Descubra a lista de ferramentas do dispositivo**

    - **Tempo:** Quando a API de segundo plano precisa obter uma lista de funções específicas (ferramentas) atualmente suportadas pelo dispositivo e seus métodos de chamada.
    - **Remetente:** API de back-end (cliente).
    - **Método:** `ferramentas/lista`
    - **Mensagem (carga útil do MCP):**
      ```json
      {
        "jsonrpc": "2.0",
        "method": "tools/list",
        "params": {
          "cursor": "" // Usado para paginação, a primeira solicitação é uma string vazia
        },
        "id": 2 // ID da solicitação
      }
      ```
    - **Tempo de resposta do dispositivo:** Depois que o dispositivo recebe a solicitação `tools/list` e gera a lista de ferramentas.
    - **Mensagem de resposta do dispositivo (carga útil do MCP):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 2, // ID da solicitação de correspondência
        "result": {
          "tools": [ // Lista de objetos de ferramenta
            {
              "name": "self.get_device_status",
              "description": "...",
              "inputSchema": { ... } // esquema de parâmetro
            },
            {
              "name": "self.audio_speaker.set_volume",
              "description": "...",
              "inputSchema": { ... } // esquema de parâmetro
            }
            // ... mais ferramentas
          ],
          "nextCursor": "..." // Se a lista for grande e precisar de paginação, o valor do cursor da próxima solicitação será incluído aqui.
        }
      }
      ```
    - **Processamento de paginação:** Se o campo `nextCursor` não estiver vazio, o cliente precisa enviar a solicitação `tools/list` novamente e trazer esse valor de `cursor` em `params` para obter a próxima página de ferramentas.

4. **Chamando ferramentas do dispositivo**

    - **Tempo:** quando a API em segundo plano precisa executar uma função específica no dispositivo.
    - **Remetente:** API de back-end (cliente).
    - **Método:** `ferramentas/chamada`
    - **Mensagem (carga útil do MCP):**
      ```json
      {
        "jsonrpc": "2.0",
        "method": "tools/call",
        "params": {
          "name": "self.audio_speaker.set_volume", // O nome da ferramenta a ser chamada
          "arguments": {
            //Parâmetros da ferramenta, formato do objeto
            "volume": 50 // Nome do parâmetro e seu valor
          }
        },
        "id": 3 // ID da solicitação
      }
      ```
    - **Tempo de resposta do dispositivo:** Depois que o dispositivo recebe a solicitação `ferramentas/chamada` e executa a função da ferramenta correspondente.
    - **Mensagem de resposta bem-sucedida do dispositivo (carga útil do MCP):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 3, // ID da solicitação de correspondência
        "result": {
          "content": [
            // Conteúdo do resultado da execução da ferramenta
            { "type": "text", "text": "true" } // Exemplo: set_volume retorna bool
          ],
          "isError": false // indica sucesso
        }
      }
      ```
    - **Mensagem de resposta de falha do dispositivo (carga útil do MCP):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 3, // ID da solicitação de correspondência
        "error": {
          "code": -32601, // Código de erro JSON-RPC, como Método não encontrado (-32601)
          "message": "Ferramenta desconhecida: self.non_existent_tool" // Descrição do erro
        }
      }
      ```

5. **O dispositivo envia mensagens ativamente (notificações)**
    - **Tempo:** Quando ocorre um evento no dispositivo que precisa ser notificado à API de back-end (por exemplo, uma mudança de estado, embora não haja nenhuma ferramenta explícita no exemplo de código para enviar tal mensagem, a existência de `Application::SendMcpMessage` indica que o dispositivo pode enviar mensagens MCP ativamente).
    - **Remetente:** Dispositivo (Servidor).
    - **Método:** Pode ser um nome de método começando com `notifications/` ou outros métodos personalizados.
    - **Mensagem (carga útil MCP):** Segue o formato de notificação JSON-RPC e não possui campo `id`.
      ```json
      {
        "jsonrpc": "2.0",
        "method": "notifications/state_changed", // Exemplo de nome do método
        "params": {
          "newState": "idle",
          "oldState": "connecting"
        }
        // Nenhum campo de id
      }
      ```
    - **Processamento da API em segundo plano:** Após receber a Notificação, a API em segundo plano executa o processamento correspondente, mas não responde.

## Diagrama de interação

A seguir está um diagrama de sequência de interação simplificado mostrando o fluxo de mensagens principal do MCP:

```mermaid
sequenceDiagram
    participant Device as ESP32 Device
    participante BackendAPI como API de segundo plano (cliente)

    Nota sobre dispositivo, BackendAPI: Estabelecer conexão WebSocket/MQTT

    Dispositivo->>BackendAPI: Olá Mensagem (contém "mcp": true)

    BackendAPI->>Device: MCP Initialize Request
    Note over BackendAPI: method: initialize
    Note over BackendAPI: params: { capabilities: ... }

    Device->>BackendAPI: MCP Initialize Response
    Note over Device: result: { protocolVersion: ..., serverInfo: ... }

    BackendAPI->>Device: MCP Get Tools List Request
    Note over BackendAPI: method: tools/list
    Note over BackendAPI: params: { cursor: "" }

    Device->>BackendAPI: MCP Get Tools List Response
    Note over Device: result: { tools: [...], nextCursor: ... }

    loop Optional Pagination
        BackendAPI->>Device: MCP Get Tools List Request
        Note over BackendAPI: method: tools/list
        Note over BackendAPI: params: { cursor: "..." }
        Device->>BackendAPI: MCP Get Tools List Response
        Note over Device: result: { tools: [...], nextCursor: "" }
    end

    BackendAPI->>Device: MCP Call Tool Request
    Note over BackendAPI: method: tools/call
    Note over BackendAPI: params: { name: "...", arguments: { ... } }

    alt Tool Call Successful
        Device->>BackendAPI: MCP Tool Call Success Response
        Note over Device: result: { content: [...], isError: false }
    else Tool Call Failed
        Device->>BackendAPI: MCP Tool Call Error Response
        Note over Device: error: { code: ..., message: ... }
    end

    opt Device Notification
        Device->>BackendAPI: MCP Notification
        Note over Device: method: notifications/...
        Note over Device: params: { ... }
    end
```

Este documento descreve os principais fluxos de interação do protocolo MCP neste projeto. Para detalhes específicos de parâmetros e funções da ferramenta, consulte `McpServer::AddCommonTools` em `main/mcp_server.cc` e a implementação de cada ferramenta.
