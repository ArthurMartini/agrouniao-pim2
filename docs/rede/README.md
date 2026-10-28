# Redes de computadores e sistemas distribuídos

Etapa 6 do PIM II, responsável: Marco. Texto extraído do documento `docs/PIM_II.docx`, onde estão também as figuras, os quadros e as referências.

A infraestrutura detalha o fluxo previsto na arquitetura em camadas da Etapa 3 (Figura 3): sensores, gateway de coleta, aplicação e dados. O desafio central é conectar propriedades dispersas, com conexão de baixa qualidade (RNF01), o que torna pouco adequadas soluções que dependem de Wi-Fi ou de sinal celular em cada ponto de coleta.


### Arquitetura da Rede

A rede é dividida em quatro domínios: (i) o campo, onde os sensores de umidade do solo e temperatura de cada propriedade transmitem por rádio LoRa; (ii) a borda regional, com gateways LoRaWAN que recebem os pacotes dos sensores; (iii) a nuvem, que hospeda o servidor de rede LoRaWAN, o servidor de aplicação e o banco de dados; e (iv) a sede da cooperativa, de onde produtores, técnicos e a comissão de comercialização acessam o sistema. A tecnologia de rádio escolhida é o LoRaWAN, protocolo para redes de longo alcance e baixo consumo (LPWAN), adequado a dispositivos que enviam pequenos pacotes de forma esporádica, como uma leitura de sensor a cada 15 minutos. Christensen e Fonseca (2023) adotaram arquitetura semelhante em um sistema de monitoramento agrícola: os sensores enviam dados por LoRa a um servidor central, que os armazena em banco de dados na nuvem, e um aplicativo móvel apresenta as informações em tempo real.


### Topologia

A topologia é em estrela em dois níveis: cada gateway é o centro de uma estrela de sensores e os gateways, por sua vez, se conectam ao servidor de rede na nuvem, que encaminha as mensagens ao servidor de aplicação (MORAES; CONCEIÇÃO, 2021). A Figura 10 apresenta a topologia completa, com o fluxo de dados e os endereçamentos adotados.

Figura 10 – Topologia da rede e fluxo de comunicação do Sistema AgroUnião

Fonte: elaborado pelos autores (2026).


### Equipamentos

O Quadro 3 lista os equipamentos. As quantidades são estimativas iniciais e dependem de um estudo de cobertura em campo; os equipamentos de radiofrequência devem ser homologados pela Anatel antes da instalação.

Quadro 3 – Equipamentos da infraestrutura

Fonte: elaborado pelos autores (2026).


### Endereçamento IP

Foram adotadas faixas de endereços privados (Quadro 4). Na sede, cada VLAN recebe uma sub-rede /24 da faixa 192.168.0.0/16. Na nuvem, uma rede virtual (VPC) 10.0.0.0/16 é dividida em três sub-redes /24: a primeira é pública e concentra os pontos de entrada (o servidor de rede LoRaWAN e um balanceador de carga HTTPS, omitido da Figura 10 por simplicidade, que encaminha o acesso dos usuários); a aplicação e o banco ficam em sub-redes privadas, sem endereço público. Os gateways de campo obtêm endereço do provedor do backhaul e sempre iniciam a conexão com a nuvem, o que dispensa liberar portas de entrada nas propriedades. O sensor LoRa não usa endereço IP: é identificado pelos identificadores e chaves do próprio protocolo LoRaWAN.

Quadro 4 – Plano de endereçamento IP

Fonte: elaborado pelos autores (2026).


### VLANs

A rede da sede é segmentada em quatro VLANs (IEEE 802.1Q), com roteamento e filtragem feitos pelo firewall, cuja política padrão é negar o tráfego entre VLANs e liberar apenas o necessário:

VLAN 10 (Administrativa): comissão de comercialização e secretaria; acessa o sistema por HTTPS e a internet, e é a única origem autorizada a administrar a VLAN 99.

VLAN 20 (Técnicos): técnicos agrícolas; acessa o sistema por HTTPS e a internet. Fica separada da VLAN 10 porque os notebooks usados em campo são mais expostos a redes externas.

VLAN 30 (Visitantes): Wi-Fi isolado, somente com saída para a internet e sem acesso às demais VLANs.

VLAN 99 (Gerência): administração do firewall, do switch e do ponto de acesso, restrita a administradores.


### Comunicação entre Serviços

