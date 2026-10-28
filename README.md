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
database/   script do banco SQL Server, consultas e SELECT da demonstração
docs/       documento do PIM II e diagramas UML
```

## Banco de dados (SQL Server)

SQL Server 2019 ou superior (Express serve). O script cria o banco `agrouniao`, as tabelas e os dados de exemplo:

```bash
sqlcmd -S localhost -E -f 65001 -i database/agrouniao_schema.sql
```

Ou abra `database/agrouniao_schema.sql` no SSMS e execute (F5).

## Programa em C

Com banco (ODBC; precisa do *ODBC Driver 17 ou 18 for SQL Server*):

```bash
gcc -std=c99 -Wall -Wextra -DUSE_DB -o agrouniao src/agrouniao.c -lodbc32   # Windows (MinGW-w64)
# Linux: troque -lodbc32 por -lodbc
# conexão padrão: SQL Server local, autenticação do Windows
# outra conexão (variável AGRO_DB, string de conexão ODBC):
AGRO_DB="Driver={ODBC Driver 17 for SQL Server};Server=localhost;Database=agrouniao;UID=SEU_USUARIO;PWD=SUA_SENHA;TrustServerCertificate=yes;" ./agrouniao
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
sqlcmd -S localhost -E -d agrouniao -f 65001 -i database\demo_select.sql
```
