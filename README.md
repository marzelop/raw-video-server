# Rodando o programa
## Servidor
Primeiro, rode o servidor numa máquina e selecione a interface desejada (sudo necessário).
### Exemplos:
#### Ethernet
```sh
sudo ./server eth0
```
#### Loopback
```sh
sudo ./server lo
```
Após isso, rode o cliente na outra máquina (ou mesma máquina no caso do loopback) com a interface adequada.
### Exemplos:
#### Ethernet
```sh
sudo ./client eth0
```
#### Loopback
```sh
sudo ./client lo
```
A partir disso, você deve poder utilizar o cliente para baixar vídeos.
## Requisitos
- Compilador GCC e Make para compilação
- Celluloid para reprodução dos vídeos
## Extra
As seguintes flags podem ser utilizadas rodando o seguinte commando durante a compilação:
```sh
make CLIFLAGS="-D_SEND_DELAY_ -D_LOG_FRAMES_"
```
Pode ser compilado com a flag -D_SEND_DELAY_ para adicionar delay no envio e forçar timeouts para teste.
Pode ser compilado com a flag -D_LOG_FRAMES_ para loggar os frames recebidos e enviados.
