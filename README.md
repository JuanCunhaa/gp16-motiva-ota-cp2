# FIAP GP16 | CP2 Motiva | Atualizacao OTA no ESP32

Projeto Wokwi: https://wokwi.com/projects/475914302405087233

O mesmo circuito funciona nas duas versoes: ESP32 DevKitC, LED RGB de catodo comum e tres resistores (um em cada cor). As alturas sao simuladas; nao ha sensor nesta CP.

## Montagem no Wokwi

| ESP32 | Componente | Indicacao |
| --- | --- | --- |
| GPIO 25 | resistor -> terminal R do LED RGB | Vermelho, alerta na v2 |
| GPIO 26 | resistor -> terminal G do LED RGB | Verde, normal na v2 |
| GPIO 27 | resistor -> terminal B do LED RGB | Azul, v1 |
| GND | terminal COM do LED RGB | Catodo comum |

Use um resistor de 220 a 330 ohms em serie com cada cor no circuito fisico. O Wokwi aceita 1 kohm para simulacao, mas o LED ficara menos brilhante. Configure `Common Pin: Cathode` no menu do LED. As duas versoes usam os mesmos pinos, portanto **nao altere o hardware durante a atualizacao OTA**.

## Arquivos e arquitetura

- `firmware_v1/firmware_v1.ino`: firmware inicial com 5 leituras pseudoaleatorias, media, LED azul, Wi-Fi e cliente OTA.
- `firmware_v2/firmware_v2.ino`: media, vetor preservado e vetor ordenado, mediana, histerese e LEDs verde/vermelho.
- `version.json`: manifesto publico lido pela v1. `version=2.0` e URL de download do binario da Release `v2.0.0`.
- `.github/workflows/firmware-release.yml`: compila as duas versoes e anexa `firmware_v2.bin` automaticamente quando a Release `v2.0.0` for publicada.
- `diagram.json`: definicao do circuito equivalente, para consulta ou recuperacao.
- `partitions.csv`: configura no Wokwi a particao `otadata` e os dois slots OTA exigidos pela gravacao remota.
- `evidencias/ota_serial_log.txt`: transcricao integral da simulacao validada, da v1 ao teste da histerese na v2.

No Wokwi, a v1 le o manifesto por HTTPS depois de **tres sessoes completas**. Se a versao remota for superior, baixa o binario publicado no GitHub Release, grava no slot OTA e reinicia como v2. O firmware trata Wi-Fi, manifesto, versao atual, download e erros de gravacao no Serial Monitor.

**Limite da demonstracao:** para simplificar HTTPS em Wokwi, a v1 usa `WiFiClientSecure.setInsecure()`, ou seja, nao valida a identidade do servidor. Este exemplo deve ser endurecido com cadeia de certificados e verificacao de integridade/assinatura antes de usar em dispositivos reais.

## Compilar e publicar

O workflow usa `arduino-cli` e o core `esp32:esp32@3.3.12`, placa `esp32:esp32:esp32`, esquema de particao `default` com dois slots OTA. O binario publicado e a **imagem de aplicacao** (`firmware_v2.ino.bin`), renomeada para `firmware_v2.bin`.

No editor web do Wokwi, mantenha `partitions.csv` junto com `sketch.ino` e `diagram.json`. Sem esta tabela, o simulador pode iniciar com uma particao de aplicativo sem slot OTA, e a atualizacao falha com `Partition Could Not be Found`.

1. Depois que o codigo estiver na branch `main`, crie e publique no GitHub uma Release `v2.0.0` com a nova tag `v2.0.0` no commit atual. A publicacao aciona o workflow.
2. Aguarde o workflow ficar verde e confira em **Releases > v2.0.0** a presenca de `firmware_v2.bin`.
3. Confira se `version.json` e a URL de download estao publicos antes de iniciar a v1 no Wokwi.
4. Inicie a simulacao pelo botao Play, abra o Serial Monitor e espere a terceira sessao. Os inicios acontecem em aproximadamente 0, 48 e 96 segundos de tempo de simulacao; as ultimas leituras em 8, 56 e 104 segundos. A consulta OTA comeca ao terminar a terceira sessao.

Nao publique a Release antes de colocar os arquivos na `main`. Para uma futura versao, atualize `FW_VERSION` no codigo, `version.json` e o nome da tag juntos; nao sobrescreva um binario antigo com a mesma versao.

## Evidencias e testes

| Teste do enunciado | Evidencia esperada |
| --- | --- |
| 1 - FW 1.0 | 5 linhas `Leitura`, media e LED azul |
| 2 - 48 segundos | Os horarios de `inicio` das sessoes diferem em 48000 ms (tolerancia de poucos ms) |
| 3 - Verificacao remota | Depois da terceira sessao: versoes instalada e disponivel no Serial |
| 4 - OTA real | Log de download/gravação, reinicio e cabecalho `FW 2.0` |
| 5 - FW 2.0 | Ordem original, crescente, media e mediana exibidas |
| 6 - Alerta | Mediana >= 16, estado ALERTA, LED vermelho |
| 7 - Manter | Mediana 15, estado anterior preservado |
| 8 - Normal | Mediana <= 14, estado NORMAL, LED verde |

Os oito testes acima foram observados na simulacao com a tabela de particoes. O log integral esta em `evidencias/ota_serial_log.txt`: a OTA registra download ate 100%, `SW_CPU_RESET` e cabecalho `FIRMWARE 2.0`. As series controladas `A`, `M` e `N` registram medianas 17, 15 e 13 cm, respectivamente.

As medidas sao pseudoaleatorias por padrao. Para reproduzir os testes da histerese, envie no Serial Monitor da v2 a letra `A` antes da proxima sessao (mediana 17), depois `M` antes da sessao seguinte (mediana 15, permanece em alerta) e `N` antes da proxima (mediana 13, volta a normal). `R` devolve a proxima sessao ao modo aleatorio. O comando afeta apenas a **proxima sessao**; as 5 leituras ainda ocorrem a cada 2 segundos e os inicios permanecem a cada 48 segundos.

## Como voltar para a v1

No editor web do Wokwi, pare a simulacao, substitua `sketch.ino` pelo conteudo completo de `firmware_v1/firmware_v1.ino` e inicie novamente. O simulador compila a v1 como firmware inicial. **Nao e rollback OTA automatico:** para observar a v1 por mais tempo, antes do teste ajuste temporariamente `version.json` para `1.0` ou espere as primeiras tres sessoes. Depois restaure `2.0` e a URL da Release para repetir a atualizacao. Salve a alteracao no Wokwi se quiser preserva-la. O circuito RGB e os fios permanecem os mesmos.

## Integrantes

Grupo GP16, Ciencias da Computacao FIAP 2CCR: Caio Cordeiro Salgado, Hector van Tol Taver, Juan Gigliotti da Cunha, Rafael Alves da Silva e Raissa Fabricio Lima. **Inserir os respectivos RMs no PDF de entrega.**
