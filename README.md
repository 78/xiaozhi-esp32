# xiaozhi-esp32 — fork com API HTTP local

Fork do projeto **[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)**, um chatbot de voz para ESP32 baseado em MCP (Model Context Protocol). Toda a funcionalidade original continua igual. Consulte o [README original](https://github.com/78/xiaozhi-esp32#readme) para hardware suportado, servidores, documentação de protocolos e licença (MIT).

O que este fork acrescenta: **uma API HTTP que roda dentro do próprio firmware**. Qualquer dispositivo da rede local (scripts, Home Assistant, Node-RED, uma página web) pode consultar o aparelho e mandar comandos diretamente, sem passar pelo servidor do Xiaozhi.

> **Status:** o código foi implementado, mas **ainda não foi compilado nem testado em hardware**. Veja [Status e pendências](#status-e-pendências).

---

## Como funciona

O firmware original já tem um servidor MCP interno (`main/mcp_server.cc`). As ferramentas que ele expõe (volume, brilho, tema, câmera e as específicas de cada placa) normalmente só são chamadas pelo servidor remoto, via WebSocket ou MQTT. Como `McpServer::ParseMessage()` aceita uma função de resposta, a resposta pode ser enviada por qualquer canal.

A API local usa o servidor HTTP do ESP-IDF (`esp_http_server`) e:

1. recebe a requisição na task do httpd;
2. agenda o trabalho na **thread principal** via `Application::Schedule()`, para não mexer em áudio ou display a partir de outra task;
3. espera a resposta com um semáforo, até o timeout configurado, e devolve JSON.

Arquivos envolvidos:

| Arquivo | Mudança |
|---|---|
| `main/local_api_server.h` / `.cc` | Servidor HTTP e os endpoints (novos) |
| `main/Kconfig.projbuild` | Menu **Local HTTP API** |
| `main/CMakeLists.txt` | Compila o servidor só se ativado; adiciona `esp_http_server` |
| `main/application.cc` | Inicia o servidor em `HandleNetworkConnectedEvent()` |
| `build-localapi.defaults` | Exemplo de configuração para build com a API ligada |

## Endpoints

Todas as respostas são JSON.

| Método e rota | Descrição |
|---|---|
| `GET /api/info` | Versão, placa, UUID, estado, IP, uptime e memória livre (interna e PSRAM) |
| `GET /api/status` | Status do aparelho (`Board::GetDeviceStatusJson()`): volume, tela, bateria, rede |
| `GET /api/tools` | Todas as ferramentas MCP, inclusive as *user-only* (paginação resolvida internamente) |
| `POST /api/tools/<nome>` | Executa uma ferramenta MCP; o corpo é o objeto de argumentos (pode ser vazio) |
| `POST /api/mcp` | Repassa uma requisição MCP JSON-RPC 2.0 e devolve a resposta sem alteração |
| `POST /api/chat/start` | Inicia uma conversa, como a palavra de ativação; corpo opcional `{"text":"..."}` |
| `POST /api/chat/toggle` | Mesmo efeito de apertar o botão de conversa |
| `POST /api/chat/abort` | Interrompe o assistente enquanto ele fala |

Códigos de resposta: `200`/`202` sucesso · `400` requisição ou argumento inválido · `401` token ausente ou errado · `404` rota inexistente · `413` corpo grande demais · `503` a thread principal não respondeu dentro do timeout.

### Exemplos

```bash
IP=192.168.0.50
TOKEN=meu-token

# Informações gerais
curl -H "Authorization: Bearer $TOKEN" http://$IP:8080/api/info

# Listar ferramentas disponíveis na placa
curl -H "Authorization: Bearer $TOKEN" http://$IP:8080/api/tools

# Ajustar o volume
curl -X POST -H "Authorization: Bearer $TOKEN" \
     http://$IP:8080/api/tools/self.audio_speaker.set_volume -d '{"volume":60}'

# Brilho da tela (placas com display)
curl -X POST -H "Authorization: Bearer $TOKEN" \
     http://$IP:8080/api/tools/self.screen.set_brightness -d '{"brightness":80}'

# MCP JSON-RPC direto
curl -X POST -H "Authorization: Bearer $TOKEN" http://$IP:8080/api/mcp \
     -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"self.get_device_status","arguments":{}}}'

# Iniciar e interromper uma conversa
curl -X POST -H "Authorization: Bearer $TOKEN" http://$IP:8080/api/chat/start
curl -X POST -H "Authorization: Bearer $TOKEN" http://$IP:8080/api/chat/abort
```

Os nomes e argumentos exatos de cada ferramenta variam por placa. Use `GET /api/tools` para ver o que a sua expõe.

## Configuração

Em `idf.py menuconfig` → **Xiaozhi Assistant → Local HTTP API**:

| Opção | Padrão | Descrição |
|---|---|---|
| `CONFIG_ENABLE_LOCAL_API` | `n` | Liga a API |
| `CONFIG_LOCAL_API_PORT` | `8080` | Porta HTTP |
| `CONFIG_LOCAL_API_CTRL_PORT` | `32780` | Porta UDP interna do httpd; precisa ser única entre servidores httpd |
| `CONFIG_LOCAL_API_TOKEN` | vazio | Token de acesso: `Authorization: Bearer <token>` ou `X-Api-Token: <token>` |
| `CONFIG_LOCAL_API_TIMEOUT_MS` | `10000` | Tempo máximo de espera pela thread principal |
| `CONFIG_LOCAL_API_MAX_BODY` | `4096` | Tamanho máximo do corpo da requisição |
| `CONFIG_LOCAL_API_ENABLE_CORS` | `y` | Envia cabeçalhos CORS, permitindo chamadas de páginas web |

## Build (ESP32-S3)

O projeto exige **ESP-IDF 6.x** (o CI usa `release-v6.1`). Pelo Docker, sem instalar o IDF:

```bash
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v $PWD:/project -w /project espressif/idf:release-v6.1 \
  bash -c 'idf.py -B build_localapi -D SDKCONFIG=build_localapi/sdkconfig \
           -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;build-localapi.defaults" \
           set-target esp32s3 build'
```

O arquivo `build-localapi.defaults` seleciona a placa `bread-compact-wifi`, liga a API e define o token `test`. **Troque a placa para a sua** (os símbolos `CONFIG_BOARD_TYPE_*` estão em `main/Kconfig.projbuild`) e defina um token próprio.

A imagem Docker ocupa cerca de 5 GB e o build mais uns 2 GB: tenha pelo menos 8 GB livres.

Para gravar e ver o log:

```bash
idf.py -B build_localapi -p /dev/ttyUSB0 flash monitor
```

Depois que o Wi-Fi conectar, o log mostra `Local API listening on http://<ip>:8080/api`.

## Cuidados

- **Segurança:** é HTTP sem criptografia. Sem token, qualquer pessoa na rede controla o aparelho, e o firmware emite um aviso no log. Use sempre um token e não exponha a porta para a internet.
- **Memória:** o servidor usa 3 conexões simultâneas e pilha de 6 KB na RAM interna. No S3 com PSRAM isso costuma sobrar, mas vale acompanhar `free_internal` e `min_free_internal` em `/api/info`.
- **Conflito de porta:** as placas `otto-robot` e `electron-bot` já rodam um servidor WebSocket na porta 8080. Nelas, mude `CONFIG_LOCAL_API_PORT`.
- **Rede:** só funciona com Wi-Fi ou Ethernet. Placas 4G (ML307/NT26) ficam atrás do NAT da operadora.
- **Respostas assíncronas:** `tools/call` roda na thread principal. Se ela estiver ocupada por mais tempo que o timeout (por exemplo, abrindo o canal de áudio ou enviando foto da câmera), a API responde `503`, e a ferramenta ainda pode terminar de executar depois.
- `/api/chat/start` sem corpo envia o texto padrão `你好小智` (a palavra de ativação original). Passe `{"text":"..."}` para usar outro.

## Status e pendências

- [x] Implementação dos endpoints, autenticação, CORS e tratamento de erros
- [x] Integração com Kconfig, CMake e o ciclo de vida da aplicação
- [ ] Compilar com ESP-IDF 6.1
- [ ] Testar em hardware ESP32-S3: servidor sobe, uso de memória, chamadas de ferramentas, início de conversa
- [ ] (Opcional) mDNS (`xiaozhi.local`) para não depender do IP

## Créditos e licença

Projeto original: **[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)**, © Shenzhen Xinzhi Future Technology Co., Ltd. e colaboradores, sob licença MIT. Este fork mantém a mesma licença (veja [LICENSE](LICENSE)).
