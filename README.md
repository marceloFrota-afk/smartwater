# SmartWater

Firmware para monitoramento de nível de caixa d'água usando um ESP32-C6, sensor de distância VL53L0X e publicação MQTT em um broker HiveMQ Cloud com TLS.

O projeto mede a distância entre o sensor e a superfície da água, converte essa leitura para percentual de volume estimado e publica os dados em JSON no tópico MQTT `caixaagua/distancia`.

## O que este firmware faz

- Conecta o ESP32-C6 a uma rede Wi-Fi.
- Inicializa o barramento I2C nos pinos definidos para o sensor VL53L0X.
- Lê continuamente a distância medida pelo sensor em milímetros.
- Aplica uma correção fixa de offset na distância lida.
- Converte a distância corrigida em porcentagem de nível da caixa.
- Limita o resultado final entre `0%` e `100%`.
- Publica a leitura no HiveMQ Cloud usando MQTT na porta TLS `8883`.
- Envia mensagens a cada `2` segundos.

## Arquitetura

```text
ESP32-C6
  |
  | I2C
  v
VL53L0X
  |
  | leitura em mm
  v
cálculo de nível
  |
  | JSON
  v
MQTT / HiveMQ Cloud
```

## Hardware esperado

- Placa ESP32-C6.
- Sensor VL53L0X.
- Rede Wi-Fi disponível.
- Broker MQTT compatível com usuário/senha e TLS.

## Ligação do sensor

O firmware inicializa o I2C explicitamente nestes pinos:

| Sinal | Pino no ESP32-C6 |
| --- | --- |
| SDA | `6` |
| SCL | `7` |

Esses pinos foram definidos no código com:

```cpp
#define SDA_PIN 6
#define SCL_PIN 7
```

Essa decisão torna a pinagem explícita e evita depender do mapeamento padrão da placa usada pela IDE.

## Dependências

O código usa as seguintes bibliotecas:

- `WiFi.h`: conexão Wi-Fi do ESP32.
- `WiFiClientSecure.h`: cliente TCP com suporte a TLS.
- `PubSubClient.h`: cliente MQTT.
- `Wire.h`: comunicação I2C.
- `VL53L0X.h`: driver do sensor VL53L0X.

Em Arduino IDE, instale pelo Library Manager:

- `PubSubClient`
- `VL53L0X`

As bibliotecas `WiFi`, `WiFiClientSecure` e `Wire` fazem parte do suporte ESP32/Arduino.

## Configuração de segredos

As credenciais ficam em `secrets.h`, que está listado no `.gitignore` para evitar commit acidental de dados sensíveis.

Crie um arquivo `secrets.h` na raiz do projeto com este formato:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "nome-da-rede"
#define WIFI_PASSWORD "senha-da-rede"

#define MQTT_USER "usuario-mqtt"
#define MQTT_PASSWORD "senha-mqtt"

#define MQTT_SERVER "broker.example.com"
#define MQTT_PORT 8883

#endif
```

## Configuração MQTT

O firmware publica no tópico:

```text
caixaagua/distancia
```

Payload publicado:

```json
{
  "distancia": 465,
  "porcentagem": 59.4
}
```

Campos:

| Campo | Tipo | Descrição |
| --- | --- | --- |
| `distancia` | inteiro | Distância corrigida em milímetros. |
| `porcentagem` | decimal | Nível estimado da caixa d'água entre `0.0` e `100.0`. |

## Como o cálculo funciona

O sensor VL53L0X mede distância. Em uma caixa d'água, quanto menor a distância entre o sensor e a superfície da água, mais cheia a caixa está.

O código define:

```cpp
int distanciaCheio = 100;
int distanciaVazio = 1000;
```

Interpretação:

- `100 mm`: caixa considerada cheia.
- `1000 mm`: caixa considerada vazia.

Antes do cálculo, o firmware aplica um offset:

```cpp
distancia = distancia - 35;
```

Essa decisão compensa uma diferença física de montagem ou calibração do sensor. Se o resultado ficar negativo, ele é travado em `0`:

```cpp
if (distancia < 0) {
  distancia = 0;
}
```

A porcentagem é calculada assim:

```cpp
float porcentagem =
  ((float)(distanciaVazio - distancia) /
  (distanciaVazio - distanciaCheio)) * 100.0;
