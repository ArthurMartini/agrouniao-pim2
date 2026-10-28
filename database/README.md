# Banco de dados (SQL Server)

Modelo da Etapa 4 do PIM II. O script `agrouniao_schema.sql` cria as tabelas e carrega dados de exemplo.

SQL Server 2019 ou superior. O script cria o banco `agrouniao` (se não existir), as tabelas e os dados de exemplo:

```bash
sqlcmd -S localhost -E -f 65001 -i database/agrouniao_schema.sql
```

Ou abra o arquivo no SSMS e execute (F5).

| Tabela | Papel |
|---|---|
| `cooperativa` | Dados da cooperativa (nome, CNPJ) |
| `produtor` | Cooperados; pertencem a uma cooperativa |
| `propriedade` | Propriedades de cada produtor (área, localização) |
| `plantio` | Cultura, área plantada e datas de plantio e colheita prevista |
| `previsao_safra` | Volume previsto para cada plantio |
| `safra` | Volume colhido, em relação 1:1 com o plantio |
| `leitura_sensor` | Umidade e temperatura por propriedade, com data e hora |
| `comprador` | Compradores da produção |
| `negociacao` | Venda coletiva: volume, preço e comprador |
| `pagamento` | Valor pago a cada produtor por negociação |

Arquivos:
- `agrouniao_schema.sql`: tabelas, restrições e dados de exemplo.
- `agrouniao_consultas.sql`: consultas de produção, consolidação e pagamento proporcional.
- `demo_select.sql`: SELECT usado para conferir o que o programa em C gravou.

Etapa 4 (Modelagem de Banco de Dados), responsável: Bruno.
