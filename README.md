# SmartWater

Firmware para monitoramento de nivel de caixa d'agua usando um ESP32-C6, sensor de distancia VL53L0X e publicacao MQTT em um broker HiveMQ Cloud com TLS.

O projeto mede a distancia entre o sensor e a superficie da agua, converte essa leitura para percentual de volume estimado e publica os dados em JSON no topico MQTT `caixaagua/distancia`.

## O que este firmware faz

- Conecta o ESP32-C6 a uma rede Wi-Fi.
- Inicializa o barramento I2C nos pinos definidos para o sensor VL53L0X.
- Le continuamente a distancia medida pelo sensor em milimetros.
- Aplica uma correcao fixa de offset na distancia lida.
- Converte a distancia corrigida em porcentagem de nivel da caixa.
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
calculo de nivel
  |
  | JSON
  v
MQTT / HiveMQ Cloud
```

## Hardware esperado

- Placa ESP32-C6.
- Sensor VL53L0X.
- Rede Wi-Fi disponivel.
- Broker MQTT compativel com usuario/senha e TLS.

## Ligacao do sensor

O firmware inicializa o I2C explicitamente nestes pinos:

| Sinal | Pino no ESP32-C6 |
| --- | --- |
| SDA | `6` |
| SCL | `7` |

Esses pinos foram definidos no codigo com:

```cpp
#define SDA_PIN 6
#define SCL_PIN 7
```

Essa decisao torna a pinagem explicita e evita depender do mapeamento padrao da placa usada pela IDE.

## Dependencias

O codigo usa as seguintes bibliotecas:

- `WiFi.h`: conexao Wi-Fi do ESP32.
- `WiFiClientSecure.h`: cliente TCP com suporte a TLS.
- `PubSubClient.h`: cliente MQTT.
- `Wire.h`: comunicacao I2C.
- `VL53L0X.h`: driver do sensor VL53L0X.

Em Arduino IDE, instale pelo Library Manager:

- `PubSubClient`
- `VL53L0X`

As bibliotecas `WiFi`, `WiFiClientSecure` e `Wire` fazem parte do suporte ESP32/Arduino.

## Configuracao de segredos

As credenciais ficam em `secrets.h`, que esta listado no `.gitignore` para evitar commit acidental de dados sensiveis.

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

## Configuracao MQTT

O firmware publica no topico:

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

| Campo | Tipo | Descricao |
| --- | --- | --- |
| `distancia` | inteiro | Distancia corrigida em milimetros. |
| `porcentagem` | decimal | Nivel estimado da caixa d'agua entre `0.0` e `100.0`. |

## Como o calculo funciona

O sensor VL53L0X mede distancia. Em uma caixa d'agua, quanto menor a distancia entre o sensor e a superficie da agua, mais cheia a caixa esta.

O codigo define:

```cpp
int distanciaCheio = 100;
int distanciaVazio = 1000;
```

Interpretacao:

- `100 mm`: caixa considerada cheia.
- `1000 mm`: caixa considerada vazia.

Antes do calculo, o firmware aplica um offset:

```cpp
distancia = distancia - 35;
```

Essa decisao compensa uma diferenca fisica de montagem ou calibracao do sensor. Se o resultado ficar negativo, ele e travado em `0`:

```cpp
if (distancia < 0) {
  distancia = 0;
}
```

A porcentagem e calculada assim:

```cpp
float porcentagem =
  ((float)(distanciaVazio - distancia) /
  (distanciaVazio - distanciaCheio)) * 100.0;