```

Depois disso, o valor é limitado:

```cpp
if (porcentagem > 100) porcentagem = 100;
if (porcentagem < 0) porcentagem = 0;
```

Essa proteção evita publicar valores fisicamente impossíveis quando a leitura passa dos limites calibrados.

## Decisões de implementação

### ESP32-C6 como client ID MQTT

O client ID usado na conexão MQTT é fixo:

```cpp
String clientId = "ESP32-C6";
```

Isso simplifica a identificação do dispositivo no broker. Se mais de uma placa usar o mesmo broker, cada uma deve ter um client ID diferente para evitar desconexões causadas por conflito de sessão MQTT.

### Publicação periódica a cada 2 segundos

O loop termina com:

```cpp
delay(2000);
```

Essa decisão reduz tráfego MQTT e evita leituras excessivamente frequentes. Para dashboards em tempo quase real, `2` segundos costuma ser suficiente. Para economia de energia, esse intervalo pode ser aumentado.

### Leitura contínua do VL53L0X

O sensor é iniciado com:

```cpp
sensor.startContinuous();
```

O modo contínuo evita reiniciar uma medição manualmente a cada ciclo e deixa o loop principal mais simples.

### Timeout do sensor

O timeout do VL53L0X é configurado em `500 ms`:

```cpp
sensor.setTimeout(500);
```

Depois da leitura, o firmware checa:

```cpp
if (!sensor.timeoutOccurred()) {
  ...
}
```

Essa decisão impede publicar uma leitura quando o sensor não respondeu dentro do tempo esperado.

### TLS sem validação de certificado

O código usa:

```cpp
espClient.setInsecure();
```

Isso habilita a conexão TLS sem validar o certificado do servidor. A vantagem é simplificar o setup em ambiente de protótipo, porque não é necessário embarcar certificado raiz no firmware. A desvantagem é que a conexão perde a garantia completa de autenticidade do servidor.

Para produção, prefira configurar a CA correta com `setCACert(...)`.

### `secrets.h` fora do versionamento

O projeto inclui `secrets.h` no código, mas o arquivo está no `.gitignore`.

Essa decisão separa configuração sensível do firmware e reduz o risco de expor credenciais de Wi-Fi e MQTT no repositório.

### JSON manual com `sprintf`

O payload é montado manualmente:

```cpp
sprintf(
  json,
  "{\"distancia\":%d,\"porcentagem\":%.1f}",
  distancia,
  porcentagem
);
```

Essa escolha evita adicionar uma biblioteca de JSON apenas para um payload pequeno e fixo. Como o formato é simples, o custo de manter a string manual é baixo.

### Buffer fixo para payload

O payload usa:

```cpp
char json[100];
```

O tamanho é suficiente para o JSON atual. Se novos campos forem adicionados, esse buffer deve ser revisado para evitar truncamento ou overflow.

## Como compilar e enviar

1. Abra o projeto na Arduino IDE.
2. Instale o suporte para placas ESP32, se ainda não estiver instalado.
3. Selecione uma placa ESP32-C6 compatível.
4. Instale as bibliotecas `PubSubClient` e `VL53L0X`.
5. Crie o arquivo `secrets.h` com as credenciais.
6. Conecte o sensor nos pinos `SDA=6` e `SCL=7`.
7. Compile e envie o firmware para a placa.
8. Abra o Serial Monitor em `115200 baud`.

## Logs seriais esperados

Durante a inicialização:

```text
Conectando WiFi
...
WiFi conectado!
Conectando MQTT...
MQTT conectado!
```

Durante a operação:

```text
Enviado: 59.4
```

Se o sensor falhar na inicialização:

```text
Falha VL53L0X
```

## Observação sobre organização do código

As configurações sensíveis são definidas em `secrets.h` e expostas ao firmware em variáveis globais no `code.cpp`:

```cpp
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWORD;
```

Essa decisão centraliza as credenciais em um único arquivo ignorado pelo Git e deixa o restante do firmware lendo nomes claros e estáveis.

## Calibração

Os valores mais importantes para ajustar ao seu reservatório são:

```cpp
int distanciaCheio = 100;
int distanciaVazio = 1000;
distancia = distancia - 35;
```

Ajuste recomendado:

1. Com a caixa cheia, meça a distância reportada pelo sensor e use esse valor como `distanciaCheio`.
2. Com a caixa no menor nível aceitável, meça a distância e use como `distanciaVazio`.
3. Ajuste o offset `35` apenas se houver diferença conhecida causada pela montagem física do sensor.

## Troubleshooting

| Sintoma | Possível causa | Ação |
| --- | --- | --- |
| `Falha VL53L0X` | Sensor não encontrado no I2C | Verifique alimentação, GND, SDA e SCL. |
| Fica em `Conectando WiFi` | Credenciais ou rede incorretas | Revise `WIFI_SSID` e `WIFI_PASSWORD`. |
| `Erro MQTT` no Serial | Broker, porta ou credenciais MQTT incorretas | Revise `MQTT_SERVER`, `MQTT_PORT`, usuário e senha. |
| Porcentagem sempre `100` ou `0` | Calibração incompatível com a caixa | Ajuste `distanciaCheio` e `distanciaVazio`. |
| Leituras instáveis | Reflexão, vapor, ângulo ou superfície irregular | Reposicione o sensor e valide a fixação. |

## Possíveis evoluções

- Trocar `setInsecure()` por validação de certificado com CA.
- Publicar também status de Wi-Fi, RSSI e uptime.
- Parametrizar `distanciaCheio`, `distanciaVazio` e offset por constantes globais.
- Adicionar reconexão Wi-Fi caso a rede caia após o boot.
- Usar deep sleep se o projeto for alimentado por bateria.
