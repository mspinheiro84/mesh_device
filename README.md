# _Sobre o projeto_

Mesh device é um sistema desenvolvido para Esp32 afim de aplicar os conhecimentos adquiridos no curso "Aprenda programar o ESP32 com ESP-IDF" oferecido pela Embarcados.

O sistema Mesh device monitora a tensão e corrente de um ou mais equipamentos, através do Hardware Sensor, e atua no acionamento destes equipamentos, através do Hardware Switch. E todas as informações podem ser acompanhadas, assim como controladas, pelo app mobile.

O app mobile ainda não foi desenvolvido neste projeto.


## Como usar o sistema

O sistema requer pelo menos dois devices, para utilizar a rede mesh e os dois hardware, sensor e switch, além de uma conexão com internet via wifi.

No menu de configuração do projeto é possivel selecionar o Hardware Device, Sensor ou Switch.

Ao ligar o device deve segurar o botão para defini-lo como root da rede mesh. Este device ficará piscando de forma rápida e criará um rede wifi sem senha "ESP32", o usuário irá se conectar a rede gerada e passar SSID e senha da rede que o device ficará conectado com a internet. Após passar as credenciais da rede o device root irá desabilitar a rede gerada e se conectará a rede informada, neste momento o led irá piscar de forma mais lenta. Ao fechar conexão o led ficará acesso informando que ele é o root e criará a rede mesh para os devices node poderem se contectar a ele.

Para os devices node basta liga-los que já estarão configurados como node. Para adiciona-lo a rede mesh criada pelo root será necessário aceita-lo via app mobile. Só após ser aceito é que o device executará suas funções.

Qualquer uma das configurações de hardware, Sensor e Switch, podem ser definidos como root.


## Como é feita a comunicação com o backend

A comunicação entre o app mobile e o Esp32 é feita através do protocolo MQTT, com os payloads no formato de JSON, sob o endereço de broker "mqtt://mqtt.eclipseprojects.io", podendo ser alterado na componente mqtt_app.c.

Ao conectar com a rede wifi o root subscreve no tópico "mesh/(MAC_do_root)/toDevice" e passa a publicar nele toda a comunicação dos dispositivos da rede mesh. O payload recebido e enviado são no formato JSON, com os seguintes chaves: **"sn"** - serial number, **"m"** - MAC, **"nd"** - new device, **"v"** - tensão, **"a"** - corrente, **"s"** - array de quatro bits para controlar reles.

## Hardware

### Hardware sensor
Tem a função de monitorar a tensão e corrente AC de um equipamento. Possui o sensor Zmpt101b, de tensão AC, configurado no pino 35, sob o define SENSOR_GPIO2. Além do sensor Hall 50, de corrente, configurado no pino 34, sob o define SENSOR_GPIO1. 

As leituras são realizadas e salvas a cada 5s e a cada 50 leituras todas elas são enviadas para o app mobile. Estes tempos podem ser alterados na main.

### Hardware switch
Tem a função de controlar equipamentos ligando ou desligando-o. Possui quatro pinos dedicados para o controle, pinos 27, 26, 25 e 33 sob os defines SWITCH_GPIO1, SWITCH_GPIO2, SWITCH_GPIO3 e SWITCH_GPIO4 respectivamente.

A cada 10s são enviados o estado de cada pino. Para alterar o estado de um equipamento, de forma independente e sem alterar os outros, basta enviar **-**  na posição dos pinos que não se quer alterar. Por exemplo, ligar o equipamento 3 sem alterar os estados dos outros equipamentos: **--1-**.
