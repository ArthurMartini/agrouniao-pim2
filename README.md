# AgroUnião — PIM II (UNIP)

Sistema para a **Cooperativa AgroUnião** (cooperativa fictícia de agricultura familiar): cadastro de plantios, registro de colheitas, previsão de safra por cultura e pagamento proporcional por produtor.

Projeto Integrado Multidisciplinar (PIM II) do curso de Análise e Desenvolvimento de Sistemas da UNIP, desenvolvido com Scrum na disciplina de Engenharia de Software Ágil.

## Equipe (DevTime)

| Integrante | Papel / etapa |
|---|---|
| Arthur | Líder, Product Owner e Scrum Master |
| Bruno Cartolano | Modelagem de banco de dados (Etapa 4) |
| Camila | Programação estruturada em C (Etapa 5) |
| Marco | Redes de computadores (Etapa 6) |
| Rafael | Pesquisa e inovação (Etapa 2) |

## Estrutura

```
src/        programa em C (agrouniao.c)
database/   script do banco PostgreSQL, consultas e SELECT da demonstração
docs/       documento do PIM II e diagramas UML
```

## Banco de dados (PostgreSQL 16)

```bash
createdb agrouniao
psql -d agrouniao -f database/agrouniao_schema.sql
```

## Programa em C

Com banco (libpq):

```bash
gcc -std=c99 -Wall -Wextra -DUSE_PG -I/usr/include/postgresql -o agrouniao src/agrouniao.c -lpq
AGRO_DB="host=localhost dbname=agrouniao user=SEU_USUARIO password=SUA_SENHA" ./agrouniao
```

Somente arquivo (sem banco):

```bash
gcc -std=c99 -Wall -Wextra -o agrouniao src/agrouniao.c
./agrouniao
```

Usuários de demonstração (autenticação simplificada, apenas didática):

| Perfil | Login | Senha | Opções |
|---|---|---|---|
| Produtor | `produtor` | `agro123` | cadastrar plantio, listar os seus, registrar colheita |
| Gestor | `gestor` | `coop123` | listar, previsão por cultura, pagamento proporcional, relatório |

Ao abrir, o programa informa se está conectado ao banco ou em modo arquivo.

## Verificação no banco

Depois de cadastrar um plantio ou registrar uma colheita no programa:

```bash
psql -d agrouniao -f database/demo_select.sql
```