O caminho de uma leitura de sensor tem seis passos: (1) o sensor transmite o pacote por rádio LoRa ao gateway; (2) o computador de borda encaminha o pacote ao servidor de rede LoRaWAN por conexão TLS iniciada por ele mesmo e, se a internet estiver indisponível, mantém os pacotes em fila local e os envia quando a conexão volta (RNF01 e RN03); (3) o servidor de rede valida o pacote, descarta duplicatas e verifica os contadores de quadro; (4) entrega a mensagem ao servidor de aplicação por MQTT ou HTTPS; (5) a aplicação grava a leitura no SQL Server por conexão SQL protegida por TLS e avalia a regra de alerta (RN04); (6) os usuários consultam os dados por HTTPS, em interface web ou mobile.


### Utilização de Computação em Nuvem

A nuvem hospeda o servidor de rede LoRaWAN, a aplicação e o banco de dados, por três razões: o custo inicial baixo (pagamento por uso, sem sala de servidores nem compra de hardware), a escalabilidade sob demanda e a dispensa de equipe de TI própria para manter infraestrutura física, o que é compatível com uma cooperativa de pequeno porte. O provedor pode ser uma nuvem pública (como AWS, Microsoft Azure ou Google Cloud) ou um VPS nacional, preferindo-se uma região no Brasil para reduzir a latência e a necessidade de transferência internacional de dados pessoais (LGPD, tratada na Etapa 7). O banco de dados conta com backup automático diário e retenção definida (RNF03).


### Arquitetura Distribuída

O sistema é distribuído porque seus pontos de coleta, as propriedades, estão fisicamente separados e operam de forma independente: sensores e gateways continuam funcionando mesmo sem conexão com a nuvem, e o estado consolidado reside em um ponto central. A comunicação entre os componentes é assíncrona (mensagens entre o servidor de rede e a aplicação) e a consistência entre a borda e a nuvem é eventual: as leituras acumuladas na fila local chegam à nuvem quando a conexão é restabelecida, e o sistema tolera partições de rede sem perder dados enquanto a fila local tiver espaço (RN03). A separação em camadas (borda, nuvem e usuários) também isola falhas: a queda de um gateway afeta apenas as propriedades sob a sua cobertura.


### Mecanismos de Escalabilidade

Incluir uma nova propriedade não exige alterar a arquitetura: basta instalar o sensor dentro da área de cobertura de um gateway existente ou, se não houver cobertura, instalar um novo gateway apontado para o mesmo servidor de rede. Na nuvem, a camada de aplicação pode crescer horizontalmente, com mais instâncias atrás de um balanceador de carga; o banco pode ser ampliado verticalmente ou receber réplicas de leitura para os relatórios, e a tabela de leituras pode migrar para uma solução de séries temporais (Etapa 4). A carga prevista é baixa: com 60 propriedades e uma leitura a cada 15 minutos, chegam cerca de 240 mensagens por hora, ou 4 por minuto, o que deixa ampla folga em relação ao RNF05 e ao tempo de resposta de 3 segundos (RNF04). Como o canal de rádio é compartilhado e tem capacidade limitada, a frequência de envio dos sensores deve permanecer baixa.


### Segurança da Infraestrutura

A segurança é tratada em camadas:

Rádio (LoRaWAN): o conteúdo das mensagens é cifrado por chaves simétricas de 128 bits na camada de aplicação, e um código de integridade, também com chaves de 128 bits, protege as mensagens; o protocolo prevê autenticação mútua, integridade e confidencialidade e usa contadores de quadro contra repetição de mensagens (MORAES; CONCEIÇÃO, 2021). Cada sensor recebe chaves únicas e deve usar ativação pela rede (OTAA), em que as chaves de sessão são derivadas das chaves-raiz no momento da conexão. A mesma revisão aponta fragilidades, como requisições de ingresso sem cifra e gestão pouco especificada dos contadores, que devem ser mitigadas com atualização de firmware e monitoramento de comportamento anômalo dos dispositivos.

Transporte: TLS entre os gateways e a nuvem, HTTPS para os usuários e TLS nas conexões ao banco de dados.

Rede da sede: firewall com negação padrão entre VLANs, Wi-Fi com WPA2 ou WPA3, rede de visitantes isolada e VLAN de gerência restrita.

Nuvem: aplicação e banco em sub-redes privadas, sem endereço público; regras de acesso limitadas às portas necessárias; contas com privilégio mínimo e autenticação multifator para administradores.

Continuidade: backups diários com teste periódico de restauração (RNF03), registro de logs e atualização periódica do firmware dos gateways.
