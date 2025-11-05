# Instruções de uso de controle de IoT do protocolo MCP

> Este documento descreve como implementar o controle IoT de dispositivos ESP32 baseados no protocolo MCP. Para processo de protocolo detalhado, consulte [`mcp-protocol.md`](./mcp-protocol.md).

## Introdução

MCP (Model Context Protocol) é um protocolo de nova geração recomendado para controle de IoT. Ele descobre e chama "Ferramentas" entre o plano de fundo e o dispositivo por meio do formato padrão JSON-RPC 2.0 para obter controle flexível do dispositivo.

## Processo de uso típico

1. Após o dispositivo ser iniciado, ele estabelece uma conexão com o plano de fundo por meio de protocolos básicos (como WebSocket/MQTT).
2. O background inicializa a sessão através do método `initialize` do protocolo MCP.
3. O background obtém todas as ferramentas (funções) e descrições de parâmetros suportadas pelo dispositivo através de `tools/list`.
4. O plano de fundo chama ferramentas específicas através de `tools/call` para controlar o dispositivo.

Para formato de protocolo detalhado e interação, consulte [`mcp-protocol.md`](./mcp-protocol.md).

## Descrição do método de registro da ferramenta no lado do dispositivo

O dispositivo registra “ferramentas” que podem ser chamadas em segundo plano através do método `McpServer::AddTool`. Suas assinaturas de função comumente usadas são as seguintes:

```cpp
void AddTool(
    const std::string& name, // Nome da ferramenta, recomenda-se que seja único e hierárquico, como self.dog.forward
    const std::string& description, // Descrição da ferramenta, descrição concisa de funções, modelos grandes fáceis de entender
    const PropertyList& propriedades, // Lista de parâmetros de entrada (pode estar vazia), tipos suportados: Boolean, inteiro, string
    std::function<ReturnValue(const PropertyList&)> callback //Implementação de retorno de chamada quando a ferramenta é chamada
);
```
- nome: O identificador exclusivo da ferramenta. Recomenda-se usar o estilo de nomenclatura "module.function".
- descrição: descrição em linguagem natural, fácil de entender para IA/usuários.
- propriedades: lista de parâmetros, os tipos suportados são booleanos, inteiro, string, intervalo e valor padrão podem ser especificados.
- callback: a lógica de execução real ao receber a solicitação de chamada, o valor de retorno pode ser bool/int/string.

## Exemplo típico de registro (tomando ESP-Hi como exemplo)

```cpp
void InitializeTools() {
    auto& mcp_server = McpServer::GetInstance();
    //Exemplo 1: Sem parâmetros, controle o robô para avançar
    mcp_server.AddTool("self.dog.forward", "Robô avança", PropertyList(), [this](const PropertyList&) -> ReturnValue {
        servo_dog_ctrl_send(DOG_STATE_FORWARD, NULL);
        return true;
    });
    //Exemplo 2: Com parâmetros, defina a cor RGB da luz
    mcp_server.AddTool("self.light.set_rgb", "Definir cor RGB", PropertyList({
        Property("r", kPropertyTypeInteger, 0, 255),
        Property("g", kPropertyTypeInteger, 0, 255),
        Property("b", kPropertyTypeInteger, 0, 255)
    }), [this](const PropertyList& properties) -> ReturnValue {
        int r = properties["r"].value<int>();
        int g = properties["g"].value<int>();
        int b = properties["b"].value<int>();
        led_on_ = true;
        SetLedColor(r, g, b);
        return true;
    });
}
```

## Exemplos comuns de chamada de ferramenta JSON-RPC

### 1. Obtenha a lista de ferramentas
```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "params": { "cursor": "" },
  "id": 1
}
```

### 2. Controle o chassi para avançar
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.chassis.go_forward",
    "arguments": {}
  },
  "id": 2
}
```

### 3. Alternar modo de iluminação
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.chassis.switch_light_mode",
    "arguments": { "light_mode": 3 }
  },
  "id": 3
}
```

### 4. Virar a câmera
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.camera.set_camera_flipped",
    "arguments": {}
  },
  "id": 4
}
```

## Observação
- Para o nome da ferramenta, parâmetros e valor de retorno, consulte o registro `AddTool` no lado do dispositivo.
- Recomenda-se que todos os novos projetos utilizem uniformemente o protocolo MCP para controle de IoT.
- Para protocolo detalhado e uso avançado, consulte [`mcp-protocol.md`](./mcp-protocol.md).