```

Depois disso, o valor e limitado:

```cpp
if (porcentagem > 100) porcentagem = 100;
if (porcentagem < 0) porcentagem = 0;
```

Essa protecao evita publicar valores fisicamente impossiveis quando a leitura passa dos limites calibrados.

## Decisoes de implementacao

### ESP32-C6 como client ID MQTT

O client ID usado na conexao MQTT e fixo:

```cpp
String clientId = "ESP32-C6";
```

Isso simplifica a identificacao do dispositivo no broker. Se mais de uma placa usar o mesmo broker, cada uma deve ter um client ID diferente para evitar desconexoes causadas por conflito de sessao MQTT.

### Publicacao periodica a cada 2 segundos

O loop termina com:

```cpp
delay(2000);
```

Essa decisao reduz trafego MQTT e evita leituras excessivamente frequentes. Para dashboards em tempo quase real, `2` segundos costuma ser suficiente. Para economia de energia, esse intervalo pode ser aumentado.

### Leitura continua do VL53L0X

O sensor e iniciado com:

```cpp
sensor.startContinuous();
```

O modo continuo evita reiniciar uma medicao manualmente a cada ciclo e deixa o loop principal mais simples.

### Timeout do sensor

O timeout do VL53L0X e configurado em `500 ms`:

```cpp
sensor.setTimeout(500);
```

Depois da leitura, o firmware checa:

```cpp
if (!sensor.timeoutOccurred()) {
  ...
}
```

Essa decisao impede publicar uma leitura quando o sensor nao respondeu dentro do tempo esperado.

### TLS sem validacao de certificado

O codigo usa:

```cpp
espClient.setInsecure();
```

Isso habilita a conexao TLS sem validar o certificado do servidor. A vantagem e simplificar o setup em ambiente de prototipo, porque nao e necessario embarcar certificado raiz no firmware. A desvantagem e que a conexao perde a garantia completa de autenticidade do servidor.

Para producao, prefira configurar a CA correta com `setCACert(...)`.

### `secrets.h` fora do versionamento

O projeto inclui `secrets.h` no codigo, mas o arquivo esta no `.gitignore`.

Essa decisao separa configuracao sensivel do firmware e reduz o risco de expor credenciais de Wi-Fi e MQTT no repositorio.

### JSON manual com `sprintf`

O payload e montado manualmente:

```cpp
sprintf(
  json,
  "{\"distancia\":%d,\"porcentagem\":%.1f}",
  distancia,
  porcentagem
);
```

Essa escolha evita adicionar uma biblioteca de JSON apenas para um payload pequeno e fixo. Como o formato e simples, o custo de manter a string manual e baixo.

### Buffer fixo para payload

O payload usa:

```cpp
char json[100];
```

O tamanho e suficiente para o JSON atual. Se novos campos forem adicionados, esse buffer deve ser revisado para evitar truncamento ou overflow.

## Como compilar e enviar

1. Abra o projeto na Arduino IDE.
2. Instale o suporte para placas ESP32, se ainda nao estiver instalado.
3. Selecione uma placa ESP32-C6 compativel.
4. Instale as bibliotecas `PubSubClient` e `VL53L0X`.
5. Crie o arquivo `secrets.h` com as credenciais.
6. Conecte o sensor nos pinos `SDA=6` e `SCL=7`.
7. Compile e envie o firmware para a placa.
8. Abra o Serial Monitor em `115200 baud`.

## Logs seriais esperados

Durante a inicializacao:

```text
Conectando WiFi
...
WiFi conectado!
Conectando MQTT...
MQTT conectado!
```

Durante a operacao:

```text
Enviado: 59.4
```

Se o sensor falhar na inicializacao:

```text
Falha VL53L0X
```

## Observacao sobre organizacao do codigo

As configuracoes sensiveis sao definidas em `secrets.h` e expostas ao firmware em variaveis globais no `code.cpp`:

```cpp
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWORD;
```

Essa decisao centraliza as credenciais em um unico arquivo ignorado pelo Git e deixa o restante do firmware lendo nomes claros e estaveis.

## Calibracao

Os valores mais importantes para ajustar ao seu reservatorio sao:

```cpp
int distanciaCheio = 100;
int distanciaVazio = 1000;
distancia = distancia - 35;
```

Ajuste recomendado:

1. Com a caixa cheia, meca a distancia reportada pelo sensor e use esse valor como `distanciaCheio`.
2. Com a caixa no menor nivel aceitavel, meca a distancia e use como `distanciaVazio`.
3. Ajuste o offset `35` apenas se houver diferenca conhecida causada pela montagem fisica do sensor.

## Troubleshooting

| Sintoma | Possivel causa | Acao |
| --- | --- | --- |
| `Falha VL53L0X` | Sensor nao encontrado no I2C | Verifique alimentacao, GND, SDA e SCL. |
| Fica em `Conectando WiFi` | Credenciais ou rede incorretas | Revise `WIFI_SSID` e `WIFI_PASSWORD`. |
| `Erro MQTT` no Serial | Broker, porta ou credenciais MQTT incorretas | Revise `MQTT_SERVER`, `MQTT_PORT`, usuario e senha. |
| Porcentagem sempre `100` ou `0` | Calibracao incompatavel com a caixa | Ajuste `distanciaCheio` e `distanciaVazio`. |
| Leituras instaveis | Reflexao, vapor, angulo ou superficie irregular | Reposicione o sensor e valide a fixacao. |

## Possiveis evolucoes

- Trocar `setInsecure()` por validacao de certificado com CA.
- Publicar tambem status de Wi-Fi, RSSI e uptime.
- Parametrizar `distanciaCheio`, `distanciaVazio` e offset por constantes globais.
- Adicionar reconexao Wi-Fi caso a rede caia apos o boot.
- Usar deep sleep se o projeto for alimentado por bateria.
