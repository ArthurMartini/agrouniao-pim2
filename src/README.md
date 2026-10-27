# Programa em C

Etapa 5 do PIM II (Programação Estruturada em C), responsável: Camila.

O programa pede login e abre um menu conforme o perfil:

| Perfil | Login | Opções |
|---|---|---|
| Produtor | `produtor` | cadastrar plantio, listar os seus plantios, registrar colheita |
| Gestor | `gestor` | listar plantios, previsão por cultura, pagamento proporcional, relatório |

Com o PostgreSQL disponível, cada cadastro e cada colheita é gravado no banco na hora. Sem conexão, o programa avisa na abertura e usa o arquivo `plantios.txt`.

Funções principais:
- `previsaoSafra`: área multiplicada pela produtividade própria ou pela média regional.
- `consolidarPorCultura`: soma da previsão por cultura.
- `volumePorProdutor` e `imprimirPagamentos`: pagamento proporcional ao volume colhido.
- `dbInserirPlantio` e `dbRegistrarColheita`: gravação no banco com comandos parametrizados.

Compilação e configuração da conexão: veja o README da raiz do repositório.
